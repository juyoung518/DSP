#include </Users/juyoung/Library/Arduino15/packages/esp32/hardware/esp32/1.0.4/tools/sdk/include/driver/driver/adc.h>
#include <chrono>
#include <thread>
#include <string>
#include <sstream>
#include <algorithm>

#include<iostream>
#include <math.h>
#include <WiFi.h>
//#include <WiFiClient.h>
//#include <WiFiAP.h>
#include <LiquidCrystal_I2C.h>
#include <adcfilter.h>
#include "esp_wifi.h"
using namespace std;

#include <uart_ctrl.h>

#include "ESPAsyncWebServer.h"
#include "FS.h"
#include "ASyncTCP.h"

#include <PubSubClient.h>

#define ON_OFF_THRESHOLD  100

/* ------------------ Configs ------------------ */

// UART  RCV Buffer
#define WIFI_CREDENTIAL_SZ 40

uint8_t* rxBuf = (uint8_t*) malloc(RX_BUF_SIZE+1);

typedef struct wifi_creds {
  char ssid[WIFI_CREDENTIAL_SZ + 1];
  char passwd[WIFI_CREDENTIAL_SZ + 1];
} wifi_creds_t;

// Connected Devices
#define maxConn 30
string connList[maxConn];
short int connCount = 0;

// WiFi Credentials
// APT
const char* apSSID = "gbrainHHI";
const char* apPASS = "gbrain1908";

WiFiServer hhiAP(80);
AsyncWebServer wbServer(80);
#define WIFI_CREDENTIAL_SZ 40
WiFiClient wifi_sta_cli;

// LCD Settings
const int lcdColumns = 16;
const int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

// MQTT Settings
PubSubClient client(wifi_sta_cli);
const char* mqttServer = "mqtt.eclipse.org";
const int mqttPort = 1883;
static char temp_topic_id[21];
static char temp_client_id[21];

/* ------------------ Functions ------------------ */

// Random String
static void get_random_string(char *str, unsigned int len)
{
    unsigned int i;

    // reseed the random number generator
    srand(time(NULL));
    
    for (i = 0; i < len; i++)
    {
        // Add random printable ASCII char
        str[i] = (rand() % ('~' - ' ')) + ' ';
    }
    str[i] = '\0';
}

// Improved Delay Function
void alternateDelay(int delayMS)
{
    int currentTime = millis();
    while ((millis() - currentTime) < delayMS){
        // Empty loop
    }
}

// WiFi Station(Client) Setup
void WiFiSetup(const char* ssid, const char* password)
{

    // Try WiFi Connection : Empty Loop
    // Display details on LCD
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Connecting to : ");
    alternateDelay(200);
    lcd.setCursor(0, 1);
    //lcd.print(ssid);

    WiFi.begin(ssid, password);

    // Empty Loop until connection
    while (WiFi.status() != WL_CONNECTED) {
        alternateDelay(300);
    }

    // Print wifi status
    lcd.clear();
    alternateDelay(200);
    lcd.setCursor(0,0);
    lcd.print("Connected : w");
    alternateDelay(200);
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
}

// WiFi AP setup
void SoftAPSetup(const char* ssid, const char* password, const char* topicID)
{
    // WiFi AP Setup
    WiFi.softAP(apSSID, apPASS);
    // Wait for AP Start
    alternateDelay(100);
    // Static WiFi IP
    WiFi.softAPConfig(IPAddress(192, 168, 9, 9), IPAddress(192, 168, 9, 9), IPAddress(255, 255, 255, 0));
    IPAddress myIP = WiFi.softAPIP();
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("AP Open :");
    lcd.setCursor(0, 1);
    lcd.print(myIP);
    //hhiAP.begin();
    propogateWiFiInfo(ssid, password, topicID);
}

void waitClient(void)
{
    wifi_sta_list_t wifi_sta_list;
    tcpip_adapter_sta_list_t adapter_sta_list;

    memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
    memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

    // until receive 1 byte from host
    while (uart_read_bytes(UART_NUM_1, rxBuf, 1, 1000 / portTICK_RATE_MS) < 1){
        //WiFiClient client = hhiAP.available();

        // Get list of connected devices
        esp_wifi_ap_get_sta_list(&wifi_sta_list);
        tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);

        // Loop through connected devices
        for (int i = 0; i < adapter_sta_list.num; i++) {
            // station : connected deivce info (struct)
            tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
            // Save MAC address to string macStr
            char macAddr[18];
            sprintf(macAddr, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(station.mac));
            string macStr = macAddr;
            // Save IP address to ipAddr -> convert to (string) ipStr
            char ipAddr[18];
            sprintf(ipAddr, "%s", ip4addr_ntoa(&(station.ip)));
            string ipStr = ipAddr;
            // Check if deivce is already recorded
            bool alreadyExists;
            for (int j=0; j<maxConn; j++){
                if (connList[j] == macStr){
                    alreadyExists = true;
                    break;
                } else {
                    alreadyExists = false;
                }
            }
            // If not, add macStr, ipAddr to list.
            if (alreadyExists == false){
                connList[connCount] = macStr;
                //ipList[connCount] = ipAddr;
                connCount += 1;
                uartSend(macAddr);
                Serial.println(macAddr);
            }
            alternateDelay(500);
        }
    }
}


void propogateWiFiInfo(const char* ssid, const char* password, const char* topicID)
{
    String wsData(String(ssid) + "\n" + String(password) + "\n" + String(topicID));
    wbServer.on("/propogate", HTTP_GET, [wsData](AsyncWebServerRequest *request){
      request->send(200, "text/plain", wsData);
    });

    wbServer.begin();
}


// Uart Comm
wifi_creds_t* read_wifi_creds(void) {
  
  wifi_creds_t* wifi_creds = (wifi_creds_t*) malloc(sizeof(wifi_creds_t));
  
  int bufferedSize;
  bool wifiCredsRcvd = false;
  while (!wifiCredsRcvd) {
    uart_get_buffered_data_len(UART_NUM_1, (size_t*) &bufferedSize);
    
    if (bufferedSize >= 2*WIFI_CREDENTIAL_SZ)
      wifiCredsRcvd = true;
      
    delay(100);
  }
  
  uart_read_bytes(UART_NUM_1, rxBuf, 2*WIFI_CREDENTIAL_SZ, 100);
  parse_wifi_info(wifi_creds);
  
  return wifi_creds;
}


// Uart Parser
void parse_wifi_info(wifi_creds_t* wifi_creds) {
  strncpy(wifi_creds->ssid, (const char*) rxBuf, WIFI_CREDENTIAL_SZ);
  wifi_creds->ssid[WIFI_CREDENTIAL_SZ] = NULL;
  
  strncpy(wifi_creds->passwd, (const char*) rxBuf + WIFI_CREDENTIAL_SZ, WIFI_CREDENTIAL_SZ);
  wifi_creds->passwd[WIFI_CREDENTIAL_SZ] = NULL;
}

// MQTT Publish
void mqtt_publish(const char* topicID, bool tengON){
  if (tengON == true) {
    client.publish(topicID, "ON");
  } else {
    client.publish(topicID, "OFF");
  }
}

// MQTT Reconnect
void mqtt_reconnect(void){
  while (!client.connected()){
    char* client_id = (char *) calloc(1, strlen(temp_client_id) + 1);
    strcpy(client_id, temp_client_id);
    client.connect(client_id);
  }
}


/* ------------------ Arduino Code ------------------ */

void setup() {
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0,0);
    lcd.print("GBRAIN HHI");

    // Serial
    Serial.begin(115200);

    // ADC
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);
    adc1_config_width(ADC_WIDTH_BIT_12);

    // Start UART
    uartInit();
    uart_flush(UART_NUM_1);

    // WiFi Credential struct
    wifi_creds_t* wifi_creds = read_wifi_creds();

    // Test
    get_random_string(temp_topic_id, 10);
    get_random_string(temp_client_id, 10);

    char* client_id = (char *) calloc(1, strlen(temp_client_id) + 1);
    strcpy(client_id, temp_client_id);
    char* topic_id = (char *) calloc(1, strlen(temp_topic_id) + 1);
    strcpy(topic_id, temp_topic_id);
    Serial.println(topic_id);
    
    // Start SoftAP
    SoftAPSetup((const char*) wifi_creds->ssid, (const char*) wifi_creds->passwd, topic_id);

    uart_flush(UART_NUM_1);
    
    // Wait until all Clients are connected : user sends a single byte to stop waiting
    waitClient();

    // Turn off AP
    WiFi.mode(WIFI_OFF);
    wbServer.end();

    // Connect to given WiFi
    WiFiSetup((const char*) wifi_creds->ssid, (const char*) wifi_creds->passwd);

    client.setServer(mqttServer, mqttPort);
    mqtt_reconnect();
}

adcFilter lpfFilt;
adcFilter envFilt;

void loop() {
    float num[] = {1.2345, 1.2133, 1.3457, 1.3463, 1.2356};
    float den[] = {1.0, 1.0, 1.0, 1.0};
    lpfFilt.init(num, den, 1.0, 1);
    envFilt.init(num, den, 1.0, 1);

    int threshold = 0;

    for (int i=0; i<3000; i++){
        int val = envFilt.envelope(lpfFilt.lpf(adc1_get_raw(ADC1_CHANNEL_4)));
        if (val > threshold && i > 1000) {
            threshold = val;
        }
        delayMicroseconds(1935);
    }

    threshold += 1;

    //Serial.println(threshold);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Start");

    int startTime = 0;
    bool relayON = false;
    short int relayONCount = 0;
    short int relayOFFCount = 0;
    int valSave = 0;
    while (true) {
        int valSave = 0;
        for (int i=0; i < 2; i++){
          int val = envFilt.envelope(lpfFilt.lpf(adc1_get_raw(ADC1_CHANNEL_4)));
          valSave += (int) val/2;
          delayMicroseconds(1900);
        }
        
        char valChr[6];
        sprintf(valChr, "*%04d", valSave);
        uartSend(valChr);
        //Serial.println(valChr);
        if (uart_read_bytes(UART_NUM_1, rxBuf, 1, 1) >= 1) {
          mqtt_reconnect();
          char* topic_id = (char *) calloc(1, strlen(temp_topic_id) + 1);
          client.publish(topic_id, "RST");
          ESP.restart();
        }
        if (valSave > threshold){
            relayONCount += 1;
            relayOFFCount = 0;
            if (relayON == false && relayONCount > ON_OFF_THRESHOLD) {
                //Serial.println("Relay ON!");
                relayON = true;
                mqtt_reconnect();
                char* topic_id = (char *) calloc(1, strlen(temp_topic_id) + 1);
                strcpy(topic_id, temp_topic_id);
                mqtt_publish(topic_id, true);
            }
        } else {
            relayONCount = 0;
            relayOFFCount += 1;
            if (relayON == true && relayOFFCount > ON_OFF_THRESHOLD) {
                //Serial.println("Relay OFF!");
                relayON = false;
                mqtt_reconnect();
                char* topic_id = (char *) calloc(1, strlen(temp_topic_id) + 1);
                strcpy(topic_id, temp_topic_id);
                mqtt_publish((const char*) topic_id, false);
            }
        }
    }
    exit(0);
}
