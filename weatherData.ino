#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

const char* ssid     = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";

String openWeatherMapApiKey = "OPEN_WEATHER_API_KEY";
String timezoneApiKey = "TIMEZONE_API_KEY";
// Royal Oak, MI
String lat = "42.49";
String lon = "-83.14";
String unit = "imperial";
String dayCount = "7";

void getWeatherData(){
  lcd.clear();
  printToLCD(F("Fetching Updated"), 0);
  printToLCD(F("Weather Data"), 1);

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