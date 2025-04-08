#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <functional>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Arduino_JSON.h>
#include <SoftwareSerial.h>

char c;
String dataIn;

const char* ssid     = "WIFI_SSID"; //enter your ssid/ wi-fi(case sensitive) router name - 2.4 Ghz only
const char* password = "WIFI_PASSWORD";     // enter ssid password (case sensitive)

String openWeatherMapApiKey = "OPEN_WEATHER_API_KEY";
String timezoneApiKey = "TIMEZONE_API_KEY";
// Royal Oak
String lat = "42.49";
String lon = "-83.14";
String unit = "imperial";
String dayCount = "7";

boolean initialCall = true;

unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
// Check hourly
unsigned long timerDelay = 3600000;
// Set timer to 10 seconds (10000)
// unsigned long timerDelay = 10000;

String jsonBuffer;
String jsonBuilder = "";
JSONVar jsonResponse;
JSONVar jsonResponseTime;

unsigned long unixTime;
String day;

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);
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
        Serial.println("Parsing input failed!");
        return;
      }

      unixTime = long(jsonResponseTime["timestamp"]);
      day = dow(unixTime);
    }
    else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }

  jsonBuilder = "<{\"day\":\""+day+"\",\"list\":[{";
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

  Serial1.println(jsonBuilder);
  delay(500);
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
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}
