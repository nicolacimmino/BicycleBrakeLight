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
//  This is as I am going to use this as more generic code for other applications
//  that will use all axis.

#include <EEPROM.h>

// X-Axis is parallel to the direction of travel.
#define ACC_X_PIN 0
#define ACC_Y_PIN 1
#define ACC_Z_PIN 2

// We use an IIR (Infinite Impulse Response) filter to filter the accelleration.
// The filter taps have been calculated in Octave.
// The Low Pass filter is set for:
// Sampling frequency: 100Hz
// Cut-off frequency: 1Hz (3 dB)
// Order: 3rd
// 
// Below we define this filter forward and feedback taps and the circular buffer used to hold the samples.
#define accellFilterTapsCount 4
float accellFilterForward_Taps[accellFilterTapsCount] = { 0.00002915, 0.00008744, 0.00008744, 0.00002915 };
float accellFilterFeedback_Taps[accellFilterTapsCount] = { 1.0000, -2.8744, 2.7565, -0.8819 };
float accellInputSamples[3][accellFilterTapsCount];
float accellOutputSamples[3][accellFilterTapsCount];
int accellSamplesBufferIndex = 0;

// We use an IIR (Infinite Impulse Response) filter to separate gravity effects from bike accelleration.
// This is needed to remove the forward component of gravitational accelleration when we are on an incline.
// This filter has a much lower cut-off frequency consideting slope is channhing more slowly than braking
//  effort. This filter has a lower order than the other as a 3rd order one would have had too small forward
//  coefficient values that woul have had issues with the limited precision of Arduino float.
// The filter taps have been calculated in Octave.
// The Low Pass filter is set for:
// Sampling frequency: 100Hz
// Cut-off frequency: 0.1Hz (3 dB)
// Order: 2nd
// 
// Below we define this filter forward and feedback taps and the circular buffer used to hold the samples.
#define gravityFilterTapsCount 3
float gravityFilterForward_Taps[gravityFilterTapsCount] = {  0.00000983, 0.00001965, 0.00000983 };
float gravityFilterFeedback_Taps[gravityFilterTapsCount] = { 1.0000, -1.9911, 0.9912 };
float gravityInputSamples[3][gravityFilterTapsCount];
float gravityOutputSamples[3][gravityFilterTapsCount];
int gravitySamplesBufferIndex = 0;    

// Interval between accelerometer samples in mS. This is a sampling frequency of 100Hz
#define samplingInterval 10

// Stores the timestamp of the last sample so that the next can be taken at the exact needed moment.
long lastSampleTime = 0;

// Amount of samples averaged during calibration
#define calibrationSamples 100

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
float accelerometerPins[] = { ACC_X_PIN, ACC_Y_PIN, ACC_Z_PIN };

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
      for(int sample=0; sample<calibrationSamples; sample++) {
        total += analogRead(accelerometerPins[axis])/2;
        delay(samplingInterval);
      }
      int average = total/calibrationSamples;
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
  
  for(int axis=0; axis<3; axis++) 
  {
    for(int sampleIndex=0; sampleIndex<gravityFilterTapsCount; sampleIndex++)
    {
      gravityInputSamples[axis][sampleIndex] = 0;
      gravityOutputSamples[axis][sampleIndex] = 0;
    }
    
    for(int sampleIndex=0; sampleIndex<accellFilterTapsCount; sampleIndex++)
    {
      accellInputSamples[axis][sampleIndex] = 0;
      accellOutputSamples[axis][sampleIndex] = 0;
    }
  }
  // Blink status at 1Hz during calibration.
  digitalWrite(LED_STATUS, LOW);
  
  Serial.println("Calibration done.");
    
}

void loop(){

  // Wait right time for sampling.	
  while(millis() - lastSampleTime < samplingInterval)
  {
    delay(1);
  }
  lastSampleTime = millis();
  
  for(int axis=0; axis<3; axis++) {
    
    // Get the current reading and convert to g according to the calibration table.
    float currentReading = analogRead(accelerometerPins[axis])/2;
    currentReading = (currentReading-EEPROM.read(EEPROM_CALIBRAION_BASE+(axis*2)))/(float)EEPROM.read(EEPROM_CALIBRAION_BASE+1+(axis*2));
    
    // Store the current reading at the current position of the circular buffer.
    accellInputSamples[axis][accellSamplesBufferIndex] = currentReading; 
    gravityInputSamples[axis][gravitySamplesBufferIndex] = currentReading; 
  }

  // Move to the next  position of the circular buffer and wrap around if we are
  // at the end of the array.  
  accellSamplesBufferIndex = (accellSamplesBufferIndex + 1) % accellFilterTapsCount;
  gravitySamplesBufferIndex = (gravitySamplesBufferIndex + 1) % gravityFilterTapsCount;
  
  
  // Calculate both IIR filters output for each axis.
  float accellOutput[] = { 0, 0, 0 };
  float gravityOutput[] = { 0, 0, 0 };
  
  for(int axis=0; axis<3; axis++) {
    int sampleIndex = accellSamplesBufferIndex;
    for(int ix=0;ix<accellFilterTapsCount;ix++) {
      sampleIndex = (sampleIndex>0)?sampleIndex-1:accellFilterTapsCount-1;
      accellOutput[axis] += accellInputSamples[axis][sampleIndex] * accellFilterForward_Taps[ix] 
                            - accellOutputSamples[axis][sampleIndex] * accellFilterFeedback_Taps[ix];
    }
    accellOutputSamples[axis][accellSamplesBufferIndex] = accellOutput[axis];
    
    sampleIndex = gravitySamplesBufferIndex;
    for(int ix=0;ix<gravitySamplesBufferIndex;ix++) {
      sampleIndex = (sampleIndex>0)?sampleIndex-1:gravityFilterTapsCount-1;
      gravityOutput[axis] += gravityInputSamples[axis][sampleIndex] * gravityFilterForward_Taps[ix] 
                            - gravityOutputSamples[axis][sampleIndex] * gravityFilterFeedback_Taps[ix];
    }
    gravityOutputSamples[axis][gravitySamplesBufferIndex] = gravityOutput[axis];
  }
  
  // Work out the gravity component that is affecting the X-Axis (forward)
  float forwardAccelleration = accellOutput[X_AXIS]/* * gravityOutput[Z_AXIS] + accellOutput[Z_AXIS] * gravityOutput[X_AXIS]*/;
  
  // We have some hytesresis in the detector. We consider
  //  braking above 0.05g and we get out of the breaking status
  //  below 0.03g.  These values were empirically determined
  //  in road tests.
  if(forwardAccelleration > -0.03)
  {
    digitalWrite(LED_A, LOW);
  }
  if(forwardAccelleration < -0.05)
  {
    digitalWrite(LED_A, HIGH);
  } 
  
  Serial.print(millis());
  Serial.print(" X:");
  Serial.print(accellOutput[0]);
  Serial.print(" Y:");
  Serial.print(accellOutput[1]);
  Serial.print(" Z:");
  Serial.println(accellOutput[2]);
  
  if(Serial.read()=='c') {
    calibrate();
  }  
}


