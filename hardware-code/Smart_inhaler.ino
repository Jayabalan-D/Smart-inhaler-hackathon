#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>

// OLED setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT22 setup
#define DHTPIN 14
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Pins
#define GAS_SENSOR_PIN 34
#define BUTTON_PIN 26
#define BUZZER_PIN 27

// Buzzer PWM channel
#define BUZZER_CHANNEL 0

// Thresholds
int gasThresholdPPM = 600;  // Threshold in PPM
int usageCount = 0;

const int totalPuffs = 200; // Total inhaler capacity

// Improved debounce variables
bool lastButtonStableState = HIGH;  // For debounced state
bool lastButtonReading = HIGH;      // Last raw reading
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// For buzzer beep timing
unsigned long lastBeepTime = 0;
const unsigned long beepInterval = 500;  // 500ms interval between beeps
bool buzzerState = false;  // To toggle buzzer on and off

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(GAS_SENSOR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Setup buzzer channel for PWM
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED not found"));
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Smart Inhaler Start");
  display.display();
  delay(2000);
}

void loop() {
  float gasPPM = (analogRead(GAS_SENSOR_PIN) / 4095.0) * 1000.0;
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  int remainingPuffs = totalPuffs - usageCount;

  // Print to Serial Monitor with warnings
  Serial.print("Gas PPM: ");
  Serial.print(gasPPM, 1);
  if (gasPPM > gasThresholdPPM) {
    Serial.print("  [WARNING: Poor Air Quality!]");
  } else if (gasPPM < 50) {
    Serial.print("  [WARNING: Sensor may be disconnected or reading abnormally low!]");
  }
  Serial.println();

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" *C");
  if (temperature > 40) {
    Serial.print("  [WARNING: Temperature too HIGH!]");
  } else if (temperature < 10) {
    Serial.print("  [WARNING: Temperature too LOW!]");
  }
  Serial.println();

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print(" %");
  if (humidity > 80) {
    Serial.print("  [WARNING: Humidity too HIGH!]");
  } else if (humidity < 30) {
    Serial.print("  [WARNING: Humidity too LOW!]");
  }
  Serial.println();

  Serial.print("Usage Count: ");
  Serial.println(usageCount);

  Serial.print("Remaining Puffs: ");
  Serial.println(remainingPuffs);

  // Display on OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Gas: ");
  display.print(gasPPM, 1);
  display.println(" ppm");

  display.print("Temp: ");
  display.print(temperature);
  display.println(" C");

  display.print("Hum: ");
  display.print(humidity);
  display.println(" %");

  display.print("Used: ");
  display.print(usageCount);
  display.print("/");
  display.println(totalPuffs);

  display.print("Remaining: ");
  display.println(remainingPuffs);

  // Replace continuous buzzer tone with beep pattern when gas is poor
  if (gasPPM > gasThresholdPPM) {
    if (millis() - lastBeepTime >= beepInterval) {
      lastBeepTime = millis();
      buzzerState = !buzzerState;
      if (buzzerState) {
        ledcWriteTone(BUZZER_CHANNEL, 2000); // 2kHz buzzer sound ON
      } else {
        ledcWriteTone(BUZZER_CHANNEL, 0);    // buzzer OFF
      }
    }
    display.println("Air: Poor!");
  } else {
    ledcWriteTone(BUZZER_CHANNEL, 0);
    display.println("Air: OK");
  }

  // Improved Debounced Button Press Logic (active LOW)
  bool currentReading = digitalRead(BUTTON_PIN);
  if (currentReading != lastButtonReading) {
    lastDebounceTime = millis();
  }
  lastButtonReading = currentReading;

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if ((lastButtonStableState == HIGH) && (currentReading == LOW)) {
      usageCount++;
      if (usageCount > totalPuffs) usageCount = totalPuffs;

      // Short beep for button press
      ledcWriteTone(BUZZER_CHANNEL, 1000);
      delay(200);
      ledcWriteTone(BUZZER_CHANNEL, 0);

      Serial.println("Inhaler Used!");
    }
    lastButtonStableState = currentReading;
  }

  display.display();
  delay(50);
}
