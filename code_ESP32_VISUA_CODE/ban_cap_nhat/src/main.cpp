#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

const char* ssid = "Natech";
const char* password = "natech888";

WebServer server(80);

// Biến cho OTA và Analog
bool otaInProgress = false;
#define ANALOG_PIN 34

// HTML trang chủ với OTA + Analog Data
const char* html_content = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32 Web Server + OTA + Analog</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { 
      font-family: Arial, sans-serif; 
      margin: 20px; 
      background: #f5f5f5;
    }
    .container {
      max-width: 900px;
      margin: 0 auto;
      background: white;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    }
    h1 { color: #333; text-align: center; }
    .status { 
      background: #e8f5e8; 
      padding: 10px; 
      border-radius: 5px;
      margin: 10px 0;
    }
    .ota-section {
      background: #fff3cd;
      padding: 20px;
      border-radius: 5px;
      margin: 20px 0;
      border: 2px dashed #ffc107;
    }
    .data-section {
      background: #d1ecf1;
      padding: 20px;
      border-radius: 5px;
      margin: 20px 0;
    }
    .warning {
      background: #f8d7da;
      color: #721c24;
      padding: 10px;
      border-radius: 5px;
      margin: 10px 0;
    }
    button {
      background: #007bff;
      color: white;
      border: none;
      padding: 12px 24px;
      margin: 5px;
      border-radius: 5px;
      cursor: pointer;
      font-size: 16px;
    }
    button:hover { background: #0056b3; }
    button:disabled {
      background: #6c757d;
      cursor: not-allowed;
    }
    .progress {
      width: 100%;
      height: 20px;
      background: #e9ecef;
      border-radius: 10px;
      margin: 10px 0;
      overflow: hidden;
    }
    .progress-bar {
      height: 100%;
      background: #28a745;
      width: 0%;
      transition: width 0.3s;
    }
    .log {
      background: #f8f9fa;
      border: 1px solid #dee2e6;
      border-radius: 5px;
      padding: 10px;
      margin: 10px 0;
      height: 100px;
      overflow-y: auto;
      font-family: monospace;
      font-size: 12px;
    }
    .sensor-data {
      display: flex;
      justify-content: space-around;
      text-align: center;
      margin: 20px 0;
    }
    .sensor-card {
      background: white;
      padding: 15px;
      border-radius: 8px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.1);
      min-width: 150px;
    }
    .sensor-value {
      font-size: 24px;
      font-weight: bold;
      color: #007bff;
    }
    .sensor-unit {
      font-size: 14px;
      color: #6c757d;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      margin: 10px 0;
    }
    th, td {
      border: 1px solid #ddd;
      padding: 8px;
      text-align: center;
    }
    th {
      background-color: #4CAF50;
      color: white;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 Web Server + OTA + Analog Read</h1>
    
    <div class="status">
      <strong>IP Address:</strong> %IP% | 
      <strong>Chip ID:</strong> %CHIPID% |
      <strong>Free Heap:</strong> <span id="heap">%HEAP%</span> bytes |
      <strong>Firmware:</strong> v1.1.0 (Analog Support)
    </div>

    <!-- Analog Data Section -->
    <div class="data-section">
      <h2>Analog Sensor Data (GPIO 34)</h2>
      
      <div class="sensor-data">
        <div class="sensor-card">
          <div class="sensor-value" id="analogValue">0</div>
          <div class="sensor-unit">Raw Value (0-4095)</div>
        </div>
        <div class="sensor-card">
          <div class="sensor-value" id="voltageValue">0.00</div>
          <div class="sensor-unit">Voltage (V)</div>
        </div>
        <div class="sensor-card">
          <div class="sensor-value" id="percentageValue">0%</div>
          <div class="sensor-unit">Percentage</div>
        </div>
      </div>

      <div style="text-align: center;">
        <button onclick="updateSensorData()"> Update Sensor Data</button>
        <button onclick="startAutoSensorUpdate()">Auto Update (3s)</button>
        <button onclick="stopAutoSensorUpdate()">Stop Auto</button>
      </div>

      <h3>Recent Data Log</h3>
      <table>
        <thead>
          <tr>
            <th>Time</th>
            <th>Raw Value</th>
            <th>Voltage</th>
            <th>Percentage</th>
          </tr>
        </thead>
        <tbody id="sensorTableBody">
          <!-- Sensor data will be inserted here -->
        </tbody>
      </table>
    </div>

    <!-- OTA Section -->
    <div class="ota-section">
      <h2>OTA Firmware Update</h2>
      
      <div class="warning">
        <strong>WARNING:</strong> 
        Do not power off during update process! 
        Device will automatically restart after update.
      </div>

      <form id="otaForm" enctype="multipart/form-data">
        <input type="file" id="firmwareFile" accept=".bin" required>
        <button type="submit">Upload & Update Firmware</button>
      </form>

      <div class="progress" id="progressBar" style="display: none;">
        <div class="progress-bar" id="progressFill"></div>
      </div>

      <div class="log" id="otaLog">
        OTA Ready... Select firmware file and click Upload.
      </div>
    </div>

    <div>
      <h2>System Information</h2>
      <p><strong>Flash Size:</strong> %FLASH% MB</p>
      <p><strong>SDK Version:</strong> %SDK%</p>
      <p><strong>CPU Frequency:</strong> %CPU% MHz</p>
      <p><strong>Analog Pin:</strong> GPIO 34</p>
    </div>

    <div>
      <h2>Device Control</h2>
      <button onclick="controlLED(1)">LED ON</button>
      <button onclick="controlLED(0)">LED OFF</button>
      <button onclick="restartDevice()"> Restart ESP32</button>
    </div>
  </div>

  <script>
    let otaInProgress = false;
    let sensorUpdateInterval;

    // Update Sensor Data
    function updateSensorData() {
      fetch('/sensor')
        .then(response => response.json())
        .then(data => {
          document.getElementById('analogValue').textContent = data.analog;
          document.getElementById('voltageValue').textContent = data.voltage;
          document.getElementById('percentageValue').textContent = data.percentage + '%';
          updateSensorTable(data);
        });
    }

    function updateSensorTable(data) {
      const tableBody = document.getElementById('sensorTableBody');
      const now = new Date();
      const timeString = now.toLocaleTimeString();
      
      // Giới hạn table 10 rows
      if (tableBody.rows.length >= 10) {
        tableBody.deleteRow(0);
      }
      
      const row = tableBody.insertRow();
      row.innerHTML = `
        <td>${timeString}</td>
        <td>${data.analog}</td>
        <td>${data.voltage} V</td>
        <td>${data.percentage}%</td>
      `;
    }

    function startAutoSensorUpdate() {
      sensorUpdateInterval = setInterval(updateSensorData, 3000);
      updateSensorData(); // Update immediately
    }

    function stopAutoSensorUpdate() {
      clearInterval(sensorUpdateInterval);
    }

    // OTA Functions
    document.getElementById('otaForm').addEventListener('submit', function(e) {
      e.preventDefault();
      
      if (otaInProgress) {
        alert('OTA update already in progress!');
        return;
      }

      const fileInput = document.getElementById('firmwareFile');
      const file = fileInput.files[0];
      
      if (!file) {
        alert('Please select a firmware file!');
        return;
      }

      if (!file.name.endsWith('.bin')) {
        alert('Please select a .bin file!');
        return;
      }

      startOTAUpdate(file);
    });

    function startOTAUpdate(file) {
      otaInProgress = true;
      const progressBar = document.getElementById('progressBar');
      const progressFill = document.getElementById('progressFill');
      const log = document.getElementById('otaLog');
      
      progressBar.style.display = 'block';
      log.innerHTML = 'Starting OTA update...\n';

      const formData = new FormData();
      formData.append('firmware', file);

      const xhr = new XMLHttpRequest();
      
      xhr.upload.addEventListener('progress', function(e) {
        if (e.lengthComputable) {
          const percent = (e.loaded / e.total) * 100;
          progressFill.style.width = percent + '%';
          log.innerHTML = `Uploading: ${percent.toFixed(1)}%\n` + log.innerHTML;
        }
      });

      xhr.addEventListener('load', function() {
        if (xhr.status === 200) {
          log.innerHTML = '✅Update successful! Rebooting...\n' + log.innerHTML;
          setTimeout(() => {
            window.location.reload();
          }, 3000);
        } else {
          log.innerHTML = '❌ Update failed: ' + xhr.responseText + '\n' + log.innerHTML;
        }
        otaInProgress = false;
      });

      xhr.addEventListener('error', function() {
        log.innerHTML = '❌ Upload failed! Check connection.\n' + log.innerHTML;
        otaInProgress = false;
      });

      xhr.open('POST', '/update');
      xhr.send(formData);
    }

    function controlLED(state) {
      fetch('/led?state=' + state)
        .then(response => response.text())
        .then(data => console.log('LED:', data));
    }

    function restartDevice() {
      if (confirm('Are you sure you want to restart the device?')) {
        fetch('/restart')
          .then(() => {
            alert('Device restarting...');
            setTimeout(() => {
              window.location.reload();
            }, 5000);
          });
      }
    }

    // Auto-start sensor updates
    document.addEventListener('DOMContentLoaded', function() {
      startAutoSensorUpdate();
      
      // Update heap memory every 5 seconds
      setInterval(() => {
        fetch('/heap')
          .then(response => response.text())
          .then(heap => {
            document.getElementById('heap').textContent = heap;
          });
      }, 5000);
    });
  </script>
</body>
</html>
)rawliteral";

// Hàm đọc analog và tính toán
String readAnalogData() {
  int analogValue = analogRead(ANALOG_PIN);
  float voltage = (analogValue * 3.3) / 4095.0;
  int percentage = (analogValue * 100) / 4095;
  
  String data = "{";
  data += "\"analog\":" + String(analogValue) + ",";
  data += "\"voltage\":" + String(voltage, 2) + ",";
  data += "\"percentage\":" + String(percentage);
  data += "}";
  
  return data;
}

void handleRoot() {
  String html = String(html_content);
  html.replace("%IP%", WiFi.localIP().toString());
  html.replace("%CHIPID%", String((uint32_t)ESP.getEfuseMac(), HEX));
  html.replace("%HEAP%", String(ESP.getFreeHeap()));
  html.replace("%FLASH%", String(ESP.getFlashChipSize() / (1024 * 1024)));
  html.replace("%SDK%", String(ESP.getSdkVersion()));
  html.replace("%CPU%", String(ESP.getCpuFreqMHz()));
  
  server.send(200, "text/html", html);
}

void handleSensor() {
  server.send(200, "application/json", readAnalogData());
}

void handleUpdate() {
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    otaInProgress = true;
    Serial.printf("OTA Update Started: %s\n", upload.filename.c_str());
    
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
    
    float progress = (float)upload.totalSize / (float)upload.totalSize * 100;
    Serial.printf("Progress: %.1f%%\n", progress);
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("OTA Success: %u bytes\n", upload.totalSize);
      Serial.println("Rebooting...");
      delay(1000);
      ESP.restart();
    } else {
      Update.printError(Serial);
    }
    otaInProgress = false;
  }
}

void handleLED() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    digitalWrite(2, state.toInt());
    server.send(200, "text/plain", "LED: " + state);
  }
}

void handleRestart() {
  server.send(200, "text/plain", "Restarting...");
  delay(1000);
  ESP.restart();
}

void handleHeap() {
  server.send(200, "text/plain", String(ESP.getFreeHeap()));
}

void setup() {
  Serial.begin(9600);
  
  // Cấu hình ADC cho GPIO34
  analogReadResolution(12);       // 12-bit (0-4095)
  analogSetAttenuation(ADC_11db); // Đọc 0-3.3V
  
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
  Serial.println("📊 Analog sensor ready on GPIO 34");

  // Khởi tạo LED
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  // Định nghĩa routes
  server.on("/", handleRoot);
  server.on("/sensor", handleSensor);
  server.on("/update", HTTP_POST, 
    []() { server.send(200, "text/plain", "Update complete!"); },
    handleUpdate
  );
  server.on("/led", handleLED);
  server.on("/restart", handleRestart);
  server.on("/heap", handleHeap);

  // Bắt đầu server
  server.begin();
  Serial.println("✅ HTTP server + OTA + Analog Ready!");
  Serial.println("📱 Open: http://" + WiFi.localIP().toString());
}

void loop() {
  server.handleClient();
  
  // In giá trị analog ra Serial Monitor mỗi 5 giây
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    lastPrint = millis();
    int analogValue = analogRead(ANALOG_PIN);
    float voltage = (analogValue * 3.3) / 4095.0;
    Serial.printf("📊 Analog: %d (%.2fV)\n", analogValue, voltage);
  }
}