uint16_t dataToSend;
uint8_t testMode;
const int SINE = 0x2000; //0010 0000 0000 0000                    
const int SQUARE = 0x2028; //0010 0000 0010 1000                  
const int TRIANGLE = 0x2002; //0010 0000 0000 0010
bool ended=false;

void sendFrequency (long freq){
  uint16_t MSB,LSB;
  long freqWord;
  float divide;
  divide = (((float)(freq))/25000000);
  freqWord = divide * 0x10000000;
  MSB = (uint16_t)((freqWord & 0xFFFC000) >> 14); //take only 14 low bits
  LSB = (uint16_t)(freqWord & 0x3FFF); //take only 14 upper bits

  LSB |= 0x4000; //01 ... Write to FREQ0 Register
  MSB |= 0x4000; //01 ... Write to FREQ0 Register
  
  //Send LSB
  Serial.write(0xF0); 
  Serial.write((uint8_t)(LSB & 0xFF));      
  Serial.write((uint8_t)((LSB >> 8) & 0xFF)); 
  delay(100);

  //Send MSB
  Serial.write(0xF0);
  Serial.write((uint8_t)(MSB & 0xFF));      
  Serial.write((uint8_t)((MSB >> 8) & 0xFF)); 
  delay(100);
  return;
}

void changeShape(uint16_t shape){
  Serial.write(0x0F); //waveform change
  Serial.write((uint8_t)(shape & 0xFF));      
  Serial.write((uint8_t)((shape >> 8) & 0xFF)); 
  delay(10);
  return;
}

void setup() {
  Serial.begin(9600);  // Set the baud rate to 9600 (adjust as needed)
}

void loop() {
  long freq = 2137;
  sendFrequency(freq);
  delay(1000);

  Serial.write(0xFF); //wrong mode test
  dataToSend = 0x59A1;
  Serial.write((uint8_t)(dataToSend & 0xFF));      
  Serial.write((uint8_t)((dataToSend >> 8) & 0xFF)); 
  delay(1000);        //wrong mode test
  
  changeShape(TRIANGLE);
  delay(1000);

  freq = 21000;
  sendFrequency(freq);
  changeShape(SINE);
  delay(1000);
 
}