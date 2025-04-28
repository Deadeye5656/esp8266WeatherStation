void handleWeatherScroll(boolean leftClicked, boolean rightClicked){
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

void handleTimerScroll(boolean leftClicked, boolean rightClicked){
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

void handleAlarmScroll(boolean leftClicked, boolean rightClicked){
  // Clear previously shown hours/minutes
  lcd.setCursor(7, 1);
  lcd.print("  ");
  if (settingHoursAlarm){
    if (leftClicked){
      if (alarmHours == 1) {
        alarmHours = 12;
      } else {
        alarmHours--;
      }
    } else if (rightClicked){
      if (alarmHours == 12) {
        alarmHours = 1;
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

void handleDoubleClickTimer(){
  if (timerEnabled){ // if timer is counting down and there is a double click, then cancel timer
    timerEnabled = false;
  } else if (!settingHoursTimer && !settingMinutesTimer){
    settingHoursTimer = true;
    scrollClicked = true; // this avoids immediately changing hours after double click
  }
  lcd.clear();
}

void handleDoubleClickAlarm(){
  if (alarmEnabled){ // if timer is counting down and there is a double click, then cancel timer
    alarmEnabled = false;
  } else if (!settingHoursAlarm && !settingMinutesAlarm && !settingAmpmAlarm){
    settingHoursAlarm = true;
    scrollClicked = true; // this avoids immediately changing hours after double click
  }
  lcd.clear();
}

void handleModeClick(){
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
}