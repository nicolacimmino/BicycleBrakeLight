// TailLight implements logic for a smart bike tail light.
//  Copyright (C) 2014 Nicola Cimmino
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see http://www.gnu.org/licenses/.
//

#include <EEPROM.h>

// Z-Axis is parallel to the direction of travel.
// Y-Axis is the vertical.
#define ACC_X_PIN 0
#define ACC_Y_PIN 1
#define ACC_Z_PIN 2


// We filter data sampled from the z-axis and keep
//  a running average of this length.
#define accelerometerRunningAverageLen 10

// Interval between accelerometer samples in mS.
#define samplingInterval 50

// Hysteresis in mS of the brake light once it lights up.
#define brakeLightHysteresis 500

// This is a circular buffer we use to store the last
//  n samples so that we can keep calucating the running
//  average.
int zAxisSamples[accelerometerRunningAverageLen];
int zAxisBufferIndex = 0;    
int total = 0;               

#define ACCELEROMETER_PWR 6
#define ACCELEROMETER_VIN 13
#define ACCELEROMETER_GND A3
#define ACCELEROMETER_X A2
#define ACCELEROMETER_Y A1
#define ACCELEROMETER_Z A0
#define LED_A 12
#define LED_K 11
#define LED_STATUS 9

#define EEPROM_CALIBRAION_BASE 0

// Accelerometer pins in an array so we can access them programmatically
//  in sequence.
int accelerometerPins[] = { ACC_X_PIN, ACC_Y_PIN, ACC_Z_PIN };
    
void setup(){

  // Power up the acceletometer
  pinMode(ACCELEROMETER_PWR, OUTPUT);
  digitalWrite(ACCELEROMETER_PWR, HIGH);
  pinMode(ACCELEROMETER_GND, OUTPUT);
  digitalWrite(ACCELEROMETER_GND, LOW);
  
  // Give 5v on the accelerometer Vin, this is used
  //  to generate the analog outputs.
  pinMode(ACCELEROMETER_VIN, OUTPUT);
  digitalWrite(ACCELEROMETER_VIN, HIGH);
  
  // Accelerometer outputs
  pinMode(ACCELEROMETER_X, INPUT);
  pinMode(ACCELEROMETER_Y, INPUT);
  pinMode(ACCELEROMETER_Z, INPUT);

  // LED off.
  pinMode(LED_A, OUTPUT);
  digitalWrite(LED_A, HIGH);
  digitalWrite(LED_K, LOW);
  pinMode(LED_K, OUTPUT);
  
  // Status LED
  pinMode(LED_STATUS, OUTPUT);
  digitalWrite(LED_STATUS, LOW);
  
  for (int thisReading = 0; thisReading < accelerometerRunningAverageLen; thisReading++)
    zAxisSamples[thisReading] = 0;
    
  Serial.begin(9600);
  
  for(int ix=0; ix<6; ix++)
  {
    Serial.print(EEPROM.read(ix));
    Serial.print(" ");
  }
  Serial.println("");
}

void calibrate()
{
  Serial.println("Calibrating....");
  
  long startTime = millis();  
  
  int maxValues[] = { 0, 0, 0 };
  int minValues[] = { 1024, 1024, 1024 };
  
  // Calibrate for 30s
  while(millis() - startTime < 30000)
  {
    // Blink status at 1Hz during calibration.
    digitalWrite(LED_STATUS, ((millis() % 1000)<500)?HIGH:LOW);
    
    for(int axis=0; axis<3; axis++) {
      long total = 0;
      for(int sample=0; sample<accelerometerRunningAverageLen; sample++) {
        total += analogRead(accelerometerPins[axis])/2;
        delay(samplingInterval);
      }
      int average = total/accelerometerRunningAverageLen;
      if(average > maxValues[axis]) {
        maxValues[axis] = average;
      }
      
      if(average < minValues[axis]) {
        minValues[axis] = average;
      }
    }
  }
  
  // Sore in EEPROM. We use locations from EEPROM_CALIBRAION_BASE as:
  // ZeroX, KX, ZeroY, KY, ZeroZ, KZ
  // Where Zero* is the value corresponding to 0g on the axis and
  // K* is the value representing 1g.
  for(int axis=0; axis<3; axis++) {
    EEPROM.write(EEPROM_CALIBRAION_BASE+(axis*2), minValues[axis]+(maxValues[axis]-minValues[axis])/2);
    EEPROM.write(EEPROM_CALIBRAION_BASE+(axis*2)+1, (maxValues[axis]-minValues[axis])/2);
  }
  
  // Blink status at 1Hz during calibration.
  digitalWrite(LED_STATUS, LOW);
  
  Serial.println("Calibration done.");
    
}

void loop(){
	
  // subtract the oldest reading from the total
  total= total - zAxisSamples[zAxisBufferIndex];         
 
  // Get a new sample and store it at the current
  //  position of the circular buffer and add to the total
  zAxisSamples[zAxisBufferIndex] = analogRead(ACC_Z_PIN); 
  total= total + zAxisSamples[zAxisBufferIndex];       
  
  // Move to the next  position and wrap around if we are
  // at the end of the array.  
  zAxisBufferIndex = zAxisBufferIndex + 1;                    
  if(zAxisBufferIndex >= accelerometerRunningAverageLen) {
    zAxisBufferIndex = 0;                         
  }  
      
  
  int average = total / accelerometerRunningAverageLen;

  // We consider braking values inside a certain window, this was determined
  //  empirically. Calibration will be needed.
  if(average>180 && average<230) {
    digitalWrite(12, HIGH);
    delay(brakeLightHysteresis);
  } else {
    digitalWrite(12, LOW);
  }
  	
  delay(samplingInterval);
  
  Serial.print("X:");
  Serial.print(((signed int)analogRead(ACC_X_PIN)/2-EEPROM.read(EEPROM_CALIBRAION_BASE+0))/(double)EEPROM.read(EEPROM_CALIBRAION_BASE+1));
  Serial.print(" Y:");
  Serial.print(((signed int)analogRead(ACC_Y_PIN)/2-EEPROM.read(EEPROM_CALIBRAION_BASE+2))/(double)EEPROM.read(EEPROM_CALIBRAION_BASE+3));
  Serial.print(" Z:");
  Serial.println(((signed int)analogRead(ACC_Z_PIN)/2-EEPROM.read(EEPROM_CALIBRAION_BASE+4))/(double)EEPROM.read(EEPROM_CALIBRAION_BASE+5));
  
  if(Serial.read()=='c') {
    calibrate();
  }  
}


