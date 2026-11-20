/*
  SISTEMA DE MONITOREO AMBIENTAL INTELIGENTE - ESP32
  Firmware integrado inicial para TP.

  Componentes:
  - BMP280 (I2C 0x76 / 0x77)
  - INA219 (I2C)
  - LCD I2C 16x2 (direccion 0x27)
  - MQ-2 x2 (entradas analogicas)
  - LDR (entrada analogica)
  - PC817 (entrada digital)
  - HLK-LD2410C (salida OUT digital)
  - LEDs y rele
  - WiFiManager, MQTT y Telegram
  - Configuracion persistente mediante Preferences (NVS del ESP32)

  IMPORTANTE:
  1) Revisar TODOS los GPIO contra el esquematico y la placa real.
  2) No conectar 5 V directamente a GPIO del ESP32.
  3) Las escalas porcentuales de MQ-2 y LDR son aproximadas y requieren calibracion.
  4) No hay sensor de humedad instalado: se publica humedad=null.
  5) El pin HLK_OUT esta configurado en GPIO1 como en el mapeo previo.
     GPIO1 es UART0 TX; por eso este firmware no utiliza Serial.
  6) Cambiar BOT_TOKEN y CHAT_ID por credenciales propias. Si el token se
     compartio en un chat publico o no confiable, revocarlo con BotFather.
*/

#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_INA219.h>
#include <LiquidCrystal_I2C.h>

#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <PubSubClient.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

#include <Preferences.h>
#include <time.h>

// ==========================================================
// 1. CREDENCIALES
// ==========================================================

// WiFiManager permite configurar la red desde un portal.
// Estas credenciales se dejan como respaldo opcional.
const char* WIFI_SSID_RESPALDO = "MECA-IoT-V2";
const char* WIFI_PASSWORD_RESPALDO = "CAMBIAR_CLAVE";

#define BOT_TOKEN "PEGAR_TOKEN_NUEVO_DEL_BOT"
#define TELEGRAM_CHAT_ID "PEGAR_CHAT_ID"

// ==========================================================
// 2. MQTT
// ==========================================================

const char* MQTT_SERVER = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_TOPIC = "tp-iot/grupo2/mediciones";
const char* MQTT_CLIENT_ID = "ESP32_TP_IOT_GRUPO2";

// ==========================================================
// 3. PINES - verificar con el esquematico
// ==========================================================

#define SDA_PIN 21
#define SCL_PIN 22

#define MQ2_1_PIN 34
#define MQ2_2_PIN 35
#define LDR_PIN 32
#define PC817_PIN 19
#define HLK_OUT_PIN 1

#define RELE_PIN 33
#define LED_VERDE_PIN 5
#define LED_AMARILLO_PIN 17
#define LED_ROJO_PIN 16

#define BTN_MENU_PIN 25
#define BTN_ARRIBA_PIN 26
#define BTN_ABAJO_PIN 27
#define BTN_ACEPTAR_PIN 14
#define BTN_ATRAS_PIN 12

// Cambiar a true si el modulo de rele se activa con LOW.
const bool RELE_ACTIVO_EN_LOW = false;

// ==========================================================
// 4. OBJETOS
// ==========================================================

Adafruit_BMP280 bmp;
Adafruit_INA219 ina219;
LiquidCrystal_I2C lcd(0x27, 16, 2);

WiFiClient mqttClient;
PubSubClient mqtt(mqttClient);

WiFiClientSecure telegramClient;
UniversalTelegramBot* bot = nullptr;

Preferences preferences;

// ==========================================================
// 5. CONFIGURACION
// ==========================================================

struct Configuracion {
  int offsetGMT = -3;
  float temperaturaMax = 35.0;
  float temperaturaCritica = 45.0;
  float gasAlerta = 60.0;
  float gasPeligro = 80.0;
  float luzMinima = 20.0;
  float corrienteMax = 1000.0;
  uint32_t intervaloMQTT = 10000UL;
};

Configuracion config;

// ==========================================================
// 6. MEDICIONES
// ==========================================================

float temperatura = NAN;
float presion = NAN;
float humedad = NAN;  // No hay sensor instalado.

float gas1 = 0;
float gas2 = 0;
float gas = 0;
float luz = 0;

float corriente = NAN;
float tension = NAN;
float potencia = NAN;

bool presencia = false;
bool opto = false;

bool bmpDisponible = false;
bool inaDisponible = false;

String fechaHora = "Sin sincronizar";

// ==========================================================
// 7. MAQUINA DE ESTADOS
// ==========================================================

enum EstadoMaquina {
  ESTADO_INICIO,
  ESTADO_CONEXION,
  ESTADO_LECTURA,
  ESTADO_EVALUACION,
  ESTADO_MONITOREO
};

enum EstadoAmbiental {
  NORMAL,
  ALERTA,
  PELIGRO
};

EstadoMaquina estadoMaquina = ESTADO_INICIO;
EstadoAmbiental estadoAmbiental = NORMAL;
EstadoAmbiental estadoAmbientalAnterior = NORMAL;

// ==========================================================
// 8. TEMPORIZADORES
// ==========================================================

const uint32_t INTERVALO_LECTURA = 2000UL;
const uint32_t INTERVALO_TELEGRAM = 3000UL;
const uint32_t INTERVALO_LCD = 2500UL;

uint32_t ultimaLectura = 0;
uint32_t ultimoEnvioMQTT = 0;
uint32_t ultimaPantalla = 0;
uint32_t ultimaConsultaTelegram = 0;

// ==========================================================
// 9. MENU
// ==========================================================

bool menuActivo = false;
bool editando = false;
int opcionMenu = 0;

const int CANT_OPCIONES = 8;

const char* nombresMenu[CANT_OPCIONES] = {
  "GMT",
  "Temp. max",
  "Temp. crit.",
  "Gas alerta",
  "Gas peligro",
  "Luz minima",
  "Corriente max",
  "Intervalo MQTT"
};

// ==========================================================
// 10. CONFIGURACION PERSISTENTE
// ==========================================================

void cargarConfiguracion() {
  preferences.begin("config", false);

  config.offsetGMT = preferences.getInt("gmt", -3);
  config.temperaturaMax = preferences.getFloat("tmax", 35.0);
  config.temperaturaCritica = preferences.getFloat("tcrit", 45.0);
  config.gasAlerta = preferences.getFloat("galerta", 60.0);
  config.gasPeligro = preferences.getFloat("gpeligro", 80.0);
  config.luzMinima = preferences.getFloat("luz", 20.0);
  config.corrienteMax = preferences.getFloat("corr", 1000.0);
  config.intervaloMQTT = preferences.getUInt("mqtt", 10000UL);

  config.intervaloMQTT = constrain(config.intervaloMQTT, 5000UL, 300000UL);
}

void guardarConfiguracion() {
  preferences.putInt("gmt", config.offsetGMT);
  preferences.putFloat("tmax", config.temperaturaMax);
  preferences.putFloat("tcrit", config.temperaturaCritica);
  preferences.putFloat("galerta", config.gasAlerta);
  preferences.putFloat("gpeligro", config.gasPeligro);
  preferences.putFloat("luz", config.luzMinima);
  preferences.putFloat("corr", config.corrienteMax);
  preferences.putUInt("mqtt", config.intervaloMQTT);
}

// ==========================================================
// 11. INICIALIZACION
// ==========================================================

void iniciarSensores() {
  Wire.begin(SDA_PIN, SCL_PIN);

  bmpDisponible = bmp.begin(0x76);
  if (!bmpDisponible) {
    bmpDisponible = bmp.begin(0x77);
  }

  inaDisponible = ina219.begin();

  analogReadResolution(12);
  analogSetPinAttenuation(MQ2_1_PIN, ADC_11db);
  analogSetPinAttenuation(MQ2_2_PIN, ADC_11db);
  analogSetPinAttenuation(LDR_PIN, ADC_11db);
}

void iniciarEntradasSalidas() {
  pinMode(RELE_PIN, OUTPUT);
  pinMode(LED_VERDE_PIN, OUTPUT);
  pinMode(LED_AMARILLO_PIN, OUTPUT);
  pinMode(LED_ROJO_PIN, OUTPUT);

  pinMode(PC817_PIN, INPUT_PULLUP);
  pinMode(HLK_OUT_PIN, INPUT);

  pinMode(BTN_MENU_PIN, INPUT_PULLUP);
  pinMode(BTN_ARRIBA_PIN, INPUT_PULLUP);
  pinMode(BTN_ABAJO_PIN, INPUT_PULLUP);
  pinMode(BTN_ACEPTAR_PIN, INPUT_PULLUP);
  pinMode(BTN_ATRAS_PIN, INPUT_PULLUP);

  digitalWrite(LED_VERDE_PIN, LOW);
  digitalWrite(LED_AMARILLO_PIN, LOW);
  digitalWrite(LED_ROJO_PIN, LOW);
  controlarRele(false);
}

void controlarRele(bool activar) {
  bool nivel = activar;
  if (RELE_ACTIVO_EN_LOW) nivel = !nivel;
  digitalWrite(RELE_PIN, nivel ? HIGH : LOW);
}

// ==========================================================
// 12. LECTURA DE SENSORES
// ==========================================================

// Estos valores son referencias iniciales, NO ppm ni calibracion real.
const int GAS_ADC_MIN = 500;
const int GAS_ADC_MAX = 3000;

// Ajustar segun el divisor resistivo y orientacion del LDR.
const int LUZ_ADC_MIN = 0;
const int LUZ_ADC_MAX = 4095;

float limitarPorcentaje(float valor) {
  if (valor < 0) return 0;
  if (valor > 100) return 100;
  return valor;
}

float mapearPorcentaje(int lectura, int minimo, int maximo) {
  if (maximo <= minimo) return 0;
  return limitarPorcentaje(
    (lectura - minimo) * 100.0f / (maximo - minimo)
  );
}

void leerSensores() {
  if (bmpDisponible) {
    temperatura = bmp.readTemperature();
    presion = bmp.readPressure() / 100.0f;
  }

  int lecturaGas1 = analogRead(MQ2_1_PIN);
  int lecturaGas2 = analogRead(MQ2_2_PIN);

  gas1 = mapearPorcentaje(lecturaGas1, GAS_ADC_MIN, GAS_ADC_MAX);
  gas2 = mapearPorcentaje(lecturaGas2, GAS_ADC_MIN, GAS_ADC_MAX);
  gas = max(gas1, gas2);

  int lecturaLuz = analogRead(LDR_PIN);
  luz = mapearPorcentaje(lecturaLuz, LUZ_ADC_MIN, LUZ_ADC_MAX);

  if (inaDisponible) {
    corriente = ina219.getCurrent_mA();
    tension = ina219.getBusVoltage_V();
    potencia = ina219.getPower_mW();
  }

  // Suposicion: salida del modulo PC817 activa en LOW.
  opto = (digitalRead(PC817_PIN) == LOW);

  // Suposicion: HLK OUT en HIGH cuando detecta presencia.
  presencia = (digitalRead(HLK_OUT_PIN) == HIGH);

  // No existe sensor de humedad instalado.
  humedad = NAN;
}

// ==========================================================
// 13. FECHA Y HORA
// ==========================================================

void iniciarHora() {
  // Argentina: GMT-3. El offset puede modificarse desde el menu.
  configTime(config.offsetGMT * 3600, 0,
             "pool.ntp.org", "time.google.com");
}

void actualizarFechaHora() {
  time_t ahora = time(nullptr);

  if (ahora < 100000) {
    fechaHora = "Sin sincronizar";
    return;
  }

  struct tm tiempo;
  localtime_r(&ahora, &tiempo);

  char buffer[24];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tiempo);
  fechaHora = String(buffer);
}

// ==========================================================
// 14. EVALUACION DE UMBRALES
// ==========================================================

void evaluarAmbiente() {
  estadoAmbiental = NORMAL;

  bool alertaTemperatura =
    bmpDisponible && temperatura > config.temperaturaMax;

  bool alertaGas = gas >= config.gasAlerta;
  bool alertaLuz = luz < config.luzMinima;
  bool alertaCorriente =
    inaDisponible && corriente > config.corrienteMax;

  if (alertaTemperatura || alertaGas || alertaLuz || alertaCorriente) {
    estadoAmbiental = ALERTA;
  }

  bool peligroTemperatura =
    bmpDisponible && temperatura >= config.temperaturaCritica;

  bool peligroGas = gas >= config.gasPeligro;

  if (peligroTemperatura || peligroGas) {
    estadoAmbiental = PELIGRO;
  }
}

// ==========================================================
// 15. LEDS Y RELE
// ==========================================================

void actualizarActuadores() {
  digitalWrite(LED_VERDE_PIN, LOW);
  digitalWrite(LED_AMARILLO_PIN, LOW);
  digitalWrite(LED_ROJO_PIN, LOW);

  if (estadoAmbiental == NORMAL) {
    digitalWrite(LED_VERDE_PIN, HIGH);
    controlarRele(false);
  } else if (estadoAmbiental == ALERTA) {
    digitalWrite(LED_AMARILLO_PIN, HIGH);
    controlarRele(false);
  } else {
    digitalWrite(LED_ROJO_PIN, HIGH);
    controlarRele(true);
  }
}

// ==========================================================
// 16. PANTALLA LCD
// ==========================================================

void mostrarPantalla() {
  if (menuActivo) return;
  if (millis() - ultimaPantalla < INTERVALO_LCD) return;

  ultimaPantalla = millis();

  static uint8_t pagina = 0;
  pagina = (pagina + 1) % 5;

  lcd.clear();

  switch (pagina) {
    case 0:
      if (estadoAmbiental == NORMAL) lcd.print("ESTADO: NORMAL");
      if (estadoAmbiental == ALERTA) lcd.print("ESTADO: ALERTA");
      if (estadoAmbiental == PELIGRO) lcd.print("ESTADO: PELIGRO");

      lcd.setCursor(0, 1);
      lcd.print("TEMP:");
      if (bmpDisponible) lcd.print(temperatura, 1);
      else lcd.print("--");
      lcd.print(" C");
      break;

    case 1:
      lcd.print("GAS:");
      lcd.print(gas, 0);
      lcd.print("%");
      lcd.setCursor(0, 1);
      lcd.print("LUZ:");
      lcd.print(luz, 0);
      lcd.print("%");
      break;

    case 2:
      lcd.print("CORRIENTE:");
      lcd.setCursor(0, 1);
      if (inaDisponible) lcd.print(corriente, 1);
      else lcd.print("SIN INA219");
      if (inaDisponible) lcd.print(" mA");
      break;

    case 3:
      lcd.print("PRESENCIA:");
      lcd.print(presencia ? "SI" : "NO");
      lcd.setCursor(0, 1);
      lcd.print("OPTO:");
      lcd.print(opto ? "ACTIVO" : "INACTIVO");
      break;

    case 4:
      lcd.print("WIFI:");
      lcd.print(WiFi.status() == WL_CONNECTED ? "OK" : "NO");
      lcd.setCursor(0, 1);
      lcd.print("MQTT:");
      lcd.print(mqtt.connected() ? "OK" : "NO");
      break;
  }
}

// ==========================================================
// 17. BOTONES Y MENU
// ==========================================================

bool botonPresionado(int pin) {
  static uint32_t ultimoCambio[40] = {0};
  static bool estadoAnterior[40] = {false};

  bool estadoActual = (digitalRead(pin) == LOW);
  bool evento = false;

  if (estadoActual && !estadoAnterior[pin] &&
      millis() - ultimoCambio[pin] > 180) {
    evento = true;
    ultimoCambio[pin] = millis();
  }

  estadoAnterior[pin] = estadoActual;
  return evento;
}

float obtenerValorMenu() {
  switch (opcionMenu) {
    case 0: return config.offsetGMT;
    case 1: return config.temperaturaMax;
    case 2: return config.temperaturaCritica;
    case 3: return config.gasAlerta;
    case 4: return config.gasPeligro;
    case 5: return config.luzMinima;
    case 6: return config.corrienteMax;
    case 7: return config.intervaloMQTT / 1000.0f;
  }
  return 0;
}

void modificarValorMenu(int cambio) {
  switch (opcionMenu) {
    case 0:
      config.offsetGMT = constrain(config.offsetGMT + cambio, -12, 14);
      break;

    case 1:
      config.temperaturaMax =
        constrain(config.temperaturaMax + cambio, 10.0f, 60.0f);
      break;

    case 2:
      config.temperaturaCritica =
        constrain(config.temperaturaCritica + cambio, 15.0f, 80.0f);
      break;

    case 3:
      config.gasAlerta =
        constrain(config.gasAlerta + cambio, 0.0f, 100.0f);
      break;

    case 4:
      config.gasPeligro =
        constrain(config.gasPeligro + cambio, 0.0f, 100.0f);
      break;

    case 5:
      config.luzMinima =
        constrain(config.luzMinima + cambio, 0.0f, 100.0f);
      break;

    case 6:
      config.corrienteMax =
        constrain(config.corrienteMax + cambio * 100.0f,
                   100.0f, 10000.0f);
      break;

    case 7:
      if (cambio > 0 && config.intervaloMQTT < 300000UL) {
        config.intervaloMQTT += 1000UL;
      } else if (cambio < 0 && config.intervaloMQTT > 5000UL) {
        config.intervaloMQTT -= 1000UL;
      }
      config.intervaloMQTT =
        constrain(config.intervaloMQTT, 5000UL, 300000UL);
      break;
  }
}

void mostrarMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(nombresMenu[opcionMenu]);

  lcd.setCursor(0, 1);
  lcd.print(editando ? "Editar: " : "Valor: ");

  if (opcionMenu == 7) {
    lcd.print(config.intervaloMQTT / 1000UL);
    lcd.print(" seg");
  } else if (opcionMenu == 0) {
    lcd.print(config.offsetGMT);
  } else if (opcionMenu == 6) {
    lcd.print(config.corrienteMax, 0);
    lcd.print("mA");
  } else {
    lcd.print(obtenerValorMenu(), 1);
  }
}

void gestionarMenu() {
  if (botonPresionado(BTN_MENU_PIN)) {
    if (!menuActivo) {
      menuActivo = true;
      editando = false;
      opcionMenu = 0;
      mostrarMenu();
    } else if (!editando) {
      menuActivo = false;
      guardarConfiguracion();
      iniciarHora();
      lcd.clear();
    }
  }

  if (!menuActivo) return;

  if (botonPresionado(BTN_ARRIBA_PIN)) {
    if (editando) {
      modificarValorMenu(1);
    } else {
      opcionMenu = (opcionMenu + 1) % CANT_OPCIONES;
    }
    mostrarMenu();
  }

  if (botonPresionado(BTN_ABAJO_PIN)) {
    if (editando) {
      modificarValorMenu(-1);
    } else {
      opcionMenu--;
      if (opcionMenu < 0) opcionMenu = CANT_OPCIONES - 1;
    }
    mostrarMenu();
  }

  if (botonPresionado(BTN_ACEPTAR_PIN)) {
    editando = !editando;
    if (!editando) {
      guardarConfiguracion();
      iniciarHora();
    }
    mostrarMenu();
  }

  if (botonPresionado(BTN_ATRAS_PIN)) {
    if (editando) {
      editando = false;
      mostrarMenu();
    } else {
      menuActivo = false;
      guardarConfiguracion();
      iniciarHora();
      lcd.clear();
    }
  }
}

// ==========================================================
// 18. WIFI Y PORTAL DE CONFIGURACION
// ==========================================================

void iniciarWiFi() {
  WiFi.mode(WIFI_STA);

  // Intenta primero la red definida como respaldo.
  if (String(WIFI_SSID_RESPALDO).length() > 0 &&
      String(WIFI_PASSWORD_RESPALDO) != "CAMBIAR_CLAVE") {
    WiFi.begin(WIFI_SSID_RESPALDO, WIFI_PASSWORD_RESPALDO);

    uint32_t inicio = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - inicio < 12000UL) {
      delay(250);
    }
  }

  // Si no conecto, crea un portal de configuracion.
  if (WiFi.status() != WL_CONNECTED) {
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(180);
    wifiManager.autoConnect("TP-IOT-CONFIG");
  }
}

// ==========================================================
// 19. MQTT
// ==========================================================

void conectarMQTT() {
  if (mqtt.connected() || WiFi.status() != WL_CONNECTED) return;

  mqtt.connect(MQTT_CLIENT_ID);
}

void publicarMQTT() {
  if (!mqtt.connected()) return;

  StaticJsonDocument<512> documento;

  if (bmpDisponible) {
    documento["temperatura"] = temperatura;
    documento["presion"] = presion;
  } else {
    documento["temperatura"] = nullptr;
    documento["presion"] = nullptr;
  }

  // No se instala sensor de humedad: enviar null, no inventar un valor.
  documento["humedad"] = nullptr;

  documento["luz"] = luz;
  documento["gas"] = gas;
  documento["gas1"] = gas1;
  documento["gas2"] = gas2;

  if (inaDisponible) {
    documento["corriente"] = corriente;
    documento["tension"] = tension;
    documento["potencia"] = potencia;
  } else {
    documento["corriente"] = nullptr;
    documento["tension"] = nullptr;
    documento["potencia"] = nullptr;
  }

  documento["opto"] = opto ? 1 : 0;
  documento["presencia"] = presencia ? 1 : 0;
  documento["fecha_hora"] = fechaHora;

  if (estadoAmbiental == NORMAL) documento["estado"] = "NORMAL";
  else if (estadoAmbiental == ALERTA) documento["estado"] = "ALERTA";
  else documento["estado"] = "PELIGRO";

  char mensaje[512];
  size_t largo = serializeJson(documento, mensaje, sizeof(mensaje));

  if (largo > 0) {
    mqtt.publish(MQTT_TOPIC, mensaje);
  }
}

// ==========================================================
// 20. TELEGRAM
// ==========================================================

String obtenerEstadoTexto() {
  String mensaje = "SISTEMA DE MONITOREO\n\n";

  mensaje += "Temperatura: ";
  mensaje += bmpDisponible ? String(temperatura, 1) + " C\n"
                           : "No disponible\n";

  mensaje += "Humedad: No disponible (sin sensor)\n";

  mensaje += "Presion: ";
  mensaje += bmpDisponible ? String(presion, 1) + " hPa\n"
                           : "No disponible\n";

  mensaje += "Gas: " + String(gas, 1) + " %\n";
  mensaje += "Luz: " + String(luz, 1) + " %\n";

  mensaje += "Corriente: ";
  mensaje += inaDisponible ? String(corriente, 1) + " mA\n"
                           : "No disponible\n";

  mensaje += "Opto: ";
  mensaje += opto ? "ACTIVO\n" : "INACTIVO\n";

  mensaje += "Presencia: ";
  mensaje += presencia ? "SI\n" : "NO\n";

  mensaje += "Fecha: " + fechaHora + "\n";
  mensaje += "Estado: ";

  if (estadoAmbiental == NORMAL) mensaje += "NORMAL";
  else if (estadoAmbiental == ALERTA) mensaje += "ALERTA";
  else mensaje += "PELIGRO";

  return mensaje;
}

void enviarCambioEstadoTelegram() {
  if (bot == nullptr || WiFi.status() != WL_CONNECTED) return;
  if (estadoAmbiental == estadoAmbientalAnterior) return;

  String mensaje;

  if (estadoAmbiental == ALERTA) {
    mensaje = "ALERTA: Se supero uno o mas umbrales ambientales.";
  } else if (estadoAmbiental == PELIGRO) {
    mensaje = "PELIGRO: Se detecto una condicion critica.";
  } else {
    mensaje = "El ambiente volvio al estado NORMAL.";
  }

  if (bot->sendMessage(TELEGRAM_CHAT_ID, mensaje, "")) {
    estadoAmbientalAnterior = estadoAmbiental;
  }
}

void consultarTelegram() {
  if (bot == nullptr || WiFi.status() != WL_CONNECTED) return;
  if (millis() - ultimaConsultaTelegram < INTERVALO_TELEGRAM) return;

  ultimaConsultaTelegram = millis();

  int cantidad = bot->getUpdates(bot->last_message_received + 1);

  for (int i = 0; i < cantidad; i++) {
    if (bot->messages[i].chat_id != String(TELEGRAM_CHAT_ID)) continue;

    String texto = bot->messages[i].text;

    if (texto == "/start") {
      bot->sendMessage(
        TELEGRAM_CHAT_ID,
        "Monitoreo ambiental activo.\nEscribi /estado para consultar las mediciones.",
        ""
      );
    } else if (texto == "/estado") {
      bot->sendMessage(TELEGRAM_CHAT_ID, obtenerEstadoTexto(), "");
    }
  }
}

// ==========================================================
// 21. SETUP
// ==========================================================

void setup() {
  // No usar Serial porque GPIO1 esta asignado a HLK_OUT en este mapeo.

  cargarConfiguracion();
  iniciarEntradasSalidas();
  iniciarSensores();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("MONITOREO IOT");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");

  iniciarWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    iniciarHora();
  }

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);

  if (String(BOT_TOKEN) != "PEGAR_TOKEN_NUEVO_DEL_BOT" &&
      String(TELEGRAM_CHAT_ID) != "PEGAR_CHAT_ID") {
    telegramClient.setInsecure();
    bot = new UniversalTelegramBot(BOT_TOKEN, telegramClient);
  }

  delay(1200);
  lcd.clear();

  estadoMaquina = ESTADO_LECTURA;
}

// ==========================================================
// 22. LOOP PRINCIPAL
// ==========================================================

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt.connected()) conectarMQTT();
    mqtt.loop();
  }

  gestionarMenu();

  if (menuActivo) return;

  consultarTelegram();

  switch (estadoMaquina) {
    case ESTADO_INICIO:
      estadoMaquina = ESTADO_CONEXION;
      break;

    case ESTADO_CONEXION:
      estadoMaquina = ESTADO_LECTURA;
      break;

    case ESTADO_LECTURA:
      if (millis() - ultimaLectura >= INTERVALO_LECTURA) {
        ultimaLectura = millis();
        leerSensores();
        actualizarFechaHora();
        estadoMaquina = ESTADO_EVALUACION;
      }
      break;

    case ESTADO_EVALUACION:
      evaluarAmbiente();
      actualizarActuadores();
      estadoMaquina = ESTADO_MONITOREO;
      break;

    case ESTADO_MONITOREO:
      mostrarPantalla();
      enviarCambioEstadoTelegram();

      if (millis() - ultimoEnvioMQTT >= config.intervaloMQTT) {
        ultimoEnvioMQTT = millis();
        publicarMQTT();
      }

      estadoMaquina = ESTADO_LECTURA;
      break;
  }
}
