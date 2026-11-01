#pragma once

#include <ESP_I2S.h>
#include <memory>
#include <stdint.h>

#define TASK_SIZE 4096

enum e_state {
    IDLE_DISCONNECTED = 0,
    IDLE_CONNECTED,
    IN_CALL,
    INCOMING_CALL,
    OUTGOING_CALL,
};

class EspHf {
public:
    // Singleton to ensure only one instance.
    static EspHf& getInstance()
    {
        static EspHf obj;
        return obj;
    }

    ~EspHf();

    EspHf(const EspHf&) = delete;
    EspHf& operator=(const EspHf&) = delete;

    /**
     * @brief Setup bluetooth controller and enable classic bluetooth with defined settings.
     *        DO NOT call without having set device name and legacy pin.
     * @return True if initializing was successful, false if unsuccessful.
     */
    bool begin();

    // Setters

    /**
     * @brief Set device name of bluetooth device. The string is copied. Must be called before begin().
     * @param device_name String representing device name.
     * @return True if action was successful, false if error occurred.
     */
    bool setDeviceName(const char* device_name);

    /**
     * @brief Set legacy pin (copied). Must be called before begin(). Max length 16, numbers and/or letters.
     *        Pin should be at least one character long.
     * @param legacy_pin String representing the legacy pin.
     * @return True if action was successful, false if error occurred.
     */
    bool setLegacyPin(const char* legacy_pin);

    /**
     * @brief Enable/disable secure pairing (ssp). If disabled legacy pin will be forced.
     * @return True if action was successful, false if error occurred.
     */
    bool setSecurePairing(bool enable_ssp);

    /**
     * @brief Set I2S object for speaker. Object must be init with setPin and begin.
     * @param speaker Pointer to I2SClass object.
     * @return True if action was successful, false if error occurred.
     */
    bool setSpeakerI2s(I2SClass* speaker);

    /**
     * @brief Set i2s speaker internally.
     * @return Pointer to i2s object or NULL if initialization failed.
     */
    I2SClass* setupSpeaker(int8_t bclk, int8_t ws, int8_t dout,
        int8_t din = (int8_t)(-1), int8_t mclk = (int8_t)(-1));

    /**
     * @brief Use the ESP32's built-in ADC to capture an analog microphone.
     * Use an ADC1 pin (GPIO 32-39 on the original ESP32). The signal must
     * be biased within the ADC input range; this does not add amplification.
     */
    bool setMicrophoneAdcPin(uint8_t pin);

    /**
     * @brief Capture from a standard I2S microphone.
     * Uses 32-bit mono I2S slots and converts the upper 16 bits to HFP PCM.
     * Capture starts when HFP audio connects and uses its negotiated rate.
     */
    bool setMicrophoneI2s(int8_t bclk, int8_t ws, int8_t din,
        int8_t mclk = (int8_t)(-1));

    /* Callback setters. Callbacks can be changed at runtime. */

    /**
     * @brief Set callback for incoming calls. Value 'sim_call' will be true when you get a call on your simcard.
     * If phone call is over simcard you will get the phone number of who is calling you.
     * @param ptr Function pointer.
     * @return True if action was successful, false if error occurred.
     */
    bool setCbIncomingCall(void (*ptr)(char phone_number[15], bool sim_call));

    /**
     * @brief Set callback for outgoing calls. Value 'sim_call' will be true when you call over your simcard.
     * If phone call is over simcard you will get the phone number of who you are calling.
     * @param ptr Function pointer.
     * @return True if action was successful, false if error occurred.
     */
    bool setCbOutgoingCall(void (*ptr)(char phone_number[15], bool sim_call));

    bool setCbPinCorrect(void (*ptr)(void));
    bool setCbPinIncorrect(void (*ptr)(void));
    bool setCbBtConnected(void (*ptr)(void));
    bool setCbBtDisconnected(void (*ptr)(void));

    bool setCbSspAuth(void (*ptr)(char[7]));
    bool setCbLegacyAuth(void (*ptr)(void));

    // Actions

    /**
     * @brief Accept incoming call.
     * @return True regardless of whether a call is present. False if begin was not called before.
     */
    bool acceptIncomingCall(void);

    /**
     * @brief Reject incoming call or outgoing call.
     * @return True regardless of whether a call is present. False if begin was not called before.
     */
    bool rejectIncomingCall(void);

    bool callNumber(char *phone_number);

    // --- Getters
    e_state getState(void);

private:
    EspHf();

    // Defined in EspHf_impl.hpp. Holds all state and private functions.
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
