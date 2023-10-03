#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include<string.h>

//Map pins
#define FSYNC 10
#define CLK 5
#define DATA 4
#define PUSH 2

//Define contants for AD9833
const int SINE = 0x2000; //0010 0000 0000 0000                    
const int SQUARE = 0x2028; //0010 0000 0010 1000                  
const int TRIANGLE = 0x2002; //0010 0000 0000 0010

//set some default values
int waveform = SINE; //default waveform is sine.
long freq = 1000; //default frequency is 1 kHz.
long step = 100; //default is 100 Hz step.

static uint8_t prevNextCode = 0; //for encoder
static uint16_t store=0; //for encoder

enum Mode {FREQ,SHAPE,STEP,AMPL};
Mode mode = FREQ;

enum rotationDirection {LEFT,RIGHT,NOISE};
rotationDirection dir;

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
  Serial.print("Mode:");
  Serial.print(mode);
  Serial.print(" Shape:");
  Serial.print(wave);
  Serial.print(" Freq:");
  Serial.print(freq);
  Serial.print(" Step:");
  Serial.print(step);
  Serial.println(" ");
}

void check_button(bool& change){ //SUB FUNCTION TO CHANGE MODE
  if(!digitalRead(PUSH)){ //button pushed, connected to ground (LOW)
    change = true; 
    delay(200); //some delay for bouncing
    if (mode == Mode::FREQ) 
      mode = SHAPE; 
    else if (mode == Mode::SHAPE)
      mode = STEP;
    else if (mode == Mode::STEP)
      mode = AMPL;
    else if (mode == Mode::AMPL)
      mode = FREQ;
  }
}

/*
void printMode(){ //debug function
  if (mode == Mode::FREQ) {
      Serial.println("Mode: FREQ");
    }
    else if (mode == Mode::SHAPE){
      Serial.println("Mode: SHAPE");
    }
    else if (mode == Mode::STEP){
      Serial.println("Mode: STEP");
    }
    else if (mode == Mode::AMPL){
      Serial.println("Mode: AMPL");
    }
}
*/

void adjust_temp(){ // CORE FUNCTION [WIP]
    if (mode == Mode::FREQ){ //frequency adjust [WIP] 
      long temp = 0; //sanity check
      if (dir == rotationDirection::RIGHT){//right
          temp = freq + step;
          if (temp <= 1000000) //1 MHz limit
            freq+=step; //increase the frequency by given step
      }
      else if (dir == rotationDirection::LEFT){ //left
          temp = freq - step;
          if (temp >= 0) //sanity
            freq-=step; //decrease the frequency by given step
          else
            freq = 0;
      }
    }
  ////// NEXT MODE //////
    else if (mode == Mode::SHAPE){ //shape adjust [WIP]
      if (dir == rotationDirection::RIGHT){
        if (waveform == SINE)
          waveform = SQUARE;
        else if (waveform == SQUARE)
          waveform = TRIANGLE;
        else if (waveform == TRIANGLE)
          waveform = SINE;
      }
      else if (dir == rotationDirection::LEFT){
        if (waveform == SINE)
          waveform = TRIANGLE;
        else if (waveform == SQUARE)
          waveform = SINE;
        else if (waveform == TRIANGLE)
          waveform = SQUARE;
      }
    }
  ////// NEXT MODE ////// 
  else if (mode == Mode::STEP){ //adjust step [WIP]
    if (dir == rotationDirection::RIGHT){ //right
      if (step < 1000000) //sanity check
        step*=10; 
    }
    else if (dir == rotationDirection::LEFT){ //left
      if(step != 1) //sanity check
        step/=10;
    }
  }
  ///// NEXT MODE //////
  else if (mode == Mode::AMPL){ //TO DO [WIP]
    //For sure we need to map gain to selected signal shape.
    //Square Wave has significantly higher amplitude than the rest.
    
    }
}


int8_t read_rotary() {
  static int8_t rot_enc_table[] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0};

  prevNextCode <<= 2;
  if (digitalRead(DATA)) prevNextCode |= 0x02;
  if (digitalRead(CLK)) prevNextCode |= 0x01;
  prevNextCode &= 0x0f;

   // If valid then store as 16 bit data.
   if  (rot_enc_table[prevNextCode] ) {
      store <<= 4;
      store |= prevNextCode;
      //if (store==0xd42b) return 1;
      //if (store==0xe817) return -1;
      if ((store&0xff)==0x2b) return -1;
      if ((store&0xff)==0x17) return 1;
   }
   return 0;
}

void setup(){ 
  Serial.begin(9600);
  
  //Setup all pins
  pinMode(PUSH,INPUT_PULLUP); //change mode (test)
  pinMode(CLK,INPUT_PULLUP); //encoder pin A
  pinMode(DATA,INPUT_PULLUP); //encoder pin B

  SPI.begin(); //start the SPI communication
  reset(); //reset the AD9833 module

  write_frequency(freq); //write default: Sine 1 kHz
}

void loop(){
  bool change = false; //update the state of generator only when we need to
  static int8_t c,val;

  check_button(change); //switch mode if button was pressed
  if (val = read_rotary()) { //check if encoder has been rotated
    change = true;
    c += val;
    if (prevNextCode == 0x0b)
      dir = rotationDirection::LEFT;
    if (prevNextCode == 0x07)
      dir = rotationDirection::RIGHT;
    adjust_temp(); //adjust 
  }

  if (change) { //update only when we need to
   display(); //serial monitor debug
   write_frequency(freq); //set parameters
  }
}