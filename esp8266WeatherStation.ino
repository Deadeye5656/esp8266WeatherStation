#include <Arduino_JSON.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <LiquidCrystal_I2C.h>
#include <TimeLib.h>

#define SEALEVELPRESSURE_HPA (1013.25)
#define BUZZER D3
#define BUZZER_FREQ 4000
#define RIGHT_CLICK D7
#define LEFT_CLICK D6
#define BUTTON_MODE D5

#define CLOCK_MODE 0
#define TEMP_MODE 1
#define WEATHER_MODE 2
#define FORECAST_MODE 3
#define TIMER_MODE 4
#define ALARM_MODE 5

// Clock Variables
int lastDay;

// Forecast variables
String jsonBuffer;
JSONVar jsonResponseForecast;
JSONVar jsonResponseTime;
int dayIndex = 0;
JSONVar weatherDataList;
int weatherUpdateTimerDelay = 1800000; // 30 minutes
unsigned long lastWeatherUpdate = 0;

// Weather Variables
JSONVar jsonResponseWeather;
int weatherTemp = 0;
String weatherCondition = "";
int weatherHumidity = 0;

// Temp variables
Adafruit_BME280 bme;
int humidity = 0;
int temp = 0;
unsigned long tempCheckTimerDelay = 1000;
unsigned long lastTempCheck = 0;
float tempTolerance = 0;
unsigned long lastToleranceCheck = 0;

// Timer variables
int timerHours = 0;
int timerMins = 0;
boolean settingHoursTimer = false;
boolean settingMinutesTimer = false;
boolean timerEnabled = false;
unsigned long timerEnd = 0;
 
// Alarm variables
int alarmHours = 12;
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

boolean modeClicked = false;
boolean bothClickedTimer = false;
boolean bothClickedAlarm = false;
boolean scrollClicked = false;
boolean alarmDisabledClicked = false;
unsigned long lastScrollSet = 0;
unsigned long firstScrollSet = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);
int currentMode = 0; // 0 = time, 1 = temp/humidity, 2 = forecast, 3 = current weather, 4 = timer, 5 = alarm

void setup() {
  bme.begin(0x76);  

  pinMode(LEFT_CLICK, INPUT);
  pinMode(RIGHT_CLICK, INPUT);
  pinMode(BUZZER, OUTPUT);

  lcd.init();
  lcd.backlight();
  printToLCD(F("David's Weather"), 0);
  printToLCD(F("Station"), 1);
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
      if (!alarmDisabledClicked){
        alarmDisabledClicked = true;
        alarmTriggered = false;
        lcd.clear();
        tone(BUZZER, BUZZER_FREQ, 100);
      }
    } else {
      if (alarmBuzzerOn) {
        tone(BUZZER, BUZZER_FREQ);
      } else {
        noTone(BUZZER);
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
      alarmDisabledClicked = false;
    }
    return;
  }

  // Left and right clicked while in timer mode
  if (leftClicked == HIGH && rightClicked == HIGH && currentMode == TIMER_MODE){
    if (!bothClickedTimer){
      bothClickedTimer = true;
      handleDoubleClickTimer();
      tone(BUZZER, BUZZER_FREQ, 100);
      // This prevents double click adjusting timer/alarm
      scrollClicked = true;
      firstScrollSet = millis();
    } 
  } else {
    bothClickedTimer = false;
  }

  // Left and right clicked while in alarm mode
  if (leftClicked == HIGH && rightClicked == HIGH && currentMode == ALARM_MODE){
    if (!bothClickedAlarm){
      bothClickedAlarm = true;
      handleDoubleClickAlarm();
      tone(BUZZER, BUZZER_FREQ, 100);
      // This prevents double click adjusting timer/alarm
      scrollClicked = true;
      firstScrollSet = millis();
    } 
  } else {
    bothClickedAlarm = false;
  }

  // Left or right click selected while setting timer/alarm minutes or hours, or weather day
  if ((leftClicked == HIGH || rightClicked == HIGH) && 
      (((currentMode == TIMER_MODE || currentMode == ALARM_MODE)
      && (settingHoursTimer || settingMinutesTimer || settingHoursAlarm || settingMinutesAlarm || settingAmpmAlarm))
      || currentMode == FORECAST_MODE)) {
    if (!scrollClicked){ 
      scrollClicked = true;
      if (currentMode == FORECAST_MODE){
        handleWeatherScroll(leftClicked, rightClicked);
      }
      if (currentMode == TIMER_MODE){
        handleTimerScroll(leftClicked, rightClicked);
      }
      if (currentMode == ALARM_MODE){
        handleAlarmScroll(leftClicked, rightClicked);
      }
      firstScrollSet = millis();
      lastScrollSet = millis();
      tone(BUZZER, BUZZER_FREQ, 100);
    } else if (millis() - firstScrollSet > 1500){
      if (millis() - lastScrollSet > 150){
        if (currentMode == TIMER_MODE){
          handleTimerScroll(leftClicked, rightClicked);
        }
        if (currentMode == ALARM_MODE){
          handleAlarmScroll(leftClicked, rightClicked);
        }
        lastScrollSet = millis();
      }
    }
  } else {
    scrollClicked = false;
  }

  // Mode change pressed, handle new mode or setting hours or minutes
  if (modeButtonClicked == HIGH) {
    if (!modeClicked){
      modeClicked = true; 
      handleModeClick();
      tone(BUZZER, BUZZER_FREQ, 100);
    }
  } else {
    modeClicked = false;
  }

  // Set time
  if (currentMode == CLOCK_MODE){
    String hourStr = getDoubleDigit(hourFormat12());
    String minuteStr = getDoubleDigit(minute());
    String secondStr = getDoubleDigit(second());
    String amPm = getAMPM();
    int currentDay = weekday();
    if (lastDay != currentDay) {lcd.clear();} // Clear if day has changed
    printToLCD(String(month()) + "/" + String(day()) + "/" + String(year()).substring(2,4) + " " + getDay(currentDay), 0);
    printToLCD(hourStr + ":" + minuteStr + ":" + secondStr + " " + amPm, 1);
    lastDay = currentDay;
  }

  // Every 2 seconds, check temp and humidity
  if (currentMode == TEMP_MODE && ((millis() - lastTempCheck) > tempCheckTimerDelay)) {
    humidity = int(round(bme.readHumidity()));
    temp = int(round(1.8 * bme.readTemperature() + 32.0 + tempTolerance));

    printToLCD(F("Indoor:"), 0);
    printToLCD(F("T-") + String(temp) + F("F H-") + String(humidity) + F("%"), 1);
    lastTempCheck = millis();
  }

  // Set weather
  if (currentMode == WEATHER_MODE){
    printToLCD(F("Outdoor: ") + weatherCondition.substring(1, weatherCondition.length()-1), 0);
    printToLCD(F("T-") + String(weatherTemp) + F("F H-") + String(weatherHumidity) + F("%"), 1);
  }

  // Set forecast
  if (currentMode == FORECAST_MODE){
    String min = String(int(round(JSON.stringify(weatherDataList[dayIndex]["temp"]["min"]).toFloat())));
    String max = String(int(round(JSON.stringify(weatherDataList[dayIndex]["temp"]["max"]).toFloat())));
    String condition = JSON.stringify(weatherDataList[dayIndex]["weather"][0]["main"]);
    String dayDisplayed = getDisplayDay(weekday(), dayIndex);

    printToLCD(dayDisplayed + F(" Low-") + min + F("F"), 0);
    printToLCD(F("High-") + max + F("F ") + condition.substring(1, condition.length()-1), 1);
  }

  // Show timer or prompt for new timer
  if (currentMode == TIMER_MODE){
    if (timerEnabled){
      int secondsRemaining = int((timerEnd - millis()) / 1000);
      int hoursRemaining = int(secondsRemaining / 3600);
      secondsRemaining = secondsRemaining % 3600;
      int minsRemaining = int(secondsRemaining / 60);
      secondsRemaining = secondsRemaining % 60;
      printToLCD(F("Time remaining:"), 0);
      printToLCD(getDoubleDigit(hoursRemaining)+":"+getDoubleDigit(minsRemaining)+":"+getDoubleDigit(secondsRemaining), 1);
    } else if (settingHoursTimer){
      printToLCD(F("How many hours?"), 0);
      lcd.setCursor(7, 1);
      lcd.print(timerHours);
    } else if (settingMinutesTimer){
      printToLCD(F("How many mins?"), 0);
      lcd.setCursor(7, 1);
      lcd.print(timerMins);
    } else {
      printToLCD(F("Start a timer?"), 0);
      lcd.setCursor(0, 1);
      lcd.print(F("Click "));
      lcd.write(0x7F); // Left arrow
      lcd.print(F(" and "));
      lcd.write(0x7E); // Right arrow
    }
  }

  // Set alarm
  if (currentMode == ALARM_MODE){
    if (alarmEnabled){
      printToLCD(F("Alarm Set For:"), 0);
      printToLCD(getDoubleDigit(alarmHours)+":"+getDoubleDigit(alarmMins)+" ", 1);
      if (alarmAm){
        lcd.print(F("AM"));
      } else {
        lcd.print(F("PM"));
      }
    } else if (settingHoursAlarm){
      printToLCD(F("At which hour?"), 0);
      lcd.setCursor(7, 1);
      lcd.print(getDoubleDigit(alarmHours));
    } else if (settingMinutesAlarm){
      printToLCD(F("At which minute?"), 0);
      lcd.setCursor(7, 1);
      lcd.print(getDoubleDigit(alarmMins));
    } else if (settingAmpmAlarm){
      printToLCD(F("AM or PM?"), 0);
      lcd.setCursor(7, 1);
      if (alarmAm){
        lcd.print(F("AM"));
      } else {
        lcd.print(F("PM"));
      }
    } else {
      printToLCD(F("Set an alarm?"), 0);
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
    printToLCD(F("Timer ended"), 0);
    alarmTriggered = true;
    timerEnabled = false;
  }
  
  // Trigger alarm if at alarm time
  if (alarmEnabled && hourFormat12() == alarmHours && minute() == alarmMins && ((isAM() && alarmAm) || (isPM() && !alarmAm))){
    lcd.clear();
    printToLCD(F("Alarm triggered"), 0);
    alarmTriggered = true;
    alarmEnabled = false;
  }

  // Adjust temp tolerance at 5, 10 minute mark to account for self-heating
  if (tempTolerance > -2 && millis() - lastToleranceCheck > 300000){
    tempTolerance--;
    lastToleranceCheck = millis();
  }

  // Update weather data hourly
  if ((millis() - lastWeatherUpdate) > weatherUpdateTimerDelay){
    getWeatherData();
    lastWeatherUpdate = millis();
  }
}