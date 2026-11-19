#include "salt.h"

uint32_t salt_randomState = 0;

String salt_generateSalt() {
  String salt = "";
  String saltWithCounter = "";

  uint32_t value = 0;
  uint8_t counterVal = 0;

  salt_collectSaltSeed();
  for(uint8_t i = 0; i < SALT_SIZE; i += 4) {
    value = salt_generateRandom();
    for(uint8_t j = 0; j < 4 && (i + j) < SALT_SIZE; j++) {
      uint8_t byteVal = (value >> (8 * j)) & 0xFF;
      if (byteVal == 0) byteVal = 1;
      salt += (char)byteVal;
    }
  }

  EEPROM_read(&counterVal, EEPROM_SALT_COUNTER_POS);

  for(uint8_t i = 0; i < SALT_SIZE; i++) {
    uint8_t byteVal = (i % 2) ? ((uint8_t)salt[i] ^ counterVal) : (uint8_t)salt[i];
    if (byteVal == 0) byteVal = 1;
    saltWithCounter += (char)byteVal;
  }
  return saltWithCounter;
}

void salt_collectSaltSeed() {
  uint32_t seed = 0;
  uint8_t mod = 0;

  for(uint8_t i = 0; i < 32; i++) {
    seed = seed << 1;
    mod = i % NUM_FLOATING_PINS;
    if(mod == 0) { seed ^= analogRead(A2) & 0x1; }
    else if(mod == 1) { seed ^= analogRead(A3) & 0x1; }
    else if(mod == 2) { seed ^= analogRead(A4) & 0x1; }
  }

  seed ^= (uint32_t)(millis());

  //Insert seed to the random state variable
  if(seed == 0) { salt_randomState = 0xA3FAB12C; }
  else { salt_randomState = seed; }
}

uint32_t salt_generateRandom() {
  salt_randomState ^= salt_randomState << 13;
  salt_randomState ^= salt_randomState >> 17;
  salt_randomState ^= salt_randomState << 5;

  return salt_randomState;
}

void salt_storeSalt(String salt) {
  uint8_t temp = 0;

  EEPROM_update_array((uint8_t *)salt.c_str(), SALT_SIZE, EEPROM_SALT_POS);

  //update counter
  EEPROM_read(&temp, EEPROM_SALT_COUNTER_POS);

  temp++;
  EEPROM_update(temp, EEPROM_SALT_COUNTER_POS);
}

String salt_fetchSalt() {
  String salt = "";
  uint8_t tempChar = 0;

  for(uint16_t i = 0; i < SALT_SIZE; i++) {
    EEPROM_read(&tempChar, EEPROM_SALT_POS + i);
    if (tempChar == 0) tempChar = 1;
    salt += (char)tempChar;
  }

  return salt;
}