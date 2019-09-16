// pio run --target upload
// pio device monitor -p COM12 -b 115200

#define TRIGPIN 13
#define ECHOPIN 2

#include <Arduino.h>
#include <SPI.h>

void setup() {
  // Define inputs and outputs
  pinMode(TRIGPIN, OUTPUT);
  pinMode(ECHOPIN, INPUT);
  
  // Begin Serial communication at a baudrate of 9600:
  Serial.begin(115200);
}

void loop() {
  
  digitalWrite(TRIGPIN, LOW);   // Clear the TRIGPIN by setting it LOW:
  delayMicroseconds(2); 
  digitalWrite(TRIGPIN, HIGH);  // Trigger the sensor by setting the TRIGPIN high for 10 microseconds:
  delayMicroseconds(10);
  digitalWrite(TRIGPIN, LOW);
  
  // Read the ECHOPIN. pulseIn() returns the duration (length of the pulse) in microseconds:
  long duration = pulseIn(ECHOPIN, HIGH);
  
  double distance = duration / 58.0;
  
  Serial.printf("Distance = %f cm\n", distance);
  delay(1000);
}