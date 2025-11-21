#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Natech";
const char* password = "natech888";

WebServer server(80);

// HTML trang chủ
const char* html_content = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32 Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin: 50px; }
    button { padding: 10px 20px; margin: 10px; font-size: 16px; }
    .on { background: #4CAF50; color: white; }
    .off { background: #f44336; color: white; }
  </style>
</head>
<body>
  <h1> ESP32 Web Server</h1>
  <p>Welcome to ESP32 Control Panel by trandinhtri</p>
  
  <h2> LED Control (GPIO 2)</h2>
  <button class="on" onclick="fetch('/led?state=1')"> LED ON</button>
  <button class="off" onclick="fetch('/led?state=0')"> LED OFF</button>
  
  <h2> Sensor Data</h2>
  <p>Analog Value: <span id="analogValue">0</span></p>
  <button onclick="updateAnalog()"> Update Analog</button>
  
  <script>
    function updateAnalog() {
      fetch('/analog')
        .then(response => response.text())
        .then(data => {
          document.getElementById('analogValue').textContent = data;
        });
    }
    
    // Tự động update mỗi 5 giây
    setInterval(updateAnalog, 5000);
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", html_content);
}

void handleAnalog() {
  int analogValue = analogRead(34);
  server.send(200, "text/plain", String(analogValue));
}

void handleLED() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    digitalWrite(2, state.toInt());
    server.send(200, "text/plain", "LED: " + state);
  } else {
    server.send(400, "text/plain", "Missing state parameter");
  }
}

void setup() {
  Serial.begin(9600);
  
  // Kết nối WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  
  Serial.print("✅ WiFi connected! IP: ");
  Serial.println(WiFi.localIP());

  // Định nghĩa routes
  server.on("/", handleRoot);
  server.on("/analog", handleAnalog);
  server.on("/led", handleLED);

  // Khởi tạo LED
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  // Bắt đầu server
  server.begin();
  Serial.println("✅ HTTP server started!");
  Serial.println("📱 Open: http://" + WiFi.localIP().toString());
}

void loop() {
  server.handleClient();
}