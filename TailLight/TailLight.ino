
#include <Serial.h>

#define MASTER_LED_PIN 12
#define SLAVE_LED_PIN 3
#define DBG_LED_PIN 4
#define N_FILTER_CHANNEL 2
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


