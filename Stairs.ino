#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>


Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

int pins[2] = {5, 6};           // Piny wyjściowe -> 5. White, 6. Yellow
int values[2] = {0, 0};         // Aktualne wartości PWM
int newValues[2] = {0, 0};      // Docelowe wartości PWM


int commandNumber = 0;

bool readingSensor = true;


// SENSORS
const int NUM_SENSORS = 4;              // Liczba czujników
const int PIR_PINS[NUM_SENSORS] = {9, 10, 11, 12}; // Piny, do których podłączone są czujniki PIR
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


void changeIntensity(int targetValues[], int delayTime = 10);

void setup() {
  Serial.println("Setup: ");
  pinMode(pins[0], OUTPUT);
  pinMode(pins[1], OUTPUT);
  Serial.begin(9600);

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


  Serial.println("  Configure PIR");
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(PIR_PINS[i], INPUT);
  }

  Serial.println("  Configure TSL");
  // Automatyczna kontrola wzmocnienia
  tsl.enableAutoRange(false);
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
  Serial.println("    Sensor configured");

  Serial.println("Setup done!");
  delay(2000);
  Serial.flush();
}

void loop() {
  motionDetected = 0;
  int effectNumber = 0;
  
  if (Serial.available() > 0) {           // input in terminal
    commandNumber = Serial.parseInt();
    //Serial.println(commandNumber);
  }

  if (commandNumber != 0) {
    effectNumber = commandNumber;
  }
  

  if (true) {
    if (!turnOffMotion){
      if (readingSensor) {
        sensors_event_t event;
        tsl.getEvent(&event);
        
        if (event.light) {
          LDRValue = event.light;
        } else {
          // Jeśli wartość jest 0, być może światło jest poza zakresem czujnika
          LDRValue = 0;
        }
      }
  }

  if (LDRValue > dayLightValue){
    dayModeFlag = true;
    tempOffTime = delayOffInterval;
  } else {
    dayModeFlag = false;
    tempOffTime = delayDimmInterval;
  }


    Serial.println(LDRValue);
    
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

      Serial.println(millis() - lastTriggerSignalTime);
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
      newValues[0] = 15;  // White Light
      newValues[1] = 25;  // Yellow Light
      changeIntensity(newValues);
      break;
    case 3:
      newValues[0] = 0; // Docelowe wartości
      newValues[1] = 30;
      changeIntensity(newValues);
      break;
    case 4:
      newValues[0] = 0; // Docelowe wartości
      newValues[1] = 5;
      changeIntensity(newValues, 20);
      break;
    case 5:
      newValues[0] = 10; // Docelowe wartości
      newValues[1] = 0;
      changeIntensity(newValues, 20);
      break;
  }
}



void changeIntensity(int targetValues[], int delayTime = 10) {
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
        analogWrite(pins[i], tempValues[i]); // Ustawienie nowej wartości PWM
      }
    }
    delay(delayTime); // Małe opóźnienie dla płynniejszej zmiany
  }

  // Aktualizacja globalnych wartości
  for (int i = 0; i < 2; i++) {
    values[i] = tempValues[i];
  }
}
