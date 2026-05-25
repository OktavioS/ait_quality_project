#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include "DHT.h"

const char* serverName = "http://DenysPolitech.pythonanywhere.com/api/data";

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define SHARP_LED_PIN 5
#define SHARP_VO_PIN 0
#define FAN_PIN 7

const unsigned long FAN_ON_DURATION = 120000;
const unsigned long FAN_OFF_DURATION = 300000;

unsigned long stateStartTime = 0;
bool isFanActiveState = false;

float lastSentT = -100.0;
float lastSentH = -100.0;
float lastSentDust = -100.0;

void localLog(String message) {
  Serial.print("[");
  Serial.print(millis() / 1000);
  Serial.print("s] LOG: ");
  Serial.println(message);
}

void setup() {
  Serial.begin(115200);
  localLog("Start of Advanced State-Machine Air Monitor");

  dht.begin();
  pinMode(SHARP_LED_PIN, OUTPUT);
  digitalWrite(SHARP_LED_PIN, HIGH);

  pinMode(FAN_PIN, OUTPUT);

  analogWrite(FAN_PIN, 180);
  isFanActiveState = true;
  stateStartTime = millis();
  localLog("System started: Fan is ON (Purging & Stabilization phase)...");

  WiFiManager wm;
  if (!wm.autoConnect("AirMonitor_Setup", "12345678")) {
    localLog("Could not connect. Restart...");
    delay(3000);
    ESP.restart();
  }

  localLog("Connected to Wi-Fi!");
}

void loop() {
  unsigned long currentMillis = millis();

  if (isFanActiveState) {
    if (currentMillis - stateStartTime >= FAN_ON_DURATION) {

      float h = dht.readHumidity();
      float t = dht.readTemperature();

      digitalWrite(SHARP_LED_PIN, LOW);
      delayMicroseconds(280);
      int voMeasured = analogRead(SHARP_VO_PIN);
      delayMicroseconds(40);
      digitalWrite(SHARP_LED_PIN, HIGH);

      float calcVoltage = voMeasured * (3.3 / 4095.0);
      localLog("End of 2-min cycle. Raw Voltage: " + String(calcVoltage) + "V");

      float dustDensity = 0.172 * (calcVoltage - 2.20) * 1000.0;
      if (dustDensity < 0) dustDensity = 0.0;

      if (!isnan(h) && !isnan(t)) {
        localLog("Stable Data -> Temp: " + String(t) + "C, Hum: " + String(h) + "%, Dust: " + String(dustDensity));

        if (WiFi.status() == WL_CONNECTED) {
          HTTPClient http;
          http.begin(serverName);
          http.addHeader("Content-Type", "application/json");

          String jsonPayload = "{\"pm25\":" + String(dustDensity) + ",\"temperature\":" + String(t) + ",\"humidity\":" + String(h) + "}";
          int httpResponseCode = http.POST(jsonPayload);

          if (httpResponseCode == 200) {
            localLog("Data successfully uploaded to PythonAnywhere!");
          } else {
            localLog("Server error code: " + String(httpResponseCode));
          }
          http.end();
        } else {
          localLog("Wi-Fi disconnected, data not sent");
        }
      } else {
        localLog("Sensor reading error!");
      }

      analogWrite(FAN_PIN, 0);
      isFanActiveState = false;
      stateStartTime = currentMillis;
      localLog("Switching to ECO Mode: Fan is OFF. Silent phase for 5 minutes...");
    }
  }
  else {
    if (currentMillis - stateStartTime >= FAN_OFF_DURATION) {
      analogWrite(FAN_PIN, 180);
      isFanActiveState = true;
      stateStartTime = currentMillis;
      localLog("ECO Mode finished. Fan turned ON for a new 2-minute stabilization cycle...");
    }
  }
}