#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// OpenWeatherMap API settings
const char* apiKey = "YOUR_API_KEY";
const char* city = "YOUR_CITY";
const char* countryCode = "YOUR_COUNTRY_CODE";

// NTP settings
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // Update every minute

// Weather variables
String weatherDescription = "";
float temperature = 0.0;
int humidity = 0;

// Timing variables
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherUpdateInterval = 600000; // 10 minutes
unsigned long lastTimeUpdate = 0;
const unsigned long timeUpdateInterval = 100; // 0.1 second for smoother display

void setup() {
  Serial.begin(115200);
  delay(100);
  
  // Initialize I2C
  Wire.begin(0, 2); // SDA=GPIO0, SCL=GPIO2 for ESP-01
  
  // Try different I2C addresses
  Serial.println("Scanning for I2C devices...");
  for(byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print("I2C device found at address 0x");
      Serial.println(address, HEX);
    }
  }
  
  // Initialize display - try 0x3C first, then 0x3D
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Failed with 0x3C, trying 0x3D...");
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println(F("SSD1306 allocation failed"));
      for(;;);
    }
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Starting...");
  display.display();
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("WiFi Connected!");
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.display();
    delay(2000);
  } else {
    Serial.println("WiFi connection failed!");
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("WiFi Failed!");
    display.display();
    delay(2000);
  }
  
  // Initialize NTP client
  timeClient.begin();
  timeClient.setTimeOffset(HOUR_OFFSET_IN_SECONDS); // # Hour offset in seconds. For example GMT+1 = 3600.
  
  // Get initial weather data
  getWeatherData();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Update time every 0.1 seconds
  if (currentMillis - lastTimeUpdate >= timeUpdateInterval) {
    timeClient.update();
    lastTimeUpdate = currentMillis;
  }
  
  // Update weather every 10 minutes
  if (currentMillis - lastWeatherUpdate >= weatherUpdateInterval) {
    getWeatherData();
    lastWeatherUpdate = currentMillis;
  }
  
  // Update display
  updateDisplay();
  
  delay(50); // Reduced delay for smoother updates
}

void getWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + 
                 String(city) + "," + String(countryCode) + 
                 "&appid=" + String(apiKey) + "&units=metric";
    
    http.begin(client, url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println("Weather API Response: " + payload);
      
      // Parse JSON
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      
      temperature = doc["main"]["temp"];
      humidity = doc["main"]["humidity"];
      weatherDescription = doc["weather"][0]["description"].as<String>();
      
      Serial.println("Weather updated successfully");
    } else {
      Serial.println("Error getting weather data: " + String(httpResponseCode));
    }
    
    http.end();
  }
}

void updateDisplay() {
  display.clearDisplay();
  
  // Get current time
  time_t rawtime = timeClient.getEpochTime();
  struct tm * timeinfo = localtime(&rawtime);
  
  // Format date (DD/MM/YYYY)
  char dateStr[12];
  sprintf(dateStr, "%02d/%02d/%04d", 
          timeinfo->tm_mday,
          timeinfo->tm_mon + 1, 
          timeinfo->tm_year + 1900);
  
  // Format time (H:MM:SS) - removed milliseconds to fit better
  char timeStr[10];
  sprintf(timeStr, "%d:%02d:%02d", 
          timeinfo->tm_hour, 
          timeinfo->tm_min, 
          timeinfo->tm_sec);
  
  // Display date at top center (small text)
  display.setTextSize(1);
  int dateWidth = strlen(dateStr) * 6; // Approximate width calculation
  int dateX = (SCREEN_WIDTH - dateWidth) / 2;
  display.setCursor(dateX, 2);
  display.println(dateStr);
  
  // Display time in center (larger text)
  display.setTextSize(2);
  int timeWidth = strlen(timeStr) * 12; // Approximate width for size 2 text
  int timeX = (SCREEN_WIDTH - timeWidth) / 2;
  display.setCursor(timeX, 20);
  display.println(timeStr);
  
  // Display temperature on left side (below clock)
  display.setTextSize(1);
  display.setCursor(5, 45);
  if (temperature != 0.0) {
    display.print(temperature, 1);
    display.print("C");
  } else {
    display.print("--.-C");
  }
  
  // Display humidity on right side (below clock)
  display.setCursor(85, 45);
  if (humidity != 0) {
    display.print(humidity);
    display.print("%");
  } else {
    display.print("--%");
  }
  
  // Display weather description at bottom center
  if (weatherDescription != "") {
    display.setCursor(0, 55);
    String desc = weatherDescription;
    if (desc.length() > 21) {
      desc = desc.substring(0, 18) + "...";
    }
    // Center the weather description
    int descWidth = desc.length() * 6;
    int descX = (SCREEN_WIDTH - descWidth) / 2;
    display.setCursor(descX, 55);
    display.println(desc);
  }
  
  // WiFi status indicator (top right corner)
  display.setCursor(120, 2);
  if (WiFi.status() == WL_CONNECTED) {
    display.println("W");
  } else {
    display.println("X");
  }
  
  display.display();
}
