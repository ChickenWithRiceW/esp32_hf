// Public interface for the ESP32 Bluetooth Hands-Free client.
//
// This header intentionally includes NO ESP-IDF / Arduino headers.
// All of that lives in EspHf_impl.hpp, which is only included by the .cpp files.

#pragma once

#include <stdint.h>
#include <memory>
#include <ESP_I2S.h>

enum esp32_hfp {
    SUCCESS,
    BLUETOOTH_INIT_FAILED
};

class EspHf
{
    public:
        static EspHf& getInstance() {
            static EspHf obj;
            return obj;
        }

        // Must be defined in the .cpp, where Impl is a complete type.
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
         * @brief Set device name of bluetooth device. Ensure that the string will not be freed at runtime.
         *
         * This is runtime changeable but will only be visible when the AG Host disconnects from client and reconnects.
         * @param device_name String representing device name.
         * @return True if action was successful, false if error occurred.
         */
        bool setDeviceName(const char *device_name);

        /**
         * @brief Set legacy pin. Legacy pin has a max length of 16 and should only contain numbers and/or letters.
         *        Pin should be at least one character long.
         * @param legacy_pin String representing the legacy pin.
         * @return True if action was successful, false if error occurred.
         */
        bool setLegacyPin(const char *legacy_pin);

        /**
         * @brief Set I2S object for speaker. Object must be init with setPin and begin.
         * @param speaker Pointer to I2SClass object.
         * @return True if action was successful, false if error occurred.
         */
        bool setSpeakerI2s(I2SClass *speaker);

        /**
         * @brief Set i2s speaker internally.
         * @return Pointer to i2s object or NULL if initialization failed.
         */
        I2SClass *setSpeaker(int8_t bclk, int8_t ws, int8_t dout,
                             int8_t din = (int8_t)(-1), int8_t mclk = (int8_t)(-1));

        /**
         * @brief Enable/disable secure pairing (ssp). If disabled legacy pin will be used.
         * @return True if action was successful, false if error occurred.
         */
        bool setSecurePairing(bool enable_ssp);

        // Callback setters. Callbacks can be changed at runtime.
        // @return True if action was successful, false if error occurred.
        bool setCbIncomingCall(void (*ptr)(char phone_number[15], bool sim_call));
        bool setCbOutgoingCall(void (*ptr)(char phone_number[15], bool sim_call));
        bool setCbPinCorrect(void (*ptr)(void));
        bool setCbPinIncorrect(void (*ptr)(void));
        bool setCbBtConnected(void (*ptr)(void));
        bool setCbBtDisconnected(void (*ptr)(void));

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

    private:
        EspHf();

        // Defined in EspHf_impl.hpp. Holds all state and private functions.
        struct Impl;
        std::unique_ptr<Impl> _impl;
};
