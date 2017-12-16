//Tim Eckel's new ping library
#include <NewPing.h>
#include <LiquidCrystal.h>
#include <OneWire.h>
//RGB led pins
#define RED_PIN  A2
#define GREEN_PIN   A1
#define BLUE_PIN   A0
#define TRIGGER_PIN  13  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN    12  // Arduino pin tied to echo pin on the ultrasonic sensor.
#define MAX_DISTANCE 400 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.
#define BUTTON_PIN1 2   //increase delay button
#define BUTTON_PIN2 11  //decrease delay button 
#define INTERRUPT_BUTTON 3   //interrupt pin
#define PIR_PIN A3    //the pin connected to the PIR sensor's output
#define SWITCH_PIN A5   //magnetic contact sensor pin
#define TEMPR_PIN A4   //temprature sensor pin
#define MOSFET_PIN 10    //for refreshener


// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(8, 6, 9, 4, 5, 7);
// NewPing setup of pins and maximum distance.
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
//onewire setup for temprature sensor
OneWire  ds(TEMPR_PIN);


//the time when the sensor outputs a low impulse
volatile long unsigned int lowIn;

//the amount of milliseconds the sensor has to be low
//before we assume all motion has stopped
const long unsigned int pause = 500;

volatile boolean lockLow = true;
volatile boolean takeLowTime;
volatile boolean motionExists = false;
volatile boolean triggered = false;
volatile boolean triggered2 = false;
volatile boolean useKnown = false;


//0 - not in use     1 - in use unknown
//2 - number 1       3 - number 2
//4 - cleaning       5 - triggered         6 - menu
volatile int currentState = 0;
volatile long unsigned int inTime = 0;
volatile long unsigned int currentTime = 0;
volatile long unsigned int prevTime = 0 ;
volatile long unsigned int cycleTime = 0;
volatile long unsigned int useTime = 0 ;
//total motion time
volatile long unsigned int motionTime = 0;
//sensor senses  motion
volatile long unsigned int motionStartTime;
//sensor senses no motion
volatile long unsigned int motionStopTime;
//serial time interval
volatile long unsigned int lastSerialTime = 0;


//current distance
volatile long int currentDistance;
//time spent in distance range
volatile long int rangeTime = 0;
//pushbutton status
volatile int buttonState1 = 0;
volatile int buttonState2 = 0;
//for handling debouncing issues
volatile int lastButtonState1 = HIGH;
volatile int lastButtonState2 = HIGH;
// the last time the output pin was toggled
volatile long lastDebounceTime1 = 0; 
volatile long lastDebounceTime2 = 0; 
// the debounce time
volatile long debounceDelay = 20; 
//last button time for intterupt debounce
volatile long lastIntDebounceTime = 0;
//interrupt button state
volatile int intrButtonState = HIGH; 
volatile int lastInterState = HIGH; 
//temprature
volatile float currentTempr = 20.0; 
//remaining spray number
volatile int sprayLeft = 2400; 
//spray delay in seconds
volatile int sprayDelay = 15;
volatile int magnetState = 0 ;
volatile int lastMagnetState = 0;
volatile long lastMagnetTime = 0;
volatile long triggerTime = 0;
volatile long triggerTime2 = 0;
volatile long menuTime = 0;
volatile int personIn = 0;    // value stating if there is a person inside
volatile int sprayState = 0;
String inputString = "";         // a string to hold incoming data
volatile boolean stringComplete = false;  // whether the string is complete
volatile char inChar ;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(PIR_PIN, INPUT);
 //  inputString.reserve(200);
  digitalWrite(PIR_PIN, LOW);
  delay(10);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  

  pinMode(BUTTON_PIN1, INPUT);
  pinMode(BUTTON_PIN2, INPUT);
  pinMode(SWITCH_PIN , INPUT);
  digitalWrite(SWITCH_PIN , HIGH);
  pinMode(MOSFET_PIN, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(INTERRUPT_BUTTON), debounceInterrupt, FALLING);

}

void loop() {
  // put your main code here, to run repeatedly:
  

  
  //LCD menu and current overlay
  /*if(currentState == 6  ){
    lcd.setCursor(0, 0);
    lcd.print("Delay:   ");
    lcd.print(sprayDelay);
    lcd.setCursor(0, 1);
    lcd.print("<--  -    +  -->");
  }else{
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(currentTempr, 2);
    lcd.setCursor(0, 1);
    lcd.print("Spray left: ");
    lcd.print(sprayLeft);
  }*/
  if((millis()- menuTime) > 5000){
    if(currentState == 6){
      noMovement();
    }
  }

  if((millis() - motionStopTime > 20000) && currentState == 4){
    currentState = 0 ;
    noMovement();
  }

  prevTime = currentTime ;
  currentTime = millis();
  cycleTime = currentTime - prevTime ;
  if(personIn == 1)
    useTime = currentTime - inTime ;
  currentDistance = sonar.ping_cm();
  motion();
  button();
  calcTimeInRange();
  tempr();
  magnet();
  leds();
  runRefresh();
  //debugging purpose
  





  if (motionExists && currentState != 6 && currentState != 5) {
    //change state to in use unknown
    if(!useKnown){
      currentState = 1;
      useKnown = true;
    }
    if(personIn == 0){
      inTime = millis();
      personIn = 1;
    }
    
    if ( useTime >= 5000 && useTime < 20000 && rangeTime > 10000   ){
      currentState = 2 ;
    }
    else if (useTime >= 10000 && rangeTime > 20000 ){
      currentState = 3 ;
    }
    else if (useTime >= 20000  && rangeTime <8000 ) {
      currentState = 4;
    
    }
  if (Serial.available()) {
    // get the new byte:
    inChar = (char)Serial.read();
    
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    stringComplete = true;
    
  }
     // print the string when a newline arrives:
  if (stringComplete) {
    if(inputString[0] == '3'){
      lcd.setCursor(0, 0);
      lcd.print("Hands washed   ");
    }else if(inputString[0] == '4'){
      lcd.setCursor(0, 0);
      lcd.print("Hands not washed");
    }else if(inputString[0] == '1'){
      currentState = 5;
      sprayonce();
      Serial.write("spray once");
    }else if(inputString[0] == '2'){
      currentState = 5;
      Serial.write("spray twice");
      spraytwice();
    }
    
    // clear the string:
    inputString = "";
    stringComplete = false;
  }


    if (currentState == 2 &&  magnetState == 1) {
      sprayonce();
    }
    else if (currentState == 3 &&  magnetState == 1) {
      spraytwice();
    }
  }

}

void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}
void runRefresh(){
  if(sprayState == 1){
    if(triggered && (currentTime - triggerTime + 15000) > sprayDelay*1000 && (currentTime - triggerTime) < (sprayDelay*1000 ) ){
      digitalWrite(MOSFET_PIN,HIGH);
    }else if(triggered && (currentTime - triggerTime) > (sprayDelay*1000 )){
      digitalWrite(MOSFET_PIN, LOW);
      if(triggered){
        triggered = false;
        noMovement();
      }
    }
  }else if(sprayState == 2){
    if(triggered && (currentTime - triggerTime + 14500) > sprayDelay*1000 && (currentTime - triggerTime) < (sprayDelay*1000 ) ){
      digitalWrite(MOSFET_PIN,HIGH);
    }else if(triggered && (currentTime - triggerTime) > (sprayDelay*1000 )&& !triggered2){
      digitalWrite(MOSFET_PIN, LOW);
      triggered2 = true;
      if(triggered){
        triggered = false;
        triggerTime2 = currentTime;
      }
    }else if(triggered2 &&(currentTime - triggerTime2 + 14500) > sprayDelay*1000 && (currentTime - triggerTime2) < (sprayDelay*1000 )){
      digitalWrite(MOSFET_PIN,HIGH);
    }else if(triggered2 && (currentTime - triggerTime2) > (sprayDelay*1000 )){
       
       digitalWrite(MOSFET_PIN,LOW);
       if(triggered2){
         triggered2 = false;
         noMovement();
       }
    }
  }

}
void leds(){
  switch(currentState){
    case 0 :
      digitalWrite(RED_PIN, LOW);
      digitalWrite(BLUE_PIN, LOW);
      digitalWrite(GREEN_PIN, LOW);
      break;
    case 1 :
      digitalWrite(RED_PIN, HIGH);
      digitalWrite(BLUE_PIN, LOW);
      digitalWrite(GREEN_PIN, LOW);
      break;
    case 2 :
      digitalWrite(RED_PIN, HIGH);
      digitalWrite(BLUE_PIN, HIGH);
      digitalWrite(GREEN_PIN, LOW);
      break;
    case 3 :
      digitalWrite(RED_PIN, LOW);
      digitalWrite(BLUE_PIN, HIGH);
      digitalWrite(GREEN_PIN, HIGH);
      break;
    case 4 :
      digitalWrite(RED_PIN, HIGH);
      digitalWrite(BLUE_PIN, LOW);
      digitalWrite(GREEN_PIN, HIGH);
      break;
    case 5 :
      digitalWrite(RED_PIN, LOW);
      digitalWrite(BLUE_PIN, LOW);
      digitalWrite(GREEN_PIN, HIGH);
      break;
    case 6 :
      digitalWrite(RED_PIN, LOW);
      digitalWrite(BLUE_PIN, HIGH);
      digitalWrite(GREEN_PIN, LOW);
      break;
  }

  
}
void noMovement(){
  currentState = 0 ;
  inTime = 0;
  useTime = 0 ;
  motionExists = false;
  useKnown = false;
  rangeTime = 0;
  personIn = 0;
  motionTime = 0;
}
void debounceInterrupt(){
  if(!triggered)
    sprayonce();
}

void calcTimeInRange() {
  if (currentDistance <= 60 && currentDistance >= 5) {
    rangeTime = rangeTime + cycleTime ;
  }else if (currentDistance != 0){
    //rangeTime = 0;
  }
}
void magnet(){
  int reading = digitalRead(SWITCH_PIN);
  
  if (reading != lastMagnetState) {
    // reset the debouncing timer
    lastMagnetTime = millis();
  }

  
  if ((millis() - lastMagnetTime) > debounceDelay) {
    if(magnetState != reading){
      magnetState = reading;
    }
    
  }

  lastMagnetState = reading;
}

void button(){
  int reading1 = digitalRead(BUTTON_PIN1);
  int reading2 = digitalRead(BUTTON_PIN2);
  
  if (reading1 != lastButtonState1) {
    // reset the debouncing timer
    lastDebounceTime1 = millis();
  }
  if (reading2 != lastButtonState2) {
    // reset the debouncing timer
    lastDebounceTime2 = millis();
  }

  
  if ((millis() - lastDebounceTime1) > debounceDelay) {
    if(buttonState1 != reading1 ){
      buttonState1 = reading1;
      if(reading1 == HIGH  && sprayDelay < 45)
        sprayDelay++;
    }
  }
  if ((millis() - lastDebounceTime2) > debounceDelay) {
    if(buttonState2 != reading2){
      buttonState2 = reading2;
      if(reading2 == HIGH && sprayDelay > 15)
        sprayDelay--;
    }
  }

  if (buttonState1 == LOW) {
     Serial.println("Button1 pressed");
     currentState = 6;
     menuTime = millis();
  } else {
  }
  if (buttonState2 == LOW) {
     Serial.println("Button2 pressed");
     currentState = 6;
     menuTime = millis();
  } else {
  }

  lastButtonState1 = reading1;
  lastButtonState2 = reading2;
}
void sprayonce() {
  currentState = 5;
  if(!triggered){
    sprayLeft--;
    triggered = true;
    triggerTime = millis();
    sprayState = 1;
  }
}
void spraytwice() {
  currentState = 5;
  if(!triggered){
    sprayLeft = sprayLeft - 2;
    sprayState = 2;
    triggered = true;
    triggerTime = millis();
    
  }
  
}

void motion() {
  if (digitalRead(PIR_PIN) == HIGH) {

    if (lockLow) {
      //makes sure we wait for a transition to LOW before any further output is made:
      lockLow = false;
      Serial.println("---");
      Serial.print("motion detected at ");
      Serial.print(millis() / 1000);
      motionStartTime = millis();
      Serial.println(" sec");
      if(!motionExists)
        motionExists = true;
      delay(20);
    }
    takeLowTime = true;
  }

  if (digitalRead(PIR_PIN) == LOW) {

    if (takeLowTime) {
      lowIn = millis();          //save the time of the transition from high to LOW
      takeLowTime = false;       //make sure this is only done at the start of a LOW phase
    }
    //if the sensor is low for more than the given pause,
    //we assume that no more motion is going to happen
    if (!lockLow && (millis() - lowIn > pause)) {
      //makes sure this block of code is only executed again after
      //a new motion sequence has been detected
      lockLow = true;
      Serial.print("motion ended at ");      //output
      Serial.print((millis() - pause) / 1000);
      motionStopTime = millis();
      Serial.println(" sec");
      delay(20);
    }
  }
}

void tempr(){
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;
  
  if ( !ds.search(addr)) {
    ds.reset_search();
    delay(10);
    return;
  }
  


  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      type_s = 1;
      break;
    case 0x28:
      type_s = 0;
      break;
    case 0x22:
      type_s = 0;
      break;
    default:
      return;
  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(10);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad


  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }


  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
  currentTempr = celsius ;
  //Serial.print("  Temperature = ");
  //Serial.print(celsius);
  //Serial.print(" Celsius, ");
  //Serial.print(fahrenheit);
  //Serial.println(" Fahrenheit");
  
}

