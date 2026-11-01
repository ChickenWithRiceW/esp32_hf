// HandsFreeI2SMic
// Same as HandsFreeAnalogMic, but with a digital I2S microphone (for example an INMP441).
//
// Wiring (see the "Hardware and wiring" section of the README for details):
//   MAX98357A amp: BCLK -> GPIO 5, LRC -> GPIO 25, DIN -> GPIO 26
//   INMP441 mic:   SCK -> GPIO 14, WS -> GPIO 27, SD -> GPIO 32, L/R -> GND

#include <Arduino.h>
#include <esp32_hf.hpp>

#define SPEAKER_BCLK 5
#define SPEAKER_WS 25
#define SPEAKER_DOUT 26

#define MIC_BCLK 14
#define MIC_WS 27
#define MIC_DIN 32

EspHf& hf = EspHf::getInstance();

void onIncoming(char number[15], bool simCall)
{
    Serial.printf("Incoming call %s\n", simCall ? number : "(app call)");
}

void setup()
{
    Serial.begin(115200);

    hf.setDeviceName("ESP32 Hands-Free");
    hf.setLegacyPin("1234");

    if (!hf.setupSpeaker(SPEAKER_BCLK, SPEAKER_WS, SPEAKER_DOUT))
        Serial.println("Speaker setup failed");

    // The microphone uses I2S port 0, the library's speaker uses port 1, so both work together.
    hf.setMicrophoneI2s(MIC_BCLK, MIC_WS, MIC_DIN);

    hf.setCbIncomingCall(onIncoming);

    if (!hf.begin())
        Serial.println("Could not start the library, check the log above");
}

void loop()
{
    delay(1000);
}
