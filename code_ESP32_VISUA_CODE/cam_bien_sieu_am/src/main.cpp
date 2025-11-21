#include<Arduino.h>
// Định nghĩa các chân kết nối đã thay đổi
const int TRIG_PIN = 5;  // Chân Trig (Output) nối với ESP32 GPIO 5
const int ECHO_PIN = 18; // Chân Echo (Input) nối với ESP32 GPIO 18 (ĐÃ QUA CẦU PHÂN ÁP!)
const int LED = 2;
// Các biến lưu trữ giá trị
long duration; // Thời gian sóng đi và về (microseconds)
int distanceCm; // Khoảng cách tính bằng centimet

void setup() {
  // Cài đặt tốc độ truyền dữ liệu Serial Monitor
  Serial.begin(9600);
  
  // Thiết lập chân Trig là OUTPUT (Gửi xung)
  pinMode(TRIG_PIN, OUTPUT);
  // Thiết lập chân Echo là INPUT (Nhận xung phản hồi)
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED, OUTPUT);
  // Khởi tạo trạng thái LOW cho chân Trig
  digitalWrite(TRIG_PIN, LOW);
  digitalWrite(LED, HIGH);
}

void loop() {
  // --- 1. Gửi Xung Kích Hoạt ---
  
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  // Gửi xung HIGH trong 10 micro giây để kích hoạt cảm biến
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // --- 2. Đo Thời Gian ---
  
  // Đo thời gian sóng đi và về
  duration = pulseIn(ECHO_PIN, HIGH);
  
  // --- 3. Tính Khoảng Cách ---
  
  // Khoảng cách (cm) = duration / 58
  distanceCm = duration / 58;

  // --- 4. In Kết Quả ---
  
  Serial.print("Khoang cach: ");
  if (distanceCm > 400 || distanceCm <= 0) {
    Serial.println("Ngoai pham vi hoac khong hop le");
  } else {
    Serial.print(distanceCm);
    Serial.println(" cm");
  }

  // Đợi một chút trước khi đo lần tiếp theo
  delay(500);
}