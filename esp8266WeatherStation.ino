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

// Weather variables
String jsonBuffer;
JSONVar jsonResponseWeather;
JSONVar jsonResponseTime;
int dayIndex = 0;
JSONVar weatherDataList;
int weatherUpdateTimerDelay = 3600000;
unsigned long lastWeatherUpdate = 0;

// Temp variables
Adafruit_BME280 bme;
int humidity = 0;
int temp = 0;
unsigned long tempCheckTimerDelay = 1000;
unsigned long lastTempCheck = 0;

// Timer variables
int timerHours = 0;
int timerMins = 0;
boolean settingHoursTimer = false;
boolean settingMinutesTimer = false;
boolean timerEnabled = false;
unsigned long timerEnd = 0;
 
// Alarm variables
int alarmHours = 0;
int alarmMins = 0;
boolean alarmAm = true;
boolean settingHoursAlarm = false;
boolean settingMinutesAlarm = false;
boolean settingAmpmAlarm = false;
boolean alarmEnabled = false;

// Buzzer Alarm variables
long buzzerFlipInterval = 300;
unsigned long lastBuzzerFlip = 0;
boolean alarmBuzzerOn = false;
boolean alarmTriggered = false;

boolean buzzerOn = false;
unsigned long buzzerStart;

boolean modeClicked = false;
boolean bothClickedTimer = false;
boolean bothClickedAlarm = false;
boolean scrollClicked = false;

LiquidCrystal_I2C lcd(0x27, 16, 2);
int currentMode = 0; // 0 = time, 1 = temp/humidity, 2 = weather, 3 = timer, 4 = alarm

void setup() {
  bme.begin(0x76);  

  pinMode(LEFT_CLICK, INPUT);
  pinMode(RIGHT_CLICK, INPUT);
  pinMode(BUZZER, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(F("David's Weather"));
  lcd.setCursor(0, 1);
  lcd.print(F("Station"));
  wifiConnect();
  getWeatherData();
  lcd.clear();
}

void loop(){
  int leftClicked = digitalRead(LEFT_CLICK);
  int rightClicked = digitalRead(RIGHT_CLICK);
  int modeButtonClicked = digitalRead(BUTTON_MODE);
  // Alarm or timer going off
  if (alarmTriggered){
    if (leftClicked || rightClicked || modeButtonClicked){
      alarmTriggered = false;
      lcd.clear();
      buzzerOn = true;
      buzzerStart = millis();
    } else {
      if (alarmBuzzerOn) {
        digitalWrite(BUZZER, HIGH);
      } else {
        digitalWrite(BUZZER, LOW);
      }
      if (millis() - lastBuzzerFlip >= buzzerFlipInterval) {
        lastBuzzerFlip = millis();
        if (alarmBuzzerOn){
          buzzerFlipInterval = 2000;
        } else {
          buzzerFlipInterval = 300;
        }
        alarmBuzzerOn = !alarmBuzzerOn; // Toggle between high and low
      }
      return;
    }
  }

  // Left and right clicked while in timer mode
  if (leftClicked == HIGH && rightClicked == HIGH && currentMode == 3){
    if (!bothClickedTimer){
      bothClickedTimer = true;
      if (timerEnabled){ // if timer is counting down and there is a double click, then cancel timer
        timerEnabled = false;
        lcd.clear();
      } else if (!settingHoursTimer && !settingMinutesTimer){
        settingHoursTimer = true;
        scrollClicked = true; // this avoids immediately changing hours after double click
        lcd.clear();
      }
      buzzerOn = true;
      buzzerStart = millis();
    } 
  } else {
    bothClickedTimer = false;
  }

  // Left and right clicked while in alarm mode
  if (leftClicked == HIGH && rightClicked == HIGH && currentMode == 4){
    if (!bothClickedAlarm){
      bothClickedAlarm = true;
      if (alarmEnabled){ // if timer is counting down and there is a double click, then cancel timer
        alarmEnabled = false;
        lcd.clear();
      } else if (!settingHoursAlarm && !settingMinutesAlarm && !settingAmpmAlarm){
        settingHoursAlarm = true;
        scrollClicked = true; // this avoids immediately changing hours after double click
        lcd.clear();
      }
      buzzerOn = true;
      buzzerStart = millis();
    } 
  } else {
    bothClickedAlarm = false;
  }

  // Left or right click selected while setting timer minutes or hours
  if ((leftClicked == HIGH || rightClicked == HIGH) && 
      (((currentMode == 3 || currentMode == 4)
      && (settingHoursTimer || settingMinutesTimer || settingHoursAlarm || settingMinutesAlarm || settingAmpmAlarm))
      || currentMode == 2)) {
    if (!scrollClicked){
      scrollClicked = true;

      if (currentMode == 2){
        if (leftClicked == HIGH) {
          if (dayIndex == 0) {
            dayIndex = 6;
          } else {
            dayIndex--;
          }
        }
        if (rightClicked == HIGH) {
          if (dayIndex == 6) {
            dayIndex = 0;
          } else {
            dayIndex++;
          }
        }
        lcd.clear();
      }

      if (currentMode == 3){
        // Clear previously shown hours/minutes
        lcd.setCursor(7, 1);
        lcd.print(F("  "));
        if (settingHoursTimer){
          if (leftClicked){
            if (timerHours == 0) {
              timerHours = 24;
            } else {
              timerHours--;
            }
          } else if (rightClicked){
            if (timerHours == 24) {
              timerHours = 0;
            } else {
              timerHours++;
            }
          }
        } else if (settingMinutesTimer){
          if (leftClicked){
            if (timerMins == 0) {
              timerMins = 59;
            } else {
              timerMins--;
            }
          } else if (rightClicked){
            if (timerMins == 59) {
              timerMins = 0;
            } else {
              timerMins++;
            }
          }
        }
      }

      if (currentMode == 4){
        // Clear previously shown hours/minutes
        lcd.setCursor(7, 1);
        lcd.print("  ");
        if (settingHoursAlarm){
          if (leftClicked){
            if (alarmHours == 0) {
              alarmHours = 24;
            } else {
              alarmHours--;
            }
          } else if (rightClicked){
            if (alarmHours == 24) {
              alarmHours = 0;
            } else {
              alarmHours++;
            }
          }
        } else if (settingMinutesAlarm){
          if (leftClicked){
            if (alarmMins == 0) {
              alarmMins = 59;
            } else {
              alarmMins--;
            }
          } else if (rightClicked){
            if (alarmMins == 59) {
              alarmMins = 0;
            } else {
              alarmMins++;
            }
          }
        } else if (settingAmpmAlarm){
          alarmAm = !alarmAm;
        }
      }

      buzzerOn = true;
      buzzerStart = millis();
    }
  } else {
    scrollClicked = false;
  }

  // Mode change pressed, handle new mode or setting hours or minutes
  if (modeButtonClicked == HIGH) {
    if (!modeClicked){
      modeClicked = true; 
      if (settingHoursTimer){
        settingMinutesTimer = true;
        settingHoursTimer = false;
      } else if (settingMinutesTimer){
        timerEnabled = true;
        unsigned long totalTimerLength = ((timerHours * 60) + timerMins) * 60 * 1000;
        timerEnd = millis() + totalTimerLength;
        settingMinutesTimer = false;
      } else if (settingHoursAlarm){
        settingMinutesAlarm = true;
        settingHoursAlarm = false;
      } else if (settingMinutesAlarm){
        settingMinutesAlarm = false;
        settingAmpmAlarm = true;
      } else if (settingAmpmAlarm){
        alarmEnabled = true;
        settingAmpmAlarm = false;
      } else if (currentMode == 1) {
        dayIndex = 0;
        currentMode++;
      } else if (currentMode == 4){
        currentMode = 0;
      } else {
        currentMode++;
      }
      lcd.clear();
      buzzerOn = true;
      buzzerStart = millis();
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

  // Set time
  if (currentMode == 0){
    String hourStr = getDoubleDigitHour(hour());
    String minuteStr = getDoubleDigit(minute());
    String secondStr = getDoubleDigit(second());
    String amPm = getAMPM();
    lcd.setCursor(0, 0);
    lcd.print(String(month()) + "/" + String(day()) + "/" + String(year()).substring(2,4) + " " + getDay(weekday()));
    lcd.setCursor(0, 1);
    lcd.print(hourStr + ":" + minuteStr + ":" + secondStr + " " + amPm);
  }

  // Every 2 seconds, check temp and humidity
  if (((millis() - lastTempCheck) > tempCheckTimerDelay) && currentMode == 1) {
    humidity = int(bme.readHumidity());
    temp = int(1.8 * bme.readTemperature() + 32);
    lcd.setCursor(0, 0);
    lcd.print(F("Temp-"));
    lcd.print(temp);
    lcd.print(F("F"));
    lcd.setCursor(0, 1);
    lcd.print(F("Humidity-"));
    lcd.print(humidity);
    lcd.print(F("%"));
    lastTempCheck = millis();
  }

  // Set weather
  if (currentMode == 2){
    int min = int(weatherDataList[dayIndex]["temp"]["min"]);
    int max = int(weatherDataList[dayIndex]["temp"]["max"]);
    String desc = JSON.stringify(weatherDataList[dayIndex]["weather"][0]["main"]);
    String dayDisplayed = getDisplayDay(weekday(), dayIndex);

    lcd.setCursor(0, 0);
    lcd.print(dayDisplayed);
    lcd.print(F(" Low-"));
    lcd.print(min);
    lcd.print(F("F"));
    lcd.setCursor(0, 1);
    lcd.print(F("High-"));
    lcd.print(max);
    lcd.print(F("F "));
    lcd.print(desc.substring(1, desc.length()-1));
  }

  // Show timer or prompt for new timer
  if (currentMode == 3){
    if (timerEnabled){
      int secondsRemaining = int((timerEnd - millis()) / 1000);
      int hoursRemaining = int(secondsRemaining / 3600);
      secondsRemaining = secondsRemaining % 3600;
      int minsRemaining = int(secondsRemaining / 60);
      secondsRemaining = secondsRemaining % 60;
      lcd.setCursor(0, 0);
      lcd.print(F("Time remaining:"));
      lcd.setCursor(0,1);
      lcd.print(getDoubleDigit(hoursRemaining)+":"+getDoubleDigit(minsRemaining)+":"+getDoubleDigit(secondsRemaining));
    } else if (settingHoursTimer){
      lcd.setCursor(0, 0);
      lcd.print(F("How many hours?"));
      lcd.setCursor(7, 1);
      lcd.print(timerHours);
    } else if (settingMinutesTimer){
      lcd.setCursor(0, 0);
      lcd.print(F("How many mins?"));
      lcd.setCursor(7, 1);
      lcd.print(timerMins);
    } else {
      lcd.setCursor(0, 0);
      lcd.print(F("Start a timer?"));
      lcd.setCursor(0, 1);
      lcd.print(F("Click "));
      lcd.write(0x7F); // Left arrow
      lcd.print(F(" and "));
      lcd.write(0x7E); // Right arrow
    }
  }

  // Set alarm
  if (currentMode == 4){
    if (alarmEnabled){
      lcd.setCursor(0, 0);
      lcd.print(F("Alarm Set For:"));
      lcd.setCursor(0,1);
      lcd.print(getDoubleDigit(alarmHours)+":"+getDoubleDigit(alarmMins)+" ");
      if (alarmAm){
        lcd.print(F("AM"));
      } else {
        lcd.print(F("PM"));
      }
    } else if (settingHoursAlarm){
      lcd.setCursor(0, 0);
      lcd.print(F("At which hour?"));
      lcd.setCursor(7, 1);
      lcd.print(alarmHours);
    } else if (settingMinutesAlarm){
      lcd.setCursor(0, 0);
      lcd.print("At which minute?");
      lcd.setCursor(7, 1);
      lcd.print(alarmMins);
    } else if (settingAmpmAlarm){
      lcd.setCursor(0, 0);
      lcd.print(F("AM or PM?"));
      lcd.setCursor(7, 1);
      if (alarmAm){
        lcd.print(F("AM"));
      } else {
        lcd.print(F("PM"));
      }
    } else {
      lcd.setCursor(0, 0);
      lcd.print(F("Set an alarm?"));
      lcd.setCursor(0, 1);
      lcd.print(F("Click "));
      lcd.write(0x7F); // Left arrow
      lcd.print(F(" and "));
      lcd.write(0x7E); // Right arrow
    }
  }

  // trigger timer at end if set
  if (millis() > timerEnd && timerEnabled){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Timer ended"));
    alarmTriggered = true;
    timerEnabled = false;
    timerEnd = 0;
  }
  
  // Trigger alarm if at alarm time
  if (alarmEnabled && hour() == alarmHours && minute() == alarmMins && (isAM() && alarmAm) || (isPM() && !alarmAm)){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Alarm triggered"));
    alarmEnabled = false;
    alarmTriggered = true;
  }

  // Update weather data hourly
  if ((millis() - lastWeatherUpdate) > weatherUpdateTimerDelay){
    getWeatherData();
    lastWeatherUpdate = millis();
  }
}

void getWeatherData(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Fetching Updated"));
  lcd.setCursor(0, 1);
  lcd.print(F("Weather Data"));

  wifiConnect();
  if(WiFi.status()== WL_CONNECTED){
    String serverPath = "http://api.openweathermap.org/data/2.5/forecast/daily?lat=" + lat + "&lon=" + lon + "&units=" + unit + "&cnt=" + dayCount + "&appid=" + openWeatherMapApiKey;
    
    jsonBuffer = httpGETRequest(serverPath.c_str());
    jsonResponseWeather = JSON.parse(jsonBuffer);
    weatherDataList = jsonResponseWeather["list"];

    serverPath = "http://api.timezonedb.com/v2.1/get-time-zone?key=" + timezoneApiKey + "&format=json&by=position&lat="+ lat + "&lng="+ lon;
    
    jsonBuffer = httpGETRequest(serverPath.c_str());
    jsonResponseTime = JSON.parse(jsonBuffer);

    setTime(long(jsonResponseTime["timestamp"]));
    if (JSON.typeof(jsonResponseWeather) == "undefined" || JSON.typeof(jsonResponseTime) == "undefined") {
      return;
    }
  }
  lcd.clear();
}

void wifiConnect(){
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
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
  if (val > 12){
    val = val - 12;
  }
  return getDoubleDigit(val);
}