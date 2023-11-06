#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>

//Map pins
#define FSYNC 10
#define CLK 5
#define DATA 4
#define PUSH 2
#define RELAY 6

//Define contants for AD9833
const int SINE = 0x2000; //0010 0000 0000 0000                    
const int SQUARE = 0x2028; //0010 0000 0010 1000                  
const int TRIANGLE = 0x2002; //0010 0000 0000 0010

//set some default values
int waveform = SINE; //default waveform is sine.
long freq = 1000; //default frequency is 1 kHz.
long step = 100; //default is 100 Hz step.
const long freqLimit = 5000000; //5MHz limit

//UART variables
uint8_t lowByte = 0x00;
uint8_t highByte = 0x00;
uint16_t LSB,MSB;
bool wroteLSB_FREQ0 = false;
bool wroteLSB_FREQ1 = false;
bool isUART = false;

//Encoder variables
static uint8_t prevNextCode = 0; 
static uint16_t store=0;
static int8_t c,val; 
unsigned long pushStart = 0; 
unsigned long pushEnd = 0;
unsigned long pushDuration = 0;




enum Mode {FREQ,SHAPE,STEP,AMPL};
Mode mode = FREQ;

enum rotationDirection {LEFT,RIGHT,NOISE};
rotationDirection dir;

LiquidCrystal_I2C lcd(0x20, 20, 4);  // Configure LiquidCrystal_I2C library with 0x27 address, 16 columns and 2 rows

/*
void write_frequency(long frequency){
    int MSB,LSB; //int is 32 bits
    int phase = 0;
    long calculated_freq_word;
    float AD9833Val = 0.00000000;
    AD9833Val = (((float)(frequency)) / 25000000); // 25 MHz clock
    calculated_freq_word = AD9833Val * 0x10000000;
    MSB = (int)((calculated_freq_word & 0xFFFC000) >> 14); //take only 14 low bits
    LSB = (int)(calculated_freq_word & 0x3FFF); //take only 14 upper bits
    LSB |= 0x4000;
    MSB |= 0x4000;
    phase &= 0xC000;
    write_register(0x2100);
    write_register(LSB); //freq0
    write_register(MSB); //freq1
    write_register(phase);
    write_register(waveform);
}
*/
 
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

void set_default(){
    int MSB,LSB; //int is 32 bits
    long calculated_freq_word;
    //// DEFAULT VALUES ////
    long frequency = 1000;
    int phase = 0;
    freq = 1000;
    waveform = SINE;
    step = 100;
    mode = FREQ;
    //// DEFAULT VALUES ////
    float AD9833Val = 0.00000000;
    AD9833Val = (((float)(frequency)) / 25000000); // 25 MHz clock
    calculated_freq_word = AD9833Val * 0x10000000;
    MSB = (int)((calculated_freq_word & 0xFFFC000) >> 14); //take only 14 low bits
    LSB = (int)(calculated_freq_word & 0x3FFF); //take only 14 upper bits
    LSB |= 0x4000;
    MSB |= 0x4000;
    phase &= 0xC000;
    write_register(0x2100); 
    write_register(LSB); //freq0
    write_register(MSB); //freq1
    write_register(phase);
    write_register(waveform);
}
 

void display(){ //ONLY FOR SERIAL MONITOR (testing)
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

void printFrequency(){ //separate MHz, kHz and Hz.
  if (freq < 0){ //something went wrong...
    lcd.print("Error");
    return;
  }
  unsigned char digits[7];
  digits[6]=(freq/1000000) %10; //1MHz  
  digits[5]=(freq/100000) %10;  //100kHz
  digits[4]=(freq/10000) %10;   //10kHz
  digits[3]=(freq/1000) %10;    //1kHz
  digits[2]=(freq/100) %10;     //100Hz 
  digits[1]=(freq/10) %10;      //10Hz   
  digits[0]=freq %10;           //1Hz

  lcd.print(digits[6],DEC);
  lcd.print(" , ");
  lcd.print(digits[5],DEC);
  lcd.print(digits[4],DEC);
  lcd.print(digits[3],DEC);
  lcd.print(" , ");
  lcd.print(digits[2],DEC);
  lcd.print(digits[1],DEC);
  lcd.print(digits[0],DEC);
}

void printStep(){
  if (step < 0){ //something went wrong...
      lcd.print("Error");
      return;
  }
  if (step == 1000000) //1 MHz step
    lcd.print("step:1MHz  ");
  else if (step == 100000) //100kHz step
    lcd.print("step:100kHz");
  else if (step == 10000) //10kHz step
    lcd.print("step:10kHz ");
  else if (step == 1000)  //1kHz step
    lcd.print("step:1kHz  ");
  else if (step == 100) //100 Hz step
    lcd.print("step:100Hz ");
  else if (step == 10)
    lcd.print("step:10Hz  ");
  else if (step == 1)
    lcd.print("step:1Hz   ");
  else 
    lcd.print("Error      ");
  return;
}

void printShape(){
  if (waveform == SINE)
    lcd.print("shape:Sine    ");
  else if (waveform == TRIANGLE)
    lcd.print("shape:Triangle");
  else if (waveform == SQUARE)
    lcd.print("shape:Square  ");
  else
    lcd.print("Error"); //something went wrong...
  return;
}

void display_LCD(){ //(col,row)
  // if (mode == Mode::STEP || mode == Mode::SHAPE || isUART) //we need this
  //   lcd.clear();
  ///////////////////////////////
  lcd.setCursor(0,0); //PRINT FREQ
  if (mode == Mode::FREQ)
    lcd.print("*");
  else
    lcd.print(" ");
  lcd.setCursor(1,0);
  lcd.print("f:");
  lcd.setCursor(4,0); //f:0,000,000
  //here print freq  // MHz kHz Hz 
  printFrequency();
  //////////////////////////////
  lcd.setCursor(4,1); //static row
  lcd.print("MHz  kHz  Hz"); //static row
  //////////////////////////////
  lcd.setCursor(0, 2); //PRINT STEP
   if (mode == Mode::STEP)
    lcd.print("*");
  else
    lcd.print(" ");
  lcd.setCursor(1,2);
  printStep();
  /////////////////////////////
  lcd.setCursor(0,3); //PRINT SHAPE
  if (mode == Mode::SHAPE)
    lcd.print("*");
  else
    lcd.print(" ");
  lcd.setCursor(1,3);
  printShape();
}

void checkButton(/*bool& change*/){ //SUB FUNCTION TO CHANGE MODE
  if(!digitalRead(PUSH)){ //button pushed, connected to ground (LOW)
    pushStart = millis();
    while(!digitalRead(PUSH))
      pushEnd = millis();
    pushDuration = pushEnd - pushStart;
    if (pushDuration >= 1000){ //RESET
      Serial.println("RESET");
      set_default();
      display_LCD();
      display();
      return; //escape
    }
    //Serial.println(pushDuration);
    //change = true; 
    delay(50); //some delay for bouncing
    switch (mode){
      case Mode::FREQ:
        mode = STEP;
        break;
      case Mode::STEP:
        mode = SHAPE;
        break;
      case Mode::SHAPE:
        mode = FREQ;
        break;
      default:
        break;
    }
    display_LCD();
    display();
  } //CURRENT STATE DIRECTION: FREQ->STEP->SHAPE->FREQ->...
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

void set_frequency(long frequency){
    int MSB,LSB; //int is 32 bits
    long calculated_freq_word;
    float AD9833Val = 0.00000000;
    AD9833Val = (((float)(frequency)) / 25000000); // 25 MHz clock
    calculated_freq_word = AD9833Val * 0x10000000;
    MSB = (int)((calculated_freq_word & 0xFFFC000) >> 14); //take only 14 low bits
    LSB = (int)(calculated_freq_word & 0x3FFF); //take only 14 upper bits
    LSB |= 0x4000; //write to FREQ0 
    MSB |= 0x4000; //write to FREQ0
    //write_register(0x2100); check if needed
    write_register(LSB);
    write_register(MSB); 
}

/*
void adjust_temp(){ // CORE FUNCTION [WIP]
    if (mode == Mode::FREQ){ //frequency adjust [WIP] 
      long temp = 0; //sanity check
      if (dir == rotationDirection::RIGHT){//right
          temp = freq + step;
          if (temp <= freqLimit) //5 MHz limit
            freq+=step; //increase the frequency by given step
          else
            freq = freqLimit; //or set it to max
      }
      else if (dir == rotationDirection::LEFT){ //left
          temp = freq - step;
          if (temp > 0) //sanity
            freq-=step; //decrease the frequency by given step
          else
            freq = 0; //or set it to min
      }
      set_frequency(freq); //DONE
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
      write_register(waveform); //DONE
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
*/
void adjust_temp(){ // CORE FUNCTION [WIP]
  switch (mode){
    case Mode::FREQ:
      long temp = 0; //sanity check
      if (dir == rotationDirection::RIGHT){ //right
          temp = freq + step;
          if (temp <= freqLimit) //5 MHz limit
            freq+=step; //increase the frequency by given step
          else
            freq = freqLimit; //or set it to max
      }
      else if (dir == rotationDirection::LEFT){ //left
          temp = freq - step;
          if (temp > 0) //sanity
            freq-=step; //decrease the frequency by given step
          else
            freq = 0; //or set it to min
      }
      set_frequency(freq); //DONE
      break;
    ////// NEXT MODE //////
    case Mode::SHAPE:
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
      write_register(waveform); //DONE
      break;
    ////// NEXT MODE ////// 
    case Mode::STEP: //adjust step [WIP]
      if (dir == rotationDirection::RIGHT){ //right
        if (step < 1000000) //sanity check
          step*=10; 
      }
      else if (dir == rotationDirection::LEFT){ //left
        if(step != 1) //sanity check
        step/=10;
      }
      break;
    default:
      break;
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
      if ((store&0xff)==0x2b) return -1;
      if ((store&0xff)==0x17) return 1;
   }
   return 0;
}

void decodeFreq_UART(uint16_t LSB,uint16_t MSB){
  LSB &= 0xBFFF; 
  MSB &= 0xBFFF; 
  long freqWord = ((long)MSB << 14) | LSB;
  float divide = (float)freqWord / 0x10000000;
  long freqDecoded = (long)(divide * 25000000);
  freqDecoded++;
  Serial.print("freq decode:");
  Serial.println(freqDecoded,DEC);
  //update the main frequency.
  freq = freqDecoded;
  return;
}

void checkUART(/*bool& change*/){
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
        wroteLSB_FREQ0 = false;
        //lets decode the waveform
        if ((command & 0x0800 != 0)){
          Serial.println("Changing FSELECT is not allowed!"); //ABORT
          break;
        }
        //change = true;
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
        isUART = true;
        write_register(command);
        display_LCD();
        break;
      case 0x4000: //NOTE: We need to ensure that proper frequency register is selected for phase accumulation
        //Serial.println("FREQ0 REG write.");
        if(!wroteLSB_FREQ0){
          LSB = command;
          wroteLSB_FREQ0 = true;
        } else { 
          MSB = command;
          write_register(LSB);
          write_register(MSB);
          decodeFreq_UART(LSB,MSB);
          wroteLSB_FREQ0 = false;
          //isUART = true; not needed here
          Serial.println("FREQ0 REG write.");
          display_LCD();
        }
        break;
      case 0x8000: //NOTE: We need to ensure that proper frequency register is selected for phase accumulation
        Serial.println("FREQ1 REG write."); // NOT SUPPORTED
        wroteLSB_FREQ0 = false;
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
        wroteLSB_FREQ0 = false;
        Serial.println("Phase reg write.");
        break;
      break;
    }
  }
  isUART = false;
}

bool isSquare(){
  if (waveform == SQUARE)
    return true;
  return false;
}

void checkSquare(){ //check if a waveform is square wave
  if (isSquare()) {
    if (digitalRead(RELAY))
      digitalWrite(RELAY,LOW); //relay is active LOW (sink current)
  }
  else {
    if (!digitalRead(RELAY))
      digitalWrite(RELAY,HIGH);
  }
}

void checkEncoder(){
  if (val = read_rotary()) { //check if encoder has been rotated
    c += val;
    if (prevNextCode == 0x0b)
      dir = rotationDirection::LEFT; 
    if (prevNextCode == 0x07)
      dir = rotationDirection::RIGHT;
    adjust_temp(); //adjust 
    display();
    display_LCD();
  }
}

void setup(){ 
  //Serial communication setup
  Serial.begin(9600);
  Serial.flush(); //clear whats left in the buffer

  //LCD setup
  lcd.init(); //initialize the LCD
  lcd.backlight();

  //Setup all pins
  pinMode(PUSH,INPUT_PULLUP); //change mode (test)
  pinMode(CLK,INPUT_PULLUP); //encoder pin A
  pinMode(DATA,INPUT_PULLUP); //encoder pin B
  pinMode(RELAY,OUTPUT); //output pin to control relay

  //SPI setup
  SPI.begin(); //start the SPI communication
  reset(); //reset the AD9833 module

  set_default(); //Set default state of generator
  display_LCD();
}

void loop(){
  //We update the state of generator ONLY when needed

  checkUART(); //check if data is sent through UART

  checkButton(); //switch mode if button was pressed

  checkEncoder(); //check if encoder was rotated, if so adjust 

  checkSquare(); //check if the waveform is square (relay circuit)
 
  //delay(10);
}
