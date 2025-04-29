void printToLCD(String line, boolean lineNum){
  if (lineNum == 0){
    lcd.setCursor(0,0);
  } else{
    lcd.setCursor(0,1);
  }
  lcd.print(line);
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