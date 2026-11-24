//PERRUCCI ALBERTONI, SANGUINETTI WALSH, KAPLAN AGOSTI
#include <Wire.h>
#include <Adafruit_INA219.h>


Adafruit_INA219 ina219;


void setup() {
  Serial.begin(115200);
  delay(1000);


  Serial.println("Iniciando prueba INA219...");


  Wire.begin(21, 22);
  Serial.println("I2C iniciado");


  if (!ina219.begin()) {
    Serial.println("ERROR: INA219 no encontrado");
    while (1) {
      delay(1000);
    }
  }


  Serial.println("INA219 conectado correctamente");
}


void loop() {
  Serial.println("Leyendo sensor...");


  Serial.print("Voltaje: ");
  Serial.print(ina219.getBusVoltage_V());
  Serial.println(" V");


  Serial.print("Corriente: ");
  Serial.print(ina219.getCurrent_mA());
  Serial.println(" mA");


  Serial.print("Potencia: ");
  Serial.print(ina219.getPower_mW());
  Serial.println(" mW");


  Serial.println("-------------------");
  delay(1000);
}

