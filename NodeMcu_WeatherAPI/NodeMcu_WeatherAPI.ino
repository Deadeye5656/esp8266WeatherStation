#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <functional>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Arduino_JSON.h>
#include <SoftwareSerial.h>

#define ARDUINO_RESET 4
#define RESET 0

const char* ssid     = "WIFI_SSID"; //enter your ssid/ wi-fi(case sensitive) router name - 2.4 Ghz only
const char* password = "WIFI_PASSWORD";     // enter ssid password (case sensitive)

String openWeatherMapApiKey = "OPEN_WEATHER_API_KEY";
String timezoneApiKey = "TIMEZONE_API_KEY";
// Royal Oak
String lat = "42.49";
String lon = "-83.14";
String unit = "imperial";
String dayCount = "7";

String jsonBuffer;
String jsonBuilder = "";
JSONVar jsonResponse;
JSONVar jsonResponseTime;

unsigned long lastTime = 0;
// Check hourly
unsigned long timerDelay = 3600000;


unsigned long unixTime;
String day;
char sendBit;
boolean initialCall = true;

void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  pinMode(ARDUINO_RESET, INPUT);
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, HIGH);
 }

void loop() {
  // Send an HTTP GET request
  if (((millis() - lastTime) > timerDelay) || initialCall) {
    initialCall = false;
    if(WiFi.status()== WL_CONNECTED){
      String serverPath = "http://api.openweathermap.org/data/2.5/forecast/daily?lat=" + lat + "&lon=" + lon + "&units=" + unit + "&cnt=" + dayCount + "&appid=" + openWeatherMapApiKey;
      
      jsonBuffer = httpGETRequest(serverPath.c_str());
      jsonResponse = JSON.parse(jsonBuffer);

      serverPath = "http://api.timezonedb.com/v2.1/get-time-zone?key=" + timezoneApiKey + "&format=json&by=position&lat="+ lat + "&lng="+ lon;
      
      jsonBuffer = httpGETRequest(serverPath.c_str());
      jsonResponseTime = JSON.parse(jsonBuffer);
  
      if (JSON.typeof(jsonResponse) == "undefined" || JSON.typeof(jsonResponseTime) == "undefined") {
        initialCall = true;
        return;
      }

      unixTime = long(jsonResponseTime["timestamp"]);
      day = dow(unixTime);
      jsonBuilder = "{\"day\":\""+day+"\",\"list\":[{";
      for (int i = 0; i < dayCount.toInt(); i++){
        String min = JSON.stringify(int(jsonResponse["list"][i]["temp"]["min"]));
        String max = JSON.stringify(int(jsonResponse["list"][i]["temp"]["max"]));
        String desc = JSON.stringify(jsonResponse["list"][i]["weather"][0]["main"]);
        jsonBuilder = jsonBuilder + "\"min\":\"" + min + "\",";
        jsonBuilder = jsonBuilder + "\"max\":\"" + max + "\",";
        jsonBuilder = jsonBuilder + "\"desc\":" + desc + "}";
        if (i != dayCount.toInt()-1){
          jsonBuilder = jsonBuilder + ",{";
        }
      }
      jsonBuilder = jsonBuilder + "]}>";
    }
    lastTime = millis();
  }

  Serial.println(jsonBuilder);
  delay(500);
  int buttonState = digitalRead(ARDUINO_RESET);
  if (buttonState == HIGH) {
    ESP.deepSleep(36e8 - 5e6); // 5 seconds before 1 hour mark
  }
}

String dow(unsigned long t)
{
    return String((((t / 86400) + 3) % 7) + 1);
}

String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;
    
  // Your IP address with path or Domain name with URL path 
  http.begin(client, serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    payload = http.getString();
  }
  // Free resources
  http.end();

  return payload;
}
