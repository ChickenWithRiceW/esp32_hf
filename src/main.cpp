#include "esp32_hf.h"
#include <Arduino.h>


// Create object. Make sure it stays in scope for the full project.
EspHf& hf = EspHf::getInstance();

void ownFunction(esp_hf_client_cb_param_t *param) {
  Serial.println("Someone is calling!");
  delay(2000);
  esp_hf_client_answer_call();
}

// Example
void setup() {
  Serial.begin(115200);
  Serial.println("Begun.");

  // Mandatory setup.
  hf.setDeviceName("ESP-HF TEST");
  hf.setLegacyPin("4333");

  // // Optional
  // hf.disableSecurePairing();
  hf.setIncomingCallCallback(ownFunction);

  

  // Start Hf after setup.
  hf.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(1000);
}