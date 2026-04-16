// Import Libary
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// Konfigurasi WIFI
const char* ssid = "1am";
const char* password = "mimpi123";
const char* mqtt_server = "broker.emqx.io";

// Konfigurasi pin
#define WATER_SENSOR 34
#define BUZZER 17
#define SERVO_PIN 26

WiFiClient espClient;
PubSubClient client(espClient);
Servo servo;

// state
bool isAutoMode = true;
bool buzzerState = false;
int servoAngle = 0;

int servoStep = 0;

unsigned long lastBlink = 0;
unsigned long lastSend = 0;

// Koneksi ke wifi
void connectWiFi() {
  Serial.print("WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println(" CONNECTED");
}

// fungsi untuk ngontrol servo
void setServo(int angle) {
  servo.write(angle);
}

// fungsi untu set buzzzer on/of
void setBuzzer(bool state) {
  digitalWrite(BUZZER, state ? HIGH : LOW);
}

// fungsi untuk baca water sensor
int readWater() {
  return analogRead(WATER_SENSOR);
}

// fungsi untuk mengambil status level air
String getStatus(int val) {
  if (val <= 800) return "AMAN";
  else if (val <= 1500) return "WASPADA";
  else return "BAHAYA";
}

// fungsi untuk kontrol auto mode
void autoControl(String status) {

  if (status == "AMAN") {
    servoAngle = 0;
    buzzerState = false;
  }
  else if (status == "WASPADA") {
    servoAngle = 90;
    buzzerState = false;
  }
  else {
    servoAngle = 180;

    if (millis() - lastBlink > 500) {
      lastBlink = millis();
      buzzerState = !buzzerState;
    }
  }

  setServo(servoAngle);
  setBuzzer(buzzerState);
}

// fungsi untuk control manual
void manualControl() {
  setServo(servoAngle);
  setBuzzer(buzzerState);
}

// fungsi untuk handdle mqqtt
void handleControl(String topic, String message) {

  message.trim();

  Serial.println("\n=== MQTT ===");
  Serial.println(topic + " : " + message);

  // ===== MODE =====
  if (topic == "arif/iot/mode") {
    if (message == "AUTO") {
      isAutoMode = true;
    }
    else if (message == "MANUAL") {
      isAutoMode = false;
    }
  }

  else if (topic == "arif/iot/servo_btn") {
    if (!isAutoMode) {

      if (message == "TOGGLE") {

        if (servoStep == 0) {
          servoAngle = 0;
          servoStep = 1;
        }
        else if (servoStep == 1) {
          servoAngle = 90;
          servoStep = 2;
        }
        else if (servoStep == 2) {
          servoAngle = 180;
          servoStep = 3;
        }
        else if (servoStep == 3) {
          servoAngle = 90;
          servoStep = 4;
        }
        else {
          servoAngle = 0;
          servoStep = 0;
        }

        setServo(servoAngle);
      }
    }
  }

  else if (topic == "arif/iot/buzzer") {
    if (!isAutoMode) {

      if (message == "ON") buzzerState = true;
      else if (message == "OFF") buzzerState = false;

      setBuzzer(buzzerState);
    }
  }
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  handleControl(String(topic), msg);
}

// fungsi untuk koneksi ke mqtt broker
void connectMQTT() {
  while (!client.connected()) {
    Serial.print("MQTT...");
    if (client.connect("ESP32_BENDUNGAN")) {
      Serial.println(" CONNECTED");

      client.subscribe("arif/iot/mode");
      client.subscribe("arif/iot/servo_btn");
      client.subscribe("arif/iot/buzzer");

    } else {
      Serial.println(" FAILED");
      delay(2000);
    }
  }
}

// fungsi publish data ke mqtt
void publishData(int value, String status) {

  client.publish("arif/iot/nilai", String(value).c_str());
  client.publish("arif/iot/status", status.c_str());
  client.publish("arif/iot/servo_status", String(servoAngle).c_str());
  client.publish("arif/iot/buzzer_status", buzzerState ? "ON" : "OFF");
  client.publish("arif/iot/mode_status", isAutoMode ? "AUTO" : "MANUAL");
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER, OUTPUT);

  servo.attach(SERVO_PIN);
  servo.write(0);

  connectWiFi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);

  Serial.println("SYSTEM READY");
}

void loop() {

  if (!client.connected()) connectMQTT();
  client.loop();

  int waterValue = readWater();
  String status = getStatus(waterValue);

  if (isAutoMode) autoControl(status);
  else manualControl();

  if (millis() - lastSend > 1000) {
    lastSend = millis();
    publishData(waterValue, status);

    Serial.println("\n=== DATA ===");
    Serial.print("Air    : "); Serial.println(waterValue);
    Serial.print("Status : "); Serial.println(status);
    Serial.print("Mode   : "); Serial.println(isAutoMode ? "AUTO" : "MANUAL");
    Serial.print("Servo  : "); Serial.println(servoAngle);
    Serial.print("Step   : "); Serial.println(servoStep);
    Serial.print("Buzzer : "); Serial.println(buzzerState ? "ON" : "OFF");
  }
}