
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include <math.h>

// ─── WiFi / NTP ───────────────────────────────────────────────────────────────
const char* ssid               = "TU WIFI";
const char* password           = "PASSWORD WIFI";
const char* ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = -21600;
const int   daylightOffset_sec = 3600;

TFT_eSPI tft = TFT_eSPI();

// ─── Dimensiones pantalla ─────────────────────────────────────────────────────
#define SW  135
#define SH  240

// ─── Caja dorada: margen mínimo para que se vea el borde ─────────────────────
#define CASE_X  1
#define CASE_Y  1
#define CASE_W  (SW - 2)     // 133
#define CASE_H  (SH - 2)     // 238

// ─── Esfera blanca interior ───────────────────────────────────────────────────
#define DIAL_PAD  6
#define DIAL_X    (CASE_X + DIAL_PAD)       //  7
#define DIAL_Y    (CASE_Y + DIAL_PAD)       //  7
#define DIAL_W    (CASE_W - DIAL_PAD * 2)   // 121
#define DIAL_H    (CASE_H - DIAL_PAD * 2)   // 226

// Centro exacto de la esfera
#define CX  (DIAL_X + DIAL_W / 2)    // 67
#define CY  (DIAL_Y + DIAL_H / 2)    // 120

// Radio de trabajo: usamos el semi-eje menor (horizontal) para
// que las marcas y números no salgan de la esfera
#define R   (DIAL_W / 2 - 3)    // ~57  (cabe en ambas dimensiones)

// Longitudes de manecillas
#define LEN_HOUR  (R - 12)   // 45
#define LEN_MIN   (R - 4)    // 53
#define LEN_SEC   (R - 1)    // 56
#define LEN_TAIL  13

// ─── Colores (definidos en setup) ────────────────────────────────────────────
uint16_t C_GOLD, C_GOLD_DK, C_GOLD_LT;
uint16_t C_DIAL, C_DIAL_LN;
uint16_t C_INK, C_BRAND, C_SWISS;
uint16_t C_BLUE, C_BLUE_DK;
uint16_t C_RED;
uint16_t C_DOT_GOLD;
uint16_t C_MK_H, C_MK_M;
uint16_t C_DIGI, C_DATE_C;
uint16_t C_WIN_BG, C_WIN_BD;

void initColores() {
  C_GOLD     = tft.color565(208, 158,  78);
  C_GOLD_DK  = tft.color565(128,  88,  28);
  C_GOLD_LT  = tft.color565(242, 202, 132);
  C_DIAL     = tft.color565(250, 248, 240);
  C_DIAL_LN  = tft.color565(232, 228, 214);
  C_INK      = tft.color565(  8,  10,  28);
  C_BRAND    = tft.color565( 80,  72,  52);
  C_SWISS    = tft.color565(140, 130, 110);
  C_BLUE     = tft.color565( 20,  58, 195);
  C_BLUE_DK  = tft.color565(  8,  22,  88);
  C_RED      = tft.color565(205,  22,  12);
  C_DOT_GOLD = tft.color565(218, 172,  52);
  C_MK_H    = tft.color565(  8,  10,  28);
  C_MK_M    = tft.color565(158, 150, 135);
  C_DIGI    = tft.color565( 60,  78, 155);
  C_DATE_C  = tft.color565(168, 152, 105);
  C_WIN_BG  = tft.color565(238, 232, 218);
  C_WIN_BD  = tft.color565(168, 158, 138);
}

// ─── Helpers angulares ────────────────────────────────────────────────────────
// angDeg: 0=arriba(12), 90=derecha(3), 180=abajo(6), 270=izquierda(9)
inline int angX(float deg, int r) {
  return CX + (int)(r * sinf(deg * M_PI / 180.0f));
}
inline int angY(float deg, int r) {
  return CY - (int)(r * cosf(deg * M_PI / 180.0f));
}

// ─── Estado ───────────────────────────────────────────────────────────────────
unsigned long lastUpdate = 0;

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  initColores();
  pantallaInicio();
  conectarWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(2000);

  tft.fillScreen(TFT_BLACK);
  dibujarCaja();
  dibujarEsfera();
}

void loop() {
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    actualizarReloj();
  }
}

// ─── Pantalla de inicio ───────────────────────────────────────────────────────
void pantallaInicio() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_GOLD, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("Fallband", SW / 2, SH / 2 - 12);  // Cambiado de Jaeger-LeCoultre a Fallband
  tft.drawFastHLine(22, SH / 2, 91, C_GOLD);
  tft.setTextColor(tft.color565(175, 162, 128), TFT_BLACK);
  tft.drawString("R E V E R S O", SW / 2, SH / 2 + 14);
  delay(1500);
  tft.fillScreen(TFT_BLACK);
}

// ─── WiFi ─────────────────────────────────────────────────────────────────────
void conectarWiFi() {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_GOLD, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("WiFi...", SW / 2, SH / 2);
  WiFi.begin(ssid, password);
  for (int i = 0; i < 24; i++) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(500);
  }
  tft.fillScreen(TFT_BLACK);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(tft.color565(60, 165, 80), TFT_BLACK);
    tft.drawString("WiFi OK", SW / 2, SH / 2);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Sin WiFi", SW / 2, SH / 2);
  }
  delay(800);
  tft.fillScreen(TFT_BLACK);
}

// ─── Dibuja caja dorada ───────────────────────────────────────────────────────
void dibujarCaja() {
  // Sombra suave
  tft.fillRoundRect(CASE_X + 2, CASE_Y + 2, CASE_W, CASE_H, 3,
                    tft.color565(55, 38, 8));

  // Cuerpo dorado
  tft.fillRoundRect(CASE_X, CASE_Y, CASE_W, CASE_H, 3, C_GOLD);

  // Biseles (efecto tallado)
  tft.drawRoundRect(CASE_X,     CASE_Y,     CASE_W,     CASE_H,     3, C_GOLD_DK);
  tft.drawRoundRect(CASE_X + 1, CASE_Y + 1, CASE_W - 2, CASE_H - 2, 2, C_GOLD_LT);
  tft.drawRoundRect(CASE_X + 2, CASE_Y + 2, CASE_W - 4, CASE_H - 4, 2, C_GOLD_DK);
  tft.drawRoundRect(CASE_X + 3, CASE_Y + 3, CASE_W - 6, CASE_H - 6, 1, C_GOLD_LT);

  // Corona lateral derecha
  tft.fillRect(SW - 2, CY - 6, 4, 12, C_GOLD);
  tft.drawRect(SW - 2, CY - 6, 4, 12, C_GOLD_DK);
  tft.drawFastVLine(SW - 1, CY - 4, 8, C_GOLD_LT);
}

// ─── Dibuja esfera blanca marfil (completa, estática) ─────────────────────────
void dibujarEsfera() {
  // Fondo marfil
  tft.fillRect(DIAL_X, DIAL_Y, DIAL_W, DIAL_H, C_DIAL);

  // Patrón guilloché: líneas verticales finas
  for (int x = DIAL_X; x < DIAL_X + DIAL_W; x++) {
    uint16_t lc;
    int rel = x - DIAL_X;
    if      (rel % 4 == 0) lc = C_DIAL_LN;
    else if (rel % 4 == 2) lc = tft.color565(246, 243, 232);
    else                   lc = C_DIAL;
    tft.drawFastVLine(x, DIAL_Y, DIAL_H, lc);
  }

  // Borde interior esfera
  tft.drawRect(DIAL_X,     DIAL_Y,     DIAL_W,     DIAL_H,     tft.color565(165, 155, 135));
  tft.drawRect(DIAL_X + 1, DIAL_Y + 1, DIAL_W - 2, DIAL_H - 2, tft.color565(215, 208, 192));

  // ── Marcas de minuto (puntos) ──
  for (int i = 0; i < 60; i++) {
    if (i % 5 == 0) continue;
    float a = i * 6.0f;
    tft.fillCircle(angX(a, R), angY(a, R), 1, C_MK_M);
  }

  // ── Marcas de hora (barras negras gruesas) ──
  for (int i = 1; i <= 12; i++) {
    float a  = i * 30.0f;
    int x1 = angX(a, R - 8);
    int y1 = angY(a, R - 8);
    int x2 = angX(a, R);
    int y2 = angY(a, R);
    tft.drawLine(x1,     y1,     x2,     y2,     C_MK_H);
    tft.drawLine(x1 + 1, y1,     x2 + 1, y2,     C_MK_H);
    tft.drawLine(x1,     y1 + 1, x2,     y2 + 1, C_MK_H);
  }

  // ── Números arábigos ──
  tft.setTextDatum(MC_DATUM);

  // 12, 3, 6, 9 → tamaño 2 (grandes)
  tft.setTextSize(2);
  int bn[] = {12, 3, 6, 9};
  for (int n : bn) {
    float a  = n * 30.0f;
    int nx = angX(a, R - 17);
    int ny = angY(a, R - 17);
    tft.fillRect(nx - 9, ny - 8, 18, 16, C_DIAL);
    tft.setTextColor(C_INK, C_DIAL);
    char buf[3];
    sprintf(buf, "%d", n);
    tft.drawString(buf, nx, ny);
  }

  // 1,2,4,5,7,8,10,11 → tamaño 1 (pequeños)
  tft.setTextSize(1);
  int sn[] = {1, 2, 4, 5, 7, 8, 10, 11};
  for (int n : sn) {
    float a  = n * 30.0f;
    int nx = angX(a, R - 13);
    int ny = angY(a, R - 13);
    tft.fillRect(nx - 6, ny - 4, 14, 9, C_DIAL);
    tft.setTextColor(C_INK, C_DIAL);
    char buf[3];
    sprintf(buf, "%d", n);
    tft.drawString(buf, nx, ny);
  }

  // ── Texto de marca ──
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  
  // ELIMINADO: Jaeger-LeCoultre (ahora invisible)
  // tft.fillRect(CX - 42, CY - 25, 84, 10, C_DIAL);
  // tft.setTextColor(C_BRAND, C_DIAL);
  // tft.drawString("Jaeger-LeCoultre", CX, CY - 20);
  
  // Línea decorativa debajo del nombre (también eliminada para mantener coherencia)
  // tft.drawFastHLine(CX - 30, CY - 11, 60, C_GOLD_DK);

  // "FALLBAND" debajo del centro (cambiado de SWISS MADE)
  tft.fillRect(CX - 26, CY + 36, 52, 10, C_DIAL);
  tft.setTextColor(C_SWISS, C_DIAL);
  tft.drawString("FALLBAND", CX, CY + 41);
}

// ─── Manecilla tipo espada (sword hand) ──────────────────────────────────────
// angDeg: 0=arriba, 90=derecha
void dibujarEspada(float angDeg, int largo, int grosorMax, uint16_t color) {
  float rad = (angDeg - 90.0f) * M_PI / 180.0f;
  float dx  =  cosf(rad);
  float dy  =  sinf(rad);
  float px_ = -dy;
  float py_ =  dx;

  // Contrapeso + manecilla (de -TAIL hasta +largo)
  int total = largo + LEN_TAIL;
  for (int i = 0; i <= total; i++) {
    // posición a lo largo del eje (0 = base cola, LEN_TAIL = centro)
    float t   = (float)i / total;                        // 0..1
    float pos = (float)(i - LEN_TAIL) / largo;           // -1..1 (neg=cola)

    // Ancho variable tipo espada
    float w;
    if (pos < 0.0f) {
      // Cola: ancho fijo y pequeño
      w = grosorMax * 0.30f;
    } else if (pos < 0.25f) {
      // Base→máximo
      w = grosorMax * (pos / 0.25f);
    } else {
      // Máximo→punta
      w = grosorMax * (1.0f - (pos - 0.25f) / 0.75f);
    }
    if (w < 0.5f) w = 0.5f;

    // Punto central de esta línea
    int bx = CX + (int)((-LEN_TAIL + (largo + LEN_TAIL) * t) * dx);
    int by = CY + (int)((-LEN_TAIL + (largo + LEN_TAIL) * t) * dy);

    int ax = (int)(w * px_);
    int ay = (int)(w * py_);
    tft.drawLine(bx - ax, by - ay, bx + ax, by + ay, color);
  }
}

// ─── Actualiza cada segundo ───────────────────────────────────────────────────
void actualizarReloj() {
  struct tm t;
  if (!getLocalTime(&t)) return;

  int H = t.tm_hour % 12;
  int M = t.tm_min;
  int S = t.tm_sec;

  // Re-dibujar esfera (borra manecillas)
  dibujarEsfera();

  // ── Horas ──
  float angH = H * 30.0f + M * 0.5f;
  dibujarEspada(angH, LEN_HOUR, 5, C_BLUE);

  // ── Minutos ──
  float angM = M * 6.0f;
  dibujarEspada(angM, LEN_MIN, 4, C_BLUE);

  // ── Segundero (fino, rojo) ──
  float sRad = (S * 6.0f - 90.0f) * M_PI / 180.0f;
  int stX = CX + (int)(LEN_SEC  * cosf(sRad));
  int stY = CY + (int)(LEN_SEC  * sinf(sRad));
  int scX = CX - (int)(LEN_TAIL * cosf(sRad));
  int scY = CY - (int)(LEN_TAIL * sinf(sRad));
  tft.drawLine(scX,     scY,     stX,     stY,     C_RED);
  tft.drawLine(scX + 1, scY,     stX + 1, stY,     C_RED);

  // ── Centro dorado ──
  tft.fillCircle(CX, CY, 5, C_DOT_GOLD);
  tft.fillCircle(CX, CY, 2, tft.color565(70, 52, 15));

  // ── Ventanilla de fecha (estilo Reverso) ──
  // Posición: justo debajo del centro, entre el 6
  int winX = CX - 16;
  int winY = CY + 10;
  tft.fillRect(winX, winY, 32, 13, C_WIN_BG);
  tft.drawRect(winX, winY, 32, 13, C_WIN_BD);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(C_INK, C_WIN_BG);
  char dia[3];
  sprintf(dia, "%02d", t.tm_mday);
  tft.drawString(dia, CX, winY + 6);

  // ── Hora digital pequeña (zona negra bajo la esfera — aprox. 7 px) ──
  // La esfera ocupa casi todo; ponemos hora:min dentro, muy abajo de la esfera
  // Entre CY+55 y DIAL_Y+DIAL_H-6 hay espacio suficiente
  int horaZoneY = DIAL_Y + DIAL_H - 22;
  tft.fillRect(DIAL_X + 2, horaZoneY, DIAL_W - 4, 20, C_DIAL);
  tft.setTextColor(C_DIGI, C_DIAL);
  tft.setTextSize(1);
  char horaStr[9];
  sprintf(horaStr, "%02d:%02d:%02d", t.tm_hour, M, S);
  tft.drawString(horaStr, CX, horaZoneY + 6);

  const char* dias[] = {"DOM","LUN","MAR","MIE","JUE","VIE","SAB"};
  tft.setTextColor(C_DATE_C, C_DIAL);
  tft.drawString(dias[t.tm_wday], CX, horaZoneY + 15);
}
