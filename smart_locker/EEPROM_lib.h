// OVERVIEW /////////////////////////////////////////////////////////////////////////////////////////////////
// Most data that either written and/or read during program execution stored within the SRAM
// of the ATMEGA328P. While storing data in the SRAM is sufficient for many application, whose
// data become irrevelant upon disconnecting the Arduino from power, it becomes problematic when
// data need to retain even after "shut down", given that SRAM is a volatile type of memory. A
// Example of data that should be retain passed shut down is a password. To overcome this limitation,
// a developer must write to a form of non-volatile memory like FLASH or EEPROM.
//
// Flash while useful for storing data in large chunks such as the developer's program, it less useful
// for smaller chunks like passwords. EEPROM, unlike flash, is written byte-by-byte on the ATMEGA328P.
//
// NOTE: Every register within the ATmega328p has a word size of 8 bits.
// CAUTION: Never use this function within the main loop, that is, outside conditional, otherwise that
//          would effectively destroy the memory very quickly. Write/Erase take about 3.4ms, in other
//          words, it write about 294 times a second (0.294% of its total minimum life span per second).
//
// There are four memory address/registers that must be written (see page 20-21 of ATMEGA328P datasheet):
//    1. EEARH: The EEPROM Address Register high                                  (0x42)
//    2. EEARL: The EEPROM Address Register low                                   (0x41)
//    3. EEDR:  The data register of the EEPROM used for both read/write(erase)   (0x40)
//    4. EECR:  The control register of the EEPROM                                (0x3F)
//
//
// The EEPROM location are address linearly (section 7.6.1) of the datasheet. Importantly detail for EEARH,
// All bits (10-15) are read as zero. Despite the official datasheet claiming otherwise, the EEAR9 bit can
// be written to, just like every other EEAR bit. This a copy & paste error, according to stack-exchange
// (https://stackoverflow.com/questions/78286566/how-to-write-to-atmega328p-eeprom-without-supporting-libraries)
//
// FORMAT for EEARH (top, 15-8) and EEARL (bottom, 7-0)
//
// READ/WRITE     R     R     R     R     R     R     R/W   R/W   (EEARH)
//                R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   (EEARL)
// Initial Value  0     0     0     0     0     0     X     X
//                X     X     X     X     X     X     X     X
// 
// EDDR is the data register where data retrieval and data to be stored are written. For the EERPOM write
// operation, the EDDR register contains the data to be written to the EEPROM in the address, given by the
// EEAR register. For the EEPROM read operation, the EEDR contains the data read out from the EEPROM at the
// address given by EEAR.
//
// READ/WRITE     R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W
// Initial Value  0     0     0     0     0     0     0     0
//
// The EECR register controls the type of operations performed on a given EEPROM byte along with additional
// setting during said operations. The register can be divided in to different parts: EEPMn (bits 5-4),
// EERIE (bit 3), EEMPE (bit 2), EEPE (bit 1), EERE (bit 0). 
//
// EEPMn, is the programming modes bits. While EEPE is set, any write to EEPMn will be ignored. During reset,
// the EEPMn bits will be reset to 0b00 unless the EEPROM is busy programming. EEPM1 (bit 5) and EEPM2
// (bit 4) controls the type of operations:
//    00: Erase and write (atomic)
//    01: Erase Only
//    10: Write Only
//    11: Reserve for future use
//
// EERIE is ready interrupt Enable bit. Writing EERIE to one enables the EEPROM ready interrupt if the 1 bit
// in SREG set. Writing EERIE to zero disables the interrupt. The EEPROM ready interrupt generates a constant
// interrupt when EEPE is cleared. The interrupt will not be generated during EEPROM write or SPM.
//
// EEMPE is the master write enable, its state of this bit determines whether setting EEPE to one causes the
// EEPROM to be written. When EEMPE is set, setting EEPE within four clock cycles will write data to the
// EEPROM at the selected address. If EEMPE is zero, setting EEPE will have NO effect. When EEMPE has been
// written to one by software, hardware clears the bit to zero after four clock cycles.
//
// EEPE is the write strobe to the EEPROM. When address and data are correctly set up, the EEPE bit but must
// be written to ONE to write the value into the EEPROM. The EEMPE but must be written to one before a
// logical one is written to EEPE, otherwise no EEPROM write takes place. 
//
// EERE is the read enable bit, the read strobe to the EEPROM. When the correct address is set up in the
// EEAR register, the EERE bit must be written to a logic one to trigger the EEPROM read. The EEPROM read
// access takes one instruction, and the requested data is available immediately. When the EEPROM is read,
// the CPU is halted for four cycles before the next instruction is executed.
//
// READ/WRITE     R     R     R/W   R/W   R/W   R/W   R/W   R/W
// Initial Value  0     0     X     X     0     0     X     0
//
// General procedure for writting to EEPROM
//  1. Wait until EEPE becomes zero.
//  2. Wait until SELFPRGEN in SPMCSR becomes zero (irrevelant for our purposes, (0x57) bit 0)
//  3. Write new EEPROM address to EEAR (optional)
//  4. Write new EEPROM data to EEDR
//  5. Write a logical one to the EEMPE bit while writing a zero to EEPE in EECR.
//  6. Within four clock cycles after setting EEMPE, write a logical one to EEPE.
//
//
// General procedure for reading from EERPOM
//  1. Wait until EEPE becomes zero.
//  2. Wait until SELFPRGEN in SPMCSR becomes zero (irrevelant for our purposes, (0x57) bit 0)
//  3. Write new EEPROM address to EEAR (optional)
//  4. Retrieve data from EEDR register.
//
// Additional notes: 
//    * When writing to EEPROM, the CPU is halted for 2 clock cycles. 
//    * When reading from EEPROM, the CPU is halted for 4 clock cycles.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef EEPROM_lib
#define EEPROM_lib

//Specific to ATMEGA328P
#define EEPROM_SIZE 1023  //The ATMEGA328P uses 10 bits addressing, thus 1024 unique addresses (0 - 1023)

enum {EECR_WRITE, EECR_ERASE, EECR_WRITE_AND_ERASE}; //EECR control mode
enum {EEPROM_SUCCESSFUL, EEPROM_INVALID_ADDRESS, EEPROM_FAILED_TO_WRITE}; //Return Error codes

//Registers are memory mapped to these locations on the ATMEGA328P
/* Later found out these were already defined, same definition however */

//#define EEARH (*(volatile uint8_t*)0x42)  //Address Register High
//#define EEARL (*(volatile uint8_t*)0x41)  //Adress Register Low
//#define EEDR  (*(volatile uint8_t*)0x40)  //Data Register
//#define EECR  (*(volatile uint8_t*)0x3F)  //Control Register

//Bit masking for EECR register
#define EEPM1_BIT_MASK (1 << 5) //Mode select 1
#define EEPM0_BIT_MASK (1 << 4) //Mode select 2
#define EERIE_BIT_MASK (1 << 3) //Interrupt Enable
#define EEMPE_BIT_MASK (1 << 2) //Master Write Strobe
#define EEPE_BIT_MASK  (1 << 1) //Write Enable Strobe
#define EERE_BIT_MASK  (1 << 0) //Read Enable Strobe

//EEPROM read functions
uint8_t EEPROM_read(uint8_t *data, uint16_t EEPROM_adr); //Read one byte of data from EEPROM memory
uint8_t EEPROM_retrieve(uint8_t *data, uint16_t size, uint16_t EEPROM_adr); //Retrieves a batch of data from the EEPROM register (linearly) /* RECOMMEDED TO USE */

//EEPROM Write functions
uint8_t EEPROM_write(uint8_t data, uint16_t EEPROM_adr); //Basic write function, without concern of EEPROM longitivy. /* NOT RECOMMENDED TO USE */
uint8_t EEPROM_update(uint8_t data, uint16_t EEPROM_adr); //Extends upon the basic write function, checking if the same data already exist to improve life span of EEPROM, in addition, returns a error if the EEPROM failed to write
uint8_t EEPROM_update_array(uint8_t *data, uint16_t size, uint16_t EEPROM_adr); //Update multiple byte of EEPROM using the update function a it base. /* RECOMMEDED TO USE */

//EEPROM load data
void EEPROM_load_EEDR(uint8_t data);

//EEPROM Load Address Registers
void EEPROM_load_EEAR(uint16_t EEPROM_adr);

//Configure EEPROM Control Register
void EEPROM_config_EECR(uint8_t ctrl_mode, uint8_t enable_Interrupt);

#endif

