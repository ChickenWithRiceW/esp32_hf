#include "esp32_hf.hpp"
#include <Arduino.h>

// For speaker example.
#define SPEAKER_LRC_WS 25
#define SPEAKER_DIN 26
#define SPEAKER_BCLK 5


// Create object. Make sure it stays in scope for the full project.
EspHf& hf = EspHf::getInstance();
I2SClass speaker;


void cbIncoming(char phone_number[15], bool sim_call) {
  if (sim_call)
    Serial.printf("Ingoing call from Simcard. Number %s", phone_number);
  else
    Serial.printf("Ingoing call from some app. Number %s", phone_number);
}

void cbOutgoing(char phone_number[15], bool sim_call) {
  if (sim_call)
    Serial.printf("Outgoing call from Simcard. Number %s", phone_number);
  else
    Serial.printf("Outgoing call from some app. Number %s", phone_number);
}


void con(void) {
  Serial.println("Connected");
}

void dis(void) {
  Serial.println("Disconnected");
}

void failed(void) {
  Serial.println("Validation failed");
}

void success(void) {
  Serial.println("Validation worked");
}

void ssp_auth(char *name) {
  Serial.println(name);
}


// Example
void setup() {
  Serial.begin(115200);
  Serial.println("Begun.");
  Serial.print("WHAT");

  // I2SClass *speaker = hf.setSpeaker(12, 45, 67);

  // Mandatory setup.
  hf.setDeviceName("ESP-HF TEST");
  hf.setLegacyPin("0123456789123456");

  // hf.setSpeakerI2s(&speaker);
  // hf.setSpeaker(SPEAKER_BCLK, SPEAKER_LRC_WS, SPEAKER_DIN);

  // // Optional
  // hf.setSecurePairing(false);
  hf.setCbIncomingCall(cbIncoming);
  hf.setCbOutgoingCall(cbOutgoing);

  hf.setCbBtConnected(con);
  hf.setCbBtDisconnected(dis);

  hf.setCbPinCorrect(success);
  hf.setCbPinIncorrect(failed);

  hf.setCbSspAuth(ssp_auth);



  // Start Hf after setup.
  // Serial.printf("WITHOUT INIT: %d\n", esp_hf_client_answer_call());
  // Serial.print(hf.acceptIncomingCall());

  Serial.println(hf.begin());

  // speakerTest();
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(10000);

  // This will most likely not work but I dont know.
}