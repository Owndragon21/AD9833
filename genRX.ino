#include <SPI.h>
#include <Wire.h>

uint8_t lowByte = 0x00;
uint8_t highByte = 0x00;
uint32_t fullFreq =0x00000000;
bool wroteLSB = false;
uint16_t LSB,MSB;
//Map pins
#define FSYNC 10

uint16_t msbCommand = 0x00;
uint16_t lsbCommand = 0x00;


//Define contants for AD9833
const int SINE = 0x2000; //0010 0000 0000 0000                    
const int SQUARE = 0x2028; //0010 0000 0010 1000                  
const int TRIANGLE = 0x2002; //0010 0000 0000 0010

//set some default values
int waveform = SINE; //default waveform is sine.
long freq = 7777; //default frequency is 1 kHz.

void write_register(uint16_t data){
    SPI.setDataMode(SPI_MODE2); //IMPORTANT: This is the right SPI mode for AD9833.

    digitalWrite(FSYNC, LOW); //Set FSYNC low
    delayMicroseconds(10); //prepare for transfer

    SPI.transfer(highByte(data));
    SPI.transfer(lowByte(data));

    digitalWrite(FSYNC, HIGH); //Bring FSYNC back to high
}

void write_frequency(long frequency){
    uint16_t MSB,LSB;
    long freqWord;
    float divideRatio;
    divideRatio = (((float)(frequency)) / 25000000); // MCLK = 25 MHz
    freqWord = divideRatio * 0x10000000; //multiply by 2^28
  
    
    MSB = (uint16_t)((freqWord & 0xFFFC000) >> 14); //take only 14 low bits
    //Serial.println(MSB,HEX);
    LSB = (uint16_t)(freqWord & 0x3FFF); //take only 14 upper bits
    //Serial.println(LSB,HEX);
    LSB |= 0x4000; //01 ... Write to FREQ0 Register
    MSB |= 0x4000; //01 ... Write to FREQ0 Register 

    write_register(LSB); //FREQ0 Register's lower 14 bits
    write_register(MSB); //FREQ0 Register's upper 14 bits
    write_register(waveform); //Select the waveform
}

void decodeFreq() {
    // Combine MSB and LSB to form a 28-bit frequency word
    uint32_t freqData = ((uint32_t)msbCommand << 14) | (lsbCommand & 0x3FFF);

    // Calculate the frequency from the frequency word
    float divideRatio = (float)freqData / 0x40000000; // Assuming 28-bit frequency word
    long frequency = divideRatio * 25000000; // Assuming MCLK = 25 MHz

    Serial.print("Received Frequency Word: 0x");
    Serial.print(freqData, HEX);
    Serial.print(" Frequency: ");
    Serial.print(frequency);
    Serial.println(" Hz");
}



void setup() {
  Serial.begin(9600); // Set the same baud rate as the sender
  SPI.begin();

  write_register(0x100); //reset
  delay(10); //reset

  write_frequency(freq);
}


void loop() {
  uint8_t mode = 0x00;
  if(Serial.available() >= 3){
    mode = Serial.read();
    Serial.print("Mode:");
    Serial.println(mode,HEX);
    delay(10);
    if (mode == 0x0F || mode == 0xF0){
      if (mode == 0xF0){ //Write frequency
          lowByte = Serial.read();
          highByte = Serial.read();
          uint16_t command = (uint16_t)(highByte << 8) | lowByte; //16 bit command
          if (!wroteLSB){
            Serial.print("Wrote LSB of FREQ_REG.");
            //LSB = command&0x3FFF;
            lsbCommand = command & 0x3FFF;
            wroteLSB = true;
          } else {
            Serial.print("Wrote MSB of FREQ_REG.");
            //MSB = command&0x3FFF;
            //fullFreq = (uint32_t)(MSB&0x3FFF)<<14;
            //fullFreq |= (uint32_t)(LSB&0x3FFF);
            //fullFreq = fullFreq;
            //Serial.print(" Word:");
            //Serial.print(fullFreq);
            msbCommand = command & 0x3FFF;
            wroteLSB = false;
            //decodeFreq();
          }
          Serial.println(command,HEX);
          write_register(command);
          //decodeFreq(command);
          delay(10);
      } else {
            lowByte = Serial.read();
            highByte = Serial.read();
            uint16_t command = (uint16_t)(highByte << 8) | lowByte; //16 bit command
            Serial.print("SHAPE:");
            Serial.println(command,HEX);
            write_register(command);
            delay(10);
      }
    }
    else {
      Serial.println("Invalid mode");
    }
  }
  delay(10);
}
