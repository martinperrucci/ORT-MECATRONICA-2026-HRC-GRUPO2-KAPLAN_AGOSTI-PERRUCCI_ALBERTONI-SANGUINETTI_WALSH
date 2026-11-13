#define LDR 35

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  Serial.println("Prueba LDR iniciada");
}

void loop() {
  int lectura = analogRead(LDR);

  float tension = lectura * 3.3 / 4095.0;

  Serial.print("Lectura: ");
  Serial.print(lectura);

  Serial.print(" | Tension: ");
  Serial.print(tension);
  Serial.println(" V");

  delay(500);
}
//Martin Perrucci Albertoni, Martin Kaplan Agosti, Renata Sanguinetti Walsh; grupo 2
