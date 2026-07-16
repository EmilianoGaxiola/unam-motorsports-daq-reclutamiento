// ============================================================
// Sistema de Validacion de Plausibilidad APPS - UNAM Motorsports DAQ
// Reclutamiento - Reto 1
// ============================================================
// Requiere las librerias (Sketch > Include Library > Manage Libraries...):
//  - "ESP32Servo" de Kevin Harrington / madhephaestus
//  - "WebSockets" de Markus Sattler (Links2004)
#include <ESP32Servo.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebSocketsServer.h>
// WIFI_SSID y WIFI_PASSWORD viven en wifi_credentials.h (no se sube a git,
// ver .gitignore). Si no existe ese archivo en la carpeta del sketch, copia
// wifi_credentials.example.h como wifi_credentials.h y pon tus datos ahi.
#include "wifi_credentials.h"

// ---- WiFi ----
// El ESP32 se conecta a la red WiFi existente (modo estacion), asi tu
// laptop mantiene internet y ambos quedan en la misma red. Como el router
// puede darle una IP distinta cada vez, se anuncia por mDNS con un nombre
// fijo: "esp32-apps.local" (funciona en Chrome/Edge en Windows/Mac/Linux).
const char* MDNS_HOSTNAME = "esp32-apps";   // resultado: esp32-apps.local
WebSocketsServer webSocket(81);

// ---- Pines ----
const int PIN_APPS1 = 34, PIN_APPS2 = 35;
const int PIN_SERVO = 23;   // senal PWM al servo MG90S (rotacion continua)

// LEDs verdes simples (2 patas), uno por canal: encendido = canal normal,
// apagado = ese canal esta en modo falla simulada (override activo).
const int PIN_LED1 = 25;   // estado APPS1
const int PIN_LED2 = 33;   // estado APPS2

// ---- Calibracion por canal (medida con el monitor Serial, protoboard real) ----
// Potenciometros alimentados a 5V con una resistencia en serie entre el
// riel de 5V y un extremo de cada pot (el otro extremo va directo a GND,
// el wiper directo al GPIO). Esa resistencia + el propio pot forman un
// divisor que limita el voltaje maximo del wiper por debajo de 3.3V.
// Rserie distinta por canal (6.8k en APPS1, 15k en APPS2) = asimetria
// real en el MAXIMO de cada canal.
//
// El rawMin distinto entre canales de aqui abajo es SOLO ventana de
// calibracion por software (fisicamente el pot si puede llegar a 0V en
// ambos canales) -- no hay resistencia extra en el extremo bajo. Esto
// crea asimetria en el reporte 0-100% sin tener que rearmar el cableado,
// pero OJO: a diferencia de un divisor fisico en ambos extremos, esto NO
// permite distinguir un corto real a tierra de la posicion minima normal
// del pedal (ambos leerian raw=0 -> se reportan igual). Suficiente para
// esta demo de reclutamiento, pero es una simplificacion consciente, no
// una deteccion real de corto a tierra.
struct Calib { int rawMin; int rawMax; bool invert; };
Calib CAL1 = {150, 2239, false};
Calib CAL2 = {300, 3401, false};

const float FAULT_THRESHOLD_PCT = 10.0;   // desacuerdo maximo tolerado
const unsigned long PERSIST_MS = 100;

// Servo de rotacion continua (MG90S 360): no tiene angulo, gira segun el
// ancho de pulso. ~1500us = detenido (punto muerto, puede variar segun el
// servo -- si al subir el codigo no queda quieto, ajusta SERVO_STOP_US en
// pasos de 10-20us hasta encontrar el punto muerto real de tu servo).
const int SERVO_STOP_US = 1500;
const int SERVO_RUN_US  = 1700;
Servo motorServo;

bool overrideCh1 = false, overrideCh2 = false;
unsigned long faultStart = 0;
bool faultConfirmed = false;

float toPercent(int raw, Calib c) {
  raw = constrain(raw, c.rawMin, c.rawMax);
  float pct = (raw - c.rawMin) * 100.0 / (c.rawMax - c.rawMin);
  return c.invert ? 100.0 - pct : pct;
}

// Aplica un comando de texto plano -- llega igual por Serial (cable) o
// por WebSocket (WiFi), ambos caminos usan esta misma funcion.
// OVR1:1 / OVR1:0 / OVR2:1 / OVR2:0
void applyCommand(String line) {
  line.trim();
  if (line == "OVR1:1")      { overrideCh1 = true;  digitalWrite(PIN_LED1, LOW);  }
  else if (line == "OVR1:0") { overrideCh1 = false; digitalWrite(PIN_LED1, HIGH); }
  else if (line == "OVR2:1") { overrideCh2 = true;  digitalWrite(PIN_LED2, LOW);  }
  else if (line == "OVR2:0") { overrideCh2 = false; digitalWrite(PIN_LED2, HIGH); }
}

void checkSerialCommands() {
  while (Serial.available()) {
    applyCommand(Serial.readStringUntil('\n'));
  }
}

// Comandos que llegan por WebSocket (mismo formato de texto plano que Serial).
void webSocketEvent(uint8_t clientId, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) {
    applyCommand(String((char*)payload).substring(0, length));
  }
}

// Arma la linea de telemetria JSON -- se usa igual por Serial y WebSocket.
String buildTelemetryJson(int raw1, int raw2, float pct1, float pct2, unsigned long faultMs) {
  String json = "{\"t\":" + String(millis());
  json += ",\"apps1_raw\":" + String(raw1);
  json += ",\"apps2_raw\":" + String(raw2);
  json += ",\"apps1_pct\":" + String(pct1, 1);
  json += ",\"apps2_pct\":" + String(pct2, 1);
  json += ",\"fault\":" + String(faultConfirmed ? "true" : "false");
  json += ",\"fault_ms\":" + String(faultMs);
  json += ",\"override1\":" + String(overrideCh1 ? "true" : "false");
  json += ",\"override2\":" + String(overrideCh2 ? "true" : "false");
  json += "}";
  return json;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  digitalWrite(PIN_LED1, HIGH);   // arranca prendido = canal normal
  digitalWrite(PIN_LED2, HIGH);
  motorServo.attach(PIN_SERVO);
  motorServo.writeMicroseconds(SERVO_STOP_US);   // arranca detenido (fail-safe)

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.print("\nWiFi conectado. IP: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin(MDNS_HOSTNAME)) {
    Serial.print("mDNS activo: http://");
    Serial.print(MDNS_HOSTNAME);
    Serial.println(".local");
  }

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();
  checkSerialCommands();

  int raw1 = analogRead(PIN_APPS1);
  int raw2 = analogRead(PIN_APPS2);

  float pct1 = overrideCh1 ? 0.0 : toPercent(raw1, CAL1);
  float pct2 = overrideCh2 ? 0.0 : toPercent(raw2, CAL2);

  float delta = abs(pct1 - pct2);
  unsigned long faultMs = 0;
  if (delta > FAULT_THRESHOLD_PCT) {
    if (faultStart == 0) faultStart = millis();
    faultMs = millis() - faultStart;
    faultConfirmed = faultMs > PERSIST_MS;
  } else {
    faultStart = 0; faultMs = 0; faultConfirmed = false;
  }

  motorServo.writeMicroseconds(faultConfirmed ? SERVO_STOP_US : SERVO_RUN_US);

  String json = buildTelemetryJson(raw1, raw2, pct1, pct2, faultMs);
  Serial.println(json);
  webSocket.broadcastTXT(json);

  delay(10);   // ~100 muestras/segundo
}
