#include "esp32_hf.h"
#include <Arduino.h>


// Create object. Make sure it stays in scope for the full project.
EspHf& hf = EspHf::getInstance();

void ownFunction(void) {
  Serial.println("Someone is calling!");
  delay(2000);
}

// Example
void setup() {
  Serial.begin(115200);
  Serial.println("Begun.");
  Serial.print("WHAT");

  // Mandatory setup.
  hf.setDeviceName("ESP-HF TEST");
  hf.setLegacyPin("433!");

  // // Optional
  hf.disableSecurePairing();
  hf.setCallbackIncomingCall(ownFunction);

  

  // Start Hf after setup.
  // Serial.printf("WITHOUT INIT: %d\n", esp_hf_client_answer_call());
  Serial.print(hf.acceptIncomingCall());

  hf.begin();

  Serial.print("before sleep");

  sleep(2);
  Serial.print("after sleep");

  Serial.print("HUH\n");

  Serial.print(hf.acceptIncomingCall());
  
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(1000);
}