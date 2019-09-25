//
// https://github.com/YogoGit/TTGO-LORA32-V1.0/blob/master/Receiver/Receiver.ino
// https://docs.platformio.org/en/latest/boards/espressif32/ttgo-lora32-v1.html#hardware
// https://github.com/lewisxhe/TTGO-LoRa-Series

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>  
#include <SSD1306.h>
#include "../Common/sensor.h"

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

void loraData(SensorReport report){

  int rssi = LoRa.packetRssi();
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawStringMaxWidth(0, 12 , 128, String(report.distance));

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

  if (packetSize == sizeof(SensorReport))
  {
      unsigned char *ptr = (unsigned char *)&report;
      for (int i = 0; i < packetSize; i++, ptr++)
         *ptr = (unsigned char)LoRa.read(); 

      char *stime = asctime(gmtime(&report.time));
      stime[24]='\0';
  }

  loraData(report);
}

void setup() {
  pinMode(16,OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high、
  
  Serial.begin(115200);
  while (!Serial);
  Serial.println();
  Serial.println("LoRa Receiver Callback");
  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS,RST,DI0);  
  if (!LoRa.begin(868E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  //LoRa.onReceive(cbk);
  LoRa.receive();
  Serial.println("init ok");
  display.init();
  display.flipScreenVertically();  
  display.setFont(ArialMT_Plain_10);
   
  delay(1500);
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) { cbk(packetSize);  }
  delay(10);
}