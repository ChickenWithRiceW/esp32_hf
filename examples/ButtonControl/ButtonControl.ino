// ButtonControl
// Answer and reject calls with two buttons.
//
// Wiring: button 1 between GPIO 18 and GND (answer), button 2 between GPIO 19 and GND
// (reject / hang up). The internal pull-up resistors are used, no other parts needed.
//
// Note: like MinimalCalls, this sketch has no speaker or microphone.

#include <Arduino.h>
#include <esp32_hf.hpp>

#define ANSWER_PIN 18
#define REJECT_PIN 19

EspHf& hf = EspHf::getInstance();

void onIncoming(char number[15], bool simCall)
{
    if (simCall)
        Serial.printf("Incoming call from %s. Press the answer or reject button.\n", number);
    else
        Serial.println("Incoming call from an app. Press the answer or reject button.");
}

void setup()
{
    Serial.begin(115200);
    pinMode(ANSWER_PIN, INPUT_PULLUP);
    pinMode(REJECT_PIN, INPUT_PULLUP);

    hf.setDeviceName("ESP32 Hands-Free");
    hf.setLegacyPin("1234");
    hf.setCbIncomingCall(onIncoming);

    if (!hf.begin())
        Serial.println("Could not start the library, check the log above");
}

void loop()
{
    // Buttons read LOW when pressed. Wait for release so one press sends one command.
    if (digitalRead(ANSWER_PIN) == LOW) {
        hf.acceptIncomingCall();
        while (digitalRead(ANSWER_PIN) == LOW)
            delay(10);
    }
    if (digitalRead(REJECT_PIN) == LOW) {
        hf.rejectIncomingCall(); // Also hangs up a call that is already running.
        while (digitalRead(REJECT_PIN) == LOW)
            delay(10);
    }
    delay(10);
}
