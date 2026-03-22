# ⌚ FALLBAND - Reloj Analógico Inteligente con Alertas por Email

![Versión](https://img.shields.io/badge/version-1.0.0-blue.svg)
![Plataforma](https://img.shields.io/badge/platform-ESP32-orange.svg)
![Librerías](https://img.shields.io/badge/libraries-TFT_eSPI%2C%20WiFi%2C%20WebServer%2C%20Preferences-brightgreen.svg)
![Licencia](https://img.shields.io/badge/license-MIT-lightgrey.svg)

**FALLBAND** es un reloj analógico digital de lujo basado en **TTGO T-Display ESP32**. Combina la elegancia de un reloj clásico con funcionalidades modernas como configuración WiFi a través de punto de acceso, 5 estilos visuales únicos y un sistema de alertas por correo electrónico (EmailJS) activado por movimiento detectado desde el navegador.

---

## ✨ Características Principales

- **Reloj Analógico en Pantalla TFT** de 135x240 píxeles con 5 estilos diferentes.
- **Configuración WiFi sencilla** mediante punto de acceso (AP) y servidor web.
- **Sincronización horaria NTP** automática para hora precisa.
- **Alertas por correo electrónico** usando EmailJS al detectar movimiento (acelerómetro del dispositivo).
- **Interfaz web elegante** con diseño dorado (`FALLBAND_SETUP`).
- **Botones físicos**:
  - **Izquierdo**: Cambia el estilo del reloj.
  - **Derecho**: Activa el modo configuración manual.

---

## 🖥️ Plataforma

**URL del proyecto en producción:**
- Web principal: [https://fallbandreloj.foroactivo.com/](https://fallbandreloj.foroactivo.com/)
- Reportes y alertas: [https://fallbandreloj.foroactivo.com/h1-reportes](https://fallbandreloj.foroactivo.com/h1-reportes)

---

## 📚 Librerías Utilizadas

| Librería | Versión | Descripción |
|----------|--------|-------------|
| **TFT_eSPI** | 2.5.0 | Control de pantalla gráfica TFT para ESP32. Maneja el display de 135x240 y la interfaz gráfica del reloj. |
| **WiFi.h** | Built-in | Conectividad WiFi para sincronizar hora y acceder a internet. |
| **time.h** | Built-in | Gestión de tiempo NTP para mantener la hora exacta. |
| **math.h** | Built-in | Cálculos trigonométricos para dibujar las manecillas del reloj. |
| **WebServer.h** | Built-in | Servidor web embebido para la configuración WiFi. |
| **Preferences.h** | Built-in | Almacenamiento no volátil de credenciales WiFi, teléfono y correo. |

> **EmailJS** se integra desde el frontend web mediante su SDK (versión 4) para el envío de correos desde el navegador del cliente.

---

## 🎨 Estilos de Reloj (5)

El reloj cuenta con **5 paletas de colores** diferentes que se pueden cambiar con el botón izquierdo:

1. **Clásico** – Fondo crema, números negros, manecillas azul oscuro, segundero rojo.
2. **Moderno** – Azul noche, detalles cian, letras blancas.
3. **Deportivo** – Negro, rojo intenso, manecillas rojas.
4. **Minimalista** – Blanco y negro puro, sin colores saturados.
5. **Vintage** – Beige, marrón, dorado, estilo retro.

Cada estilo incluye:
- Color de caja, fondo, marcadores, números, manecillas, segundero, acento y texto de marca.

---

## ⚙️ Cómo Funciona

### 🔌 Inicio
1. El ESP32 intenta conectarse al WiFi guardado en la memoria flash.
2. Si no hay credenciales o falla la conexión, entra automáticamente en **modo configuración**.
3. En modo configuración, crea un punto de acceso `FALLBAND_SETUP` con contraseña `12345678`.
4. El usuario se conecta al AP y accede a `192.168.4.1` desde su navegador.
5. Ingresa SSID, contraseña WiFi, teléfono y correo electrónico.
6. El ESP32 guarda los datos, se conecta a internet y obtiene la hora mediante NTP.

### 🕹️ Interacción con Botones

- **Botón Izquierdo (GPIO0):** Cambia entre los 5 estilos de reloj. Tiene antirrebote y espera mínima de 300ms.
- **Botón Derecho (GPIO35):** Fuerza la entrada al modo configuración en cualquier momento.

### 📧 Alertas por Email (EmailJS)
Desde la página de configuración se activa un sensor de movimiento usando el **acelerómetro del teléfono móvil**:
- Detecta cambios bruscos de orientación (beta/gamma > 25°).
- Envía una alerta por correo electrónico mediante EmailJS con:
  - Tipo de movimiento (vertical, horizontal, completo)
  - Datos detallados del cambio
  - Fecha y hora local (México)
  - Dispositivo registrado

**Nota:** El correo se envía usando el template `template_fallback` y el servicio `service_xrvdddn`.

---

## 🧩 Estructura del Código

FALLBAND_ESP32/
├── fallband.ino # Código principal
├── README.md # Este archivo
├── librerías/
│ └── TFT_eSPI/ # Configuración de pantalla
├── web/
│ └── config.html # Página incrustada en el código
└── preferences/ # Almacena credenciales


---

## 📱 Interfaz Web de Configuración

- **Estilo:** Negro con acentos dorados, gradientes y animaciones.
- **Campos:** SSID, contraseña, teléfono, correo (todos requeridos).
- **Sensor:** Visualización en tiempo real del ángulo del dispositivo y detección de movimiento.
- **Alertas:** Animación visual al detectar movimiento y envío automático de correo.

---

## 🔧 Configuración Personalizada

### 📌 EmailJS
Debes tener una cuenta en [EmailJS](https://www.emailjs.com/) y configurar:
- **Service ID:** `service_xrvdddn`
- **Template ID:** `template_fallback` (creado por ti)
- **Public Key:** `S278dC2zQyziLhWNb`

Dentro del template puedes usar variables como `{{to_email}}`, `{{alerta_tipo}}`, `{{alerta_datos}}`, `{{alerta_fecha}}`, etc.

### 📍 Zona Horaria
Actualmente configurada para **México (GMT-6 con horario de verano)**:
```cpp
const long  gmtOffset_sec      = -21600;
const int   daylightOffset_sec = 3600;
Puedes cambiar estos valores según tu ubicación.
```

## 🧪 Pruebas y Reportes
Los reportes de alertas y actividad se pueden visualizar en:
https://fallbandreloj.foroactivo.com/h1-reportes

Allí se almacenan los envíos de correo y la respuesta del sistema.

## 📦 Instalación en tu ESP32
Clona este repositorio.

Instala las librerías necesarias en el Arduino IDE o PlatformIO.

Configura User_Setup.h de TFT_eSPI para TTGO T-Display (ST7789V, 135x240, pines correctos).

Sube el código al ESP32.

Abre el monitor serie para ver el estado de conexión.

## 🚀 Próximas Mejoras
Guardar múltiples redes WiFi.

Personalizar horario de alertas.

Dashboard web con estadísticas.

Soporte para más sensores (temperatura, humedad).

Control de brillo automático.

## 🤝 Contribuciones
Las contribuciones son bienvenidas. Por favor, abre un issue o pull request para sugerir cambios.

## 📄 Licencia
Este proyecto está bajo la licencia MIT. Puedes usarlo libremente, modificarlo y distribuirlo con atribución.

## 📧 Contacto
Proyecto FALLBAND
Sitio: https://fallbandreloj.foroactivo.com/
Foro de soporte: https://fallbandreloj.foroactivo.com/h1-reportes

