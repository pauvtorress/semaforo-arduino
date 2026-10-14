// Tiempos idénticos al firmware del Arduino Uno (segundos)
const DURATIONS = { red: 20, yellow: 5, green: 15 };
const LABELS = { red: 'Stop — Alto', yellow: 'Caution — Precaución', green: 'Go — Avance' };
const COLORS = { red: 'var(--red)', yellow: '#9a8200', green: 'var(--green)' };
// Secuencia del semáforo real: verde -> amarillo -> rojo -> (verde...)
const ORDER = ['green', 'yellow', 'red'];

const statusValue = document.getElementById('statusValue');
const countdownEl = document.getElementById('countdown');
const progressBar = document.getElementById('progressBar');
const connBadge = document.getElementById('connBadge');
const srcInfo = document.getElementById('srcInfo');
const lights = {
  red: document.getElementById('lightRed'),
  yellow: document.getElementById('lightYellow'),
  green: document.getElementById('lightGreen'),
};

function render(light, remaining, connected) {
  const total = DURATIONS[light] || 1;
  const rem = Math.max(0, remaining);
  for (const [name, el] of Object.entries(lights)) {
    el.classList.toggle('active', name === light);
  }
  statusValue.textContent = LABELS[light] || light;
  statusValue.style.color = COLORS[light] || '#111';
  countdownEl.textContent = Math.ceil(rem);
  progressBar.style.width = `${(100 * (total - rem)) / total}%`;
  progressBar.style.background = COLORS[light] || '#111';

  if (connected) {
    connBadge.textContent = '● Arduino conectado';
    connBadge.className = 'badge live';
    srcInfo.textContent = 'Datos en vivo del Arduino (USB → puente → /api/state cada 1 s).';
  } else {
    connBadge.textContent = '● Modo simulación';
    connBadge.className = 'badge demo';
    srcInfo.textContent = 'Sin señal del Arduino: simulando con los mismos tiempos (20/5/15 s). Sube el .ino, corre el servidor y el puente serie.';
  }
}

// --- Modo simulación local (mismos tiempos que el ESP32) ---
let simLight = 'red';
let simEndsAt = Date.now() + DURATIONS.red * 1000;
function simTick() {
  const rem = (simEndsAt - Date.now()) / 1000;
  if (rem <= 0) {
    simLight = ORDER[(ORDER.indexOf(simLight) + 1) % ORDER.length];
    simEndsAt = Date.now() + DURATIONS[simLight] * 1000;
  }
  return { light: simLight, remaining: Math.max(0, (simEndsAt - Date.now()) / 1000) };
}

// --- Polling al servidor (que refleja lo que manda el Arduino vía el puente) ---
async function poll() {
  try {
    const res = await fetch('/api/state', { cache: 'no-store' });
    if (!res.ok) throw new Error('http ' + res.status);
    const data = await res.json();
    if (data.connected) {
      render(data.light, data.remaining, true);
      return;
    }
  } catch (_) {
    // Sin servidor (p. ej. abierto con file://): usar simulación
  }
  const sim = simTick();
  render(sim.light, sim.remaining, false);
}

render('red', DURATIONS.red, false);
setInterval(poll, 500);
poll();
