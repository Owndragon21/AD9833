#include <SPI.h>
#include <Wire.h>
//#include <LiquidCrystal_I2C.h>
#include<string.h>

//Map pins
const int FSYNC = 10; 
const int PUSH = 2;
const int testButt = 3;

//Define contants for AD9833
const int SINE = 0x2000; //0010 0000 0000 0000                    
const int SQUARE = 0x2028; //0010 0000 0010 1000                  
const int TRIANGLE = 0x2002; //0010 0000 0000 0010

//set some default values
int waveform = SINE; //default waveform is sine.
long freq = 1000; //default frequency is 1 kHz.
int x = 0b0001; //cyclic for shape change
int step = 1; //default is 10 Hz step.

enum Mode {FREQ,SHAPE,STEP,AMPL};
Mode mode = FREQ;

void write_frequency(long frequency){
    int MSB,LSB;
    int phase = 0;
    long calculated_freq_word;
    float AD9833Val = 0.00000000;
    AD9833Val = (((float)(frequency)) / 25000000); // zegar DDS 25 MHz
    calculated_freq_word = AD9833Val * 0x10000000;
    MSB = (int)((calculated_freq_word & 0xFFFC000) >> 14);
    LSB = (int)(calculated_freq_word & 0x3FFF);
    LSB |= 0x4000;
    MSB |= 0x4000;
    phase &= 0xC000;
    write_register(0x2100);
    write_register(LSB);
    write_register(MSB);
    write_register(phase);
    write_register(waveform);
}
 
void write_register(uint16_t data){
    SPI.setDataMode(SPI_MODE2); //IMPORTANT: This is the right SPI mode for AD9833.

    digitalWrite(FSYNC, LOW); //Set FSYNC low
    delayMicroseconds(10); //prepare for transfer

    SPI.transfer(highByte(data));
    SPI.transfer(lowByte(data));

    digitalWrite(FSYNC, HIGH); //Bring FSYNC back to high
}

void reset(){
  write_register(0x100);
  delay(10);
}

void display(){
  String wave="";
  if (waveform == SINE)
    wave = "Sine";
  else if (waveform == SQUARE)
    wave = "Square";
  else if (waveform == TRIANGLE)
    wave = "Triangle";
  Serial.println(wave);
  Serial.println(freq);
}

void check_button(bool& change){
  if(!digitalRead(PUSH)){ //button pushed, connected to ground (LOW)
    change = true;
    if (mode == Mode::FREQ)
      mode = SHAPE;
    else if (mode == Mode::SHAPE)
      mode = STEP;
    else if (mode == Mode::STEP)
      mode = AMPL;
    else if (mode == Mode::AMPL)
      mode = FREQ;
    delay(50); //some delay for bouncing
  }
  printMode(); //debug function
}

void printMode(){ //debug function
  if (mode == Mode::FREQ) {
      //Serial.println("Mode: FREQ");
    }
    else if (mode == Mode::SHAPE){
      //Serial.println("Mode: SHAPE");
    }
    else if (mode == Mode::STEP){
      //Serial.println("Mode: STEP");
    }
    else if (mode == Mode::AMPL){
      //Serial.println("Mode: AMPL");
    }
}

void adjust(bool& change){ //WIP
  if (mode == Mode::FREQ){
      /* WIP
      if (true){ //right
          freq+=step;
      }
      else if (false){ //left
          freq-=step;
      } 
      */
  }
  else if (mode == Mode::SHAPE){
    int dir = 0;
    if(!digitalRead(testButt)){ //check encoder rotation //1 = SINE, 2 = SQUARE, 4 = TRIANGLE
      change = true;
      x <<= 1;
      if (x > 4)
        x = 0b0001;
      if (x == 1)
        waveform = SINE;
      else if (x == 2)
        waveform = SQUARE;
      else if (x == 4)
        waveform = TRIANGLE;
       delay(50);
    }
    /*
    else if(false){
      if (x == 1){
        x = 0b0100;
        waveform = TRIANGLE;
      }
      else{
        x >>= 1;
        if(x == 1)
          waveform = SINE;
        else if (x == 2){
          waveform = SQUARE;
        }
      }
      */
   }
  else if (mode == Mode::STEP){
    /*
    if (true){ //right
      if (step < 1000000)
        step*=10; 
      delay(100);
    }
    else if (false){ //left
      if(step != 1){
        step/=10;
      }
    }
    */
  }
  else if (mode == Mode::AMPL){
   //nop
  }
}

void setup(){ 
  Serial.begin(9600);
  pinMode(PUSH,INPUT_PULLUP);
  pinMode(testButt,INPUT_PULLUP);

  SPI.begin(); //start the SPI communication
  reset(); //reset the AD9833 module

  write_frequency(freq); //write default: Sine 1 kHz
}

void loop(){
  bool change = false;
  check_button(change); //switch mode if button was pressed
  adjust(change); //adjust values with encoder
  if(change){
    display();
    write_frequency(freq); //set parameters
  }
  //delay(200); //some delay is desired
  delay(100);
}
