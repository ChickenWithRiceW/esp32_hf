// CompletePhoneKit
// A complete hands-free kit in one sketch. It covers the most important uses:
//
//   - pairing and connection messages
//   - hear the caller (speaker) and be heard (microphone)
//   - answer, reject, hang up and redial with buttons
//   - volume up / down and mute with buttons
//   - a status LED that shows what is going on
//   - dialing a number and reading phone information from the Serial Monitor
//
// Wiring: see the "Complete phone kit" picture in the README (docs/images/wiring-complete.svg).
//
//   MAX98357A amp:  BCLK -> GPIO 5,  LRC -> GPIO 25,  DIN -> GPIO 26
//   Analog mic:     OUT  -> GPIO 34  (or set USE_I2S_MIC to true, see below)
//   Buttons (each between its pin and GND):
//       ANSWER 18, REJECT 19, VOL+ 21, VOL- 22, MUTE 23
//   Status LED:     GPIO 4 -> 220 ohm resistor -> LED -> GND
//
// Serial Monitor (115200). Type a line and press Enter:
//   i            show phone information
//   d0123456789  dial that number

#include <Arduino.h>
#include <esp32_hf.hpp>

// ---------------- Pins ----------------
#define SPEAKER_BCLK 5
#define SPEAKER_WS 25
#define SPEAKER_DOUT 26

// Choose the microphone: analog (ADC1 pin) or digital I2S (for example INMP441).
#define USE_I2S_MIC false
#define MIC_ADC_PIN 34
#define MIC_I2S_BCLK 14
#define MIC_I2S_WS 27
#define MIC_I2S_DIN 32

#define PIN_ANSWER 18
#define PIN_REJECT 19
#define PIN_VOL_UP 21
#define PIN_VOL_DOWN 22
#define PIN_MUTE 23
#define PIN_LED 4

EspHf& hf = EspHf::getInstance();

// ---------------- A simple debounced button ----------------
// pressed() is true once for each press. Buttons connect the pin to GND (INPUT_PULLUP).
struct Button {
    uint8_t pin;
    bool last_level = HIGH;
    uint32_t last_change = 0;

    explicit Button(uint8_t p) : pin(p) {}
    void begin() { pinMode(pin, INPUT_PULLUP); }
    bool pressed()
    {
        const bool level = digitalRead(pin);
        if (level != last_level && millis() - last_change > 30) {
            last_change = millis();
            last_level = level;
            return level == LOW;
        }
        return false;
    }
};

Button answerButton(PIN_ANSWER);
Button rejectButton(PIN_REJECT);
Button volUpButton(PIN_VOL_UP);
Button volDownButton(PIN_VOL_DOWN);
Button muteButton(PIN_MUTE);

bool muted = false;

// ---------------- Callbacks ----------------
// Callbacks run in their own tasks, so they only print here. For real work, set a flag
// and handle it in loop().

void onConnected() { Serial.println("[phone] connected"); }
void onDisconnected() { Serial.println("[phone] disconnected, waiting for it to come back"); }

void onIncoming(char number[15], bool simCall)
{
    if (simCall)
        Serial.printf("[call] incoming from %s\n", number);
    else
        Serial.println("[call] incoming from an app (no number)");
}

void onOutgoing(char number[15], bool simCall)
{
    if (simCall)
        Serial.printf("[call] calling %s\n", number);
    else
        Serial.println("[call] calling from an app (no number)");
}

void onCallActive() { Serial.println("[call] active"); }
void onCallEnded() { Serial.println("[call] ended"); }
void onAudioOn() { Serial.println("[audio] sound is on the ESP32"); }
void onAudioOff() { Serial.println("[audio] sound stopped"); }

void onPairingCode(char code[7]) { Serial.printf("[pairing] code %s\n", code); }
void onPaired() { Serial.println("[pairing] done"); }
void onPairingFailed() { Serial.println("[pairing] failed"); }

// ---------------- Status LED ----------------
// off          no phone connected
// slow blink   phone connected, no call
// fast blink   phone is ringing
// on           call in progress
void updateLed()
{
    bool on = false;
    switch (hf.getState()) {
    case IDLE_DISCONNECTED:
        on = false;
        break;
    case IDLE_CONNECTED:
        on = (millis() % 2000) < 100; // short flash every 2 seconds
        break;
    case INCOMING_CALL:
    case OUTGOING_CALL:
        on = (millis() % 300) < 150; // fast blink
        break;
    case IN_CALL:
        on = true;
        break;
    }
    digitalWrite(PIN_LED, on);
}

// ---------------- Buttons ----------------
void handleButtons()
{
    if (answerButton.pressed()) {
        // The answer button answers a ringing call, or calls the last number when idle.
        if (hf.getState() == INCOMING_CALL)
            hf.acceptIncomingCall();
        else if (hf.getState() == IDLE_CONNECTED)
            hf.redial();
    }

    if (rejectButton.pressed())
        hf.endCall(); // Rejects a ringing call, or hangs up a running one.

    if (volUpButton.pressed()) {
        hf.setVolume(min(hf.getVolume() + 1, 15));
        Serial.printf("[volume] %d\n", hf.getVolume());
    }
    if (volDownButton.pressed()) {
        hf.setVolume(max(hf.getVolume() - 1, 0));
        Serial.printf("[volume] %d\n", hf.getVolume());
    }

    if (muteButton.pressed()) {
        muted = !muted;
        hf.setMicMute(muted);
        Serial.println(muted ? "[mic] muted" : "[mic] unmuted");
    }
}

// ---------------- Serial commands ----------------
void printInfo()
{
    Serial.println("---- phone information ----");
    Serial.printf("connected:   %s\n", hf.isConnected() ? "yes" : "no");
    Serial.printf("phone name:  %s\n", hf.getPeerName());
    Serial.printf("state:       %d (0 disconnected, 1 idle, 2 in call, 3 incoming, 4 outgoing)\n", hf.getState());
    Serial.printf("battery:     %d (0-5, -1 = not reported)\n", hf.getBatteryLevel());
    Serial.printf("signal:      %d (0-5, -1 = not reported)\n", hf.getSignalStrength());
    Serial.printf("network:     %s\n", hf.isNetworkAvailable() ? "yes" : "no");
    Serial.printf("volume:      %d (0-15)\n", hf.getVolume());
    Serial.printf("mic muted:   %s\n", muted ? "yes" : "no");
}

void handleSerial()
{
    if (!Serial.available())
        return;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
        return;

    if (line == "i") {
        printInfo();
    } else if (line[0] == 'd' && line.length() > 1) {
        String number = line.substring(1);
        Serial.printf("[call] dialing %s -> %s\n", number.c_str(), hf.callNumber(number.c_str()) ? "sent" : "failed");
    } else {
        Serial.println("Commands: i (info), d<number> (dial)");
    }
}

// ---------------- Setup and loop ----------------
void setup()
{
    Serial.begin(115200);
    Serial.setTimeout(20);

    pinMode(PIN_LED, OUTPUT);
    answerButton.begin();
    rejectButton.begin();
    volUpButton.begin();
    volDownButton.begin();
    muteButton.begin();

    // Required: how phones see the kit, and the PIN for phones that ask for one.
    hf.setDeviceName("ESP32 Phone Kit");
    hf.setLegacyPin("1234");

    // Sound.
    if (!hf.setupSpeaker(SPEAKER_BCLK, SPEAKER_WS, SPEAKER_DOUT))
        Serial.println("Speaker setup failed");
    if (USE_I2S_MIC)
        hf.setMicrophoneI2s(MIC_I2S_BCLK, MIC_I2S_WS, MIC_I2S_DIN);
    else
        hf.setMicrophoneAdcPin(MIC_ADC_PIN);

    // Messages.
    hf.setCbBtConnected(onConnected);
    hf.setCbBtDisconnected(onDisconnected);
    hf.setCbIncomingCall(onIncoming);
    hf.setCbOutgoingCall(onOutgoing);
    hf.setCbCallActive(onCallActive);
    hf.setCbCallEnded(onCallEnded);
    hf.setCbAudioConnected(onAudioOn);
    hf.setCbAudioDisconnected(onAudioOff);
    hf.setCbSspAuth(onPairingCode);
    hf.setCbPinCorrect(onPaired);
    hf.setCbPinIncorrect(onPairingFailed);

    if (hf.begin())
        Serial.println("Ready. Pair 'ESP32 Phone Kit' in your phone's Bluetooth settings.");
    else
        Serial.println("Could not start the library, check the log above");
}

void loop()
{
    handleButtons();
    handleSerial();
    updateLed();
    delay(5);
}
