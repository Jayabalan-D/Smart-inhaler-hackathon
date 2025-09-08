#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "AdafruitIO_WiFi.h"
#include <DHT.h>
#include <Adafruit_Sensor.h>

// WiFi & Adafruit IO credentials
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define AIO_USERNAME  "YOUR_ADAFRUIT_USERNAME"
#define AIO_KEY       "YOUR_ADAFRUIT_KEY"

AdafruitIO_WiFi io(AIO_USERNAME, AIO_KEY, WIFI_SSID, WIFI_PASS);

// Adafruit IO feeds (note the feed names)
AdafruitIO_Feed *gasLevelFeed     = io.feed("gas_level");
AdafruitIO_Feed *usageFeed        = io.feed("usage_count");
AdafruitIO_Feed *alertFeed        = io.feed("Alert");
AdafruitIO_Feed *temperatureFeed  = io.feed("Temperature");
AdafruitIO_Feed *humidityFeed     = io.feed("Humidity");

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

// Constants
int gasThresholdPPM = 600;
const int totalPuffs = 200;
int usageCount = 0;

// Debounce variables
bool lastButtonStableState = HIGH;
bool lastButtonReading = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 20;  // faster response

// Buzzer timing
unsigned long lastBeepTime = 0;
const unsigned long beepInterval = 500;
bool buzzerState = false;

// Upload Interval - increased to 20 seconds to avoid throttling
unsigned long lastUploadTime = 0;
const unsigned long uploadInterval = 20000;  // 20,000 milliseconds

// To track last uploaded values and avoid redundant uploads
float lastGasPPM = -1;
int lastUsageCount = -1;
float lastTemperature = -1000;
float lastHumidity = -1;
String lastAlert = "";

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(GAS_SENSOR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

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

  Serial.print("Connecting to Adafruit IO");
  io.connect();

  unsigned long connectStart = millis();
  while(io.status() < AIO_CONNECTED && millis() - connectStart < 20000) {
    io.run();
    Serial.print(".");
    delay(500);
  }

  if (io.status() == AIO_CONNECTED) Serial.println("\nConnected to Adafruit IO!");
  else Serial.println("\nFailed to connect to Adafruit IO, continuing offline");
}

void loop() {
  io.run();

  // Read sensors
  float gasPPM      = (analogRead(GAS_SENSOR_PIN) / 4095.0) * 1000.0;
  float humidity    = dht.readHumidity();
  float temperature = dht.readTemperature();
  int remainingPuffs = totalPuffs - usageCount;
  if (remainingPuffs < 0) remainingPuffs = 0;

  // Alert logic
  bool isGasBad    = (gasPPM > gasThresholdPPM);
  bool isTempHigh  = (temperature > 40);
  bool isTempLow   = (temperature < 10);
  bool isHumHigh   = (humidity >= 90);
  bool isHumLow    = (humidity < 25);
  bool isLowPuffs  = (usageCount > 170);

  String alertMsg = "";
  bool isDanger = false;

  if (isGasBad) {
    alertMsg = "Alert-1";
    isDanger = true;
  } 

  if (isTempHigh || isTempLow) {
    alertMsg = "Alert-2";
    isDanger = true;
  } 
  
  if (isHumHigh || isHumLow) {
    alertMsg = "Alert-3";
    isDanger = true;
  } 
  
  else if (isLowPuffs) {
    alertMsg = "Low Puff Alert!";
  } else {
    alertMsg = "None";
  }

  // Serial output
  Serial.println("----SMART INHALER----");
  Serial.print("Gas               : "); Serial.print((int)gasPPM); Serial.println(" ppm");
  Serial.print("Used              : "); Serial.print(usageCount); Serial.print("/"); Serial.println(totalPuffs);
  Serial.print("Temperature  : "); Serial.print(temperature,1); Serial.println(" *C");
  Serial.print("Humidity       : "); Serial.print(humidity,1); Serial.println(" %");
  Serial.print("Alert             : "); Serial.println(alertMsg);
  Serial.println("----------------------");

  // OLED Display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);  display.println("----SMART INHALER----");
  display.setCursor(0, 10); display.print("Gas : "); display.print((int)gasPPM); display.print(" ppm");
  display.setCursor(0, 20); display.print("Used : "); display.print(usageCount); display.print("/"); display.print(totalPuffs);
  display.setCursor(0, 30); display.print("Temperature : "); display.print(temperature,1); display.print(" *C");
  display.setCursor(0, 40); display.print("Humidity : "); display.print(humidity,1); display.print(" %");
  display.setCursor(0, 50); display.print("Alert : "); display.print(alertMsg);
  display.display();

  // Buzzer only on danger alert
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

  // Upload data every 20 seconds if values changed
  if (io.status() == AIO_CONNECTED && (millis() - lastUploadTime > uploadInterval)) {
    bool anyChange = false;

    if (abs(gasPPM - lastGasPPM) > 1.0) {
      gasLevelFeed->save(gasPPM);
      lastGasPPM = gasPPM;
      anyChange = true;
    }
    if (usageCount != lastUsageCount) {
      usageFeed->save(usageCount);
      lastUsageCount = usageCount;
      anyChange = true;
    }
    if (abs(temperature - lastTemperature) > 0.5) {
      temperatureFeed->save(temperature);
      lastTemperature = temperature;
      anyChange = true;
    }
    if (abs(humidity - lastHumidity) > 1.0) {
      humidityFeed->save(humidity);
      lastHumidity = humidity;
      anyChange = true;
    }
    if (alertMsg != lastAlert) {
      alertFeed->save(alertMsg);
      lastAlert = alertMsg;
      anyChange = true;
    }
    if (anyChange) lastUploadTime = millis();
  }

  // Button debounce & usage count increment (no beep on press)
  bool currentReading = digitalRead(BUTTON_PIN);
  if (currentReading != lastButtonReading) lastDebounceTime = millis();
  lastButtonReading = currentReading;
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (lastButtonStableState == HIGH && currentReading == LOW) {
      usageCount++;
      if (usageCount > totalPuffs) usageCount = totalPuffs;
      if (io.status() == AIO_CONNECTED) usageFeed->save(usageCount); // instant update on press
      Serial.println("Inhaler Used!");
    }
    lastButtonStableState = currentReading;
  }
}
