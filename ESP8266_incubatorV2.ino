#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include "LittleFS.h"

#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <DHT.h>
#include <ArduinoJson.h> 
#include "AsyncJson.h"

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";

//Variables to save values from HTML form
String ssid;
String pass;
String ip;
String gateway;
String configJson;
// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";
const char* gatewayPath = "/gateway.txt";
const char* incubatorConfigPath = "/incubatorConfig.json";

IPAddress localIP;
//IPAddress localIP(192, 168, 1, 200); // hardcoded

// Set your Gateway IP address
IPAddress localGateway;
//IPAddress localGateway(192, 168, 1, 1); //hardcoded
IPAddress subnet(255, 255, 0, 0);

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

// Set LED GPIO
const int ledPin = 2;
// Stores LED state

String ledState;

boolean restart = false;

/** START ************/
#define WDT_TIMEOUT WDTO_8S // if defined, enable hardware watchdog
#define DHTPIN 0 // data pin of the DHT T/H sensor
#define HEATER_PIN 14 // data pin of the HEATER
#define HUMIDITY_PIN 2 // data pin of the HUMIDITY
#define TURN_LEFT_PIN 12 // data pin of the TURN LEFT
#define TURN_RIGHT_PIN 13 // data pin of the TURN RIGHT
#define DELAY 2000 // loop delay in ms

#define analogPin A0
#define apName "O3-WIFI-MANAGER"
DHT dht(DHTPIN, DHT21);
LiquidCrystal_I2C lcd(0x3E, 16, 2);



byte DotUp[8] = {
  0b01100,
  0b01100,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};
#define RIGHT 16
#define UP 8
#define DOWN 4
#define LEFT 2
#define SELECT 1
#define NO_KEY 0

byte getKey() {  
  return NO_KEY;
  int key = analogRead(analogPin);
//   Serial.println(key);
  
  if (key < 400) {
    return NO_KEY;
  } else if (key < 570) {
    return DOWN;
  } else if (key < 630) {
    return UP;
  } else if (key < 700) {
    return SELECT;
  } else {
    return NO_KEY;
  }
}

float Ts, Hs, Tsm, Hsm, To, Ho; // set points & offset
byte Hcontrol; // H control mode
byte Ts_changed, Hs_changed, Tsm_changed, Hsm_changed, To_changed, Ho_changed ; // setpoint & offset changed flags
float ET, dETdt, IETdt; // prop/diff/integ terms for T
float EH, dEHdt, IEHdt; // prop/diff/integ terms for H
float T, Tavg = NAN, Tvar, Tstd;
float H, Havg = NAN, Hvar, Hstd;
unsigned long t0, t10, t20, t30;
byte displayMode;
byte key;
int turnInterval, turnDuration, turnIntervalCD, turnDurationCD;
byte turnIntervalMode, turnDurationUnit, turnIntervalUnit , turnDurationStatus, turnIntervalModeChange;
byte TI_changed, TIU_changed, TIM_changed, TD_changed, TDU_changed, TDS_changed;
byte upS,  upM, upH, upD;
unsigned long upSx;
unsigned long timerIntervalSec, timerDurationSec, timerDurationLeft;
float intUnt, durUnt;
byte On = 0;
byte Off = 1;
boolean SenErr, turnManual, heaterManual,humidityManual;
boolean heaterStatus, humidityStatus,turnLeftStatus, turnRightStatus;
/** END ************/

// Initialize LittleFS
void initFS() {
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  else{
    Serial.println("LittleFS mounted successfully");
  }
}


// Read File from LittleFS
String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;
  }
  file.close();
  return fileContent;
}

// Write file to LittleFS
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, "w");
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- frite failed");
  }
  file.close();
}

// Initialize WiFi

bool initWiFi()   {
  lcd.clear();
  if(ssid=="" || ip==""){    
    lcd.setCursor(0, 0);
//    lcd.print("AP : ");
    lcd.print(apName);
    lcd.setCursor(0, 1);
    lcd.print("Open:");
    lcd.print(ip);
    Serial.println("Undefined SSID or IP address.");
    delay(500);
    return false;
  }

  WiFi.mode(WIFI_STA);
  localIP.fromString(ip.c_str());
  localGateway.fromString(gateway.c_str());

// if (!WiFi.config(localIP, localGateway, subnet )){
//   lcd.setCursor(0, 0);
//   lcd.print("STA Failed");
//   Serial.println("STA Failed to configure");
//   delay(500);
//   return false;
// }
  WiFi.begin(ssid.c_str(), pass.c_str());
  
  lcd.setCursor(0, 0);
  lcd.print("Connecting to :");
  lcd.setCursor(0, 1);
  lcd.print(ssid.c_str());
  Serial.print("Connecting to : ");
  Serial.println(ssid.c_str());
//  delay(20000);
//  if(WiFi.status() != WL_CONNECTED) {
//    // Serial.println("Failed to connect.");
//    lcd.setCursor(0, 0);
//    lcd.print("Failed to connect.");
//    return false;
//  }
   while (WiFi.status() != WL_CONNECTED) {
     Serial.print('.');
     delay(1000);
   }
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
//  delay(10000);
  if(WiFi.status() != WL_CONNECTED) {
    lcd.setCursor(0, 1);
    lcd.print("Failed to connect.");
    Serial.println("Failed to connect.");
    return false;
  }
  lcd.clear();
  lcd.print("Web Config :");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  Serial.println(WiFi.localIP());
  delay(10000);
  return true;
}

// Replaces placeholder with LED state value
String processor(const String& var) {
  if(var == "STATE") {
    if(!digitalRead(ledPin)) {
      ledState = "ON";
    }
    else {
      ledState = "OFF";
    }
    return ledState;
  }
  return String();
}


String dataIncubator(){
  String responseData;
  StaticJsonDocument<500> jsonData;
  jsonData["T"] = T;
  jsonData["Ts"] = Ts;
  jsonData["Tsm"] = Tsm;
  jsonData["To"] = To;
  jsonData["H"] = H;
  jsonData["Hs"] = Hs;
  jsonData["Hsm"] = Hsm;
  jsonData["Ho"] = Ho;
  jsonData["Hcontrol"] = Hcontrol;
  jsonData["turnInterval"] = turnInterval;  
  jsonData["turnIntervalUnit"] = turnIntervalUnit;
  jsonData["turnIntervalMode"] = turnIntervalMode;
  jsonData["turnIntervalModeChange"] = turnIntervalModeChange;    
  jsonData["turnDuration"] = turnDuration;  
  jsonData["turnDurationUnit"] = turnDurationUnit;
  jsonData["turnDurationStatus"] = turnDurationStatus;
  jsonData["timerIntervalSec"] = timerIntervalSec;
  jsonData["timerDurationSec"] = timerDurationSec;
  jsonData["turnManual"] = turnManual;
  jsonData["heaterManual"] = heaterManual;
  jsonData["humidityManual"] = humidityManual;
  jsonData["heaterStatus"] = heaterStatus;
  jsonData["humidityStatus"] = humidityStatus;
  jsonData["turnLeftStatus"] = turnLeftStatus;
  jsonData["turnRightStatus"] = turnRightStatus;
  serializeJson(jsonData, responseData);
  String tipeResponse = "{\"tipe\":\"dataIncubator\",\"data\":"+responseData+"}";
  return tipeResponse;
}

void notFound(AsyncWebServerRequest *request){
  if (request->method() == HTTP_OPTIONS){    
    request->send(200);
  }else{
    request->send(404, "application/json", "{\"message\":\"Not found\"}");
  }
}

void notifyClients(String dataValues,uint32_t id) {
  if(id){
    ws.text(id,dataValues);
  }else{
    ws.textAll(dataValues);  
  }
  
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len, uint32_t id) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    
    StaticJsonDocument<100> json;
    DeserializationError err = deserializeJson(json, data);
    if (err) {
        Serial.print(F("deserializeJson() failed with code "));
        Serial.println(err.c_str());
        return;
    }
  
    const char *tipe = json["tipe"];
    if (json["tipe"] = "t-config") {
      String data;
      serializeJson(json["data"],data);
      Serial.println(data.c_str());
    }else{
      Serial.print("Not implemet ");
      Serial.println(tipe);
    }
    
    String tipeResponse = "{\"tipe\":\"response\",\"message\":\"success\"}";
    notifyClients(tipeResponse,id);
    
//    free(data);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
    switch (type) {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        notifyClients(dataIncubator(),client->id());
        break;
      case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
      case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len,client->id());
        break;
      case WS_EVT_PONG:
      case WS_EVT_ERROR:
        break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}


void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(HUMIDITY_PIN, OUTPUT);
  pinMode(TURN_LEFT_PIN, OUTPUT);
  pinMode(TURN_RIGHT_PIN, OUTPUT);

  digitalWrite(HEATER_PIN, Off);
  digitalWrite(HUMIDITY_PIN, Off);
  digitalWrite(TURN_LEFT_PIN, Off);
  digitalWrite(TURN_RIGHT_PIN, Off);
  
  lcd.begin(16,2);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("SMART INCUBATOR");
  lcd.setCursor(0, 1);      
  lcd.print("BY OBENGKUMANA");
  delay(5000);
  lcd.createChar(0, DotUp);
  dht.begin();

  initFS();
  String inConfig = readFile(LittleFS, incubatorConfigPath);
  DynamicJsonDocument incubatorConfig(1024);
  deserializeJson(incubatorConfig,inConfig);
  Ts = incubatorConfig["Ts"];
  if(!(0<Ts && Ts<50)) {
    Ts = incubatorConfig["Ts"] = 37.8;
    // write_float(TS_ADDR, Ts=37.8);
  }

  Tsm = incubatorConfig["Tsm"];
  if(!(0<Tsm && Tsm<50)) {
    Tsm = incubatorConfig["Tsm"] = 37.5;
  }

  To = incubatorConfig["To"];
  if(!(-10<To && To<20)) {
    To = incubatorConfig["To"] = 0.0;
  }
  
  Hs = incubatorConfig["Hs"];
  if(!(0<Hs && Hs<100)) {
    Hs = incubatorConfig["Hs"] = 60;    
  }

  Hsm = incubatorConfig["Hsm"];
  if(!(0<Hsm && Hsm<100)) {
    Hsm = incubatorConfig["Hsm"] = 55;
  }

  Ho = incubatorConfig["Ho"];
  if(!(-30<Ho && Ho<30)) {
    Ho = incubatorConfig["Ho"] = 0.0;
  }

  turnInterval = turnIntervalCD = incubatorConfig["turnInterval"];
  turnDuration = turnDurationCD = incubatorConfig["turnDuration"];
  turnIntervalUnit = incubatorConfig["turnIntervalUnit"];
  turnIntervalMode = turnIntervalModeChange = incubatorConfig["turnIntervalMode"];
  turnDurationUnit = incubatorConfig["turnDurationUnit"];
  turnDurationStatus = incubatorConfig["turnDurationStatus"];
  Hcontrol = incubatorConfig["Hcontrol"];

  intUnt = turnIntervalUnit == 1 ? 60 : (turnIntervalUnit > 1 ? 3600 : 1);
  timerIntervalSec = (turnIntervalCD * intUnt);
  durUnt = turnDurationUnit == 1 ? 60 :1;
  timerDurationSec = (turnDurationCD * durUnt);
  sei();

  
  

  // Set GPIO 2 as an OUTPUT
//  pinMode(ledPin, OUTPUT);
//  digitalWrite(ledPin, LOW);
  
  // Load values saved in LittleFS
  ssid = readFile(LittleFS, ssidPath);
  pass = readFile(LittleFS, passPath);
  ip = readFile(LittleFS, ipPath);
  gateway = readFile (LittleFS, gatewayPath);
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);
  Serial.println(gateway);

  if(initWiFi()) {
    initWebSocket();
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/index.html", "text/html");
    });
    
    server.serveStatic("/", LittleFS, "/");
    

    server.on("/json/config", HTTP_GET, [](AsyncWebServerRequest *request) {
      // request->send(200,  "application/json", configJson);
      request->send(LittleFS, "/incubatorConfig.json", "application/json", false, processor);
    });

    server.on("/json/data", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "application/json", dataIncubator());
    });

    AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/json/submit", [](AsyncWebServerRequest *request, JsonVariant &json) {
      StaticJsonDocument<500> jsonData;
      if (json.is<JsonArray>()){
        jsonData = json.as<JsonArray>();
      }else if (json.is<JsonObject>()){
        jsonData = json.as<JsonObject>();
      }
          
      StaticJsonDocument<300> jsonSave;      
      Ts = jsonSave["Ts"] = jsonData["data"]["Ts"];
      Tsm = jsonSave["Tsm"] = jsonData["data"]["Tsm"];
      To = jsonSave["To"] = jsonData["data"]["To"];
      Hs = jsonSave["Hs"] = jsonData["data"]["Hs"];
      Hsm = jsonSave["Hsm"] = jsonData["data"]["Hsm"];
      Ho = jsonSave["Ho"] = jsonData["data"]["Ho"];
      Hcontrol = jsonSave["Hcontrol"] = jsonData["data"]["Hcontrol"];
      turnInterval = jsonSave["turnInterval"] = jsonData["data"]["turnInterval"];  
      turnIntervalMode = jsonSave["turnIntervalUnit"] = jsonData["data"]["turnIntervalUnit"];
      turnIntervalMode = jsonSave["turnIntervalMode"] = jsonData["data"]["turnIntervalMode"];
      turnDuration = jsonSave["turnDuration"] = jsonData["data"]["turnDuration"];      
      turnDurationUnit = jsonSave["turnDurationUnit"] = jsonData["data"]["turnDurationUnit"];
      turnDurationStatus = jsonSave["turnDurationStatus"] = jsonData["data"]["turnDurationStatus"];
      
      turnIntervalCD = turnInterval;
      intUnt = turnIntervalUnit == 1 ? 60 : (turnIntervalUnit > 1 ? 3600 : 1);
      timerIntervalSec = (turnIntervalCD * intUnt);

      turnDurationCD = turnDuration;
      durUnt = turnDurationUnit == 1 ? 60 :1;          
      timerDurationSec = (turnDurationCD * durUnt);
      
      String response;
      serializeJson(jsonSave, response);
      writeFile(LittleFS, incubatorConfigPath, response.c_str());
      request->send(200, "application/json", "{\"message\":\"success\"}");
      Serial.println(response);
    });
    server.addHandler(handler);server.onNotFound(notFound);
    
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Allow", "HEAD,GET,PUT,POST,DELETE,OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, HEAD, POST, PUT");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "X-Requested-With, X-HTTP-Method-Override, Content-Type, Cache-Control, Accept");
    server.begin();
  } else {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP(apName, NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP); 
    // delay(500);
    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(LittleFS, "/wifimanager.html", "text/html");
    });
    
    server.serveStatic("/", LittleFS, "/");
    
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        const AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(LittleFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(LittleFS, passPath, pass.c_str());
          }
          // HTTP POST IP value
          if (p->name() == PARAM_INPUT_3) {
            ip = p->value().c_str();
            Serial.print("IP set to: ");
            Serial.println(ip);
            // Write file to save value
            writeFile(LittleFS, ipPath, ip.c_str());
          }
          // HTTP POST Gateway value
          if (p->name() == PARAM_INPUT_4) {
            gateway = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(gateway);
            // Write file to save value
            writeFile(LittleFS, gatewayPath, gateway.c_str());
          }
        }
      }
      restart = true;
      request->send(200, "text/plain", "Done. Perangkat akan restart. Menghubungkan ke router. Silahkan buka IP yang tercantum di LCD");
    });
    server.begin();
  }
}

void loop() {
  if (restart){
    delay(5000);
    ESP.restart();
  }

  if(WiFi.status() != WL_CONNECTED) {    
    return;
  }
  unsigned long t1 = millis();
  unsigned long t11 = t1;
  unsigned long t21 = t1;
  unsigned long t31 = t1;
  
  byte IHleft, IMleft, ISLeft;
  byte DHleft, DMleft, DSLeft;
  int dt = t1 - t0;
  int dt1 = t11 - t10;
  int dt2 = t21 - t20;
  int dt3 = t31 - t30;

  if ( dt3 > 250 ){
    ws.cleanupClients();
    if(turnManual){
      turnManual = turnLeftStatus = turnRightStatus = 0;
      digitalWrite(TURN_LEFT_PIN, Off);
      digitalWrite(TURN_RIGHT_PIN, Off);
    }
    if(heaterManual){
      heaterManual = heaterStatus = 0;
      digitalWrite(HEATER_PIN, Off);
    }

    if(humidityManual){
      humidityManual = humidityStatus = 0;
      digitalWrite(HUMIDITY_PIN, Off);
    }
  }

  if ( dt2 > 1000 ){
    ISLeft = timerIntervalSec % 60;
    IMleft = ( timerIntervalSec % 3600) / 60;
    IHleft = timerIntervalSec / 3600;

    DSLeft = timerDurationSec % 60;
    DMleft = ( timerDurationSec % 3600) / 60;
    if(!SenErr || !turnManual ) {
      if( timerIntervalSec > 0 ) {
        Serial.print("Turn Off : ");
        Serial.print(IHleft);
        Serial.print(":");
        Serial.print(IMleft);
        Serial.print(":");
        Serial.println(ISLeft);
        timerIntervalSec = --timerIntervalSec;
      }else{
        if(turnDurationStatus){
          if( timerDurationSec > 0  ){
            turnIntervalModeChange == 1 ? turnRightStatus = 1 : turnLeftStatus = 1;
            digitalWrite(turnIntervalModeChange == 1 ? TURN_RIGHT_PIN : TURN_LEFT_PIN , On); 
            Serial.print("Turn On : ");
            Serial.print(turnIntervalModeChange == 1 ? "R " : "L ");
            Serial.print(DMleft);
            Serial.print(":");
            Serial.println(DSLeft);
            timerDurationSec = --timerDurationSec;
          }else{
             /****************** RESTART TURN INTERVAL & DURATION ***************/
             turnIntervalCD = turnInterval;
             intUnt = turnIntervalUnit == 1 ? 60 : (turnIntervalUnit > 1 ? 3600 : 1);
             timerIntervalSec = (turnIntervalCD * intUnt);
  
             turnDurationCD = turnDuration;
             durUnt = turnDurationUnit == 1 ? 60 :1;          
             timerDurationSec = (turnDurationCD * durUnt);
             
             if (turnIntervalMode == 2){
              turnIntervalModeChange = turnIntervalModeChange == 1 ? 0 : 1;
             }else{
              turnIntervalModeChange = turnIntervalMode;
             }
             
             turnLeftStatus = turnRightStatus = 0;
             digitalWrite(TURN_LEFT_PIN, Off);
             digitalWrite(TURN_RIGHT_PIN, Off);
          }
        }else{
          Serial.println("Turn tray is OFF");
        }
        
      }  
    }
    notifyClients(dataIncubator(),0);
    t20 = t21;
  }

  
  delay(3);
    
  


  if ( dt1 > 15000 ){
    if (displayMode) displayMode = key = 0;
    t10 = t11;
  }

  if (dt > DELAY ) {
    T = dht.readTemperature() + To;
    H = dht.readHumidity() + Ho;
    
    if ( isnan(T) || isnan(H) ) {
      lcd.clear();
      lcd.print("SENSOR ERROR!");
      lcd.setCursor(0, 1);
      lcd.print(T);
      lcd.write(byte(0));
      lcd.print("C ");
      lcd.print(H);
      lcd.print("%");
      Serial.println("S Error");
      Serial.println(dt);
      Serial.println(key);
      t0 = t1;
      SenErr = 1;
      return;   // JIKA SENSOR DT TIDAK TERBACA
    } 
    
    Serial.print("Sensor : ");
    Serial.print(T);
    Serial.print("-");
    Serial.println(H);
    SenErr = 0;
    
    if ( T >= Ts ) {
      heaterStatus = 0;
      digitalWrite(HEATER_PIN, Off);
    }

    if ( T <= Tsm ) {
      heaterStatus = 1;
      digitalWrite(HEATER_PIN, On);
    }


    if ( H >= Hs ) {
      humidityStatus = 0;
      digitalWrite(HUMIDITY_PIN, Off);
      Serial.println("HUMIDITY OFF");
    }

    if ( H <= Hsm ) {
      humidityStatus = 1;
      Serial.println("HUMIDITY ON");
      digitalWrite(HUMIDITY_PIN, On);
    }
    
    float dts = dt * 1e-3;
     
    
    /*  LCD TAMPILAN AWAL BARIS 1*/
    lcd.clear();;
    lcd.print(T,1);
    lcd.write(byte(0));
    lcd.print("C ");
    lcd.print(H, 0);
    lcd.print("%");
    if(displayMode > 0 ){
      lcd.print("  P");
      lcd.print(displayMode);
    }else{      
      if( !timerIntervalSec & turnDurationStatus & turnDuration > 0 ){
//        lcd.print(" T : ");
        lcd.print(turnIntervalModeChange == 1 ? " RIGHT" : " LEFT");   
      }     
    }
    lcd.setCursor(0, 1);
    float uptime;
    char unit;
    lcd.print(Ts,1);
    lcd.write(byte(0));
    lcd.print("C ");
    lcd.print(Hs, 0);
    lcd.print("% ");
    if( !timerIntervalSec & turnDurationStatus & turnDuration > 0 ){
      DSLeft = timerDurationSec % 60;
      DMleft = ( timerDurationSec % 3600) / 60;
      if(DMleft < 10 ) lcd.print(0);
      lcd.print(DMleft);
      lcd.print(":");
      if(DSLeft < 10 ) lcd.print(0);
      lcd.print(DSLeft);
    }    

    key = 0;
    t0 = t1;
  }
}
