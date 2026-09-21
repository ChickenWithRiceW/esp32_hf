#include "esp32_hf.h"
#include <Arduino.h>

// For speaker example.
#define SPEAKER_LRC_WS 25
#define SPEAKER_DIN 26
#define SPEAKER_BCLK 5


// Create object. Make sure it stays in scope for the full project.
// EspHf& hf = EspHf::getInstance();
EspHf& hf = EspHf::getInstance();

I2SClass speaker;


void cbIncoming(void) {
  Serial.println("Incoming general!");
}

void cbRinging(void) {
  Serial.println(hf.getBatteryLevel());
}

// Example
void setup() {
  char *test = "Test";
  test = NULL;
  Serial.begin(115200);
  Serial.println("Begun.");
  Serial.print("WHAT");

  // I2SClass *speaker = hf.setSpeaker(12, 45, 67);

  // Mandatory setup.
  hf.setDeviceName("HFP TEST");
  hf.setLegacyPin("433!");
  // hf.setSpeakerI2s(&speaker);
  // hf.setSpeaker(SPEAKER_BCLK, SPEAKER_LRC_WS, SPEAKER_DIN);

  // // Optional
  // hf.disableSecurePairing();
  // hf.setCbOtherCall(cbIncoming);
  // hf.setCbSimCall(cbRinging);


  // Start Hf after setup.
  // Serial.printf("WITHOUT INIT: %d\n", esp_hf_client_answer_call());
  // Serial.print(hf.acceptIncomingCall());

  hf.begin();

  // speakerTest();
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(10000);

  // This will most likely not work but I dont know.
  esp_bt_gap_set_device_name("Changed name without");
}