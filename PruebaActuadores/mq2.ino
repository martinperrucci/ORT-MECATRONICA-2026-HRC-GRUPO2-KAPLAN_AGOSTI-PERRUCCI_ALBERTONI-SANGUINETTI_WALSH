//KAPLAN AGOSTI, SANGUINETTI WALSH, PERRUCCI ALBERTONI
#define MQ2 34

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetPinAttenuation(MQ2, ADC_11db);

  Serial.println("Prueba MQ-2 iniciada");
}

void loop() {
  int lectura = analogRead(MQ2);

  float tensionADC = lectura * 3.3 / 4095.0;
  float tensionSensor = tensionADC * 1.5;

  Serial.print("Lectura ADC: ");
  Serial.println(lectura);

  Serial.print("Tension aproximada del sensor: ");
  Serial.print(tensionSensor);
  Serial.println(" V");

  delay(1000);
}

