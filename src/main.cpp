#include "esp32_hf.h"
#include <Arduino.h>

// For speaker example.
#define SPEAKER_LRC_WS 25
#define SPEAKER_DIN 26
#define SPEAKER_BCLK 5
#define MSBC 16000 




// Create object. Make sure it stays in scope for the full project.
EspHf& hf = EspHf::getInstance();
I2SClass speaker;


void ownFunction(void) {
  Serial.println("Someone is calling!");
  delay(2000);
}

// Example
void setup() {
  Serial.begin(115200);
  Serial.println("Begun.");
  Serial.print("WHAT");

  I2SClass *speaker = hf.setSpeaker(12, 45, 67);
  speaker.setPins(SPEAKER_BCLK, SPEAKER_LRC_WS, SPEAKER_DIN);
  speaker.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);

  // Mandatory setup.
  hf.setDeviceName("ESP-HF TEST");
  hf.setLegacyPin("433!");
  hf.setSpeakerI2s(&speaker);

  // // Optional
  // hf.disableSecurePairing();
  hf.setCallbackIncomingCall(ownFunction);


  // Start Hf after setup.
  // Serial.printf("WITHOUT INIT: %d\n", esp_hf_client_answer_call());
  Serial.print(hf.acceptIncomingCall());

  hf.begin();

  // speakerTest();
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(1000);
}