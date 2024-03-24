#include "AESLib.h"

#include  <SPI.h>
#include "RF24.h"

#define BAUD 9600
RF24 radio(9,8);
const uint64_t pipe = 0x8675309ABCll;

#define INPUT_BUFFER_LIMIT (100 + 1) // designed for Arduino UNO, not stress-tested anymore (this works with readBuffer[129])
AESLib aesLib;
unsigned char cleartext[INPUT_BUFFER_LIMIT] = {0}; // THIS IS INPUT BUFFER (FOR TEXT)
unsigned char ciphertext[2*INPUT_BUFFER_LIMIT] = {0}; // THIS IS OUTPUT BUFFER (FOR BASE64-ENCODED ENCRYPTED DATA)
int cipherindex = 0;
bool strt_send = false;
bool pst_ava = false;


// AES Encryption Key (same as in node-js example)
byte aes_key[16] = {0};

// General initialization vector (same as in node-js example) (you must use your own IV's in production for full security!!!)
byte aes_iv[N_BLOCK] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

// Generate IV (once)
void aes_init() {
  aesLib.gen_iv(aes_iv);
  aesLib.set_paddingmode((paddingMode)0);
}

uint16_t encrypt_to_ciphertext(char * msg, uint16_t msgLen, byte iv[]) {
  Serial.println("Calling encrypt (string)...");
  // aesLib.get_cipher64_length(msgLen);
  int cipherlength = aesLib.encrypt((byte*)msg, msgLen, (char*)ciphertext, aes_key, sizeof(aes_key), iv);
  // uint16_t encrypt(byte input[], uint16_t input_length, char * output, byte key[],int bits, byte my_iv[]);
  return cipherlength;
}

uint16_t decrypt_to_cleartext(byte msg[], uint16_t msgLen, byte iv[]) {
  Serial.print("Calling decrypt...; ");
  uint16_t dec_bytes = aesLib.decrypt(msg, msgLen, (char*)cleartext, aes_key, sizeof(aes_key), iv);
  Serial.print("Decrypted bytes: "); Serial.println(dec_bytes);
  return dec_bytes;
}

// Working IV buffer: Will be updated after encryption to follow up on next block.
// But we don't want/need that in this test, so we'll copy this over with enc_iv_to/enc_iv_from
// in each loop to keep the test at IV iteration 1. We could go further, but we'll get back to that later when needed.

// General initialization vector (same as in node-js example) (you must use your own IV's in production for full security!!!)
//byte enc_iv[N_BLOCK] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

bool word_start = true;
bool word_end = false;
int pos = 0;
int ind = 0;
char readBuffer[120] = "";
int byte_pos = 0;
void setup(){
  pinMode(0,INPUT);
  pinMode(1,INPUT);
  pinMode(2,INPUT);
  pinMode(3,INPUT);
  pinMode(4,INPUT);
  pinMode(5,INPUT);
  pinMode(6,INPUT);
  pinMode(7,INPUT);
  pinMode(10,INPUT);
  pinMode(A0,INPUT);
  pinMode(13,OUTPUT);
}
void loop(){
  while(PINC & 1){
    if(!strt_send){
      continue;
    }
    if(strt_send && ind > 200){
      strt_send = false;
      ind = 0;
      continue;
    }
    send_mgs();    
  }
  ind++;
  if(read_b(2)){
    if(!word_start){
      word_start = true;
      clrRead();
    }
    word_end = true;
  }
  else{
    if(word_end){
      word_end = false;
      //send();
      writeBuffer();
      clrRead();
    }
    word_start = false;
  }
  if(byte_pos<8){
    byte_pos++;
    write13(cleartext[pos-1] & 1);
    cleartext[pos-1] = cleartext[pos-1] >> 1;
  }
  else{
    byte_pos = 0;
    if(read_b(2)){
      if(pos < 100){
        readBuffer[pos] = PIND;
        pos++;
      }
      else{
        //send();
        writeBuffer();
        pos = 0;
      }
    }
  }
  while(!(PINC & 1));
  if(!read_b(2)){
    cipherindex = 0;
  }
  else{
    if(cipherindex >= 200){
      cipherindex = 0;
      //decrypt amd play
    }
    if(radio.available()){
      if(pst_ava == false){
        cipherindex = 0;
      }
      radio.read(&ciphertext + cipherindex, 1);
      cipherindex++;
      pst_ava = true;    
    }
    else{
      pst_ava = false;
    }
  }
}

bool read_b(uint8_t pin){
  return (bool)((PINB & (1 << pin)) >> pin);
}
void write13(int bit){
  if(bit){
    PORTB = PORTB | B00100000;
  }
  else{
    PORTB = PORTB & B11011111;
  }
}

void clrRead(){
  pos = 0;
}

void play_sound(){
  
}
void writeBuffer(){
  for (int i=0;i<pos;i++){
    //Serial.print(readBuffer[i]);
    cleartext[i] = readBuffer[i];
  }
  for(int i=pos;i<100;i++){
    cleartext[i] = '\0';
  }
  //Serial.print("\n");
}
void send_mgs(){
  encrypt_mgs();
  radio.write(&ciphertext+ind, 1);
}
void encrypt_mgs(){
  aes_init();
  uint16_t msgLen = sizeof(readBuffer);
  memcpy(aes_iv, aes_iv, sizeof(aes_iv));
  encrypt_to_ciphertext((char*)cleartext, msgLen, aes_iv);
  strt_send = true;
  ind = 0;
}