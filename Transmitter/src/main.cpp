// pio run --target upload
// pio device monitor -p COM12 -b 115200
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <TBeamPower.h>
#include "TinyGPS.h"
#include "main.h"

TBeamPower power(BUSPWR, BATTERY_PIN,PWRSDA,PWRSCL);
TinyGPSPlus gps;                            
struct SensorConfig config;

void setup() {
  // Begin Serial communication at a baudrate of 9600:
  Serial.begin(115200);

  power.begin();
  power.power_sensors(false);
  power.power_peripherals(false);

  startLoRa();
  startGPS();
}

void loop() {
  SensorReport report;
  
  MakeDataPacket(report);
  SendLora(report);

  smartDelay(1000);
}

void MakeDataPacket(SensorReport report)
{
  uint8_t chipid[6];
  esp_efuse_read_mac(chipid);
  char *stime = asctime(gmtime(&report.time));
  stime[24]='\0';

  report.distance = GetDistance();
  report.volts = power.getBatteryVoltage();

  struct tm curtime;
  curtime.tm_sec = gps.time.second();
  curtime.tm_min=gps.time.minute();
  curtime.tm_hour= gps.time.hour();
  curtime.tm_mday= gps.date.day();
  curtime.tm_mon= gps.date.month()-1;
  curtime.tm_year= gps.date.year()-1900;
  curtime.tm_isdst=false;

  Serial.printf("%s %f/%f alt=%f sats=%d hdop=%d dis=%f v=%f\n",
  stime, report.lat, report.lng ,report.alt , +report.sats , +report.hdop ,report.distance, report.volts );
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

void stopLoRa()
{
  LoRa.sleep();
  LoRa.end();
  power.power_LoRa(false);
}

void startLoRa() {
  power.power_LoRa(true);
  Serial.printf("Starting Lora: freq:%lu enableCRC:%d coderate:%d spread:%d bandwidth:%lu txpower:%d\n", config.frequency, config.enableCRC, config.codingRate, config.spreadFactor, config.bandwidth, config.txpower);

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

void SendLora( SensorReport report ) {
  power.led_onoff(true);
  LoRa.beginPacket();
  LoRa.write( (const uint8_t *)&report, sizeof(SensorReport));
  LoRa.endPacket();
  power.led_onoff(false);
}

void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do
  {
    while (Serial1.available())
      gps.encode(Serial1.read());
  } while (millis() - start < ms);
}


void startGPS() {
  power.power_GPS(true);
  Serial1.begin(GPSBAUD, SERIAL_8N1, GPSRX, GPSTX);
  Serial.println("Wake GPS");
  int data = -1;
  do {
    for(int i = 0; i < 20; i++){ //send random to trigger respose
        Serial1.write(0xFF);
      }
    data = Serial1.read();
  } while(data == -1);
  Serial.println("GPS is awake");
}

void stopGPS() {
  const byte CFG_RST[12] = {0xb5, 0x62, 0x06, 0x04, 0x04, 0x00, 0x00, 0x00, 0x01,0x00, 0x0F, 0x66};//Controlled Software reset
  const byte RXM_PMREQ[16] = {0xb5, 0x62, 0x02, 0x41, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x4d, 0x3b};   //power off until wakeup call
  Serial1.write(CFG_RST, sizeof(CFG_RST));
  delay(600); //give some time to restart //TODO wait for ack
  Serial1.write(RXM_PMREQ, sizeof(RXM_PMREQ));
  power.power_GPS(false);
}

GPSLOCK getGpsLock() {
  for (int i=0; i<config.gps_timeout; i++)
  {
    
    Serial.printf("waiting for GPS try: %d  Age:%u  valid: %d   %d\n", i, gps.location.age(), gps.location.isValid(), gps.time.isValid());

    // check whethe we have  gps sig
    if (gps.location.lat()!=0 && gps.location.isValid() )
    {
      // in the report window?
      if (gps.time.hour() >=config.fromHour && gps.time.hour() < config.toHour)
        return LOCK_OK;          
      else
        return LOCK_WINDOW;
    }

    power.flashlight(1);    

    smartDelay(1000);
  }
  return LOCK_FAIL;
}
