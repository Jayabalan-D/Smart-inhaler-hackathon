Smart Inhaler with Air Quality Monitoring

An IoT-based smart inhaler that monitors air quality (MQ135), temperature & humidity (DHT11), and tracks inhaler usage.  
Data is processed with an Edge Impulse ML model and visualized through a web dashboard.

---

🔧 Features
- Real-time air quality monitoring (Good / Moderate / Risk)  
- Inhaler dose tracking  
- Edge Impulse ML model integration  
- Web dashboard (HTML/CSS/JS)  

---

⚙️ Tech Stack
- Hardware: ESP32/ESP8266, MQ135, DHT11, OLED  
- Software: Arduino IDE, Edge Impulse, HTML/CSS/JS  

---

🚀 Setup
1. Flash `hardware-code/main.ino` to ESP board  
2. Add WiFi credentials in `credentials.h` (copy from `credentials_template.h`)  
3. Open `web-app/index.html` to view dashboard  

---

📊 Air Quality Levels
- Good (0) : 0–400 ppm  
- Moderate (1) : 400–600 ppm  
- Risk (2) : 600–900 ppm  

---

📜 License
Licensed under the MIT License.

