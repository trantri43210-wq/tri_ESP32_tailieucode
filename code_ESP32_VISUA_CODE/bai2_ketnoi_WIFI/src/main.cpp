#include <WiFi.h>

// 🔧 Nhập tên WiFi và mật khẩu của bạn
const char* ssid     = "CMCC-4NKt";
const char* password = "xzcb6276";

void setup() {
  Serial.begin(9600);
  delay(1000);

  Serial.println();
  Serial.println("📶 KHOI DONG KET NOI WIFI");
  Serial.print("Dang ket noi toi: ");
  Serial.println(ssid);

  // Đặt chế độ Station (thiết bị kết nối đến Router)
  WiFi.mode(WIFI_STA);
  // Bắt đầu kết nối
  WiFi.begin(ssid, password);

  // Chờ kết nối
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("✅ Da ket noi WiFi thanh cong!");
  Serial.print("📡 Dia chi IP cua ESP32: ");
  Serial.println(WiFi.localIP());  // In IP nhận được từ router
}

void loop() {
  // In tín hiệu Wi-Fi định kỳ
  Serial.print("Cường độ tín hiệu (RSSI): ");
  Serial.println(WiFi.RSSI());  // RSSI càng cao, sóng càng mạnh
  delay(5000);
}
