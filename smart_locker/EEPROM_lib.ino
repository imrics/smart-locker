#include "EEPROM_lib.h"

uint8_t EEPROM_read(uint8_t *data, uint16_t EEPROM_adr) {
  //Check if the memory address is valid
  if (EEPROM_adr > EEPROM_SIZE) {
    return EEPROM_INVALID_ADDRESS;
  }

  //Wait for completion of the previous write command, if the device is writing
  while (EECR & EEPE_BIT_MASK);

  //Load the address register
  EEPROM_load_EEAR(EEPROM_adr);

  //Begin reading from EEPROM address
  EECR |= EERE_BIT_MASK; //Toggle the right strobe to 1

  //Pass data to the user address
  *data = EEDR; //Return the retrieve data from register to user
  return EEPROM_SUCCESSFUL;
}

uint8_t EEPROM_retrieve(uint8_t *data, uint16_t size, uint16_t EEPROM_adr) {
  uint8_t status = EEPROM_SUCCESSFUL;

  //Check for attempt out of bounds memory access
  if ( EEPROM_adr + (size - 1) > EEPROM_SIZE ) {
    return EEPROM_INVALID_ADDRESS;
  }

  //Itterate through each EEPROM Register
  for(uint16_t i = 0; i < size; i++) {
    status = EEPROM_read(data + i, EEPROM_adr + i);
    if (status != EEPROM_SUCCESSFUL) {
      return status;
    }
  }

  return status;
}

uint8_t EEPROM_write(uint8_t data, uint16_t EEPROM_adr) {
  //Check if the memory address is valid
  if (EEPROM_adr > EEPROM_SIZE) {
    return EEPROM_INVALID_ADDRESS;
  }

  //Wait for completion of the previous write command, if the device is writing
  while (EECR & EEPE_BIT_MASK);

  // Keep the master-enable/write sequence within four CPU cycles.
  uint8_t saved_sreg = SREG;
  noInterrupts();

  //Load data and address register
  EEPROM_load_EEAR(EEPROM_adr);
  EEPROM_load_EEDR(data);

  //Begin the writing process to EEPROM
  EECR |= EEMPE_BIT_MASK; //Enable master write strobe
  EECR |= EEPE_BIT_MASK;  //Enable write strobe
  SREG = saved_sreg;

  return EEPROM_SUCCESSFUL;
}

uint8_t EEPROM_update(uint8_t data, uint16_t EEPROM_adr) {
  uint8_t status = EEPROM_SUCCESSFUL;

  //Check if the data already exist in EEPROM (Saves EEPROM life span by preventing unncessary erase/write cycle)
  uint8_t tempValue = 0;
  status = EEPROM_read(&tempValue, EEPROM_adr);

  if (status == EEPROM_INVALID_ADDRESS) {
    return EEPROM_INVALID_ADDRESS;
  }
  if (tempValue == data) {
    return EEPROM_SUCCESSFUL; //Data is already there
  }

  //If the value is not the same, then write new value into register
  EEPROM_write(data, EEPROM_adr);

  //Check if data was succesfully written to memory address
  EEPROM_read(&tempValue, EEPROM_adr);

  if (tempValue != data) {
    return EEPROM_FAILED_TO_WRITE;
  }

  return status;
}

uint8_t EEPROM_update_array(uint8_t *data, uint16_t size, uint16_t EEPROM_adr) {
  uint8_t status = EEPROM_SUCCESSFUL;

  //Check for attemps to access out of bounds memory
  if ( EEPROM_adr + (size - 1) > EEPROM_SIZE ) {
    return EEPROM_INVALID_ADDRESS;
  }

  for(uint16_t i = 0; i < size; i++) {
    status = EEPROM_update(data[i], EEPROM_adr + i);
    if (status != EEPROM_SUCCESSFUL) {
      return status;
    }
  }

  return status;
}

void EEPROM_load_EEAR(uint16_t EEPROM_adr) {
  //Perform a logical bit shift 8 to store and store the two MSB into bits 1 and 0
  EEARH = (EEPROM_adr >> 8);
  //Serial.println((uint8_t)(EEPROM_adr >> 8)); /*Testing purpose*.

  //This move the first 8 bits of the 16 address into EEARL
  EEARL = (EEPROM_adr & 0xFF); //0xFF is the hexademical equivalent of 0b0011111111
  //Serial.println((uint8_t)EEPROM_adr); /*Testing purpose*/
}

void EEPROM_load_EEDR(uint8_t data) {
  EEDR = data; //Copy data to register
}

void EEPROM_config_EECR(uint8_t ctrl_mode, uint8_t enable_Interrupt) {
  EECR &= ~(EEPM1_BIT_MASK | EEPM0_BIT_MASK); //Clear data

  //Select mode of operation (ATMEGA328P datasheet p.32)
  switch (ctrl_mode) {
  case EECR_WRITE:
    EECR |= (EEPM1_BIT_MASK);
    break;
  case EECR_ERASE:
    EECR |= (EEPM0_BIT_MASK);
    break;
  case EECR_WRITE_AND_ERASE:
    EECR &= ~(EEPM1_BIT_MASK | EEPM0_BIT_MASK);
    break;
  }

  if(enable_Interrupt) { EECR |= EERIE_BIT_MASK; } //Turn on interrupts
  else { EECR &= ~EERIE_BIT_MASK; }                //Turn off interrupts
}
