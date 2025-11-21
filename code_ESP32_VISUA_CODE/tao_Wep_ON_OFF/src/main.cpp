#include <Arduino.h>
#include <WiFi.h>

// --- Thông tin WiFi ---
const char* ssid     = "Natech";
const char* password = "natech888";

// --- Web server cổng 80 ---
WiFiServer server(80);

// --- LED tích hợp GPIO2 ---
const int ledPin = 2;
bool ledState = false;

// --- Password để điều khiển LED ---
const String controlPassword = "1234";

// --- Hàm gửi trang HTML ---
void sendWebPage(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<title>ESP32 LED Control</title></head><body>");
  client.println("<h1>ESP32 LED Control (GPIO2)</h1>");
  client.print("<p>LED State: ");
  client.print(ledState ? "ON" : "OFF");
  client.println("</p>");

  // Nút bấm ON/OFF
  if (!ledState)
    client.println("<p><a href=\"/on?pwd=" + controlPassword + "\"><button style='font-size:20px;padding:10px;'>TURN ON</button></a></p>");
  else
    client.println("<p><a href=\"/off?pwd=" + controlPassword + "\"><button style='font-size:20px;padding:10px;'>TURN OFF</button></a></p>");

  client.println("</body></html>");
}

// --- Hàm xử lý client ---
void handleClient(WiFiClient &client) {
  String currentLine = "";
  String request = "";
  unsigned long startTime = millis();

  while (client.connected() && millis() - startTime < 5000) {
    if (client.available()) {
      char c = client.read();
      request += c;
      if (c == '\n') {
        if (currentLine.length() == 0) {

          // --- Kiểm tra lệnh ON/OFF qua URL ---
          if (request.indexOf("/on?pwd=" + controlPassword) >= 0) {
            ledState = true;
            digitalWrite(ledPin, HIGH);
            Serial.println("LED ON");
          }
          else if (request.indexOf("/off?pwd=" + controlPassword) >= 0) {
            ledState = false;
            digitalWrite(ledPin, LOW);
            Serial.println("LED OFF");
          }

          // --- Gửi giao diện ---
          sendWebPage(client);
          break;
        } else {
          currentLine = "";
        }
      } else if (c != '\r') {
        currentLine += c;
      }
    }
  }
  client.stop();
  Serial.println("Client disconnected");
}

void setup() {
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // Kết nối WiFi
  Serial.print("Connecting to WiFi ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New Client");
    handleClient(client);
  }
}
