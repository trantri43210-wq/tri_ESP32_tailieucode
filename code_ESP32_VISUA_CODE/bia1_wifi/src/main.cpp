#include <Arduino.h>
#include <WiFi.h>


void setup() {
     Serial.begin(9600);

     WiFi.mode(WIFI_STA);
     WiFi.disconnect();

     Serial.println("Setup done");
     delay(2000);

}

void loop() {
     Serial.println("Svan Start");
     int n = WiFi.scanNetworks();
     if(n==0)
     {
      Serial.println("No Wifi found");
     }
     else{
      Serial.println("Found: " + String(n) + "WiFi");
      for(int i = 0; i<n; i++)
      {
      
      Serial.println(String(i+1)+":" + WiFi.SSID(i) + "(" + WiFi.RSSI(i) + ")");

      }
      

     }
     delay(5);
    }

