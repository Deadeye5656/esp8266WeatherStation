#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <Keypad.h>
#include "DHT.h"
#include <LiquidCrystal.h>

#define DHTTYPE DHT21   // DHT 21 (AM2301)
#define DHTPIN 40     // Digital pin connected to the DHT sensor
DHT dht(DHTPIN, DHTTYPE);
float humidity;
float temp;
int heatIndex;
unsigned long timerDelay = 2000;
float tempTolerance = -5;
boolean initialCall = true;
unsigned long lastTime = 0;

const int rs = 52, en = 50, d4 = 48, d5 = 46, d6 = 44, d7 = 42;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const byte ROWS = 4; 
const byte COLS = 4; 

char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {9, 8, 7, 6}; 
byte colPins[COLS] = {5, 4, 3, 2}; 

Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 
char currentKey = 'x';
char lastKey = 'x';

SoftwareSerial Arduino_SoftSerial(10, 11);  // RX, TX
String data = "";

boolean buzzerOn = false;
unsigned long buzzerStart;

void setup() {
  Serial.begin(4800);
  Arduino_SoftSerial.begin(4800);
  lcd.begin(16, 2);
  lcd.clear();
  pinMode(22, OUTPUT); //buzzer
  // dht.begin();
}

void loop() {
  // if (((millis() - lastTime) > timerDelay) || initialCall) {
  //   initialCall = false;
  //   humidity = dht.readHumidity();
  //   temp = dht.readTemperature(true);
  //   lastTime = millis();
  //   heatIndex = int(dht.computeHeatIndex(temp, humidity) + tempTolerance);
  //   Serial.println(heatIndex);
  // }

  char customKey = customKeypad.getKey();

  if (buzzerOn) {
    digitalWrite(22, HIGH);
    if (millis()-buzzerStart > 10) {
      buzzerOn = false;
    }
  } else {
    digitalWrite(22, LOW);
  }

  if (customKey && int(customKey) - '0' >= 1 && int(customKey) - '0' <= 7){
    currentKey = customKey;
    buzzerOn = true;
    buzzerStart = millis();
  }

  while (Arduino_SoftSerial.available()) {
    char c = Arduino_SoftSerial.read();
    if (c == '\n'){
      processCall(data);
      data = "";
    } else {
      data.concat(c);
    }
  }
}

void processCall(String command) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(command);
  String test = root["list"][6]["desc"];
  if (test == "") {return;}
  if (root.success()) {
    boolean validData = true;
    if (currentKey != 'x' && lastKey != currentKey){ 
      int currentKeyInt = int(currentKey) - '0';
      int currentDay = atoi(root["day"]);
      int dayDiff = currentKeyInt - currentDay;
      if (dayDiff < 0) {dayDiff += 7;}
      int min = atoi(root["list"][dayDiff]["min"]);
      int max = atoi(root["list"][dayDiff]["max"]);
      String desc = root["list"][dayDiff]["desc"];
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(getDay(currentKeyInt));
      lcd.print(" Low-");
      lcd.print(min);
      lcd.print("F");
      lcd.setCursor(0, 1);
      lcd.print("High-");
      lcd.print(max);
      lcd.print("F ");
      lcd.print(desc);
      lastKey = currentKey;
    }
  }
}

String getDay(int day){
  switch(day){
      case 1:
        return "Monday";
      case 2:
        return "Tuesday";
      case 3:
        return "Wed.";
      case 4:
        return "Thursday";
      case 5:
        return "Friday";
      case 6:
        return "Saturday";
      case 7:
        return "Sunday";
      default:
        return "";
  }
}