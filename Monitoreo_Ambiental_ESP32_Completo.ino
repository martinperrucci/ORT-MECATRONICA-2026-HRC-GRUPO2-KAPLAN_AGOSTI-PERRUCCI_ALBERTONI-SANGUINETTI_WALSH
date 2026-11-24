
/*
  SISTEMA DE MONITOREO AMBIENTAL
  TEST INDIVIDUAL DE COMPONENTES
  ESP32 - TP INTEGRADOR

  Sensores:
  BMP280
  INA219
  MQ-2 x2
  LDR
  PC817
  HLK-LD2410C (SALIDA DIGITAL OUT)

  Actuadores:
  LCD I2C 16x2
  RELE
  3 LEDs
  5 PULSADORES
*/

#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_INA219.h>
#include <LiquidCrystal_I2C.h>

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
// 2. OBJETOS
// ==================================================

Adafruit_BMP280 bmp;
Adafruit_INA219 ina219;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==================================================
// 3. VARIABLES
// ==================================================

bool bmpDisponible = false;
bool inaDisponible = false;

unsigned long ultimaLectura = 0;
unsigned long ultimaPantalla = 0;
unsigned long ultimaPruebaActuadores = 0;

const unsigned long INTERVALO_LECTURA = 2000;
const unsigned long INTERVALO_LCD = 3000;

// ==================================================
// 4. INICIALIZAR SENSORES
// ==================================================

void iniciarSensores() {

  Wire.begin(SDA_PIN, SCL_PIN);

  // BMP280

  bmpDisponible = bmp.begin(0x76);

  if (!bmpDisponible) {
    bmpDisponible = bmp.begin(0x77);
  }

  // INA219

  inaDisponible = ina219.begin();

  // Resolucion ADC

  analogReadResolution(12);

  analogSetPinAttenuation(MQ2_1_PIN, ADC_11db);
  analogSetPinAttenuation(MQ2_2_PIN, ADC_11db);
  analogSetPinAttenuation(LDR_PIN, ADC_11db);

  Serial.println("\n========== SENSORES ==========");

  Serial.print("BMP280: ");
  Serial.println(bmpDisponible ? "OK" : "NO DETECTADO");

  Serial.print("INA219: ");
  Serial.println(inaDisponible ? "OK" : "NO DETECTADO");

}

// ==================================================
// 5. INICIALIZAR ENTRADAS Y SALIDAS
// ==================================================

void iniciarPines() {

  pinMode(PC817_PIN, INPUT_PULLUP);
  pinMode(HLK_OUT_PIN, INPUT);

  pinMode(RELE_PIN, OUTPUT);

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);

  pinMode(BTN_MENU, INPUT_PULLUP);
  pinMode(BTN_ARRIBA, INPUT_PULLUP);
  pinMode(BTN_ABAJO, INPUT_PULLUP);
  pinMode(BTN_ACEPTAR, INPUT_PULLUP);
  pinMode(BTN_ATRAS, INPUT_PULLUP);

  digitalWrite(RELE_PIN, LOW);

  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AMARILLO, LOW);
  digitalWrite(LED_ROJO, LOW);

}

// ==================================================
// 6. LECTURA DEL BMP280
// ==================================================

void probarBMP280() {

  Serial.println("\n--- BMP280 ---");

  if (bmpDisponible) {

    float temperatura = bmp.readTemperature();

    float presion = bmp.readPressure() / 100.0;

    Serial.print("Temperatura: ");
    Serial.print(temperatura);
    Serial.println(" C");

    Serial.print("Presion: ");
    Serial.print(presion);
    Serial.println(" hPa");

  }

  else {

    Serial.println("BMP280 no disponible");

  }

}

// ==================================================
// 7. LECTURA DEL INA219
// ==================================================

void probarINA219() {

  Serial.println("\n--- INA219 ---");

  if (inaDisponible) {

    float corriente = ina219.getCurrent_mA();

    float tension = ina219.getBusVoltage_V();

    float potencia = ina219.getPower_mW();

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

  else {

    Serial.println("INA219 no disponible");

  }

}

// ==================================================
// 8. LECTURA DE LOS MQ-2
// ==================================================

void probarMQ2() {

  Serial.println("\n--- MQ-2 ---");

  int gas1 = analogRead(MQ2_1_PIN);

  int gas2 = analogRead(MQ2_2_PIN);

  Serial.print("MQ-2 numero 1: ");
  Serial.println(gas1);

  Serial.print("MQ-2 numero 2: ");
  Serial.println(gas2);

}

// ==================================================
// 9. LECTURA DEL LDR
// ==================================================

void probarLDR() {

  Serial.println("\n--- LDR ---");

  int lectura = analogRead(LDR_PIN);

  Serial.print("Lectura analogica: ");
  Serial.println(lectura);

  float porcentaje = lectura * 100.0 / 4095.0;

  Serial.print("Porcentaje ADC: ");
  Serial.print(porcentaje);
  Serial.println(" %");

}

// ==================================================
// 10. LECTURA DEL PC817
// ==================================================

void probarPC817() {

  Serial.println("\n--- PC817 ---");

  bool estado = digitalRead(PC817_PIN) == LOW;

  Serial.print("Estado: ");

  Serial.println(estado ? "ACTIVO" : "INACTIVO");

}

// ==================================================
// 11. LECTURA DEL HLK-LD2410C
// ==================================================

void probarHLK() {

  Serial.println("\n--- HLK-LD2410C ---");

  bool presencia = digitalRead(HLK_OUT_PIN) == HIGH;

  Serial.print("Presencia: ");

  Serial.println(presencia ? "DETECTADA" : "NO DETECTADA");

}

// ==================================================
// 12. PRUEBA DE LOS PULSADORES
// ==================================================

void probarBotones() {

  Serial.println("\n--- PULSADORES ---");

  if (digitalRead(BTN_MENU) == LOW)
    Serial.println("Boton MENU presionado");

  if (digitalRead(BTN_ARRIBA) == LOW)
    Serial.println("Boton ARRIBA presionado");

  if (digitalRead(BTN_ABAJO) == LOW)
    Serial.println("Boton ABAJO presionado");

  if (digitalRead(BTN_ACEPTAR) == LOW)
    Serial.println("Boton ACEPTAR presionado");

  if (digitalRead(BTN_ATRAS) == LOW)
    Serial.println("Boton ATRAS presionado");

}

// ==================================================
// 13. PRUEBA DE LOS LEDs
// ==================================================

void probarLEDs() {

  static int paso = 0;

  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AMARILLO, LOW);
  digitalWrite(LED_ROJO, LOW);

  switch (paso) {

    case 0:
      digitalWrite(LED_VERDE, HIGH);
      Serial.println("LED VERDE encendido");
      break;

    case 1:
      digitalWrite(LED_AMARILLO, HIGH);
      Serial.println("LED AMARILLO encendido");
      break;

    case 2:
      digitalWrite(LED_ROJO, HIGH);
      Serial.println("LED ROJO encendido");
      break;

  }

  paso = (paso + 1) % 3;

}

// ==================================================
// 14. PRUEBA DEL RELE
// ==================================================

void probarRele() {

  static bool estadoRele = false;

  estadoRele = !estadoRele;

  digitalWrite(RELE_PIN, estadoRele ? HIGH : LOW);

  Serial.print("Rele: ");

  Serial.println(estadoRele ? "ACTIVADO" : "DESACTIVADO");

}

// ==================================================
// 15. PANTALLA LCD
// ==================================================

void probarLCD() {

  static int pagina = 0;

  lcd.clear();

  switch (pagina) {

    case 0:

      lcd.setCursor(0, 0);
      lcd.print("BMP280");

      lcd.setCursor(0, 1);

      if (bmpDisponible) {

        lcd.print(bmp.readTemperature(), 1);
        lcd.print(" C");

      }

      else {

        lcd.print("NO DETECTADO");

      }

      break;

    case 1:

      lcd.setCursor(0, 0);
      lcd.print("INA219");

      lcd.setCursor(0, 1);

      if (inaDisponible) {

        lcd.print(ina219.getCurrent_mA(), 1);
        lcd.print(" mA");

      }

      else {

        lcd.print("NO DETECTADO");

      }

      break;

    case 2:

      lcd.setCursor(0, 0);
      lcd.print("MQ2 1 / MQ2 2");

      lcd.setCursor(0, 1);

      lcd.print(analogRead(MQ2_1_PIN));
      lcd.print("/");
      lcd.print(analogRead(MQ2_2_PIN));

      break;

    case 3:

      lcd.setCursor(0, 0);
      lcd.print("LECTURA LDR");

      lcd.setCursor(0, 1);

      lcd.print(analogRead(LDR_PIN));

      break;

    case 4:

      lcd.setCursor(0, 0);
      lcd.print("PRESENCIA HLK");

      lcd.setCursor(0, 1);

      lcd.print(
        digitalRead(HLK_OUT_PIN) == HIGH ?
        "DETECTADA" : "NO DETECTADA"
      );

      break;

    case 5:

      lcd.setCursor(0, 0);
      lcd.print("PC817");

      lcd.setCursor(0, 1);

      lcd.print(
        digitalRead(PC817_PIN) == LOW ?
        "ACTIVO" : "INACTIVO"
      );

      break;

  }

  pagina = (pagina + 1) % 6;

}

// ==================================================
// 16. SETUP
// ==================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println("\n================================");
  Serial.println(" TEST DE COMPONENTES ESP32");
  Serial.println("================================");

  iniciarPines();

  iniciarSensores();

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("TEST ESP32");

  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");

  delay(2000);

  lcd.clear();

}

// ==================================================
// 17. LOOP PRINCIPAL
// ==================================================

void loop() {

  unsigned long ahora = millis();

  // Lectura de sensores cada 2 segundos

  if (ahora - ultimaLectura >= INTERVALO_LECTURA) {

    ultimaLectura = ahora;

    Serial.println("\n================================");

    probarBMP280();

    probarINA219();

    probarMQ2();

    probarLDR();

    probarPC817();

    probarHLK();

    probarBotones();

    Serial.println("\n================================");

  }

  // Actualizacion de LCD cada 3 segundos

  if (ahora - ultimaPantalla >= INTERVALO_LCD) {

    ultimaPantalla = ahora;

    probarLCD();

  }

  // LEDs y rele: secuencia de prueba cada segundo

  if (ahora - ultimaPruebaActuadores >= 1000) {

    ultimaPruebaActuadores = ahora;

    probarLEDs();

    probarRele();

  }

}
