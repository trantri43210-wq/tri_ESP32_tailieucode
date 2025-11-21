#include <Arduino.h>
#include<WiFi.h>

// put function declarations here:
const char *ssid = "Natech";
const char *password = "natech888";
unsigned long dem =0;
unsigned long dat =5000;
void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid,password);
  Serial.print ("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED);

  {
    Serial.print (".");
    delay(1000);
  }
  Serial.println(WiFi.localIP());

  
}

void setup() {
  Serial.begin(9600);
  initWiFi();
  Serial.println("RSSI:  ");
  Serial.print(WiFi.RSSI());
}

void loop() {
  unsigned long curren = millis();
  if(WiFi.status()!= WL_CONNECTED && (curren - dem >= dat))
  {
    Serial.println(millis());
    Serial.println("Connecting to WIFi ...");
    WiFi.disconnect();
    WiFi.reconnect();
    dem = curren;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("RSSI:  ");
    Serial.print(WiFi.RSSI());
    delay(1000);
    }
  
 
}

// CODE  NẾU MÀ WIFI CU KET NỐI LẠI NHIÊU LẦN KO ĐƯỢC THÌ CHUYỂN SANG BẮT MẠNG DỰ PHÒNG
/*
#include <WiFi.h>

const char* ssid1 = "Natech";        // WiFi chính
const char* pass1 = "natech888";

const char* ssid2 = "BackupWiFi";    // WiFi dự phòng
const char* pass2 = "12345678";

unsigned long lastTry = 0;
unsigned long retryInterval = 5000;  // Thử lại sau 5 giây
int currentWiFi = 1;  // đang dùng WiFi 1

void connectWiFi(const char* ssid, const char* pass) {
  Serial.print("Dang ket noi den: ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  unsigned long startAttempt = millis();

  // chờ trong 7 giây
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 7000) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nDa ket noi!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nKet noi that bai!");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);

  // Thử WiFi chính
  connectWiFi(ssid1, pass1);

  // Nếu không được → thử WiFi phụ
  if (WiFi.status() != WL_CONNECTED) {
    currentWiFi = 2;
    connectWiFi(ssid2, pass2);
  }
}

void loop() {
  unsigned long now = millis();

  // Nếu đang mất kết nối quá lâu → thử mạng khác
  if (WiFi.status() != WL_CONNECTED && (now - lastTry >= retryInterval)) {

    Serial.println("Mat ket noi! Thu ket noi lai...");

    if (currentWiFi == 1) {
      // thử lại WiFi chính trước
      connectWiFi(ssid1, pass1);
      if (WiFi.status() != WL_CONNECTED) {
        // thử qua WiFi phụ
        currentWiFi = 2;
        connectWiFi(ssid2, pass2);
      }
    } else {
      // đang dùng WiFi phụ → thử lại WiFi phụ trước
      connectWiFi(ssid2, pass2);
      if (WiFi.status() != WL_CONNECTED) {
        // thử lại WiFi chính
        currentWiFi = 1;
        connectWiFi(ssid1, pass1);
      }
    }

    lastTry = now;
  }

  // In RSSI khi kết nối
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
  }

  delay(1000);
}

*/