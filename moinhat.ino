

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// ================= THÔNG TIN WIFI (ĐIỂM PHÁT TỪ ĐIỆN THOẠI) =================
const char* ssid = "No 1";
const char* password = "13012005";

// ================= CẤU HÌNH UART VÀ LED DEBUG =================
#define RXD2 16
#define TXD2 17
#define LED_PIN 2

typedef struct __attribute__((packed)) {
    uint8_t header;       
    float setpoint_pos;   
    int32_t current_pos;  
    float target_vel;     
    float current_vel;    
    float rpm_motor;      
    float rpm_output;     
    float v_Kp; float v_Ki; float v_Kd;    
    float p_Kp; float p_Ki; float p_Kd;    
    float pwm_out;        
    float current_ma;     
    uint8_t checksum;
    uint8_t footer;       
} TelemetryData;

typedef struct __attribute__((packed)) {
    uint8_t header;    
    uint8_t cmd_id;    
    float value;       
    uint8_t checksum;
    uint8_t footer;    
} ControlPacket;

ControlPacket txData;

enum RX_State { WAIT_HEADER, WAIT_PAYLOAD };
RX_State rx_state = WAIT_HEADER;
uint8_t rx_buffer[sizeof(TelemetryData)];
uint8_t rx_index = 0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool isSampling = false;
uint32_t samplingStartTime = 0;
uint32_t samplingDuration = 0;
uint32_t last_ws_send = 0; 

// ================= GIAO DIỆN WEB (HTML + JS) =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Real-time Fuzzy Logic PID Monitor</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f2f5; margin: 0; padding: 15px; }
        .container { max-width: 1100px; margin: auto; background: white; padding: 20px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); }
        h2 { color: #333; margin-top: 0; }
        #status { font-weight: bold; margin-bottom: 20px; font-size: 18px; padding: 10px; border-radius: 8px; background: #fff3cd; color: #856404; border: 1px solid #ffeeba; transition: 0.3s;}
        
        .controls { display: flex; flex-wrap: wrap; gap: 15px; margin-bottom: 20px; justify-content: center; align-items: flex-start;}
        .input-group { flex: 1; min-width: 150px; text-align: center; display: flex; flex-direction: column; }
        input { padding: 12px; font-size: 16px; border: 1px solid #ced4da; border-radius: 6px; text-align: center; width: 100%; box-sizing: border-box;}
        button { padding: 12px 24px; font-size: 16px; border: none; border-radius: 6px; font-weight: bold; color: white; cursor: pointer; transition: 0.2s; flex: 1; min-width: 120px; height: 45px;}
        
        .btn-start { background-color: #28a745; }
        .btn-start:hover { background-color: #218838; }
        .btn-stop { background-color: #dc3545; }
        .btn-stop:hover { background-color: #c82333; }
        
        .dashboard-grid { 
            display: grid; 
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); 
            gap: 10px; 
            background: #e9ecef; 
            padding: 15px; 
            border-radius: 8px; 
            margin-bottom: 10px; 
            font-size: 16px; 
            font-weight: bold;
        }
        .box { background: white; padding: 10px; border-radius: 6px; border: 1px solid #ddd; box-shadow: 0 2px 4px rgba(0,0,0,0.05); }
        .val-current { color: #007bff; }
        .val-target { color: #dc3545; }
        .val-vel { color: #9c27b0; } 
        .val-time { color: #28a745; }
        .val-fuzzy { color: #e67e22; }
        .val-metric { color: #d35400; background: #fff3e0 !important; }
        .val-error { color: #d32f2f; background: #ffebee !important; } 
        
        .chart-wrapper { position: relative; width: 100%; height: 500px; margin-top: 10px; background: #fafafa; border: 1px solid #eee; border-radius: 8px; padding: 10px; box-sizing: border-box;}
    </style>
</head>
<body>
    <div class="container">
        <h2>Real-time Sequence Fuzzy Logic PID Monitor</h2>
        <div id="status">⏳ Connecting to ESP32 via WebSocket...</div>
        
        <div class="controls">
            <div class="input-group">
                <input type="number" id="setpoint_A" placeholder="Angle A (&deg;)" oninput="updateRevolutions(this.value, 'rev_A')">
                <div style="font-size: 14px; color: #555; margin-top: 6px;">~ <span id="rev_A" style="font-weight: bold; color: #007bff;">0.00</span> revs</div>
            </div>

            <div class="input-group">
                <input type="number" id="setpoint_B" placeholder="Angle B (&deg;)" oninput="updateRevolutions(this.value, 'rev_B')">
                <div style="font-size: 14px; color: #555; margin-top: 6px;">~ <span id="rev_B" style="font-weight: bold; color: #007bff;">0.00</span> revs</div>
            </div>
            
            <div class="input-group">
                <input type="number" id="duration" placeholder="Duration (ms)" value="10000">
            </div>
            
            <button class="btn-start" onclick="startSampling()">▶ START</button>
            <button class="btn-stop" onclick="stopSampling()">⏹ STOP</button>
        </div>

        <div class="dashboard-grid">
            <div class="box val-time">⏱ Time: <span id="live_time">0.00</span>s</div>
            <div class="box val-current">🔵 Position: <span id="live_pos">0</span>&deg;</div>
            <div class="box val-target">🔴 Target: <span id="live_set">0</span>&deg;</div>
            <div class="box val-vel">🟣 Speed: <span id="live_vel">0.0</span> RPM</div>
            
            <div class="box val-fuzzy">🧠 Kp: <span id="live_kp">0.00</span></div>
            <div class="box val-fuzzy">🧠 Ki: <span id="live_ki">0.00</span></div>
            <div class="box val-fuzzy">🧠 Kd: <span id="live_kd">0.00</span></div>
            <div class="box" style="color:#607d8b">🔄 Sampling: <span id="live_dt">0</span> ms</div>

            <div class="box val-error" style="grid-column: span 2;">⚠️ Real-time Error: <span id="live_error">0&deg; (Matched)</span></div>
            
            <div class="box val-metric" style="grid-column: span 2;">📈 Overshoot: <span id="live_overshoot">0&deg; (0.0%)</span></div>
            <div class="box val-metric" style="grid-column: span 2;">🎯 Response Time: <span id="live_response">0.00s</span></div>
        </div>

        <div class="chart-wrapper">
            <canvas id="pidChart"></canvas>
        </div>
    </div>

    <script>
        var gateway = `ws://${window.location.hostname}/ws`;
        var websocket = new WebSocket(gateway);
        var chart = null;

        var last_time_ms = 0;
        var max_peak_pulses = 0;
        var reached_setpoint = false;

        // Sequence State Machine Variables
        var seq_state = 0; // 0: Idle, 1: Moving to A, 2: Waiting, 3: Moving to B, 4: Done
        var target_A_pulses = 0;
        var target_B_pulses = 0;

        // Tỉ lệ quy đổi
        const PULSES_PER_REV = 1320.0;
        const DEG_PER_PULSE = 360.0 / PULSES_PER_REV;
        const PULSE_PER_DEG = PULSES_PER_REV / 360.0;

        function updateRevolutions(degrees, elementId) {
            if(!degrees || isNaN(degrees)) {
                document.getElementById(elementId).innerText = "0.00";
                return;
            }
            var revs = (parseFloat(degrees) / 360.0).toFixed(2);
            document.getElementById(elementId).innerText = revs;
        }

        function setStatus(msg, bg, col) {
            let st = document.getElementById("status");
            st.innerHTML = msg;
            st.style.background = bg;
            st.style.color = col;
        }

        websocket.onopen = function(e) {
            setStatus("✅ CONNECTED TO ESP32 SUCCESSFULLY!", "#d4edda", "#155724");
        };

        websocket.onclose = function(e) {
            setStatus("❌ CONNECTION LOST! Please reload the page...", "#f8d7da", "#721c24");
        };

        websocket.onmessage = function(event) {
            var obj = JSON.parse(event.data);
            var t_sec = (obj.t / 1000.0).toFixed(2); 
            var rpm = ((obj.vel * 60.0) / PULSES_PER_REV).toFixed(1);

            var pos_deg = (obj.pos * DEG_PER_PULSE).toFixed(1);
            var set_deg = (obj.set * DEG_PER_PULSE).toFixed(1);

            var dt = 0;
            if(last_time_ms > 0) dt = obj.t - last_time_ms;
            last_time_ms = obj.t;

            // ================= LÔGIC CHUYỂN TRẠNG THÁI (A -> ĐỢI 2s -> B) =================
            if (seq_state === 1) {
                // Nếu sai số < 5 xung (~ 1.3 độ) coi như đã đến vị trí A
                if (Math.abs(obj.pos - target_A_pulses) <= 5) {
                    seq_state = 2; 
                    setStatus("⏳ Reached Angle A. Waiting 2s...", "#fff3cd", "#856404");
                    
                    // Đợi 2000ms (2s) rồi chuyển sang B
                    setTimeout(() => {
                        if (seq_state === 2) {
                            seq_state = 3; 
                            // Reset các thông số đánh giá cho chặng B
                            max_peak_pulses = obj.pos;
                            reached_setpoint = false;
                            
                            setStatus("🚀 Moving to Angle B...", "#cce5ff", "#004085");
                            if(websocket.readyState === WebSocket.OPEN) {
                                // Gửi lệnh phụ chỉ cập nhật setpoint (không reset timer)
                                websocket.send(JSON.stringify({ command: "set_only", setpoint: target_B_pulses }));
                            }
                        }
                    }, 2000); // Đã sửa thời gian thành 2000ms
                }
            } else if (seq_state === 3) {
                if (Math.abs(obj.pos - target_B_pulses) <= 5) {
                    seq_state = 4;
                    setStatus("✅ Sequence Complete (At Angle B)!", "#d4edda", "#155724");
                }
            }
            // ==============================================================================

            var current_set_pulses = obj.set;
            if (Math.abs(obj.pos) > Math.abs(max_peak_pulses)) {
                max_peak_pulses = obj.pos; 
            }

            var err_pulses = obj.set - obj.pos;
            var err_deg = Math.abs(err_pulses * DEG_PER_PULSE).toFixed(1);
            var err_str = err_deg + "&deg; ";
            
            if (Math.abs(err_pulses) <= 3) { // Deadzone của STM32
                err_str += "(Matched 🎯)";
            } else if (err_pulses > 0) {
                err_str += (obj.set >= 0) ? "(Lagging 🐢)" : "(Overshooting 🚀)";
            } else {
                err_str += (obj.set >= 0) ? "(Overshooting 🚀)" : "(Lagging 🐢)";
            }
            document.getElementById("live_error").innerHTML = err_str;

            if (current_set_pulses !== 0) {
                var os_val_pulses = Math.abs(max_peak_pulses) - Math.abs(current_set_pulses);
                if (os_val_pulses > 0) {
                    var os_pct = (os_val_pulses / Math.abs(current_set_pulses)) * 100.0;
                    var os_deg = (os_val_pulses * DEG_PER_PULSE).toFixed(1);
                    document.getElementById("live_overshoot").innerHTML = os_deg + "&deg; (" + os_pct.toFixed(1) + "%)";
                }

                if (!reached_setpoint && Math.abs(obj.pos) >= Math.abs(current_set_pulses) * 0.95) {
                    reached_setpoint = true;
                    document.getElementById("live_response").innerText = t_sec + "s";
                }
            }

            document.getElementById("live_time").innerText = t_sec;
            document.getElementById("live_pos").innerHTML = pos_deg + "&deg;";
            document.getElementById("live_set").innerHTML = set_deg + "&deg;";
            document.getElementById("live_vel").innerText = rpm; 
            
            document.getElementById("live_kp").innerText = obj.kp.toFixed(2);
            document.getElementById("live_ki").innerText = obj.ki.toFixed(2);
            document.getElementById("live_kd").innerText = obj.kd.toFixed(2);
            document.getElementById("live_dt").innerText = dt;

            if(chart) {
                chart.data.labels.push(t_sec);
                chart.data.datasets[0].data.push(pos_deg);  
                chart.data.datasets[1].data.push(set_deg);  
                chart.data.datasets[2].data.push(rpm);      
                chart.update();
            }
        };

        function startSampling() {
            let deg_A = document.getElementById("setpoint_A").value;
            let deg_B = document.getElementById("setpoint_B").value;
            let dur = document.getElementById("duration").value;
            if(deg_A === "" || deg_B === "" || dur === "") { alert("Please enter valid numbers for both Angles!"); return; }

            last_time_ms = 0;
            max_peak_pulses = 0;
            reached_setpoint = false;
            
            // Khởi tạo máy trạng thái
            seq_state = 1;
            target_A_pulses = parseFloat(deg_A) * PULSE_PER_DEG;
            target_B_pulses = parseFloat(deg_B) * PULSE_PER_DEG;

            setStatus("🚀 Moving to Angle A...", "#cce5ff", "#004085");

            document.getElementById("live_time").innerText = "0.00";
            document.getElementById("live_pos").innerHTML = "0&deg;";
            document.getElementById("live_set").innerHTML = deg_A + "&deg;";
            document.getElementById("live_vel").innerText = "0.0";
            document.getElementById("live_error").innerHTML = "0&deg; (Matched)";
            document.getElementById("live_overshoot").innerHTML = "0&deg; (0.0%)";
            document.getElementById("live_response").innerText = "Not reached...";
            
            if(chart) {
                chart.data.labels =[];
                chart.data.datasets[0].data = [];
                chart.data.datasets[1].data = [];
                chart.data.datasets[2].data =[];
                chart.update();
            }

            if(websocket.readyState === WebSocket.OPEN) {
                websocket.send(JSON.stringify({ command: "start", setpoint: target_A_pulses, duration: parseInt(dur) }));
            }
        }

        function stopSampling() {
            seq_state = 0; // Hủy chuỗi chạy
            setStatus("⏹ Sequence STOPPED", "#f8d7da", "#721c24");
            if(websocket.readyState === WebSocket.OPEN) {
                websocket.send(JSON.stringify({ command: "stop" }));
            }
        }
    </script>

    <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.9.1/chart.min.js"></script>
    <script>
        window.onload = function() {
            if(typeof Chart !== 'undefined') {
                var ctx = document.getElementById('pidChart').getContext('2d');
                chart = new Chart(ctx, {
                    type: 'line',
                    data: {
                        labels:[],
                        datasets:[
                            { label: 'Current Position (°)', borderColor: '#007bff', data: [], borderWidth: 2, pointRadius: 0, fill: false, yAxisID: 'y' },
                            { label: 'Target Position (°)', borderColor: '#dc3545', data:[], borderWidth: 2, borderDash:[5, 5], pointRadius: 0, fill: false, yAxisID: 'y' },
                            { label: 'Speed (RPM)', borderColor: '#9c27b0', data:[], borderWidth: 2, pointRadius: 0, fill: false, yAxisID: 'y1' }
                        ]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        animation: false,
                        interaction: {
                            mode: 'index',
                            intersect: false,
                        },
                        scales: {
                            x: { 
                                title: { display: true, text: 'Time (Seconds)' } 
                            },
                            y: { 
                                type: 'linear',
                                display: true,
                                position: 'left',
                                title: { display: true, text: 'Position (Degrees °)' } 
                            },
                            y1: {
                                type: 'linear',
                                display: true,
                                position: 'right',
                                title: { display: true, text: 'Speed (RPM)' },
                                grid: { drawOnChartArea: false } 
                            }
                        }
                    }
                });
            } else {
                document.getElementById("status").innerHTML += "<br>⚠️ Error: Cannot load the chart due to poor network connection!";
            }
        };
    </script>
</body>
</html>
)rawliteral";

// ================= HÀM GỬI LỆNH XUỐNG STM32 =================
void sendToSTM32(uint8_t cmd, float val) {
    txData.header = 0xBB;
    txData.cmd_id = cmd;
    txData.value = val;
    
    uint8_t sum = 0;
    uint8_t *ptr = (uint8_t*)&txData;
    for (int i = 1; i < sizeof(ControlPacket) - 2; i++) {
        sum += ptr[i];
    }
    txData.checksum = sum;
    txData.footer = 0xCC;
    
    Serial2.write((uint8_t*)&txData, sizeof(ControlPacket));
}

// ================= XỬ LÝ SỰ KIỆN WEBSOCKET TỪ WEB =================
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0; 
            String msg = (char*)data;
            
            StaticJsonDocument<200> doc;
            DeserializationError error = deserializeJson(doc, msg);
            if (!error) {
                const char* command = doc["command"];
                
                if (strcmp(command, "start") == 0) {
                    float sp = doc["setpoint"];
                    samplingDuration = doc["duration"];
                    
                    sendToSTM32(1, sp); 
                    
                    isSampling = true;
                    samplingStartTime = millis();
                    Serial.println("Started sampling data...");
                    
                } 
                // Lệnh phụ dành riêng cho chặng B (Chỉ cập nhật Setpoint, không reset đồ thị)
                else if (strcmp(command, "set_only") == 0) {
                    float sp = doc["setpoint"];
                    sendToSTM32(1, sp); 
                    Serial.println("Moving to Setpoint B...");
                }
                else if (strcmp(command, "stop") == 0) {
                    isSampling = false;
                }
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2); 
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW); 

    WiFi.mode(WIFI_STA);   
    WiFi.disconnect();     
    delay(100);            

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("✅ WiFi connected SUCCESSFULLY!");
    Serial.print("OPEN BROWSER AND GO TO: http://");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET,[](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    ws.onEvent(onEvent);
    server.addHandler(&ws);
    server.begin();
}

void loop() {
    ws.cleanupClients(); 

    if (isSampling && (millis() - samplingStartTime >= samplingDuration)) {
        isSampling = false;
        Serial.println("✅ Sampling process completed!");
    }

    while (Serial2.available()) {
        uint8_t rx_byte = Serial2.read();
        
        switch (rx_state) {
            case WAIT_HEADER:
                if (rx_byte == 0xAA) {
                    rx_buffer[0] = rx_byte;
                    rx_index = 1;
                    rx_state = WAIT_PAYLOAD;
                }
                break;
                
            case WAIT_PAYLOAD:
                rx_buffer[rx_index++] = rx_byte;
                
                if (rx_index >= sizeof(TelemetryData)) {
                    TelemetryData *p = (TelemetryData*)rx_buffer;
                    
                    if (p->footer == 0x55) { 
                        uint8_t calc_sum = 0;
                        for (int i = 1; i < sizeof(TelemetryData) - 2; i++) {
                            calc_sum += rx_buffer[i];
                        }
                        
                        if (calc_sum == p->checksum) { 
                            digitalWrite(LED_PIN, !digitalRead(LED_PIN));

                            if (isSampling && (millis() - last_ws_send >= 100)) {
                                last_ws_send = millis();
                                uint32_t t_ms = millis() - samplingStartTime; 
                                
                                char jsonStr[200];
                                snprintf(jsonStr, sizeof(jsonStr), "{\"t\":%u,\"pos\":%d,\"set\":%.1f,\"vel\":%.1f,\"kp\":%.2f,\"ki\":%.2f,\"kd\":%.2f}", 
                                         t_ms, p->current_pos, p->setpoint_pos, p->current_vel, p->p_Kp, p->p_Ki, p->p_Kd);
                                
                                ws.textAll(jsonStr); 
                            }
                        }
                    }
                    rx_state = WAIT_HEADER; 
                }
                break;
        }
    }
}

