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
int PIR_PIN = 12;
bool turnOffMotion = false;
int motionDetected = 0;
int LDRValue = 0;
int lastTriggerSignalTime = 0;
int delayInterval = 3000;
int delayOffInterval = 5000;

// LIGHT VALUES
int lvlYellowLight = 10;
int lvlWhiteLight = 100;



void setup() {
  pinMode(pins[0], OUTPUT);
  pinMode(pins[1], OUTPUT);
  Serial.begin(9600);


  // LIGHT SENSOR
  Wire.setClock(50000); // Ustaw prędkość na 50 kHz

  Serial.begin(9600);
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
  

  if (readingSensor) {
    sensors_event_t event;
    tsl.getEvent(&event);
    
    if (event.light) {
      // Wyświetl odczytane natężenie światła w luksach
      // Serial.print("Natężenie światła: ");
      // Serial.print(event.light);
      // Serial.println(" lux");
      LDRValue = event.light;
    } else {
      // Jeśli wartość jest 0, być może światło jest poza zakresem czujnika
      Serial.println("Światło poza zakresem czujnika!");
    }

    motionDetected = digitalRead(PIR_PIN);  // Read the PIR sensor
    //motionDetected = 0; //  <- PIR broken now, we dont use
    //Serial.println(motionDetected);

    //Serial.println(LDRValue);
    

    if (millis() - lastTriggerSignalTime >= delayInterval) {

      if (LDRValue < lvlYellowLight) {
        if (motionDetected == HIGH) {
          Serial.println("Yellow Light");
          effectNumber = 2;
          turnOffMotion = true;
          lastTriggerSignalTime = millis();
        }
      } else if (LDRValue < lvlWhiteLight) {
        if (motionDetected == HIGH) {
          Serial.println("White Light");
          effectNumber = 3;
          turnOffMotion = true;
          lastTriggerSignalTime = millis();
        }
      }
      if (turnOffMotion == true) {
        if (millis() - lastTriggerSignalTime >= delayOffInterval) {
          Serial.println("Turn Off");
          effectNumber = 1;
          turnOffMotion = false;
        }
      }
    }
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
    case 2:
      newValues[0] = 255; // Docelowe wartości
      newValues[1] = 255;
      changeIntensity(newValues);
      break;
    case 3:
      newValues[0] = 50; // Docelowe wartości
      newValues[1] = 130;
      changeIntensity(newValues);
      break;
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
    delay(5); // Małe opóźnienie dla płynniejszej zmiany
  }

  // Aktualizacja globalnych wartości
  for (int i = 0; i < 2; i++) {
    values[i] = tempValues[i];
  }
}
