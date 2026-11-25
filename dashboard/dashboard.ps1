# dashboard.ps1 — Genera un HTML comparativo con datos reales del sistema
# Ejecutar desde la VM (PowerShell hooked o no; el script detecta ambos)
# Uso: .\dashboard.ps1 [-LabPath C:\lab] [-Prefix '$rk'] [-OutFile dashboard.html]

param(
    [string]$LabPath = "C:\lab",
    [string]$Prefix  = '$rk',
    [string]$OutFile = "dashboard.html",
    [switch]$NoOpen
)

$ErrorActionPreference = "SilentlyContinue"
$timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

Write-Host "[*] Recogiendo datos del sistema..." -ForegroundColor Cyan

# --- 1. Procesos (vista de esta shell) ---
Write-Host "    Procesos (Get-Process)..."
$procs = Get-Process | Select-Object Id, ProcessName | Sort-Object ProcessName
$procLines = @()
foreach ($p in $procs) {
    $hidden = $p.ProcessName -like "$Prefix*"
    $procLines += @{ pid = $p.Id; name = $p.ProcessName; hidden = $hidden }
}
$totalProcs = $procs.Count
$hiddenProcs = ($procs | Where-Object { $_.ProcessName -like "$Prefix*" }).Count

# --- 2. Ficheros (vista de esta shell) ---
Write-Host "    Ficheros (Get-ChildItem $LabPath)..."
$files = @()
if (Test-Path $LabPath) {
    $items = Get-ChildItem -Path $LabPath -Force 2>$null
    foreach ($f in $items) {
        $hidden = $f.Name -like "$Prefix*"
        $files += @{
            name    = $f.Name
            size    = if ($f.PSIsContainer) { "&lt;DIR&gt;" } else { "{0:N0}" -f $f.Length }
            date    = $f.LastWriteTime.ToString("dd/MM/yyyy HH:mm")
            hidden  = $hidden
            isDir   = $f.PSIsContainer
        }
    }
}
$totalFiles = $files.Count
$hiddenFiles = ($files | Where-Object { $_.hidden }).Count

# --- 3. Ficheros ocultos detectados por nombre exacto (Test-Path) ---
Write-Host "    Verificando ficheros ocultos por nombre exacto..."
$testPathResults = @()
if (Test-Path $LabPath) {
    $allOnDisk = Get-ChildItem -Path $LabPath -Force 2>$null | Where-Object { $_.Name -like "$Prefix*" }
    foreach ($f in $allOnDisk) {
        $tp = Test-Path $f.FullName
        $testPathResults += @{ name = $f.Name; testpath = $tp }
    }
}

# --- 4. Registro (clave Run) ---
Write-Host "    Registro (HKCU\...\Run)..."
$runKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
$regEntries = @()
if (Test-Path $runKey) {
    $props = Get-ItemProperty -Path $runKey
    foreach ($name in $props.PSObject.Properties) {
        if ($name.Name -notmatch '^PS') {
            $regEntries += @{ name = $name.Name; value = $name.Value }
        }
    }
}

# --- 5. Sysmon (ultimos eventos relevantes) ---
Write-Host "    Sysmon (eventos 1, 7, 8, 10, 11, 13)..."
$sysmonLog = "Microsoft-Windows-Sysmon/Operational"

function Get-SysmonEvents($eventId, $max) {
    try {
        $events = Get-WinEvent -FilterHashtable @{
            LogName = $sysmonLog
            Id      = $eventId
        } -MaxEvents $max 2>$null
        return $events
    } catch { return @() }
}

$ev1  = Get-SysmonEvents 1 50
$ev7  = Get-SysmonEvents 7 50
$ev8  = Get-SysmonEvents 8 50
$ev10 = Get-SysmonEvents 10 50
$ev11 = Get-SysmonEvents 11 30
$ev13 = Get-SysmonEvents 13 20

function Format-SysmonLine($ev, $fields) {
    $time = $ev.TimeCreated.ToString("HH:mm:ss.fff")
    $data = @{}
    foreach ($prop in $ev.Properties) { }
    $msg = $ev.Message -replace "`r`n", " | " -replace "`n", " | "
    if ($msg.Length -gt 200) { $msg = $msg.Substring(0, 200) + "..." }
    return @{ time = $time; id = $ev.Id; message = $msg }
}

# Filtrar eventos que mencionan el lab o el prefijo
function Filter-LabEvents($events) {
    $result = @()
    foreach ($ev in $events) {
        $msg = $ev.Message
        if ($msg -match [regex]::Escape($LabPath) -or $msg -match [regex]::Escape($Prefix)) {
            $time = $ev.TimeCreated.ToString("HH:mm:ss.fff")
            $short = $msg -replace "`r`n", " " -replace "`n", " "
            if ($short.Length -gt 300) { $short = $short.Substring(0, 300) + "..." }
            $result += @{ time = $time; id = $ev.Id; msg = $short }
        }
    }
    return $result
}

$labEv1  = Filter-LabEvents $ev1
$labEv7  = Filter-LabEvents $ev7
$labEv8  = Filter-LabEvents $ev8
$labEv10 = Filter-LabEvents $ev10
$labEv11 = Filter-LabEvents $ev11
$labEv13 = Filter-LabEvents $ev13

Write-Host "[*] Generando HTML..." -ForegroundColor Cyan

# --- Construir JSON para el HTML ---
function To-JsonSafe($obj) {
    $json = $obj | ConvertTo-Json -Depth 5 -Compress
    return $json -replace '<', '\u003c' -replace '>', '\u003e' -replace '&', '\u0026'
}

$jsonData = @{
    timestamp    = $timestamp
    labPath      = $LabPath
    prefix       = $Prefix
    processes    = @{
        list       = $procLines
        total      = $totalProcs
        hiddenCount = $hiddenProcs
    }
    files        = @{
        list        = $files
        total       = $totalFiles
        hiddenCount = $hiddenFiles
        testPath    = $testPathResults
    }
    registry     = $regEntries
    sysmon       = @{
        ev1  = $labEv1
        ev7  = $labEv7
        ev8  = $labEv8
        ev10 = $labEv10
        ev11 = $labEv11
        ev13 = $labEv13
    }
} | ConvertTo-Json -Depth 5

# Escapar para meter dentro del HTML
$jsonData = $jsonData -replace '<', '\u003c' -replace '>', '\u003e'

$html = @"
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>PIV-Rootkit Visibility Dashboard</title>
<style>
  :root {
    --bg: #0c0c14; --bg-panel: #12121e; --bg-term: #0a0a12;
    --fg: #c8cad0; --fg-dim: #6b6e7a; --fg-bright: #e8eaf0;
    --accent: #4fc3f7; --accent-dim: #1a3a4f;
    --danger: #ef5350; --danger-dim: #3a1a1a;
    --success: #66bb6a; --success-dim: #1a3a1e;
    --warning: #ffa726; --warning-dim: #3a2a10;
    --border: #1e2030; --radius: 8px;
    color-scheme: dark;
  }
  * { box-sizing: border-box; margin: 0; }
  body {
    background: var(--bg); color: var(--fg);
    font-family: Segoe UI, system-ui, sans-serif;
    padding: 20px; line-height: 1.5;
  }
  .header { text-align: center; margin-bottom: 24px; }
  .header h1 { font-size: 1.4rem; color: var(--fg-bright); letter-spacing: -0.02em; }
  .header .ts { font-size: 0.75rem; color: var(--fg-dim); margin-top: 4px; }
  .tabs { display: flex; gap: 4px; justify-content: center; margin-bottom: 20px; flex-wrap: wrap; }
  .tab {
    padding: 7px 18px; border-radius: 6px; border: 1px solid var(--border);
    background: var(--bg-panel); color: var(--fg-dim); font-size: 0.8rem;
    font-weight: 500; cursor: pointer; font-family: inherit;
  }
  .tab:hover { color: var(--fg); border-color: var(--fg-dim); }
  .tab.active { background: var(--accent-dim); color: var(--accent); border-color: var(--accent); }
  .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 14px; max-width: 1200px; margin: 0 auto; }
  @media (max-width: 800px) { .grid { grid-template-columns: 1fr; } }
  .panel { background: var(--bg-panel); border: 1px solid var(--border); border-radius: var(--radius); overflow: hidden; }
  .panel-hdr {
    display: flex; align-items: center; gap: 8px;
    padding: 10px 14px; border-bottom: 1px solid var(--border);
    font-size: 0.75rem; font-weight: 600; text-transform: uppercase; letter-spacing: 0.05em;
  }
  .dot { width: 8px; height: 8px; border-radius: 50%; flex-shrink: 0; }
  .dot-r { background: var(--danger); }
  .dot-g { background: var(--success); }
  .dot-b { background: var(--accent); }
  .term {
    background: var(--bg-term); padding: 12px 14px;
    font-family: Consolas, 'Courier New', monospace; font-size: 0.7rem;
    line-height: 1.6; color: #c0c4cc; min-height: 180px;
    overflow-x: auto; white-space: pre; max-height: 400px; overflow-y: auto;
  }
  .p { color: #66bb6a; } .c { color: #e0e0e0; }
  .h { color: #ffa726; font-weight: 600; } .a { color: #4fc3f7; }
  .d { color: #555; } .r { color: #ef5350; }
  .cards { max-width: 1200px; margin: 16px auto 0; display: grid; grid-template-columns: repeat(auto-fit, minmax(180px,1fr)); gap: 10px; }
  .card { background: var(--bg-panel); border: 1px solid var(--border); border-radius: var(--radius); padding: 14px; }
  .card .lbl { font-size: 0.65rem; font-weight: 600; text-transform: uppercase; letter-spacing: 0.05em; color: var(--fg-dim); margin-bottom: 4px; }
  .card .val { font-size: 1.05rem; font-weight: 700; color: var(--fg-bright); }
  .card .det { font-size: 0.68rem; color: var(--fg-dim); margin-top: 3px; }
  .card.dn .val { color: var(--danger); }
  .card.ok .val { color: var(--success); }
  .card.ac .val { color: var(--accent); }
  .sec { display: none; } .sec.on { display: block; }
  table.mx { width: 100%; border-collapse: collapse; font-size: 0.75rem; }
  table.mx th { text-align: left; padding: 8px 10px; background: var(--bg-panel); border-bottom: 2px solid var(--border); color: var(--fg-dim); font-size: 0.68rem; text-transform: uppercase; letter-spacing: 0.04em; }
  table.mx td { padding: 8px 10px; border-bottom: 1px solid var(--border); }
  .badge { display: inline-block; padding: 2px 7px; border-radius: 4px; font-size: 0.68rem; font-weight: 600; }
  .badge.v { background: var(--success-dim); color: var(--success); }
  .badge.x { background: var(--danger-dim); color: var(--danger); }
  .badge.w { background: var(--warning-dim); color: var(--warning); }
  .full { max-width: 1200px; margin: 0 auto; }
  .full h2 { font-size: 0.95rem; color: var(--fg-bright); margin-bottom: 10px; }
</style>
</head>
<body>

<div class="header">
  <h1>PIV-Rootkit &mdash; Visibility Dashboard</h1>
  <div class="ts">Datos recogidos: $timestamp &mdash; Prefijo: $Prefix &mdash; Lab: $LabPath</div>
</div>

<div class="tabs">
  <button class="tab active" onclick="show('proc')">Procesos</button>
  <button class="tab" onclick="show('file')">Ficheros</button>
  <button class="tab" onclick="show('reg')">Registro</button>
  <button class="tab" onclick="show('sysmon')">Sysmon</button>
  <button class="tab" onclick="show('matrix')">Matriz</button>
</div>

<div id="sec-proc" class="sec on"></div>
<div id="sec-file" class="sec"></div>
<div id="sec-reg" class="sec"></div>
<div id="sec-sysmon" class="sec"></div>
<div id="sec-matrix" class="sec"></div>

<script>
var D = $jsonData;

function esc(s) { return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
function show(id) {
  document.querySelectorAll('.tab').forEach(function(t,i){ t.classList.toggle('active', i===['proc','file','reg','sysmon','matrix'].indexOf(id)); });
  document.querySelectorAll('.sec').forEach(function(s){ s.classList.remove('on'); });
  document.getElementById('sec-'+id).classList.add('on');
}

// --- PROCESOS ---
(function(){
  var el = document.getElementById('sec-proc');
  var visible = [], hidden = [];
  D.processes.list.forEach(function(p){ (p.hidden ? hidden : visible).push(p); });

  var hookedTerm = '<span class="p">PS C:\\lab&gt;</span> <span class="c">Get-Process | Format-Table Id, ProcessName</span>\n\n';
  hookedTerm += '<span class="d">   Id ProcessName</span>\n<span class="d">   -- -----------</span>\n';
  visible.forEach(function(p){ hookedTerm += ' ' + String(p.pid).padStart(5) + ' ' + esc(p.name) + '\n'; });
  if (hidden.length > 0) {
    hookedTerm += '\n<span class="d"># ' + hidden.length + ' proceso(s) con prefijo "' + esc(D.prefix) + '" FILTRADOS por el hook</span>';
  } else {
    hookedTerm += '\n<span class="d"># Ningun proceso con prefijo "' + esc(D.prefix) + '" en la lista (hook activo o no hay)</span>';
  }

  var sysmonTerm = '<span class="p">Sysmon eventos 1 + 10 (filtrados por ' + esc(D.labPath) + ' / ' + esc(D.prefix) + ')</span>\n\n';
  var allSys = (D.sysmon.ev1 || []).concat(D.sysmon.ev10 || []);
  if (allSys.length === 0) {
    sysmonTerm += '<span class="d"># Sin eventos de Sysmon para este lab/prefijo</span>';
  } else {
    allSys.forEach(function(e){
      sysmonTerm += '<span class="d">[' + esc(e.time) + ']</span> <span class="a">Ev' + e.id + '</span> ' + esc(e.msg).substring(0, 200) + '\n';
    });
  }

  el.innerHTML = '<div class="grid"><div class="panel"><div class="panel-hdr"><span class="dot dot-r"></span>Shell actual (Get-Process)</div><div class="term">' + hookedTerm + '</div></div>'
    + '<div class="panel"><div class="panel-hdr"><span class="dot dot-b"></span>Sysmon Ev1 + Ev10</div><div class="term">' + sysmonTerm + '</div></div></div>'
    + '<div class="cards"><div class="card dn"><div class="lbl">Procesos visibles</div><div class="val">' + visible.length + '</div><div class="det">de ' + D.processes.total + ' totales en esta shell</div></div>'
    + '<div class="card ok"><div class="lbl">Sysmon Ev1 (lab)</div><div class="val">' + (D.sysmon.ev1||[]).length + '</div><div class="det">ProcessCreate del lab</div></div>'
    + '<div class="card ac"><div class="lbl">Sysmon Ev10 (lab)</div><div class="val">' + (D.sysmon.ev10||[]).length + '</div><div class="det">ProcessAccess del lab</div></div></div>';
})();

// --- FICHEROS ---
(function(){
  var el = document.getElementById('sec-file');
  var hookedTerm = '<span class="p">PS ' + esc(D.labPath) + '&gt;</span> <span class="c">Get-ChildItem -Force</span>\n\n';
  if (D.files.list.length === 0) {
    hookedTerm += '<span class="d"># Carpeta vacia o no existe</span>';
  } else {
    D.files.list.forEach(function(f){
      var cls = f.hidden ? 'h' : '';
      var tag = f.hidden ? ' <span class="r">[OCULTO si hook activo]</span>' : '';
      hookedTerm += '<span class="' + cls + '">' + esc(f.date) + '  ' + esc(f.size).padStart(12) + '  ' + esc(f.name) + '</span>' + tag + '\n';
    });
  }

  var tpTerm = '<span class="p">Test-Path (acceso directo por nombre)</span>\n\n';
  if ((D.files.testPath||[]).length === 0) {
    tpTerm += '<span class="d"># No hay ficheros con prefijo "' + esc(D.prefix) + '"</span>';
  } else {
    D.files.testPath.forEach(function(t){
      var color = t.testpath ? 'a' : 'r';
      tpTerm += '<span class="' + color + '">Test-Path "' + esc(t.name) + '" = ' + t.testpath + '</span>\n';
    });
    tpTerm += '\n<span class="d"># Test-Path no pasa por NtQueryDirectoryFile: el hook NO lo afecta</span>';
  }

  el.innerHTML = '<div class="grid"><div class="panel"><div class="panel-hdr"><span class="dot dot-g"></span>Get-ChildItem -Force (todos)</div><div class="term">' + hookedTerm + '</div></div>'
    + '<div class="panel"><div class="panel-hdr"><span class="dot dot-b"></span>Test-Path (nombre exacto)</div><div class="term">' + tpTerm + '</div></div></div>'
    + '<div class="cards"><div class="card dn"><div class="lbl">Ficheros con prefijo</div><div class="val">' + D.files.hiddenCount + '</div><div class="det">de ' + D.files.total + ' en ' + esc(D.labPath) + '</div></div>'
    + '<div class="card ok"><div class="lbl">Sysmon Ev11</div><div class="val">' + (D.sysmon.ev11||[]).length + '</div><div class="det">FileCreate del lab</div></div>'
    + '<div class="card ac"><div class="lbl">Hook NtQueryDirectoryFile</div><div class="val">Filtra dir/Explorer</div><div class="det">No afecta a Test-Path ni Sysmon</div></div></div>';
})();

// --- REGISTRO ---
(function(){
  var el = document.getElementById('sec-reg');
  var regTerm = '<span class="p">PS&gt;</span> <span class="c">Get-ItemProperty HKCU:\\...\\Run</span>\n\n';
  if (D.registry.length === 0) {
    regTerm += '<span class="d"># No hay entradas en la clave Run</span>';
  } else {
    D.registry.forEach(function(r){
      var cls = String(r.value).indexOf(D.prefix) >= 0 ? 'h' : '';
      regTerm += '<span class="' + cls + '">' + esc(r.name).padEnd(20) + ' ' + esc(r.value) + '</span>\n';
    });
  }

  var sysmonTerm = '<span class="p">Sysmon Ev13 (RegistryEvent)</span>\n\n';
  if ((D.sysmon.ev13||[]).length === 0) {
    sysmonTerm += '<span class="d"># Sin eventos Ev13 para este lab/prefijo</span>';
  } else {
    D.sysmon.ev13.forEach(function(e){
      sysmonTerm += '<span class="d">[' + esc(e.time) + ']</span> <span class="h">Ev13</span> ' + esc(e.msg).substring(0, 250) + '\n\n';
    });
  }

  el.innerHTML = '<div class="grid"><div class="panel"><div class="panel-hdr"><span class="dot dot-r"></span>Clave Run (HKCU)</div><div class="term">' + regTerm + '</div></div>'
    + '<div class="panel"><div class="panel-hdr"><span class="dot dot-b"></span>Sysmon Ev13</div><div class="term">' + sysmonTerm + '</div></div></div>'
    + '<div class="cards"><div class="card dn"><div class="lbl">Entradas Run</div><div class="val">' + D.registry.length + '</div><div class="det">en HKCU\\...\\CurrentVersion\\Run</div></div>'
    + '<div class="card ok"><div class="lbl">Sysmon Ev13</div><div class="val">' + (D.sysmon.ev13||[]).length + '</div><div class="det">RegistryEvent SetValue</div></div>'
    + '<div class="card ac"><div class="lbl">MITRE</div><div class="val">T1547.001</div><div class="det">Registry Run Keys</div></div></div>';
})();

// --- SYSMON COMPLETO ---
(function(){
  var el = document.getElementById('sec-sysmon');
  var types = [
    {key:'ev1',  label:'Ev1 ProcessCreate'},
    {key:'ev7',  label:'Ev7 ImageLoad'},
    {key:'ev8',  label:'Ev8 CreateRemoteThread'},
    {key:'ev10', label:'Ev10 ProcessAccess'},
    {key:'ev11', label:'Ev11 FileCreate'},
    {key:'ev13', label:'Ev13 RegistryEvent'}
  ];
  var html = '<div class="full"><h2>Eventos de Sysmon filtrados por ' + esc(D.labPath) + ' / ' + esc(D.prefix) + '</h2>';
  types.forEach(function(t){
    var evts = D.sysmon[t.key] || [];
    html += '<div class="panel" style="margin-bottom:12px"><div class="panel-hdr"><span class="dot dot-b"></span>' + t.label + ' <span style="margin-left:auto;color:var(--accent)">' + evts.length + '</span></div>';
    html += '<div class="term">';
    if (evts.length === 0) {
      html += '<span class="d"># Sin eventos</span>';
    } else {
      evts.forEach(function(e){
        html += '<span class="d">[' + esc(e.time) + ']</span> ' + esc(e.msg).substring(0, 300) + '\n';
      });
    }
    html += '</div></div>';
  });
  html += '</div>';
  html += '<div class="cards"><div class="card ac"><div class="lbl">Total eventos lab</div><div class="val">' + ((D.sysmon.ev1||[]).length + (D.sysmon.ev7||[]).length + (D.sysmon.ev8||[]).length + (D.sysmon.ev10||[]).length + (D.sysmon.ev11||[]).length + (D.sysmon.ev13||[]).length) + '</div><div class="det">Todos los tipos combinados</div></div></div>';
  el.innerHTML = html;
})();

// --- MATRIZ ---
(function(){
  var el = document.getElementById('sec-matrix');
  var ev1  = (D.sysmon.ev1||[]).length;
  var ev7  = (D.sysmon.ev7||[]).length;
  var ev8  = (D.sysmon.ev8||[]).length;
  var ev10 = (D.sysmon.ev10||[]).length;
  var ev11 = (D.sysmon.ev11||[]).length;
  var ev13 = (D.sysmon.ev13||[]).length;

  function badge(count, stdVisible) {
    if (count > 0) return '<span class="badge v">' + count + ' eventos</span>';
    if (!stdVisible) return '<span class="badge x">0 (config?)</span>';
    return '<span class="badge w">0</span>';
  }

  el.innerHTML = '<div class="full"><h2>Matriz de visibilidad (datos reales de esta captura)</h2>'
    + '<div style="overflow-x:auto"><table class="mx"><thead><tr><th>Fuente</th><th>Eventos capturados</th><th>Que revela</th></tr></thead><tbody>'
    + '<tr><td style="color:var(--fg-bright);font-weight:600">Ev1 ProcessCreate</td><td>' + badge(ev1,true) + '</td><td>Creacion de procesos del lab con CLI y hashes</td></tr>'
    + '<tr><td style="color:var(--fg-bright);font-weight:600">Ev7 ImageLoad</td><td>' + badge(ev7,false) + '</td><td>DLL inyectada (Signed:false) en cada proceso target</td></tr>'
    + '<tr><td style="color:var(--fg-bright);font-weight:600">Ev8 CreateRemoteThread</td><td>' + badge(ev8,false) + '</td><td>Remote threads con StartFunction=LoadLibraryW</td></tr>'
    + '<tr><td style="color:var(--fg-bright);font-weight:600">Ev10 ProcessAccess</td><td>' + badge(ev10,false) + '</td><td>OpenProcess con GrantedAccess 0x142A</td></tr>'
    + '<tr><td style="color:var(--fg-bright);font-weight:600">Ev11 FileCreate</td><td>' + badge(ev11,true) + '</td><td>Creacion de ficheros con prefijo en el lab</td></tr>'
    + '<tr><td style="color:var(--fg-bright);font-weight:600">Ev13 RegistryEvent</td><td>' + badge(ev13,true) + '</td><td>Claves de persistencia en Run</td></tr>'
    + '<tr><td style="color:var(--fg-bright);font-weight:600">Get-Process (esta shell)</td><td><span class="badge ' + (D.processes.hiddenCount > 0 ? 'x' : 'w') + '">' + D.processes.hiddenCount + ' ocultos</span></td><td>Procesos con prefijo filtrados del resultado</td></tr>'
    + '<tr><td style="color:var(--fg-bright);font-weight:600">dir / GCI (esta shell)</td><td><span class="badge ' + (D.files.hiddenCount > 0 ? 'x' : 'w') + '">' + D.files.hiddenCount + ' ocultos</span></td><td>Ficheros con prefijo filtrados de la enumeracion</td></tr>'
    + '</tbody></table></div></div>'
    + '<div class="cards" style="margin-top:12px">'
    + '<div class="card dn"><div class="lbl">User-mode oculta</div><div class="val">' + (D.processes.hiddenCount + D.files.hiddenCount) + '</div><div class="det">procesos + ficheros</div></div>'
    + '<div class="card ok"><div class="lbl">Kernel-mode ve</div><div class="val">' + (ev1+ev7+ev8+ev10+ev11+ev13) + '</div><div class="det">eventos totales de Sysmon</div></div>'
    + '<div class="card ac"><div class="lbl">Conclusion</div><div class="val">Configurable</div><div class="det">Los puntos ciegos son de config, no del sensor</div></div></div>';
})();
</script>
</body>
</html>
"@

$outPath = Join-Path (Get-Location) $OutFile
$html | Out-File -FilePath $outPath -Encoding UTF8
Write-Host "[+] Dashboard generado: $outPath" -ForegroundColor Green

if (-not $NoOpen) {
    Write-Host "[*] Abriendo en el navegador..." -ForegroundColor Cyan
    Start-Process $outPath
}

Write-Host @"

Uso:
  .\dashboard.ps1                          # Genera y abre dashboard.html
  .\dashboard.ps1 -Prefix '`$rk'           # Prefijo custom
  .\dashboard.ps1 -LabPath C:\test         # Otra carpeta
  .\dashboard.ps1 -NoOpen                  # Solo genera, no abre
  .\dashboard.ps1 -OutFile resultado.html  # Nombre custom

Para comparar hooked vs no-hooked:
  1. Ejecuta desde una shell SIN hook  -> dashboard_clean.html
  2. Inyecta hook.dll en otra shell
  3. Ejecuta desde la shell hooked     -> dashboard_hooked.html
  4. Abre ambos HTMLs lado a lado
"@
