#include "esp32-hal.h"
#include "keypad_matrix.h"
#include <Arduino.h>
#include "esp32_hf.hpp"

// For speaker example.
#define SPEAKER_LRC_WS 25
#define SPEAKER_DOUT 26
#define SPEAKER_BCLK 5

EspHf& hf = EspHf::getInstance();
constexpr uint8_t kLeverPin = 13;
bool lastLever = false;

void setup() {
  Serial.begin(115200);
  KeypadMatrix::begin();
  
  // Mandatory setup.
  hf.setDeviceName("ESP-HF 42");
  hf.setLegacyPin("0000");
  
  hf.setupSpeaker(SPEAKER_BCLK, SPEAKER_LRC_WS, SPEAKER_DOUT);
  pinMode(kLeverPin, INPUT_PULLUP);
  delay(10);
  lastLever = (digitalRead(kLeverPin) == LOW);
  hf.begin();
}

char phone_number[15] = {0}; 
int i = 0;


void loop() {
  // char key = KeypadMatrix::getKeyPressed();  // plays the phone tone itself
  char key = 0;
  if (key)
  {
    Serial.println(key);
    if (key == 'W')
    {
      phone_number[0] = 0;
      i = 0;
    }
    else
    {
      phone_number[i++] = key;
      phone_number[i] = 0;
    }
  }

  bool lever = (digitalRead(kLeverPin) == LOW);
  if (lever && !lastLever) {
    // KeypadMatrix::playBeep(500, 100);        // lever beep through the same buzzer
    if (!phone_number[0])
      hf.acceptIncomingCall();
    else
      hf.callNumber(phone_number);
  }
  if (!lever && lastLever)
  {
    Serial.println("Reject");
    hf.rejectIncomingCall();
  }
  lastLever = lever;
  delayMicroseconds(500);

}