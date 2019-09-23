#ifndef __TTGO_Ultrasound__
#define __TTGO_Ultrasound__

#include "../Common/sensor.h"

#define TRIGPIN      13
#define ECHOPIN      2

#define BATTERY_PIN  35   // battery level measurement pin, here is the voltage divider connected
#define BUSPWR       4    // GPIO04 -- sensor bus power control
#define PWRSDA       21
#define PWRSCL       22

#define SCK          5    // GPIO5  -- SX1278's SCK
#define MISO         19   // GPIO19 -- SX1278's MISO
#define MOSI         27   // GPIO27 -- SX1278's MOSI
#define SS           18   // GPIO18 -- SX1278's CS
#define RST          14   // GPIO14 -- SX1278's RESET
#define DI0          26   // GPIO26 -- SX1278's IRQ(Interrupt Request)

#define FREQUENCY 868E6
#define BAND      125E3   
#define SPREAD       12   
#define CODERATE      6
#define SYNCWORD 0xa5a5
#define PREAMBLE      8
#define TXPOWER      20   // max transmit power
#define MINBATVOLTS 2.7   // minimum voltage on battery - if lower, device goes to deep sleep to recharge

#define GPSRX        34   
#define GPSTX        12   
#define GPSBAUD      9600

struct SensorConfig
{
  char  ssid[16] = "VESTRONG_U";
  char  password[16] = "";
  int   gps_timeout = 4 * 60;    // wait n seconds to get GPS fix
  int   failedGPSsleep = 5 * 60; // sleep this long if failed to get GPS
  int   reportEvery = 60 * 60;   // get sample every n seconds
  int   fromHour = 6;            // between these hours
  int   toHour = 22;             // between these hours
  long  frequency = FREQUENCY;   // LoRa transmit frequency
  int   txpower = TXPOWER;       // LoRa transmit power
  long  preamble = PREAMBLE;     // bits to send before transmition
  int   syncword = SYNCWORD;     // unique packet identifier
  float txvolts = MINBATVOLTS;   // power supply must be delivering this voltage in order to xmit.
  int   lowvoltsleep = 60*60*8;  // sleep this long (seconds) if low on volts (8hrs)
  long  bandwidth = BAND;        // lower (narrower) bandwidth values give longer range but become unreliable the tx/rx drift in frequency
  int   spreadFactor = SPREAD;   // signal processing gain. higher values give greater range but take longer (more power) to transmit
  int   codingRate = CODERATE;   // extra info for CRC
  bool  enableCRC = true;        // cyclic redundancy check mode
} default_config;

enum GPSLOCK
{
    LOCK_OK,
    LOCK_FAIL,
    LOCK_WINDOW
};

void MakeDataPacket(SensorReport report);
double GetDistance();
void stopLoRa();
void startLoRa();
void SendLora(SensorReport report);
void smartDelay(unsigned long ms);
GPSLOCK getGpsLock();
void stopGPS();
void startGPS();

#endif