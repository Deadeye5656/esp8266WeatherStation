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
unsigned long lastScrollSet = 0;
unsigned long firstScrollSet = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);
int currentMode = 0; // 0 = time, 1 = temp/humidity, 2 = weather, 3 = timer, 4 = alarm

void setup() {
  Serial.begin(9600);

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
      alarmTriggered = false;
      lcd.clear();
      tone(BUZZER, BUZZER_FREQ, 100);
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
      return;
    }
  }

  // Left and right clicked while in timer mode
  if (leftClicked == HIGH && rightClicked == HIGH && currentMode == 3){
    if (!bothClickedTimer){
      bothClickedTimer = true;
      handleDoubleClickTimer();
      tone(BUZZER, BUZZER_FREQ, 100);
    } 
  } else {
    bothClickedTimer = false;
  }

  // Left and right clicked while in alarm mode
  if (leftClicked == HIGH && rightClicked == HIGH && currentMode == 4){
    if (!bothClickedAlarm){
      bothClickedAlarm = true;
      handleDoubleClickAlarm();
      tone(BUZZER, BUZZER_FREQ, 100);
    } 
  } else {
    bothClickedAlarm = false;
  }

  // Left or right click selected while setting timer/alarm minutes or hours, or weather day
  if ((leftClicked == HIGH || rightClicked == HIGH) && 
      (((currentMode == 3 || currentMode == 4)
      && (settingHoursTimer || settingMinutesTimer || settingHoursAlarm || settingMinutesAlarm || settingAmpmAlarm))
      || currentMode == 2)) {
    if (!scrollClicked){
      scrollClicked = true;
      if (currentMode == 2){
        handleWeatherScroll(leftClicked, rightClicked);
      }
      if (currentMode == 3){
        handleTimerScroll(leftClicked, rightClicked);
      }
      if (currentMode == 4){
        handleAlarmScroll(leftClicked, rightClicked);
      }
      firstScrollSet = millis();
      lastScrollSet = millis();
      tone(BUZZER, BUZZER_FREQ, 100);
    } else if (millis() - firstScrollSet > 1500){
      if (millis() - lastScrollSet > 150){
        if (currentMode == 3){
          handleTimerScroll(leftClicked, rightClicked);
        }
        if (currentMode == 4){
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
  if (currentMode == 0){
    String hourStr = getDoubleDigitHour(hour());
    String minuteStr = getDoubleDigit(minute());
    String secondStr = getDoubleDigit(second());
    String amPm = getAMPM();
    printToLCD(String(month()) + "/" + String(day()) + "/" + String(year()).substring(2,4) + " " + getDay(weekday()), 0);
    printToLCD(hourStr + ":" + minuteStr + ":" + secondStr + " " + amPm, 1);
  }

  // Every 2 seconds, check temp and humidity
  if (((millis() - lastTempCheck) > tempCheckTimerDelay) && currentMode == 1) {
    humidity = int(bme.readHumidity());
    temp = int(1.8 * bme.readTemperature() + 32);
    printToLCD(F("Temp-") + String(temp) + F("F"), 0);
    printToLCD(F("Humidity-") + String(humidity) + F("%"), 1);
    lastTempCheck = millis();
  }

  // Set weather
  if (currentMode == 2){
    String min = String(int(weatherDataList[dayIndex]["temp"]["min"]));
    String max = String(int(weatherDataList[dayIndex]["temp"]["max"]));
    String desc = JSON.stringify(weatherDataList[dayIndex]["weather"][0]["main"]);
    String dayDisplayed = getDisplayDay(weekday(), dayIndex);

    printToLCD(dayDisplayed + F(" Low-") + min + F("F"), 0);
    printToLCD(F("High-") + max + F("F ") + desc.substring(1, desc.length()-1), 1);
  }

  // Show timer or prompt for new timer
  if (currentMode == 3){
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
  if (currentMode == 4){
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
    timerEnd = 0;
  }
  
  // Trigger alarm if at alarm time
  if (alarmEnabled && hour() == alarmHours && minute() == alarmMins && ((isAM() && alarmAm) || (isPM() && !alarmAm))){
    lcd.clear();
    printToLCD(F("Alarm triggered"), 0);
    alarmEnabled = false;
    alarmTriggered = true;
  }

  // Update weather data hourly
  if ((millis() - lastWeatherUpdate) > weatherUpdateTimerDelay){
    getWeatherData();
    lastWeatherUpdate = millis();
  }
}