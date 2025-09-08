// ====== Replace with your own credentials ======
const USERNAME = "Adafruit IO username";  // your Adafruit IO username
const AIO_KEY  = "Adafruit IO Key";    // your Adafruit IO Key (keep private!)
// ===============================================

// Connect to Adafruit IO over MQTT (secure WebSocket)
const client = mqtt.connect("wss://io.adafruit.com:443/mqtt", {
  username: USERNAME,
  password: AIO_KEY,
});

client.on("connect", () => {
  console.log("✅ Connected to Adafruit IO");

  // Subscribe to feeds
  client.subscribe(`${USERNAME}/feeds/gas_level`);
  client.subscribe(`${USERNAME}/feeds/humidity`);
  client.subscribe(`${USERNAME}/feeds/temperature`);
  client.subscribe(`${USERNAME}/feeds/Usage_count`);
});

client.on("message", (topic, message) => {
  const value = message.toString();
  console.log(`📡 ${topic}: ${value}`);

  if (topic.endsWith("gas_level")) {
    document.getElementById("aq").innerText = value;
  } else if (topic.endsWith("humidity")) {
    document.getElementById("hum").innerText = value + " %";
  } else if (topic.endsWith("temperature")) {
    document.getElementById("temp").innerText = value + " °C";
  } else if (topic.endsWith("Usage_count")) {
    document.getElementById("usage").innerText = value;
  }
});
