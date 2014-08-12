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
#include <Serial.h>
#define ACC_X_PIN 2
#define ACC_Y_PIN 1
#define ACC_Z_PIN 0

void setup(){

  //// power up devices
  pinMode(6, OUTPUT);
  digitalWrite(6, HIGH);
  
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  
  pinMode(A3, OUTPUT);
  digitalWrite(A3, LOW);
  
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);

  pinMode(12, OUTPUT);
  pinMode(11, OUTPUT);
  
  digitalWrite(11, LOW);
  digitalWrite(12, HIGH);
	
  //////

  Serial.begin(9600);
  
}

void loop(){
	
	int x = analogRead(A0);
	
	if(x<180) {
		digitalWrite(12, HIGH);
		delay(500);
	} else {
		digitalWrite(12, LOW);
	}
	
	Serial.println(x);
}


