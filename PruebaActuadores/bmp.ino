//KAPLAN AGOSTI, PERRUCCI ALBERTONI, SANGUINETTI WALSH
#include <Wire.h>
#include <Adafruit_BMP280.h>

Adafruit_BMP280 bmp;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!bmp.begin(0x76)) {
    Serial.println("Error: BMP280 no encontrado");
    while (1);
  }

  Serial.println("BMP280 funcionando correctamente");
}

void loop() {
  Serial.print("Temperatura: ");
  Serial.print(bmp.readTemperature());
  Serial.println(" °C");

  Serial.print("Presion: ");
  Serial.print(bmp.readPressure() / 100.0);
  Serial.println(" hPa");

  Serial.print("Altitud aproximada: ");
  Serial.print(bmp.readAltitude(1013.25));
  Serial.println(" m");

  Serial.println("-------------------");
  delay(2000);
}

