const byte battery_pin = A2;
const uint16_t battery_scale_mv = 20152; // 5000 * (1000 + 330) / 330.
const uint16_t battery_low_mv = 7500; // Early warning above regulator dropout; verify under load.
const uint16_t battery_clear_mv = 8000;
const uint32_t battery_check_ms = 5000;
const uint32_t battery_reminder_ms = 60000; // Awake time; no wakeups to sound an alarm.
bool battery_low = false;
uint32_t battery_last_check = 0;
uint32_t battery_last_warning = 0 - battery_reminder_ms; // First warning is due immediately.

void setupBattery() {
  pinMode(battery_pin, INPUT);
  DIDR0 |= _BV(ADC2D); // No unused digital-input current at the divider voltage.
}

void wakeBattery(uint32_t now) {
  battery_last_check = now - battery_check_ms + 200; // Settle before checking.
  battery_last_warning = now - battery_reminder_ms;
}

void pauseBatteryWarning(uint32_t now) {
  if (feedback == UI_LOW_BATTERY && motorMoving()) {
    feedback = UI_NONE;
    battery_last_warning = now - battery_reminder_ms; // Retry after movement.
  }
}

// Milestone 4a: use the existing AVCC reference, only while stationary and awake.
void updateBattery(uint32_t now) {
  if (motorMoving()) {
    battery_last_check = now - battery_check_ms + 200; // Wait 200 ms after motion.
    return;
  }
  if (shouldSleep(now) || (uint32_t)(now - battery_last_check) < battery_check_ms) return;
  battery_last_check = now;

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
  if (battery_low && feedback == UI_NONE &&
      (uint32_t)(now - battery_last_warning) >= battery_reminder_ms) {
    battery_last_warning = now;
    showFeedback(UI_LOW_BATTERY);
    Serial.println(F("LOW BATTERY: replace 9V"));
  }
}

