#ifndef HASHBROWN_H
#define HASHBROWN_H

//This array alone takes 256 Bytes of memory, however, this is easier to implement and faster then calculating the
//hexadecimal equivalent of the fractional part of the cube root of first eighty prime numbers
uint32_t _K[] = 
       {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
/* 
* To reduce the memory footprint, rather than making functions
* macros are define with the bitwise operation required, reducing
* each function call by 12 bytes. Which isn't much when I write it out.
*/

//FUNCTIONS/OPERATIONS
//=========================================================================
#define ROTR(x,n) ( (x >> n) | (x << (32 - n)) )
#define SHR(x,n) (x >> n)

#define sigma0(x) ( ROTR(x, 7) ^ ROTR(x, 18) ^ SHR(x, 3) )
#define sigma1(x) ( ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10) )
#define capSigma0(x) ( ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22) )
#define capSigma1(x) ( ROTR(x,6) ^ ROTR(x, 11) ^ ROTR(x, 25) )

#define choice(x,y,z) ( (x & y) ^ (~x & z) )
#define majority(x,y,z) ( (x & y) ^ (x & z) ^ (y & z) )

//To find the initial hash values for the 8 H0, convert the fractional part of the first 8 prime numbers into hexadecimal
  //H0 = 0x6a09e667 //sqrt(2)
  //H1 = 0xbb67ae85 //sqrt(3)
  //H2 = 0x3c6ef372 //sqrt(5)
  //H3 = 0xa54ff53a //sqrt(7)
  //H4 = 0x510e527f //sqrt(11)
  //H5 = 0x9b05688c //sqrt(13)
  //H6 = 0x1f83d9ab //sqrt(17)
  //H7 = 0x5be0cd19 //sqrt(19)

//Calculating the Temp (T) values
#define T1(h, e, f, g, W, t) ( h + capSigma1(e) + choice(e, f, g) + _K[t] + W )
#define T2(a, b, c) ( capSigma0(a) + majority(a, b, c) )

enum {a, b, c, d, e, f, g, h}; //Working variable position in array

typedef struct _BinaryBlock {
  uint8_t* memAddress;
  uint32_t sizeOfBlock;
} BINARY_BLOCK;

String hashBrown(const String &password);

BINARY_BLOCK constructBinaryBlock(uint64_t StrLength);
void insertDataToBinaryBlock(const String &password, BINARY_BLOCK* binBlock);
void setupMessageSchedule(uint8_t binaries[], uint32_t schedule[], uint16_t N);
void updateWorkingVar(uint32_t Var[], uint32_t message);
String BinHashtoChar(uint32_t hash[]);

#endif
