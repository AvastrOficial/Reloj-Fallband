/*
 * Reloj Analógico con Múltiples Estilos - CORREGIDO
 * TTGO T-Display ESP32 - 135x240 px - Orientación Vertical
 * Botón izquierdo (GPIO0): Cambia estilo de reloj
 */

#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include <math.h>

// ─── WiFi / NTP ───────────────────────────────────────────────────────────────
const char* ssid               = "INFINITUM09B3";
const char* password           = "cx3FWDATq7";
const char* ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = -21600;
const int   daylightOffset_sec = 3600;

TFT_eSPI tft = TFT_eSPI();

// ─── Dimensiones pantalla ─────────────────────────────────────────────────────
#define SW  135
#define SH  240

// ─── Botones TTGO T-Display ─────────────────────────────────────────────────
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

// ─── Caja ─────────────────────────────────────────────────────────────────────
#define CASE_X  1
#define CASE_Y  1
#define CASE_W  (SW - 2)
#define CASE_H  (SH - 2)

// ─── Esfera ───────────────────────────────────────────────────────────────────
#define DIAL_PAD  6
#define DIAL_X    (CASE_X + DIAL_PAD)
#define DIAL_Y    (CASE_Y + DIAL_PAD)
#define DIAL_W    (CASE_W - DIAL_PAD * 2)
#define DIAL_H    (CASE_H - DIAL_PAD * 2)

// Centro exacto de la esfera
#define CX  (DIAL_X + DIAL_W / 2)
#define CY  (DIAL_Y + DIAL_H / 2)

// Radio de trabajo
#define R   (DIAL_W / 2 - 3)

// Longitudes de manecillas
#define LEN_HOUR  (R - 12)
#define LEN_MIN   (R - 4)
#define LEN_SEC   (R - 1)
#define LEN_TAIL  13

// ─── Colores dinámicos según estilo ──────────────────────────────────────────
struct PaletaColores {
  uint16_t fondo;
  uint16_t marcadores;
  uint16_t numeros;
  uint16_t manecillas;
  uint16_t segundero;
  uint16_t acento;
  uint16_t textoMarca;
  uint16_t caja;
};

PaletaColores paletas[TOTAL_ESTILOS];

void inicializarPaletas() {
  // Estilo Clásico
  paletas[ESTILO_CLASICO] = {
    tft.color565(250, 248, 240),  // fondo marfil
    tft.color565(8, 10, 28),      // marcadores negro
    tft.color565(8, 10, 28),      // números negro
    tft.color565(20, 58, 195),    // manecillas azul
    tft.color565(205, 22, 12),    // segundero rojo
    tft.color565(208, 158, 78),   // acento dorado
    tft.color565(80, 72, 52),     // texto marca
    tft.color565(208, 158, 78)    // caja dorada
  };
  
  // Estilo Moderno
  paletas[ESTILO_MODERNO] = {
    tft.color565(20, 25, 45),     // fondo azul oscuro
    tft.color565(0, 255, 255),    // marcadores cian
    tft.color565(255, 255, 255),  // números blanco
    tft.color565(0, 200, 255),    // manecillas azul eléctrico
    tft.color565(255, 100, 100),  // segundero rojo claro
    tft.color565(0, 255, 200),    // acento turquesa
    tft.color565(150, 200, 255),  // texto marca
    tft.color565(100, 100, 120)   // caja gris azulada
  };
  
  // Estilo Deportivo
  paletas[ESTILO_DEPORTIVO] = {
    tft.color565(30, 30, 30),     // fondo negro/gris
    tft.color565(255, 60, 60),    // marcadores rojo
    tft.color565(255, 255, 255),  // números blanco
    tft.color565(255, 80, 80),    // manecillas rojo
    tft.color565(255, 200, 0),    // segundero amarillo
    tft.color565(255, 60, 60),    // acento rojo
    tft.color565(255, 100, 100),  // texto marca
    tft.color565(80, 80, 80)      // caja negra
  };
  
  // Estilo Minimalista
  paletas[ESTILO_MINIMALISTA] = {
    tft.color565(255, 255, 255),  // fondo blanco
    tft.color565(0, 0, 0),        // marcadores negro
    tft.color565(0, 0, 0),        // números negro
    tft.color565(80, 80, 80),     // manecillas gris oscuro
    tft.color565(120, 120, 120),  // segundero gris
    tft.color565(200, 200, 200),  // acento gris claro
    tft.color565(100, 100, 100),  // texto marca
    tft.color565(180, 180, 180)   // caja plateada
  };
  
  // Estilo Vintage
  paletas[ESTILO_VINTAGE] = {
    tft.color565(245, 222, 179),  // fondo beige
    tft.color565(101, 67, 33),    // marcadores marrón
    tft.color565(101, 67, 33),    // números marrón
    tft.color565(139, 69, 19),    // manecillas marrón
    tft.color565(205, 133, 63),   // segundero naranja
    tft.color565(184, 115, 51),   // acento cobre
    tft.color565(160, 100, 50),   // texto marca
    tft.color565(184, 134, 11)    // caja bronce
  };
}

// ─── Estado ───────────────────────────────────────────────────────────────────
unsigned long lastUpdate = 0;
int segundoAnterior = -1;

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
  pantallaInicio();
  conectarWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(2000);

  tft.fillScreen(TFT_BLACK);
  dibujarCaja();
  dibujarEsferaCompleta();
  Serial.println("Setup completado");
}

void loop() {
  // Leer estado del botón izquierdo
  int estadoBoton = digitalRead(BTN_LEFT);
  
  // Detectar flanco descendente (botón presionado)
  if (estadoBoton == LOW && !botonPresionado) {
    if (millis() - ultimoCambio > tiempoMinimoEntreCambios) {
      botonPresionado = true;
      ultimoCambio = millis();
      
      // Cambiar estilo
      estiloActual = (estiloActual + 1) % TOTAL_ESTILOS;
      Serial.print("Cambiando a estilo: ");
      Serial.println(estiloActual);
      
      // Redibujar completamente el reloj con el nuevo estilo
      tft.fillScreen(TFT_BLACK);
      dibujarCaja();
      dibujarEsferaCompleta();
      segundoAnterior = -1; // Forzar actualización
    }
  }
  
  // Detectar liberación del botón
  if (estadoBoton == HIGH && botonPresionado) {
    botonPresionado = false;
  }
  
  // Actualizar reloj cada segundo
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    
    struct tm t;
    if (getLocalTime(&t)) {
      actualizarManecillas(t);
    } else {
      Serial.println("Error obteniendo hora");
    }
  }
}

// ─── Actualizar solo las manecillas ─────────────────────────────────────────
void actualizarManecillas(struct tm &t) {
  int H = t.tm_hour % 12;
  int M = t.tm_min;
  int S = t.tm_sec;
  
  // Solo redibujar si cambió el segundo
  if (S == segundoAnterior) return;
  segundoAnterior = S;
  
  PaletaColores pal = paletas[estiloActual];
  
  // PRIMERO: Redibujar toda la esfera estática (esto borra todas las manecillas)
  dibujarEsferaEstatica();
  
  // SEGUNDO: Dibujar las nuevas manecillas sobre la esfera limpia
  
  // Manecilla de horas
  float angH = H * 30.0f + M * 0.5f;
  dibujarEspada(angH, LEN_HOUR, 5, pal.manecillas);
  
  // Manecilla de minutos
  float angM = M * 6.0f;
  dibujarEspada(angM, LEN_MIN, 4, pal.manecillas);
  
  // Segundero (línea simple)
  float sRad = (S * 6.0f - 90.0f) * M_PI / 180.0f;
  int stX = CX + (int)(LEN_SEC * cosf(sRad));
  int stY = CY + (int)(LEN_SEC * sinf(sRad));
  int scX = CX - (int)(LEN_TAIL * cosf(sRad));
  int scY = CY - (int)(LEN_TAIL * sinf(sRad));
  tft.drawLine(scX, scY, stX, stY, pal.segundero);
  tft.drawLine(scX + 1, scY, stX + 1, stY, pal.segundero);
  
  // Centro
  tft.fillCircle(CX, CY, 5, pal.acento);
  tft.fillCircle(CX, CY, 2, pal.manecillas);
  
  // Fecha
  int winX = CX - 16;
  int winY = CY + 10;
  tft.drawRect(winX, winY, 32, 13, pal.acento);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(pal.numeros, pal.fondo);
  char dia[3];
  sprintf(dia, "%02d", t.tm_mday);
  tft.drawString(dia, CX, winY + 6);
  
  // Hora digital
  int horaZoneY = DIAL_Y + DIAL_H - 22;
  tft.setTextColor(pal.manecillas, pal.fondo);
  char horaStr[9];
  sprintf(horaStr, "%02d:%02d:%02d", t.tm_hour, M, S);
  tft.drawString(horaStr, CX, horaZoneY + 6);
  
  const char* dias[] = {"DOM","LUN","MAR","MIE","JUE","VIE","SAB"};
  tft.setTextColor(pal.acento, pal.fondo);
  tft.drawString(dias[t.tm_wday], CX, horaZoneY + 15);
}

// ─── Dibuja esfera completa (incluyendo texto y números) ─────────────────────
void dibujarEsferaCompleta() {
  dibujarEsferaEstatica();
  
  // Añadir fecha y hora digital después de la esfera
  struct tm t;
  if (getLocalTime(&t)) {
    PaletaColores pal = paletas[estiloActual];
    
    int winX = CX - 16;
    int winY = CY + 10;
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

// ─── Dibuja solo la parte estática de la esfera (sin manecillas) ─────────────
void dibujarEsferaEstatica() {
  PaletaColores pal = paletas[estiloActual];
  
  // Fondo
  tft.fillRect(DIAL_X, DIAL_Y, DIAL_W, DIAL_H, pal.fondo);
  
  // Borde interior
  tft.drawRect(DIAL_X, DIAL_Y, DIAL_W, DIAL_H, pal.marcadores);
  tft.drawRect(DIAL_X + 1, DIAL_Y + 1, DIAL_W - 2, DIAL_H - 2, pal.acento);
  
  // Marcas de minuto (puntos)
  for (int i = 0; i < 60; i++) {
    if (i % 5 == 0) continue;
    float a = i * 6.0f;
    tft.fillCircle(angX(a, R), angY(a, R), 1, pal.marcadores);
  }
  
  // Marcas de hora (barras)
  for (int i = 1; i <= 12; i++) {
    float a = i * 30.0f;
    int x1 = angX(a, R - 8);
    int y1 = angY(a, R - 8);
    int x2 = angX(a, R);
    int y2 = angY(a, R);
    tft.drawLine(x1, y1, x2, y2, pal.marcadores);
    tft.drawLine(x1 + 1, y1, x2 + 1, y2, pal.marcadores);
  }
  
  // Números
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(pal.numeros, pal.fondo);
  
  // Números grandes (12,3,6,9)
  tft.setTextSize(2);
  int bn[] = {12, 3, 6, 9};
  for (int n : bn) {
    float a = n * 30.0f;
    int nx = angX(a, R - 17);
    int ny = angY(a, R - 17);
    tft.fillRect(nx - 9, ny - 8, 18, 16, pal.fondo);
    char buf[3];
    sprintf(buf, "%d", n);
    tft.drawString(buf, nx, ny);
  }
  
  // Números pequeños
  tft.setTextSize(1);
  int sn[] = {1, 2, 4, 5, 7, 8, 10, 11};
  for (int n : sn) {
    float a = n * 30.0f;
    int nx = angX(a, R - 13);
    int ny = angY(a, R - 13);
    tft.fillRect(nx - 6, ny - 4, 14, 9, pal.fondo);
    char buf[3];
    sprintf(buf, "%d", n);
    tft.drawString(buf, nx, ny);
  }
  
  // Texto FALLBAND arriba
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  int marcaY = DIAL_Y + 12;
  tft.fillRect(CX - 35, marcaY - 5, 70, 12, pal.fondo);
  tft.setTextColor(pal.textoMarca, pal.fondo);
  tft.drawString("FALLBAND", CX, marcaY);
  tft.drawFastHLine(CX - 28, marcaY + 3, 56, pal.acento);
}

// ─── Pantalla de inicio ───────────────────────────────────────────────────────
void pantallaInicio() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(paletas[estiloActual].acento, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("FALLBAND", SW / 2, SH / 2 - 12);
  tft.drawFastHLine(22, SH / 2, 91, paletas[estiloActual].acento);
  tft.setTextColor(tft.color565(175, 162, 128), TFT_BLACK);
  tft.drawString("R E V E R S O", SW / 2, SH / 2 + 14);
  delay(1500);
}

// ─── WiFi ─────────────────────────────────────────────────────────────────────
void conectarWiFi() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(paletas[estiloActual].acento, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("WiFi...", SW / 2, SH / 2);
  WiFi.begin(ssid, password);
  
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    intentos++;
    Serial.print(".");
  }
  Serial.println();
  
  tft.fillScreen(TFT_BLACK);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi conectado");
    tft.setTextColor(tft.color565(60, 165, 80), TFT_BLACK);
    tft.drawString("WiFi OK", SW / 2, SH / 2);
  } else {
    Serial.println("WiFi falló");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Sin WiFi", SW / 2, SH / 2);
  }
  delay(800);
}

// ─── Dibuja caja según estilo ─────────────────────────────────────────────────
void dibujarCaja() {
  PaletaColores pal = paletas[estiloActual];
  
  // Sombra
  tft.fillRoundRect(CASE_X + 2, CASE_Y + 2, CASE_W, CASE_H, 3,
                    tft.color565(55, 38, 8));
  
  // Cuerpo
  tft.fillRoundRect(CASE_X, CASE_Y, CASE_W, CASE_H, 3, pal.caja);
  
  // Biseles
  tft.drawRoundRect(CASE_X, CASE_Y, CASE_W, CASE_H, 3, tft.color565(128, 88, 28));
  tft.drawRoundRect(CASE_X + 1, CASE_Y + 1, CASE_W - 2, CASE_H - 2, 2, tft.color565(242, 202, 132));
}

// ─── Manecilla tipo espada ──────────────────────────────────────────────────────
void dibujarEspada(float angDeg, int largo, int grosorMax, uint16_t color) {
  float rad = (angDeg - 90.0f) * M_PI / 180.0f;
  float dx = cosf(rad);
  float dy = sinf(rad);
  float px_ = -dy;
  float py_ = dx;
  
  int total = largo + LEN_TAIL;
  for (int i = 0; i <= total; i++) {
    float t = (float)i / total;
    float pos = (float)(i - LEN_TAIL) / largo;
    
    float w;
    if (pos < 0.0f) {
      w = grosorMax * 0.30f;
    } else if (pos < 0.25f) {
      w = grosorMax * (pos / 0.25f);
    } else {
      w = grosorMax * (1.0f - (pos - 0.25f) / 0.75f);
    }
    if (w < 0.5f) w = 0.5f;
    
    int bx = CX + (int)((-LEN_TAIL + (largo + LEN_TAIL) * t) * dx);
    int by = CY + (int)((-LEN_TAIL + (largo + LEN_TAIL) * t) * dy);
    
    int ax = (int)(w * px_);
    int ay = (int)(w * py_);
    tft.drawLine(bx - ax, by - ay, bx + ax, by + ay, color);
  }
}

// Helpers
inline int angX(float deg, int r) {
  return CX + (int)(r * sinf(deg * M_PI / 180.0f));
}

inline int angY(float deg, int r) {
  return CY - (int)(r * cosf(deg * M_PI / 180.0f));
}
