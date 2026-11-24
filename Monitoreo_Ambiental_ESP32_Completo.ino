
/*
  SISTEMA DE MONITOREO AMBIENTAL INTELIGENTE
  ESP32 - TP INTEGRADOR

  Componentes:
  BMP280
  INA219
  LCD I2C 16x2
  MQ-2 x2
  LDR
  PC817
  HLK-LD2410C (SOLO SALIDA DIGITAL OUT)
  Rele
  3 LEDs
  5 pulsadores

  Comunicaciones:
  WiFiManager
  MQTT
  Telegram

  Configuracion:
  Preferences (memoria no volatil ESP32)
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

// ======================================================
// 1. CREDENCIALES
// ======================================================

#define BOT_TOKEN "COLOCAR_TOKEN_NUEVO"
#define CHAT_ID "COLOCAR_CHAT_ID"

// ======================================================
// 2. CONFIGURACION MQTT
// ======================================================

const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

const char* MQTT_TOPIC = "tp-iot/grupo2/mediciones";
const char* MQTT_CLIENT_ID = "ESP32_MONITOREO_GRUPO2";

// ======================================================
// 3. PINES
// ======================================================

// I2C
#define SDA_PIN 21
#define SCL_PIN 22

// Sensores
#define MQ2_1_PIN 34
#define MQ2_2_PIN 35
#define LDR_PIN 32

#define PC817_PIN 19
#define HLK_OUT_PIN 23

// Actuadores
#define RELE_PIN 33

#define LED_VERDE 16
#define LED_AMARILLO 17
#define LED_ROJO 18

// Pulsadores
#define BTN_MENU 25
#define BTN_ARRIBA 26
#define BTN_ABAJO 27
#define BTN_ACEPTAR 14
#define BTN_ATRAS 13

// ======================================================
// 4. OBJETOS
// ======================================================

Adafruit_BMP280 bmp;
Adafruit_INA219 ina219;

LiquidCrystal_I2C lcd(0x27, 16, 2);

WiFiClient clienteMQTT;
PubSubClient mqtt(clienteMQTT);

WiFiClientSecure clienteTelegram;
UniversalTelegramBot* bot = nullptr;

Preferences preferences;

// ======================================================
// 5. CONFIGURACION DEL SISTEMA
// ======================================================

struct Configuracion {

  int offsetGMT = -3;

  float temperaturaMax = 35.0;
  float temperaturaCritica = 45.0;

  float gasAlerta = 60.0;
  float gasPeligro = 80.0;

  float luzMinima = 20.0;

  float corrienteMax = 1000.0;

  uint32_t intervaloMQTT = 10000;

};

Configuracion config;

// ======================================================
// 6. VARIABLES DE SENSORES
// ======================================================

float temperatura = NAN;
float presion = NAN;
float humedad = NAN;

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

// ======================================================
// 7. MAQUINA DE ESTADOS
// ======================================================

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
EstadoAmbiental estadoAnterior = NORMAL;

// ======================================================
// 8. TEMPORIZADORES
// ======================================================

const unsigned long INTERVALO_LECTURA = 2000;
const unsigned long INTERVALO_TELEGRAM = 3000;
const unsigned long INTERVALO_LCD = 2500;

unsigned long ultimaLectura = 0;
unsigned long ultimoEnvioMQTT = 0;
unsigned long ultimaPantalla = 0;
unsigned long ultimaConsultaTelegram = 0;

// ======================================================
// 9. MENU
// ======================================================

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

// ======================================================
// 10. CONFIGURACION EEPROM / NVS
// ======================================================

void cargarConfiguracion() {

  preferences.begin("config", false);

  config.offsetGMT =
    preferences.getInt("gmt", -3);

  config.temperaturaMax =
    preferences.getFloat("tmax", 35.0);

  config.temperaturaCritica =
    preferences.getFloat("tcrit", 45.0);

  config.gasAlerta =
    preferences.getFloat("galerta", 60.0);

  config.gasPeligro =
    preferences.getFloat("gpeligro", 80.0);

  config.luzMinima =
    preferences.getFloat("luz", 20.0);

  config.corrienteMax =
    preferences.getFloat("corr", 1000.0);

  config.intervaloMQTT =
    preferences.getUInt("mqtt", 10000);

  config.intervaloMQTT =
    constrain(config.intervaloMQTT, 5000UL, 300000UL);

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

// ======================================================
// 11. INICIALIZACION DE SENSORES
// ======================================================

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

// ======================================================
// 12. INICIALIZACION DE ENTRADAS Y SALIDAS
// ======================================================

void controlarRele(bool activar);

void iniciarEntradasSalidas() {

  pinMode(RELE_PIN, OUTPUT);

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);

  pinMode(PC817_PIN, INPUT_PULLUP);

  pinMode(HLK_OUT_PIN, INPUT);

  pinMode(BTN_MENU, INPUT_PULLUP);
  pinMode(BTN_ARRIBA, INPUT_PULLUP);
  pinMode(BTN_ABAJO, INPUT_PULLUP);
  pinMode(BTN_ACEPTAR, INPUT_PULLUP);
  pinMode(BTN_ATRAS, INPUT_PULLUP);

  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AMARILLO, LOW);
  digitalWrite(LED_ROJO, LOW);

  controlarRele(false);

}

// ======================================================
// 13. CONTROL DEL RELE
// ======================================================

// Cambiar a true si el circuito se activa con LOW.

const bool RELE_ACTIVO_EN_LOW = false;

void controlarRele(bool activar) {

  bool nivel = activar;

  if (RELE_ACTIVO_EN_LOW) {
    nivel = !nivel;
  }

  digitalWrite(RELE_PIN, nivel ? HIGH : LOW);

}

// ======================================================
// 14. LECTURA DE SENSORES
// ======================================================

// Valores iniciales de referencia.
// Deben calibrarse experimentalmente.

const int GAS_ADC_MIN = 500;
const int GAS_ADC_MAX = 3000;

const int LUZ_ADC_MIN = 0;
const int LUZ_ADC_MAX = 4095;

float limitarPorcentaje(float valor) {

  return constrain(valor, 0.0f, 100.0f);

}

float mapearPorcentaje(int lectura, int minimo, int maximo) {

  if (maximo <= minimo) {
    return 0;
  }

  float resultado =
    (lectura - minimo) * 100.0f / (maximo - minimo);

  return limitarPorcentaje(resultado);

}

void leerSensores() {

  // BMP280

  if (bmpDisponible) {

    temperatura = bmp.readTemperature();

    presion = bmp.readPressure() / 100.0;

  }

  // MQ2 numero 1 y 2

  int lecturaGas1 = analogRead(MQ2_1_PIN);

  int lecturaGas2 = analogRead(MQ2_2_PIN);

  gas1 = mapearPorcentaje(
    lecturaGas1, GAS_ADC_MIN, GAS_ADC_MAX
  );

  gas2 = mapearPorcentaje(
    lecturaGas2, GAS_ADC_MIN, GAS_ADC_MAX
  );

  gas = max(gas1, gas2);

  // LDR

  int lecturaLuz = analogRead(LDR_PIN);

  luz = mapearPorcentaje(
    lecturaLuz, LUZ_ADC_MIN, LUZ_ADC_MAX
  );

  // INA219

  if (inaDisponible) {

    corriente = ina219.getCurrent_mA();

    tension = ina219.getBusVoltage_V();

    potencia = ina219.getPower_mW();

  }

  // PC817
  // Suposicion: entrada activa en LOW.

  opto = digitalRead(PC817_PIN) == LOW;

  // HLK-LD2410C
  // Solamente se utiliza la salida digital OUT.
  // HIGH = presencia detectada.

  presencia = digitalRead(HLK_OUT_PIN) == HIGH;

  // No hay sensor de humedad instalado.

  humedad = NAN;

}

// ======================================================
// 15. FECHA Y HORA
// ======================================================

void iniciarHora() {

  configTime(
    config.offsetGMT * 3600,
    0,
    "pool.ntp.org",
    "time.google.com"
  );

}

void actualizarFechaHora() {

  time_t ahora = time(nullptr);

  if (ahora < 100000) {

    fechaHora = "Sin sincronizar";

    return;

  }

  struct tm tiempo;

  localtime_r(&ahora, &tiempo);

  char buffer[25];

  strftime(
    buffer,
    sizeof(buffer),
    "%Y-%m-%d %H:%M:%S",
    &tiempo
  );

  fechaHora = String(buffer);

}

// ======================================================
// 16. EVALUACION DE UMBRALES
// ======================================================

void evaluarAmbiente() {

  estadoAmbiental = NORMAL;

  bool alertaTemperatura =
    bmpDisponible &&
    temperatura > config.temperaturaMax;

  bool alertaGas =
    gas >= config.gasAlerta;

  bool alertaLuz =
    luz < config.luzMinima;

  bool alertaCorriente =
    inaDisponible &&
    corriente > config.corrienteMax;

  if (
    alertaTemperatura ||
    alertaGas ||
    alertaLuz ||
    alertaCorriente
  ) {

    estadoAmbiental = ALERTA;

  }

  bool peligroTemperatura =
    bmpDisponible &&
    temperatura >= config.temperaturaCritica;

  bool peligroGas =
    gas >= config.gasPeligro;

  if (peligroTemperatura || peligroGas) {

    estadoAmbiental = PELIGRO;

  }

}

// ======================================================
// 17. CONTROL DE LEDS Y RELE
// ======================================================

void actualizarActuadores() {

  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AMARILLO, LOW);
  digitalWrite(LED_ROJO, LOW);

  if (estadoAmbiental == NORMAL) {

    digitalWrite(LED_VERDE, HIGH);

    controlarRele(false);

  }

  else if (estadoAmbiental == ALERTA) {

    digitalWrite(LED_AMARILLO, HIGH);

    controlarRele(false);

  }

  else {

    digitalWrite(LED_ROJO, HIGH);

    controlarRele(true);

  }

}

// ======================================================
// 18. PANTALLA LCD
// ======================================================

void mostrarPantalla() {

  if (menuActivo) return;

  if (
    millis() - ultimaPantalla <
    INTERVALO_LCD
  ) return;

  ultimaPantalla = millis();

  static int pagina = 0;

  pagina = (pagina + 1) % 6;

  lcd.clear();

  switch (pagina) {

    case 0:

      if (estadoAmbiental == NORMAL)
        lcd.print("ESTADO: NORMAL");

      if (estadoAmbiental == ALERTA)
        lcd.print("ESTADO: ALERTA");

      if (estadoAmbiental == PELIGRO)
        lcd.print("ESTADO: PELIGRO");

      lcd.setCursor(0, 1);

      lcd.print("TEMP:");

      if (bmpDisponible)
        lcd.print(temperatura, 1);
      else
        lcd.print("--");

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

      if (inaDisponible)
        lcd.print(corriente, 1);
      else
        lcd.print("SIN INA219");

      if (inaDisponible)
        lcd.print(" mA");

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

      lcd.print(
        WiFi.status() == WL_CONNECTED ? "OK" : "NO"
      );

      lcd.setCursor(0, 1);

      lcd.print("MQTT:");

      lcd.print(mqtt.connected() ? "OK" : "NO");

      break;

    case 5:

      lcd.print("HUMEDAD:");

      lcd.setCursor(0, 1);

      lcd.print("SIN SENSOR");

      break;

  }

}

// ======================================================
// 19. BOTONES
// ======================================================

bool botonPresionado(int pin) {

  static unsigned long ultimoCambio[40] = {0};

  static bool estadoAnteriorBoton[40] = {false};

  bool estadoActual =
    digitalRead(pin) == LOW;

  bool evento = false;

  if (
    estadoActual &&
    !estadoAnteriorBoton[pin] &&
    millis() - ultimoCambio[pin] > 180
  ) {

    evento = true;

    ultimoCambio[pin] = millis();

  }

  estadoAnteriorBoton[pin] = estadoActual;

  return evento;

}

// ======================================================
// 20. VALORES DEL MENU
// ======================================================

float obtenerValorMenu() {

  switch (opcionMenu) {

    case 0: return config.offsetGMT;

    case 1: return config.temperaturaMax;

    case 2: return config.temperaturaCritica;

    case 3: return config.gasAlerta;

    case 4: return config.gasPeligro;

    case 5: return config.luzMinima;

    case 6: return config.corrienteMax;

    case 7: return config.intervaloMQTT / 1000.0;

  }

  return 0;

}

// ======================================================
// 21. MODIFICAR CONFIGURACION
// ======================================================

void modificarValorMenu(int cambio) {

  switch (opcionMenu) {

    case 0:

      config.offsetGMT =
        constrain(config.offsetGMT + cambio, -12, 14);

      break;

    case 1:

      config.temperaturaMax =
        constrain(
          config.temperaturaMax + cambio,
          10.0f, 60.0f
        );

      break;

    case 2:

      config.temperaturaCritica =
        constrain(
          config.temperaturaCritica + cambio,
          15.0f, 80.0f
        );

      break;

    case 3:

      config.gasAlerta =
        constrain(
          config.gasAlerta + cambio,
          0.0f, 100.0f
        );

      break;

    case 4:

      config.gasPeligro =
        constrain(
          config.gasPeligro + cambio,
          0.0f, 100.0f
        );

      break;

    case 5:

      config.luzMinima =
        constrain(
          config.luzMinima + cambio,
          0.0f, 100.0f
        );

      break;

    case 6:

      config.corrienteMax =
        constrain(
          config.corrienteMax + cambio * 100.0f,
          100.0f, 10000.0f
        );

      break;

    case 7:

      if (cambio > 0 &&
          config.intervaloMQTT < 300000UL) {

        config.intervaloMQTT += 1000UL;

      }

      else if (cambio < 0 &&
               config.intervaloMQTT > 5000UL) {

        config.intervaloMQTT -= 1000UL;

      }

      break;

  }

}

// ======================================================
// 22. MOSTRAR MENU
// ======================================================

void mostrarMenu() {

  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print(nombresMenu[opcionMenu]);

  lcd.setCursor(0, 1);

  lcd.print(editando ? "Editar: " : "Valor: ");

  if (opcionMenu == 7) {

    lcd.print(config.intervaloMQTT / 1000);

    lcd.print(" seg");

  }

  else if (opcionMenu == 0) {

    lcd.print(config.offsetGMT);

  }

  else if (opcionMenu == 6) {

    lcd.print(config.corrienteMax, 0);

    lcd.print("mA");

  }

  else {

    lcd.print(obtenerValorMenu(), 1);

  }

}

// ======================================================
// 23. GESTION DEL MENU
// ======================================================

void gestionarMenu() {

  if (botonPresionado(BTN_MENU)) {

    if (!menuActivo) {

      menuActivo = true;

      editando = false;

      opcionMenu = 0;

      mostrarMenu();

    }

    else if (!editando) {

      menuActivo = false;

      guardarConfiguracion();

      iniciarHora();

      lcd.clear();

    }

  }

  if (!menuActivo) return;

  if (botonPresionado(BTN_ARRIBA)) {

    if (editando) {

      modificarValorMenu(1);

    }

    else {

      opcionMenu =
        (opcionMenu + 1) % CANT_OPCIONES;

    }

    mostrarMenu();

  }

  if (botonPresionado(BTN_ABAJO)) {

    if (editando) {

      modificarValorMenu(-1);

    }

    else {

      opcionMenu--;

      if (opcionMenu < 0)
        opcionMenu = CANT_OPCIONES - 1;

    }

    mostrarMenu();

  }

  if (botonPresionado(BTN_ACEPTAR)) {

    editando = !editando;

    if (!editando) {

      guardarConfiguracion();

      iniciarHora();

    }

    mostrarMenu();

  }

  if (botonPresionado(BTN_ATRAS)) {

    if (editando) {

      editando = false;

      mostrarMenu();

    }

    else {

      menuActivo = false;

      guardarConfiguracion();

      iniciarHora();

      lcd.clear();

    }

  }

}

// ======================================================
// 24. CONEXION WIFI
// ======================================================

void iniciarWiFi() {

  WiFi.mode(WIFI_STA);

  WiFiManager wifiManager;

  wifiManager.setConfigPortalTimeout(180);

  // Si no hay credenciales guardadas,
  // crea el portal TP-IOT-CONFIG.

  bool conectado =
    wifiManager.autoConnect("TP-IOT-CONFIG");

  if (conectado) {

    Serial.println("WiFi conectado");

    Serial.println(WiFi.localIP());

  }

  else {

    Serial.println("No se pudo conectar al WiFi");

  }

}

// ======================================================
// 25. CONEXION MQTT
// ======================================================

void conectarMQTT() {

  if (mqtt.connected()) return;

  if (WiFi.status() != WL_CONNECTED) return;

  if (mqtt.connect(MQTT_CLIENT_ID)) {

    Serial.println("MQTT conectado");

  }

  else {

    Serial.println("Error de conexion MQTT");

  }

}

// ======================================================
// 26. PUBLICAR DATOS MQTT
// ======================================================

void publicarMQTT() {

  if (!mqtt.connected()) return;

  StaticJsonDocument<512> documento;

  if (bmpDisponible) {

    documento["temperatura"] = temperatura;

    documento["presion"] = presion;

  }

  else {

    documento["temperatura"] = nullptr;

    documento["presion"] = nullptr;

  }

  // No existe sensor de humedad.

  documento["humedad"] = nullptr;

  documento["luz"] = luz;

  documento["gas"] = gas;

  documento["gas1"] = gas1;

  documento["gas2"] = gas2;

  if (inaDisponible) {

    documento["corriente"] = corriente;

    documento["tension"] = tension;

    documento["potencia"] = potencia;

  }

  else {

    documento["corriente"] = nullptr;

    documento["tension"] = nullptr;

    documento["potencia"] = nullptr;

  }

  documento["opto"] = opto ? 1 : 0;

  documento["presencia"] = presencia ? 1 : 0;

  documento["fecha_hora"] = fechaHora;

  if (estadoAmbiental == NORMAL)
    documento["estado"] = "NORMAL";

  else if (estadoAmbiental == ALERTA)
    documento["estado"] = "ALERTA";

  else
    documento["estado"] = "PELIGRO";

  char mensaje[512];

  serializeJson(documento, mensaje, sizeof(mensaje));

  mqtt.publish(MQTT_TOPIC, mensaje);

  Serial.println("Datos enviados por MQTT");

}

// ======================================================
// 27. TELEGRAM: CONSULTAR ESTADO
// ======================================================

String obtenerEstadoTexto() {

  String mensaje = "MONITOREO AMBIENTAL\n\n";

  mensaje += "Temperatura: ";

  mensaje += bmpDisponible ?
    String(temperatura, 1) + " C\n" :
    "No disponible\n";

  mensaje += "Humedad: Sin sensor instalado\n";

  mensaje += "Presion: ";

  mensaje += bmpDisponible ?
    String(presion, 1) + " hPa\n" :
    "No disponible\n";

  mensaje += "Gas: " + String(gas, 1) + " %\n";

  mensaje += "Luz: " + String(luz, 1) + " %\n";

  mensaje += "Corriente: ";

  mensaje += inaDisponible ?
    String(corriente, 1) + " mA\n" :
    "No disponible\n";

  mensaje += "Opto: ";

  mensaje += opto ? "ACTIVO\n" : "INACTIVO\n";

  mensaje += "Presencia: ";

  mensaje += presencia ? "SI\n" : "NO\n";

  mensaje += "Fecha: " + fechaHora + "\n";

  mensaje += "Estado: ";

  if (estadoAmbiental == NORMAL)
    mensaje += "NORMAL";

  else if (estadoAmbiental == ALERTA)
    mensaje += "ALERTA";

  else
    mensaje += "PELIGRO";

  return mensaje;

}

// ======================================================
// 28. TELEGRAM: ALERTAS
// ======================================================

void enviarCambioEstadoTelegram() {

  if (bot == nullptr) return;

  if (WiFi.status() != WL_CONNECTED) return;

  if (estadoAmbiental == estadoAnterior) return;

  String mensaje;

  if (estadoAmbiental == ALERTA) {

    mensaje = "ALERTA: Se supero uno o mas umbrales.";

  }

  else if (estadoAmbiental == PELIGRO) {

    mensaje = "PELIGRO: Se detecto una condicion critica.";

  }

  else {

    mensaje = "El ambiente volvio al estado NORMAL.";

  }

  if (bot->sendMessage(CHAT_ID, mensaje, "")) {

    estadoAnterior = estadoAmbiental;

  }

}

// ======================================================
// 29. TELEGRAM: COMANDOS
// ======================================================

void consultarTelegram() {

  if (bot == nullptr) return;

  if (WiFi.status() != WL_CONNECTED) return;

  if (
    millis() - ultimaConsultaTelegram <
    INTERVALO_TELEGRAM
  ) return;

  ultimaConsultaTelegram = millis();

  int cantidad =
    bot->getUpdates(bot->last_message_received + 1);

  while (cantidad > 0) {

    for (int i = 0; i < cantidad; i++) {

      if (
        bot->messages[i].chat_id !=
        String(CHAT_ID)
      ) continue;

      String texto = bot->messages[i].text;

      if (texto == "/start") {

        bot->sendMessage(
          CHAT_ID,
          "Sistema de monitoreo activo.\nUsa /estado para consultar.",
          ""
        );

      }

      else if (texto == "/estado") {

        bot->sendMessage(
          CHAT_ID,
          obtenerEstadoTexto(),
          ""
        );

      }

    }

    cantidad =
      bot->getUpdates(bot->last_message_received + 1);

  }

}

// ======================================================
// 30. SETUP
// ======================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println("Iniciando monitoreo ambiental");

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

  mqtt.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );

  if (
    String(BOT_TOKEN) != "COLOCAR_TOKEN_NUEVO" &&
    String(CHAT_ID) != "COLOCAR_CHAT_ID"
  ) {

    clienteTelegram.setInsecure();

    bot = new UniversalTelegramBot(
      BOT_TOKEN,
      clienteTelegram
    );

  }

  delay(1500);

  lcd.clear();

  estadoMaquina = ESTADO_LECTURA;

}

// ======================================================
// 31. LOOP PRINCIPAL
// ======================================================

void loop() {

  // Comunicacion WiFi y MQTT.

  if (WiFi.status() == WL_CONNECTED) {

    if (!mqtt.connected()) {

      conectarMQTT();

    }

    mqtt.loop();

  }

  // Gestion del menu.

  gestionarMenu();

  if (menuActivo) return;

  // Consultas de Telegram.

  consultarTelegram();

  // Maquina de estados.

  switch (estadoMaquina) {

    case ESTADO_INICIO:

      estadoMaquina = ESTADO_CONEXION;

      break;

    case ESTADO_CONEXION:

      estadoMaquina = ESTADO_LECTURA;

      break;

    case ESTADO_LECTURA:

      if (
        millis() - ultimaLectura >=
        INTERVALO_LECTURA
      ) {

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

      if (
        millis() - ultimoEnvioMQTT >=
        config.intervaloMQTT
      ) {

        ultimoEnvioMQTT = millis();

        publicarMQTT();

      }

      estadoMaquina = ESTADO_LECTURA;

      break;

  }

}
