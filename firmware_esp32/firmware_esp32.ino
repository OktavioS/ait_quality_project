#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include "DHT.h"

const char* serverName = "http://DenysPolitech.pythonanywhere.com/api/data";

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define SHARP_LED_PIN 5
#define SHARP_VO_PIN 2

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 3600000;

float lastSentT = -100.0;
float lastSentH = -100.0;
float lastSentDust = -100.0;

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Запуск РОЗУМНОЇ Системи ---");

  dht.begin();
  pinMode(SHARP_LED_PIN, OUTPUT);
  digitalWrite(SHARP_LED_PIN, HIGH);

  WiFiManager wm;

  // Якщо захочеш "забути" збережений пароль, розкоментуй рядок нижче,
  // проший, потім знову закоментуй і проший ще раз:
  // wm.resetSettings();

  Serial.println("Шукаю збережені мережі...");


  if (!wm.autoConnect("AirMonitor_Setup", "12345678")) {
    Serial.println("❌ Не вдалося підключитися. Перезавантаження...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("\n✅ Підключено до Wi-Fi!");
  Serial.print("IP Адреса: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  digitalWrite(SHARP_LED_PIN, LOW);
  delayMicroseconds(280);
  int voMeasured = analogRead(SHARP_VO_PIN);
  delayMicroseconds(40);
  digitalWrite(SHARP_LED_PIN, HIGH);
  delayMicroseconds(9680);

  float calcVoltage = voMeasured * (3.3 / 4095.0);
  float dustDensity = (0.17 * calcVoltage - 0.47) * 1000.0;
  if (dustDensity < 0) dustDensity = 0.0;

  if (isnan(h) || isnan(t)) {
    Serial.println("⚠️ Помилка DHT11!");
    delay(2000);
    return;
  }

  bool tempChanged = abs(t - lastSentT) >= 1.0;     // Зміна на 1 градус
  bool humChanged = abs(h - lastSentH) >= 5.0;      // Зміна на 5%
  bool dustChanged = abs(dustDensity - lastSentDust) >= 11.0; // Стрибок пилу на 11 мкг

  bool timePassed = (millis() - lastSendTime) >= sendInterval;
  bool firstRun = (lastSendTime == 0);

  if (tempChanged || humChanged || dustChanged || timePassed || firstRun) {

      if(WiFi.status() == WL_CONNECTED){
        HTTPClient http;
        http.begin(serverName);
        http.addHeader("Content-Type", "application/json");

        String jsonPayload = "{\"pm25\":" + String(dustDensity) + ",\"temperature\":" + String(t) + ",\"humidity\":" + String(h) + "}";

        Serial.println("\n-------------------------");
        if(timePassed || firstRun) Serial.println("⏳ Відправка за розкладом (1 год)");
        else Serial.println("🚨 УВАГА! Різка зміна клімату! Екстрена відправка!");

        Serial.print("📤 Дані: "); Serial.println(jsonPayload);

        int httpResponseCode = http.POST(jsonPayload);

        if (httpResponseCode == 200) {
          Serial.println("✅ Успішно доставлено на сервер!");
          lastSentT = t;
          lastSentH = h;
          lastSentDust = dustDensity;
          lastSendTime = millis();
        } else {
          Serial.print("❌ Помилка: "); Serial.println(httpResponseCode);
        }
        http.end();
      }
  } else {
     Serial.print(".");
  }

  delay(5000);
}