/*
 * Reloj Analógico con Configuración WiFi y Alertas por EmailJS
 * TTGO T-Display ESP32 - 135x240 px
 * Botón izquierdo: Cambia estilo de reloj
 * Botón derecho: Forzar modo configuración
 */

#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include <math.h>
#include <WebServer.h>
#include <Preferences.h>

// ─── Configuración ───────────────────────────────────────────────────────────
const char* ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = -21600;
const int   daylightOffset_sec = 3600;

// ─── Punto de Acceso para configuración ──────────────────────────────────────
const char* ap_ssid     = "FALLBAND_SETUP";
const char* ap_password = "12345678";
WebServer server(80);
Preferences preferences;

bool modoConfiguracion = false;
bool wifiConfigurado = false;
String saved_ssid = "";
String saved_password = "";
String saved_telefono = "";
String saved_correo = "";
unsigned long tiempoActivacionConfig = 0;
const unsigned long tiempoConfigActivo = 300000; // 5 minutos

TFT_eSPI tft = TFT_eSPI();

// ─── Dimensiones pantalla ─────────────────────────────────────────────────────
#define SW  135
#define SH  240

// ─── Botones ─────────────────────────────────────────────────────────────────
#define BTN_LEFT  0   // GPIO0 - Botón izquierdo
#define BTN_RIGHT 35  // GPIO35 - Botón derecho

// ─── Estilos de reloj ────────────────────────────────────────────────────────
enum EstiloReloj {
  ESTILO_CLASICO,
  ESTILO_MODERNO,
  ESTILO_DEPORTIVO,
  ESTILO_MINIMALISTA,
  ESTILO_VINTAGE,
  TOTAL_ESTILOS
};

int estiloActual = ESTILO_CLASICO;
bool botonPresionado = false;
unsigned long ultimoCambio = 0;
const unsigned long tiempoMinimoEntreCambios = 300;

// ─── Caja y esfera ───────────────────────────────────────────────────────────
#define CASE_X  1
#define CASE_Y  1
#define CASE_W  (SW - 2)
#define CASE_H  (SH - 2)
#define DIAL_PAD  6
#define DIAL_X    (CASE_X + DIAL_PAD)
#define DIAL_Y    (CASE_Y + DIAL_PAD)
#define DIAL_W    (CASE_W - DIAL_PAD * 2)
#define DIAL_H    (CASE_H - DIAL_PAD * 2)
#define CX  (DIAL_X + DIAL_W / 2)
#define CY  (DIAL_Y + DIAL_H / 2)
#define R   (DIAL_W / 2 - 3)
#define LEN_HOUR  (R - 12)
#define LEN_MIN   (R - 4)
#define LEN_SEC   (R - 1)
#define LEN_TAIL  13

// ─── Colores ──────────────────────────────────────────────────────────────────
struct PaletaColores {
  uint16_t fondo, marcadores, numeros, manecillas, segundero, acento, textoMarca, caja;
};

PaletaColores paletas[TOTAL_ESTILOS];

void inicializarPaletas() {
  paletas[ESTILO_CLASICO] = {tft.color565(250,248,240), tft.color565(8,10,28), tft.color565(8,10,28), tft.color565(20,58,195), tft.color565(205,22,12), tft.color565(208,158,78), tft.color565(80,72,52), tft.color565(208,158,78)};
  paletas[ESTILO_MODERNO] = {tft.color565(20,25,45), tft.color565(0,255,255), tft.color565(255,255,255), tft.color565(0,200,255), tft.color565(255,100,100), tft.color565(0,255,200), tft.color565(150,200,255), tft.color565(100,100,120)};
  paletas[ESTILO_DEPORTIVO] = {tft.color565(30,30,30), tft.color565(255,60,60), tft.color565(255,255,255), tft.color565(255,80,80), tft.color565(255,200,0), tft.color565(255,60,60), tft.color565(255,100,100), tft.color565(80,80,80)};
  paletas[ESTILO_MINIMALISTA] = {tft.color565(255,255,255), tft.color565(0,0,0), tft.color565(0,0,0), tft.color565(80,80,80), tft.color565(120,120,120), tft.color565(200,200,200), tft.color565(100,100,100), tft.color565(180,180,180)};
  paletas[ESTILO_VINTAGE] = {tft.color565(245,222,179), tft.color565(101,67,33), tft.color565(101,67,33), tft.color565(139,69,19), tft.color565(205,133,63), tft.color565(184,115,51), tft.color565(160,100,50), tft.color565(184,134,11)};
}

unsigned long lastUpdate = 0;
int segundoAnterior = -1;

// ─── Funciones de dibujo del reloj ───────────────────────────────────────────
inline int angX(float deg, int r) { return CX + (int)(r * sinf(deg * M_PI / 180.0f)); }
inline int angY(float deg, int r) { return CY - (int)(r * cosf(deg * M_PI / 180.0f)); }

void dibujarEspada(float angDeg, int largo, int grosorMax, uint16_t color) {
  float rad = (angDeg - 90.0f) * M_PI / 180.0f;
  float dx = cosf(rad), dy = sinf(rad);
  float px_ = -dy, py_ = dx;
  int total = largo + LEN_TAIL;
  for (int i = 0; i <= total; i++) {
    float t = (float)i / total;
    float pos = (float)(i - LEN_TAIL) / largo;
    float w;
    if (pos < 0.0f) w = grosorMax * 0.30f;
    else if (pos < 0.25f) w = grosorMax * (pos / 0.25f);
    else w = grosorMax * (1.0f - (pos - 0.25f) / 0.75f);
    if (w < 0.5f) w = 0.5f;
    int bx = CX + (int)((-LEN_TAIL + (largo + LEN_TAIL) * t) * dx);
    int by = CY + (int)((-LEN_TAIL + (largo + LEN_TAIL) * t) * dy);
    int ax = (int)(w * px_), ay = (int)(w * py_);
    tft.drawLine(bx - ax, by - ay, bx + ax, by + ay, color);
  }
}

void dibujarCaja() {
  PaletaColores pal = paletas[estiloActual];
  tft.fillRoundRect(CASE_X + 2, CASE_Y + 2, CASE_W, CASE_H, 3, tft.color565(55,38,8));
  tft.fillRoundRect(CASE_X, CASE_Y, CASE_W, CASE_H, 3, pal.caja);
  tft.drawRoundRect(CASE_X, CASE_Y, CASE_W, CASE_H, 3, tft.color565(128,88,28));
  tft.drawRoundRect(CASE_X + 1, CASE_Y + 1, CASE_W - 2, CASE_H - 2, 2, tft.color565(242,202,132));
}

void dibujarEsferaEstatica() {
  PaletaColores pal = paletas[estiloActual];
  tft.fillRect(DIAL_X, DIAL_Y, DIAL_W, DIAL_H, pal.fondo);
  tft.drawRect(DIAL_X, DIAL_Y, DIAL_W, DIAL_H, pal.marcadores);
  tft.drawRect(DIAL_X + 1, DIAL_Y + 1, DIAL_W - 2, DIAL_H - 2, pal.acento);
  
  for (int i = 0; i < 60; i++) {
    if (i % 5 == 0) continue;
    float a = i * 6.0f;
    tft.fillCircle(angX(a, R), angY(a, R), 1, pal.marcadores);
  }
  
  for (int i = 1; i <= 12; i++) {
    float a = i * 30.0f;
    int x1 = angX(a, R - 8), y1 = angY(a, R - 8);
    int x2 = angX(a, R), y2 = angY(a, R);
    tft.drawLine(x1, y1, x2, y2, pal.marcadores);
    tft.drawLine(x1 + 1, y1, x2 + 1, y2, pal.marcadores);
  }
  
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(pal.numeros, pal.fondo);
  tft.setTextSize(2);
  int bn[] = {12, 3, 6, 9};
  for (int n : bn) {
    float a = n * 30.0f;
    int nx = angX(a, R - 17), ny = angY(a, R - 17);
    tft.fillRect(nx - 9, ny - 8, 18, 16, pal.fondo);
    char buf[3];
    sprintf(buf, "%d", n);
    tft.drawString(buf, nx, ny);
  }
  
  tft.setTextSize(1);
  int sn[] = {1, 2, 4, 5, 7, 8, 10, 11};
  for (int n : sn) {
    float a = n * 30.0f;
    int nx = angX(a, R - 13), ny = angY(a, R - 13);
    tft.fillRect(nx - 6, ny - 4, 14, 9, pal.fondo);
    char buf[3];
    sprintf(buf, "%d", n);
    tft.drawString(buf, nx, ny);
  }
  
  int marcaY = DIAL_Y + 12;
  tft.fillRect(CX - 35, marcaY - 5, 70, 12, pal.fondo);
  tft.setTextColor(pal.textoMarca, pal.fondo);
  tft.drawString("FALLBAND", CX, marcaY);
  tft.drawFastHLine(CX - 28, marcaY + 3, 56, pal.acento);
}

void dibujarEsferaCompleta() {
  dibujarEsferaEstatica();
  struct tm t;
  if (getLocalTime(&t)) {
    PaletaColores pal = paletas[estiloActual];
    int winX = CX - 16, winY = CY + 10;
    tft.drawRect(winX, winY, 32, 13, pal.acento);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(pal.numeros, pal.fondo);
    char dia[3];
    sprintf(dia, "%02d", t.tm_mday);
    tft.drawString(dia, CX, winY + 6);
    
    int horaZoneY = DIAL_Y + DIAL_H - 22;
    tft.setTextColor(pal.manecillas, pal.fondo);
    char horaStr[9];
    sprintf(horaStr, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    tft.drawString(horaStr, CX, horaZoneY + 6);
    
    const char* dias[] = {"DOM","LUN","MAR","MIE","JUE","VIE","SAB"};
    tft.setTextColor(pal.acento, pal.fondo);
    tft.drawString(dias[t.tm_wday], CX, horaZoneY + 15);
  }
}

void actualizarManecillas(struct tm &t) {
  int H = t.tm_hour % 12, M = t.tm_min, S = t.tm_sec;
  if (S == segundoAnterior) return;
  segundoAnterior = S;
  PaletaColores pal = paletas[estiloActual];
  dibujarEsferaEstatica();
  
  float angH = H * 30.0f + M * 0.5f;
  dibujarEspada(angH, LEN_HOUR, 5, pal.manecillas);
  float angM = M * 6.0f;
  dibujarEspada(angM, LEN_MIN, 4, pal.manecillas);
  
  float sRad = (S * 6.0f - 90.0f) * M_PI / 180.0f;
  int stX = CX + (int)(LEN_SEC * cosf(sRad));
  int stY = CY + (int)(LEN_SEC * sinf(sRad));
  int scX = CX - (int)(LEN_TAIL * cosf(sRad));
  int scY = CY - (int)(LEN_TAIL * sinf(sRad));
  tft.drawLine(scX, scY, stX, stY, pal.segundero);
  tft.drawLine(scX + 1, scY, stX + 1, stY, pal.segundero);
  
  tft.fillCircle(CX, CY, 5, pal.acento);
  tft.fillCircle(CX, CY, 2, pal.manecillas);
  
  int winX = CX - 16, winY = CY + 10;
  tft.drawRect(winX, winY, 32, 13, pal.acento);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(pal.numeros, pal.fondo);
  char dia[3];
  sprintf(dia, "%02d", t.tm_mday);
  tft.drawString(dia, CX, winY + 6);
  
  int horaZoneY = DIAL_Y + DIAL_H - 22;
  tft.setTextColor(pal.manecillas, pal.fondo);
  char horaStr[9];
  sprintf(horaStr, "%02d:%02d:%02d", t.tm_hour, M, S);
  tft.drawString(horaStr, CX, horaZoneY + 6);
  
  const char* dias[] = {"DOM","LUN","MAR","MIE","JUE","VIE","SAB"};
  tft.setTextColor(pal.acento, pal.fondo);
  tft.drawString(dias[t.tm_wday], CX, horaZoneY + 15);
}

// ─── Página web con EmailJS integrado ────────────────────────────────────────
String paginaConfiguracion() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
    <title>FALLBAND - Configuración</title>
    
    <!-- EmailJS SDK -->
    <script type="text/javascript" src="https://cdn.jsdelivr.net/npm/@emailjs/browser@4/dist/email.min.js"></script>
    <script type="text/javascript">
        // Inicializar EmailJS con tus credenciales
        (function() {
            emailjs.init({
                publicKey: "S278dC2zQyziLhWNb",
            });
        })();
        
        // Configuración de EmailJS
        const EMAILJS_SERVICE_ID = "service_xrvdddn";
        const EMAILJS_TEMPLATE_ID = "template_fallback"; // Deberás crear este template
    </script>
    
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Playfair Display', 'Segoe UI', 'Georgia', serif;
            background: radial-gradient(circle at 10% 20%, #0a0a0a 0%, #1a1a2e 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
            position: relative;
        }
        body::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: url('data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><path fill="none" stroke="rgba(212,175,55,0.05)" stroke-width="0.5" d="M10 10 L90 10 M10 20 L90 20 M10 30 L90 30 M10 40 L90 40 M10 50 L90 50 M10 60 L90 60 M10 70 L90 70 M10 80 L90 80 M10 90 L90 90 M10 10 L10 90 M20 10 L20 90 M30 10 L30 90 M40 10 L40 90 M50 10 L50 90 M60 10 L60 90 M70 10 L70 90 M80 10 L80 90 M90 10 L90 90"/></svg>');
            background-size: 30px 30px;
            opacity: 0.3;
            pointer-events: none;
        }
        .container {
            background: rgba(10, 10, 18, 0.92);
            backdrop-filter: blur(10px);
            border-radius: 30px;
            padding: 40px 35px;
            box-shadow: 0 25px 50px rgba(0,0,0,0.5), 0 0 0 1px rgba(212,175,55,0.2);
            max-width: 480px;
            width: 100%;
            text-align: center;
            position: relative;
            z-index: 1;
        }
        .logo { font-size: 4.5em; margin-bottom: 5px; text-shadow: 0 0 20px rgba(212,175,55,0.5); animation: glow 2s ease-in-out infinite alternate; }
        @keyframes glow { from { text-shadow: 0 0 10px rgba(212,175,55,0.3); } to { text-shadow: 0 0 25px rgba(212,175,55,0.8); } }
        h1 { background: linear-gradient(135deg, #d4af37 0%, #f5e7a3 50%, #b8860b 100%); -webkit-background-clip: text; background-clip: text; color: transparent; margin-bottom: 8px; font-size: 2.2em; }
        .subtitle { color: #9a8c6f; margin-bottom: 30px; font-size: 0.85em; border-bottom: 1px solid rgba(212,175,55,0.3); display: inline-block; }
        .form-group { margin: 22px 0; text-align: left; }
        label { display: block; margin-bottom: 8px; color: #e5d5a8; font-weight: 500; font-size: 0.85em; text-transform: uppercase; }
        input { width: 100%; padding: 14px 18px; background: rgba(20, 20, 30, 0.8); border: 1.5px solid rgba(212,175,55,0.4); border-radius: 12px; font-size: 15px; color: #fff; }
        input:focus { outline: none; border-color: #d4af37; box-shadow: 0 0 15px rgba(212,175,55,0.3); }
        button { background: linear-gradient(135deg, #b8860b 0%, #d4af37 50%, #f5e7a3 100%); color: #1a1a2e; border: none; padding: 14px 30px; font-weight: bold; border-radius: 40px; cursor: pointer; width: 100%; margin-top: 25px; text-transform: uppercase; transition: all 0.3s; }
        button:hover { transform: translateY(-2px); box-shadow: 0 8px 25px rgba(212,175,55,0.4); }
        .info { margin-top: 28px; padding-top: 20px; border-top: 1px solid rgba(212,175,55,0.2); font-size: 0.7em; color: #8a7a5a; }
        .info strong { color: #d4af37; }
        .message { margin-top: 20px; padding: 12px; border-radius: 12px; display: none; }
        .message.success { background: rgba(30, 70, 40, 0.9); color: #d4ffb0; border: 1px solid #d4af37; }
        .message.error { background: rgba(70, 30, 30, 0.9); color: #ffb0b0; border: 1px solid #b8860b; }
        .required { color: #d4af37; }
        .gold-line { width: 60px; height: 2px; background: linear-gradient(90deg, transparent, #d4af37, transparent); margin: 15px auto; }
        .sensor-status { margin-top: 15px; padding: 10px; background: rgba(0,0,0,0.5); border-radius: 10px; font-size: 0.75em; color: #d4af37; transition: all 0.3s; }
        .alert-animation { animation: alertPulse 0.5s ease-in-out 3; }
        @keyframes alertPulse {
            0% { background: rgba(0,0,0,0.5); }
            50% { background: rgba(255,0,0,0.5); }
            100% { background: rgba(0,0,0,0.5); }
        }
        @media (max-width: 550px) { .container { padding: 30px 20px; } .logo { font-size: 3.5em; } }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">⌚</div>
        <h1>FALLBAND</h1>
        <div class="gold-line"></div>
        <div class="subtitle">Configuración de lujo</div>
        
        <form id="configForm">
            <div class="form-group">
                <label>📡 Red WiFi <span class="required">*</span></label>
                <input type="text" id="ssid" required placeholder="SSID de tu red">
            </div>
            <div class="form-group">
                <label>🔑 Contraseña</label>
                <input type="password" id="password" placeholder="••••••••">
            </div>
            <div class="form-group">
                <label>📱 Teléfono <span class="required">*</span></label>
                <input type="tel" id="telefono" required placeholder="Número de contacto">
            </div>
            <div class="form-group">
                <label>✉️ Correo <span class="required">*</span></label>
                <input type="email" id="correo" required placeholder="tu@email.com">
            </div>
            <button type="submit">✦ REGISTRAR Y CONECTAR ✦</button>
        </form>
        
        <div id="message" class="message"></div>
        
        <div class="sensor-status" id="sensorStatus">
            🔄 Activando sensor de movimiento...
        </div>
        
        <div class="info">
            <p>🔌 <strong>FALLBAND_SETUP</strong> · 🔑 <strong>12345678</strong></p>
            <p>✨ El sensor usa el acelerómetro de tu teléfono ✨</p>
            <p>📧 Las alertas se enviarán por correo electrónico</p>
            <p style="margin-top: 10px;">📱 Gira tu teléfono para probar el sensor</p>
        </div>
    </div>
    
    <script>
        let ultimaAlerta = 0;
        let anguloAnterior = null;
        let correoUsuario = "";
        let telefonoUsuario = "";
        
        // Función para enviar correo con EmailJS
        async function enviarCorreoAlerta(tipo, datos) {
            if (!correoUsuario) {
                console.log("No hay correo registrado");
                return false;
            }
            
            const ahora = new Date();
            const fecha = ahora.toLocaleString('es-MX', { timeZone: 'America/Mexico_City' });
            
            const templateParams = {
                to_email: correoUsuario,
                to_name: correoUsuario.split('@')[0],
                user_telefono: telefonoUsuario,
                alerta_tipo: tipo,
                alerta_datos: datos,
                alerta_fecha: fecha,
                mensaje: "FALLBAND - Alerta por movimiento detectado en tu reloj",
                dispositivo: "FALLBAND CLOCK"
            };
            
            try {
                const response = await emailjs.send(
                    EMAILJS_SERVICE_ID,
                    EMAILJS_TEMPLATE_ID,
                    templateParams
                );
                console.log("Correo enviado:", response);
                return true;
            } catch (error) {
                console.error("Error al enviar correo:", error);
                return false;
            }
        }
        
        // Detectar si el dispositivo tiene acelerómetro
        if (window.DeviceOrientationEvent) {
            // Solicitar permiso en iOS
            if (typeof DeviceOrientationEvent.requestPermission === 'function') {
                document.getElementById('sensorStatus').innerHTML = '📱 Presiona para activar sensor';
                document.getElementById('sensorStatus').style.cursor = 'pointer';
                document.getElementById('sensorStatus').onclick = async () => {
                    const permission = await DeviceOrientationEvent.requestPermission();
                    if (permission === 'granted') {
                        window.addEventListener('deviceorientation', handleOrientation);
                        document.getElementById('sensorStatus').innerHTML = '✅ Sensor activo<br>📱 Gira tu teléfono para probar';
                        document.getElementById('sensorStatus').style.cursor = 'default';
                    }
                };
            } else {
                // Android y otros
                window.addEventListener('deviceorientation', handleOrientation);
                document.getElementById('sensorStatus').innerHTML = '✅ Sensor activo<br>📱 Gira tu teléfono para probar';
            }
        } else {
            document.getElementById('sensorStatus').innerHTML = '❌ Tu dispositivo no soporta sensor de movimiento';
        }
        
        function handleOrientation(event) {
            const beta = Math.round(event.beta);  // inclinación adelante/atrás
            const gamma = Math.round(event.gamma); // inclinación izquierda/derecha
            const alpha = Math.round(event.alpha); // rotación
            
            const sensorDiv = document.getElementById('sensorStatus');
            sensorDiv.innerHTML = `📊 Orientación: ${beta}° / ${gamma}°<br>🎯 Sensor activo | Alertas activadas`;
            
            // Detectar cambios bruscos de orientación
            if (anguloAnterior !== null) {
                const cambioBeta = Math.abs(beta - anguloAnterior.beta);
                const cambioGamma = Math.abs(gamma - anguloAnterior.gamma);
                
                // Si hay un cambio brusco (>25 grados)
                if (cambioBeta > 25 || cambioGamma > 25) {
                    const ahora = Date.now();
                    if (ahora - ultimaAlerta > 60000) { // 1 minuto entre alertas
                        ultimaAlerta = ahora;
                        
                        // Determinar tipo de movimiento
                        let tipo = "";
                        let datosDetalle = "";
                        
                        if (cambioBeta > 25 && cambioGamma > 25) {
                            tipo = "MOVIMIENTO_COMPLETO";
                            datosDetalle = `Inclinación vertical: ${cambioBeta}°, horizontal: ${cambioGamma}°`;
                        } else if (cambioBeta > 25) {
                            tipo = "INCLINACION_VERTICAL";
                            datosDetalle = `Cambio vertical de ${cambioBeta}° (${beta}° → ${anguloAnterior.beta}°)`;
                        } else if (cambioGamma > 25) {
                            tipo = "ROTACION_HORIZONTAL";
                            datosDetalle = `Rotación de ${cambioGamma}° (${gamma}° → ${anguloAnterior.gamma}°)`;
                        }
                        
                        // Enviar correo
                        enviarCorreoAlerta(tipo, datosDetalle);
                        
                        // Mostrar alerta visual
                        sensorDiv.classList.add('alert-animation');
                        sensorDiv.innerHTML = `⚠️ ¡MOVIMIENTO DETECTADO! ⚠️<br>${tipo}<br>📧 Enviando alerta por correo...`;
                        
                        setTimeout(() => {
                            sensorDiv.classList.remove('alert-animation');
                            sensorDiv.innerHTML = `📊 Orientación: ${beta}° / ${gamma}°<br>🎯 Sensor activo | Alertas activadas`;
                        }, 3000);
                    }
                }
            }
            
            anguloAnterior = { beta: beta, gamma: gamma, alpha: alpha };
        }
        
        document.getElementById('configForm').onsubmit = async function(e) {
            e.preventDefault();
            
            const ssid = document.getElementById('ssid').value.trim();
            const password = document.getElementById('password').value;
            const telefono = document.getElementById('telefono').value.trim();
            const correo = document.getElementById('correo').value.trim();
            
            if(!ssid) { showMessage('❖ Ingresa el nombre de la red WiFi', 'error'); return; }
            if(!telefono) { showMessage('❖ Ingresa tu número de teléfono', 'error'); return; }
            if(!correo) { showMessage('❖ Ingresa tu correo electrónico', 'error'); return; }
            
            // Guardar para usar en alertas
            correoUsuario = correo;
            telefonoUsuario = telefono;
            
            showMessage('⏳ Conectando y registrando...', 'success');
            
            try {
                const response = await fetch('/connect', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: 'ssid=' + encodeURIComponent(ssid) + 
                          '&password=' + encodeURIComponent(password) +
                          '&telefono=' + encodeURIComponent(telefono) +
                          '&correo=' + encodeURIComponent(correo)
                });
                
                const result = await response.text();
                
                if(result.includes('exitoso') || result.includes('Conectado')) {
                    showMessage('✓ ' + result, 'success');
                    
                    // Enviar correo de bienvenida
                    await enviarCorreoAlerta("REGISTRO_EXITOSO", "Tu reloj FALLBAND ha sido configurado correctamente");
                    
                    setTimeout(() => { window.location.reload(); }, 3000);
                } else {
                    showMessage('✗ ' + result, 'error');
                }
            } catch(err) {
                showMessage('✗ Error de conexión', 'error');
            }
        }
        
        function showMessage(msg, type) {
            const msgDiv = document.getElementById('message');
            msgDiv.textContent = msg;
            msgDiv.className = 'message ' + type;
            msgDiv.style.display = 'block';
            setTimeout(() => {
                if(msgDiv.style.display === 'block' && !msg.includes('Conectado')) {
                    msgDiv.style.opacity = '0';
                    setTimeout(() => { msgDiv.style.display = 'none'; msgDiv.style.opacity = '1'; }, 500);
                }
            }, 5000);
        }
    </script>
</body>
</html>
)rawliteral";
  return html;
}

// ─── Configurar servidor web ─────────────────────────────────────────────────
void configurarServidorConfig() {
  server.on("/", []() {
    server.send(200, "text/html", paginaConfiguracion());
  });
  
  server.on("/connect", HTTP_POST, []() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    String telefono = server.arg("telefono");
    String correo = server.arg("correo");
    
    if (ssid.length() > 0 && telefono.length() > 0 && correo.length() > 0) {
      server.send(200, "text/plain", "Conectando a " + ssid + "...");
      delay(500);
      
      preferences.begin("wifi", false);
      preferences.putString("ssid", ssid);
      preferences.putString("password", password);
      preferences.putString("telefono", telefono);
      preferences.putString("correo", correo);
      preferences.end();
      
      saved_ssid = ssid;
      saved_password = password;
      saved_telefono = telefono;
      saved_correo = correo;
      
      WiFi.begin(ssid.c_str(), password.c_str());
      int intentos = 0;
      while (WiFi.status() != WL_CONNECTED && intentos < 25) {
        delay(500);
        intentos++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        wifiConfigurado = true;
        modoConfiguracion = false;
      } else {
        server.send(200, "text/plain", "Error: No se pudo conectar");
        return;
      }
    } else {
      server.send(200, "text/plain", "Error: Campos obligatorios");
    }
  });
  
  server.begin();
}

// ─── Activar modo configuración ──────────────────────────────────────────────
void activarModoConfiguracion() {
  if (modoConfiguracion) return;
  
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  modoConfiguracion = true;
  tiempoActivacionConfig = millis();
  
  configurarServidorConfig();
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(tft.color565(212,175,55), TFT_BLACK);
  tft.drawString("MODO CONFIGURACION", SW/2, SH/2 - 40);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("SSID: " + String(ap_ssid), SW/2, SH/2 - 15);
  tft.drawString("Pass: " + String(ap_password), SW/2, SH/2);
  tft.drawString("IP: 192.168.4.1", SW/2, SH/2 + 15);
  tft.setTextColor(tft.color565(255, 200, 0), TFT_BLACK);
  tft.drawString("Conectate y abre", SW/2, SH/2 + 45);
  tft.drawString("navegador web", SW/2, SH/2 + 60);
}

// ─── Conectar a WiFi guardado ────────────────────────────────────────────────
bool conectarWiFiGuardado() {
  preferences.begin("wifi", false);
  saved_ssid = preferences.getString("ssid", "");
  saved_password = preferences.getString("password", "");
  saved_telefono = preferences.getString("telefono", "");
  saved_correo = preferences.getString("correo", "");
  preferences.end();
  
  if (saved_ssid.length() > 0) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(paletas[estiloActual].acento, TFT_BLACK);
    tft.drawString("CARGANDO...", SW/2, SH/2 - 20);
    tft.drawString("Conectando a WiFi", SW/2, SH/2);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(saved_ssid.c_str(), saved_password.c_str());
    
    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 25) {
      delay(500);
      intentos++;
      tft.fillRect(SW/2 - 30, SH/2 + 25, (intentos * 3), 4, tft.color565(212,175,55));
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      wifiConfigurado = true;
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(tft.color565(60, 165, 80), TFT_BLACK);
      tft.drawString("WiFi Conectado!", SW/2, SH/2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(WiFi.localIP().toString(), SW/2, SH/2 + 15);
      tft.setTextColor(tft.color565(212,175,55), TFT_BLACK);
      tft.drawString("Alertas por Email", SW/2, SH/2 + 35);
      delay(1500);
      return true;
    }
  }
  return false;
}

// ─── Pantalla de inicio ───────────────────────────────────────────────────────
void pantallaCarga() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(paletas[estiloActual].acento, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("FALLBAND", SW/2, SH/2 - 10);
  tft.setTextSize(1);
  tft.setTextColor(tft.color565(175, 162, 128), TFT_BLACK);
  tft.drawString("Alertas por correo", SW/2, SH/2 + 15);
  tft.drawString("vía EmailJS", SW/2, SH/2 + 30);
  delay(2000);
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  
  inicializarPaletas();
  pantallaCarga();
  
  if (!conectarWiFiGuardado()) {
    activarModoConfiguracion();
  } else {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  }
  
  tft.fillScreen(TFT_BLACK);
  dibujarCaja();
  dibujarEsferaCompleta();
}

void loop() {
  if (modoConfiguracion) {
    server.handleClient();
    if (millis() - tiempoActivacionConfig > tiempoConfigActivo) ESP.restart();
    if (wifiConfigurado && WiFi.status() == WL_CONNECTED) {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      modoConfiguracion = false;
      tft.fillScreen(TFT_BLACK);
      dibujarCaja();
      dibujarEsferaCompleta();
      segundoAnterior = -1;
    }
  }
  
  // Botón izquierdo - Cambiar estilo
  int estadoBotonIzq = digitalRead(BTN_LEFT);
  if (estadoBotonIzq == LOW && !botonPresionado) {
    if (millis() - ultimoCambio > tiempoMinimoEntreCambios) {
      botonPresionado = true;
      ultimoCambio = millis();
      estiloActual = (estiloActual + 1) % TOTAL_ESTILOS;
      tft.fillScreen(TFT_BLACK);
      dibujarCaja();
      dibujarEsferaCompleta();
      segundoAnterior = -1;
    }
  }
  if (estadoBotonIzq == HIGH && botonPresionado) botonPresionado = false;
  
  // Botón derecho - Forzar modo configuración
  static bool botonDerechoPresionado = false;
  int estadoBotonDer = digitalRead(BTN_RIGHT);
  if (estadoBotonDer == LOW && !botonDerechoPresionado) {
    botonDerechoPresionado = true;
    if (!modoConfiguracion) activarModoConfiguracion();
  }
  if (estadoBotonDer == HIGH && botonDerechoPresionado) botonDerechoPresionado = false;
  
  // Actualizar reloj
  if (!modoConfiguracion && millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    struct tm t;
    if (getLocalTime(&t)) actualizarManecillas(t);
  }
}
