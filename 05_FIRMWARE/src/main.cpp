// NOISYLESS Standard v1.0.6 — S3, WiFi+MQTT+Analog, LED fix, version blink, OTA-ready
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <Update.h>
#include <mbedtls/sha256.h>
#include "wifi_config.h"
#include "credentials.h"

#if ARDUINO_USB_CDC_ON_BOOT
#define HWSerial Serial0
#define USBSerial Serial
#else
#define HWSerial Serial
#define USBSerial Serial
#endif

static const int PIN_LED_R = 12;
static const int PIN_LED_G = 13;
static const int PIN_LED_B = 14;
static const int PIN_LUX1_AIN = 1;
static const int PIN_LUX2_AIN = 2;
static const int PIN_MIC1 = 3;
static const int PIN_MIC2 = 4;

WiFiClient espClient;
PubSubClient mqtt(espClient);

char DEVICE_ID[20];
char MQTT_TOPIC[128];
unsigned long lastPub = 0;
unsigned long greenOnAt = 0;
unsigned long pubFlashOnAt = 0;
unsigned long lastOtaCheck = 0;

const char* FW_VERSION = "1.0.8";
const unsigned long PUB_INTERVAL = 10000;
const unsigned long OTA_INTERVAL = 60000;
const unsigned long LED_BLINK_MS = 150;

// LED logic — INVERTED: LOW = ON, HIGH = OFF
void ledOff()  { digitalWrite(PIN_LED_R,HIGH); digitalWrite(PIN_LED_G,HIGH); digitalWrite(PIN_LED_B,HIGH); }
void ledRed()  { ledOff(); digitalWrite(PIN_LED_R,LOW); }
void ledBlue() { ledOff(); digitalWrite(PIN_LED_B,LOW); }
void ledGreen(){ ledOff(); digitalWrite(PIN_LED_G,LOW); }

// Blink blue N times to show 3rd digit of version
void blinkVersion() {
  int patch = 0;
  // Extract patch number: 1.0.6 → 6, 1.10.3 → 3, etc.
  const char* p = FW_VERSION;
  for (int dots = 0; *p; p++) { if (*p == '.') dots++; if (dots == 2) { patch = atoi(p+1); break; } }
  if (patch <= 0) patch = 1;
  for (int i = 0; i < patch; i++) {
    ledBlue(); delay(LED_BLINK_MS);
    ledOff();   delay(LED_BLINK_MS);
  }
}

void connectWiFi() {
  ledBlue();
  Serial.printf("[WiFi] %s...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (int i=0; i<60; i++) { if (WiFi.status()==WL_CONNECTED) break; delay(250); }
  if (WiFi.status()==WL_CONNECTED) {
    Serial.printf("[WiFi] OK IP:%s\n", WiFi.localIP().toString().c_str());
    // Blink version after WiFi connect
    blinkVersion();
    ledGreen(); greenOnAt=millis();
  } else { Serial.println("[WiFi] FAIL"); ledOff(); }
}

void connectMQTT() {
  if (mqtt.connected()) return;
  char cid[64]; snprintf(cid,sizeof(cid),"nl_%s",DEVICE_ID);
  mqtt.setServer(MQTT_HOST,MQTT_PORT);
  mqtt.connect(cid,MQTT_USER,MQTT_PASS);
}

static String toHex(const uint8_t* buf, size_t len) {
  const char* hex="0123456789abcdef"; String s; s.reserve(len*2);
  for (size_t i=0;i<len;i++){s+=hex[(buf[i]>>4)&0xF];s+=hex[buf[i]&0xF];} return s;
}
void checkOTA() {
  if (WiFi.status()!=WL_CONNECTED) return;
  HTTPClient http; http.setTimeout(10000); http.begin("http://91.99.26.43/firmware/latest.json");
  if (http.GET()!=200){http.end();return;}
  String body=http.getString();http.end();
  auto extract=[&](const char* key)->String{
    int i=body.indexOf("\""+String(key)+"\"");if(i<0)return"";
    i=body.indexOf(":",i);if(i<0)return"";
    i=body.indexOf("\"",i);if(i<0)return"";
    int j=body.indexOf("\"",i+1);if(j<0)return"";
    return body.substring(i+1,j);
  };
  String latest=extract("version"),url=extract("url"),sha256hex=extract("sha256");
  if(!latest.length()||!url.length()||!sha256hex.length())return;
  sha256hex.toLowerCase();
  if(latest==FW_VERSION)return;
  Serial.printf("[OTA] Downloading %s...\n",latest.c_str());
  HTTPClient dl;dl.setTimeout(30000);dl.begin(url);
  if(dl.GET()!=200){dl.end();return;}
  int len=dl.getSize();
  mbedtls_sha256_context ctx;mbedtls_sha256_init(&ctx);mbedtls_sha256_starts_ret(&ctx,0);
  if(!Update.begin(len>0?len:UPDATE_SIZE_UNKNOWN)){dl.end();mbedtls_sha256_free(&ctx);return;}
  WiFiClient* stream=dl.getStreamPtr();
  uint8_t buf[1024];size_t total=0;
  while(dl.connected()&&(len>0||len==-1)){
    size_t avail=stream->available();if(!avail){delay(1);continue;}
    size_t rd=stream->readBytes(buf,avail>sizeof(buf)?sizeof(buf):avail);
    if(!rd)break;total+=rd;
    mbedtls_sha256_update_ret(&ctx,buf,rd);
    if(Update.write(buf,rd)!=rd){Update.abort();break;}
    if(len>0)len-=rd;
  }
  dl.end();
  uint8_t digest[32];mbedtls_sha256_finish_ret(&ctx,digest);mbedtls_sha256_free(&ctx);
  String got=toHex(digest,32);got.toLowerCase();
  if(got!=sha256hex){Serial.println("[OTA] SHA256 mismatch");Update.abort();return;}
  if(!Update.end()||!Update.isFinished()){Serial.println("[OTA] Flash failed");return;}
  Serial.println("[OTA] OK — rebooting");delay(200);ESP.restart();
}

void setup() {
  Serial.begin(115200);delay(500);
  pinMode(PIN_LED_R,OUTPUT);pinMode(PIN_LED_G,OUTPUT);pinMode(PIN_LED_B,OUTPUT);
  ledRed();  // Boot = red
  Serial.println("\n===== NOISYLESS v"+String(FW_VERSION)+" =====");
  analogReadResolution(12);
  connectWiFi();  // blue → version blink → green
  String mac=WiFi.macAddress();mac.replace(":","");mac.toUpperCase();
  snprintf(DEVICE_ID,sizeof(DEVICE_ID),"NL-%s",mac.c_str());
  snprintf(MQTT_TOPIC,sizeof(MQTT_TOPIC),"esp32/noisyless/%s/datas",mac.c_str());
  Serial.printf("[ID] %s\n",DEVICE_ID);
  connectMQTT();
  lastOtaCheck=millis();
  Serial.println("[OK]");
}

void loop() {
  unsigned long now=millis();

  // WiFi watchdog
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  // LED: RED if WiFi lost, else green timing
  if (WiFi.status() != WL_CONNECTED) {
    ledRed();
  } else {
    // LED green: 1s after connect, then off
    if (greenOnAt > 0 && now - greenOnAt > 1000) { greenOnAt = 0; ledOff(); }
    // Publish flash: 200ms green, then back to off (unless greenOnAt still active)
    if (pubFlashOnAt > 0 && now - pubFlashOnAt > 200) { pubFlashOnAt = 0; if (greenOnAt==0) ledOff(); }
  }

  // OTA check every 60s
  if (now - lastOtaCheck >= OTA_INTERVAL) { lastOtaCheck = now; checkOTA(); }

  // Publish sensors every 10s
  if (now - lastPub >= PUB_INTERVAL) {
    lastPub = now;
    uint16_t lux1=analogRead(PIN_LUX1_AIN), lux2=analogRead(PIN_LUX2_AIN);
    unsigned long t0=now; uint16_t min1=4095,max1=0,min2=4095,max2=0;
    while(millis()-t0<50){uint16_t s1=analogRead(PIN_MIC1);uint16_t s2=analogRead(PIN_MIC2);
      if(s1<min1)min1=s1;if(s1>max1)max1=s1;if(s2<min2)min2=s2;if(s2>max2)max2=s2;delay(1);}
    uint16_t mic1=(max1>min1)?max1-min1:0, mic2=(max2>min2)?max2-min2:0;

    char payload[512];
    snprintf(payload,sizeof(payload),
      "{\"product\":\"noisyless_env\",\"client_code\":\"client_demo\",\"device_id\":\"%s\",\"fw_version\":\"%s\",\"lux1_raw\":%u,\"lux2_raw\":%u,\"sound_mic1_pp\":%u,\"sound_mic2_pp\":%u,\"rssi_dbm\":%d,\"uptime_s\":%lu}",
      DEVICE_ID,FW_VERSION,lux1,lux2,mic1,mic2,WiFi.RSSI(),millis()/1000);

    // Only flash LED AFTER successful publish
    if (mqtt.publish(MQTT_TOPIC, payload)) {
      ledGreen(); pubFlashOnAt = now;
    }
  }
  delay(10);
}
