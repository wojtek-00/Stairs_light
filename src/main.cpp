#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <WebServer.h>


// ######### INTERNET ###############

const char* ssid = "DOM_ZZZR";          // Nazwa sieci Wi-Fi
const char* password = "eWF8qn8tnfgSga5Vkv7E";      // Hasło do sieci Wi-Fi

// Statyczny adres IP, maska podsieci, brama
IPAddress staticIP(192, 168, 8, 61);  // Adres IP
IPAddress gateway(192, 168, 8, 1);      // Brama (router)
IPAddress subnet(255, 255, 255, 0);     // Maska podsieci

WebServer server(80);

int pinsLight[4][2] = {        // Piny wyjściowe -> 5. White, 6. Yellow
        {1, 5},
        {2, 6},
        {3, 7},
        {4, 8}
};


Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40); // Adres domyślny 0x40
unsigned long checkLightReceiving = 5400000;
unsigned long oldLightReceiving = 0;



int values[2] = {0, 0};         // Aktualne wartości PWM
int newValues[2] = {0, 0};      // Docelowe wartości PWM


int commandNumber = 0;

double lightVal = 100.0;

// SENSORS
const int NUM_SENSORS = 4;              // Liczba czujników
const int PIR_PINS[NUM_SENSORS] = {16, 27, 26, 14}; // Piny, do których podłączone są czujniki PIR
int highCount[NUM_SENSORS] = {0};           // Liczniki dla każdego sensora
int threshold = 10;                          // Liczba wymaganych sygnałów HIGH
bool motionDetected = false;

bool toDimm = false;
bool turnOffMotion = false;
int LDRValue = 0;
unsigned long lastTriggerSignalTime = 0;

unsigned long delayDimmInterval = 20000; // Time after that the light will be dimmed
unsigned long delayOffInterval = 120000;  // Time after that the light will be turned off


// LIGHT SENSOR VALUES
int lvlYellowLight = 7;
int lvlWhiteLight = 30;

bool readPir = true;


// DAY MODE
bool dayModeFlag = false;
int dayLightValue = 30;
unsigned long tempOffTime = 0;

const int buttonPin = 25;      // Pin, do którego podłączony jest przycisk
int buttonState = 0;          // Zmienna przechowująca stan przycisku
int lastButtonState = 0;      // Ostatni stan przycisku
unsigned long lastDebounceTime = 0;  // Czas ostatniej zmiany stanu
unsigned long debounceDelay = 50;    // Czas debouncingu w milisekundach
unsigned long clickTimeout = 1000;   // Czas oczekiwania na zakończenie zliczania kliknięć (w milisekundach)
int clickCount = 0;           // Liczba kliknięć przycisku
unsigned long lastClickTime = 0;    // Czas ostatniego kliknięcia
bool isCounting = false;      // Flaga, która określa, czy liczymy kliknięcia

bool disableBySwitch = false;
bool isOnFlag = false;


void changeIntensity(int targetValues[], int delayTime = 10);

void selectCommand(int command);

void setup() {
  delay(5000);
  Serial.println("Setup: ");

  Wire.begin();
  pwm.begin();

  for (int cntPins = 0; cntPins < 4; cntPins++) {
    for (int i = 0; i < 2; i++) {  
      pwm.setPWM(pinsLight[cntPins][i], 0, 0);
    }
  }

  Serial.begin(115200);

  Serial.println("  Check LEDs");
  int setupValues[2] = {0, 0};
  // changeIntensity(setupValues);
  // delay(1000);
  // setupValues[0] = 255;
  // setupValues[1] = 0;
  // changeIntensity(setupValues);
  // delay(1000);
  // setupValues[0] = 0;
  // setupValues[1] = 255;
  // changeIntensity(setupValues);
  // delay(1000);
  // setupValues[0] = 0;
  // setupValues[1] = 0;
  changeIntensity(setupValues);

  pinMode(buttonPin, INPUT_PULLUP);

  Serial.println("  Configure PIR");
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(PIR_PINS[i], INPUT);
  }

  // INTERNET

  WiFi.config(staticIP, gateway, subnet);
   // Łączenie z Wi-Fi
  Serial.println("Łączenie z Wi-Fi...");
  WiFi.begin(ssid, password);

  // Czekanie na połączenie
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  // Wyświetlenie informacji o połączeniu
  Serial.println("");
  Serial.println("Połączono z Wi-Fi!");
  Serial.print("Adres IP: ");
  Serial.println(WiFi.localIP());
//_______________________________________

server.on("/data", HTTP_GET, [](){
  String lightStr = server.arg("light");  // Otrzymanie wartości jako String
  lightVal = lightStr.toDouble();            // Konwersja String na double i zapisanie w zmiennej globalnej

  oldLightReceiving = millis();
  // Debugowanie
  Serial.print("Dane odebrane: ");
  Serial.println(lightVal);

  String response = "Dane z parametru odebrane";
  server.send(200, "text/plain", response);
});

server.begin();


}

void loop() {
  server.handleClient();

  if (millis() - oldLightReceiving > checkLightReceiving) {
    lightVal = lvlYellowLight -1;
  }

  motionDetected = 0;
  int effectNumber = 0;
  
  if (Serial.available() > 0) {           // input in terminal
    commandNumber = Serial.parseInt();
    //Serial.println(commandNumber);
  }

  if (commandNumber != 0) {
    effectNumber = commandNumber;
  }

  //####################  BUTTON   ###########################
  // Odczyt stanu przycisku
  int reading = digitalRead(buttonPin);

  // Sprawdzanie, czy stan przycisku się zmienił
  if (reading != lastButtonState) {
    lastDebounceTime = millis();  // Zapisz czas zmiany stanu
  }

  // Jeśli upłynął wymagany czas debouncingu, zaktualizuj stan przycisku
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      // Sprawdzenie, czy przycisk jest wciśnięty
      if (buttonState == LOW) {
        // Jeżeli przycisk został naciśnięty, zliczamy kliknięcia
        if (!isCounting) {
          clickCount = 0;  // Resetowanie liczby kliknięć przed rozpoczęciem liczenia
          isCounting = true;  // Rozpoczynamy liczenie kliknięć
          lastClickTime = millis();  // Zapisz czas pierwszego kliknięcia
        }
        clickCount++;  // Zwiększamy liczbę kliknięć
        Serial.print("Liczba kliknięć: ");
        Serial.println(clickCount);
      }
    }
  }

  // Sprawdzanie, czy upłynął czas na zakończenie liczenia kliknięć
  if (isCounting && (millis() - lastClickTime) > clickTimeout) {
    // Po upływie czasu (np. 1 sekunda), interpretujemy liczbę kliknięć
    static int onValues[2] = {0, 0};
      switch (clickCount) {
        case 1:
          // turn on
          if (isOnFlag) {
            Serial.println("Turn Off");
            disableBySwitch = true;
            onValues[0] = 0;
            onValues[1] = 0;
            changeIntensity(onValues);
            isOnFlag = false;
          } else {
            Serial.println("Turn On");
            disableBySwitch = true;
            onValues[0] = 4096;
            onValues[1] = 0;
            changeIntensity(onValues);
            isOnFlag = true;
          }
          break;
        case 2:
          // default sensors
          Serial.println("Sensors");
          int onDelay = 200;
          delay(onDelay);
         /*  analogWrite(pins[0], 0);
          analogWrite(pins[1], 0);
          delay(onDelay);
          analogWrite(pins[0], 100);
          analogWrite(pins[1], 0);
          delay(onDelay);
          analogWrite(pins[0], 0);
          analogWrite(pins[1], 0);
          delay(onDelay);
          analogWrite(pins[0], 0);
          analogWrite(pins[1], 100);
          delay(onDelay);
          analogWrite(pins[0], 0);
          analogWrite(pins[1], 0);
          delay(onDelay);
          analogWrite(pins[0], 100);
          analogWrite(pins[1], 0);
          delay(onDelay);
          delay(onDelay);
          analogWrite(pins[0], 0);
          analogWrite(pins[1], 0); */
          onValues[0] = 0;
          onValues[1] = 0;
          changeIntensity(onValues);
          delay(onDelay);
          delay(100);

          disableBySwitch = false;
          turnOffMotion = false;
          toDimm = false;
          break;
      }
    // Resetowanie i przygotowanie do następnego cyklu
    isCounting = false;
    clickCount = 0;
  }

  // Zapisz aktualny stan przycisku
  lastButtonState = reading;


  //###############################################
  

  if (!disableBySwitch) {
    if (!turnOffMotion){

      LDRValue = lightVal;
  }

  if (LDRValue > dayLightValue){
    dayModeFlag = true;
    tempOffTime = delayDimmInterval;
  } else {
    dayModeFlag = false;
    tempOffTime = delayOffInterval;
  }


    //Serial.println(LDRValue);
    
    for (int i = 0; i < NUM_SENSORS; i++) {
      int pirState = digitalRead(PIR_PINS[i]); // Odczyt sygnału z danego czujnika PIR

      if (pirState == HIGH) {
        highCount[i]++; // Zwiększ licznik dla danego sensora
      } else {
        highCount[i] = 0; // Zresetuj licznik, jeśli sygnał nie jest HIGH
      }

      if (highCount[i] >= threshold) {
        Serial.print("Motion Detected on Sensor ");
        Serial.println(i + 1); // Wypisuje numer czujnika, na którym wykryto ruch
        motionDetected = true; // Ustawia flagę ruchu
        highCount[i] = 0;      // Opcjonalnie reset licznika po wykryciu
      }
  }
  

    if (motionDetected == HIGH) {
      lastTriggerSignalTime = millis(); 
    }

    if (!dayModeFlag) {
      if (LDRValue < lvlYellowLight && (!turnOffMotion || !toDimm)) {
        if (motionDetected == HIGH) {
          Serial.println("Yellow Light");
          effectNumber = 3;
          turnOffMotion = true;
          toDimm = true;
        }
      } else if (LDRValue < lvlWhiteLight && (!turnOffMotion || !toDimm)) {
        if (motionDetected == HIGH) {
          Serial.println("White Light");
          effectNumber = 2;
          turnOffMotion = true;
          toDimm = true;
        }
      }
    } else if (dayModeFlag) {
        if (motionDetected == HIGH) {
          Serial.println("Day Light");
          effectNumber = 5;
          turnOffMotion = true;
          toDimm = false;
        }
    }

    if (turnOffMotion && toDimm) {
      if (millis() - lastTriggerSignalTime >= delayDimmInterval) {
        Serial.println("Dimm");
        effectNumber = 4;
        //turnOffMotion = false;
        toDimm = false;
      }
    }

    if (turnOffMotion) {
      if (millis() - lastTriggerSignalTime >= tempOffTime) {
        Serial.println("Turn Off");
        effectNumber = 1;
        turnOffMotion = false;
        toDimm = false;
      }
    }

      //Serial.println(millis() - lastTriggerSignalTime);
      //printTime(millis() - lastTriggerSignalTime);
    
  } //turnOffMotion
  




  selectCommand(effectNumber);
}

void printTime(unsigned long time) {
  static unsigned long previousSecond = 0;
  unsigned long currentSecond = time / 1000; // Obliczenie liczby sekund

  // Wyświetlanie tylko, gdy zmienia się pełna sekunda
  if (currentSecond != previousSecond) {
    Serial.print("TIME:");
    Serial.println(currentSecond);
    previousSecond = currentSecond; // Aktualizacja ostatniej wyświetlonej sekundy
  }
}

void selectCommand(int command) {
  switch (command) {
    case 1:
      newValues[0] = 0; // Docelowe wartości
      newValues[1] = 0;
      changeIntensity(newValues);
      break;
    case 2:   // Yellow
      newValues[0] = 2000;  // White Light
      newValues[1] = 2500;  // Yellow Light
      changeIntensity(newValues);
      break;
    case 3:
      newValues[0] = 0; // Docelowe wartości
      newValues[1] = 1500;
      changeIntensity(newValues);
      break;
    case 4:
      newValues[0] = 0; // Docelowe wartości
      newValues[1] = 800;
      changeIntensity(newValues, 20);
      break;
    case 5:
      newValues[0] = 700; // Docelowe wartości
      newValues[1] = 0;
      changeIntensity(newValues, 20);
      break;
  }
}



void changeIntensity(int targetValues[], int delayTime) {
  int tempValues[2];          // Tymczasowe wartości PWM
  for (int i = 0; i < 2; i++) {
    tempValues[i] = values[i];
  }

  bool readyFlag[2] = {false, false};  // Flagi stanu dla każdego kanału

  while (!(readyFlag[0] && readyFlag[1])) { // Dopóki oba kanały nie osiągną celu
    for (int i = 0; i < 2; i++) {
      if (!readyFlag[i]) {  // Działaj tylko na kanale, który nie jest gotowy
        if (tempValues[i] < targetValues[i]) {
          tempValues[i]++;
        } else if (tempValues[i] > targetValues[i]) {
          tempValues[i]--;
        } else {
          readyFlag[i] = true; // Osiągnięto wartość docelową
        }

        for (int cntPins = 0; cntPins < 4; cntPins++) {
          pwm.setPWM(pinsLight[cntPins][i], 0, tempValues[i]);
        }
      }
    }
    delay(delayTime); // Małe opóźnienie dla płynniejszej zmiany
  }

  // Aktualizacja globalnych wartości
  for (int i = 0; i < 2; i++) {
    values[i] = tempValues[i];
  }
}
