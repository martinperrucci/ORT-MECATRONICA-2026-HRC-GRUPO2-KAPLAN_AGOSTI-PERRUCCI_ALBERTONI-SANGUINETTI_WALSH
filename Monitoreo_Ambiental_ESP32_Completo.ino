
/*
  SISTEMA DE MONITOREO AMBIENTAL
  TEST MEJORADO DE COMPONENTES
  ESP32 - TP INTEGRADOR

  Sensores:
  - BMP280
  - INA219
  - MQ-2 x2
  - LDR
  - PC817
  - HLK-LD2410C (salida digital OUT)

  Actuadores:
  - LCD I2C 16x2
  - Rele
  - 3 LEDs
  - 5 pulsadores
*/

#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_INA219.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

// ==================================================
// 1. PINES
// ==================================================

// I2C
#define SDA_PIN 21
#define SCL_PIN 22

// Sensores analogicos
#define MQ2_1_PIN 34
#define MQ2_2_PIN 35
#define LDR_PIN 32

// Sensores digitales
#define PC817_PIN 19
#define HLK_OUT_PIN 23

// Rele
#define RELE_PIN 33

// LEDs
#define LED_VERDE 16
#define LED_AMARILLO 17
#define LED_ROJO 18

// Pulsadores
#define BTN_MENU 25
#define BTN_ARRIBA 26
#define BTN_ABAJO 27
#define BTN_ACEPTAR 14
#define BTN_ATRAS 13

// ==================================================
// 2. CONFIGURACION
// ==================================================

#define RELE_ACTIVO HIGH
#define RELE_INACTIVO LOW

const unsigned long INTERVALO_LECTURA = 2000;
const unsigned long INTERVALO_LCD = 3000;
const unsigned long INTERVALO_LED = 1500;
const unsigned long INTERVALO_RELE = 3000;
const unsigned long ANTIRREBOTE = 50;

const int CANTIDAD_MUESTRAS = 10;

// ==================================================
// 3. OBJETOS
// ==================================================

Adafruit_BMP280 bmp;
Adafruit_INA219 ina219;

LiquidCrystal_I2C *lcd = nullptr;

// ==================================================
// 4. VARIABLES
// ==================================================

bool bmpDisponible = false;
bool inaDisponible = false;
bool lcdDisponible = false;

int direccionLCD = 0;

unsigned long ultimaLectura = 0;
unsigned long ultimaPantalla = 0;
unsigned long ultimoLED = 0;
unsigned long ultimoRele = 0;

bool estadoRele = false;

int pasoLED = 0;
int paginaLCD = 0;

// Variables para pulsadores
const int pinesBotones[5] = {
  BTN_MENU,
  BTN_ARRIBA,
  BTN_ABAJO,
  BTN_ACEPTAR,
  BTN_ATRAS
};

const char* nombresBotones[5] = {
  "MENU",
  "ARRIBA",
  "ABAJO",
  "ACEPTAR",
  "ATRAS"
};

bool estadosAnteriores[5] = {
  HIGH, HIGH, HIGH, HIGH, HIGH
};

unsigned long ultimoCambio[5] = {
  0, 0, 0, 0, 0
};

// ==================================================
// 5. DETECCION DE DISPOSITIVOS I2C
// ==================================================

bool dispositivoI2C(int direccion) {

  Wire.beginTransmission(direccion);

  return Wire.endTransmission() == 0;
}

void detectarDispositivosI2C() {

  Serial.println("\n--- ESCANEO I2C ---");

  Serial.print("BMP280: ");

  bmpDisponible = bmp.begin(0x76);

  if (!bmpDisponible) {
    bmpDisponible = bmp.begin(0x77);
  }

  Serial.println(
    bmpDisponible ? "DETECTADO" : "NO DETECTADO"
  );

  Serial.print("INA219: ");

  inaDisponible = ina219.begin();

  Serial.println(
    inaDisponible ? "DETECTADO" : "NO DETECTADO"
  );

  Serial.print("LCD: ");

  if (dispositivoI2C(0x27)) {
    direccionLCD = 0x27;
  }
  else if (dispositivoI2C(0x3F)) {
    direccionLCD = 0x3F;
  }

  if (direccionLCD != 0) {

    lcdDisponible = true;

    lcd = new LiquidCrystal_I2C(
      direccionLCD, 16, 2
    );

    lcd->init();
    lcd->backlight();

    Serial.print("DETECTADA EN 0x");
    Serial.println(direccionLCD, HEX);

  }
  else {

    lcdDisponible = false;

    Serial.println("NO DETECTADA");

  }

}

// ==================================================
// 6. INICIALIZACION DE PINES
// ==================================================

void iniciarPines() {

  pinMode(PC817_PIN, INPUT_PULLUP);
  pinMode(HLK_OUT_PIN, INPUT);

  pinMode(RELE_PIN, OUTPUT);

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);

  for (int i = 0; i < 5; i++) {
    pinMode(pinesBotones[i], INPUT_PULLUP);
  }

  digitalWrite(RELE_PIN, RELE_INACTIVO);

  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AMARILLO, LOW);
  digitalWrite(LED_ROJO, LOW);

}

// ==================================================
// 7. CONFIGURACION DE SENSORES
// ==================================================

void iniciarSensores() {

  Wire.begin(SDA_PIN, SCL_PIN);

  analogReadResolution(12);

  analogSetPinAttenuation(MQ2_1_PIN, ADC_11db);
  analogSetPinAttenuation(MQ2_2_PIN, ADC_11db);
  analogSetPinAttenuation(LDR_PIN, ADC_11db);

  detectarDispositivosI2C();

}

// ==================================================
// 8. LECTURA ANALOGICA PROMEDIADA
// ==================================================

int leerPromedio(int pin) {

  long suma = 0;

  for (int i = 0; i < CANTIDAD_MUESTRAS; i++) {

    suma += analogRead(pin);

    delay(2);
  }

  return suma / CANTIDAD_MUESTRAS;

}

void mostrarEstadoADC(int lectura) {

  if (lectura <= 5 || lectura >= 4090) {
    Serial.println("REVISAR: lectura en un extremo del ADC");
  }
  else {
    Serial.println("LECTURA ADC DISPONIBLE");
  }

}

// ==================================================
// 9. PRUEBA BMP280
// ==================================================

void probarBMP280() {

  Serial.println("\n--- BMP280 ---");

  if (!bmpDisponible) {
    Serial.println("ERROR: NO DETECTADO");
    return;
  }

  float temperatura = bmp.readTemperature();
  float presion = bmp.readPressure() / 100.0;

  if (!isfinite(temperatura) || !isfinite(presion)) {
    Serial.println("ERROR: LECTURA INVALIDA");
    return;
  }

  Serial.println("ESTADO: OK");

  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" C");

  Serial.print("Presion: ");
  Serial.print(presion);
  Serial.println(" hPa");

}

// ==================================================
// 10. PRUEBA INA219
// ==================================================

void probarINA219() {

  Serial.println("\n--- INA219 ---");

  if (!inaDisponible) {
    Serial.println("ERROR: NO DETECTADO");
    return;
  }

  float corriente = ina219.getCurrent_mA();
  float tension = ina219.getBusVoltage_V();
  float potencia = ina219.getPower_mW();

  if (!isfinite(corriente) ||
      !isfinite(tension) ||
      !isfinite(potencia)) {

    Serial.println("ERROR: LECTURA INVALIDA");
    return;
  }

  Serial.println("ESTADO: OK");

  Serial.print("Corriente: ");
  Serial.print(corriente);
  Serial.println(" mA");

  Serial.print("Tension: ");
  Serial.print(tension);
  Serial.println(" V");

  Serial.print("Potencia: ");
  Serial.print(potencia);
  Serial.println(" mW");

}

// ==================================================
// 11. PRUEBA MQ-2
// ==================================================

void probarMQ2() {

  Serial.println("\n--- SENSORES MQ-2 ---");

  int gas1 = leerPromedio(MQ2_1_PIN);
  int gas2 = leerPromedio(MQ2_2_PIN);

  Serial.print("MQ-2 numero 1: ");
  Serial.println(gas1);

  mostrarEstadoADC(gas1);

  Serial.print("MQ-2 numero 2: ");
  Serial.println(gas2);

  mostrarEstadoADC(gas2);

  Serial.println(
    "NOTA: valores ADC, no concentracion en ppm"
  );

}

// ==================================================
// 12. PRUEBA LDR
// ==================================================

void probarLDR() {

  Serial.println("\n--- LDR ---");

  int lectura = leerPromedio(LDR_PIN);

  float porcentaje = lectura * 100.0 / 4095.0;

  Serial.print("Lectura ADC: ");
  Serial.println(lectura);

  Serial.print("Porcentaje ADC: ");
  Serial.print(porcentaje);
  Serial.println(" %");

  mostrarEstadoADC(lectura);

}

// ==================================================
// 13. PRUEBA PC817
// ==================================================

void probarPC817() {

  Serial.println("\n--- PC817 ---");

  bool estado = digitalRead(PC817_PIN) == LOW;

  Serial.print("Estado: ");

  Serial.println(
    estado ? "ACTIVO" : "INACTIVO"
  );

}

// ==================================================
// 14. PRUEBA HLK-LD2410C
// ==================================================

void probarHLK() {

  Serial.println("\n--- HLK-LD2410C ---");

  bool presencia = digitalRead(HLK_OUT_PIN) == HIGH;

  Serial.print("Presencia: ");

  Serial.println(
    presencia ? "DETECTADA" : "NO DETECTADA"
  );

}

// ==================================================
// 15. PRUEBA DE PULSADORES CON ANTIRREBOTE
// ==================================================

void probarBotones() {

  for (int i = 0; i < 5; i++) {

    bool lectura = digitalRead(pinesBotones[i]);

    if (lectura != estadosAnteriores[i]) {

      ultimoCambio[i] = millis();

      estadosAnteriores[i] = lectura;
    }

    if (millis() - ultimoCambio[i] >= ANTIRREBOTE) {

      static bool yaInformado[5] = {
        false, false, false, false, false
      };

      if (lectura == LOW && !yaInformado[i]) {

        Serial.print("BOTON PRESIONADO: ");
        Serial.println(nombresBotones[i]);

        yaInformado[i] = true;
      }

      if (lectura == HIGH) {
        yaInformado[i] = false;
      }
    }
  }
}

// ==================================================
// 16. PRUEBA DE LOS LEDS
// ==================================================

void probarLEDs() {

  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AMARILLO, LOW);
  digitalWrite(LED_ROJO, LOW);

  switch (pasoLED) {

    case 0:

      digitalWrite(LED_VERDE, HIGH);

      Serial.println("LED VERDE: ORDEN DE ENCENDIDO");

      break;

    case 1:

      digitalWrite(LED_AMARILLO, HIGH);

      Serial.println("LED AMARILLO: ORDEN DE ENCENDIDO");

      break;

    case 2:

      digitalWrite(LED_ROJO, HIGH);

      Serial.println("LED ROJO: ORDEN DE ENCENDIDO");

      break;

  }

  pasoLED = (pasoLED + 1) % 3;

}

// ==================================================
// 17. PRUEBA DEL RELE
// ==================================================

void probarRele() {

  estadoRele = !estadoRele;

  digitalWrite(
    RELE_PIN,
    estadoRele ? RELE_ACTIVO : RELE_INACTIVO
  );

  Serial.print("RELE: ");

  Serial.println(
    estadoRele ? "ORDEN DE ACTIVACION" :
                 "ORDEN DE DESACTIVACION"
  );

}

// ==================================================
// 18. PRUEBA DE LCD
// ==================================================

void probarLCD() {

  if (!lcdDisponible || lcd == nullptr) {
    return;
  }

  lcd->clear();

  switch (paginaLCD) {

    case 0:

      lcd->setCursor(0, 0);
      lcd->print("BMP280");

      lcd->setCursor(0, 1);

      if (bmpDisponible) {
        lcd->print(bmp.readTemperature(), 1);
        lcd->print(" C");
      }
      else {
        lcd->print("NO DETECTADO");
      }

      break;

    case 1:

      lcd->setCursor(0, 0);
      lcd->print("INA219");

      lcd->setCursor(0, 1);

      if (inaDisponible) {
        lcd->print(ina219.getCurrent_mA(), 1);
        lcd->print(" mA");
      }
      else {
        lcd->print("NO DETECTADO");
      }

      break;

    case 2:

      lcd->setCursor(0, 0);
      lcd->print("MQ2 1 / MQ2 2");

      lcd->setCursor(0, 1);
      lcd->print(leerPromedio(MQ2_1_PIN));
      lcd->print("/");
      lcd->print(leerPromedio(MQ2_2_PIN));

      break;

    case 3:

      lcd->setCursor(0, 0);
      lcd->print("LECTURA LDR");

      lcd->setCursor(0, 1);
      lcd->print(leerPromedio(LDR_PIN));

      break;

    case 4:

      lcd->setCursor(0, 0);
      lcd->print("PRESENCIA HLK");

      lcd->setCursor(0, 1);

      lcd->print(
        digitalRead(HLK_OUT_PIN) == HIGH ?
        "DETECTADA" : "NO DETECTADA"
      );

      break;

    case 5:

      lcd->setCursor(0, 0);
      lcd->print("PC817");

      lcd->setCursor(0, 1);

      lcd->print(
        digitalRead(PC817_PIN) == LOW ?
        "ACTIVO" : "INACTIVO"
      );

      break;

  }

  paginaLCD = (paginaLCD + 1) % 6;

}

// ==================================================
// 19. INFORME GENERAL
// ==================================================

void informeGeneral() {

  Serial.println("\n");
  Serial.println("================================");
  Serial.println("       INFORME DE SENSORES");
  Serial.println("================================");

  probarBMP280();
  probarINA219();
  probarMQ2();
  probarLDR();
  probarPC817();
  probarHLK();

  Serial.println("\n================================");
  Serial.println("        FIN DEL INFORME");
  Serial.println("================================");

}

// ==================================================
// 20. SETUP
// ==================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println("\n================================");
  Serial.println("   TEST MEJORADO ESP32");
  Serial.println("================================");

  iniciarPines();

  iniciarSensores();

  if (lcdDisponible) {

    lcd->setCursor(0, 0);
    lcd->print("TEST ESP32");

    lcd->setCursor(0, 1);
    lcd->print("Iniciando...");

  }

  Serial.println("\nSistema de prueba iniciado");

  Serial.println(
    "Los sensores MQ-2 necesitan calentamiento."
  );

  delay(2000);

  if (lcdDisponible) {
    lcd->clear();
  }

}

// ==================================================
// 21. LOOP PRINCIPAL
// ==================================================

void loop() {

  unsigned long ahora = millis();

  // Informe de sensores cada 2 segundos

  if (ahora - ultimaLectura >= INTERVALO_LECTURA) {

    ultimaLectura = ahora;

    informeGeneral();

  }

  // Actualizacion LCD

  if (ahora - ultimaPantalla >= INTERVALO_LCD) {

    ultimaPantalla = ahora;

    probarLCD();

  }

  // Prueba secuencial de LEDs

  if (ahora - ultimoLED >= INTERVALO_LED) {

    ultimoLED = ahora;

    probarLEDs();

  }

  // Prueba del rele

  if (ahora - ultimoRele >= INTERVALO_RELE) {

    ultimoRele = ahora;

    probarRele();

  }

  // Lectura de botones

  probarBotones();

}
