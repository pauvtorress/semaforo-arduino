#!/usr/bin/env python3
"""Servidor sencillo del semáforo (solo librería estándar, sin pip).

- Sirve la página web (index.html + styles.css + script.js) en http://<IP>:8000/
- Recibe la señal de la placa en POST /api/state  {"light":"red","remaining":17}
  (con Arduino Uno, serial_bridge.ps1 reenvía lo que el Uno manda por USB-serie)
- La página consulta GET /api/state cada 0.5 s y muestra la luz + cuenta regresiva.

Uso:
    python server.py [puerto]        # por defecto 8000
"""
import json
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse, parse_qs

BASE = Path(__file__).resolve().parent
DURATIONS = {"red": 20, "yellow": 5, "green": 15}
PINS = {"red": 2, "yellow": 4, "green": 5}
TIMEOUT_S = 6  # sin heartbeat del ESP32 en 6 s => se considera desconectado

# Último estado reportado por el ESP32
state = {"light": "red", "remaining": 20.0, "at": 0.0}  # at = time.time() del POST

MIME = {".html": "text/html; charset=utf-8", ".css": "text/css; charset=utf-8",
        ".js": "application/javascript; charset=utf-8", ".json": "application/json"}


class Handler(BaseHTTPRequestHandler):
    server_version = "SemaforoServer/1.0"

    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

    def _send(self, code, body: bytes, ctype="application/json"):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self._cors()
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204)
        self._cors()
        self.end_headers()

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/api/state":
            now = time.time()
            age = now - state["at"]
            connected = age <= TIMEOUT_S and state["at"] > 0
            remaining = max(0.0, state["remaining"] - age) if state["at"] > 0 else DURATIONS["red"]
            payload = {
                "light": state["light"],
                "remaining": round(remaining, 1),
                "durations": DURATIONS,
                "pins": PINS,
                "connected": connected,
                "age": round(age, 1) if state["at"] > 0 else None,
                "server_time": now,
            }
            self._send(200, json.dumps(payload).encode(), MIME[".json"])
            return
        # Archivos estáticos de la página
        rel = "index.html" if parsed.path in ("/", "/index.html") else parsed.path.lstrip("/")
        target = (BASE / rel).resolve()
        if not str(target).startswith(str(BASE)) or not target.is_file():
            self._send(404, b'{"error":"not found"}')
            return
        self._send(200, target.read_bytes(), MIME.get(target.suffix, "application/octet-stream"))

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path != "/api/state":
            self._send(404, b'{"error":"not found"}')
            return
        try:
            length = int(self.headers.get("Content-Length", 0))
            raw = self.rfile.read(length).decode("utf-8", "replace") if length else ""
        except Exception:
            raw = ""
        light, remaining = None, None
        if raw:
            try:
                data = json.loads(raw)
                light = str(data.get("light", "")).lower()
                if data.get("remaining") is not None:
                    remaining = float(data["remaining"])
            except Exception:
                pass
        # Alternativa simple sin JSON: POST /api/state?light=red&remaining=17
        if not light:
            qs = parse_qs(parsed.query)
            if "light" in qs:
                light = qs["light"][0].lower()
            if "remaining" in qs:
                try:
                    remaining = float(qs["remaining"][0])
                except ValueError:
                    pass
        if light not in DURATIONS:
            self._send(400, json.dumps({"error": "light debe ser red|yellow|green"}).encode())
            return
        if remaining is None:
            remaining = float(DURATIONS[light])  # aviso solo de cambio: asume duración completa
        state.update({"light": light, "remaining": max(0.0, remaining), "at": time.time()})
        print(f"[Placa] light={light} remaining={remaining:.1f}s", flush=True)
        self._send(200, b'{"ok":true}')


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    srv = ThreadingHTTPServer(("0.0.0.0", port), Handler)
    print(f"Servidor semáforo en http://0.0.0.0:{port}/  (sirviendo {BASE})")
    print(f"API ESP32: POST http://<IP-de-este-equipo>:{port}/api/state")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\nDetenido.")
