#include <Arduino.h>
int PHOTODIODE_PIN = 32;

void setup() {
  Serial.begin(9600);
  pinMode(4, INPUT_PULLUP);
}

void loop() {
  int sensorValue = analogRead(PHOTODIODE_PIN);
  
  // ĐẢO NGƯỢC GIÁ TRỊ
  int invertedValue = 4095 - sensorValue;
  
  float voltage = invertedValue * (3.3 / 4095.0);
  
  Serial.print("Giá trị gốc: ");
  Serial.print(sensorValue);
  Serial.print(" - Giá trị đảo: ");
  Serial.print(invertedValue);
  Serial.print(" - Điện áp: ");
  Serial.print(voltage);
  Serial.println("V");
  
  delay(500);
}