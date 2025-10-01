// ====== Replace with your own credentials ======
const USERNAME = "Your username";
const AIO_KEY  = "Your Key";
// ===============================================

const client = mqtt.connect("wss://io.adafruit.com:443/mqtt", {
  username: USERNAME,
  password: AIO_KEY,
});

// Section switching
function showSection(id) {
  document.querySelectorAll(".section").forEach(sec => sec.classList.remove("active"));
  document.getElementById(id).classList.add("active");
}

// Thresholds
const THRESHOLDS = {
  ppm: { min: 0, max: 600 },
  humidity: { min: 25, max: 90 },
  temperature: { min: 10, max: 40 }
};

// Create Chart
function createChart(ctx) {
  return new Chart(ctx, {
    type: "line",
    data: {
      labels: [],
      datasets: [
        { label: "PPM", data: [], borderColor: "#d2691e", fill: false },
        { label: "Humidity", data: [], borderColor: "#4b5320", fill: false },
        { label: "Temperature", data: [], borderColor: "#e75480", fill: false },
      ]
    },
    options: { responsive: true, maintainAspectRatio: false }
  });
}

const liveChart = createChart(document.getElementById("liveChart"));

// Check Thresholds
function checkThreshold(param, value) {
  const { min, max } = THRESHOLDS[param];
  return value >= min && value <= max;
}

// Log abnormal events
function logEvent(param, value) {
  const tbody = document.getElementById("event-log");
  const row = document.createElement("tr");
  const now = new Date().toLocaleString();
  row.innerHTML = `<td>${now}</td><td>${param}</td><td>${value}</td>`;
  tbody.prepend(row);
}

// ===== Persistent Alert Logic =====
let latestValues = { ppm: null, humidity: null, temperature: null };

function updateAlerts() {
  let alertMessages = [];

  if (latestValues.ppm !== null && !checkThreshold("ppm", latestValues.ppm)) {
    alertMessages.push("Air Quality (PPM)!");
    logEvent("ppm", latestValues.ppm);
    document.getElementById("ppm-card").style.borderColor = "red";
  } else if (latestValues.ppm !== null) {
    document.getElementById("ppm-card").style.borderColor = "green";
  }

  if (latestValues.humidity !== null && !checkThreshold("humidity", latestValues.humidity)) {
    alertMessages.push("Humidity!");
    logEvent("humidity", latestValues.humidity);
    document.getElementById("hum-card").style.borderColor = "red";
  } else if (latestValues.humidity !== null) {
    document.getElementById("hum-card").style.borderColor = "green";
  }

  if (latestValues.temperature !== null && !checkThreshold("temperature", latestValues.temperature)) {
    alertMessages.push("Temperature!");
    logEvent("temperature", latestValues.temperature);
    document.getElementById("temp-card").style.borderColor = "red";
  } else if (latestValues.temperature !== null) {
    document.getElementById("temp-card").style.borderColor = "green";
  }

  document.getElementById("alert").innerText = alertMessages.length > 0 ? alertMessages.join(" ") : "No alerts";
}

// ===== PPM Classification =====
function classifyPPM(ppm) {
  if (ppm <= 800) return { text: "Good ✅", color: "green" };
  if (ppm <= 1200) return { text: "Moderate ⚠️", color: "orange" };
  if (ppm <= 2000) return { text: "Poor ❌", color: "red" };
  return { text: "Very Poor ☠️", color: "darkred" };
}

// MQTT
client.on("connect", () => {
  console.log("✅ Connected to Adafruit IO");
  client.subscribe(`${USERNAME}/feeds/gas_level`);
  client.subscribe(`${USERNAME}/feeds/humidity`);
  client.subscribe(`${USERNAME}/feeds/temperature`);
  client.subscribe(`${USERNAME}/feeds/Usage_count`);
});

client.on("message", (topic, message) => {
  const value = parseFloat(message.toString());

  if (topic.endsWith("gas_level")) {
    const ppm = parseFloat(value.toFixed(2));
    const quality = classifyPPM(ppm);

    document.getElementById("aq").innerHTML = `${ppm} PPM<br><span style="color:${quality.color}; font-size:1.1em; font-weight:bold;">${quality.text}</span>`;

    latestValues.ppm = ppm;
    liveChart.data.labels.push(new Date().toLocaleTimeString());
    liveChart.data.datasets[0].data.push(ppm);
    liveChart.update();
    updateAlerts();
  }

  else if (topic.endsWith("humidity")) {
    const hum = parseFloat(value.toFixed(2));
    document.getElementById("hum").innerText = `${hum} %`;
    latestValues.humidity = hum;
    liveChart.data.datasets[1].data.push(hum);
    liveChart.update();
    updateAlerts();
  }

  else if (topic.endsWith("temperature")) {
    const temp = parseFloat(value.toFixed(2));
    document.getElementById("temp").innerText = `${temp} °C`;
    latestValues.temperature = temp;
    liveChart.data.datasets[2].data.push(temp);
    liveChart.update();
    updateAlerts();
  }

  else if (topic.endsWith("Usage_count")) {
    const usage = parseFloat(value.toFixed(2));
    document.getElementById("usage").innerText = usage;
  }
});

// ===== Expiry Status Logic =====
const EXPIRY_DAYS = 180;  // 6 months
const START_DATE = "2025-09-10"; // Change this to inhaler open date

function checkExpiry() {
  const start = new Date(START_DATE);
  const today = new Date();
  const expiryDate = new Date(start);
  expiryDate.setDate(start.getDate() + EXPIRY_DAYS);

  const diffTime = expiryDate - today;
  const remainingDays = Math.ceil(diffTime / (1000 * 60 * 60 * 24));

  const expiryElement = document.getElementById("expiry");

  if (remainingDays <= 0) {
    expiryElement.innerText = "❌ Expired!";
    expiryElement.style.color = "red";
  } else {
    expiryElement.innerText = `${remainingDays} days left`;
    expiryElement.style.color = (remainingDays < 20) ? "red" : "green";
  }
}

// update once on load + daily
checkExpiry();
setInterval(checkExpiry, 86400000);
