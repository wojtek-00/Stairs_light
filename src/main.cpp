#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <WebServer.h>
#include <WiFi.h>

// OTA
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// TELNET
#include <WiFiClient.h>

// ######### INTERNET ###############
const char* ssid = "DOM_ZR_IoT";          
const char* password = "OC4C7yLP3cRRf4S";      

IPAddress staticIP(192, 168, 8, 61);  
IPAddress gateway(192, 168, 8, 1);      
IPAddress subnet(255, 255, 255, 0);     

WebServer server(80);

// Telnet
WiFiServer telnetServer(23);
WiFiClient telnetClient;
void logPrintln(const String &msg) {
  Serial.println(msg);
  if (telnetClient && telnetClient.connected()) {
    telnetClient.println(msg);
  }
}

// ---------- Twoje zmienne ----------
int pinsLight[4][2] = { {0, 1}, {2, 3}, {4, 5}, {6, 7} };
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

unsigned long checkLightReceiving = 5400000;
unsigned long oldLightReceiving = 0;

int values[2] = {0, 0};         
int newValues[2] = {0, 0};      
int commandNumber = 0;
double lightVal = 0.0;

const int NUM_SENSORS = 4;              
const int PIR_PINS[NUM_SENSORS] = {16, 27, 26, 14}; 
int highCount[NUM_SENSORS] = {0};           
int threshold = 10;                          
bool motionDetected = false;

bool toDimm = false;
bool turnOffMotion = false;
int LDRValue = 0;
unsigned long lastTriggerSignalTime = 0;

unsigned long delayDimmInterval = 20000; 
unsigned long delayOffInterval = 60000;  

int lvlYellowLight = 7;
int lvlWhiteLight = 30;
bool downStairsMode = false;
bool readPir = true;

bool dayModeFlag = false;
int dayLightValue = 30;
unsigned long tempOffTime = 0;

const int buttonPin = 25;      
int buttonState = 0;          
int lastButtonState = 0;      
unsigned long lastDebounceTime = 0;  
unsigned long debounceDelay = 50;    
unsigned long clickTimeout = 1000;   
int clickCount = 0;           
unsigned long lastClickTime = 0;    
bool isCounting = false;      

bool disableBySwitch = false;
bool isOnFlag = false;
bool noFirstReading = true;

const int relayPin = 32;

void changeIntensity(int targetValues[], int delayTime = 0, bool dayMode = false);
void selectCommand(int command);

void setup() {
  delay(3000);
  Serial.begin(115200);
  logPrintln("Setup: ");
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  Wire.begin();
  pwm.begin();
  
  for (int cntPins = 0; cntPins < 4; cntPins++) {
    for (int i = 0; i < 2; i++) {  
      pwm.setPWM(pinsLight[cntPins][i], 0, 0);
    }
  }
  logPrintln("  Set PWM Pins");
  digitalWrite(relayPin, HIGH);

  logPrintln("  Check LEDs");
  int setupValues[2] = {0, 0};
  changeIntensity(setupValues);

  pinMode(buttonPin, INPUT_PULLUP);

  logPrintln("  Configure PIR");
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(PIR_PINS[i], INPUT);
  }

  // ----------- WiFi -----------
  WiFi.config(staticIP, gateway, subnet);
  logPrintln("Łączenie z Wi-Fi...");
  WiFi.begin(ssid, password);

  logPrintln("");
  logPrintln("Połączono z Wi-Fi!");
  logPrintln("Adres IP: " + WiFi.localIP().toString());

  // ----------- OTA -----------
  ArduinoOTA.setHostname("StairsLight");
  ArduinoOTA
    .onStart([]() { logPrintln("OTA Start"); })
    .onEnd([]() { logPrintln("OTA End"); })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]\n", error);
    });
  ArduinoOTA.begin();
  logPrintln("OTA Ready");

  // ----------- WebServer -----------
  server.on("/data", HTTP_GET, []() {
    String lightStr = server.arg("light");
    lightVal = lightStr.toDouble();
    oldLightReceiving = millis();
    logPrintln("Dane odebrane: " + String(lightVal));
    server.send(200, "text/plain", "Dane z parametru odebrane");
    if (noFirstReading) noFirstReading = false;
  });
  server.begin();

  // ----------- Telnet -----------
  telnetServer.begin();
  telnetServer.setNoDelay(true);
  logPrintln("Telnet server started");

  logPrintln("Setup done");
}

void loop() {
  downStairsMode = false;
  server.handleClient();
  ArduinoOTA.handle();

  // Obsługa Telnet
  if (telnetServer.hasClient()) {
    if (!telnetClient || !telnetClient.connected()) {
      if (telnetClient) telnetClient.stop();
      telnetClient = telnetServer.available();
      logPrintln("Nowy klient telnet.");
    } else {
      WiFiClient extra = telnetServer.available();
      extra.stop();
    }
  }

  if (millis() - oldLightReceiving > checkLightReceiving) {
    lightVal = lvlYellowLight - 1;
  }

  motionDetected = 0;
  int effectNumber = 0;
  
  if (Serial.available() > 0) {           
    commandNumber = Serial.parseInt();
  }

  if (commandNumber != 0) {
    effectNumber = commandNumber;
  }

  //####################  BUTTON   ###########################
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();  
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        if (!isCounting) {
          clickCount = 0;  
          isCounting = true;  
          lastClickTime = millis();  
        }
        clickCount++;  
        logPrintln("Liczba kliknięć: " + String(clickCount));
      }
    }
  }
  if (isCounting && (millis() - lastClickTime) > clickTimeout) {
    static int onValues[2] = {0, 0};
    switch (clickCount) {
      case 1:
        if (isOnFlag) {
          logPrintln("Turn Off");
          disableBySwitch = true;
          onValues[0] = 0; onValues[1] = 0;
          changeIntensity(onValues);
          isOnFlag = false;
        } else {
          logPrintln("Turn On");
          disableBySwitch = true;
          onValues[0] = 0; onValues[1] = 1200;
          changeIntensity(onValues);
          isOnFlag = true;
        }
        break;
      case 2:
        logPrintln("Sensors");
        int onDelay = 200;
        delay(onDelay);

        pwm.setPWM(0, 0, 1000); delay(300);
        pwm.setPWM(0, 0, 0); pwm.setPWM(1, 0, 0); delay(300);
        pwm.setPWM(0, 0, 0); pwm.setPWM(1, 0, 1000); delay(300);
        pwm.setPWM(0, 0, 0); pwm.setPWM(1, 0, 0); delay(300);
        pwm.setPWM(0, 0, 1000); pwm.setPWM(1, 0, 0); delay(300);
        pwm.setPWM(0, 0, 0); pwm.setPWM(1, 0, 0);

        values[0] = 0; values[1] = 0;
        delay(onDelay);
        disableBySwitch = false;
        turnOffMotion = false;
        toDimm = false;
        break;
    }
    isCounting = false;
    clickCount = 0;
  }
  lastButtonState = reading;

  //###############################################
  if (!disableBySwitch) {
    LDRValue = lightVal;
    if (LDRValue > dayLightValue){
      dayModeFlag = true; tempOffTime = delayDimmInterval;
    } else {
      dayModeFlag = false; tempOffTime = delayOffInterval;
    }
    if (noFirstReading) {
      LDRValue = lvlYellowLight - 1;
    }
    for (int i = 0; i < NUM_SENSORS; i++) {
      int pirState = digitalRead(PIR_PINS[i]); 
      if (pirState == HIGH) {
        highCount[i]++; 
      } else {
        highCount[i] = 0; 
      }
      if (highCount[i] >= threshold) {
        logPrintln("Motion Detected on Sensor " + String(i+1));
        motionDetected = true; 
        highCount[i] = 0;      
      }
    }
    if (motionDetected == HIGH) {
      lastTriggerSignalTime = millis(); 
    }
    if (!dayModeFlag) {
      if (LDRValue < lvlYellowLight && (!turnOffMotion || !toDimm)) {
        if (motionDetected == HIGH) {
          logPrintln("Yellow Light");
          effectNumber = 3; turnOffMotion = true; toDimm = true;
        }
      } else if (LDRValue < lvlWhiteLight && (!turnOffMotion || !toDimm)) {
        if (motionDetected == HIGH) {
          logPrintln("White Light");
          effectNumber = 2; turnOffMotion = true; toDimm = true;
        }
      }
    } else if (dayModeFlag) {
      if (motionDetected == HIGH) {
        logPrintln("Day Light");
        effectNumber = 5; turnOffMotion = true; toDimm = false;
      }
    }
    if (turnOffMotion && toDimm) {
      if (millis() - lastTriggerSignalTime >= delayDimmInterval) {
        logPrintln("Dimm");
        effectNumber = 4; toDimm = false;
      }
    }
    if (turnOffMotion) {
      if (millis() - lastTriggerSignalTime >= tempOffTime) {
        logPrintln("Turn Off");
        effectNumber = 1; turnOffMotion = false; toDimm = false;
      }
    }
  } 
  selectCommand(effectNumber);
}

void selectCommand(int command) {
  switch (command) {
    case 1:
      newValues[0] = 0; newValues[1] = 0;
      changeIntensity(newValues); break;
    case 2:   
      newValues[0] = 500; newValues[1] = 600;
      changeIntensity(newValues); break;
    case 3:
      newValues[0] = 0; newValues[1] = 900;
      changeIntensity(newValues); break;
    case 4:
      newValues[0] = 0; newValues[1] = 400;
      changeIntensity(newValues); break;
    case 5:
      downStairsMode = true;
      newValues[0] = 0; newValues[1] = 300;
      changeIntensity(newValues, 0, downStairsMode); break;
  }
}

void changeIntensity(int targetValues[], int delayTime, bool dayMode) {
  int tempValues[2];          
  for (int i = 0; i < 2; i++) tempValues[i] = values[i];
  bool readyFlag[2] = {false, false};  

  while (!(readyFlag[0] && readyFlag[1])) {
    for (int i = 0; i < 2; i++) {
      if (!readyFlag[i]) {
        if (tempValues[i] < targetValues[i]) tempValues[i] += 10;
        else if (tempValues[i] > targetValues[i]) tempValues[i] -= 10;
        else readyFlag[i] = true;
      }
      if (!dayMode){
        for (int cntPins = 0; cntPins < 4; cntPins++) {
          pwm.setPWM(pinsLight[cntPins][i], 0, tempValues[i]);
        }
      } else {
        for (int cntPins = 0; cntPins < 2; cntPins++) {
          pwm.setPWM(pinsLight[cntPins][i], 0, tempValues[i]);
        }
        for (int cntPins = 2; cntPins < 4; cntPins++) {
          pwm.setPWM(pinsLight[cntPins][i], 0, 0);
        }
      }
    }
    delay(delayTime);
  }
  for (int i = 0; i < 2; i++) values[i] = tempValues[i];
}
