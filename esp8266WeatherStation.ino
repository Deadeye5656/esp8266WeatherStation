#include <Arduino_JSON.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <TimeLib.h>

#define SEALEVELPRESSURE_HPA (1013.25)
#define BUZZER D8
#define RIGHT_CLICK D7
#define LEFT_CLICK D6
#define BUTTON_MODE D5

const char* ssid     = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";

String openWeatherMapApiKey = "OPEN_WEATHER_API_KEY";
String timezoneApiKey = "TIMEZONE_API_KEY";
// Royal Oak, MI
String lat = "42.49";
String lon = "-83.14";
String unit = "imperial";
String dayCount = "7";

String jsonBuffer;
JSONVar jsonResponse;
JSONVar jsonResponseTime;
int dayIndex = 0;
boolean updateWeatherDisplay = false;
JSONVar weatherDataList;
unsigned long weatherUpdateTimerDelay = 3600000;
unsigned long lastWeatherUpdate = 0;

unsigned long timeSetTimerDelay = 1000;
unsigned long lastTimeSet = 0;
int lastMinute = -1;
long timeOffset = 1;

Adafruit_BME280 bme;
int humidityTolerance = -3;
int humidity = 0;
int temp = 0;
int lastHumidity = 0;
int lastTemp = 0;
unsigned long tempCheckTimerDelay = 1000;
unsigned long lastTempCheck = 0;
boolean updateTempDisplay = false;

boolean buzzerOn = false;
unsigned long buzzerStart;

boolean modeClicked = false;
boolean leftClicked = false;
boolean rightClicked = false;

LiquidCrystal_I2C lcd(0x27, 16, 2);
int currentMode = 0; // 0 = time, 1 = temp/humidity, 2 = weather

void setup() {
  Serial.begin(9600);

  bme.begin(0x76);  

  pinMode(LEFT_CLICK, INPUT);
  pinMode(RIGHT_CLICK, INPUT);
  pinMode(BUZZER, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("David's Weather");
  lcd.setCursor(0, 1);
  lcd.print("Station");
  wifiConnect();
  getWeatherData();
  lcd.clear();
}

void loop(){
  if (digitalRead(LEFT_CLICK) == HIGH && currentMode == 2) {
    if (!leftClicked){
      buzzerOn = true;
      buzzerStart = millis();
      leftClicked = true;
      if (dayIndex == 0) {
        dayIndex = 6;
      } else {
        dayIndex--;
      }
      updateWeatherDisplay = true;
    }
  } else {
    leftClicked = false;
  }

  if (digitalRead(RIGHT_CLICK) == HIGH && currentMode == 2) {
    if (!rightClicked){
      buzzerOn = true;
      buzzerStart = millis();
      rightClicked = true;
      if (dayIndex == 6) {
        dayIndex = 0;
      } else {
        dayIndex++;
      }
      updateWeatherDisplay = true;
    }
  } else {
    rightClicked = false;
  }

  if (digitalRead(BUTTON_MODE) == HIGH) {
    if (!modeClicked){
      buzzerOn = true;
      buzzerStart = millis();
      modeClicked = true;
      if (currentMode == 0) {
        currentMode++;
        lastHumidity = -1;
        lastTemp = -1;
        lastTempCheck = 0;
        updateTempDisplay = true;
      } else if (currentMode == 1){
        currentMode++;
        dayIndex = 0;
        updateWeatherDisplay = true;
      } else {
        currentMode = 0;
        lastTimeSet = 0;
        lastMinute = -1;
      }
    }
  } else {
    modeClicked = false;
  }

  // Beep for key input
  if (buzzerOn) {
    digitalWrite(BUZZER, HIGH);
    if (millis()-buzzerStart > 100) {
      buzzerOn = false;
      digitalWrite(BUZZER, LOW);
    }
  }

  if (currentMode == 0 && ((millis() - lastTimeSet) > timeSetTimerDelay)){
    String hourStr = getDoubleDigitHour(hour());
    String minuteStr = getDoubleDigit(minute());
    String secondStr = getDoubleDigit(second());

    int currentMinute = minute();
    if (lastMinute != currentMinute){
      String amPm = getAMPM();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(String(month()) + "/" + String(day()) + "/" + String(year()).substring(2,4) + " " + getDay(weekday()));
      lcd.setCursor(0, 1);
      lcd.print(hourStr + ":" + minuteStr + ":" + secondStr + " " + amPm);
    } else {
      lcd.setCursor(6, 1);
      lcd.print(secondStr);
    }
    lastMinute = currentMinute;
    lastTimeSet = millis();
  }

  // Every 2 seconds, check temp and humidity
  if (((millis() - lastTempCheck) > tempCheckTimerDelay) && currentMode == 1) {
    humidity = int(bme.readHumidity()) + humidityTolerance;
    temp = int(1.8 * bme.readTemperature() + 32);
    lastTempCheck = millis();
    if (updateTempDisplay){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Temp-");
      lcd.setCursor(7, 0);
      lcd.print("F");
      lcd.setCursor(0, 1);
      lcd.print("Humidity-");
      lcd.setCursor(11, 1);
      lcd.print("%");
      updateTempDisplay = false;
    }
    if (temp != 32 && lastTemp != temp){
      lcd.setCursor(5, 0);
      lcd.print(temp);
      lastTemp = temp;
    }
    if (humidity != 0 && lastHumidity != humidity){
      lcd.setCursor(9, 1);
      lcd.print(humidity);
      lastHumidity = humidity;
    }
  }

  if (currentMode == 2 && updateWeatherDisplay){
    int min = int(weatherDataList[dayIndex]["temp"]["min"]);
    int max = int(weatherDataList[dayIndex]["temp"]["max"]);
    String desc = JSON.stringify(weatherDataList[dayIndex]["weather"][0]["main"]);
    String dayDisplayed = getDisplayDay(weekday(), dayIndex);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(dayDisplayed);
    lcd.print(" Low-");
    lcd.print(min);
    lcd.print("F");
    lcd.setCursor(0, 1);
    lcd.print("High-");
    lcd.print(max);
    lcd.print("F ");
    lcd.print(desc.substring(1, desc.length()-1));
    updateWeatherDisplay = false;
  }

  if ((millis() - lastWeatherUpdate) > weatherUpdateTimerDelay){
    getWeatherData();
    lastWeatherUpdate = millis();
  }
}

void getWeatherData(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Fetching Updated");
  lcd.setCursor(0, 1);
  lcd.print("Weather Data");

  wifiConnect();
  if(WiFi.status()== WL_CONNECTED){
    String serverPath = "http://api.openweathermap.org/data/2.5/forecast/daily?lat=" + lat + "&lon=" + lon + "&units=" + unit + "&cnt=" + dayCount + "&appid=" + openWeatherMapApiKey;
    
    jsonBuffer = httpGETRequest(serverPath.c_str());
    jsonResponse = JSON.parse(jsonBuffer);
    weatherDataList = jsonResponse["list"];

    serverPath = "http://api.timezonedb.com/v2.1/get-time-zone?key=" + timezoneApiKey + "&format=json&by=position&lat="+ lat + "&lng="+ lon;
    
    jsonBuffer = httpGETRequest(serverPath.c_str());
    jsonResponseTime = JSON.parse(jsonBuffer);

    setTime(long(jsonResponseTime["timestamp"]) + timeOffset);
    if (JSON.typeof(jsonResponse) == "undefined" || JSON.typeof(jsonResponseTime) == "undefined") {
      return;
    }
  }
  // reset displays
  if (currentMode == 0){
    lastTimeSet = 0;
    lastMinute = -1;
  } else if (currentMode == 1){
    lastHumidity = -1;
    lastTemp = -1;
    lastTempCheck = 0;
  } else {
    updateWeatherDisplay = true;
  }
}

void wifiConnect(){
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}


String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;
    
  http.begin(client, serverName);
  
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    payload = http.getString();
  }
  http.end();

  return payload;
}

String getDisplayDay(int day, int dayIndex) {
  int dayDisplayed = day + dayIndex;
  if (dayDisplayed > 7) {
    dayDisplayed = dayDisplayed - 7;
  }
  return getDay(dayDisplayed);
}

String getDay(int day){
  switch(day){
      case 1:
        return "Sun.";
      case 2:
        return "Mon.";
      case 3:
        return "Tues.";
      case 4:
        return "Wed.";
      case 5:
        return "Thurs.";
      case 6:
        return "Fri.";
      case 7:
        return "Sat.";
      default:
        return "";
  }
}

String getAMPM(){
  if (isAM()){
    return "AM";
  } else {
    return "PM";
  }
}


String getDoubleDigit(int val){
  if (val < 10) {
    return "0" + String(val);
  }
  return String(val);
}

String getDoubleDigitHour(int val){
  if (val == 0) {
    return "12";
  }
  return getDoubleDigit(val);
}