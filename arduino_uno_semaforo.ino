/*
  Semáforo con Arduino Uno + página web.
  -------------------------------------
  - Rojo     -> pin 2,  20 s
  - Amarillo -> pin 4,   5 s
  - Verde    -> pin 5,  15 s
  - Secuencia: VERDE (15 s) -> AMARILLO (5 s) -> ROJO (20 s) -> ...

  El Uno no tiene WiFi: reporta su estado por el puerto SERIE (USB) con líneas:
      STATE green 14.2
      STATE yellow 3.0
  y el script serial_bridge.ps1 del PC las reenvía al servidor
  (POST http://localhost:8000/api/state) para actualizar la página.

  Conexión: cada LED del pin a GND con resistencia de 220 ohm en serie.
  Monitor Serie a 9600 baudios para depurar.
*/

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

// Reporta el estado + segundos restantes por el puerto serie
void sendState() {
  unsigned long elapsed = millis() - stateStart;
  unsigned long total = lightDuration(current);
  float remaining = (elapsed >= total) ? 0 : (total - elapsed) / 1000.0;
  Serial.print("STATE ");
  Serial.print(lightName(current));
  Serial.print(" ");
  Serial.println(remaining, 1);
}

void setLight(Light l) {
  current = l;
  stateStart = millis();
  lastHeartbeat = millis();
  applyPins();
  sendState();
}

void setup() {
  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  Serial.begin(9600);
  Serial.println("HELLO arduino-uno");
  setLight(L_GREEN);  // arranca en verde
}

void loop() {
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
