#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "time.h"

// Firebase Add-ons
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// WiFi credentials
#define WIFI_SSID "ABCDssss"
#define WIFI_PASSWORD "gggggggg"

// Firebase project credentials
#define API_KEY ""
#define USER_EMAIL ""
#define USER_PASSWORD ""
#define DATABASE_URL ""

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String uid;
String databasePath;
String tempPath = "/temperature";
String humPath = "/humidity";
String presPath = "/pressure";
String timePath = "/timestamp";
String notePath = "/note";

String parentPath;
int timestamp;
FirebaseJson json;

const char* ntpServer = "pool.ntp.org";

// BME280
Adafruit_BME280 bme;
float temperature;
float humidity;
float pressure;

// Buzzer pin
#define BUZZER_PIN D5

// Timer: 10 seconds
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 10000;

void initBME() {
  if (!bme.begin(0x76)) {
    Serial.println("BME280 not found!");
    while (1);
  }
}

void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println("\nWiFi Connected: " + WiFi.localIP().toString());
}

unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return 0;
  time(&now);
  return now;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);

  initBME();
  initWiFi();
  configTime(0, 0, ntpServer);

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);
  Firebase.begin(&config, &auth);

  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  uid = auth.token.uid.c_str();
  Serial.print("\nUser UID: ");
  Serial.println(uid);

  databasePath = "/UsersData/" + uid + "/readings";
}

void loop() {
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    pressure = bme.readPressure() / 100.0F;
    timestamp = getTime();

    parentPath = databasePath + "/" + String(timestamp);

    // Buat pesan status
    String note;
    if (temperature > 32) {
      note = "Suhu terlalu tinggi, pendingin dinyalakan";
      tone(BUZZER_PIN, 1000);  // Buzzer ON dengan nada 1000 Hz
    } else if (temperature < 25) {
      note = "Suhu terlalu rendah, pemanas dinyalakan";
      tone(BUZZER_PIN, 1000);  // Buzzer ON dengan nada 1000 Hz
    } else {
      note = "Suhu normal";
      noTone(BUZZER_PIN);  // Buzzer OFF
    }

    // Kirim ke Firebase
    json.set(tempPath.c_str(), String(temperature));
    json.set(humPath.c_str(), String(humidity));
    json.set(presPath.c_str(), String(pressure));
    json.set(timePath.c_str(), String(timestamp));
    json.set(notePath.c_str(), note);

    Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
    Serial.println("Status: " + note);
  }
}