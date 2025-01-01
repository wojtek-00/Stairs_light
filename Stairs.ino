#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

// Utwórz obiekt czujnika TSL2561
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);


int pins[2] = {5, 6};           // Piny wyjściowe -> 5. White, 6. Yellow
int values[2] = {0, 0};         // Aktualne wartości PWM
int newValues[2] = {0, 0};      // Docelowe wartości PWM


int commandNumber = 0;

bool readingSensor = true;


// SENSORS
int PIR_PIN_BASEMENT = 9;
int PIR_PIN_PORCH = 10;
int PIR_PIN_1FLOOR = 11;
int PIR_PIN_2FLOOR = 12;

bool toDimm = false;
bool turnOffMotion = false;
int motionDetected = 0;
int LDRValue = 0;
unsigned long lastTriggerSignalTime = 0;

unsigned long delayDimmInterval = 180000; // Time after that the light will be dimmed
unsigned long delayOffInterval = 900000;  // Time after that the light will be turned off


// LIGHT SENSOR VALUES
int lvlYellowLight = 4;
int lvlWhiteLight = 40;



void setup() {
  pinMode(pins[0], OUTPUT);
  pinMode(pins[1], OUTPUT);
  Serial.begin(9600);
  Serial.println("SET UP");

  pinMode(PIR_PIN_BASEMENT, INPUT);
  pinMode(PIR_PIN_PORCH, INPUT);
  pinMode(PIR_PIN_1FLOOR, INPUT);
  pinMode(PIR_PIN_2FLOOR, INPUT);


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
  int effectNumber = 0;
  
  if (Serial.available() > 0) {           // input in terminal
    commandNumber = Serial.parseInt();
    //Serial.println(commandNumber);
  }

  if (commandNumber != 0) {
    effectNumber = commandNumber;
  }
  //Serial.println(effectNumber);
  

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

    //Serial.println(LDRValue);
    motionDetected = /* digitalRead(PIR_PIN_BASEMENT) || */ digitalRead(PIR_PIN_PORCH) || digitalRead(PIR_PIN_1FLOOR) || digitalRead(PIR_PIN_2FLOOR);
    //Serial.println(motionDetected);

    //Serial.println(LDRValue);

    if (motionDetected == HIGH) {
      lastTriggerSignalTime = millis(); 
    }

    if (LDRValue < lvlYellowLight && (!turnOffMotion || !toDimm)) {
      if (motionDetected == HIGH) {
        Serial.println("Yellow Light");
        effectNumber = 2;
        turnOffMotion = true;
        toDimm = true;
      }
    } else if (LDRValue < lvlWhiteLight && (!turnOffMotion || !toDimm)) {
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

      Serial.println(millis() - lastTriggerSignalTime);
    
  } //turnOffMotion
  




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
      newValues[0] = 20;  // White Light
      newValues[1] = 50;  // Yellow Light
      changeIntensity(newValues);
      break;
    case 3:
      newValues[0] = 180; // Docelowe wartości
      newValues[1] = 0;
      changeIntensity(newValues);
      break;
    case 4:
      newValues[0] = 0; // Docelowe wartości
      newValues[1] = 10;
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
