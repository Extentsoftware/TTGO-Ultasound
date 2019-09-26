//
// https://github.com/YogoGit/TTGO-LORA32-V1.0/blob/master/Receiver/Receiver.ino
// https://docs.platformio.org/en/latest/boards/espressif32/ttgo-lora32-v1.html#hardware
// https://github.com/lewisxhe/TTGO-LoRa-Series

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>  
#include <SSD1306.h>
#include "main.h"

#define SCK     5    // GPIO5  -- SX1278's SCK
#define MISO    19   // GPIO19 -- SX1278's MISO
#define MOSI    27   // GPIO27 -- SX1278's MOSI
#define SS      18   // GPIO18 -- SX1278's CS
#define RST     14   // GPIO14 -- SX1278's RESET
#define DI0     26   // GPIO26 -- SX1278's IRQ(Interrupt Request)
#define BAND    868E6

#define OLED_SDA    4
#define OLED_SCL    15
#define OLED_RST    16

SSD1306 display(0x3c, OLED_SDA, OLED_SCL);

struct SensorConfig config;

void loraData(SensorReport report){

  int rssi = LoRa.packetRssi();
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  char buf[32];
  sprintf(buf, "%02x%02x%02x%02x", report.chipid[2], report.chipid[3], report.chipid[4], report.chipid[5]);
  display.drawStringMaxWidth(0, 0, 128, buf);

  display.setFont(ArialMT_Plain_24);
  display.drawStringMaxWidth(0, 16, 128, String(report.distance));

  if (rssi>-80)
    display.fillRect(108,0, 5, 10);
  else
    display.drawRect(108,0, 5, 10);

  if (rssi>-100)
    display.fillRect(115,3, 5, 7);
  else
    display.drawRect(115,3, 5, 7);

  if (rssi>-120)
    display.fillRect(122,7, 5, 3);
  else
    display.drawRect(122,7, 5, 3);

  display.display();
}

void cbk(int packetSize) {
  
  SensorReport report;
  Serial.println("got packet");

  if (packetSize == sizeof(SensorReport))
  {
      unsigned char *ptr = (unsigned char *)&report;
      for (int i = 0; i < packetSize; i++, ptr++)
         *ptr = (unsigned char)LoRa.read(); 

      char *stime = asctime(gmtime(&report.time));
      stime[24]='\0';
      Serial.printf("distance=%f\n",report.distance);
      loraData(report);
  }
}

void setup() {
  pinMode(16,OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high、
  
  Serial.begin(115200);
  while (!Serial);
  
  startLoRa();

  LoRa.receive();
  Serial.println("init screen");
  display.init();
  display.flipScreenVertically();  
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawStringMaxWidth(0, 0, 128, "Waiting");
  display.display();
  Serial.println("init ok");
   
  delay(1500);
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) { cbk(packetSize);  }
  delay(10);
}


void startLoRa() {
  Serial.printf("\nStarting Lora: freq:%lu enableCRC:%d coderate:%d spread:%d bandwidth:%lu txpower:%d\n", config.frequency, config.enableCRC, config.codingRate, config.spreadFactor, config.bandwidth, config.txpower);

  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS,RST,DI0);

  int result = LoRa.begin(config.frequency);
  if (!result) 
    Serial.printf("Starting LoRa failed: err %d\n", result);
  else
    Serial.println("Started LoRa OK");

  LoRa.setPreambleLength(config.preamble);
  LoRa.setSyncWord(config.syncword);    
  LoRa.setSignalBandwidth(config.bandwidth);
  LoRa.setSpreadingFactor(config.spreadFactor);
  LoRa.setCodingRate4(config.codingRate);
  if (config.enableCRC)
      LoRa.enableCrc();
    else 
      LoRa.disableCrc();

  LoRa.setTxPower(config.txpower);
  LoRa.idle();
  
  if (!result) {
    Serial.printf("Starting LoRa failed: err %d", result);
  }  
}