#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

const char* ssid = "Natech";
const char* password = "natech888";

WebServer server(80);

// HTML trang chủ với OTA
const char* html_content = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32 Web Server + OTA Update</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { 
      font-family: Arial, sans-serif; 
      margin: 20px; 
      background: #f5f5f5;
    }
    .container {
      max-width: 800px;
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
    button:hover {
      background: #0056b3;
    }
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
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 Web Server + OTA Update</h1>
    
    <div class="status">
      <strong>IP Address:</strong> %IP% | 
      <strong>Chip ID:</strong> %CHIPID% |
      <strong>Free Heap:</strong> <span id="heap">%HEAP%</span> bytes |
      <strong>Firmware:</strong> v1.0.0
    </div>

    <div class="ota-section">
      <h2> OTA Firmware Update</h2>
      
      <div class="warning">
      <div class="warning">
      <strong>WARNING:</strong> 
      Do not power off during update process! 
      Device will automatically restart after update.
</div>
      </div>

      <form id="otaForm" enctype="multipart/form-data">
        <input type="file" id="firmwareFile" accept=".bin" required>
        <button type="submit"> Upload & Update Firmware</button>
      </form>

      <div class="progress" id="progressBar" style="display: none;">
        <div class="progress-bar" id="progressFill"></div>
      </div>

      <div class="log" id="otaLog">
        OTA Ready... Select firmware file and click Upload.
      </div>
    </div>

    <div>
      <h2> System Information</h2>
      <p><strong>Flash Size:</strong> %FLASH% MB</p>
      <p><strong>SDK Version:</strong> %SDK%</p>
      <p><strong>CPU Frequency:</strong> %CPU% MHz</p>
    </div>

    <div>
      <h2>Device Control</h2>
      <button onclick="controlLED(1)">LED ON</button>
      <button onclick="controlLED(0)">LED OFF</button>
      <button onclick="restartDevice()">Restart ESP32</button>
    </div>
  </div>

  <script>
    let otaInProgress = false;

    // Xử lý form OTA
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
          log.innerHTML = '✅ Update successful! Rebooting...\n' + log.innerHTML;
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

    // Update heap memory every 5 seconds
    setInterval(() => {
      fetch('/heap')
        .then(response => response.text())
        .then(heap => {
          document.getElementById('heap').textContent = heap;
        });
    }, 5000);
  </script>
</body>
</html>
)rawliteral";

// Biến toàn cục
bool otaInProgress = false;

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

void handleUpdate() {
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    otaInProgress = true;
    Serial.printf("OTA Update Started: %s\n", upload.filename.c_str());
    
    // Bắt đầu update
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    // Ghi dữ liệu firmware
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
    
    // Hiển thị tiến độ
    float progress = (float)upload.totalSize / (float)upload.totalSize * 100;
    Serial.printf("Progress: %.1f%%\n", progress);
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    // Kết thúc update
    if (Update.end(true)) {
      Serial.printf("OTA Success: %u bytes\n", upload.totalSize);
      Serial.println("Rebooting...");
      
      // Delay trước khi reboot
      delay(1000);
      ESP.restart();
    } else {
      Update.printError(Serial);
    }
    otaInProgress = false;
  }
}

void handleUpdateDone() {
  server.send(200, "text/plain", "Update complete! Rebooting...");
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

  // Khởi tạo LED
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  // Định nghĩa routes
  server.on("/", handleRoot);
  server.on("/update", HTTP_POST, 
    []() { server.send(200, "text/plain", "Update complete!"); },
    handleUpdate
  );
  server.on("/led", handleLED);
  server.on("/restart", handleRestart);
  server.on("/heap", handleHeap);

  // Bắt đầu server
  server.begin();
  Serial.println("✅ HTTP server + OTA Ready!");
  Serial.println("📱 Open: http://" + WiFi.localIP().toString());
  Serial.println("🔄 OTA Update available via web interface");
}

void loop() {
  server.handleClient();
}