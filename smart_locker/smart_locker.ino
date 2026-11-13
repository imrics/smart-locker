#include "EEPROM_lib.h"

const int adc_pin = A0;
const int current_pin = A1;
const int unlock_trip = 32;
const int lock_trip = 40;

// Milestone 4b: three steady status LEDs and one tone per event.
const byte red_led = 5;
const byte green_led = 6;
const byte yellow_led = 7;
const byte buzzer_pin = 3; // PD3 / OC2B: Timer2, separate from the D9 servo.

// state variables
enum State {
  LOCKING, UNLOCKING, LOCKED, UNLOCKED, CHANGING_PASSWORD
};
State state = LOCKING;
String valid_password = "1234";
String entered_password = "";

enum Feedback { UI_NONE, UI_SUCCESS, UI_ERROR, UI_NOTICE, UI_CANCEL, UI_LOW_BATTERY };
Feedback feedback = UI_NONE;
uint32_t feedback_started = 0;
uint16_t buzzer_hz = 0;
bool ui_blockage = false; // Display history only; never controls the motor.

// keypad state variables
enum KeyState { WAIT_FOR_RELEASE, READY, PRESSED };
KeyState key_state = WAIT_FOR_RELEASE;
char pressed_key = '\0';
uint32_t key_started = 0;

// timing vars
uint32_t move_started = 0;
uint32_t last_poll = 0;
byte spike_count = 0;
uint32_t last_activity = 0;

ISR(WDT_vect) {} // The watchdog only wakes the CPU; it does not reset it.

bool shouldSleep(uint32_t now) {
  return (state == LOCKED || state == UNLOCKED) && key_state == READY &&
         entered_password == "" && feedback == UI_NONE &&
         (uint32_t)(now - last_activity) >= 30000;
}

// Milestone 4b: these helpers schedule indications without delay() or a library.
void showFeedback(Feedback next) {
  feedback = next;
  feedback_started = millis();
}

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

void updateIndicators(uint32_t now) {
  pauseBatteryWarning(now);
  uint32_t elapsed = now - feedback_started;
  uint16_t duration = feedback == UI_LOW_BATTERY ? 1000 : feedback == UI_ERROR ? 400 :
                      feedback == UI_SUCCESS ? 150 : 100;
  if (feedback != UI_NONE && elapsed >= duration) feedback = UI_NONE;

  // LEDs follow only the state. Keypresses and buzzer feedback never change them.
  bool awake = !shouldSleep(now);
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

void powerDown() {
  ADCSRA &= ~_BV(ADEN);
  noInterrupts();
  __asm__ __volatile__("wdr" ::: "memory");
  MCUSR &= ~_BV(WDRF);
  WDTCSR = _BV(WDCE) | _BV(WDE);
  WDTCSR = _BV(WDIF) | _BV(WDIE) | _BV(WDP0); // Approximately 32 ms.
  SMCR = _BV(SM1) | _BV(SE);
  // Keep interrupt enable and sleep adjacent so a wakeup cannot be missed.
  __asm__ __volatile__("sei\n\tsleep\n\tcli" ::: "memory");
  SMCR = 0;
  WDTCSR = _BV(WDCE) | _BV(WDE);
  WDTCSR = 0;
  ADCSRA |= _BV(ADEN);
  interrupts();
}

char voltageToKey(float voltage) {
  if (voltage > 3.8500 || voltage < 2.1500) {
    return '\0';
  }
  else if (voltage >= 3.6397) return '1';
  else if (voltage >= 3.4314) return '4';
  else if (voltage >= 3.2456) return '7';
  else if (voltage >= 3.0790) return '*';
  else if (voltage >= 2.9286) return '2';
  else if (voltage >= 2.7922) return '5';
  else if (voltage >= 2.6680) return '8';
  else if (voltage >= 2.5544) return '0';
  else if (voltage >= 2.4500) return '3';
  else if (voltage >= 2.3539) return '6';
  else if (voltage >= 2.2650) return '9';
  else return '#';
}

bool motorMoving() {
  return state == LOCKING || state == UNLOCKING;
}

void startMove(State destination) {
  state = destination;
  move_started = last_poll = millis();
  spike_count = 0;
  entered_password = "";
  key_state = WAIT_FOR_RELEASE;

  bool locking = state == LOCKING;
  OCR1A = locking ? 1000 : 2000;
  TCCR1A |= _BV(COM1A1);
  TCCR1B |= _BV(CS11);
  Serial.println(locking ? F("Locking") : F("Unlocking"));
}

void reverseMotor() {
  Serial.println(F("Blocked: returning to previous position"));
  startMove(state == UNLOCKING ? LOCKING : UNLOCKING);
}

void updateMotor(uint32_t now) {
  if (!motorMoving()) return;
  uint32_t sample_elapsed = now - last_poll;
  uint32_t elapsed = now - move_started;
  if (elapsed >= 450) {
    reverseMotor();
    return;
  }
  if (sample_elapsed < 20) return;
  last_poll = now;

  analogRead(current_pin);
  int current = analogRead(current_pin);

  bool locking = state == LOCKING;
  int threshold = locking ? lock_trip : unlock_trip;
  if (elapsed > (locking ? 250 : 200) && current >= threshold) spike_count++;
  else spike_count = 0;

  if (spike_count >= 3) {
    reverseMotor();
  } else if (elapsed >= 400 && spike_count == 0) {
    while (PINB & _BV(PB1)) {} // Finish the D9 pulse before stopping Timer1.
    TCCR1A &= ~_BV(COM1A1);
    TCCR1B &= ~_BV(CS11);
    digitalWrite(9, LOW);
    state = locking ? LOCKED : UNLOCKED;
    last_activity = now;
    Serial.println(locking ? F("Locked") : F("Unlocked"));
  }
}

void handleKey(char key) {
  if (state == CHANGING_PASSWORD) {
    if (key == '*') {
      entered_password = "";
      state = UNLOCKED;
      showFeedback(UI_CANCEL);
      Serial.println("Password change canceled");
    } else if (key == '#') {
      if (entered_password != "") {
        if (EEPROM_update_array((uint8_t *)entered_password.c_str(),
                                entered_password.length() + 1, 0) != EEPROM_SUCCESSFUL) {
          Serial.println("Password save failed");
          showFeedback(UI_ERROR);
        } else {
          valid_password = entered_password;
          entered_password = "";
          state = UNLOCKED;
          showFeedback(UI_SUCCESS);
          Serial.println("Password changed");
        }
      } else {
        Serial.println("Enter a password first");
        showFeedback(UI_ERROR);
      }
    } else {
      entered_password += key;
    }
  } else if (key == '*') {
    entered_password = "";
    if (state == UNLOCKED) startMove(LOCKING);
  } else if (state == LOCKED && key == '#') {
    if (entered_password == valid_password) startMove(UNLOCKING);
    else {
      Serial.println("Incorrect password");
      showFeedback(UI_ERROR);
    }
    entered_password = "";
  } else if (state == LOCKED) {
    entered_password += key;
  }
}

void updateKeypad(uint32_t now) {
  if (!shouldSleep(now) && (uint32_t)(now - last_poll) < 20) return;
  if (shouldSleep(now) && (uint32_t)(last_poll - last_activity) < 30000) {
    Serial.println(F("Power down"));
    Serial.flush();
  }
  last_poll = now;
  analogRead(adc_pin); // Discard after switching from A1.
  char key = voltageToKey(analogRead(adc_pin) * (5.0 / 1023.0));
  if (key != '\0') {
    if (shouldSleep(now)) {
      Serial.println(F("Awake"));
      digitalWrite(4, HIGH);
      wakeBattery(now);
    }
    last_activity = now;
  }

  switch (key_state) {
    case WAIT_FOR_RELEASE:
      if (key == '\0') key_state = READY;
      break;
    case READY:
      if (key != '\0') {
        pressed_key = key;
        key_started = now;
        key_state = PRESSED;
      }
      break;
    case PRESSED:
      if (key == '\0') {
        key_state = READY;
        Serial.println(pressed_key);
        handleKey(pressed_key);
      } else if (key == '#' && pressed_key == '#' && state == UNLOCKED &&
                 (uint32_t)(now - key_started) >= 5000) {
        state = CHANGING_PASSWORD;
        entered_password = "";
        key_state = WAIT_FOR_RELEASE;
        showFeedback(UI_NOTICE);
        Serial.println("Enter new password, then press #");
      }
      break;
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); 

  // read saved digits from EEPROM
  String saved_password = "";
  for (uint16_t address = 0; address <= EEPROM_SIZE; address++) {
    uint8_t digit;
    EEPROM_read(&digit, address);

    if (digit == '\0') {
      if (saved_password != "") valid_password = saved_password;
      break;
    }

    if (digit < '0' || digit > '9') break;
    saved_password += char(digit);
  }
  pinMode(4, OUTPUT);
  pinMode(red_led, OUTPUT);
  pinMode(green_led, OUTPUT);
  pinMode(yellow_led, OUTPUT);
  pinMode(buzzer_pin, OUTPUT);
  TCCR2A = TCCR2B = TIMSK2 = 0; // Timer2 belongs only to the passive buzzer.
  digitalWrite(buzzer_pin, LOW);
  pinMode(adc_pin, INPUT);
  pinMode(current_pin, INPUT);
  setupBattery();
  pinMode(9, OUTPUT);
  digitalWrite(4, HIGH);
  // clear Timer1 control registers
  TCCR1A = 0;
  TCCR1B = 0;

  TCNT1 = 0; // initializes counter value to 0
  ICR1 = 20000; // frequency 50 Hz
  OCR1A = 1000; // locked position 1 ms pulse width

  // Timer1 sends PWM on pin 9
  TCCR1A = _BV(COM1A1);
  TCCR1B = _BV(WGM13) | _BV(CS11);
  startMove(LOCKING);
}

void loop() {
  uint32_t now = millis();
  State previous_motor_state = state;
  updateMotor(now);
  // Observe the existing motor transitions solely to drive LEDs and the piezo.
  if (state != previous_motor_state) {
    if (motorMoving()) {
      ui_blockage = true;
      showFeedback(UI_ERROR);
    } else {
      if (!ui_blockage) showFeedback(state == LOCKED ? UI_NOTICE : UI_SUCCESS);
      ui_blockage = false;
    }
  }
  if (state == LOCKED || state == UNLOCKED || state == CHANGING_PASSWORD) {
    updateKeypad(now);
  }
  updateBattery(millis());
  now = millis(); // Battery/EEPROM/Serial work may cross the idle-sleep deadline.
  updateIndicators(now);
  if (shouldSleep(now)) {
    digitalWrite(4, LOW);
    powerDown();
  }
}
