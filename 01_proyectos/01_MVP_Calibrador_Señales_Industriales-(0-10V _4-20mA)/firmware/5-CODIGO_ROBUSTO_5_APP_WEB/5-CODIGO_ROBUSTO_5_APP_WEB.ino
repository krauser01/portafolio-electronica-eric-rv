// ===== CODIGO_CALIBRACION_REMOTA =====
/*
 * PROYECTO: MVP Calibrador de Señales Industriales (0-10V / 4-20mA)
 * VERSIÓN: 2.3 (Calibración Remota + NVS + WiFi Mejorado)
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Preferences.h>

// ==========================================
// 📶 CONFIGURACIÓN WIFI
// ==========================================
const char* AP_SSID = "Calibrador-MVP";
const char* AP_PASSWORD = "1234567890";  // 🔑 NUEVA CLAVE
const int PUERTO_WEB = 80;
const int PUERTO_WS = 81;

WebServer server(PUERTO_WEB);
WebSocketsServer webSocket = WebSocketsServer(PUERTO_WS);
Preferences preferences;

// ==========================================
// 🎨 PALETA DE COLORES
// ==========================================
#define ST7735_DARKGREY 0x4208

// ==========================================
// 📌 PINOUT CONGELADO
// ==========================================
#define TFT_CS     5
#define TFT_DC    22
#define TFT_RST    4
#define TFT_SCLK  18
#define TFT_MOSI  23
#define BTN_UP    32
#define BTN_DOWN  33
#define BTN_OK    21
#define BTN_BACK  13
#define LED_VOLT  27
#define LED_CURR  14
#define DAC_VOLT  25
#define DAC_CURR  26
#define ADC_VOLT  34
#define ADC_CURR  35

// ==========================================
// ⚙️ PARÁMETROS DE CALIBRACIÓN (EDITABLES)
// ==========================================
const int DAC_VOLT_LIMIT = 232;
const int NUM_PUNTOS_CAL_V = 21;

const float tablaSetpointV[NUM_PUNTOS_CAL_V] = {
  0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5,
  5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0, 8.5, 9.0, 9.5, 10.0
};

// Valores por defecto (se sobrescriben con NVS si existe calibración)
float tablaRealV[NUM_PUNTOS_CAL_V] = {
  0.250, 0.250, 0.780, 1.280, 1.788, 2.300, 2.830, 3.345, 3.855, 4.335,
  4.815, 5.335, 5.855, 6.360, 6.890, 7.360, 7.870, 8.435, 9.010, 9.460, 9.930
};

float CAL_I_GAIN   = 0.96875;
float CAL_I_OFFSET = 0.625;

// 🆕 Calibración de entradas (medición)
float CAL_MEAS_V_GAIN   = 1.0;
float CAL_MEAS_V_OFFSET = 0.0;
float CAL_MEAS_I_GAIN   = 1.0;
float CAL_MEAS_I_OFFSET = 0.0;

// ==========================================
// ⚙️ VARIABLES DE ESTADO
// ==========================================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

enum Estados {
  MENU_PRINCIPAL, INYECTAR_V_MAN, INYECTAR_V_AUT,
  INYECTAR_I_MAN, INYECTAR_I_AUT, MEDIR_V, MEDIR_I
};
Estados estadoActual = MENU_PRINCIPAL;

int opcionSeleccionada = 0;
const int TOTAL_OPCIONES = 6;
const char* menuItems[] = {
  "1. Inyectar V (Manual)", "2. Inyectar V (Auto)",
  "3. Inyectar I (Manual)", "4. Inyectar I (Auto)",
  "5. Medir 0-10V", "6. Medir 4-20mA"
};

float vManualTarget = 0.0;
float iManualTarget = 4.0;

const float rampaVoltaje[] = {0.0, 2.5, 5.0, 7.5, 10.0, 7.5, 5.0, 2.5};
const float rampaCorriente[] = {4.0, 8.0, 12.0, 16.0, 20.0, 16.0, 12.0, 8.0};
const int TOTAL_PASOS_RAMPA = 8;
int indiceRampaAuto = 0;

unsigned long ultimoRefrescoMedicion = 0;
const unsigned long INTERVALO_MEDICION = 250;
unsigned long ultimoCambioRampaAuto = 0;
const unsigned long INTERVALO_RAMPA = 10000;
unsigned long ultimoTiempoBoton = 0;
const unsigned long TIEMPO_DEBOUNCE = 200;

float vManualAnterior = -1.0;
float iManualAnterior = -1.0;
int indiceRampaAnterior = -1;

float ultimoValorInV = 0.0;
float ultimoValorInI = 0.0;
unsigned long ultimoEnvioWS = 0;
const unsigned long INTERVALO_WS = 200;

int clientesWiFiAnterior = -1;
unsigned long ultimoRefrescoWiFi = 0;
const unsigned long INTERVALO_WIFI = 1000;

// ==========================================
// 💾 FUNCIONES DE CALIBRACIÓN NVS
// ==========================================
void cargarCalibracion() {
  preferences.begin("calibrador", false);
  int version = preferences.getInt("version", 0);

  if (version == 1) {
    preferences.getBytes("tablaRV", tablaRealV, sizeof(tablaRealV));
    CAL_I_GAIN   = preferences.getFloat("cIGain", 0.96875);
    CAL_I_OFFSET = preferences.getFloat("cIOff", 0.625);
    CAL_MEAS_V_GAIN   = preferences.getFloat("mVGain", 1.0);
    CAL_MEAS_V_OFFSET = preferences.getFloat("mVOff", 0.0);
    CAL_MEAS_I_GAIN   = preferences.getFloat("mIGain", 1.0);
    CAL_MEAS_I_OFFSET = preferences.getFloat("mIOff", 0.0);
    Serial.println("✅ Calibración cargada desde NVS");
  } else {
    Serial.println("ℹ️ Usando calibración por defecto");
  }
  preferences.end();
}

void guardarCalibracion() {
  preferences.begin("calibrador", false);
  preferences.putInt("version", 1);
  preferences.putBytes("tablaRV", tablaRealV, sizeof(tablaRealV));
  preferences.putFloat("cIGain", CAL_I_GAIN);
  preferences.putFloat("cIOff", CAL_I_OFFSET);
  preferences.putFloat("mVGain", CAL_MEAS_V_GAIN);
  preferences.putFloat("mVOff", CAL_MEAS_V_OFFSET);
  preferences.putFloat("mIGain", CAL_MEAS_I_GAIN);
  preferences.putFloat("mIOff", CAL_MEAS_I_OFFSET);
  preferences.end();
  Serial.println("💾 Calibración guardada en NVS");
}

void restaurarFabrica() {
  // Valores originales de fábrica
  float fabricaV[] = {
    0.250, 0.250, 0.780, 1.280, 1.788, 2.300, 2.830, 3.345, 3.855, 4.335,
    4.815, 5.335, 5.855, 6.360, 6.890, 7.360, 7.870, 8.435, 9.010, 9.460, 9.930
  };
  memcpy(tablaRealV, fabricaV, sizeof(tablaRealV));
  CAL_I_GAIN = 0.96875; CAL_I_OFFSET = 0.625;
  CAL_MEAS_V_GAIN = 1.0; CAL_MEAS_V_OFFSET = 0.0;
  CAL_MEAS_I_GAIN = 1.0; CAL_MEAS_I_OFFSET = 0.0;

  preferences.begin("calibrador", false);
  preferences.clear();
  preferences.end();
  Serial.println("🔄 Calibración restaurada a fábrica");
}

// ==========================================
// 🔧 INTERPOLACIÓN
// ==========================================
float interpolarCalibracionV(float voltajeDeseado) {
  if (voltajeDeseado <= tablaSetpointV[0]) return tablaRealV[0];
  if (voltajeDeseado >= tablaSetpointV[NUM_PUNTOS_CAL_V - 1])
    return tablaRealV[NUM_PUNTOS_CAL_V - 1];

  for (int i = 0; i < NUM_PUNTOS_CAL_V - 1; i++) {
    if (voltajeDeseado >= tablaSetpointV[i] && voltajeDeseado <= tablaSetpointV[i + 1]) {
      float factor = (voltajeDeseado - tablaSetpointV[i]) /
                     (tablaSetpointV[i + 1] - tablaSetpointV[i]);
      return tablaRealV[i] + factor * (tablaRealV[i + 1] - tablaRealV[i]);
    }
  }
  return voltajeDeseado;
}

// ==========================================
// 🌐 PÁGINA HTML CON CALIBRACIÓN REMOTA
// ==========================================
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Calibrador MVP</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;font-family:Arial,sans-serif}
html,body{background:linear-gradient(135deg,#1a2a4a 0%,#2a4270 100%);color:#fff;height:100vh;overflow:hidden;padding:5px}
.container{max-width:600px;margin:0 auto;height:100%;display:flex;flex-direction:column;gap:4px}
.header{background:rgba(0,0,0,0.5);padding:5px 8px;border-radius:5px;display:flex;justify-content:space-between;align-items:center;border:1px solid rgba(79,195,247,0.3);flex-shrink:0}
.header h1{font-size:0.95em;color:#4fc3f7}
.status{display:flex;align-items:center;gap:4px;font-size:0.7em}
.status-dot{width:8px;height:8px;border-radius:50%;background:#f44336}
.status-dot.connected{background:#4caf50;animation:pulse 1.5s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}
.panel{background:rgba(0,0,0,0.35);padding:6px;border-radius:5px;border:1px solid rgba(255,255,255,0.08);flex-shrink:0}
.modo-actual{text-align:center;padding:4px;background:rgba(79,195,247,0.15);border-radius:4px;font-size:0.8em;font-weight:bold;color:#4fc3f7;margin-bottom:4px}
.valores{display:grid;grid-template-columns:1fr 1fr;gap:5px;margin-bottom:4px}
.valor-box{background:rgba(0,0,0,0.5);padding:5px;border-radius:4px;text-align:center;border:2px solid transparent}
.valor-box.out{border-color:#4caf50}.valor-box.in{border-color:#ffc107}
.valor-label{font-size:0.65em;color:#aaa;margin-bottom:2px}
.valor-numero{font-size:1.5em;font-weight:bold;font-family:'Courier New',monospace}
.valor-box.out .valor-numero{color:#4caf50}.valor-box.in .valor-numero{color:#ffc107}
.valor-unidad{font-size:0.45em;color:#ccc}
.porcentaje{text-align:center;font-size:0.85em;color:#4fc3f7;margin-top:2px}
.modos-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:3px}
.btn-modo{background:rgba(255,255,255,0.08);border:1px solid rgba(255,255,255,0.2);color:#fff;padding:6px 2px;border-radius:3px;cursor:pointer;font-size:0.65em;text-align:center}
.btn-modo:active{transform:scale(0.95)}
.btn-modo.activo{background:#4caf50;border-color:#4caf50;font-weight:bold}
.btn-menu{background:#c62828;border:1px solid #e53935;color:#fff;padding:6px 2px;border-radius:3px;cursor:pointer;font-size:0.65em;font-weight:bold;grid-column:span 2}
.btn-cal{background:#f57c00;border:1px solid #fb8c00;color:#fff;padding:6px 2px;border-radius:3px;cursor:pointer;font-size:0.65em;font-weight:bold;grid-column:span 2}
.btn-menu:active,.btn-cal:active{transform:scale(0.95)}
.control-manual{display:none}
.control-manual.activo{display:block}
.slider-container{margin:4px 0}
.slider-container label{display:block;margin-bottom:2px;font-size:0.7em;color:#ccc}
input[type="range"]{width:100%;height:5px;border-radius:3px;background:rgba(255,255,255,0.2);outline:none;-webkit-appearance:none}
input[type="range"]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;border-radius:50%;background:#4fc3f7;cursor:pointer;border:2px solid #fff}
.input-exacto{display:flex;gap:4px;align-items:center;margin-top:4px;padding:4px;background:rgba(0,0,0,0.3);border-radius:3px;border:1px solid rgba(79,195,247,0.3)}
.input-exacto label{font-size:0.7em;color:#4fc3f7;font-weight:bold}
.input-exacto input[type="number"]{flex:1;padding:4px;font-size:0.9em;font-weight:bold;border:1px solid #4fc3f7;border-radius:3px;background:rgba(0,0,0,0.5);color:#fff;font-family:'Courier New',monospace;text-align:center;outline:none;width:60px}
.input-exacto .unidad-input{font-size:0.75em;color:#ffc107;font-weight:bold}
.btn-aplicar{background:#4caf50;border:1px solid #66bb6a;color:#fff;padding:5px 8px;border-radius:3px;cursor:pointer;font-size:0.75em;font-weight:bold}
.botones-rapidos{display:grid;grid-template-columns:repeat(5,1fr);gap:2px;margin-top:4px}
.btn-rapido{background:rgba(79,195,247,0.15);border:1px solid #4fc3f7;color:#fff;padding:4px 1px;border-radius:3px;cursor:pointer;font-size:0.65em}
.info-box{background:rgba(0,0,0,0.4);padding:3px;border-radius:3px;font-size:0.6em;color:#888;text-align:center;flex-shrink:0}
.info-box strong{color:#4fc3f7}

/* 🆕 PANEL DE CALIBRACIÓN */
.panel-cal{display:none;background:rgba(0,0,0,0.6);padding:6px;border-radius:5px;border:1px solid #f57c00;max-height:55vh;overflow-y:auto}
.panel-cal.activo{display:block}
.panel-cal h3{color:#f57c00;font-size:0.85em;margin-bottom:5px;text-align:center}
details{background:rgba(0,0,0,0.3);padding:4px;border-radius:3px;margin-bottom:4px;border:1px solid rgba(245,124,0,0.3)}
summary{cursor:pointer;font-size:0.75em;color:#ffc107;font-weight:bold;padding:2px}
summary:hover{color:#fff}
.cal-content{padding:5px 0}
.cal-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:3px}
.cal-item{font-size:0.65em;display:flex;flex-direction:column;gap:1px}
.cal-item label{color:#aaa}
.cal-item input{padding:3px;font-size:0.8em;border:1px solid #f57c00;border-radius:2px;background:rgba(0,0,0,0.5);color:#fff;font-family:'Courier New',monospace;text-align:center;width:100%}
.cal-2col{display:grid;grid-template-columns:1fr 1fr;gap:5px}
.cal-2col .cal-item input{width:100%}
.cal-btns{display:grid;grid-template-columns:1fr 1fr 1fr;gap:3px;margin-top:5px}
.btn-cal-action{padding:6px 3px;border-radius:3px;cursor:pointer;font-size:0.7em;font-weight:bold;border:1px solid;color:#fff}
.btn-cargar{background:#1976d2;border-color:#2196f3}
.btn-guardar{background:#388e3c;border-color:#4caf50}
.btn-restaurar{background:#c62828;border-color:#e53935}
.btn-cal-action:active{transform:scale(0.95)}
.toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);background:#4caf50;color:#fff;padding:8px 16px;border-radius:4px;font-size:0.8em;font-weight:bold;opacity:0;transition:opacity 0.3s;pointer-events:none;z-index:1000}
.toast.show{opacity:1}
.toast.error{background:#c62828}
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <h1>⚡ Calibrador MVP</h1>
    <div class="status">
      <div class="status-dot" id="statusDot"></div>
      <span id="statusText">--</span>
    </div>
  </div>

  <div class="panel">
    <div class="modo-actual" id="modoActual">MODO: MENÚ PRINCIPAL</div>
    <div class="valores">
      <div class="valor-box out">
        <div class="valor-label">📤 SALIDA</div>
        <div class="valor-numero"><span id="valorOut">0.00</span><span class="valor-unidad" id="unidadOut">V</span></div>
      </div>
      <div class="valor-box in">
        <div class="valor-label">📥 ENTRADA</div>
        <div class="valor-numero"><span id="valorIn">0.00</span><span class="valor-unidad" id="unidadIn">V</span></div>
      </div>
    </div>
    <div class="porcentaje" id="porcentaje">0.0 %</div>
  </div>

  <div class="panel">
    <div class="modos-grid">
      <button class="btn-modo" data-modo="1" onclick="cambiarModo(1)">💉 V Manual</button>
      <button class="btn-modo" data-modo="2" onclick="cambiarModo(2)">🔄 V Auto</button>
      <button class="btn-modo" data-modo="3" onclick="cambiarModo(3)">💉 I Manual</button>
      <button class="btn-modo" data-modo="4" onclick="cambiarModo(4)">🔄 I Auto</button>
      <button class="btn-modo" data-modo="5" onclick="cambiarModo(5)">📊 Medir V</button>
      <button class="btn-modo" data-modo="6" onclick="cambiarModo(6)">📊 Medir I</button>
      <button class="btn-menu" onclick="irMenuPrincipal()">⬅ MENÚ</button>
      <button class="btn-cal" onclick="toggleCalibracion()">⚙️ CALIBRAR</button>
    </div>
  </div>

  <div class="panel control-manual" id="controlVManual">
    <div class="slider-container">
      <label>Rápido: <strong id="sliderVValue">0.0</strong> V</label>
      <input type="range" id="sliderV" min="0" max="100" value="0" step="5" oninput="actualizarSliderV(this.value)">
    </div>
    <div class="input-exacto">
      <label>EXACTO:</label>
      <input type="number" id="inputVExacto" min="0" max="10" step="0.01" value="0.00">
      <span class="unidad-input">V</span>
      <button class="btn-aplicar" onclick="aplicarVExacto()">OK</button>
    </div>
    <div class="botones-rapidos">
      <button class="btn-rapido" onclick="setValorV(0)">0V</button>
      <button class="btn-rapido" onclick="setValorV(2.5)">2.5V</button>
      <button class="btn-rapido" onclick="setValorV(5)">5V</button>
      <button class="btn-rapido" onclick="setValorV(7.5)">7.5V</button>
      <button class="btn-rapido" onclick="setValorV(10)">10V</button>
    </div>
  </div>

  <div class="panel control-manual" id="controlIManual">
    <div class="slider-container">
      <label>Rápido: <strong id="sliderIValue">4.0</strong> mA</label>
      <input type="range" id="sliderI" min="40" max="200" value="40" step="10" oninput="actualizarSliderI(this.value)">
    </div>
    <div class="input-exacto">
      <label>EXACTO:</label>
      <input type="number" id="inputIExacto" min="4" max="20" step="0.01" value="4.00">
      <span class="unidad-input">mA</span>
      <button class="btn-aplicar" onclick="aplicarIExacto()">OK</button>
    </div>
    <div class="botones-rapidos">
      <button class="btn-rapido" onclick="setValorI(4)">4mA</button>
      <button class="btn-rapido" onclick="setValorI(8)">8mA</button>
      <button class="btn-rapido" onclick="setValorI(12)">12mA</button>
      <button class="btn-rapido" onclick="setValorI(16)">16mA</button>
      <button class="btn-rapido" onclick="setValorI(20)">20mA</button>
    </div>
  </div>

  <!-- 🆕 PANEL DE CALIBRACIÓN REMOTA -->
  <div class="panel-cal" id="panelCalibracion">
    <h3>⚙️ CALIBRACIÓN REMOTA</h3>

    <details open>
      <summary>📊 Salida Voltaje (21 puntos)</summary>
      <div class="cal-content">
        <div class="cal-grid" id="tablaCalV"></div>
      </div>
    </details>

    <details>
      <summary>⚡ Salida Corriente (Zero/Span)</summary>
      <div class="cal-content">
        <div class="cal-2col">
          <div class="cal-item"><label>Zero (4mA real):</label><input type="number" id="calIZero" step="0.001" value="0.625"></div>
          <div class="cal-item"><label>Span (Ganancia):</label><input type="number" id="calISpan" step="0.0001" value="0.96875"></div>
        </div>
      </div>
    </details>

    <details>
      <summary>📏 Medición Voltaje (Zero/Span)</summary>
      <div class="cal-content">
        <div class="cal-2col">
          <div class="cal-item"><label>Offset (V):</label><input type="number" id="calMVOff" step="0.01" value="0.0"></div>
          <div class="cal-item"><label>Ganancia:</label><input type="number" id="calMVGain" step="0.001" value="1.0"></div>
        </div>
      </div>
    </details>

    <details>
      <summary>🔌 Medición Corriente (Zero/Span)</summary>
      <div class="cal-content">
        <div class="cal-2col">
          <div class="cal-item"><label>Offset (mA):</label><input type="number" id="calMIOff" step="0.01" value="0.0"></div>
          <div class="cal-item"><label>Ganancia:</label><input type="number" id="calMIGain" step="0.001" value="1.0"></div>
        </div>
      </div>
    </details>

    <div class="cal-btns">
      <button class="btn-cal-action btn-cargar" onclick="cargarCalData()">📥 Cargar</button>
      <button class="btn-cal-action btn-guardar" onclick="guardarCalData()">💾 Guardar</button>
      <button class="btn-cal-action btn-restaurar" onclick="restaurarFab()">🔄 Fábrica</button>
    </div>
  </div>

  <div class="info-box">
    <strong>WiFi:</strong> Calibrador-MVP | <strong>Pass:</strong> 1234567890 | <strong>IP:</strong> 192.168.4.1
  </div>
</div>

<div class="toast" id="toast"></div>

<script>
let ws;let reconnectInterval;

function showToast(msg,isError){
  const t=document.getElementById('toast');
  t.textContent=msg;
  t.className='toast show'+(isError?' error':'');
  setTimeout(()=>t.className='toast',2500);
}

function generarTablaCalV(){
  const c=document.getElementById('tablaCalV');
  let h='';
  for(let i=0;i<21;i++){
    const sp=(i*0.5).toFixed(1);
    h+=`<div class="cal-item"><label>${sp}V →</label><input type="number" id="cv${i}" step="0.001"></div>`;
  }
  c.innerHTML=h;
}

function conectarWebSocket(){
  ws=new WebSocket('ws://'+window.location.hostname+':81/');
  ws.onopen=function(){
    document.getElementById('statusDot').classList.add('connected');
    document.getElementById('statusText').textContent='OK';
    if(reconnectInterval){clearInterval(reconnectInterval);reconnectInterval=null;}
  };
  ws.onmessage=function(evt){
    try{
      const d=JSON.parse(evt.data);
      if(d.cmd==='calData'){cargarEnUI(d);showToast('✅ Datos cargados');return;}
      if(d.cmd==='calOk'){showToast('💾 Guardado en NVS');return;}
      if(d.cmd==='calReset'){showToast('🔄 Restaurado');return;}
      actualizarInterfaz(d);
    }catch(e){console.error(e);}
  };
  ws.onclose=function(){
    document.getElementById('statusDot').classList.remove('connected');
    document.getElementById('statusText').textContent='--';
    if(!reconnectInterval)reconnectInterval=setInterval(conectarWebSocket,2000);
  };
  ws.onerror=function(e){console.error(e);};
}

function cargarEnUI(d){
  for(let i=0;i<21;i++){
    const el=document.getElementById('cv'+i);
    if(el&&d.tablaV[i]!==undefined)el.value=d.tablaV[i].toFixed(3);
  }
  document.getElementById('calIZero').value=d.iOffset.toFixed(3);
  document.getElementById('calISpan').value=d.iGain.toFixed(4);
  document.getElementById('calMVOff').value=d.mvOffset.toFixed(2);
  document.getElementById('calMVGain').value=d.mvGain.toFixed(3);
  document.getElementById('calMIOff').value=d.miOffset.toFixed(2);
  document.getElementById('calMIGain').value=d.miGain.toFixed(3);
}

function actualizarInterfaz(d){
  document.getElementById('modoActual').textContent='MODO: '+d.modo;
  document.getElementById('valorOut').textContent=d.out.toFixed(2);
  document.getElementById('unidadOut').textContent=d.unidadOut;
  document.getElementById('valorIn').textContent=d.in.toFixed(2);
  document.getElementById('unidadIn').textContent=d.unidadIn;
  document.getElementById('porcentaje').textContent=d.porcentaje.toFixed(1)+' %';
  document.querySelectorAll('.btn-modo').forEach(b=>{
    b.classList.remove('activo');
    if(parseInt(b.dataset.modo)===d.modoNum)b.classList.add('activo');
  });
  document.getElementById('controlVManual').classList.toggle('activo',d.modoNum===1);
  document.getElementById('controlIManual').classList.toggle('activo',d.modoNum===3);
  if(d.modoNum===1){
    const s=document.getElementById('sliderV'),i=document.getElementById('inputVExacto');
    if(document.activeElement!==s&&document.activeElement!==i){
      s.value=d.out*10;
      document.getElementById('sliderVValue').textContent=d.out.toFixed(1);
      i.value=d.out.toFixed(2);
    }
  }else if(d.modoNum===3){
    const s=document.getElementById('sliderI'),i=document.getElementById('inputIExacto');
    if(document.activeElement!==s&&document.activeElement!==i){
      s.value=d.out*10;
      document.getElementById('sliderIValue').textContent=d.out.toFixed(1);
      i.value=d.out.toFixed(2);
    }
  }
}

function enviar(cmd,val){
  if(ws&&ws.readyState===WebSocket.OPEN)ws.send(JSON.stringify({cmd:cmd,value:val}));
}
function cambiarModo(m){enviar('modo',m);}
function irMenuPrincipal(){enviar('menu',0);}

function actualizarSliderV(v){
  const val=v/10;
  document.getElementById('sliderVValue').textContent=val.toFixed(1);
  document.getElementById('inputVExacto').value=val.toFixed(2);
  enviar('setV',val);
}
function actualizarSliderI(v){
  const val=v/10;
  document.getElementById('sliderIValue').textContent=val.toFixed(1);
  document.getElementById('inputIExacto').value=val.toFixed(2);
  enviar('setI',val);
}
function aplicarVExacto(){
  const i=document.getElementById('inputVExacto');
  let v=parseFloat(i.value);
  if(isNaN(v))v=0;if(v<0)v=0;if(v>10)v=10;
  v=Math.round(v*100)/100;
  i.value=v.toFixed(2);
  document.getElementById('sliderV').value=v*10;
  document.getElementById('sliderVValue').textContent=v.toFixed(1);
  enviar('setV',v);
}
function aplicarIExacto(){
  const i=document.getElementById('inputIExacto');
  let v=parseFloat(i.value);
  if(isNaN(v))v=4;if(v<4)v=4;if(v>20)v=20;
  v=Math.round(v*100)/100;
  i.value=v.toFixed(2);
  document.getElementById('sliderI').value=v*10;
  document.getElementById('sliderIValue').textContent=v.toFixed(1);
  enviar('setI',v);
}
function setValorV(v){
  document.getElementById('sliderV').value=v*10;
  document.getElementById('sliderVValue').textContent=v.toFixed(1);
  document.getElementById('inputVExacto').value=v.toFixed(2);
  enviar('setV',v);
}
function setValorI(i){
  document.getElementById('sliderI').value=i*10;
  document.getElementById('sliderIValue').textContent=i.toFixed(1);
  document.getElementById('inputIExacto').value=i.toFixed(2);
  enviar('setI',i);
}

// 🆕 FUNCIONES DE CALIBRACIÓN
function toggleCalibracion(){
  const p=document.getElementById('panelCalibracion');
  p.classList.toggle('activo');
  if(p.classList.contains('activo'))cargarCalData();
}

function cargarCalData(){
  enviar('getCal',0);
}

function guardarCalData(){
  // Recoger tabla de voltaje
  const tablaV=[];
  for(let i=0;i<21;i++){
    const el=document.getElementById('cv'+i);
    let v=parseFloat(el.value);
    if(isNaN(v))v=0;
    tablaV.push(v);
  }
  enviar('setCalV',tablaV);

  // Corriente
  setTimeout(()=>{
    enviar('setCalI',{
      gain:parseFloat(document.getElementById('calISpan').value)||0.96875,
      offset:parseFloat(document.getElementById('calIZero').value)||0.625
    });
  },50);

  // Medición V
  setTimeout(()=>{
    enviar('setCalMV',{
      gain:parseFloat(document.getElementById('calMVGain').value)||1.0,
      offset:parseFloat(document.getElementById('calMVOff').value)||0.0
    });
  },100);

  // Medición I
  setTimeout(()=>{
    enviar('setCalMI',{
      gain:parseFloat(document.getElementById('calMIGain').value)||1.0,
      offset:parseFloat(document.getElementById('calMIOff').value)||0.0
    });
  },150);

  // Guardar en NVS
  setTimeout(()=>enviar('saveCal',0),250);
  setTimeout(()=>showToast('💾 Calibración guardada'),400);
}

function restaurarFab(){
  if(confirm('¿Restaurar valores de fábrica? Se perderá la calibración actual.')){
    enviar('resetCal',0);
    setTimeout(()=>cargarCalData(),300);
  }
}

document.addEventListener('DOMContentLoaded',function(){
  generarTablaCalV();
  document.getElementById('inputVExacto').addEventListener('keypress',e=>{if(e.key==='Enter')aplicarVExacto();});
  document.getElementById('inputIExacto').addEventListener('keypress',e=>{if(e.key==='Enter')aplicarIExacto();});
  conectarWebSocket();
});
</script>
</body>
</html>
)rawliteral";

// ==========================================
// 🌐 HANDLERS WEB
// ==========================================
void handleRoot() { server.send(200, "text/html", htmlPage); }
void handleNotFound() { server.send(404, "text/plain", "404"); }

// ==========================================
// 🔌 WEBSOCKETS
// ==========================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WS] #%u desconectado\n", num);
      break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[WS] #%u conectado\n", num);
      enviarEstadoWeb();
      break;
    }
    case WStype_TEXT: {
      String msg = String((char*)payload);
      procesarComandoWeb(msg);
      break;
    }
  }
}

void procesarComandoWeb(String msg) {
  if (msg.indexOf("\"cmd\":\"modo\"") != -1) {
    int v = extraerValorInt(msg);
    if (v >= 1 && v <= 6) cambiarEstadoA(v);
  }
  else if (msg.indexOf("\"cmd\":\"menu\"") != -1) {
    cambiarEstadoA(MENU_PRINCIPAL);
  }
  else if (msg.indexOf("\"cmd\":\"setV\"") != -1) {
    float v = extraerValorFloat(msg);
    if (v >= 0.0 && v <= 10.0) {
      vManualTarget = v;
      if (estadoActual != INYECTAR_V_MAN) cambiarEstadoA(INYECTAR_V_MAN);
      actualizarPantallaDinamica();
    }
  }
  else if (msg.indexOf("\"cmd\":\"setI\"") != -1) {
    float v = extraerValorFloat(msg);
    if (v >= 4.0 && v <= 20.0) {
      iManualTarget = v;
      if (estadoActual != INYECTAR_I_MAN) cambiarEstadoA(INYECTAR_I_MAN);
      actualizarPantallaDinamica();
    }
  }
  // 🆕 COMANDOS DE CALIBRACIÓN
  else if (msg.indexOf("\"cmd\":\"getCal\"") != -1) {
    enviarDatosCalibracion();
  }
  else if (msg.indexOf("\"cmd\":\"setCalV\"") != -1) {
    // Espera un array de 21 floats
    int start = msg.indexOf("[");
    int end = msg.indexOf("]");
    if (start != -1 && end != -1) {
      String arr = msg.substring(start + 1, end);
      int idx = 0;
      int pos = 0;
      while (pos < arr.length() && idx < NUM_PUNTOS_CAL_V) {
        int comma = arr.indexOf(',', pos);
        String val;
        if (comma == -1) { val = arr.substring(pos); pos = arr.length(); }
        else { val = arr.substring(pos, comma); pos = comma + 1; }
        float f = val.toFloat();
        tablaRealV[idx++] = f;
      }
      Serial.printf("[CAL] Tabla V actualizada (%d puntos)\n", idx);
    }
  }
  else if (msg.indexOf("\"cmd\":\"setCalI\"") != -1) {
    CAL_I_GAIN = extraerCampoFloat(msg, "gain");
    CAL_I_OFFSET = extraerCampoFloat(msg, "offset");
    Serial.printf("[CAL] I: gain=%.4f offset=%.3f\n", CAL_I_GAIN, CAL_I_OFFSET);
  }
  else if (msg.indexOf("\"cmd\":\"setCalMV\"") != -1) {
    CAL_MEAS_V_GAIN = extraerCampoFloat(msg, "gain");
    CAL_MEAS_V_OFFSET = extraerCampoFloat(msg, "offset");
    Serial.printf("[CAL] Med V: gain=%.3f offset=%.2f\n", CAL_MEAS_V_GAIN, CAL_MEAS_V_OFFSET);
  }
  else if (msg.indexOf("\"cmd\":\"setCalMI\"") != -1) {
    CAL_MEAS_I_GAIN = extraerCampoFloat(msg, "gain");
    CAL_MEAS_I_OFFSET = extraerCampoFloat(msg, "offset");
    Serial.printf("[CAL] Med I: gain=%.3f offset=%.2f\n", CAL_MEAS_I_GAIN, CAL_MEAS_I_OFFSET);
  }
  else if (msg.indexOf("\"cmd\":\"saveCal\"") != -1) {
    guardarCalibracion();
    webSocket.broadcastTXT("{\"cmd\":\"calOk\"}");
  }
  else if (msg.indexOf("\"cmd\":\"resetCal\"") != -1) {
    restaurarFabrica();
    webSocket.broadcastTXT("{\"cmd\":\"calReset\"}");
  }
}

// Funciones auxiliares para parseo JSON simple
int extraerValorInt(String msg) {
  int s = msg.indexOf("\"value\":") + 8;
  int e = msg.indexOf("}", s);
  if (e == -1) e = msg.length();
  return msg.substring(s, e).toInt();
}

float extraerValorFloat(String msg) {
  int s = msg.indexOf("\"value\":") + 8;
  int e = msg.indexOf("}", s);
  if (e == -1) e = msg.length();
  return msg.substring(s, e).toFloat();
}

float extraerCampoFloat(String msg, const char* campo) {
  String buscar = String("\"") + campo + "\":";
  int s = msg.indexOf(buscar) + buscar.length();
  if (s < buscar.length()) return 0;
  int e = msg.indexOf(",", s);
  if (e == -1) e = msg.indexOf("}", s);
  if (e == -1) e = msg.length();
  return msg.substring(s, e).toFloat();
}

void enviarDatosCalibracion() {
  String json = "{\"cmd\":\"calData\",\"tablaV\":[";
  for (int i = 0; i < NUM_PUNTOS_CAL_V; i++) {
    json += String(tablaRealV[i], 3);
    if (i < NUM_PUNTOS_CAL_V - 1) json += ",";
  }
  json += "],\"iGain\":" + String(CAL_I_GAIN, 4);
  json += ",\"iOffset\":" + String(CAL_I_OFFSET, 3);
  json += ",\"mvGain\":" + String(CAL_MEAS_V_GAIN, 3);
  json += ",\"mvOffset\":" + String(CAL_MEAS_V_OFFSET, 2);
  json += ",\"miGain\":" + String(CAL_MEAS_I_GAIN, 3);
  json += ",\"miOffset\":" + String(CAL_MEAS_I_OFFSET, 2);
  json += "}";
  webSocket.broadcastTXT(json);
}

void enviarEstadoWeb() {
  String modoNombre = "", unidadOut = "", unidadIn = "";
  float valorOut = 0.0, valorIn = 0.0, porcentaje = 0.0;
  int modoNum = estadoActual;

  switch(estadoActual) {
    case MENU_PRINCIPAL: modoNombre="MENU PRINCIPAL"; unidadOut="---"; unidadIn="---"; break;
    case INYECTAR_V_MAN:
      modoNombre="INYECTAR V MANUAL"; valorOut=vManualTarget; valorIn=ultimoValorInV;
      porcentaje=(vManualTarget/10.0)*100.0; unidadOut="V"; unidadIn="V"; break;
    case INYECTAR_V_AUT:
      modoNombre="INYECTAR V AUTO"; valorOut=rampaVoltaje[indiceRampaAuto]; valorIn=ultimoValorInV;
      porcentaje=(rampaVoltaje[indiceRampaAuto]/10.0)*100.0; unidadOut="V"; unidadIn="V"; break;
    case INYECTAR_I_MAN:
      modoNombre="INYECTAR I MANUAL"; valorOut=iManualTarget; valorIn=ultimoValorInI;
      porcentaje=((iManualTarget-4.0)/16.0)*100.0; unidadOut="mA"; unidadIn="mA"; break;
    case INYECTAR_I_AUT:
      modoNombre="INYECTAR I AUTO"; valorOut=rampaCorriente[indiceRampaAuto]; valorIn=ultimoValorInI;
      porcentaje=((rampaCorriente[indiceRampaAuto]-4.0)/16.0)*100.0; unidadOut="mA"; unidadIn="mA"; break;
    case MEDIR_V:
      modoNombre="MEDIR 0-10V"; valorIn=ultimoValorInV; unidadOut="---"; unidadIn="V"; break;
    case MEDIR_I:
      modoNombre="MEDIR 4-20mA"; valorIn=ultimoValorInI; unidadOut="---"; unidadIn="mA"; break;
  }

  String json = "{";
  json += "\"modo\":\"" + modoNombre + "\",";
  json += "\"modoNum\":" + String(modoNum) + ",";
  json += "\"out\":" + String(valorOut, 2) + ",";
  json += "\"in\":" + String(valorIn, 2) + ",";
  json += "\"porcentaje\":" + String(porcentaje, 1) + ",";
  json += "\"unidadOut\":\"" + unidadOut + "\",";
  json += "\"unidadIn\":\"" + unidadIn + "\"";
  json += "}";
  webSocket.broadcastTXT(json);
}

// ==========================================
// 🛠️ SETUP
// ==========================================
void setup() {
  pinMode(LED_VOLT, OUTPUT);
  pinMode(LED_CURR, OUTPUT);
  digitalWrite(LED_VOLT, LOW);
  digitalWrite(LED_CURR, LOW);

  dacWrite(DAC_VOLT, 0);
  dacWrite(DAC_CURR, 0);

  Serial.begin(115200);
  Serial.println("\n=== CALIBRADOR MVP v2.3 ===");

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // 💾 Cargar calibración desde NVS
  cargarCalibracion();

  // 📶 WiFi AP
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("✅ AP IP: "); Serial.println(IP);

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);

  mostrarPantallaBienvenida();
  tft.setTextSize(1);
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(10, 115);
  tft.print("IP:"); tft.print(IP.toString());

  delay(5000);
  cambiarEstadoA(MENU_PRINCIPAL);
  ultimoTiempoBoton = millis();
}

// ==========================================
// 🔄 LOOP
// ==========================================
void loop() {
  server.handleClient();
  webSocket.loop();
  gestionarPulsadores();

  if (estadoActual == INYECTAR_V_MAN) {
    float vReal = interpolarCalibracionV(vManualTarget);
    int dac = constrain(round((vReal / 10.0) * DAC_VOLT_LIMIT), 0, 255);
    dacWrite(DAC_VOLT, dac);
    if (millis() - ultimoRefrescoMedicion >= INTERVALO_MEDICION) {
      int raw = analogRead(ADC_VOLT);
      float vSinCorr = (raw / 4095.0) * 3.3 * ((10.0 + 3.3) / 3.3);
      ultimoValorInV = (vSinCorr - CAL_MEAS_V_OFFSET) / CAL_MEAS_V_GAIN;
      actualizarPantallaDinamica();
      ultimoRefrescoMedicion = millis();
    }
  }
  else if (estadoActual == INYECTAR_V_AUT) {
    if (millis() - ultimoCambioRampaAuto >= INTERVALO_RAMPA) {
      indiceRampaAuto = (indiceRampaAuto + 1) % TOTAL_PASOS_RAMPA;
      ultimoCambioRampaAuto = millis();
    }
    float vReal = interpolarCalibracionV(rampaVoltaje[indiceRampaAuto]);
    int dac = constrain(round((vReal / 10.0) * DAC_VOLT_LIMIT), 0, 255);
    dacWrite(DAC_VOLT, dac);
    if (millis() - ultimoRefrescoMedicion >= INTERVALO_MEDICION) {
      int raw = analogRead(ADC_VOLT);
      float vSinCorr = (raw / 4095.0) * 3.3 * ((10.0 + 3.3) / 3.3);
      ultimoValorInV = (vSinCorr - CAL_MEAS_V_OFFSET) / CAL_MEAS_V_GAIN;
      actualizarPantallaDinamica();
      ultimoRefrescoMedicion = millis();
    }
  }
  else if (estadoActual == INYECTAR_I_MAN) {
    float iEquiv = (iManualTarget - CAL_I_OFFSET) / CAL_I_GAIN;
    iEquiv = constrain(iEquiv, 0.0, 20.0);
    int dac = constrain(46 + round(((iEquiv - 4.0) / 16.0) * 186), 0, 255);
    dacWrite(DAC_CURR, dac);
    if (millis() - ultimoRefrescoMedicion >= INTERVALO_MEDICION) {
      int raw = analogRead(ADC_CURR);
      float vShunt = (raw / 4095.0) * 3.3;
      float iSinCorr = (vShunt / 150.0) * 1000.0;
      ultimoValorInI = (iSinCorr - CAL_MEAS_I_OFFSET) / CAL_MEAS_I_GAIN;
      actualizarPantallaDinamica();
      ultimoRefrescoMedicion = millis();
    }
  }
  else if (estadoActual == INYECTAR_I_AUT) {
    if (millis() - ultimoCambioRampaAuto >= INTERVALO_RAMPA) {
      indiceRampaAuto = (indiceRampaAuto + 1) % TOTAL_PASOS_RAMPA;
      ultimoCambioRampaAuto = millis();
    }
    float iEquiv = (rampaCorriente[indiceRampaAuto] - CAL_I_OFFSET) / CAL_I_GAIN;
    iEquiv = constrain(iEquiv, 0.0, 20.0);
    int dac = constrain(46 + round(((iEquiv - 4.0) / 16.0) * 186), 0, 255);
    dacWrite(DAC_CURR, dac);
    if (millis() - ultimoRefrescoMedicion >= INTERVALO_MEDICION) {
      int raw = analogRead(ADC_CURR);
      float vShunt = (raw / 4095.0) * 3.3;
      float iSinCorr = (vShunt / 150.0) * 1000.0;
      ultimoValorInI = (iSinCorr - CAL_MEAS_I_OFFSET) / CAL_MEAS_I_GAIN;
      actualizarPantallaDinamica();
      ultimoRefrescoMedicion = millis();
    }
  }
  else if (estadoActual == MEDIR_V || estadoActual == MEDIR_I) {
    if (millis() - ultimoRefrescoMedicion >= INTERVALO_MEDICION) {
      if (estadoActual == MEDIR_V) {
        int raw = analogRead(ADC_VOLT);
        float vSinCorr = (raw / 4095.0) * 3.3 * ((10.0 + 3.3) / 3.3);
        ultimoValorInV = (vSinCorr - CAL_MEAS_V_OFFSET) / CAL_MEAS_V_GAIN;
      } else {
        int raw = analogRead(ADC_CURR);
        float vShunt = (raw / 4095.0) * 3.3;
        float iSinCorr = (vShunt / 150.0) * 1000.0;
        ultimoValorInI = (iSinCorr - CAL_MEAS_I_OFFSET) / CAL_MEAS_I_GAIN;
      }
      actualizarPantallaDinamica();
      ultimoRefrescoMedicion = millis();
    }
  }

  // 📶 Indicador WiFi (cuadrado discreto)
  if (millis() - ultimoRefrescoWiFi >= INTERVALO_WIFI) {
    int clientes = WiFi.softAPgetStationNum();
    if (clientes != clientesWiFiAnterior) {
      dibujarIndicadorWiFi(clientes);
      clientesWiFiAnterior = clientes;
    }
    ultimoRefrescoWiFi = millis();
  }

  if (millis() - ultimoEnvioWS >= INTERVALO_WS) {
    enviarEstadoWeb();
    ultimoEnvioWS = millis();
  }
}

// ==========================================
// 🖱️ BOTONES
// ==========================================
void gestionarPulsadores() {
  if ((millis() - ultimoTiempoBoton) > TIEMPO_DEBOUNCE) {
    if (digitalRead(BTN_BACK) == LOW) {
      if (estadoActual != MENU_PRINCIPAL) cambiarEstadoA(MENU_PRINCIPAL);
      ultimoTiempoBoton = millis(); return;
    }
    if (digitalRead(BTN_OK) == LOW) {
      if (estadoActual == MENU_PRINCIPAL) cambiarEstadoA(opcionSeleccionada + 1);
      ultimoTiempoBoton = millis(); return;
    }
    if (digitalRead(BTN_DOWN) == LOW) {
      if (estadoActual == MENU_PRINCIPAL) {
        opcionSeleccionada = (opcionSeleccionada + 1) % TOTAL_OPCIONES;
        dibujarMenu();
      } else if (estadoActual == INYECTAR_V_MAN) {
        if (vManualTarget >= 0.5) vManualTarget -= 0.5;
      } else if (estadoActual == INYECTAR_I_MAN) {
        if (iManualTarget >= 5.0) iManualTarget -= 1.0;
      }
      ultimoTiempoBoton = millis(); return;
    }
    if (digitalRead(BTN_UP) == LOW) {
      if (estadoActual == MENU_PRINCIPAL) {
        opcionSeleccionada = (opcionSeleccionada - 1 + TOTAL_OPCIONES) % TOTAL_OPCIONES;
        dibujarMenu();
      } else if (estadoActual == INYECTAR_V_MAN) {
        if (vManualTarget <= 9.5) vManualTarget += 0.5;
      } else if (estadoActual == INYECTAR_I_MAN) {
        if (iManualTarget <= 19.0) iManualTarget += 1.0;
      }
      ultimoTiempoBoton = millis(); return;
    }
  }
}

void cambiarEstadoA(int nuevoEstado) {
  estadoActual = (Estados)nuevoEstado;
  tft.fillScreen(ST7735_BLACK);
  vManualAnterior = -1.0;
  iManualAnterior = -1.0;
  indiceRampaAnterior = -1;
  indiceRampaAuto = 0;
  ultimoCambioRampaAuto = millis();
  vManualTarget = 0.0;
  iManualTarget = 4.0;

  if (estadoActual == MENU_PRINCIPAL) {
    digitalWrite(LED_VOLT, LOW);
    digitalWrite(LED_CURR, LOW);
    dacWrite(DAC_VOLT, 0);
    dacWrite(DAC_CURR, 0);
    dibujarMenu();
  }
  else if (estadoActual == INYECTAR_V_MAN || estadoActual == INYECTAR_V_AUT || estadoActual == MEDIR_V) {
    digitalWrite(LED_VOLT, HIGH);
    digitalWrite(LED_CURR, LOW);
    dibujarInterfazOperacion();
    actualizarPantallaDinamica();
  }
  else if (estadoActual == INYECTAR_I_MAN || estadoActual == INYECTAR_I_AUT || estadoActual == MEDIR_I) {
    digitalWrite(LED_VOLT, LOW);
    digitalWrite(LED_CURR, HIGH);
    dibujarInterfazOperacion();
    actualizarPantallaDinamica();
  }
  clientesWiFiAnterior = -1;
  enviarEstadoWeb();
}

// ==========================================
// 📺 PANTALLAS
// ==========================================
void mostrarPantallaBienvenida() {
  tft.setTextColor(ST7735_ORANGE);
  tft.setTextSize(1);
  tft.setCursor(15, 15);
  tft.print("UNEFA - CORPOELEC");
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(15, 45);
  tft.print("PROYECTO - MVP");
  tft.setCursor(15, 65);
  tft.print("Calibrador de Lazos");
  tft.drawFastHLine(10, 85, 140, ST7735_BLUE);
  tft.setCursor(15, 100);
  tft.setTextColor(ST7735_GREEN);
  tft.print("v2.3 Cal. Remota");
}

void dibujarMenu() {
  tft.fillScreen(ST7735_BLACK);
  tft.fillRect(0, 0, 160, 16, ST7735_BLUE);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(1);
  tft.setCursor(15, 4);
  tft.print("MENU DE SELECCION");
  for (int i = 0; i < TOTAL_OPCIONES; i++) {
    tft.setCursor(5, 26 + (i * 16));
    if (i == opcionSeleccionada) {
      tft.setTextColor(ST7735_GREEN);
      tft.print("> ");
    } else {
      tft.setTextColor(ST7735_WHITE);
      tft.print("  ");
    }
    tft.print(menuItems[i]);
  }
  dibujarIndicadorWiFi(WiFi.softAPgetStationNum());
}

void dibujarInterfazOperacion() {
  tft.fillRect(0, 0, 160, 18, ST7735_DARKGREY);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  switch (estadoActual) {
    case INYECTAR_V_MAN: tft.print("INJECT V: MANUAL"); break;
    case INYECTAR_V_AUT: tft.print("INJECT V: AUTO 10s"); break;
    case INYECTAR_I_MAN: tft.print("INJECT I: MANUAL"); break;
    case INYECTAR_I_AUT: tft.print("INJECT I: AUTO 10s"); break;
    case MEDIR_V:        tft.print("MEASURE: 0-10 VDC"); break;
    case MEDIR_I:        tft.print("MEASURE: 4-20 mA"); break;
    default: break;
  }
  tft.setCursor(5, 116);
  tft.setTextColor(ST7735_RED);
  tft.print("[BACK] para regresar");
  dibujarIndicadorWiFi(WiFi.softAPgetStationNum());
}

// 🆕 INDICADOR WiFi DISCRETO (cuadrado pequeño)
void dibujarIndicadorWiFi(int clientes) {
  const int x = 148;  // Esquina superior derecha
  const int y = 3;
  const int size = 10;

  // Limpiar área
  tft.fillRect(x, y, size + 2, size + 2, ST7735_DARKGREY);

  // Color según estado
  uint16_t color = (clientes > 0) ? ST7735_GREEN : ST7735_RED;

  // Dibujar cuadrado
  tft.fillRect(x + 1, y + 1, size, size, color);

  // Número de clientes dentro
  tft.setTextColor(ST7735_BLACK);
  tft.setTextSize(1);
  tft.setCursor(x + 3, y + 2);
  tft.print(clientes > 9 ? 9 : clientes);
}

void actualizarPantallaDinamica() {
  tft.fillRect(10, 25, 140, 85, ST7735_BLACK);

  float valorOut = 0.0, valorIn = 0.0, porcentaje = 0.0;
  uint16_t colorOut = ST7735_WHITE, colorIn = ST7735_WHITE;
  bool mostrarPorcentaje = false;
  const char* unidadOut = "";
  const char* unidadIn = "";
  bool esModoInyeccion = false;

  if (estadoActual == INYECTAR_V_MAN) {
    valorOut = vManualTarget; valorIn = ultimoValorInV;
    porcentaje = (vManualTarget / 10.0) * 100.0;
    colorOut = ST7735_GREEN; colorIn = ST7735_YELLOW;
    mostrarPorcentaje = true; unidadOut = " V"; unidadIn = " V";
    esModoInyeccion = true;
  }
  else if (estadoActual == INYECTAR_V_AUT) {
    valorOut = rampaVoltaje[indiceRampaAuto]; valorIn = ultimoValorInV;
    porcentaje = (rampaVoltaje[indiceRampaAuto] / 10.0) * 100.0;
    colorOut = ST7735_GREEN; colorIn = ST7735_YELLOW;
    mostrarPorcentaje = true; unidadOut = " V"; unidadIn = " V";
    esModoInyeccion = true;
  }
  else if (estadoActual == INYECTAR_I_MAN) {
    valorOut = iManualTarget; valorIn = ultimoValorInI;
    porcentaje = ((iManualTarget - 4.0) / 16.0) * 100.0;
    colorOut = ST7735_CYAN; colorIn = ST7735_MAGENTA;
    mostrarPorcentaje = true; unidadOut = " mA"; unidadIn = " mA";
    esModoInyeccion = true;
  }
  else if (estadoActual == INYECTAR_I_AUT) {
    valorOut = rampaCorriente[indiceRampaAuto]; valorIn = ultimoValorInI;
    porcentaje = ((rampaCorriente[indiceRampaAuto] - 4.0) / 16.0) * 100.0;
    colorOut = ST7735_CYAN; colorIn = ST7735_MAGENTA;
    mostrarPorcentaje = true; unidadOut = " mA"; unidadIn = " mA";
    esModoInyeccion = true;
  }
  else if (estadoActual == MEDIR_V) {
    valorIn = ultimoValorInV; colorIn = ST7735_YELLOW; unidadIn = " V";
  }
  else if (estadoActual == MEDIR_I) {
    valorIn = ultimoValorInI; colorIn = ST7735_MAGENTA; unidadIn = " mA";
  }

  if (esModoInyeccion) {
    tft.setTextSize(1); tft.setTextColor(ST7735_WHITE);
    tft.setCursor(10, 25); tft.print("OUT:");
    tft.setTextSize(2); tft.setTextColor(colorOut);
    tft.setCursor(10, 38); tft.print(valorOut, 2);
    tft.setTextSize(1); tft.print(unidadOut);

    tft.setTextSize(1); tft.setTextColor(ST7735_WHITE);
    tft.setCursor(10, 65); tft.print("IN: ");
    tft.setTextSize(2); tft.setTextColor(colorIn);
    tft.setCursor(10, 78); tft.print(valorIn, 2);
    tft.setTextSize(1); tft.print(unidadIn);

    if (mostrarPorcentaje) {
      tft.setTextSize(1); tft.setTextColor(ST7735_WHITE);
      tft.setCursor(10, 100); tft.print(porcentaje, 1); tft.print(" %");
    }
  } else {
    tft.setTextSize(3); tft.setTextColor(colorIn);
    tft.setCursor(15, 45); tft.print(valorIn, 2);
    tft.setTextSize(1); tft.print(unidadIn);
  }
}