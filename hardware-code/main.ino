#include <MQ135_Air_Quality_Classification_inferencing.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "AdafruitIO_WiFi.h"
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

// Replace with your Wi-Fi credentials
const char* ssid = "Balas_MOTO";
const char* password = "Jaya@555";

// WiFi & Adafruit IO credentials
#define WIFI_SSID       "Balas_MOTO"
#define WIFI_PASS       "Jaya@555"
#define AIO_USERNAME    "AIO_USERNAME"
#define AIO_KEY         "AIO_KEY"

AdafruitIO_WiFi io(AIO_USERNAME, AIO_KEY, WIFI_SSID, WIFI_PASS);

// Adafruit IO feeds
AdafruitIO_Feed *gasLevelFeed     = io.feed("gas_level");
AdafruitIO_Feed *usageFeed        = io.feed("usage_count");
AdafruitIO_Feed *alertFeed        = io.feed("Alert");
AdafruitIO_Feed *temperatureFeed  = io.feed("Temperature");
AdafruitIO_Feed *humidityFeed     = io.feed("Humidity");
AdafruitIO_Feed *mlResultFeed     = io.feed("ML_result");

// OLED setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Sensors & Pins
#define GAS_SENSOR_PIN 34
#define BUTTON_PIN     26
#define BUZZER_PIN     27
#define DHTPIN         14
#define DHTTYPE        DHT11

DHT dht(DHTPIN, DHTTYPE);

// MQ135 ML model
MQ135_Air_Quality_Classification_inferencing model;

// Constants
int gasThresholdPPM = 800;
const int totalPuffs = 200;
int usageCount = 0;

// Debounce variables
bool lastButtonStableState = HIGH;
bool lastButtonReading = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 20;

// Buzzer timing
unsigned long lastBeepTime = 0;
const unsigned long beepInterval = 500;
bool buzzerState = false;

// Upload timers
unsigned long lastSensorUploadTime = 0;    
const unsigned long sensorUploadInterval = 20000; // 20 sec

// Last uploaded values
float lastGasPPM = -1;
int lastUsageCount = -1;
float lastTemperature = -1000;
float lastHumidity = -1;
String lastAlert = "";
String lastMLResult = "";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  // Wait for Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected");

  // OTA setup
  ArduinoOTA.onStart([]() { Serial.println("Start updating..."); });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA Update Finished"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) { Serial.printf("Error[%u]\n", error); });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  Serial.print("IP Address: "); Serial.println(WiFi.localIP());

  // Sensors setup
  dht.begin();
  pinMode(GAS_SENSOR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  // OLED setup
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED not found"));
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("----SMART INHALER----");
  display.display();
  delay(2000);

  // Connect to Adafruit IO
  Serial.print("Connecting to Adafruit IO");
  io.connect();
  unsigned long connectStart = millis();
  while (io.status() < AIO_CONNECTED && millis() - connectStart < 20000) {
    io.run();
    Serial.print(".");
    delay(500);
  }
  if (io.status() == AIO_CONNECTED) Serial.println("\nConnected!");
  else Serial.println("\nFailed to connect, running offline");
}

void loop() {
  ArduinoOTA.handle();
  io.run();

  // Read sensors
  float gasPPM      = (analogRead(GAS_SENSOR_PIN) / 4095.0) * 1000.0;
  float humidity    = dht.readHumidity();
  float temperature = dht.readTemperature();
  int remainingPuffs = totalPuffs - usageCount;
  if (remainingPuffs < 0) remainingPuffs = 0;

  // ML inference
  MQ135_Air_Quality_Classification_inferencing::Prediction mlResult = model.predict(gasPPM);
  String mlQuality;
  switch(mlResult.classIndex) {
    case 0: mlQuality = "Good 🌿"; break;
    case 1: mlQuality = "Moderate 😐"; break;
    case 2: mlQuality = "Poor 😷"; break;
    default: mlQuality = "Unknown"; break;
  }

  // Alerts
  bool isGasBad    = (gasPPM > gasThresholdPPM);
  bool isTempHigh  = (temperature > 40);
  bool isTempLow   = (temperature < 10);
  bool isHumHigh   = (humidity >= 90);
  bool isHumLow    = (humidity < 25);
  bool isLowPuffs  = (usageCount > 170);

  // Air quality level for original logic
  String quality;
  int level;
  if (gasPPM <= 800) { quality = "Good 🌿"; level = 1; }
  else if (gasPPM <= 1200) { quality = "Moderate 😐"; level = 2; }
  else if (gasPPM <= 2000) { quality = "Poor 😷"; level = 3; }
  else if (gasPPM <= 3000) { quality = "Very Poor ☠️"; level = 4; }
  else { quality = "Hazardous 🚨"; level = 5; }

  // Alert message
  String alertMsg = "None";
  bool isDanger = false;
  if (isGasBad) { alertMsg = "Alert-1"; isDanger = true; }
  if (isTempHigh || isTempLow) { alertMsg = "Alert-2"; isDanger = true; }
  if (isHumHigh || isHumLow) { alertMsg = "Alert-3"; isDanger = true; }
  else if (isLowPuffs) { alertMsg = "Low Puff Alert!"; }

  // Serial monitor
  Serial.println("----SMART INHALER----");
  Serial.print("Air Quality       : "); Serial.print(quality);
  Serial.print(" ("); Serial.print(gasPPM); Serial.println(" ppm)");
  Serial.print("Used              : "); Serial.print(usageCount); Serial.print("/"); Serial.println(totalPuffs);
  Serial.print("Temperature       : "); Serial.print(temperature,1); Serial.println(" *C");
  Serial.print("Humidity          : "); Serial.print(humidity,1); Serial.println(" %");
  Serial.print("Alert             : "); Serial.println(alertMsg);
  Serial.print("ML Prediction     : "); Serial.println(mlQuality);
  Serial.println("----------------------");

  // OLED display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);  display.println("----SMART INHALER----");
  display.setCursor(0, 10); display.print("Gas     : "); display.print((int)gasPPM); display.print(" ppm - "); display.print(level);
  display.setCursor(0, 20); display.print("Used        : "); display.print(usageCount); display.print("/"); display.print(totalPuffs);
  display.setCursor(0, 30); display.print("Temperature : "); display.print(temperature,1); display.print(" "); display.print("\xB0"); display.print("C");
  display.setCursor(0, 40); display.print("Humidity    : "); display.print(humidity,1); display.print(" %");
  display.setCursor(0, 50); display.print("Alert       : "); display.print(alertMsg);
  display.setCursor(0, 60); display.print("ML Result  : "); display.print(mlQuality);
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

  // ✅ Strict Upload Logic
  if (io.status() == AIO_CONNECTED) {
    unsigned long currentMillis = millis();

    // PPM, Temperature, Humidity, ML result -> strictly every 20s
    if (currentMillis - lastSensorUploadTime > sensorUploadInterval) {
      gasLevelFeed->save(gasPPM);
      temperatureFeed->save(temperature);
      humidityFeed->save(humidity);
      mlResultFeed->save(mlQuality);
      lastSensorUploadTime = currentMillis;
    }

    // usageCount -> upload instantly
    if (usageCount != lastUsageCount) {
      usageFeed->save(usageCount);
      lastUsageCount = usageCount;
    }

    // Alert -> upload instantly
    if (alertMsg != lastAlert) {
      alertFeed->save(alertMsg);
      lastAlert = alertMsg;
    }
  }

  // Button handling
  bool currentReading = digitalRead(BUTTON_PIN);
  if (currentReading != lastButtonReading) lastDebounceTime = millis();
  lastButtonReading = currentReading;
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (lastButtonStableState == HIGH && currentReading == LOW) {
      usageCount++;
      if (usageCount > totalPuffs) usageCount = totalPuffs;
      if (io.status() == AIO_CONNECTED) usageFeed->save(usageCount);
      Serial.println("Inhaler Used!");
    }
    lastButtonStableState = currentReading;
  }
}
