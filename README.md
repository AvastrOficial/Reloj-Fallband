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

## 🧩 Estructura del Proyecto

```bash
FALLBAND_ESP32/
├── 📄 fallband.ino        # Código principal
├── 📘 README.md           # Documentación del proyecto
├── 📁 librerías/
│   └── TFT_eSPI/         # Configuración de pantalla
├── 🌐 web/
│   └── config.html       # Interfaz web incrustada
└── ⚙️ preferences/       # Almacenamiento de credenciales
```

---

## 📱 Interfaz Web de Configuración

| Característica | Descripción                                               |
| -------------- | --------------------------------------------------------- |
| 🎨 **Estilo**  | Tema oscuro con acentos dorados, gradientes y animaciones |
| 📝 **Campos**  | SSID, contraseña, teléfono y correo (obligatorios)        |
| 📡 **Sensor**  | Visualización en tiempo real del ángulo y movimiento      |
| 🚨 **Alertas** | Animaciones + envío automático de correo                  |

---

## 🔧 Configuración Personalizada

### 📌 EmailJS

Debes crear una cuenta en:
👉 https://www.emailjs.com/

**Configuración requerida:**

| Parámetro   | Valor               |
| ----------- | ------------------- |
| Service ID  | `service_xrvdddn`   |
| Template ID | `template_fallback` |
| Public Key  | `S278dC2zQyziLhWNb` |

**Variables disponibles en el template:**

```txt
{{to_email}}
{{alerta_tipo}}
{{alerta_datos}}
{{alerta_fecha}}
```

---

### 📍 Zona Horaria

Configuración actual para **México (GMT-6 + horario de verano):**

```cpp
const long  gmtOffset_sec      = -21600;
const int   daylightOffset_sec = 3600;
```

💡 Puedes modificar estos valores según tu ubicación.

---

## 🧪 Pruebas y Reportes

📊 Visualiza reportes y actividad en:

🔗 https://fallbandreloj.foroactivo.com/h1-reportes

**Incluye:**

* Envíos de correo
* Respuestas del sistema
* Eventos detectados

---

## 📦 Instalación en ESP32

1. 📥 Clona este repositorio
2. 📚 Instala las librerías necesarias (Arduino IDE / PlatformIO)
3. ⚙️ Configura `User_Setup.h` de **TFT_eSPI**:

   * Pantalla: **TTGO T-Display**
   * Driver: **ST7789V**
   * Resolución: **135x240**
4. 🚀 Sube el código al ESP32
5. 🔍 Abre el monitor serie para verificar conexión

---

## 🚀 Próximas Mejoras

* 📶 Soporte para múltiples redes WiFi
* ⏰ Configuración de horarios de alerta
* 📊 Dashboard web con estadísticas
* 🌡️ Integración de sensores (temperatura, humedad)
* 🔆 Control automático de brillo

---

## 🤝 Contribuciones

Las contribuciones son bienvenidas 🙌

Puedes:

* Abrir un **issue**
* Enviar un **pull request**
* Proponer mejoras

---

## 📄 Licencia

Este proyecto está bajo la licencia **MIT**.
✔️ Uso libre
✔️ Modificación permitida
✔️ Distribución con atribución

---

## 📧 Contacto

| Tipo       | Enlace                                           |
| ---------- | ------------------------------------------------ |
| 🌐 Sitio   | https://fallbandreloj.foroactivo.com/            |
| 💬 Soporte | https://fallbandreloj.foroactivo.com/h1-reportes |


