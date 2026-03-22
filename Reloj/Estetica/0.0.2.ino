/*
 * Reloj Analógico con Configuración WiFi y Registro Automático
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
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ─── Configuración ───────────────────────────────────────────────────────────
const char* ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = -21600;
const int   daylightOffset_sec = 3600;

// ─── Punto de Acceso para configuración ──────────────────────────────────────
const char* ap_ssid     = "FALLBAND_SETUP";
const char* ap_password = "12345678";  // Contraseña del 1 al 8
WebServer server(80);
Preferences preferences;

bool modoConfiguracion = false;
bool wifiConfigurado = false;
bool registroExitoso = false;
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

// ─── Funciones de dibujo del reloj (mismas que antes) ────────────────────────
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

// ─── Función para enviar datos a la API ──────────────────────────────────────
bool enviarRegistroAPI(String telefono, String correo) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No hay conexión WiFi para enviar registro");
    return false;
  }
  
  HTTPClient http;
  http.begin("https://69c01cab72ca04f3bcba8d84.mockapi.io/api/Registros/FALLBAD");
  http.addHeader("Content-Type", "application/json");
  
  // Crear JSON con los datos
  StaticJsonDocument<200> doc;
  doc["telefono"] = telefono;
  doc["correo"] = correo;
  doc["fecha_registro"] = time(nullptr);
  doc["dispositivo"] = "FALLBAND_CLOCK";
  
  String jsonData;
  serializeJson(doc, jsonData);
  
  Serial.println("Enviando a API: " + jsonData);
  
  int httpCode = http.POST(jsonData);
  bool exito = false;
  
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
      Serial.println("Registro enviado exitosamente");
      exito = true;
    } else {
      Serial.printf("Error en API: %d\n", httpCode);
    }
  } else {
    Serial.printf("Error en conexión HTTP: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
  return exito;
}

// ─── Página web de configuración con 4 campos ─────────────────────────────────
String paginaConfiguracion() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FALLBAND - Configuración</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            background: rgba(255, 255, 255, 0.95);
            border-radius: 20px;
            padding: 30px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            max-width: 450px;
            width: 100%;
            text-align: center;
        }
        h1 {
            color: #667eea;
            margin-bottom: 10px;
            font-size: 1.8em;
        }
        .logo {
            font-size: 3em;
            margin-bottom: 10px;
        }
        .subtitle {
            color: #666;
            margin-bottom: 25px;
            font-size: 0.9em;
        }
        .form-group {
            margin: 18px 0;
            text-align: left;
        }
        label {
            display: block;
            margin-bottom: 5px;
            color: #333;
            font-weight: bold;
            font-size: 0.9em;
        }
        input {
            width: 100%;
            padding: 12px;
            border: 2px solid #ddd;
            border-radius: 10px;
            font-size: 14px;
            transition: border-color 0.3s;
        }
        input:focus {
            outline: none;
            border-color: #667eea;
        }
        button {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            padding: 12px 30px;
            font-size: 1.1em;
            border-radius: 25px;
            cursor: pointer;
            width: 100%;
            margin-top: 20px;
            transition: transform 0.2s;
        }
        button:hover {
            transform: scale(1.02);
        }
        .info {
            margin-top: 20px;
            font-size: 0.7em;
            color: #666;
        }
        .message {
            margin-top: 15px;
            padding: 10px;
            border-radius: 8px;
            display: none;
            font-size: 0.9em;
        }
        .message.success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .message.error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .required {
            color: red;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">🕐</div>
        <h1>FALLBAND CLOCK</h1>
        <div class="subtitle">Configura tu dispositivo</div>
        
        <form id="configForm">
            <div class="form-group">
                <label>📶 Nombre de la red WiFi <span class="required">*</span></label>
                <input type="text" id="ssid" required placeholder="Ej: MiWiFi_2.4G">
            </div>
            <div class="form-group">
                <label>🔐 Contraseña WiFi</label>
                <input type="password" id="password" placeholder="Contraseña de la red">
            </div>
            <div class="form-group">
                <label>📱 Número de teléfono <span class="required">*</span></label>
                <input type="tel" id="telefono" required placeholder="Ej: 5512345678">
            </div>
            <div class="form-group">
                <label>📧 Correo electrónico <span class="required">*</span></label>
                <input type="email" id="correo" required placeholder="tu@email.com">
            </div>
            <button type="submit">Registrar y Conectar</button>
        </form>
        
        <div id="message" class="message"></div>
        
        <div class="info">
            <p>🔌 Red de configuración: <strong>FALLBAND_SETUP</strong></p>
            <p>🔑 Contraseña: <strong>12345678</strong></p>
            <p>⚠️ Solo redes 2.4GHz</p>
        </div>
    </div>
    
    <script>
        document.getElementById('configForm').onsubmit = async function(e) {
            e.preventDefault();
            
            const ssid = document.getElementById('ssid').value.trim();
            const password = document.getElementById('password').value;
            const telefono = document.getElementById('telefono').value.trim();
            const correo = document.getElementById('correo').value.trim();
            
            if(!ssid) {
                showMessage('Por favor ingresa el nombre de la red WiFi', 'error');
                return;
            }
            if(!telefono) {
                showMessage('Por favor ingresa tu número de teléfono', 'error');
                return;
            }
            if(!correo) {
                showMessage('Por favor ingresa tu correo electrónico', 'error');
                return;
            }
            
            showMessage('Conectando y registrando...', 'success');
            
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
                    showMessage('✅ ' + result, 'success');
                    setTimeout(() => {
                        window.location.reload();
                    }, 3000);
                } else {
                    showMessage('❌ ' + result, 'error');
                }
            } catch(err) {
                showMessage('Error de conexión', 'error');
            }
        }
        
        function showMessage(msg, type) {
            const msgDiv = document.getElementById('message');
            msgDiv.textContent = msg;
            msgDiv.className = 'message ' + type;
            msgDiv.style.display = 'block';
        }
    </script>
</body>
</html>
)rawliteral";
  return html;
}

// ─── Configurar servidor web de configuración ─────────────────────────────────
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
      
      // Guardar credenciales WiFi
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
      
      // Intentar conectar a WiFi
      WiFi.begin(ssid.c_str(), password.c_str());
      int intentos = 0;
      while (WiFi.status() != WL_CONNECTED && intentos < 25) {
        delay(500);
        intentos++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        // Enviar registro a la API
        if (enviarRegistroAPI(telefono, correo)) {
          wifiConfigurado = true;
          registroExitoso = true;
          modoConfiguracion = false;
        } else {
          // Si falla la API pero el WiFi funciona, igual guardamos
          wifiConfigurado = true;
          modoConfiguracion = false;
        }
      } else {
        server.send(200, "text/plain", "Error: No se pudo conectar a la red WiFi. Verifica los datos.");
        return;
      }
    } else {
      server.send(200, "text/plain", "Error: Todos los campos son obligatorios");
    }
  });
  
  server.begin();
}

// ─── Activar modo configuración ──────────────────────────────────────────────
void activarModoConfiguracion() {
  if (modoConfiguracion) return;
  
  Serial.println("Activando modo configuración WiFi...");
  
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  modoConfiguracion = true;
  tiempoActivacionConfig = millis();
  
  configurarServidorConfig();
  
  // Mostrar pantalla de configuración
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(tft.color565(0, 255, 0), TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("CONFIGURACION WiFi", SW/2, SH/2 - 40);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("SSID: " + String(ap_ssid), SW/2, SH/2 - 15);
  tft.drawString("Pass: " + String(ap_password), SW/2, SH/2);
  tft.drawString("IP: 192.168.4.1", SW/2, SH/2 + 15);
  tft.setTextColor(tft.color565(255, 200, 0), TFT_BLACK);
  tft.drawString("Conectate a la red", SW/2, SH/2 + 45);
  tft.drawString("y abre un navegador", SW/2, SH/2 + 60);
  
  Serial.println("Modo configuración activado");
  Serial.print("SSID: ");
  Serial.println(ap_ssid);
  Serial.print("Contraseña: ");
  Serial.println(ap_password);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
}

// ─── Conectar a WiFi guardado y enviar registro pendiente ────────────────────
bool conectarWiFiGuardado() {
  preferences.begin("wifi", false);
  saved_ssid = preferences.getString("ssid", "");
  saved_password = preferences.getString("password", "");
  saved_telefono = preferences.getString("telefono", "");
  saved_correo = preferences.getString("correo", "");
  preferences.end();
  
  if (saved_ssid.length() > 0) {
    // Pantalla de carga
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(paletas[estiloActual].acento, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("CARGANDO...", SW/2, SH/2 - 20);
    tft.drawString("Conectando a WiFi", SW/2, SH/2);
    tft.drawString(saved_ssid, SW/2, SH/2 + 15);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(saved_ssid.c_str(), saved_password.c_str());
    
    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 25) {
      delay(500);
      intentos++;
      // Animación de carga
      tft.fillRect(SW/2 - 30, SH/2 + 35, (intentos * 3), 4, TFT_GREEN);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      wifiConfigurado = true;
      
      // Verificar si ya se envió el registro
      bool registroEnviado = preferences.getBool("registro_enviado", false);
      
      if (!registroEnviado && saved_telefono.length() > 0 && saved_correo.length() > 0) {
        tft.drawString("Enviando registro...", SW/2, SH/2 + 45);
        if (enviarRegistroAPI(saved_telefono, saved_correo)) {
          preferences.begin("wifi", false);
          preferences.putBool("registro_enviado", true);
          preferences.end();
          registroExitoso = true;
          tft.drawString("Registro exitoso!", SW/2, SH/2 + 60);
        }
      }
      
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(tft.color565(60, 165, 80), TFT_BLACK);
      tft.drawString("WiFi Conectado!", SW/2, SH/2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(WiFi.localIP().toString(), SW/2, SH/2 + 15);
      delay(1500);
      return true;
    } else {
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("No se pudo conectar", SW/2, SH/2 - 10);
      tft.drawString("Presiona botón derecho", SW/2, SH/2 + 10);
      tft.drawString("para configurar", SW/2, SH/2 + 25);
      delay(3000);
      return false;
    }
  }
  return false;
}

// ─── Pantalla de inicio con animación de carga ───────────────────────────────
void pantallaCarga() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  
  // Logo animado
  for(int i = 0; i < 3; i++) {
    tft.setTextColor(paletas[estiloActual].acento, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("FALLBAND", SW/2, SH/2 - 20);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(175, 162, 128), TFT_BLACK);
    tft.drawString("R E V E R S O", SW/2, SH/2);
    
    // Barra de carga
    for(int j = 0; j <= SW - 40; j += 5) {
      tft.drawRect(20, SH/2 + 30, SW - 40, 8, TFT_WHITE);
      tft.fillRect(22, SH/2 + 32, j, 4, tft.color565(100, 200, 255));
      delay(20);
    }
    delay(500);
    tft.fillScreen(TFT_BLACK);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando TTGO T-Display...");
  
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  
  inicializarPaletas();
  pantallaCarga();
  
  // Intentar conectar a WiFi guardado
  if (!conectarWiFiGuardado()) {
    // Si no hay WiFi o falla, activar modo configuración automáticamente
    activarModoConfiguracion();
  } else {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  }
  
  tft.fillScreen(TFT_BLACK);
  dibujarCaja();
  dibujarEsferaCompleta();
  Serial.println("Setup completado");
}

void loop() {
  // Manejar servidor web si está en modo configuración
  if (modoConfiguracion) {
    server.handleClient();
    
    // Verificar tiempo de actividad del modo configuración
    if (millis() - tiempoActivacionConfig > tiempoConfigActivo) {
      ESP.restart();
    }
    
    // Verificar si ya se configuró WiFi
    if (wifiConfigurado && WiFi.status() == WL_CONNECTED) {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      modoConfiguracion = false;
      
      tft.fillScreen(TFT_BLACK);
      dibujarCaja();
      dibujarEsferaCompleta();
      segundoAnterior = -1;
    }
  }
  
  // Leer estado del botón izquierdo (cambiar estilo)
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
    if (!modoConfiguracion) {
      activarModoConfiguracion();
    }
  }
  if (estadoBotonDer == HIGH && botonDerechoPresionado) botonDerechoPresionado = false;
  
  // Actualizar reloj
  if (!modoConfiguracion && millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    struct tm t;
    if (getLocalTime(&t)) {
      actualizarManecillas(t);
    }
  }
}
