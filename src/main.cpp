#include "esp32_hf.h"
#include <Arduino.h>
#include "ESP_I2S.h"

// For speaker example.
#define SPEAKER_LRC_WS 26
#define SPEAKER_BCLK 25
#define SPEAKER_DIN 24
#define MSBC 16000 




// Create object. Make sure it stays in scope for the full project.
EspHf& hf = EspHf::getInstance();
I2SClass I2s;


void ownFunction(void) {
  Serial.println("Someone is calling!");
  delay(2000);
}

void speakerTest(void) {
  // Implement different field for possible gain.
  I2s.setPins(SPEAKER_BCLK, SPEAKER_LRC_WS, SPEAKER_DIN);
  I2s.begin(I2S_MODE_STD, MSBC, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);

  while (true)
  {
    uint8_t sample;
    sample = 100;
    I2s.write(sample);
  }

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
  // hf.disableSecurePairing();
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