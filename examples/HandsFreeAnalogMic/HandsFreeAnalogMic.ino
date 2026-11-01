// HandsFreeAnalogMic
// A hands-free kit: hear the caller on a speaker, talk through an analog microphone.
// Pair the phone, call it, answer on the phone. The call sound moves to the ESP32.
//
// Wiring (see the pictures in the README):
//   MAX98357A amp: BCLK -> GPIO 5, LRC -> GPIO 25, DIN -> GPIO 26
//   Analog microphone board: OUT -> GPIO 34 (must be an ADC1 pin: GPIO 32-39)

#include <Arduino.h>
#include <esp32_hf.hpp>

#define SPEAKER_BCLK 5
#define SPEAKER_WS 25
#define SPEAKER_DOUT 26
#define MIC_ADC_PIN 34

EspHf& hf = EspHf::getInstance();

// Callbacks run in their own tasks. Keep them short.
void onConnected() { Serial.println("Phone connected"); }
void onDisconnected() { Serial.println("Phone disconnected"); }

void onIncoming(char number[15], bool simCall)
{
    if (simCall)
        Serial.printf("Incoming call from %s\n", number);
    else
        Serial.println("Incoming call from an app");
}

void onCallEnded() { Serial.println("Call ended"); }

void setup()
{
    Serial.begin(115200);

    // Required: the name phones show, and a PIN for phones that ask for one.
    hf.setDeviceName("ESP32 Hands-Free");
    hf.setLegacyPin("1234");

    // Sound: the speaker (I2S amplifier) and the microphone (analog, on an ADC1 pin).
    if (!hf.setupSpeaker(SPEAKER_BCLK, SPEAKER_WS, SPEAKER_DOUT))
        Serial.println("Speaker setup failed");
    hf.setMicrophoneAdcPin(MIC_ADC_PIN);

    // The most useful callbacks.
    hf.setCbBtConnected(onConnected);
    hf.setCbBtDisconnected(onDisconnected);
    hf.setCbIncomingCall(onIncoming);
    hf.setCbCallEnded(onCallEnded);

    if (hf.begin())
        Serial.println("Ready. Pair 'ESP32 Hands-Free' in your phone's Bluetooth settings.");
    else
        Serial.println("Could not start the library, check the log above");
}

void loop()
{
    // Everything runs in the background.
    delay(1000);
}
