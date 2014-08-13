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

// Z-Axis is parallel to the direction of travel.
#define ACC_Z_PIN 0

// We filter data sampled from the z-axis and keep
//  a running average of this length.
#define zAxisRunningAverageLen 10

// Interval between accelerometer samples in mS.
#define samplingInterval 50

// Hysteresis in mS of the brake light once it lights up.
#define brakeLightHysteresis 500

// This is a circular buffer we use to store the last
//  n samples so that we can keep calucating the running
//  average.
int zAxisSamples[zAxisRunningAverageLen];
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
  
  for (int thisReading = 0; thisReading < zAxisRunningAverageLen; thisReading++)
    zAxisSamples[thisReading] = 0;
    
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
  if(zAxisBufferIndex >= zAxisRunningAverageLen) {
    zAxisBufferIndex = 0;                         
  }  
      
  
  int average = total / zAxisRunningAverageLen;

  // We consider braking values inside a certain window, this was determined
  //  empirically. Calibration will be needed.
  if(average>180 && average<230) {
    digitalWrite(12, HIGH);
    delay(brakeLightHysteresis);
  } else {
    digitalWrite(12, LOW);
  }
  	
  delay(samplingInterval);
        
}


