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
// Note: the code below always samples and filters all three axes even though
//  we take decision only based on the axis parallel to the direction of travel.
//  This is as I going to use this as more generic code for other applications
//  that will use all axis.

#include <EEPROM.h>

// X-Axis is parallel to the direction of travel.
#define ACC_X_PIN 0
#define ACC_Y_PIN 1
#define ACC_Z_PIN 2

// We use a running FIR (Finite Impulse Response) filter.
// The filter taps have been calculated on http://t-filter.appspot.com/fir/index.html
// The filter is set for:
// Sampling frequency: 10Hz
// Cut-off frequency: 1Hz (3 dB)
// Out of band gain -40dB
// A good approximation was got with 15 taps.
//
// Amount of taps.
#define tapsCount 15

// Interval between accelerometer samples in mS. This is a sampling frequency of 10Hz
#define samplingInterval 100

// Filter taps. See above for filter parameters.
float filterTaps[tapsCount] = {
  -0.012592774787178462,
  -0.027048334867068192,
  -0.03115701603643162,
  -0.0033516667471792812,
  0.06651710329324825,
  0.16356430487792203,
  0.24972947322614572,
  0.28427790826227656,
  0.24972947322614572,
  0.16356430487792203,
  0.06651710329324825,
  -0.0033516667471792812,
  -0.03115701603643162,
  -0.027048334867068192,
  -0.012592774787178462 
};

// This is a circular buffer we use to store the last tapsCount samples so that we can keep
//  doing the FIR convolution.
float samplesBuffer[3][tapsCount];

// Current position in the circular buffer.
int samplesBufferIndex = 0;    

// Pins assignement.
#define ACCELEROMETER_PWR 6
#define ACCELEROMETER_VIN 13
#define ACCELEROMETER_GND A3
#define ACCELEROMETER_X A2
#define ACCELEROMETER_Y A1
#define ACCELEROMETER_Z A0
#define LED_A 12
#define LED_K 11
#define LED_STATUS 13

// EEPROM map.
#define EEPROM_CALIBRAION_BASE 0

// Accelerometer pins in an array so we can access them programmatically
//  in sequence.
int accelerometerPins[] = { ACC_X_PIN, ACC_Y_PIN, ACC_Z_PIN };

// Indexes in array that store values for each axis.
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
    
  for(int axis=0; axis<3; axis++) {
    for (int thisReading = 0; thisReading < tapsCount; thisReading++) {
      samplesBuffer[axis][thisReading] = 0;
    }
  }
    
  Serial.begin(9600);
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
      for(int sample=0; sample<tapsCount; sample++) {
        total += analogRead(accelerometerPins[axis])/2;
        delay(samplingInterval);
      }
      int average = total/tapsCount;
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
	
  for(int axis=0; axis<3; axis++) {
    
    // Get the current reading and convert to g according to the calibration table.
    float currentReading = analogRead(accelerometerPins[axis])/2;
    currentReading = (currentReading-EEPROM.read(EEPROM_CALIBRAION_BASE+(axis*2)))/(float)EEPROM.read(EEPROM_CALIBRAION_BASE+1+(axis*2));
    
    // Store the current reading at the current position of the circular buffer.
    samplesBuffer[axis][samplesBufferIndex] = currentReading; 
  }

  // Move to the next  position of the circular buffer and wrap around if we are
  // at the end of the array.  
  samplesBufferIndex = samplesBufferIndex + 1;                    
  if(samplesBufferIndex >= tapsCount) {
    samplesBufferIndex = 0;                         
  }  
  
  // Calculate FIR filter output for each axis.
  float filterOutput[] = { 0, 0, 0 };
  for(int axis=0; axis<3; axis++) {
    for(int ix=0;ix<tapsCount;ix++) {
      int index = (ix+samplesBufferIndex) % tapsCount;
      filterOutput[axis] += samplesBuffer[axis][index] * filterTaps[ix];
    }
  }
  
  if(filterOutput[X_AXIS] < -0.1)
  {
    digitalWrite(LED_A, HIGH);
  } else {
    digitalWrite(LED_A, LOW);
  } 
  
  delay(samplingInterval);
  
  Serial.print("X:");
  Serial.print(filterOutput[0]);
  Serial.print(" Y:");
  Serial.print(filterOutput[1]);
  Serial.print(" Z:");
  Serial.println(filterOutput[2]);
  
  if(Serial.read()=='c') {
    calibrate();
  }  
}


