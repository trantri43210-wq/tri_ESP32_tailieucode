#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ================== CẤU HÌNH ==================
// WiFi
#define WIFI_SSID "CMCC-4NKt"
#define WIFI_PASS "xzcb6276"

// Firebase
#define FIREBASE_HOST "timelifenhdo-doam-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "8JRlgEpa5DYsr7C5RdcE53ixCTfz87PkhZjlbqj6"

// Cảm biến ánh sáng
#define LIGHT_SENSOR_PIN 34
#define LED_PIN 2

// ================== KHAI BÁO FIREBASE ==================
FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);

unsigned long previousMillis = 0;
const long interval = 5000;

// ================== HÀM KẾT NỐI WIFI ==================
void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// ================== HÀM KẾT NỐI FIREBASE ==================
void connectFirebase() {
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase connected!");
}

// ================== HÀM TẠO TIMESTAMP ISO ==================
String getFormattedDateTime() {
  timeClient.update();
  
  unsigned long epochTime = timeClient.getEpochTime();
  time_t rawTime = (time_t)epochTime;   // ép kiểu đúng cho gmtime()

  struct tm *timeinfo = gmtime(&rawTime);

  char buffer[30];
  snprintf(buffer, sizeof(buffer),
           "%04d-%02d-%02dT%02d:%02d:%02dZ",
           timeinfo->tm_year + 1900,
           timeinfo->tm_mon + 1,
           timeinfo->tm_mday,
           timeinfo->tm_hour,
           timeinfo->tm_min,
           timeinfo->tm_sec);

  return String(buffer);
}


// ================== HÀM ĐỌC CẢM BIẾN ÁNH SÁNG ==================
int readLightSensor() {
  int rawValue = analogRead(LIGHT_SENSOR_PIN);
  int lightPercentage = map(rawValue, 0, 4095, 0, 100);
  lightPercentage = 100 - lightPercentage;

  Serial.print("ADC: ");
  Serial.print(rawValue);
  Serial.print(" | Light: ");
  Serial.print(lightPercentage);
  Serial.println("%");

  return lightPercentage;
}

// ================== HÀM GỬI DỮ LIỆU LÊN FIREBASE ==================
void sendToFirebase(int lightPercentage) {
  String timestamp = getFormattedDateTime();

  FirebaseJson json;
  json.set("light_intensity", lightPercentage);
  json.set("raw_adc", analogRead(LIGHT_SENSOR_PIN));
  json.set("timestamp", timestamp);

  String path = "/sensor_data/" + timestamp;

  if (Firebase.setJSON(fbdo, path, json)) {
    Serial.println("Data sent successfully!");
    Serial.println("Path: " + path);
  } else {
    Serial.println("❌ Firebase Error: " + fbdo.errorReason());
  }
}

// ================== SETUP ==================
void setup() {
  Serial.begin(9600);

  pinMode(LIGHT_SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  analogReadResolution(12);

  connectWiFi();
  timeClient.begin();
  connectFirebase();

  Serial.println("🚀 Light Sensor System started!");
}

// ================== LOOP ==================
void loop() {
  unsigned long currentMillis = millis();

  // Tự động reconnect WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost! Reconnecting...");
    connectWiFi();
    timeClient.begin();
  }

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    int light = readLightSensor();
    sendToFirebase(light);

    digitalWrite(LED_PIN, light < 30);
  }

  delay(200);
}
