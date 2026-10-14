/*
  Semáforo con ESP32 + página web.
  --------------------------------
  - Rojo    -> GPIO 2,  20 s
  - Amarillo-> GPIO 4,   5 s
  - Verde   -> GPIO 5,  15 s
  - Secuencia: VERDE (15 s) -> AMARILLO (5 s) -> ROJO (20 s) -> ...
  - En cada cambio y cada 1 s envía POST http://SERVER_IP:8000/api/state
    con {"light":"red|yellow|green","remaining":segundos}
    para que el servidor actualice la página con la cuenta regresiva.

  Conexión (LED + resistencia 220 ohm a GND en cada pin).
  Configura abajo tu WiFi y la IP de este equipo (donde corre `python server.py`).
*/

#include <WiFi.h>
#include <HTTPClient.h>

// ===== CONFIGURA ESTO =====
const char* WIFI_SSID = "TU_WIFI";
const char* WIFI_PASS = "TU_PASSWORD";
const char* SERVER_IP = "192.168.1.100";  // IP de este equipo (ipconfig)
const int   SERVER_PORT = 8000;
// ==========================

const int RED_PIN    = 2;  // requerido
const int YELLOW_PIN = 4;
const int GREEN_PIN  = 5;

const unsigned long GREEN_MS  = 15000;  // 15 s
const unsigned long YELLOW_MS = 5000;   // 5 s
const unsigned long RED_MS    = 20000;  // 20 s

enum Light { L_GREEN, L_YELLOW, L_RED };
Light current = L_GREEN;
unsigned long stateStart = 0;
unsigned long lastHeartbeat = 0;

const char* lightName(Light l) {
  return l == L_GREEN ? "green" : (l == L_YELLOW ? "yellow" : "red");
}

unsigned long lightDuration(Light l) {
  return l == L_GREEN ? GREEN_MS : (l == L_YELLOW ? YELLOW_MS : RED_MS);
}

void applyPins() {
  digitalWrite(RED_PIN,    current == L_RED    ? HIGH : LOW);
  digitalWrite(YELLOW_PIN, current == L_YELLOW ? HIGH : LOW);
  digitalWrite(GREEN_PIN,  current == L_GREEN  ? HIGH : LOW);
}

// Envía el estado actual + segundos restantes al servidor del PC
void sendState() {
  if (WiFi.status() != WL_CONNECTED) return;
  unsigned long elapsed = millis() - stateStart;
  unsigned long total = lightDuration(current);
  float remaining = (elapsed >= total) ? 0 : (total - elapsed) / 1000.0;

  HTTPClient http;
  String url = String("http://") + SERVER_IP + ":" + SERVER_PORT + "/api/state";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"light\":\"") + lightName(current)
              + String("\",\"remaining\":") + String(remaining, 1) + String("}");
  int code = http.POST(body);
  Serial.printf("[HTTP] POST %s -> %d\n", body.c_str(), code);
  http.end();
}

void setLight(Light l) {
  current = l;
  stateStart = millis();
  lastHeartbeat = millis();
  applyPins();
  Serial.printf("[SEMAFORO] %s (%lu s)\n", lightName(current), lightDuration(current) / 1000);
  sendState();
}

void setup() {
  Serial.begin(115200);
  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);

  Serial.printf("Conectando a WiFi %s ...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWiFi OK. IP ESP32: %s -> servidor http://%s:%d\n",
                WiFi.localIP().toString().c_str(), SERVER_IP, SERVER_PORT);

  setLight(L_GREEN);  // arranca en verde
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(1000);
    return;
  }
  unsigned long now = millis();

  // Cambio de estado al cumplirse la duración
  if (now - stateStart >= lightDuration(current)) {
    if (current == L_GREEN)       setLight(L_YELLOW);
    else if (current == L_YELLOW) setLight(L_RED);
    else                          setLight(L_GREEN);
  }
  // Heartbeat 1 s para la cuenta regresiva en vivo
  else if (now - lastHeartbeat >= 1000) {
    lastHeartbeat = now;
    sendState();
  }
  delay(50);
}
