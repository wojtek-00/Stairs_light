#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

// Utwórz obiekt czujnika TSL2561
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);


int pins[2] = {5, 6};           // Piny wyjściowe
int values[2] = {0, 0};         // Aktualne wartości PWM
int newValues[2] = {0, 0};      // Docelowe wartości PWM


int commandNumber = 0;

bool readingSensor = true;


// SENSORS
int PIR_PIN_BASEMENT = 12;
int PIR_PIN_PORCH = 13;
int PIR_PIN_1FLOOR = 13;
int PIR_PIN_2FLOOR = 13;

bool toDimm = false;
bool turnOffMotion = false;
int motionDetected = 0;
int LDRValue = 0;
unsigned long lastTriggerSignalTime = 0;

unsigned long delayInterval = 3000;
unsigned long delayOffInterval = 5000;
unsigned long delayDimmInterval = 5000;


// LIGHT SENSOR VALUES
int lvlYellowLight = 10;
int lvlWhiteLight = 100;



void setup() {
  pinMode(pins[0], OUTPUT);
  pinMode(pins[1], OUTPUT);
  Serial.begin(9600);
  Serial.println("SET UP");


  // LIGHT SENSOR
  //Wire.setClock(50000); // Ustaw prędkość na 50 kHz

  Wire.begin(); // Inicjalizacja I2C bez określania pinów (SDA = A4, SCL = A5 dla Arduino Uno)

  // Inicjalizacja czujnika
  if (!tsl.begin()) {
    Serial.println("Nie można znaleźć czujnika TSL2561. Sprawdź połączenia!");
    while (1); // Zatrzymanie programu, jeśli czujnik nie jest znaleziony
  }
  
  Serial.println("Czujnik TSL2561 znaleziony!");

  // Opcjonalne ustawienia czujnika
  tsl.enableAutoRange(true);            // Automatyczne dostosowanie zakresu
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS); // Krótki czas integracji
  
  Serial.println("Czujnik gotowy do pracy!");
}

void loop() {
  Serial.println("Loop Function");
  int effectNumber = 0;
  if (Serial.available() > 0) {           // input in terminal
    commandNumber = Serial.parseInt();
    //Serial.println(commandNumber);
  }

  if (commandNumber != 0) {
    effectNumber = commandNumber;
  }
  //Serial.println(effectNumber);
  

  if (!turnOffMotion) {
    if (readingSensor) {
      sensors_event_t event;
      tsl.getEvent(&event);
      
      if (event.light) {
        LDRValue = event.light;
        //Serial.println(LDRValue);
      } else {
        // Jeśli wartość jest 0, być może światło jest poza zakresem czujnika
        LDRValue = 0;
      }
  }

    Serial.println(LDRValue);
    motionDetected = digitalRead(PIR_PIN_BASEMENT) || digitalRead(PIR_PIN_PORCH) || digitalRead(PIR_PIN_1FLOOR) || digitalRead(PIR_PIN_2FLOOR);
    //Serial.println(motionDetected);

    Serial.println(LDRValue);

    if (motionDetected == HIGH) {
      lastTriggerSignalTime = millis(); 
    }

    if (LDRValue < lvlYellowLight && !turnOffMotion) {
      if (motionDetected == HIGH) {
        Serial.println("Yellow Light");
        effectNumber = 2;
        turnOffMotion = true;
        toDimm = true;
      }
    } else if (LDRValue < lvlWhiteLight && !turnOffMotion) {
      if (motionDetected == HIGH) {
        Serial.println("White Light");
        effectNumber = 3;
        turnOffMotion = true;
        toDimm = true;
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
      if (millis() - lastTriggerSignalTime >= delayOffInterval) {
        Serial.println("Turn Off");
        effectNumber = 1;
        turnOffMotion = false;
        toDimm = false;
      }
    }

    //Serial.println(millis() - lastTriggerSignalTime);
    
  }
  




  selectCommand(effectNumber);
}

void selectCommand(int command) {
  switch (command) {
    case 1:
      newValues[0] = 0; // Docelowe wartości
      newValues[1] = 0;
      changeIntensity(newValues);
      break;
    case 2:   // Yellow
      newValues[0] = 50;  // White Light
      newValues[1] = 130; // Yellow Light
      changeIntensity(newValues);
      break;
    case 3:
      newValues[0] = 255; // Docelowe wartości
      newValues[1] = 255;
      changeIntensity(newValues);
      break;
    case 4:
      newValues[0] = 0; // Docelowe wartości
      newValues[1] = 100;
      changeIntensity(newValues);
  }
}



void changeIntensity(int targetValues[]) {
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
    delay(10); // Małe opóźnienie dla płynniejszej zmiany
  }

  // Aktualizacja globalnych wartości
  for (int i = 0; i < 2; i++) {
    values[i] = tempValues[i];
  }
}
