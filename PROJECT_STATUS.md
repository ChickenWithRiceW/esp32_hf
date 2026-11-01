# PROJECT_STATUS

Persistent context for developers and future Claude sessions. Read this first, then the relevant source. Update on meaningful changes. User-facing docs: root `README.md` (everything: quick start, wiring, guide, how it works, troubleshooting) and `docs/API.md` (full API reference); examples in `examples/`.

Last updated: 2026-11-01. Everything below the "Hardware-verified" line is compiled only (`pio run`), not yet tested on a board.

## Overview
ESP32 library acting as a Bluetooth Classic HFP **Hands-Free unit** (phone = Audio Gateway). Target: Arduino-ESP32 3.x (pioarduino 5.5.5, ESP-IDF 5.5 based). Plain ESP-IDF is a later goal. Philosophy: small API, minimal fixes, no speculative features. Constants are `static constexpr` (see CLAUDE.md).

## Architecture
- Public API: `src/esp32_hf.hpp`, singleton `EspHf::getInstance()`, PIMPL `_impl`. State and internals: `src/esp32_hf_intern.hpp`.
- `esp32_hf.cpp`: controller/Bluedroid init with rollback (`shutdown_bluetooth`), `begin()`.
- `hf_gap/esp32_hf_gap_stack.cpp`: name, PIN, SSP, bonded-phone reconnect (`try_reconnect`, FreeRTOS timer every 5 s while disconnected, rotates through bonded phones), remote-name lookup.
- `hf_app/bt_app_hf.cpp`: HFP client events. Public state is derived from tracked indicators in `update_state()`.
- `esp32_hf_audio.cpp`: speaker (I2S1), mic (ADC1 or I2S0, one `mic` struct, capture task + queue), mute, volume in NVS, audio callbacks.
- `_setters`, `_getters`, `_action`, `_callback_wrapper` (`run_cb`, per-event heap args freed by the callback task).
- `src/main.cpp` is a serial-command hardware test sketch, only built with `-DESP32_HF_DEV_SKETCH` so library users do not get it. `*.log` are real call captures.
- `README.md`, `docs/API.md`, `examples/` (5 sketches incl. `CompletePhoneKit`, pins as `#define`; `HandsFreeAnalogMic` is the README's first example, all compiled, not run on hardware), `docs/images/wiring-*.svg` (speaker/analog-mic/i2s-mic/buttons use real part art from `docs/images/parts/` via `generate_wiring.py`; `wiring-complete.svg` is schematic-style from `generate_complete_schematic.py`; credits in `parts/CREDITS.md` (INMP441 picture license unconfirmed); keep the pin map in sync with `examples/`), `library.json`/`library.properties`/`LICENSE` (MIT) with TODO author/URL fields.

## Implemented
- Pairing (SSP auto-confirm or legacy PIN), connect to bonded phones at boot and whenever disconnected.
- "Connected" = service level connection (not just RFCOMM). Callbacks: connected/disconnected, pairing, incoming/outgoing call (number or app-call flag), call active/ended, audio connected/disconnected.
- Actions: accept, reject, endCall, callNumber, redial, connectAudio/disconnectAudio, setVolume, setMicMute.
- Getters: getState, isConnected, getBatteryLevel, getSignalStrength (-1 = not reported), isNetworkAvailable, getPeerName, getVolume.
- Speaker volume saved in NVS (`esp32_hf`/`volume`), restored in `begin()`. HFP cannot query the phone volume; it is only reported on change.
- `begin()` returns false and cleans up on failure, so it can be retried.

Hardware-verified earlier (user): mSBC audio with PCM/HCI path, incoming/outgoing/app calls, pairing, first fix batch (overflow fixes, callbacks, volume).

## State logic
`getState()` follows the phone's indicators: disconnected -> idle; setup incoming -> INCOMING_CALL; dialing/alerting -> OUTGOING_CALL; call in progress -> IN_CALL; else IDLE_CONNECTED. Independent of call audio. Logs show audio connects while still ringing (in-band ring), so audio must not drive state. `IN_CALL` now means "call active".

## Known issues / untested
- Needs hardware test: reconnect (phone out of range, several bonded phones, phone-initiated reconnect), state transitions, new API, mute, audio transfer, peer name, `begin()` failure path.
- Call waiting/hold only logged (CCWA/BTRH). CVSD (8 kHz) untested: the speaker is now reconfigured with `configureTX` to 8 or 16 kHz on each audio connect (compiled only).
- Speaker `write()` blocks in the BT task; packets >256 samples truncated.
- Mic close waits for the capture task.
- Shared state (`_info`, call tracking) is written by the Bluetooth task and read by user tasks without locks. Single ints/bools are fine in practice; `getPeerName()` can show a half-written name. Consider a mutex or snapshot if it matters.
- Security: always discoverable, SSP auto-confirmed (accepted for now by user).
- Callback typedef still says `char phone_number[15]`; internal buffer is 20 (numbers truncated at 19).
- Call ended callback fires only for calls that were active, not for rejected/missed calls.

## Limitations / compatibility
- Plain ESP-IDF unsupported: public header uses `ESP_I2S.h`/`I2SClass`, Arduino-only `esp32-hal-alloc-bt-classic-mem.h`.
- Mic on I2S0, speaker on I2S1 (ESP32 ADC DMA uses I2S0). Analog mic needs ADC1 and is unamplified.
- `sdkconfig.defaults` may not apply to the precompiled Arduino framework.
- ESP-IDF has no raw AT API; only wrappers. No `sendAt()`.

## Decisions
- Arduino first, IDF later. Pairing security unchanged. Reconnect to any bonded device. Wrapper-only AT.
- Not wanted for now: DTMF, volume-changed callback.

## Roadmap
1. Hardware-test the current batch, fix what it finds.
2. Non-blocking speaker path if glitches appear.
3. IDF support via raw-PCM callbacks, Arduino I2S as optional adapter.
4. Fill the TODOs in `LICENSE`, `library.json`, `library.properties`, `README.md` (author, repo URL, tested microphone board). Keep docs in sync with API changes.
