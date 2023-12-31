#include <Ticker.h>
hw_timer_t *dim_timer = NULL;                                    // ESP32 specific code
const byte zcPin = 18;                                           // your zero-cross-over pin (USER INPUT)
const byte pwmPin[] = {19, 21, 22, 23, 33, 25, 26, 27};                          // your pwm pin for each Channel (USER INPUT)
int channels = 7;                                                // total number of channels counting from 0 (USER INPUT)
int interrupt_interval_us = 50;                                  // interrupt interval in us (minimum 50us) (USER INPUT)
int interrupt_interval_ticks = (5 * interrupt_interval_us);      // interrupt interval in ticks (250 ticks)
int interrupt_interval_zc = (50000 / interrupt_interval_ticks);  // interrupt intervals per zero-cross (200 interrupts per zero-cross)
volatile int interrupt_count = 0;                                // interrupt count for dimming transition, each count delays triac firing by interrupt_interval_us (10us)
int transition_time_ms = 10;                                     // time intervals for each step in brightness transition (100ms - minimum 10ms) (USER INPUT)
int transition_time_zc_count = (transition_time_ms / 10);        // time intervals for each step in brightness transition (10 zero-cross cycles)
volatile int zc_count = 0;                                       // zero-cross count, 100 zero-count = 1s on 50Hz mains
volatile int test_count = 1000;                                  // test count, 100 zero-count = 1s on 50Hz mains
volatile int test_transitiion = 200;                             // test transition duration
volatile int cycle = 0;                                          // test cycle count
volatile int tar_dim_per[] = {0, 0, 0, 0, 0, 0, 0, 0};                       // target dimming percent 0-100 for Channel (0=off, 1=max dimming, 99=min dimming, 100=no dimming) (USER INPUT)
volatile int cur_dim[] = {0, 0, 0, 0, 0, 0, 0, 0};                           // current dimming level for Channel (range=0-interrupt_interval_zc)
volatile int cur_dim_per[] = {0, 0, 0, 0, 0, 0, 0, 0};                       // current dimming percent 0-100 for Channel (0=off, 1=max dimming, 99=min dimming, 100=no dimming)
volatile int min_dim_per[] = {35, 35, 35, 35, 35, 35, 35, 35};                   // minimum dimming level before channel off (range=1-99) (USER INPUT)
volatile int max_dim_per[] = {80, 80, 80, 80, 80, 80, 80, 80};                   // maximum dimming level before channel no dim (range=99-1) (USER INPUT)
void ICACHE_RAM_ATTR dimTimerISR() {
  for (int i = 0; i <= channels; i++) {
    if (interrupt_count >= cur_dim[i]) {                         //fire channel at current dimming level
      digitalWrite(pwmPin[i], 1);
    }
  }
  interrupt_count++;                                             // increment interrupt_count
  timerAlarmWrite(dim_timer, interrupt_interval_us, true);       // fire dimimerISR every interrupt_interval_us
}
void ICACHE_RAM_ATTR zcDetectISR() {
  if (interrupt_count > (0.9 * interrupt_interval_zc)) {         // prevent suprious zero-cross detection
    for (int i = 0; i <= channels; i++) {
      digitalWrite(pwmPin[i], 0);                                // switch off all channels at zero-cross
    }
    interrupt_count = 0;                                         // reset interrupt count at zero-cross
    zc_count++;                                                  // increment zero-cross count
    test_count++;                                                // test count for main loop test values
  }
  if (zc_count >= transition_time_zc_count) {
    zc_count = 0;                                                // reset zero-cross count after transition interval
    for (int i = 0; i <= channels; i++) {
      if (cur_dim_per[i] < tar_dim_per[i]) {                     // if current dim is below target dim then increment current dim
        cur_dim_per[i]++;
      } else if (cur_dim_per[i] > tar_dim_per[i]) {              // if current dim is above target dim then decrement current dim
        cur_dim_per[i]--;
      }
      if (cur_dim_per[i] == 100) {                               // if current dim percent set to 100% then set channel to fire immediately
        cur_dim[i] = 0;
      } else if (cur_dim_per[i] == 0) {                          // if current dim percent set to 0% then set channel to never fire
        cur_dim[i] = (interrupt_interval_zc+10);                 // set to interrupt_interval_vc which should not be reached, but got some flicker so add 10 to be sure
      } else {
        cur_dim[i] = ((((100 - cur_dim_per[i]) * (max_dim_per[i] - min_dim_per[i])) - (100 * max_dim_per[i]) + 10000) / interrupt_interval_us);  // calculate delay in firing channel
      }
    }
  }
  dimTimerISR();                                                 // trigger TimerISR to set initial channels states after zero-cross
}
void setup() {
  pinMode(zcPin, INPUT_PULLUP);                                  // assign ZC pin as input
  attachInterrupt(digitalPinToInterrupt(zcPin), zcDetectISR, RISING);
  dim_timer = timerBegin(1, 80, true);                           // create timer for dimTimerISR
  timerAttachInterrupt(dim_timer, &dimTimerISR, true);           // attach timer to dimTimerISR
  timerAlarmEnable(dim_timer);                                   // enable timer for dimTimerISR
  for (int i = 0; i <= channels; i++) {
    pinMode(pwmPin[i], OUTPUT);                                  // assign pwm pins as output
    digitalWrite(pwmPin[i], 0);                                  //initial state of channel is off
  }
}
void loop() {
  if (cycle == 0) {
    if (test_count >= test_transitiion) {
      tar_dim_per[0] = 100; tar_dim_per[1] = 0; tar_dim_per[2] = 0; tar_dim_per[3] = 0; tar_dim_per[4] = 100; tar_dim_per[5] = 0; tar_dim_per[6] = 0; tar_dim_per[7] = 0;
      test_count = 0;
      cycle = 1;
    }
  }
  if (cycle == 1) {
    if (test_count >= test_transitiion) {
      tar_dim_per[0] = 100; tar_dim_per[1] = 100; tar_dim_per[2] = 0; tar_dim_per[3] = 0; tar_dim_per[4] = 100; tar_dim_per[5] = 100; tar_dim_per[6] = 0; tar_dim_per[7] = 0;
      test_count = 0;
      cycle++;
    }
  }
  if (cycle == 2) {
    if (test_count >= test_transitiion) {
      tar_dim_per[0] = 100; tar_dim_per[1] = 100; tar_dim_per[2] = 100; tar_dim_per[3] = 0; tar_dim_per[4] = 100; tar_dim_per[5] = 100; tar_dim_per[6] = 100; tar_dim_per[7] = 0;
      test_count = 0;
      cycle++;
    }
  }
  if (cycle == 3) {
    if (test_count >= test_transitiion) {
      tar_dim_per[0] = 100; tar_dim_per[1] = 100; tar_dim_per[2] = 100; tar_dim_per[3] = 100; tar_dim_per[4] = 100; tar_dim_per[5] = 100; tar_dim_per[6] = 100; tar_dim_per[7] = 100;
      test_count = 0;
      cycle++;
    }
  }
  if (cycle == 4) {
    if (test_count >= test_transitiion) {
      tar_dim_per[0] = 0; tar_dim_per[1] = 100; tar_dim_per[2] = 100; tar_dim_per[3] = 100; tar_dim_per[4] = 0; tar_dim_per[5] = 100; tar_dim_per[6] = 100; tar_dim_per[7] = 100;
      test_count = 0;
      cycle++;
    }
  }
  if (cycle == 5) {
    if (test_count >= test_transitiion) {
      tar_dim_per[0] = 0; tar_dim_per[1] = 0; tar_dim_per[2] = 100; tar_dim_per[3] = 100; tar_dim_per[4] = 0; tar_dim_per[5] = 0; tar_dim_per[6] = 100; tar_dim_per[7] = 100;
      test_count = 0;
      cycle++;
    }
  }
  if (cycle == 6) {
    if (test_count >= test_transitiion) {
      tar_dim_per[0] = 0; tar_dim_per[1] = 0; tar_dim_per[2] = 0; tar_dim_per[3] = 100; tar_dim_per[4] = 0; tar_dim_per[5] = 0; tar_dim_per[6] = 0; tar_dim_per[7] = 100;
      test_count = 0;
      cycle++;
    }
  }
  if (cycle == 7) {
    if (test_count >= test_transitiion) {
      tar_dim_per[0] = 0; tar_dim_per[1] = 0; tar_dim_per[2] = 0; tar_dim_per[3] = 0; tar_dim_per[4] = 0; tar_dim_per[5] = 0; tar_dim_per[6] = 0; tar_dim_per[7] = 0;
      test_count = 0;
      cycle++;
    }
  }
  if (cycle == 8) {
    if (test_count >= test_transitiion) {
      tar_dim_per[0] = 1; tar_dim_per[1] = 1; tar_dim_per[2] = 1; tar_dim_per[3] = 1; tar_dim_per[4] = 1; tar_dim_per[5] = 1; tar_dim_per[6] = 1; tar_dim_per[7] = 1;
      test_count = 0;
      cycle++;
    }
  }
  if (cycle == 9) {
    if (test_count >= test_transitiion) {
      tar_dim_per[0] = 0; tar_dim_per[1] = 0; tar_dim_per[2] = 0; tar_dim_per[3] = 0; tar_dim_per[4] = 0; tar_dim_per[1] = 0; tar_dim_per[6] = 0; tar_dim_per[7] = 0;
      test_count = 0;
      cycle++;
    }
  }
  if (cycle == 10) {
    if (test_count >= test_transitiion) {
      tar_dim_per[0] = 0; tar_dim_per[1] = 0; tar_dim_per[2] = 0; tar_dim_per[3] = 0; tar_dim_per[4] = 0; tar_dim_per[5] = 0; tar_dim_per[6] = 0; tar_dim_per[7] = 0;
      test_count = 0;
      cycle = 0;
    }
  }
}