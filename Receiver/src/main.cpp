//
// https://github.com/YogoGit/TTGO-LORA32-V1.0/blob/master/Receiver/Receiver.ino
// https://docs.platformio.org/en/latest/boards/espressif32/ttgo-lora32-v1.html#hardware
// https://github.com/lewisxhe/TTGO-LoRa-Series

//UART	RX IO	TX IO	CTS	RTS
//UART0	GPIO3	GPIO1	N/A	N/A
//UART1	GPIO9	GPIO10	GPIO6	GPIO11
//UART2	GPIO16	GPIO17	GPIO8	GPIO7

#define TINY_GSM_MODEM_SIM800
#define SerialMon Serial
#define SerialAT Serial2
#define TINY_GSM_DEBUG SerialMon
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 115200
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#define GSM_PIN ""

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>  
#include <SSD1306.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include "main.h"

#define OLED_SDA    4
#define OLED_SCL    15
#define OLED_RST    16

char * Msg_WaitingForNetwork = "Waiting For Network";
char * Msg_NetConnectFailed = "Failed to connect to Network";
char * Msg_NetConnectOk = "Connected to Network";
char * Msg_MQTTConnectFailed = "Failed to connect to MQTT";
char * Msg_MQTTConnectOk = "Connect to MQTT";

// MQTT details
const char* broker = "86.21.199.245";

char* topic;

SSD1306 display(0x3c, OLED_SDA, OLED_SCL);
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

void SetSimpleMsg(char * msg)
{
  display.clear();
  display.drawString(0, 10, msg);
  display.display();
  SerialMon.println(msg);
}

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
  pinMode(OLED_RST,OUTPUT);
  digitalWrite(OLED_RST, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(OLED_RST, HIGH); // while OLED is running, must set GPIO16 in high、
  
  Serial.begin(115200);
  while (!Serial);

  display.init();
  display.flipScreenVertically();  
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 10, "Starting");
  display.display();
  startupStage = ModemStart;
}

void doModemStart()
{
  delay(1000);
  SerialAT.begin(9600);
  delay(1000);  
  modem.restart();  
  String modemInfo = modem.getModemInfo();
  delay(1000);  
}

bool doNetworkConnect()
{
  SetSimpleMsg(Msg_WaitingForNetwork);
  boolean status =  modem.waitForNetwork();
  SetSimpleMsg((char*)(status ? Msg_NetConnectOk : Msg_NetConnectFailed));
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
  mqtt.publish("home/sensor/temp", "1");

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
      display.clear();
      display.setFont(ArialMT_Plain_24);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.drawStringMaxWidth(0, 0, 128, "Ready");
      display.display();

    case Ready:
      int packetSize = LoRa.parsePacket();
      if (packetSize) { cbk(packetSize);  }
      delay(10);
  }
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

// incoming message
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  SerialMon.print("Message arrived [");
  SerialMon.print(topic);
  SerialMon.print("]: ");
  SerialMon.write(payload, len);
  SerialMon.println();
}

