#include <SPI.h>
#include <Wire.h>

uint8_t lowByte = 0x00;
uint8_t highByte = 0x00;
uint32_t fullFreq =0x00000000;
bool wroteLSB_FREQ0 = false;
bool wroteLSB_FREQ1 = false;
uint16_t LSB,MSB;
//Map pins
#define FSYNC 10



//Define contants for AD9833
const int SINE = 0x2000; //0010 0000 0000 0000                    
const int SQUARE = 0x2028; //0010 0000 0010 1000                  
const int TRIANGLE = 0x2002; //0010 0000 0000 0010

//set some default values
int waveform = SINE; //default waveform is sine.
long freq = 7777; //default frequency is 1 kHz.
const float ratio = ((float)25000000)/((float)0x10000000); //0.093132

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

void decodeFreq_UART(uint16_t LSB,uint16_t MSB){
  LSB &= 0xBFFF; 
  MSB &= 0xBFFF; 
  long freqWord = ((long)MSB << 14) | LSB;
  float divide = (float)freqWord / 0x10000000;
  long freq = (long)(divide * 25000000);
  freq++;
  Serial.print("freq decode:");
  Serial.println(freq,DEC);
  return;
}


void setup() {
  Serial.begin(9600); // Set the same baud rate as the sender
  SPI.begin();

  write_register(0x100); //reset
  delay(10); //reset

  write_frequency(freq);
}

/*D15 & D14 Table:
  00 -> Control word write
  01 -> FREQ0 REG write
  10 -> FREQ1 REG write
  11 -> Phase reg write
*/
void loop() {
  uint16_t command = 0x0000;
  if(Serial.available() >= 2){ //16 bit command recieved 
    lowByte = Serial.read();
    highByte = Serial.read();
    command = (uint16_t)(highByte << 8) | lowByte; //16 bit command
    Serial.print("Recieved command:");
    Serial.println(command,HEX);
    switch (command & 0xC000){
      case 0x0000:
        Serial.println("Control word write.");
        //lets decode the waveform
        if ((command & 0x0800 != 0)){
          Serial.println("Changing FSELECT is not allowed!"); //ABORT
          break;
        }
        if ((command & 0x0020) != 0){
          Serial.println("Waveform is Square");
          waveform = SQUARE;
        } else {
          if ((command & 0x0002) != 0){
            Serial.println("Waveform is Triangle");
            waveform = TRIANGLE;
          }
          else {
            Serial.println("Waveform is Sine");
            waveform = SINE;
          }
        }
        write_register(command);
        break;
      case 0x4000: //NOTE: We need to ensure that proper frequency register is selected for phase accumulation
        Serial.println("FREQ0 REG write.");
        if(!wroteLSB_FREQ0){
          LSB = command;
          wroteLSB_FREQ0 = true;
        } else { 
          MSB = command;
          write_register(LSB);
          write_register(MSB);
          decodeFreq_UART(LSB,MSB);
          wroteLSB_FREQ0 = false;
        }
        break;
      case 0x8000: //NOTE: We need to ensure that proper frequency register is selected for phase accumulation
        Serial.println("FREQ1 REG write.");
        /* NOT SUPPORTED
        if(!wroteLSB_FREQ1){
          LSB = command;
          wroteLSB_FREQ1 = true;
        } else { 
          MSB = command;
          write_register(LSB);
          write_register(MSB);
          wroteLSB_FREQ1 = false;
        }
        */
        break;
      case 0xC000: // NOT SUPPORTED
        Serial.println("Phase reg write.");
        break;
      break;
    }
  }
  delay(10);
}
