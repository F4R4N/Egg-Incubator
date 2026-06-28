#include <LiquidCrystal.h>
#include <EEPROM.h>

#define keys A0
#define currentHumidityPin A1
#define currentTempPin A2
#define potentiometerPin A3
#define openLidPin A4
#define closeLidPin A5

#define heaterPin 2
#define radiatorElectricValvePin 3
#define humidityWaterPin 10
#define powerPin 11
#define humidityWindPin 12
#define lightOut 13


#define D4 4
#define D5 5
#define D6 6
#define D7 7
#define RS 8
#define EN 9

LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);
byte preferedTemperature;
byte preferedHumidity;
byte preferedCO2;

bool lightIsOn = false;
unsigned long lightStartTime = 0;

enum MenuScreen {
  MENU_NONE,
  MENU_MAIN,
  MENU_EDIT
};

MenuScreen menuScreen = MENU_NONE;
byte menuCursor = 0;
bool menuNeedsRedraw = true;
byte *editTarget = NULL;
byte editOption = 0;
byte editValue = 0;
unsigned long keyStableSince = 0;
byte keyPending = 0;
bool waitingForKeyRelease = false;
// temperature state
unsigned long radiatorWindowStart = 0;
unsigned long heaterWindowStart = 0;
// humidity state
unsigned long humidityStateStart = 0;
byte humidityState = 0;
// CO2
long currentCO2 = 0;

void setup() {
  pinMode(powerPin, INPUT_PULLUP);
  pinMode(keys, INPUT_PULLUP);
  pinMode(lightOut, OUTPUT);
  pinMode(heaterPin, OUTPUT);
  pinMode(radiatorElectricValvePin, OUTPUT);
  pinMode(humidityWaterPin, OUTPUT);
  pinMode(humidityWindPin, OUTPUT);
  pinMode(openLidPin, OUTPUT);
  pinMode(closeLidPin, OUTPUT);

  Serial.begin(9600);
  lcd.begin(20, 4);
  Serial.println("Enter CO2: ");

  preferedTemperature = EEPROM.read(0);
  if (preferedTemperature == 255) {
    preferedTemperature = 25;
  }
  preferedHumidity = EEPROM.read(1);
  if (preferedHumidity == 255) {
    preferedHumidity = 25;
  }
  preferedCO2 = EEPROM.read(2);
  if (preferedCO2 == 255) {
    preferedCO2 = 50;
  }
}

void loop() {
  bool state = digitalRead(powerPin) == LOW;
  byte key = getNonBlockingKey();
  controlLight(key);
  if (state) {  // is on
    readCO2();
    controlHeater();
    controlRadiatorValve();
    controlHumidity();
    controlCO2();
    updateMenu(state, key);
  } else {  // is off
    digitalWrite(heaterPin, LOW);
    digitalWrite(radiatorElectricValvePin, LOW);
    digitalWrite(humidityWaterPin, LOW);
    digitalWrite(humidityWindPin, LOW);
    digitalWrite(openLidPin, LOW);
    digitalWrite(closeLidPin, LOW);

    if (menuScreen != MENU_NONE) {
      menuScreen = MENU_NONE;
      lcd.clear();
    }
    mainDisplay(state);
  }
}

void readCO2() {
  static String input = "";

  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (input.length() > 0) {
        int value = input.toInt();
        if (value >= 0 && value <= 100) {
          currentCO2 = value;

          Serial.print("CO2 = ");
          Serial.println(currentCO2);
        } else {
          Serial.println("Error: CO2 level must be between 0 and 100.");
        }
        input = "";
      }
    } else if (isDigit(c)) {
      input += c;
    }
  }
}

unsigned long CO2LastCheckTime = 0;
int preferedLidOpenness = 0;

int getPreferedLidOpenness() {
  return (int)((currentCO2 - preferedCO2) / 2) + 50;
}

void controlCO2() {
  if (millis() - CO2LastCheckTime >= 20000) {
    CO2LastCheckTime += 20000;
    preferedLidOpenness = getPreferedLidOpenness();

  } else {
    int currentLidOpenness = getCurrentLidOpenness();

    if (abs(currentLidOpenness - preferedLidOpenness) <= 5) {
      digitalWrite(openLidPin, LOW);
      digitalWrite(closeLidPin, LOW);
    } else if (currentLidOpenness < preferedLidOpenness) {
      // open command
      digitalWrite(openLidPin, HIGH);
      digitalWrite(closeLidPin, LOW);
    } else {
      // close command
      digitalWrite(openLidPin, LOW);
      digitalWrite(closeLidPin, HIGH);
    }
  }
}

void controlHeater() {
  long currentTemp = getCurrentHumidityTemp(currentTempPin);
  long dutyCycle = (preferedTemperature - currentTemp) * 40;
  dutyCycle = constrain(dutyCycle, 0, 100);

  unsigned long onTime = (10000 * dutyCycle) / 100;

  // Start a new window every 10 sec
  if (millis() - heaterWindowStart >= 10000) {
    heaterWindowStart += 10000;
  }

  if (millis() - heaterWindowStart < onTime) {
    digitalWrite(heaterPin, HIGH);
  } else {
    digitalWrite(heaterPin, LOW);
  }
}

void controlRadiatorValve() {
  long currentTemp = getCurrentHumidityTemp(currentTempPin);
  long dutyCycle = (currentTemp - preferedTemperature) * 20;
  dutyCycle = constrain(dutyCycle, 0, 100);

  unsigned long onTime = (10000 * dutyCycle) / 100;

  // Start a new window every 10 sec
  if (millis() - radiatorWindowStart >= 10000) {
    radiatorWindowStart += 10000;
  }

  if (millis() - radiatorWindowStart < onTime) {
    digitalWrite(radiatorElectricValvePin, HIGH);
  } else {
    digitalWrite(radiatorElectricValvePin, LOW);
  }
}

// States:
// 0 = check humidity
// 1 = water ON + wind ON 5 sec
// 2 = water OFF + wind ON 2 sec
// 3 = water OFF + wind OFF 1 sec
void controlHumidity() {
  long currentHumidity = getCurrentHumidityTemp(currentHumidityPin);

  switch (humidityState) {
    case 0:
      if (currentHumidity < preferedHumidity) {
        digitalWrite(humidityWindPin, HIGH);
        digitalWrite(humidityWaterPin, HIGH);

        humidityState = 1;
        humidityStateStart = millis();
      } else {
        digitalWrite(humidityWindPin, LOW);
        digitalWrite(humidityWaterPin, LOW);
      }
      break;

    case 1:
      if (millis() - humidityStateStart >= 5000) {
        digitalWrite(humidityWaterPin, LOW);

        humidityState = 2;
        humidityStateStart = millis();
      }
      break;

    case 2:
      if (millis() - humidityStateStart >= 2000) {
        digitalWrite(humidityWindPin, LOW);

        humidityState = 3;
        humidityStateStart = millis();
      }
      break;

    case 3:
      if (millis() - humidityStateStart >= 1000) {
        currentHumidity = getCurrentHumidityTemp(currentHumidityPin);

        if (currentHumidity < preferedHumidity) {
          digitalWrite(humidityWindPin, HIGH);
          digitalWrite(humidityWaterPin, HIGH);

          humidityState = 1;
          humidityStateStart = millis();
        } else {
          digitalWrite(humidityWindPin, LOW);
          digitalWrite(humidityWaterPin, LOW);
          humidityState = 0;
        }
      }
      break;
  }
}

void controlLight(byte key) {
  if (key == 4) {
    digitalWrite(lightOut, HIGH);
    lightIsOn = true;
    lightStartTime = millis();
  }
  if (lightIsOn && (millis() - lightStartTime >= 12000)) {
    digitalWrite(lightOut, LOW);
    lightIsOn = false;
  }
}

char getKeyState() {
  int a = analogRead(keys);
  if (a < 100)
    return 'u';  // UP

  if (a < 250)
    return 'd';  // DOWN

  if (a < 400)
    return 'o';  // OK

  if (a > 500 && a < 650)
    return 'l';  // LIGHT

  return 0;  // NONE
}

byte getNonBlockingKey() {
  char key = getKeyState();
  byte button = 0;

  if (key == 'u') button = 1;
  else if (key == 'd') button = 2;
  else if (key == 'o') button = 3;
  else if (key == 'l') button = 4;

  if (waitingForKeyRelease) {
    if (button == 0) waitingForKeyRelease = false;
    return 0;
  }

  if (button == 0) {
    keyPending = 0;
    return 0;
  }

  if (button != keyPending) {
    keyPending = button;
    keyStableSince = millis();
    return 0;
  }

  if (millis() - keyStableSince < 20) return 0;

  waitingForKeyRelease = true;
  return button;
}

void mainDisplay(bool state) {
  // temperature
  lcd.setCursor(0, 0);
  lcd.print("Temperature: ");
  lcd.print(preferedTemperature);
  lcd.print("/");
  if (state) {
    lcd.print(getCurrentHumidityTemp(currentTempPin));
  }
  // humidity
  lcd.setCursor(0, 1);
  lcd.print("Humidity: ");
  lcd.print(preferedHumidity);
  lcd.print("/");
  if (state) {
    lcd.print(getCurrentHumidityTemp(currentHumidityPin));
  }
  // CO2
  lcd.setCursor(0, 2);
  lcd.print("CO2: ");
  lcd.print(preferedCO2);
  lcd.print("/");
  if (state) {
    lcd.print(currentCO2);
  }
  // state
  lcd.setCursor(0, 3);
  lcd.print("Current State: ");
  if (digitalRead(powerPin) == LOW) {
    lcd.print(" ON ");
  } else {
    lcd.print("OFF");
  }
}

long getCurrentHumidityTemp(int pin) {
  return (long)analogRead(pin) * 500 / 1024;
}

long getCurrentLidOpenness() {
  return (long)analogRead(potentiometerPin) * (100.0 / 1023.0);
}

void showMainMenu() {

  lcd.setCursor(2, 0);
  lcd.print("Set Temperature");

  lcd.setCursor(2, 1);
  lcd.print("Set Humidity");

  lcd.setCursor(2, 2);
  lcd.print("CO2 Menu");

  lcd.setCursor(2, 3);
  lcd.print("Exit");
}

void drawEditScreen() {
  lcd.clear();
  lcd.setCursor(17, 0);
  lcd.print("^");
  lcd.setCursor(17, 2);
  lcd.print("v");
  lcd.setCursor(0, 1);
  switch (editOption) {
    case 0:
      lcd.print("Pref Temp: ");
      break;
    case 1:
      lcd.print("Pref Humidity: ");
      break;
    case 2:
      lcd.print("Pref CO2: ");
      break;
  }
  lcd.setCursor(17, 1);
  lcd.print(editValue);
  if (editOption == 2) {
    lcd.setCursor(0, 2);
    lcd.print("CO2: ");
    lcd.print(preferedCO2);
    lcd.print("/");
    lcd.print(currentCO2);
    lcd.setCursor(0, 3);
    lcd.print("LID: ");
    lcd.print(getPreferedLidOpenness());
    lcd.print("/");
    lcd.print(getCurrentLidOpenness());
  }
}

void openEditMenu(byte *preferedValue, byte option) {
  editTarget = preferedValue;
  editOption = option;
  editValue = *preferedValue;
  menuScreen = MENU_EDIT;
  menuNeedsRedraw = true;
}

void handleEditMenu(byte key) {
  if (menuNeedsRedraw) {
    drawEditScreen();
    menuNeedsRedraw = false;
  }

  if (key == 1 && editValue < 100) {
    editValue++;
    lcd.setCursor(17, 1);
    lcd.print("   ");
    lcd.setCursor(17, 1);
    lcd.print(editValue);
  } else if (key == 2 && editValue > 0) {
    editValue--;
    lcd.setCursor(17, 1);
    lcd.print("   ");
    lcd.setCursor(17, 1);
    lcd.print(editValue);
  } else if (key == 3) {
    *editTarget = editValue;
    EEPROM.write(editOption, editValue);
    editTarget = NULL;
    menuScreen = MENU_MAIN;
    menuNeedsRedraw = true;
  }
}

void handleMainMenu(byte key) {
  if (menuNeedsRedraw) {
    lcd.clear();
    showMainMenu();
    lcd.setCursor(0, menuCursor);
    lcd.print('>');
    menuNeedsRedraw = false;
  }

  if (key == 0) {
    return;
  }

  lcd.setCursor(0, menuCursor);
  lcd.print(' ');

  switch (key) {
    case 1:
      if (menuCursor > 0) {
        menuCursor--;
      }
      break;
    case 2:
      if (menuCursor < 3) {
        menuCursor++;
      }
      break;
    case 3:
      switch (menuCursor) {
        case 0:
          openEditMenu(&preferedTemperature, 0);
          return;
        case 1:
          openEditMenu(&preferedHumidity, 1);
          return;
        case 2:
          openEditMenu(&preferedCO2, 2);
          return;
        case 3:
          menuScreen = MENU_NONE;
          lcd.clear();
          return;
      }
      break;
  }

  lcd.setCursor(0, menuCursor);
  lcd.print('>');
}

void updateMenu(bool state, byte key) {
  switch (menuScreen) {
    case MENU_NONE:
      mainDisplay(state);
      if (key == 3) {
        menuScreen = MENU_MAIN;
        menuCursor = 0;
        menuNeedsRedraw = true;
      }
      break;
    case MENU_MAIN:
      handleMainMenu(key);
      break;
    case MENU_EDIT:
      handleEditMenu(key);
      break;
  }
}
