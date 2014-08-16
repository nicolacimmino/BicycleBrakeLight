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

// We use a running average filter to get rid of noise 
//  but also of unwanted spikes from vibrations and
//  road bumps. A running average filter is a low pass
//  with normalized cut off frequency of:
//  
//  Fc=0.44294/sqrt(N^2-1)
//
//  With a lenghth of 20 we get Fc=0.0221 which corresponds
//  to a cut off frequency of 0.8Hz for a sampling frequency of 40Hz.
// 
// Filter length
#define accelerometerRunningAverageLen 20
// Interval between accelerometer samples in mS. This is a sampling frequency of 40Hz
#define samplingInterval 25

// Hysteresis in mS of the brake light once it lights up.
#define brakeLightHysteresis 500

// This is a circular buffer we use to store the last
//  n samples so that we can keep calucating the running
//  average.
double samplesBuffer[3][accelerometerRunningAverageLen];
int samplesBufferIndex = 0;    
double total[] = { 0, 0, 0 };               

#define ACCELEROMETER_PWR 6
#define ACCELEROMETER_VIN 13
#define ACCELEROMETER_GND A3
#define ACCELEROMETER_X A2
#define ACCELEROMETER_Y A1
#define ACCELEROMETER_Z A0
#define LED_A 12
#define LED_K 11
#define LED_STATUS 13

#define EEPROM_CALIBRAION_BASE 0

// Accelerometer pins in an array so we can access them programmatically
//  in sequence.
int accelerometerPins[] = { ACC_X_PIN, ACC_Y_PIN, ACC_Z_PIN };

#define X_AXIS 0
#define Y_AXIS 1
#define Z_AXIS 2

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
  digitalWrite(LED_A, LOW);
  pinMode(LED_K, OUTPUT);
  digitalWrite(LED_K, LOW);
  
  // Status LED
  //pinMode(LED_STATUS, OUTPUT);
  //digitalWrite(LED_STATUS, LOW);
  
  for(int axis=0; axis<3; axis++) {
    for (int thisReading = 0; thisReading < accelerometerRunningAverageLen; thisReading++) {
      samplesBuffer[axis][thisReading] = 0;
    }
  }
    
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
	
  double average[] = { 0, 0, 0 };
  
  for(int axis=0; axis<3; axis++) {
    // subtract the oldest reading from the total
    total[axis] = total[axis] - samplesBuffer[axis][samplesBufferIndex];         
   
    // Get the current reading and convert to g according to the calibration table.
    double currentReading = analogRead(accelerometerPins[axis])/2;
    currentReading = (currentReading-EEPROM.read(EEPROM_CALIBRAION_BASE+(axis*2)))/(double)EEPROM.read(EEPROM_CALIBRAION_BASE+1+(axis*2));
    
    // Clip to +/-1g so that potholes and other rough surfaces don't throw our running
    //  average off with a sinle huge spike.
    if(currentReading > 1) currentReading = 1;
    if(currentReading < -1) currentReading = -1;
    
    // Store the current reading at the current position of the circular buffer and add to the total
    samplesBuffer[axis][samplesBufferIndex] = currentReading; 
    total[axis] = total[axis] + samplesBuffer[axis][samplesBufferIndex];       
        
    average[axis] = total[axis] / accelerometerRunningAverageLen;
  }

  // Move to the next  position of the circular buffer and wrap around if we are
  // at the end of the array.  
  samplesBufferIndex = samplesBufferIndex + 1;                    
  if(samplesBufferIndex >= accelerometerRunningAverageLen) {
    samplesBufferIndex = 0;                         
  }  
  
  
  if(average[X_AXIS] < -0.1)
  {
    digitalWrite(LED_A, HIGH);
    //delay(brakeLightHysteresis);
  } else {
    digitalWrite(LED_A, LOW);
  } 
  
  delay(samplingInterval);
  
  Serial.print("X:");
  Serial.print(average[0]);
  Serial.print(" Y:");
  Serial.print(average[1]);
  Serial.print(" Z:");
  Serial.print(average[2]);
  
  if(Serial.read()=='c') {
    calibrate();
  }  
}


