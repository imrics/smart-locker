#include "EEPROM_lib.h"

const int adc_pin = A0;
const int current_pin = A1;
const int unlock_trip = 3 * 6;
const int lock_trip = 3 * 5;

// state variables
enum State {
  STARTING, LOCKING, UNLOCKING, RETURNING_LOCKED, RETURNING_UNLOCKED,
  LOCKED, UNLOCKED, CHANGING_PASSWORD, FAULT
};
State state = STARTING;
String valid_password = "1234";
String entered_password = "";

// keypad state variables
enum KeyState { WAIT_FOR_RELEASE, READY, PRESSED };
KeyState key_state = WAIT_FOR_RELEASE;
char pressed_key = '\0';
uint32_t key_started = 0;
uint32_t last_keypad = 0;

// timing vars
uint32_t move_started = 0;
uint32_t last_sample = 0;
unsigned int current_sum = 0;
byte current_samples = 0;
byte high_windows = 0;
uint32_t last_log = 0;

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
  return state == STARTING || state == LOCKING || state == UNLOCKING ||
         state == RETURNING_LOCKED || state == RETURNING_UNLOCKED;
}

void startMove(State destination) {
  state = destination;
  move_started = last_sample = millis();
  current_sum = current_samples = high_windows = 0;
  entered_password = "";
  key_state = WAIT_FOR_RELEASE;

  bool locking = state == STARTING || state == LOCKING || state == RETURNING_LOCKED;
  OCR1A = locking ? 1000 : 2000;
  if (state == RETURNING_LOCKED || state == RETURNING_UNLOCKED) {
    Serial.println(F("Blocked: returning to previous position"));
  } else {
    Serial.println(locking ? F("Locking") : F("Unlocking"));
  }
}

void motorFault() {
  TCCR1A &= ~_BV(COM1A1);
  digitalWrite(9, LOW);
  state = FAULT;
  Serial.println(F("Motor fault. Clear obstruction and reset Arduino."));
}

void reverseOrStop() {
  if (state == UNLOCKING) startMove(RETURNING_LOCKED);
  else if (state == LOCKING) startMove(RETURNING_UNLOCKED);
  else motorFault();
}

void updateMotor(uint32_t now) {
  if (!motorMoving()) return;
  uint32_t sample_elapsed = now - last_sample;
  if (sample_elapsed > 10) {
    motorFault();
    return;
  }
  uint32_t elapsed = now - move_started;
  if (elapsed >= 450) {
    reverseOrStop();
    return;
  }
  if (sample_elapsed < 1) return;
  last_sample = now;

  analogRead(current_pin);
  current_sum += analogRead(current_pin);
  if (++current_samples < 20) return;
  int current_average = (current_sum + 10) / 20;
  current_sum = current_samples = 0;

  bool locking = state == STARTING || state == LOCKING || state == RETURNING_LOCKED;
  int threshold = locking ? lock_trip : unlock_trip;
  if (elapsed > 100 && current_average >= threshold) high_windows++;
  else high_windows = 0;

  if (high_windows >= 3) {
    reverseOrStop();
  } else if (elapsed >= 400 && high_windows == 0) {
    state = locking ? LOCKED : UNLOCKED;
    Serial.println(locking ? F("Locked") : F("Unlocked"));
  }

  if (motorMoving() && (uint32_t)(now - last_log) >= 100 &&
      Serial.availableForWrite() >= 32) {
    last_log = now;
    Serial.print(F("Current ADC: "));
    Serial.println(current_average);
  }
}

void handleKey(char key) {
  if (state == CHANGING_PASSWORD) {
    if (key == '*') {
      entered_password = "";
      state = UNLOCKED;
      Serial.println("Password change canceled");
    } else if (key == '#') {
      if (entered_password != "") {
        if (EEPROM_update_array((uint8_t *)entered_password.c_str(),
                                entered_password.length() + 1, 0) != EEPROM_SUCCESSFUL) {
          Serial.println("Password save failed");
        } else {
          valid_password = entered_password;
          entered_password = "";
          state = UNLOCKED;
          Serial.println("Password changed");
        }
      } else {
        Serial.println("Enter a password first");
      }
    } else {
      entered_password += key;
    }
  } else if (key == '*') {
    entered_password = "";
    if (state == UNLOCKED) startMove(LOCKING);
  } else if (state == LOCKED && key == '#') {
    if (entered_password == valid_password) startMove(UNLOCKING);
    else Serial.println("Incorrect password");
    entered_password = "";
  } else if (state == LOCKED) {
    entered_password += key;
  }
}

void updateKeypad(uint32_t now) {
  if ((uint32_t)(now - last_keypad) < 20) return;
  last_keypad = now;
  analogRead(adc_pin); // Discard after switching from A1.
  char key = voltageToKey(analogRead(adc_pin) * (5.0 / 1023.0));

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

  pinMode(adc_pin, INPUT);
  pinMode(current_pin, INPUT);
  pinMode(9, OUTPUT);

  // clear Timer1 control registers
  TCCR1A = 0;
  TCCR1B = 0;

  TCNT1 = 0; // initializes counter value to 0
  ICR1 = 20000; // frequency 50 Hz
  OCR1A = 1000; // locked position 1 ms pulse width

  // Timer1 sends PWM on pin 9
  TCCR1A = _BV(COM1A1);
  TCCR1B = _BV(WGM13) | _BV(CS11);
  startMove(STARTING);
}

void loop() {
  uint32_t now = millis();
  if (Serial.available() && Serial.read() == 'S') motorFault();
  updateMotor(now);
  if (state == LOCKED || state == UNLOCKED || state == CHANGING_PASSWORD) {
    updateKeypad(now);
  }
}
