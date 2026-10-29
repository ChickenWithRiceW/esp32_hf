#pragma once

#include <Arduino.h>

// 4x4 keypad with retro-phone (DTMF) key tones on a passive buzzer.
//
// Keypad: plain digital matrix scanning with the ESP32's internal column
// pull-ups. Rows are high-impedance except the one being scanned (driven LOW).
//
// Sound: every key plays its classic touch-tone pair (row frequency + column
// frequency). A single buzzer pin can only make one frequency at a time, so the
// two tones are alternated every few milliseconds, which the ear blends into a
// recognisable phone-dialer sound. The tone plays while the key is held.
//
// Requires a PASSIVE buzzer and ESP32 Arduino core 3.x (for tone()/noTone()).
//
// Usage:
//   KeypadMatrix::begin();                     // once, in setup()
//   char k = KeypadMatrix::getKeyPressed();    // every loop(), never blocks;
//                                              // also keeps the sound running
//   KeypadMatrix::playBeep(500, 100);          // optional: your own beep
namespace KeypadMatrix {

constexpr uint8_t kNumRows = 4;
constexpr uint8_t kNumCols = 4;

// Pins exposed on the ESP-WROOM-32 board. These avoid the project's speaker
// pins (5, 25, 26), analog mic pin (34), and optional I2S mic pins (14, 27, 32).
constexpr uint8_t kRowPins[kNumRows] = {18, 19, 21, 22};
constexpr uint8_t kColPins[kNumCols] = {4, 13, 16, 17};

constexpr char kKeys[kNumRows][kNumCols] = {
    {'1', '2', '3', 'K'},
    {'4', '5', '6', 'S'},
    {'7', '8', '9', 'W'},
    {'*', '0', '#', 'R'},
};

// Buzzer pin (free: not used by keypad, speaker, or mics).
constexpr uint8_t kBuzzerPin = 23;

// Standard DTMF frequencies. Row 0-3 and column 0-3 line up with kKeys, so the
// 4th column (K, S, W, R) gets the "A, B, C, D" tones.
constexpr uint16_t kRowHz[kNumRows] = {697, 770, 852, 941};
constexpr uint16_t kColHz[kNumCols] = {1209, 1336, 1477, 1633};

// A key must read the same for this long before it is accepted.
constexpr uint32_t kDebounceMs = 25;

// Time for the row/column lines to settle after switching a row.
constexpr uint32_t kSettleUs = 50;

// true  = DTMF pair per key (two tones alternated rapidly)
// false = one distinct pitch per key (clearer on a single buzzer)
constexpr bool kDualTone = false;

// Single-tone mode: key N (0-15, row by row) plays kBaseHz + N * kStepHz.
constexpr uint16_t kBaseHz = 500;
constexpr uint16_t kStepHz = 120;

// How often the two DTMF frequencies swap (dual-tone mode only).
constexpr uint32_t kAlternateMs = 3;

// A key tone always lasts at least this long, even for a very quick tap.
constexpr uint32_t kMinToneMs = 80;

// ---------------------------------------------------------------------------
// Sound engine (non-blocking, driven from getKeyPressed())
// ---------------------------------------------------------------------------

struct Sound {
  bool active = false;
  bool held = false;       // true while a key is down (tone follows the key)
  bool onSecond = false;   // which of the two frequencies is playing
  uint16_t freqA = 0;
  uint16_t freqB = 0;      // 0 = single tone
  uint32_t offAt = 0;      // when to stop (once not held)
  uint32_t nextSwap = 0;   // when to swap frequencies
};

inline Sound& sound() {
  static Sound s;
  return s;
}

inline void stopSound() {
  Sound& s = sound();
  if (s.active) noTone(kBuzzerPin);
  s.active = false;
  s.held = false;
}

// Starts a single-tone beep and returns immediately.
inline void playBeep(uint16_t freqHz, uint16_t durationMs) {
  Sound& s = sound();
  const uint32_t now = millis();
  s.active = true;
  s.held = false;
  s.onSecond = false;
  s.freqA = freqHz;
  s.freqB = 0;
  s.offAt = now + durationMs;
  tone(kBuzzerPin, freqHz);
}

// Starts the DTMF tone for a key. The tone keeps playing while the key is held.
inline void playKey(char key) {
  for (uint8_t row = 0; row < kNumRows; ++row) {
    for (uint8_t col = 0; col < kNumCols; ++col) {
      if (kKeys[row][col] != key) continue;

      Sound& s = sound();
      const uint32_t now = millis();
      s.active = true;
      s.held = true;
      s.onSecond = false;
      if (kDualTone) {
        s.freqA = kRowHz[row];
        s.freqB = kColHz[col];
      } else {
        s.freqA = kBaseHz + kStepHz * (row * kNumCols + col);
        s.freqB = 0;
      }
      s.offAt = now + kMinToneMs;
      s.nextSwap = now + kAlternateMs;
      tone(kBuzzerPin, s.freqA);
      return;
    }
  }
}

// Called when the key is released: the tone ends once its minimum time is up.
inline void releaseKeyTone() {
  Sound& s = sound();
  s.held = false;  // offAt was set to the minimum end time in playKey()
}

// Keeps the sound running: swaps the two DTMF frequencies and stops the tone
// at the right time. Called automatically by getKeyPressed().
inline void updateSound() {
  Sound& s = sound();
  if (!s.active) return;

  const uint32_t now = millis();

  if (!s.held && static_cast<int32_t>(now - s.offAt) >= 0) {
    stopSound();
    return;
  }

  if (s.freqB != 0 && static_cast<int32_t>(now - s.nextSwap) >= 0) {
    s.onSecond = !s.onSecond;
    tone(kBuzzerPin, s.onSecond ? s.freqB : s.freqA);
    s.nextSwap = now + kAlternateMs;
  }
}

// ---------------------------------------------------------------------------
// Keypad
// ---------------------------------------------------------------------------

// Scans the matrix once and returns the first key currently held down,
// or '\0' if none. Never waits for release.
inline char scanRaw() {
  char found = '\0';

  for (uint8_t row = 0; row < kNumRows; ++row) {
    pinMode(kRowPins[row], OUTPUT);
    digitalWrite(kRowPins[row], LOW);
    delayMicroseconds(kSettleUs);

    for (uint8_t col = 0; col < kNumCols; ++col) {
      if (digitalRead(kColPins[col]) == LOW) {
        found = kKeys[row][col];
        break;
      }
    }

    // Return the row to high-impedance so idle rows never fight the active one.
    pinMode(kRowPins[row], INPUT);

    if (found != '\0') break;
  }

  return found;
}

inline void begin() {
  for (uint8_t i = 0; i < kNumCols; ++i) {
    pinMode(kColPins[i], INPUT_PULLUP);
  }
  for (uint8_t i = 0; i < kNumRows; ++i) {
    pinMode(kRowPins[i], INPUT);  // high-Z until scanned
  }
  pinMode(kBuzzerPin, OUTPUT);
  digitalWrite(kBuzzerPin, LOW);
}

// Returns a key exactly once per physical press, after debouncing, and plays
// its phone tone. Returns '\0' when there is no new press. A held key is not
// repeated; it is reported again only after being released and pressed again.
// Non-blocking: call it every loop().
inline char getKeyPressed() {
  static char lastRaw = '\0';     // most recent raw reading
  static char reported = '\0';    // key already reported and still held
  static uint32_t changedAt = 0;  // when the raw reading last changed

  const char raw = scanRaw();
  const uint32_t now = millis();
  char result = '\0';

  if (raw != lastRaw) {
    lastRaw = raw;
    changedAt = now;
  } else if (now - changedAt >= kDebounceMs) {
    if (raw == '\0') {
      reported = '\0';  // stable release re-arms the keypad
      releaseKeyTone();
    } else if (raw != reported) {
      reported = raw;
      result = raw;
      playKey(raw);
    }
  }

  updateSound();
  return result;
}

}  // namespace KeypadMatrix