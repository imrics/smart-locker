#ifndef SALT_H 
#define SALT_H

#include "EEPROM_lib.h"

#define SALT_SIZE 16
#define NUM_FLOATING_PINS 3

#define EEPROM_SALT_POS 500
#define EEPROM_SALT_COUNTER_POS 600

extern uint32_t salt_randomState;

//Generate the seed
String salt_generateSalt(uint32_t salt); //generateSalt
  void salt_collectSaltSeed(); //collectSeed
  uint32_t salt_generateRandom(); //generateRandom

void salt_storeSalt(String salt);
String salt_fetchSalt();

#endif