# PROJECT_STATUS

Persistent context for developers and future Claude sessions. Read this first, then the relevant source. Update on meaningful changes. User-facing docs live in `WIKI.md` (to be rewritten once the API is stable).

Last audit: 2026-11-01. P0 fixes applied the same day (compiled, and user reports the hardware test checklist works; multi-phone bonding and the no-saved-volume first boot may not have been covered).

## Overview
ESP32 library acting as a Bluetooth Classic HFP **Hands-Free unit** (phone = Audio Gateway). Target: Arduino-ESP32 3.x (pioarduino 5.5.5, ESP-IDF 5.5 based) now; plain ESP-IDF is a goal but not supported yet. Philosophy: small API, minimal fixes, no speculative features.

## Architecture
- Public API: `src/esp32_hf.hpp`, singleton `EspHf::getInstance()`, PIMPL `_impl`. State and internals in `src/esp32_hf_intern.hpp`.
- `esp32_hf.cpp`: controller/Bluedroid init, `begin()`.
- `hf_gap/esp32_hf_gap_stack.cpp`: name, PIN, SSP, discovery, bonded-device connect.
- `hf_app/bt_app_hf.cpp`: HFP client events -> state, call callbacks.
- `esp32_hf_audio.cpp`: speaker (I2S1 via `I2SClass`), mic (ADC continuous or I2S0), PCM data callbacks, CVSD/mSBC frame sizes.
- `_setters`, `_getters`, `_action`, `_callback_wrapper` (async callback tasks), `_misc`.
- `src/main.cpp` is a hardware test sketch. `*.log` are real call captures.

## Implemented
Pairing (SSP auto-confirm or legacy PIN), auto-connect to a bonded phone at boot, connect/disconnect callbacks, incoming/outgoing call callbacks with number (SIM) or app-call flag, accept/reject, dial, state getter, speaker playback with volume gain, mic capture (ADC1 or I2S) for CVSD and mSBC. Hardware-verified from logs: mSBC audio with PCM/HCI path, incoming, outgoing and app calls.

## P0 done (compile-checked only)
Fixed: bugs 1 (overflow only; still connects to first bonded device), 3, 4, 5 (state init, set on connect/disconnect), 10 (name/PIN copied into fixed buffers, `ASSERT_NOT_INIT` added), speaker volume saved in NVS (namespace `esp32_hf`, key `volume`) and restored in `begin()`, default 15 = no attenuation until one is saved (bug 8 silence part). HFP cannot query the phone's volume; it is only reported on change (+VGS). Mic volume events no longer overwrite the speaker volume. Callback args are now heap-allocated per event and freed by the callback task. Phone numbers truncate at 19 chars.
Hardware check done by user (pairing, call callbacks, audio). `src/main.cpp` is now a serial-command test sketch (a/r/d/s/h, state and heap prints).

## Known bugs (priority order; items listed under P0 done are fixed)
1. `connect_to_bonded_device()`: one-address buffer passed to `esp_bt_gap_get_bond_device_list` -> stack overflow with >1 bonded phone; only the first device is tried.
2. Reconnect is broken: `already_run` is never reset; discovery is abused as a trigger; no retry when the phone is out of range; failed first connects are not retried. Discovery also disturbs audio.
3. `Impl::call()` `strcpy`s the CLCC number into `char[15]` (overflow for long numbers); `clcc.number` may be NULL (crash in `strcmp`).
4. Shared `static t_call`/`t_auth` race between callback tasks.
5. `t_info.state` / `device_name` uninitialised; `IDLE_DISCONNECTED` never set; nothing reset on disconnect.
6. State machine: call-setup idle sets `IDLE_CONNECTED` right after answer; call-ended indicator not handled.
7. "Connected" is set on RFCOMM connect, not SLC connect.
8. Speaker I2S fixed at 16 kHz (CVSD calls play at 2x); `volume` starts at 0 so gain is 0 until the phone reports volume (verify on hardware); speaker `write()` blocks in the BT task; packets >256 samples truncated.
9. `begin()` uses `ESP_ERROR_CHECK` (aborts), no rollback or `end()`, `initialised` set late, dead AEC block.
10. Name/PIN setters keep caller pointers and lack `ASSERT_NOT_INIT`; `setupSpeaker` lacks `ASSERT_NOT_INIT`; header says name is runtime-changeable but it is not.
11. Mic close busy-waits while the task may be blocked in I2S read; `Arduino.h` included unconditionally in gap file.
12. Security: always discoverable, SSP auto-confirmed (see decisions).

## Limitations / compatibility
- Plain ESP-IDF unsupported: public header uses `ESP_I2S.h`/`I2SClass`, Arduino-only `esp32-hal-alloc-bt-classic-mem.h`, single PlatformIO Arduino env.
- Mic on I2S0, speaker on I2S1 (ESP32 ADC DMA uses I2S0). Analog mic needs ADC1 and is unamplified.
- `sdkconfig.defaults` may not apply to the precompiled Arduino framework.
- ESP-IDF has no raw AT API; only wrappers. A generic `sendAt()` is not feasible.
- Untested: CVSD fallback, multiple bonded phones, call waiting/hold (CCWA/BTRH only logged).

## Decisions
- User-approved new API: getters `isConnected`, `getBatteryLevel`, `getSignalStrength`, `isNetworkAvailable`, `getPeerName`, `getVolume`; `setVolume`, `setMicMute`, `connectAudio`/`disconnectAudio`, `endCall`, `redial`; callbacks call-ended/active and audio connected/disconnected.
- Not wanted for now: DTMF, volume-changed callback, raw AT.
- Decided: Arduino support first, IDF later. Pairing security stays as is (always discoverable, SSP auto-confirm) for now. Reconnect to any bonded device. Wrapper-only AT support is fine.
- Confirmed on hardware: speaker is silent after connect until the phone volume changes (bug 8 is real; needs a sane default volume). CVSD (8 kHz) has not been observed; user's phone negotiates mSBC, so CVSD rate fix is low priority but still untested.

## Roadmap
1. P0 correctness: bugs 1, 3, 4, 5, 10.
2. P1 connection: direct `esp_hf_client_connect` with bounded retry instead of discovery, SLC-based connected flag, reset on disconnect, clean `begin()` failure/rollback.
3. P1 state/audio: state machine, per-codec speaker rate, sane default volume, non-blocking speaker path if needed.
4. P2 approved API additions above.
5. P3 IDF support: raw-PCM callbacks as the core, Arduino I2S as optional adapter.
6. Rewrite `WIKI.md`; `PROPOSED_IMPROVEMENTS.md` items 1-4 overlap bugs 3-5 and 10.
