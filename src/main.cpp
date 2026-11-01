// Hardware test sketch for developing the library. Only built when ESP32_HF_DEV_SKETCH is
// defined (see platformio.ini), so it does not end up inside projects that use the library.
#ifdef ESP32_HF_DEV_SKETCH

#include "esp32_hf.hpp"
#include <Arduino.h>

// Hardware test sketch. Open the serial monitor (115200) and type a command:
//   a = accept   r = reject   e = end call   d = dial TEST_NUMBER   l = redial
//   + / - = volume up / down   m = mute mic on/off   t = move audio to ESP / back to phone
//   i = info (battery, signal, network, phone name, volume)   s = state   h = help
// State changes and heap usage are also printed automatically.

// For speaker example.
static constexpr int8_t SPEAKER_LRC_WS = 25;
static constexpr int8_t SPEAKER_DOUT = 26;
static constexpr int8_t SPEAKER_BCLK = 5;

// Analog microphone signal into ESP32 ADC1. GPIO 34 is input-only and ADC1.
static constexpr uint8_t MIC_ADC_PIN = 34;

// Alternative standard-I2S microphone input pins (for example, an I2S MEMS
// mic).
static constexpr int8_t MIC_I2S_BCLK = 14;
static constexpr int8_t MIC_I2S_WS = 27;
static constexpr int8_t MIC_I2S_DIN = 32;

// Number used by the 'd' command. Change to a number you can safely call.
static constexpr const char* TEST_NUMBER = "112";

// Create object. Make sure it stays in scope for the full project.
EspHf& hf = EspHf::getInstance();

static const char* stateName(e_state state)
{
    switch (state) {
    case IDLE_DISCONNECTED:
        return "IDLE_DISCONNECTED";
    case IDLE_CONNECTED:
        return "IDLE_CONNECTED";
    case IN_CALL:
        return "IN_CALL";
    case INCOMING_CALL:
        return "INCOMING_CALL";
    case OUTGOING_CALL:
        return "OUTGOING_CALL";
    }
    return "UNKNOWN";
}

// Callbacks run in their own tasks, keep them short.
void cbIncoming(char phone_number[15], bool sim_call)
{
    if (sim_call)
        Serial.printf("[CALL] Incoming from SIM. Number '%s'\n", phone_number);
    else
        Serial.println("[CALL] Incoming from some app (no number).");
}

void cbOutgoing(char phone_number[15], bool sim_call)
{
    if (sim_call)
        Serial.printf("[CALL] Outgoing from SIM. Number '%s'\n", phone_number);
    else
        Serial.println("[CALL] Outgoing from some app (no number).");
}

void con(void) { Serial.println("[BT] Connected"); }

void dis(void) { Serial.println("[BT] Disconnected"); }

void failed(void) { Serial.println("[BT] Pairing failed"); }

void success(void) { Serial.println("[BT] Pairing worked"); }

void sspAuth(char* code) { Serial.printf("[BT] SSP code: %s\n", code); }

void legacyAuth(void) { Serial.println("[BT] Legacy PIN requested"); }

void callActive(void) { Serial.println("[CALL] Active"); }

void callEnded(void) { Serial.println("[CALL] Ended"); }

void audioOn(void) { Serial.println("[AUDIO] Connected"); }

void audioOff(void) { Serial.println("[AUDIO] Disconnected"); }

void printInfo()
{
    Serial.printf("[INFO] connected=%d phone='%s' battery=%d signal=%d network=%d volume=%d\n",
        hf.isConnected(), hf.getPeerName(), hf.getBatteryLevel(), hf.getSignalStrength(),
        hf.isNetworkAvailable(), hf.getVolume());
}

void printHelp()
{
    Serial.printf("Commands: a=accept r=reject e=end d=dial(%s) l=redial +/-=volume m=mute t=audio i=info s=state h=help\n", TEST_NUMBER);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Begun.");

    // Mandatory setup.
    hf.setDeviceName("ESP-HF 42");
    hf.setLegacyPin("0123456789123456");

    hf.setupSpeaker(SPEAKER_BCLK, SPEAKER_LRC_WS, SPEAKER_DOUT);
    hf.setMicrophoneAdcPin(MIC_ADC_PIN);
    // To use an I2S microphone instead, replace the ADC selection above with:
    // hf.setMicrophoneI2s(MIC_I2S_BCLK, MIC_I2S_WS, MIC_I2S_DIN);

    // Optional
    // hf.setSecurePairing(false);
    hf.setCbIncomingCall(cbIncoming);
    hf.setCbOutgoingCall(cbOutgoing);

    hf.setCbBtConnected(con);
    hf.setCbBtDisconnected(dis);

    hf.setCbPinCorrect(success);
    hf.setCbPinIncorrect(failed);

    hf.setCbSspAuth(sspAuth);
    hf.setCbLegacyAuth(legacyAuth);

    hf.setCbCallActive(callActive);
    hf.setCbCallEnded(callEnded);
    hf.setCbAudioConnected(audioOn);
    hf.setCbAudioDisconnected(audioOff);

    // Start Hf after setup.
    Serial.printf("begin() returned %d\n", hf.begin());
    printHelp();
}

void loop()
{
    static e_state last_state = (e_state)-1;
    static uint32_t last_heap_print = 0;
    static bool muted = false;
    static bool audio_on_esp = true;

    // Print every state change (also shows the initial state).
    e_state state = hf.getState();
    if (state != last_state) {
        Serial.printf("[STATE] %s (%d)\n", stateName(state), state);
        last_state = state;
    }

    // Free heap every 10 s. A steadily falling value means a memory leak.
    if (millis() - last_heap_print > 10000) {
        Serial.printf("[HEAP] free %u, min free %u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
        last_heap_print = millis();
    }

    while (Serial.available()) {
        char c = Serial.read();
        switch (c) {
        case 'a':
            Serial.printf("accept -> %d\n", hf.acceptIncomingCall());
            break;
        case 'r':
            Serial.printf("reject -> %d\n", hf.rejectIncomingCall());
            break;
        case 'd':
            Serial.printf("dial -> %d\n", hf.callNumber(TEST_NUMBER));
            break;
        case 'e':
            Serial.printf("end call -> %d\n", hf.endCall());
            break;
        case 'l':
            Serial.printf("redial -> %d\n", hf.redial());
            break;
        case '+':
        case '-': {
            int volume = constrain(hf.getVolume() + (c == '+' ? 1 : -1), 0, 15);
            Serial.printf("volume %d -> %d\n", volume, hf.setVolume(volume));
            break;
        }
        case 'm':
            muted = !muted;
            Serial.printf("mic mute %d -> %d\n", muted, hf.setMicMute(muted));
            break;
        case 't':
            // Moves the call audio away from / back to the ESP32.
            Serial.printf("audio %s -> %d\n", audio_on_esp ? "to phone" : "to ESP", audio_on_esp ? hf.disconnectAudio() : hf.connectAudio());
            audio_on_esp = !audio_on_esp;
            break;
        case 'i':
            printInfo();
            break;
        case 's':
            Serial.printf("[STATE] %s (%d)\n", stateName(state), state);
            break;
        case 'h':
            printHelp();
            break;
        }
    }
    delay(50);
}

#endif // ESP32_HF_DEV_SKETCH
