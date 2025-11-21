#include <Arduino.h>
#include<WiFi.h>
const char *ssid = "Natech";
const char *password = "natech888";

IPAddress loacal_IP(192,168,1,200);
IPAddress gat(192,168,1,1);
IPAddress subnet(255,255,255,0);
IPAddress prima(8,8,8,8);
IPAddress secon(8,8,4,4);


void setup() {
  Serial.begin(9600);
  if (!WiFi.config(loacal_IP,gat,subnet,prima,secon))
  {
    Serial.println("STA Failed to configure");
  }
  Serial.println("Connecting to ");
  Serial.print (ssid);
  WiFi.begin(ssid,password);
  while (WiFi.status() != WL_CONNECTED)
  {
    /* code */
    delay(500);
    Serial.print (".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP Address: ");
  Serial.print(WiFi.localIP());
  
  
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("Cuong do tin hieu (RIIS): ");
  Serial.print (WiFi.RSSI());
  delay(500);
}


