const byte battery_pin = A2;
const uint16_t battery_scale_mv = 20152; // 5000 * (1000 + 330) / 330.
const uint16_t battery_low_mv = 7500; // Early warning above regulator dropout; verify under load.
const uint16_t battery_clear_mv = 8000;
bool battery_low = false;

void setupBattery() {
  pinMode(battery_pin, INPUT);
  DIDR0 |= _BV(ADC2D); // No unused digital-input current at the divider voltage.
}

void pauseBatteryWarning() {
  if (motorMoving()) cancelFeedback(UI_LOW_BATTERY);
}

// Check once on a user wake, before restoring servo power. The divider stays charged in sleep.
void wakeBattery() {
  analogRead(battery_pin); // Discard the first conversion after changing channel.
  uint16_t total = 0;
  for (byte i = 0; i < 8; i++) total += analogRead(battery_pin);
  uint16_t adc = (total + 4) / 8;
  uint16_t mv = ((uint32_t)adc * battery_scale_mv + 511) / 1023;
  Serial.print(F("Battery: "));
  Serial.print(mv);
  Serial.print(F(" mV, ADC="));
  Serial.println(adc);

  // Hysteresis prevents repeated low/healthy changes near the threshold.
  if (mv <= battery_low_mv) battery_low = true;
  else if (mv >= battery_clear_mv) battery_low = false;

  // Existing feedback takes priority. Never change the lock state or its LEDs.
  if (battery_low && !feedbackActive()) {
    showFeedback(UI_LOW_BATTERY);
    Serial.println(F("LOW BATTERY: replace 9V"));
  }
}

