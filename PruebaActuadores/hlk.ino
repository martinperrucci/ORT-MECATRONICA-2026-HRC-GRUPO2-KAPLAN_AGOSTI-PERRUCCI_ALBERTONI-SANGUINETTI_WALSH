//SANGUINETTI WALSH, PERRUCCI ALBERTONI, KAPLAN AGOSTI
#define RX_HLK 16
#define TX_HLK 17


void setup() {
  Serial.begin(115200);
  delay(1500);


  Serial2.begin(256000, SERIAL_8N1, RX_HLK, TX_HLK);


  Serial.println("LD2410C iniciado");
}


void loop() {
  static unsigned long ultimoDato = 0;


  while (Serial2.available()) {
    byte dato = Serial2.read();


    if (dato < 16) Serial.print("0");
    Serial.print(dato, HEX);
    Serial.print(" ");


    ultimoDato = millis();
  }


  if (millis() - ultimoDato > 1000) {
    Serial.println();
    ultimoDato = millis();
  }
}

