#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <Keypad.h>
#include "DHT.h"
#include <LiquidCrystal.h>

#define BUZZER 22
#define ESP8266_OFF 38
#define ESP8266_ON 36

#define DHTTYPE DHT21   // DHT 21 (AM2301)
#define DHTPIN 40     // Digital pin connected to the DHT sensor
DHT dht(DHTPIN, DHTTYPE);
int humidity = 0;
float temp;
int heatIndex = 0;
int lastHumidity = 0;
int lastHeatIndex = 0;
unsigned long tempCheckTimerDelay = 2000;
unsigned long resetDisplayDelay = 5000;
float tempTolerance = -3;
boolean forceNewDisplay = true;
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

String data = "";
boolean fetchData = true;
boolean resetESP = false;
unsigned long lastDataFetch;
unsigned long dataTimerDelay = 3600000;
JsonVariant list;
int currentDay;

boolean buzzerOn = false;
unsigned long buzzerStart;

void setup() {
  Serial1.begin(9600);
  Serial.begin(115200);
  dht.begin();

  pinMode(BUZZER, OUTPUT);
  pinMode(ESP8266_OFF, OUTPUT);
  digitalWrite(ESP8266_OFF, LOW);

  pinMode(ESP8266_ON, OUTPUT);
  digitalWrite(ESP8266_ON, LOW);
  delayMicroseconds(100);
  digitalWrite(ESP8266_ON, HIGH);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("David's Weather");
  lcd.setCursor(0, 1);
  lcd.print("Station");

  delay(2000);
}

void loop() {
  // Every 2 seconds, check temp and humidity if user isn't prompting for weather data
  if (((millis() - lastTime) > tempCheckTimerDelay) && currentKey == 'x' && !fetchData) {
    humidity = int(dht.readHumidity());
    temp = dht.readTemperature(true);
    lastTime = millis();
    heatIndex = int(dht.computeHeatIndex(temp, humidity) + tempTolerance);
    // Only clear and display if data has changed
    if ((humidity != 0 && heatIndex != 0) && ((lastHumidity != humidity || lastHeatIndex != heatIndex) || forceNewDisplay)){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Temp-");
      lcd.print(heatIndex);
      lcd.print("F");
      lcd.setCursor(0, 1);
      lcd.print("Humidity-");
      lcd.print(humidity);
      lcd.print("%");
      lastHumidity = humidity;
      lastHeatIndex = heatIndex;
      forceNewDisplay = false;
    }
  }

  // After resetDisplayDelay, reset display back to temp and humidity
  if (currentKey != 'x'){
    if (millis()-buzzerStart > resetDisplayDelay) {
      currentKey = 'x';
      lastKey = 'x';
      lastHumidity = 0;
      lastHeatIndex = 0;
    }
  }

  char customKey = customKeypad.getKey();
  // Update input if 1-7
  if (customKey && int(customKey) - '0' >= 1 && int(customKey) - '0' <= 7){
    currentKey = customKey;
    lastKey = 'x';
    buzzerOn = true;
    buzzerStart = millis();
  }

  // Beep for key input
  if (buzzerOn) {
    digitalWrite(BUZZER, HIGH);
    if (millis()-buzzerStart > 100) {
      buzzerOn = false;
      digitalWrite(BUZZER, LOW);
    }
  }

  // Start ESP8266 10 seconds before the hour
  if (!resetESP && (millis()-lastDataFetch > dataTimerDelay - 10000)){
    digitalWrite(ESP8266_ON, LOW);
    delayMicroseconds(100);
    digitalWrite(ESP8266_ON, HIGH);
    resetESP = true;
  }

  // fetch data from ESP8266 every hour
  if ((millis()-lastDataFetch > dataTimerDelay)){
    fetchData = true;
  }

  if (fetchData){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Fetching Updated");
    lcd.setCursor(0, 1);
    lcd.print("Weather Data");
  }

  // Don't continue until data is pulled
  while (fetchData) {
    if (Serial1.available())  {
      data = Serial1.readStringUntil('>');
      if (data == "") {continue;}
      DynamicJsonBuffer jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(data);
      boolean isDataComplete = verifyData(root);
      if (isDataComplete){
        currentDay = int(root["day"]);
        list = root["list"];
        fetchData = false;
        lastDataFetch = millis();
        resetESP = false;
        forceNewDisplay = true;
        // Tell ESP8266 to go to sleep, allow 2 seconds to read
        digitalWrite(ESP8266_OFF, HIGH);
        delay(2000);
        digitalWrite(ESP8266_OFF, LOW);
        break;
      } 
    }
  }
  
  // Display current weather data if key is pressed
  if (currentKey != 'x' && lastKey != currentKey){ 
    processCall(list);
  }
}

boolean verifyData(JsonObject& root){
  for (int i = 0; i < 7; i++){
    int min = atoi(root["list"][i]["min"]);
    int max = atoi(root["list"][i]["max"]);
    String desc = root["list"][i]["desc"];
    if (desc == "" || (min == 0 && max == 0)){
      return false;
    }
  }
  if (root.success()){
    return true;
  } else {
    return false;
  }
}

void processCall(JsonVariant root) {
  int currentKeyInt = int(currentKey) - '0';
  int dayDiff = currentKeyInt - currentDay;
  if (dayDiff < 0) {dayDiff += 7;}
  int min = atoi(root[dayDiff]["min"]);
  int max = atoi(root[dayDiff]["max"]);
  String desc = root[dayDiff]["desc"];
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

