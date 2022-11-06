#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <FastLED.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include "AsyncJson.h"
#include <ArduinoJson.h>
#include "AsyncUDP.h"
//// BUILDFLAGS PLACEHOLDER

#include "Configuration.h"
#include "AdvancedConfig.h"

#define FIRMWAREVERSION "0.5

// WEBSERVER
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
AsyncWebServer server(80);
unsigned int requestCounter = 0;

bool connectedSend = false;

const char* ssid = "rare";
const char* password = "rare2022";


// PREFERENCES
Preferences pref;
#define PREF_NAME NAMESPACE

//// LED
#define PIN_CH1    18
#define PIN_CH2    19
#define PIN_LED 4
#define LED_COUNT_CH1 128
#define LED_COUNT_CH2 128


CRGB leds_CH1[ LED_COUNT_CH1 ];
CRGB leds_CH2[ LED_COUNT_CH2 ];


bool newOutput_CH1 =  true;
bool newArtNetPackage = false;


// LED MODULE

bool ch1_active = true;
bool ch2_active = false;

//int ch1_modules_leds[MOD_LED_MAX_SEGMENTS];
//int ch2_modules_leds[MOD_LED_MAX_SEGMENTS];

int ch1_modules_leds[MOD_LED_MAX_SEGMENTS] = {30,0,0,0};
int ch2_modules_leds[MOD_LED_MAX_SEGMENTS] = {0,0,0,0};

//String ch1_modules_modes[MOD_LED_MAX_SEGMENTS];
//String ch2_modules_modes[MOD_LED_MAX_SEGMENTS];

String ch1_modules_modes[MOD_LED_MAX_SEGMENTS] = {"full","","",""};
String ch2_modules_modes[MOD_LED_MAX_SEGMENTS] = {"half","","",""};



void moduleLedReadConfig() {

  pref.begin(PREF_NAME);

  ch1_active = pref.getBool("ch1_a",false);
  

  for(int i = 0; i < MOD_LED_MAX_SEGMENTS; i++) {
    String name = "ch1_";
    name += i;
    ch1_modules_leds[i] = pref.getInt(String(name+"_leds").c_str(),0);
    ch1_modules_modes[i] = pref.getString(String(name+"_mod").c_str(),"full");
  }

  ch2_active = pref.getBool("ch2_a",false);
  for(int i = 0; i < MOD_LED_MAX_SEGMENTS; i++) {
    String name = "ch2_";
    name += i;
    ch2_modules_leds[i] = pref.getInt(String(name+"_leds").c_str(),0);
    ch2_modules_modes[i] = pref.getString(String(name+"_mod").c_str(),"full");
  }


  pref.end();

}

void moduleLedWriteConfig() {

  pref.begin(PREF_NAME);

  pref.putBool("ch1_a",ch1_active);
  for(int i = 0; i < MOD_LED_MAX_SEGMENTS; i++) {
    String name = "ch1_";
    name += i;
    pref.putInt(String(name+"_leds").c_str(),ch1_modules_leds[i]);
    pref.putString(String(name+"mod").c_str(),ch1_modules_modes[i]);
  }

  pref.putBool("ch2_a",ch2_active);
  for(int i = 0; i < MOD_LED_MAX_SEGMENTS; i++) {
    String name = "ch2_";
    name += i;
    pref.putInt(String(name+"_leds").c_str(),ch2_modules_leds[i]);
    pref.putString(String(name+"mod").c_str(),ch2_modules_modes[i]);
  }

  pref.end();

}

void moduleLedSetConfigViaJson(StaticJsonDocument<768> doc) {
  if(!doc["channels"]) return;

  if(doc["channels"]["1"]) {
    if(doc["channels"]["1"]["active"] != "null") {
      ch1_active = doc["channels"]["1"]["active"];
    }
    if(doc["channels"]["1"]["segments"] != "null") {
      for( int i = 0; i < MOD_LED_MAX_SEGMENTS; i++) {
        if(doc["channels"]["1"]["segments"][i] != "null") {
          if(doc["channels"]["1"]["segments"][i]["leds"] != "null") {
            ch1_modules_leds[i] = doc["channels"]["1"]["segments"][i]["leds"];
          }
          if(doc["channels"]["1"]["segments"][i]["mode"] != "null") {
            String mode = doc["channels"]["1"]["segments"][i]["mode"];
            ch1_modules_modes[i] = mode;
          }
        }
      }
    }
  }

  if(doc["channels"]["2"]) {
    if(doc["channels"]["2"]["active"] != "null") {
      ch2_active = doc["channels"]["2"]["active"];
    }
    if(doc["channels"]["2"]["segments"] != "null") {
      for( int i = 0; i < MOD_LED_MAX_SEGMENTS; i++) {
        if(doc["channels"]["2"]["segments"][i] != "null") {
          if(doc["channels"]["2"]["segments"][i]["leds"] != "null") {
            ch2_modules_leds[i] = doc["channels"]["2"]["segments"][i]["leds"];
          }
          if(doc["channels"]["2"]["segments"][i]["mode"] != "null") {
            String mode = doc["channels"]["2"]["segments"][i]["mode"];
            ch2_modules_modes[i] = mode;
          }
        }
      }
    }
  }
}

StaticJsonDocument<768>  moduleLedGenerateConfigAsJson() {

  StaticJsonDocument<768> doc;
  JsonObject channels = doc.createNestedObject("channels");

  JsonObject channels_1 = channels.createNestedObject("1");
  channels_1["active"] = ch1_active;

  JsonObject channels_1_segments = channels_1.createNestedObject("segments");

  for( int i = 0 ; i < MOD_LED_MAX_SEGMENTS; i++) {
    if(ch1_modules_leds[i] > 0) {
      JsonObject channels_1_segments_segment = channels_1_segments.createNestedObject(String(i));
      channels_1_segments_segment["leds"] = ch1_modules_leds[i];
      channels_1_segments_segment["mode"] = ch1_modules_modes[i];
    }
  }


  JsonObject channels_2 = channels.createNestedObject("2");
  channels_2["active"] = ch2_active;

  JsonObject channels_2_segments = channels_2.createNestedObject("segments");

  for( int i = 0 ; i < MOD_LED_MAX_SEGMENTS; i++) {
    if(ch2_modules_leds[i] > 0) {
      JsonObject channels_2_segments_segment = channels_2_segments.createNestedObject(String(i));
      channels_2_segments_segment["leds"] = ch2_modules_leds[i];
      channels_2_segments_segment["mode"] = ch2_modules_modes[i];
    }
  }

  return doc;
}





// NETWORK
String sta_ssid = "";
String sta_secret = "";


String sta_fb_ssid = "";
String sta_fb_secret = "";

String ap_ssid = "";
String ap_secret = "";

String deviceName = "";
String serverIp = "";
String serverMDNS = "";

String authToken = "";


IPAddress serverIpAddress;

bool staFbActive = false;
bool apOnlyActive = false;


String serverPort = SERVER_PORT;




// ARTNET and UDP
volatile unsigned long lastPackageA = 0;

AsyncUDP udp;
// UDP specific
#define ART_NET_PORT 6454
// Opcodes
#define ART_POLL 0x2000
#define ART_DMX 0x5000
// Buffers
#define MAX_BUFFER_ARTNET 530
// Packet
#define ART_NET_ID "Art-Net"
#define ART_DMX_START 18

uint16_t opcode;
uint8_t sequence;
uint8_t physical;
uint16_t incomingUniverse;
uint16_t outgoingUniverse;
uint16_t dmxDataLength;

static const char artnetId[] = ART_NET_ID;

struct dataPoint {
	uint16_t pos;
	uint16_t col;
	uint32_t univ;
};

xQueueHandle demo_queue;
dataPoint dataSet[128];


// ARTNET END 





// DUMMY
unsigned long lastMillis  = 0;
String dummyServerIpString = SERVER_IP;



void setup() {

  initBasics();

  readConfig();

  devConfigOut();

  initNetwork();


  initLeds();

  iniApiServer();


moduleLedGenerateConfigAsJson();

}





unsigned long lastDevOut = 0;



unsigned long lastFrameUpdate = 0;
int framesPerSecond = 20;
int framesUpdateInterval = 1000/framesPerSecond;


unsigned long ledOnLast = 0;
int ledOnInterval = 100;



unsigned long lastInitialTransmit;
int intervalTransmit = 5000;

void loop() {

  if (WiFi.isConnected()) {
    ArduinoOTA.handle();

    if(!connectedSend) {
      if( millis() > (intervalTransmit +lastInitialTransmit) ) {
        connectedSend = transmitStatusToServer();
        lastInitialTransmit = millis() ;
      }
    }
  }







  if(newArtNetPackage &&  (ledOnInterval + ledOnLast) < millis() ) {
    newArtNetPackage = false; 
    digitalWrite(PIN_LED,HIGH);
    ledOnInterval = millis();
  } else {
 digitalWrite(PIN_LED,LOW);
  }





  if(newOutput_CH1) {
    newOutput_CH1 = false;
    for(int i=0; i<LED_COUNT_CH1; i++) {
      }
      FastLED.show();
  }


  if( (lastFrameUpdate + framesUpdateInterval) < millis() ) {
      FastLED.show();

    lastFrameUpdate = millis();
  }





}







/// INITS



void initLeds() {
FastLED.addLeds<WS2812, PIN_CH1, GRB>(leds_CH1, LED_COUNT_CH1); 
FastLED.addLeds<WS2812, PIN_CH2, GRB>(leds_CH2, LED_COUNT_CH2); 
  for(int i=0; i<LED_COUNT_CH1; i++) {
        leds_CH1[i].setRGB( 0,0,0);
  }

  for(int i=0; i<LED_COUNT_CH2; i++) {
        leds_CH2[i].setRGB( 0, 0, 0);
  }
  FastLED.show();
}

void initBasics() {
  pinMode(4,OUTPUT);
  pinMode(5,OUTPUT);
  Serial.begin(115200);
}




void readConfig() {
  Serial.println("reading configs");
  pref.begin(PREF_NAME);


  if(!pref.isKey("init")) {
    initialPrefWrite();
  } 

  sta_ssid = pref.getString("sta_ssid",STA_SSID);
  sta_secret = pref.getString("sta_secret",STA_SECRET);
  deviceName = pref.getString("device_name","");
  serverIp = pref.getString("server_ip","");
  serverMDNS = pref.getString("server_mdns","");
  authToken = pref.getString("token","");

  ap_ssid = pref.getString("ap_ssid","");
  ap_secret = pref.getString("ap_secret","");
  sta_fb_ssid = pref.getString("sta_fb_ssid","");
  sta_fb_secret = pref.getString("sta_fb_secret","");
  



  pref.end();


}

void devConfigOut() {
  Serial.print("sta_ssid: ");
  Serial.println(sta_ssid);
  Serial.print("sta_secret: ");
  Serial.println(sta_secret);
  Serial.print("deviceName: ");
  Serial.println(deviceName);
  Serial.print("serverIp: ");
  Serial.println(serverIp);
  Serial.print("serverMDNS: ");
  Serial.println(serverMDNS);
  Serial.print("authToken: ");
  Serial.println(authToken);
  Serial.print("ap_ssid: ");
  Serial.println(ap_ssid);
  Serial.print("sta_fb_ssid: ");
  Serial.println(sta_fb_ssid);
}

void initialPrefWrite(){
  pref.begin(PREF_NAME);

  pref.putBool("init",true);
  pref.putString("token",AUTH_TOKEN);
  pref.putString("device_name",DEVICE_NAME);
  pref.putString("sta_ssid",STA_SSID);
  pref.putString("sta_secret",STA_SECRET);
  pref.putString("sta_fb_ssid",STA_FB_SSID);
  pref.putString("sta_fb_secret",STA_FB_SECRET);
  pref.putString("ap_ssid",AP_SSID);
  pref.putString("ap_secret",AP_SECRET);
  pref.putString("server_ip",SERVER_IP);
  pref.putString("server_mdns",SERVER_MDNS);

  pref.end();
}


void writeConfig() {

  pref.begin(PREF_NAME);


  pref.putString("token",authToken);
  pref.putString("device_name",deviceName);
  pref.putString("sta_ssid",sta_ssid);
  pref.putString("sta_secret",sta_secret);
  pref.putString("sta_fb_ssid",sta_fb_ssid);
  pref.putString("sta_fb_secret",sta_fb_secret);
  pref.putString("ap_ssid",ap_ssid);
  pref.putString("ap_secret",ap_secret);
  pref.putString("server_ip",serverIp);
  pref.putString("server_mdns",serverMDNS);


  pref.end();
}





///// NETWORK

void initNetwork() {
  WiFi.mode(WIFI_STA);

  WiFi.disconnect(true);
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(Wifi_disconnected, SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.begin(ssid, password);

  iniUpdate();



    

 // reportingToServerTimer = xTimerCreate("reportingTimer", pdMS_TO_TICKS(60000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(transmitStatusToServer));
 // transmitStatusToServer();
      
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  udpStart();


}

void Wifi_disconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WIFI access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.disconnected.reason);
  Serial.println("Reconnecting...");
  WiFi.begin(ssid, password);
}



StaticJsonDocument<768> generateReport() {
  StaticJsonDocument<768> doc;
  JsonObject wifi = doc.createNestedObject("wifi");

    JsonObject wifi_station = wifi.createNestedObject("station");
    wifi_station["active"] = true;
    wifi_station["connected"] = WiFi.isConnected();
    wifi_station["SSID"] = WiFi.SSID();
    wifi_station["fallbackSSID"] = sta_fb_ssid;
    wifi_station["ip"] = WiFi.localIP().toString();

    JsonObject wifi_accesPoint = wifi.createNestedObject("accesPoint");
    wifi_accesPoint["active"] = false;
    wifi_accesPoint["SSID"] = ap_ssid;

    JsonObject device = doc.createNestedObject("device");
    device["mac"] =  WiFi.macAddress();
    device["name"] = deviceName;

    JsonObject server = doc.createNestedObject("server");
    server["mDNS"] = serverMDNS;
    server["ip"] = serverIp;
    server["port"] = serverPort;

    JsonObject firmware = doc.createNestedObject("firmware");
    firmware["version"] = "1.33";
    firmware["namespace"] = "test";

    JsonObject runtime = doc.createNestedObject("runtime");
    runtime["requests"] = requestCounter;
    runtime["localMillis"] = millis();

    JsonObject modules_led = doc["modules"].createNestedObject("led");

    JsonObject modules_led_ch1 = modules_led.createNestedObject("ch1");
    modules_led_ch1["active"] = true;
    modules_led_ch1["totalLeds"] = 30;
    modules_led_ch1["modules"] = 1;
    modules_led["ch2"]["active"] = false;


    return doc;
}

void iniApiServer() {



  // GET
  server.on("/api/modules/led", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(moduleLedGenerateConfigAsJson(), *response);
    request->send(response);
  });

  server.on("/api", HTTP_GET, [](AsyncWebServerRequest *request) {
    requestCounter++;
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(generateReport(), *response);
    request->send(response);
  });


  // POST
  server.addHandler(new AsyncCallbackJsonWebHandler(
    "/api/modules/led",
    [](AsyncWebServerRequest* request, JsonVariant& json) {
        if (not json.is<JsonObject>()) {
          request->send(400, "application/json", "{\"error\":\"no valid json\"}");
          return;
        }
        auto&& data = json.as<JsonObject>();


        moduleLedSetConfigViaJson(data);

        String output = "{\"status\":\"ok\"}";
        request->send(200, "application/json", output);
  }));



  server.begin();
}





void iniUpdate() {
 // ArduinoOTA.setHostname(deviceName.c_str());
 // ArduinoOTA.setPort(3232);
//  ArduinoOTA.setPasswordHash(DEVICE_SECRET);
 ArduinoOTA
   .onStart([]() {
     String type;
     if (ArduinoOTA.getCommand() == U_FLASH)
       type = "sketch";
     else // U_SPIFFS
       type = "filesystem";
     Serial.println("Start updating " + type);
   })
   .onEnd([]() {
     Serial.println("\nEnd");
   })
   .onProgress([](unsigned int progress, unsigned int total) {
     Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
   })
   .onError([](ota_error_t error) {
     Serial.printf("Error[%u]: ", error);
     if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
     else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
     else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
     else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
     else if (error == OTA_END_ERROR) Serial.println("End Failed");
   });

 ArduinoOTA.begin();
}





bool transmitStatusToServer() {



  if (!WiFi.isConnected()) return false;

    Serial.println("transmitting…");

  WiFiClient client;
  HTTPClient http;



  String url ="http://192.168.1.50:8011/api/device/connect";

  http.begin(client,"192.168.1.50",8011,"/api/device/connect");
  http.addHeader("Content-Type", "application/json");

  String content = "";
  serializeJson(generateReport(),content);


  int httpResponseCode = http.POST(content);

  Serial.println(httpResponseCode);


  http.end();

  Serial.println("…ended");

  if(httpResponseCode > 0) {
    return true;
  } else {
    return false;
  }

}




void udpStart() {
	if(udp.listen(6454)) {
        Serial.print("udp opened: ");
        Serial.println(WiFi.localIP());
        udp.onPacket([](AsyncUDPPacket packet) {
					lastPackageA = millis();
					paseArtnet(packet.data(),packet.length());

        newArtNetPackage = true;
	
   

					uint8_t * tts = packet.data();

					uint16_t c  = 0;
				uint16_t pos = 0;

				dataPoint currentDS[128];




				for (uint16_t i = ART_DMX_START ; i < dmxDataLength ; i++){
					if(c < 2 ) {
						c++;
					} else {

						uint32_t tmp2 = 0;

						uint16_t tmp1 = 0;


						tmp2 = tts[i-2] ;
						tmp2 <<= 8 ;
						tmp2 |= tts[i-1] ;
						tmp2 <<= 8 ;
						tmp2 |= tts[i] ;




						tmp1 = RGB888toRGB565(tmp2);
				//		setColorToRow(incomingUniverse,pos,tmp1);
				//		rows[incomingUniverse].setColorToBuffer(pos,tmp1);

		    dataPoint currentDp;

				if(pos < 128) {
					currentDS[pos].col = tmp1;
					currentDS[pos].pos = pos;
					currentDS[pos].univ = incomingUniverse;
				}
						c = 0;
						pos++;
					}

			  }


   


            for( int i = 0; i < LED_COUNT_CH1;i++) {
              unsigned r = (currentDS[i].col & 0xF800) >> 8;
              unsigned g = (currentDS[i].col & 0x07E0) >> 3;
              unsigned b = (currentDS[i].col & 0x1F) << 3;


              leds_CH1[i].setRGB( r,g,b);

            }

        });
    }
}



uint16_t paseArtnet(uint8_t * dat, uint16_t len){
  if (len <= MAX_BUFFER_ARTNET && len > 0) {
		uint16_t tmpCode = memcmp(dat, artnetId, sizeof(artnetId));
      if (tmpCode != 0) {
        return 0;
			}
      opcode = dat[8] | dat[9] << 8;
      if (opcode == ART_DMX){
        sequence = dat[12];
        incomingUniverse = dat[14] | dat[15] << 8;
        dmxDataLength = dat[17] | dat[16] << 8;
        return ART_DMX;
      }
      if (opcode == ART_POLL){
        return ART_POLL;
      }
  }
  return 0;
}

uint16_t RGB888toRGB565(uint32_t rgb32)
{
//  long rgb32=strtoul(rgb32_str_, 0, 16);
  return (rgb32>>8&0xf800)|(rgb32>>5&0x07e0)|(rgb32>>3&0x001f);
}