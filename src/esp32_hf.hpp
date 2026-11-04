#ifndef ESP32_HF_HPP
#define ESP32_HF_HPP

#include <ESP_I2S.h>
#include <memory>
#include <stdint.h>

/**
 * @file esp32_hf.hpp
 * @brief Bluetooth Classic Hands-Free Profile (HFP) library for the ESP32.
 *
 * The ESP32 acts as a hands-free unit (like a car kit) and connects to a phone.
 *
 * Usage:
 *   1. Get the instance with EspHf::getInstance().
 *   2. Configure it (name, PIN, audio, callbacks). This must be done BEFORE begin().
 *   3. Call begin(). The ESP32 is now visible to phones and tries to connect to a bonded phone.
 *   4. Use the actions (accept, reject, dial) and getters while the phone is connected.
 *
 * Callbacks are run in their own FreeRTOS tasks, so they may run at the same time
 * as your loop(). Keep them short and protect any data you share with loop().
 */

/** Overall state of the hands-free connection. */
enum e_state {
    IDLE_DISCONNECTED = 0, ///< No phone connected.
    IDLE_CONNECTED,        ///< Phone connected, no call.
    IN_CALL,               ///< A call is active.
    INCOMING_CALL,         ///< Phone is ringing.
    OUTGOING_CALL,         ///< A call is being placed.
};

class EspHf {
public:
    /** Get the only instance of the library. The object lives for the whole program. */
    static EspHf& getInstance()
    {
        static EspHf obj;
        return obj;
    }

    ~EspHf();

    // Not copyable, there is only one Bluetooth stack.
    EspHf(const EspHf&) = delete;
    EspHf& operator=(const EspHf&) = delete;

    /**
     * @brief Start Bluetooth Classic with the settings made so far.
     *
     * A device name and a legacy PIN are required. Everything else is optional.
     * Can only be called once.
     * @return True on success, false if the configuration is incomplete or startup failed.
     */
    bool begin();

    // ------------------------------------------------------------------
    // Setup. All of these must be called before begin().
    // ------------------------------------------------------------------

    /**
     * @brief Set the name phones show in their Bluetooth menu.
     * @param device_name Printable characters only, at most 248 bytes. The string is copied.
     * @return True on success, false if the name is invalid or begin() was already called.
     */
    bool setDeviceName(const char* device_name);

    /**
     * @brief Set the PIN used by phones that pair with a PIN (legacy pairing).
     *
     * Also used when secure pairing is turned off.
     * @param legacy_pin 1 to 16 printable characters. The string is copied.
     * @return True on success, false if the PIN is invalid or begin() was already called.
     */
    bool setLegacyPin(const char* legacy_pin);

    /**
     * @brief Turn secure simple pairing (SSP) on or off. On by default.
     *
     * With SSP the phone shows a 6 digit code and pairing is confirmed automatically.
     * When off, phones must use the legacy PIN.
     * @return True on success, false if begin() was already called.
     */
    bool setSecurePairing(bool enable_ssp);

    /**
     * @brief Use an I2S object you have set up yourself as the speaker.
     *
     * The object must already be started with setPins() and begin(). It receives
     * 16-bit mono audio, and the library changes its sample rate to match each call (8 or 16 kHz). If you also use a microphone, the speaker must not use I2S port 0 (the microphone needs it).
     * @param speaker Pointer to the I2S object. It must stay valid while the library runs.
     * @return True on success, false on an invalid object, port conflict or if begin() was called.
     */
    bool setSpeakerI2s(I2SClass* speaker);

    /**
     * @brief Let the library set up the speaker on I2S port 1 for you.
     *
     * 16-bit mono. Starts at 16 kHz and is switched to 8 kHz for narrow-band calls.
     * @param bclk Bit clock pin.
     * @param ws Word select (left/right clock) pin.
     * @param dout Data out pin, connect this to your amplifier.
     * @param din Data in pin, -1 if unused.
     * @param mclk Master clock pin, -1 if unused.
     * @return The I2S object, or NULL if setup failed.
     */
    I2SClass* setupSpeaker(int8_t bclk, int8_t ws, int8_t dout,
        int8_t din = (int8_t)(-1), int8_t mclk = (int8_t)(-1));

    /**
     * @brief Use an analog microphone connected to the ESP32's built-in ADC.
     *
     * The pin must be an ADC1 pin (GPIO 32-39). The signal must already be centered
     * in the ADC range, the library does not amplify it.
     * @param pin GPIO number.
     * @return True on success, false on an invalid pin, port conflict or if begin() was called.
     */
    bool setMicrophoneAdcPin(uint8_t pin);

    /**
     * @brief Use a digital I2S microphone (for example an I2S MEMS microphone).
     *
     * Uses I2S port 0 with 32-bit mono samples. Recording starts when call audio
     * connects and uses the sample rate of the call.
     * @param bclk Bit clock pin.
     * @param ws Word select pin.
     * @param din Data pin from the microphone.
     * @param mclk Master clock pin, -1 if unused.
     * @return True on success, false on invalid pins, port conflict or if begin() was called.
     */
    bool setMicrophoneI2s(int8_t bclk, int8_t ws, int8_t din,
        int8_t mclk = (int8_t)(-1));

    // ------------------------------------------------------------------
    // Callbacks. A callback can be replaced at any time. Passing nullptr is rejected.
    // ------------------------------------------------------------------

    /**
     * @brief Called when a call comes in.
     * @param ptr Function receiving the caller's number and 'sim_call'.
     *            'sim_call' is true for a normal phone call, the number is then filled in.
     *            For calls from other apps it is false and the number is empty.
     * @return True on success, false if ptr is null.
     */
    bool setCbIncomingCall(void (*ptr)(char phone_number[15], bool sim_call));

    /**
     * @brief Called when you start a call on the phone or with callNumber().
     * @param ptr Function receiving the dialed number and 'sim_call' (same meaning as for
     *            incoming calls).
     * @return True on success, false if ptr is null.
     */
    bool setCbOutgoingCall(void (*ptr)(char phone_number[15], bool sim_call));

    /** @brief Called when pairing succeeded. */
    bool setCbPinCorrect(void (*ptr)(void));
    /** @brief Called when pairing failed (for example a wrong PIN). */
    bool setCbPinIncorrect(void (*ptr)(void));
    /** @brief Called when a phone connects. */
    bool setCbBtConnected(void (*ptr)(void));
    /** @brief Called when the phone disconnects. */
    bool setCbBtDisconnected(void (*ptr)(void));

    /** @brief Called when a call becomes active (answered, or the other side picked up). */
    bool setCbCallActive(void (*ptr)(void));
    /** @brief Called when an active call ends. Not called for rejected or missed calls. */
    bool setCbCallEnded(void (*ptr)(void));
    /** @brief Called when call audio starts (the speaker and microphone are in use). */
    bool setCbAudioConnected(void (*ptr)(void));
    /** @brief Called when call audio stops. */
    bool setCbAudioDisconnected(void (*ptr)(void));

    /** @brief Called during secure pairing with the 6 digit code, as text, shown on the phone. */
    bool setCbSspAuth(void (*ptr)(char[7]));
    /** @brief Called when a phone asks for the legacy PIN. The PIN is answered automatically. */
    bool setCbLegacyAuth(void (*ptr)(void));

    // ------------------------------------------------------------------
    // Actions. These need begin() to have been called.
    // ------------------------------------------------------------------

    /**
     * @brief Answer the incoming call.
     * @return True if the command was sent (even if no call is ringing). False if begin() was not called.
     */
    bool acceptIncomingCall(void);

    /**
     * @brief Reject an incoming call, or hang up an outgoing or active call.
     * @return True if the command was sent (even if no call exists). False if begin() was not called.
     */
    bool rejectIncomingCall(void);

    /**
     * @brief Hang up the current call. Does the same as rejectIncomingCall(), the name is
     *        clearer when a call is already running.
     * @return True if the command was sent, false if begin() was not called.
     */
    bool endCall(void);

    /**
     * @brief Call the last dialed number again.
     * @return True if the command was sent, false if begin() was not called.
     */
    bool redial(void);

    /**
     * @brief Move call audio to the ESP32 (speaker and microphone). Needs a connected phone.
     * @return True if the request was sent, false if no phone is connected.
     */
    bool connectAudio(void);

    /**
     * @brief Move call audio back to the phone. Needs a connected phone.
     * @return True if the request was sent, false if no phone is connected.
     */
    bool disconnectAudio(void);

    /**
     * @brief Set the speaker volume, from 0 (quiet) to 15 (loud).
     *
     * Changes the volume of the ESP32 speaker and tells the phone. The value is
     * remembered after a restart. Works without a phone, then only the ESP32 changes.
     * @return True on success, false if the volume is out of range or begin() was not called.
     */
    bool setVolume(int volume);

    /**
     * @brief Mute or unmute the microphone during calls. The caller hears silence while muted.
     *        Stays set until changed. Has no effect when no microphone is configured.
     */
    bool setMicMute(bool mute);

    /**
     * @brief Place a call through the phone.
     * @param phone_number Digits (and an optional leading '+'). Must not be empty.
     * @return True if the command was sent, false on an empty number or if begin() was not called.
     */
    bool callNumber(const char* phone_number);

    // ------------------------------------------------------------------
    // Getters
    // ------------------------------------------------------------------

    /** @brief Current connection and call state. */
    e_state getState(void);

    /** @brief True when a phone is connected and ready. */
    bool isConnected(void);

    /** @brief Phone battery level 0-5, or -1 if the phone has not reported it. */
    int getBatteryLevel(void);

    /** @brief Phone signal strength 0-5, or -1 if the phone has not reported it. */
    int getSignalStrength(void);

    /** @brief True when the phone has mobile network service. */
    bool isNetworkAvailable(void);

    /** @brief Name of the connected phone. Empty when no phone is connected or the name is not known yet. */
    const char* getPeerName(void);

    /** @brief Current speaker volume, 0-15. */
    int getVolume(void);

private:
    EspHf();

    // All state and internal functions live in esp32_hf_intern.hpp (hidden from users).
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
#endif