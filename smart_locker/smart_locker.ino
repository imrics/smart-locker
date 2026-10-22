#include "EEPROM_lib.h"

const int adc_pin = A0;
char pressed_key = '\0';

// password storage variables
String valid_password = "1234";
String entered_password = "";
bool is_locked = true;

// state variables for password change
bool setting_password = false;
int hold_count = 0;

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
}

void loop() {
  int adc_value = analogRead(adc_pin);
  float voltage = adc_value * (5.0 / 1023.0);

  char instantaneous_key = voltageToKey(voltage);

  if (instantaneous_key == '#' && !is_locked && !setting_password) {
    hold_count++;
    if (hold_count == 250) {
      setting_password = true;
      entered_password = "";
      Serial.println("Enter new password, then press #");
    }
  }

  if (pressed_key == '\0') {
    pressed_key = instantaneous_key;
  } else if (instantaneous_key == '\0') {
    // print key when it's released
    if (hold_count < 250) {
      Serial.println(pressed_key);
    }

    if (hold_count == 250) {
      hold_count = 0;
    } else if (setting_password) {
      if (pressed_key == '*') {
        entered_password = "";
        setting_password = false;
        Serial.println("Password change canceled");
      } else if (pressed_key == '#') {
        if (entered_password != "") {
          if (EEPROM_update_array((uint8_t *)entered_password.c_str(),
                                  entered_password.length() + 1, 0) != EEPROM_SUCCESSFUL) {
            Serial.println("Password save failed");
          } else {
            valid_password = entered_password;
            entered_password = "";
            setting_password = false;
            Serial.println("Password changed");
          }
        } else {
          Serial.println("Enter a password first");
        }
      } else {
        entered_password += pressed_key;
      }
    } else if (pressed_key == '*') {
      // 1 ms pulse to lock
      OCR1A = 1000;
      Serial.println("Locked");

      entered_password = "";
      is_locked = true;

      // added check for password when trying to unlock
    } else if (is_locked && pressed_key == '#') {
      if (entered_password == valid_password) {
        // 2 ms pulse to unlock
        OCR1A = 2000;
        Serial.println("Unlocked");

        is_locked = false;
      } else {
        Serial.println("Incorrect password");
      }

      entered_password = "";
    } else if (is_locked) {
      entered_password += pressed_key;
    }
    
    pressed_key = '\0';
  }

  if (instantaneous_key != '#') {
    hold_count = 0;
  }

  delay(20);
}
