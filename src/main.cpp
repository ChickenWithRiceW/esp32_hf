#include "esp32_hf.h"
#include <Arduino.h>


// Create object. Make sure it stays in scope for the full project.
EspHf& hf = EspHf::getInstance();


// Example
void setup() {
  Serial.begin(115200);
  Serial.println("Begun.");

  // Mandatory setup.
  hf.setDeviceName("ESP-HF TEST");
  // hf.disableSecurePairing();
  hf.setLegacyPin("4333");

  // Start Hf after setup.
  hf.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(1000);
}