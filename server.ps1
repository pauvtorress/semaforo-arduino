# Servidor del semáforo (Arduino Uno) en PowerShell (sin Python/Node).
# Uso: powershell -ExecutionPolicy Bypass -File server.ps1 [puerto]
# Página: http://localhost:8000/   API: POST http://localhost:8000/api/state
# El Arduino Uno reporta por USB (ver arduino_uno_semaforo.ino) y
# serial_bridge.ps1 reenvía esas líneas al API.
param([int]$Port = 8000)

$Base = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$Durations = @{ red = 20; yellow = 5; green = 15 }
$Pins = @{ red = 2; yellow = 4; green = 5 }
$TimeoutS = 6
$State = @{ light = 'red'; remaining = 20.0; at = 0.0 }  # at = epoch del último POST
$Mime = @{ '.html' = 'text/html; charset=utf-8'; '.css' = 'text/css; charset=utf-8';
           '.js' = 'application/javascript; charset=utf-8'; '.json' = 'application/json' }

function Now-Epoch { [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() + [DateTimeOffset]::UtcNow.Millisecond / 1000.0 }

$listener = [System.Net.HttpListener]::new()
$listener.Prefixes.Add("http://localhost:$Port/")
$listener.Start()
Write-Host "Servidor semáforo en http://localhost:$Port/  (sirviendo $Base)"
Write-Host "API ESP32: POST http://<IP-de-este-equipo>:$Port/api/state"

try {
  while ($listener.IsListening) {
    $ctx = $listener.GetContext()
    $req = $ctx.Request; $res = $ctx.Response
    $res.Headers.Add('Access-Control-Allow-Origin', '*')
    $res.Headers.Add('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
    $res.Headers.Add('Access-Control-Allow-Headers', 'Content-Type')
    $send = {
      param([int]$code, [byte[]]$body, [string]$ctype)
      $res.StatusCode = $code
      $res.ContentType = $ctype
      $res.ContentLength64 = $body.Length
      $res.OutputStream.Write($body, 0, $body.Length)
      $res.OutputStream.Close()
    }
    try {
      if ($req.HttpMethod -eq 'OPTIONS') {
        $res.StatusCode = 204; $res.OutputStream.Close(); continue
      }
      $path = $req.Url.AbsolutePath
      if ($path -eq '/api/state') {
        if ($req.HttpMethod -eq 'GET') {
          $now = Now-Epoch
          $age = $now - $State.at
          $connected = ($age -le $TimeoutS) -and ($State.at -gt 0)
          $remaining = $Durations['red']
          if ($State.at -gt 0) { $remaining = [Math]::Max(0.0, $State.remaining - $age) }
          $payload = @{
            light = $State.light; remaining = [Math]::Round($remaining, 1)
            durations = $Durations; pins = $Pins; connected = $connected
            age = $(if ($State.at -gt 0) { [Math]::Round($age, 1) } else { $null })
            server_time = $now
          } | ConvertTo-Json -Compress
          &$send 200 ([Text.Encoding]::UTF8.GetBytes($payload)) $Mime['.json']
        } elseif ($req.HttpMethod -eq 'POST') {
          $raw = ''
          if ($req.HasEntityBody) {
            $reader = [IO.StreamReader]::new($req.InputStream, $req.ContentEncoding)
            $raw = $reader.ReadToEnd(); $reader.Close()
          }
          $light = $null; $remaining = $null
          if ($raw) {
            try {
              $data = $raw | ConvertFrom-Json
              $light = "$($data.light)".ToLower()
              if ($null -ne $data.remaining) { $remaining = [double]$data.remaining }
            } catch {}
          }
          if (-not $light -and $req.QueryString['light']) { $light = $req.QueryString['light'].ToLower() }
          if ($null -eq $remaining -and $req.QueryString['remaining']) { $remaining = [double]$req.QueryString['remaining'] }
          if (-not $Durations.ContainsKey($light)) {
            &$send 400 ([Text.Encoding]::UTF8.GetBytes('{"error":"light debe ser red|yellow|green"}')) $Mime['.json']
          } else {
            if ($null -eq $remaining) { $remaining = [double]$Durations[$light] }
            $State.light = $light; $State.remaining = [Math]::Max(0.0, $remaining); $State.at = Now-Epoch
            Write-Host ("[Placa] light={0} remaining={1:N1}s" -f $light, $remaining)
            &$send 200 ([Text.Encoding]::UTF8.GetBytes('{"ok":true}')) $Mime['.json']
          }
        } else { &$send 405 ([byte[]]@()) 'text/plain' }
        continue
      }
      # Archivos estáticos
      $rel = if ($path -in '/', '/index.html') { 'index.html' } else { $path.TrimStart('/') }
      $target = [IO.Path]::GetFullPath([IO.Path]::Combine($Base, $rel))
      if (-not $target.StartsWith($Base) -or -not [IO.File]::Exists($target)) {
        &$send 404 ([Text.Encoding]::UTF8.GetBytes('{"error":"not found"}')) $Mime['.json']
        continue
      }
      $ext = [IO.Path]::GetExtension($target).ToLower()
      $ctype = if ($Mime.ContainsKey($ext)) { $Mime[$ext] } else { 'application/octet-stream' }
      &$send 200 ([IO.File]::ReadAllBytes($target)) $ctype
    } catch { try { $res.OutputStream.Close() } catch {} }
  }
} finally { $listener.Stop() }
