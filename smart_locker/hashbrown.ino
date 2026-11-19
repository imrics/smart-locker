String hashBrown(const String &password) {
  /* Padding the the binary array */
  BINARY_BLOCK binBlock;
  binBlock = constructBinaryBlock(password.length());

  //Check if the block was allocated
  if(binBlock.memAddress == NULL) { return ""; } //Allocation failed

  /* Inserting data to the memory block */
  insertDataToBinaryBlock(password, &binBlock);

  /* Initialize the message shechuler*/
  //The message block store 32 bit words (4 Bytes), and contains
  //64 words, each element in the array is its own word.
  uint32_t messageBlock[64] = {0};

  /* Initial Hash values (See header file for how the values were calculated) */
  uint32_t H[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

  /* Split binary block into individual 512 bit blocks (64 Byte) */
  uint16_t Nblocks = (binBlock.sizeOfBlock) / 64;

  /* Computation */
  uint32_t workingVar[8] = {0}; //Working Variables

  for(uint16_t N = 0; N < Nblocks; N++) {
    setupMessageSchedule(binBlock.memAddress, messageBlock, N);
    //set working vairable to H^{i-1}
    for(uint8_t i = 0; i < 8; i++) { workingVar[i] = H[i]; }

    //Run compression
    updateWorkingVar(workingVar, messageBlock);
    //Update hash values with working variables
    for(uint8_t i = 0; i < 8; i++) { 
      H[i] = workingVar[i] + H[i]; 
    }
  }

  /* Clean up and return hash password*/
  free(binBlock.memAddress);
  binBlock.memAddress = NULL;
  /* Convert the 8 hashes into a 64 character string */
  return BinHashtoChar(H);
}

BINARY_BLOCK constructBinaryBlock(uint64_t StrLength) {
  BINARY_BLOCK binaryBlock;

  //64 bit are needed at the end, +1 for padding
  uint64_t bitLength = StrLength * 8; //number of bits in the string ('\n' is excluded)
  uint16_t bitsToFill = 0;
  
  //Determine the number of bit needed to fill by zeros (the length must be a multiple of 512 for SHA256).
  if((bitLength % 512) + 64 + 1 > 512) { bitsToFill = 1024 - ((bitLength % 512) + 64 + 1); }
  else { bitsToFill = (512 - bitLength % 512) - 64 - 1; }

  //allocate memory for binary block
  
  binaryBlock.sizeOfBlock = (bitLength + bitsToFill + 64 + 1) / 8; //bytes
  binaryBlock.memAddress = (uint8_t *)malloc(binaryBlock.sizeOfBlock * sizeof(uint8_t));
  if(binaryBlock.memAddress == NULL) { return binaryBlock; } //Allocation failed
  memset(binaryBlock.memAddress, 0, binaryBlock.sizeOfBlock); //set all bytes to zero
  return binaryBlock;
}

void insertDataToBinaryBlock(const String &password, BINARY_BLOCK* binBlock) {
  //Insert ascii character in the beginning, then append is a 1 bit.
  //NOTE: remaining bits are already set to zero
  for(uint16_t i = 0; i < password.length(); i++) {
    binBlock->memAddress[i] = (uint8_t)password[i];
  }

  //Append with 1 bit
  binBlock->memAddress[password.length()] = 0x80;

  //Start at the end of the binary block, fill the cell with 8 bits of data,
  //then move to the next adjecent block to the left. For cell shift, shift
  //the data 8 bit to the right.
  uint64_t bitLength = password.length() * 8;
  for(uint8_t i = 0; i < 8; i++) {
    binBlock->memAddress[(binBlock->sizeOfBlock - 1) - i] = 
                        (uint8_t)(bitLength >> (i*8));
  }
}

void setupMessageSchedule(uint8_t binaries[], uint32_t schedule[], uint16_t N) {
  //W0 to W15 is equal to the first 16 words(32 bits) of M0 to M15,
  //where each M is 4 elements in a uint8_t array.
  for(uint8_t i = 0; i < 16; i++) {
    schedule[i] = 0; //Clear before loading data
    for(uint8_t j = 0; j < 4; j++) {
      //Bit shifting binaries into 32 bit block
      schedule[i] |= ((uint32_t)binaries[(N*64)+4*i+j] << ((3-j)*8));
    }
  }

  //Start from W16 to W63 of the message schedule using the formula
  //describe in the pdf for sha-256
  for(uint8_t t = 16; t < 64; t++) {
    schedule[t] = sigma1(schedule[t-2]) + schedule[t-7] + 
                   sigma0(schedule[t-15]) + schedule[t-16];
  }
}

void updateWorkingVar(uint32_t Var[], uint32_t message[]) {
  uint32_t Temp1, Temp2;

  for(int8_t t = 0; t < 64; t++) {
    Temp1 = T1(Var[h], Var[e], Var[f], Var[g], message[t], t);
    Temp2 = T2(Var[a], Var[b], Var[c]);
    Var[h] = Var[g];
    Var[g] = Var[f];
    Var[f] = Var[e];
    Var[e] = Var[d] + Temp1;
    Var[d] = Var[c];
    Var[c] = Var[b];
    Var[b] = Var[a];
    Var[a] = Temp1 + Temp2;
  }
}

String BinHashtoChar(uint32_t hash[]) {
  /*Turn hash into a 64 character string*/
  String hashPassword = "";
  uint8_t tempByte = 0;
  for(int8_t i = 0; i < 8; i++) {
    for(int8_t j = 28; j >= 0; j -= 4) {
      tempByte = (uint8_t)(0x0F & (hash[i] >> j));
      hashPassword += "0123456789ABCDEF"[tempByte];
    }
  }

  return hashPassword;
}
