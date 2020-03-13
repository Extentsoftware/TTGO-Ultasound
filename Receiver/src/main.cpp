//
// https://github.com/YogoGit/TTGO-LORA32-V1.0/blob/master/Receiver/Receiver.ino
// https://docs.platformio.org/en/latest/boards/espressif32/ttgo-lora32-v1.html#hardware
// https://github.com/lewisxhe/TTGO-LoRa-Series

//UART	RX IO	TX IO	CTS	RTS
//UART0	GPIO3	GPIO1	N/A	N/A
//UART1	GPIO9	GPIO10	GPIO6	GPIO11
//UART2	GPIO16	GPIO17	GPIO8	GPIO7

// override ports for Serial2
//#define RX2 5
//#define TX2 17

#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER   1024
#define SerialMon Serial
#define SerialAT Serial2
#define TINY_GSM_DEBUG SerialMon
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 115200
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#define GSM_PIN ""
#define TINY_GSM_YIELD() { delay(2); }


#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>  
#include <SSD1306.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include "main.h"

#ifdef HAS_OLED

#define OLED_SDA    4
#define OLED_SCL    15
#define OLED_RST    16

#endif

const char * Msg_WaitingForNetwork = "Waiting For Network";
const char * Msg_NetConnectFailed = "Failed to connect to Network";
const char * Msg_NetConnectOk = "Connected to Network";
const char * Msg_MQTTConnectFailed = "Failed to connect to MQTT";
const char * Msg_MQTTConnectOk = "Connect to MQTT";

// MQTT details
const char* broker = "86.21.199.245";
const char apn[]      = "wap.vodafone.co.uk"; // APN (example: internet.vodafone.pt) use https://wiki.apnchanger.org
const char gprsUser[] = "wap"; // GPRS User
const char gprsPass[] = "wap"; // GPRS Password

char* topic;

#ifdef HAS_OLED
SSD1306 display(0x3c, OLED_SDA, OLED_SCL);
#endif
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

void setupMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int len);

enum StartupStage
{
  ModemStart,
  NetworkConnect,
  ConnectMQTT,
  LoraStart,
  Ready,
} startupStage;

struct SensorConfig config;

void SetSimpleMsg(const char * msg)
{
#ifdef HAS_OLED
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 10, msg);
  display.display();
#endif
  SerialMon.println(msg);
}

void loraData(SensorReport report) {
  char topic[32];
  char payload[132];

  sprintf(topic, "bongo/%02x%02x%02x%02x/ultrasound", report.chipid[2], report.chipid[3], report.chipid[4], report.chipid[5]);

  sprintf(payload, "{'distance':%f,'volts':%f}", report.distance, report.volts);
  SerialMon.println(topic);
  SerialMon.println(payload);
  mqtt.publish(topic, payload);

#ifdef HAS_OLED
  int rssi = LoRa.packetRssi();
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

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
  #endif
}

void cbk(int packetSize) {
  
  SensorReport report;
  SerialMon.println("got packet");

  if (packetSize == sizeof(SensorReport))
  {
      unsigned char *ptr = (unsigned char *)&report;
      for (int i = 0; i < packetSize; i++, ptr++)
         *ptr = (unsigned char)LoRa.read(); 

      char *stime = asctime(gmtime(&report.time));
      stime[24]='\0';
      SerialMon.printf("distance=%f\n",report.distance);
      loraData(report);
  }
}

void setup() {
  SerialMon.begin(115200);
  while (!Serial);

  

#ifdef HAS_OLED
  pinMode(OLED_RST,OUTPUT);
  digitalWrite(OLED_RST, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(OLED_RST, HIGH); // while OLED is running, must set GPIO16 in high、
  display.init();
  display.flipScreenVertically();  
#endif

  SetSimpleMsg("Starting");

  startupStage = ModemStart;
}

void doModemStart()
{
  SetSimpleMsg("Modem Starting");
  delay(1000);
  TinyGsmAutoBaud(SerialAT,GSM_AUTOBAUD_MIN,GSM_AUTOBAUD_MAX);
  delay(3000);  
  modem.restart();  
  String modemInfo = modem.getModemInfo();
  delay(1000);  
  SetSimpleMsg("Modem Started");
}

bool doNetworkConnect()
{
  SetSimpleMsg(Msg_WaitingForNetwork);
  boolean status =  modem.waitForNetwork();
  SetSimpleMsg((char*)(status ? Msg_NetConnectOk : Msg_NetConnectFailed));
  //modem.getRegistrationStatus();
  //modem.getOperator();
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" fail");
  }
  return status;
}

bool doConnectMQTT()
{
  // MQTT Broker setup
  uint8_t array[6];
  esp_efuse_mac_get_default(array);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x", array[0], array[1], array[2], array[3], array[4], array[5]);
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
  boolean status = mqtt.connect(macStr);
  SetSimpleMsg((char*)(status ? Msg_MQTTConnectOk : Msg_MQTTConnectFailed));

  return status;
}

void loop() {

  switch (startupStage)
  {
    case  ModemStart:
      doModemStart();
      startupStage = NetworkConnect;
      break;

    case  NetworkConnect:
      startupStage = doNetworkConnect() ? ConnectMQTT: ModemStart;
      break;

    case  ConnectMQTT:
      startupStage = doConnectMQTT() ? LoraStart : NetworkConnect;
      break;

    case LoraStart:
      startLoRa();
      LoRa.receive();
      startupStage = Ready;
#ifdef HAS_OLED
      display.clear();
      display.setFont(ArialMT_Plain_24);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.drawStringMaxWidth(0, 0, 128, "Ready");
      display.display();
#endif
    case Ready:
      int packetSize = LoRa.parsePacket();
      if (packetSize) { cbk(packetSize);  }
      delay(10);
  }
}

void startLoRa() {
  SerialMon.printf("\nStarting Lora: freq:%lu enableCRC:%d coderate:%d spread:%d bandwidth:%lu txpower:%d\n", config.frequency, config.enableCRC, config.codingRate, config.spreadFactor, config.bandwidth, config.txpower);

  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS,RST,DI0);

  int result = LoRa.begin(config.frequency);
  if (!result) 
    SerialMon.printf("Starting LoRa failed: err %d\n", result);
  else
    SerialMon.println("Started LoRa OK");

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
    SerialMon.printf("Starting LoRa failed: err %d", result);
  }  
}

// incoming message
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  SerialMon.print("Message arrived [");
  SerialMon.print(topic);
  SerialMon.print("]: ");
  SerialMon.write(payload, len);
  SerialMon.println();
}

