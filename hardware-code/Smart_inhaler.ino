// Removed ML / Edge Impulse parts

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "AdafruitIO_WiFi.h"
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

// WiFi
const char* ssid = "Balas_MOTO";
const char* password = "Jaya@555";

// Adafruit IO
#define WIFI_SSID       "Balas_MOTO"
#define WIFI_PASS       "Jaya@555"
#define AIO_USERNAME    "AIO_USERNAME"
#define AIO_KEY         "AIO_KEY"

AdafruitIO_WiFi io(AIO_USERNAME, AIO_KEY, WIFI_SSID, WIFI_PASS);

// Feeds (ML removed)
AdafruitIO_Feed *gasLevelFeed     = io.feed("gas_level");
AdafruitIO_Feed *usageFeed        = io.feed("usage_count");
AdafruitIO_Feed *alertFeed        = io.feed("Alert");
AdafruitIO_Feed *temperatureFeed  = io.feed("Temperature");
AdafruitIO_Feed *humidityFeed     = io.feed("Humidity");

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Sensors
#define GAS_SENSOR_PIN 34
#define BUTTON_PIN     26
#define BUZZER_PIN     27
#define DHTPIN         14
#define DHTTYPE        DHT11

DHT dht(DHTPIN, DHTTYPE);

// Constants
int gasThresholdPPM = 800;
const int totalPuffs = 200;
int usageCount = 0;

// Debounce
bool lastButtonStableState = HIGH;
bool lastButtonReading = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 20;

// Buzzer
unsigned long lastBeepTime = 0;
const unsigned long beepInterval = 500;
bool buzzerState = false;

// Upload timing
unsigned long lastSensorUploadTime = 0;    
const unsigned long sensorUploadInterval = 20000;

// Last values
int lastUsageCount = -1;
String lastAlert = "";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  ArduinoOTA.begin();

  dht.begin();
  pinMode(GAS_SENSOR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  io.connect();
  while (io.status() < AIO_CONNECTED) {
    io.run();
    delay(500);
  }
}

void loop() {
  ArduinoOTA.handle();
  io.run();

  float gasPPM = (analogRead(GAS_SENSOR_PIN) / 4095.0) * 1000.0;
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  int remainingPuffs = totalPuffs - usageCount;
  if (remainingPuffs < 0) remainingPuffs = 0;

  // Air quality logic (kept original simple logic)
  String quality;
  int level;

  if (gasPPM <= 800) { quality = "Good"; level = 1; }
  else if (gasPPM <= 1200) { quality = "Moderate"; level = 2; }
  else if (gasPPM <= 2000) { quality = "Poor"; level = 3; }
  else if (gasPPM <= 3000) { quality = "Very Poor"; level = 4; }
  else { quality = "Hazardous"; level = 5; }

  // Alerts
  bool isGasBad = (gasPPM > gasThresholdPPM);
  bool isTempHigh = (temperature > 40);
  bool isTempLow = (temperature < 10);
  bool isHumHigh = (humidity >= 90);
  bool isHumLow = (humidity < 25);
  bool isLowPuffs = (usageCount > 170);

  String alertMsg = "None";
  bool isDanger = false;

  if (isGasBad) { alertMsg = "Alert-1"; isDanger = true; }
  if (isTempHigh || isTempLow) { alertMsg = "Alert-2"; isDanger = true; }
  if (isHumHigh || isHumLow) { alertMsg = "Alert-3"; isDanger = true; }
  else if (isLowPuffs) { alertMsg = "Low Puff Alert!"; }

  // OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("SMART INHALER");
  display.setCursor(0, 10);
  display.print("Gas: "); display.print((int)gasPPM);
  display.setCursor(0, 20);
  display.print("Used: "); display.print(usageCount);
  display.setCursor(0, 30);
  display.print("Temp: "); display.print(temperature);
  display.setCursor(0, 40);
  display.print("Hum: "); display.print(humidity);
  display.setCursor(0, 50);
  display.print("Alert: "); display.print(alertMsg);
  display.display();

  // Buzzer
  if (isDanger) {
    if (millis() - lastBeepTime > beepInterval) {
      lastBeepTime = millis();
      buzzerState = !buzzerState;
      if (buzzerState) tone(BUZZER_PIN, 2000);
      else noTone(BUZZER_PIN);
    }
  } else {
    noTone(BUZZER_PIN);
  }

  // Upload
  if (io.status() == AIO_CONNECTED) {
    unsigned long currentMillis = millis();

    if (currentMillis - lastSensorUploadTime > sensorUploadInterval) {
      gasLevelFeed->save(gasPPM);
      temperatureFeed->save(temperature);
      humidityFeed->save(humidity);
      lastSensorUploadTime = currentMillis;
    }

    if (usageCount != lastUsageCount) {
      usageFeed->save(usageCount);
      lastUsageCount = usageCount;
    }

    if (alertMsg != lastAlert) {
      alertFeed->save(alertMsg);
      lastAlert = alertMsg;
    }
  }

  // Button
  bool currentReading = digitalRead(BUTTON_PIN);

  if (currentReading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  lastButtonReading = currentReading;

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (lastButtonStableState == HIGH && currentReading == LOW) {
      usageCount++;
      if (usageCount > totalPuffs) usageCount = totalPuffs;
    }
    lastButtonStableState = currentReading;
  }
}
