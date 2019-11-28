/* Libraries for LCD display */
#include <ArducamSSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>

/* Libraries for sound */
#include "fastpwm.h"
#include "cuckoo_u8.h"
#include "ding_dong_u8.h"
#include "tick_toc_u8.h"

/* For setting up the LCD screen */
#define OLED_RESET  16  // Pin 15 -RESET digital signal
#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16
ArducamSSD1306 display(OLED_RESET); // FOR I2C

/* Pins for setting the time! */
const int SET_PIN = 48;
const int HR_PIN = 50;
const int MIN_PIN = 52;
bool was_time_set = false;
uint8_t display_time_set_count = 0;

/* For Timer1 and PWM setup */
#define SAMPLE_RATE 8000
#define WAVE_ZERO 128

/* For displaying and tracking time */
char time_display[8]; 
unsigned long t = 0;
unsigned long start_time = 0;
unsigned long test_time = 0;
#define seconds(x)   (int)((x) % 60)
#define minutes(x)   (int)(((x) / 60) % 60)
#define hours(x)     (int)(((x) / 3600) % 24)

/* For playing the sounds */
bool play_tick_toc = false;
bool play_ding_dong = false;
bool play_cuckoo = false;
unsigned long tt_count = 0;
unsigned long dd_count = 0;
unsigned long c_count = 0;
uint8_t cuckoo_play_count = 0;

/* This  function  is the  Interrupt  Service  Routine (ISR), which is  called
 * every  time an  interrupt  occurs  for  this  timer
 */
ISR(TIMER1_COMPA_vect) {
  // re-enable global interrupts so that other ISRs can execute  
  // (millis() can keep incrementing)
  sei();
  
  unsigned int to_play_val = 0;

  test_time = micros();
  if (test_time - start_time >= 1000000) {
    start_time = test_time;
    // We lose ~1 second every hour, so we compensate:
    // (1000000usec/sec) / (3600sec/hr)
    start_time -= 277;
    t++;
   
    if (t % 5 == 0) // Every 5 seconds
      play_tick_toc = true;

    if (t % 3600 == 1800) // Every 30-past
      play_ding_dong = true;

    if (t % 3600 == 0) // Every hour
      play_cuckoo = true;
  }

  if (play_tick_toc) {
    // A 5 second mark recently occurred; play tick-toc sound
    to_play_val += pgm_read_byte_near(tick_toc_data + (tt_count % TICK_TOC_DATA_SIZE)) - WAVE_ZERO;
    tt_count++;
  }
  if (tt_count == TICK_TOC_DATA_SIZE) {
    tt_count = 0;
    play_tick_toc = false;
  }

  if (play_ding_dong) {
    // A 30-minute mark recently occurred; play ding-dong sound
    to_play_val += pgm_read_byte_near(ding_dong_data + (dd_count % DING_DONG_DATA_SIZE)) - WAVE_ZERO;
    dd_count++;
  } 
  if (dd_count == DING_DONG_DATA_SIZE) {
    dd_count = 0;
    play_ding_dong = false;
  }

  if (play_cuckoo) {
    // A 1 hour mark recently occurred; play cuckoo sound
    to_play_val += pgm_read_byte_near(cuckoo_data + (c_count % CUCKOO_DATA_SIZE)) - WAVE_ZERO;
    c_count++;
  } 
  if (c_count == CUCKOO_DATA_SIZE) {
    // Playing cuckoo is a bit different as we need to play it
    // n time where n is the current hour.
    c_count = 0;
    if (hours(t) == 0) {
      // This accounts for the time rollover
      // from 23:59:59 to 00:00:00. I don't consider
      // 0 to be a valid number of cuckoo clock plays,
      // so we play 24 cuckoos in this case.
      if (++cuckoo_play_count == 24) {
        play_cuckoo = false;
        cuckoo_play_count = 0;
      }
    } else if (++cuckoo_play_count == hours(t))  {
      // This is for the normal case cuckoo counting
      play_cuckoo = false;
      cuckoo_play_count = 0;
    }
  } 

  fastpwm_play_sample((uint8_t)(to_play_val / 2 + WAVE_ZERO));
}


void  init_timer(void) {
  noInterrupts (); //  Disable  all  interrupts
  
  //  Clear  Timer1  register  configuration
  TCCR1A = 0;
  TCCR1B = 0;
  
  //  Configure  Timer1  for  CTC  mode (WGM = 0b0100)
  bitSet(TCCR1B , WGM12);
  
  //  Disable  prescaler
  bitSet(TCCR1B , CS10);
  
  // Set  timer  period , which is  defined  as the  number  of
  // CPU  clock  cycles  (1/16 MHz) between  interrupts  - 1
  OCR1A = (F_CPU / SAMPLE_RATE) - 1;
  
  //  Enable  timer  interrupts  when  timer  count  reaches  value in  OCR1A
  bitSet(TIMSK1 , OCIE1A);
  
  //  Enable  interrupts
  interrupts ();
}

void setup() {
  // Start Serial
  Serial.begin(115200);

  // SSD1306 Init
  display.begin();  // Switch OLED
  // Clear the buffer.
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(20,20);
  display.println("Set Me!");
  display.display();

  pinMode(HR_PIN, INPUT_PULLUP);
  pinMode(MIN_PIN, INPUT_PULLUP);
  pinMode(SET_PIN, INPUT_PULLUP);

  // Enable timers etc. after initial time is set
  init_timer();
  fastpwm_init();
}

void loop() {
  
  /* 
   *  By holding down the set pin, one can manipulate the
   *  minute and hour of the clock
   */
  while (digitalRead(SET_PIN) == LOW) {
    
    display.clearDisplay();
    display.setCursor(20,20);
    display.println(time_display);
    display.display();
    
    if (digitalRead(HR_PIN) == LOW) {
      t += 3600;
      sprintf(time_display, "%02d:%02d:%02d", hours(t),
                                              minutes(t),
                                              seconds(t));
    }
    
    if (digitalRead(MIN_PIN) == LOW) {
      t += 60;
      sprintf(time_display, "%02d:%02d:%02d", hours(t), 
                                              minutes(t), 
                                              seconds(t));
    }

    was_time_set = true;
    start_time = micros();
  }

  display.clearDisplay();
  display.setCursor(20,20);

  /* 
   *  Handles flashing "Set Me!" else it just displays
   *  the time. Everytime the clock is started fresh it
   *  expects the user to set the time. If the user
   *  doesn't want to set the time they still need to
   *  hit the "time set" button to get the blinking message
   *  to go away, similar to a stove, coffee pot, or microwave
   */
  if (was_time_set) {
    sprintf(time_display, "%02d:%02d:%02d", hours(t), 
                                            minutes(t), 
                                            seconds(t));
  } else {
    if (display_time_set_count % 16 < 8)
      sprintf(time_display, "Set Me!");
    else
      sprintf(time_display, "%02d:%02d:%02d", hours(t), 
                                              minutes(t), 
                                              seconds(t));
    display_time_set_count++;
  }
  
  display.println(time_display);
  display.display();
}
