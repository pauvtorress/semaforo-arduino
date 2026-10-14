# Puente serie: Arduino Uno (USB) -> servidor de la pagina.
# Lee lineas "STATE <luz> <segundos>" del puerto COM y las reenvia con
# POST http://localhost:8000/api/state para actualizar la pagina.
#
# Uso: powershell -ExecutionPolicy Bypass -File serial_bridge.ps1 [COM4]
# Si omites el puerto y solo hay uno disponible, lo usa automaticamente.
param([string]$Port = '', [int]$Baud = 9600, [string]$Server = 'http://localhost:8000')

$ports = [IO.Ports.SerialPort]::GetPortNames()
if (-not $Port) {
  if ($ports.Count -eq 1) { $Port = $ports[0] }
  else {
    Write-Host ("Puertos serie disponibles: " + ($ports -join ', '))
    Write-Host "Uso: .\serial_bridge.ps1 COM4   (mira el Administrador de dispositivos)"
    exit 1
  }
}

$sp = New-Object IO.Ports.SerialPort $Port, $Baud
$sp.NewLine = "`n"
$sp.ReadTimeout = 2000
$sp.Open()
Write-Host ("Puente serie " + $Port + " (" + $Baud + " baudios) -> " + $Server + "/api/state")

try {
  while ($true) {
    try {
      $line = $sp.ReadLine().Trim()
    } catch [TimeoutException] { continue }
    if (-not $line) { continue }
    if ($line -match '^STATE\s+(red|yellow|green)\s+([\d.]+)') {
      $body = '{"light":"' + $Matches[1] + '","remaining":' + $Matches[2] + '}'
      try {
        Invoke-WebRequest -Uri ($Server + "/api/state") -Method POST `
          -ContentType 'application/json' -Body $body -UseBasicParsing | Out-Null
      } catch { Write-Host ("[AVISO] sin servidor en " + $Server + " - corre server.ps1?") }
    } else {
      Write-Host ("[SERIE] " + $line)
    }
  }
} finally { $sp.Close() }
