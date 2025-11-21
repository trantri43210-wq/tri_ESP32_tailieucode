#include <WiFi.h>
#include <PubSubClient.h>
#include <Arduino.h>

const char* ssid = "Natech";
const char* password = "natech888";

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "esp32/lighttri";
const char* mqtt_control_topic = "esp32/controltri";  // 🆕 Topic nhận lệnh

WiFiClient espClient;
PubSubClient client(espClient);

#define SENSOR_PIN 34
#define LED_PIN 2     // 🆕 LED trên ESP32
unsigned long lastMsg = 0;

// 🆕 HÀM XỬ LÝ LỆNH NHẬN ĐƯỢC
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);
  
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Payload: ");
  Serial.println(message);
  
  // 🆕 XỬ LÝ LỆNH BẬT/TẮT LED
  if (message == "ON" || message == "on" || message == "1") {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("✅ LED ON");
    client.publish(mqtt_topic, "LED turned ON");  // 🆕 Phản hồi
  } 
  else if (message == "OFF" || message == "off" || message == "0") {
    digitalWrite(LED_PIN, LOW);
    Serial.println("🔴 LED OFF");
    client.publish(mqtt_topic, "LED turned OFF");  // 🆕 Phản hồi
  }
}

void setup_wifi() {
  delay(10);
  Serial.begin(9600);  // 🎯 Đổi lên 115200 cho ổn định
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32_LightSensor")) {
      Serial.println("connected");
      client.subscribe(mqtt_control_topic);  // 🆕 Subscribe topic điều khiển
      Serial.println("Subscribed to: " + String(mqtt_control_topic));
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5s");
      delay(5000);
    }
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);    // 🆕 Khai báo chân LED
  digitalWrite(LED_PIN, LOW);  // 🆕 Tắt LED lúc khởi động
  
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);  // 🆕 Gán hàm xử lý lệnh
  analogReadResolution(12);
  analogSetPinAttenuation(SENSOR_PIN, ADC_11db);
  delay(200);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;

    int raw = analogRead(SENSOR_PIN);
    float percent = (raw / 4095.0) * 100.0;

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"raw\":%d, \"pct\":%.1f}", raw, percent);

    Serial.print("Publish: ");
    Serial.println(payload);
    
    if (client.publish(mqtt_topic, payload)) {
      Serial.println("✅ Publish SUCCESS to MQTT!");
    } else {
      Serial.println("❌ Publish FAILED!");
    }
  }
}