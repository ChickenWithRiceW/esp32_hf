// MinimalCalls
// The simplest use of the library: pair a phone and print call events.
// No speaker or microphone is needed, only an ESP32 and the Serial Monitor (115200).
//
// Note: call audio is sent to the ESP32 and dropped here, so you will not hear the
// caller on the phone. See HandsFreeAnalogMic to actually talk.

#include <Arduino.h>
#include <esp32_hf.hpp>

EspHf& hf = EspHf::getInstance();

void onConnected() { Serial.println("Phone connected"); }
void onDisconnected() { Serial.println("Phone disconnected"); }

void onIncoming(char number[15], bool simCall)
{
    if (simCall)
        Serial.printf("Incoming call from %s\n", number);
    else
        Serial.println("Incoming call from an app (no number)");
}

void onOutgoing(char number[15], bool simCall)
{
    if (simCall)
        Serial.printf("Calling %s\n", number);
    else
        Serial.println("Calling from an app (no number)");
}

void setup()
{
    Serial.begin(115200);

    // Both are required. The PIN is used by phones that ask for one when pairing.
    hf.setDeviceName("ESP32 Hands-Free");
    hf.setLegacyPin("1234");

    hf.setCbBtConnected(onConnected);
    hf.setCbBtDisconnected(onDisconnected);
    hf.setCbIncomingCall(onIncoming);
    hf.setCbOutgoingCall(onOutgoing);

    if (!hf.begin())
        Serial.println("Could not start the library, check the log above");
    else
        Serial.println("Ready. Pair 'ESP32 Hands-Free' in your phone's Bluetooth settings.");
}

void loop()
{
    // Everything runs in the background, nothing to do here.
    delay(1000);
}
