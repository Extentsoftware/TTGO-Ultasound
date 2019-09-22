// pio run --target upload
// pio device monitor -p COM12 -b 115200
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <TBeamPower.h>
#include "main.h"

TBeamPower power(BUSPWR, BATTERY_PIN,PWRSDA,PWRSCL);

double GetDistance();
void SendLora(double distance);

void setup() {
  // Begin Serial communication at a baudrate of 9600:
  Serial.begin(115200);

  power.begin();
  power.power_sensors(false);
  power.power_peripherals(false);
  
  float currentVoltage = power.get_battery_voltage();
}

void loop() {
  double distance = GetDistance();

  SendLora(distance);

  Serial.printf("Distance = %f cm\n", distance);

  delay(500);
}

double GetDistance() {
    // Define inputs and outputs
  pinMode(TRIGPIN, OUTPUT);
  pinMode(ECHOPIN, INPUT);
  digitalWrite(TRIGPIN, LOW);   // Clear the TRIGPIN by setting it LOW:
  delayMicroseconds(2); 
  digitalWrite(TRIGPIN, HIGH);  // Trigger the sensor by setting the TRIGPIN high for 10 microseconds:
  delayMicroseconds(10);
  digitalWrite(TRIGPIN, LOW);
  
  // Read the ECHOPIN. pulseIn() returns the duration (length of the pulse) in microseconds:
  long duration = pulseIn(ECHOPIN, HIGH);
  
  return duration / 58.0;
}

void SendLora(double distance) {
  static int counter = 0;

  power.power_LoRa(true);
  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS,RST,DI0);
  if (!LoRa.begin(LORA_BAND * 1E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  // send packet
  LoRa.beginPacket();
  uint8_t chipid[6];
  esp_efuse_read_mac(chipid);
  // LoRa.printf("%02x%02x%02x%02x%02x%02x %.1f cm\n",chipid[0], chipid[1], chipid[2], chipid[3], chipid[4], chipid[5], distance);
  LoRa.printf("%02x%02x%02x%02x %.1f cm\n",chipid[2], chipid[3], chipid[4], chipid[5], distance);
  LoRa.endPacket();

  String countStr = String(counter, DEC);
  Serial.println(countStr);

  // toggle the led to give a visual indication the packet was sent
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);

  counter++;
  delay(1500);
}