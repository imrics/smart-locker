#include "Indicators.h"

namespace {

// Milestone 4b: three steady status LEDs and one tone per event.
const byte red_led = 5;
const byte green_led = 6;
const byte yellow_led = 7;
const byte buzzer_pin = 3; // PD3 / OC2B: Timer2, separate from the D9 servo.

Feedback feedback = UI_NONE;
uint32_t feedback_started = 0;
uint16_t buzzer_hz = 0;
bool ui_blockage = false; // Display history only; never controls the motor.

void setBuzzer(uint16_t hz) {
  if (hz == buzzer_hz) return;
  TCCR2B = 0;
  TCCR2A = 0;
  digitalWrite(buzzer_pin, LOW);
  buzzer_hz = hz;
  if (hz == 0) return;

  // CTC, toggle OC2B at TOP; f = 16 MHz / (2 * 32 * (OCR2A + 1)).
  // 1000, 1250 and 2000 Hz give TOP = 249, 199 and 124.
  TCNT2 = 0;
  OCR2A = OCR2B = F_CPU / (64UL * hz) - 1;
  TIFR2 = _BV(OCF2A) | _BV(OCF2B) | _BV(TOV2);
  TCCR2A = _BV(WGM21) | _BV(COM2B0);
  TCCR2B = _BV(CS21) | _BV(CS20); // Timer2 prescaler 32; no ISR needed.
}

} // namespace

void setupIndicators() {
  pinMode(red_led, OUTPUT);
  pinMode(green_led, OUTPUT);
  pinMode(yellow_led, OUTPUT);
  pinMode(buzzer_pin, OUTPUT);
  TCCR2A = TCCR2B = TIMSK2 = 0; // Timer2 belongs only to the passive buzzer.
  digitalWrite(buzzer_pin, LOW);
}

bool feedbackActive() {
  return feedback != UI_NONE;
}

bool cancelFeedback(Feedback type) {
  if (feedback != type) return false;
  feedback = UI_NONE;
  return true;
}

// Milestone 4b: these helpers schedule indications without delay() or a library.
void showFeedback(Feedback next) {
  feedback = next;
  feedback_started = millis();
}


void updateIndicators(uint32_t now, State state, bool idle) {
  uint32_t elapsed = now - feedback_started;
  uint16_t duration = feedback == UI_LOW_BATTERY ? 1000 : feedback == UI_ERROR ? 400 :
                      feedback == UI_SUCCESS ? 150 : 100;
  if (feedback != UI_NONE && elapsed >= duration) feedback = UI_NONE;

  // LEDs follow only the state. Keypresses and buzzer feedback never change them.
  bool awake = !idle || feedbackActive();
  digitalWrite(red_led, awake && state == LOCKED);
  digitalWrite(green_led, awake && state == UNLOCKED);
  digitalWrite(yellow_led, awake &&
               (state == LOCKING || state == UNLOCKING || state == CHANGING_PASSWORD));
  uint16_t hz = 0;

  if (feedback == UI_LOW_BATTERY) {
    hz = 1250; // One steady one-second tone: replace the battery.
  } else if (feedback == UI_SUCCESS) {
    hz = 2000; // One 150 ms high tone: unlocked or password saved.
  } else if (feedback == UI_ERROR) {
    hz = 1000; // One 400 ms low tone: rejection, save error or blockage.
  } else if (feedback == UI_NOTICE || feedback == UI_CANCEL) {
    hz = feedback == UI_NOTICE ? 2000 : 1000;
  }

  setBuzzer(awake ? hz : 0);
}

void updateMotorFeedback(State previous, State current) {
  // Observe the existing motor transitions solely to drive LEDs and the piezo.
  if (current != previous) {
    if (current == LOCKING || current == UNLOCKING) {
      ui_blockage = true;
      showFeedback(UI_ERROR);
    } else {
      if (!ui_blockage) showFeedback(current == LOCKED ? UI_NOTICE : UI_SUCCESS);
      ui_blockage = false;
    }
  }
}
