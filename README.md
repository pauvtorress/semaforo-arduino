# Semáforo con Arduino Uno + Página Web

Réplica del diseño Figma (`Status` + semáforo vertical) conectada a un Arduino Uno real.

## Archivos

| Archivo | Qué es |
|---|---|
| `index.html` / `styles.css` / `script.js` | Interfaz web: luz activa del Arduino + cuenta regresiva del cambio |
| `arduino_uno_semaforo.ino` | Firmware del Arduino Uno (reporta por USB-serie) |
| `server.ps1` | Servidor (PowerShell, sin instalar nada) |
| `serial_bridge.ps1` | Puente: lee el USB del Arduino y lo reenvía al servidor |
| `server.py` / `esp32_semaforo.ino` | Alternativa con Python / ESP32 (opcional) |

## Cableado

- Rojo → **pin 2** (+ resistencia 220 Ω a GND), 20 s
- Amarillo → **pin 4**, 5 s
- Verde → **pin 5**, 15 s
- Secuencia: verde (15 s) → amarillo (5 s) → rojo (20 s) → …
- Uno al PC por cable USB.

## Puesta en marcha

1. **Arduino:** abre `arduino_uno_semaforo.ino` en Arduino IDE (placa "Arduino Uno",
   puerto COM correcto) y súbelo. En el Monitor Serie (9600) verás
   `STATE green 14.2 ...`.
2. **Servidor** (ya corre en `http://localhost:8000/`; para relanzarlo):
   ```powershell
   cd semaforo
   powershell -ExecutionPolicy Bypass -File server.ps1 8000
   ```
3. **Puente serie** (mira tu COM en Administrador de dispositivos → Puertos):
   ```powershell
   cd semaforo
   powershell -ExecutionPolicy Bypass -File serial_bridge.ps1 COM3
   ```
4. **Página:** abre `http://localhost:8000/`.
   Verás `● Arduino conectado` y la cuenta regresiva en vivo.
   Sin Arduino, la página entra en `● Modo simulación` con los mismos tiempos.

## Cómo fluyen los datos

```
Uno (pines + millis) --USB "STATE green 12.3" cada 1 s--> serial_bridge.ps1
serial_bridge.ps1 --POST /api/state--> server.ps1
página --GET /api/state cada 0.5 s--> luz + "Cambia en X s"
```
