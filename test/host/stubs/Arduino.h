#pragma once
/* Arduino.h stub for native host tests.
 * Provides the minimum surface that production modules reference.
 * All implementations are no-ops or trivial returns. */

#include <cstdint>
#include <cstdlib>
#include <cstring>

typedef uint8_t  byte;
typedef uint16_t word;

inline uint32_t millis()              { return 0; }
inline uint32_t micros()              { return 0; }
inline void     delay(uint32_t)       {}
inline void     delayMicroseconds(uint32_t) {}

inline void     pinMode(uint8_t, uint8_t) {}
inline void     digitalWrite(uint8_t, uint8_t) {}
inline int      digitalRead(uint8_t)  { return 0; }
inline int      analogRead(uint8_t)   { return 0; }

#define INPUT        0
#define OUTPUT       1
#define INPUT_PULLUP 2
#define HIGH         1
#define LOW          0
#define CHANGE       3
#define RISING       4
#define FALLING      5

inline void attachInterrupt(uint8_t, void(*)(), int) {}
inline int  digitalPinToInterrupt(uint8_t pin) { return pin; }

/* Minimal Serial no-op */
struct _Serial_t {
    void begin(uint32_t) {}
    template<typename T> void print(T)   {}
    template<typename T> void println(T) {}
    void println() {}
};
extern _Serial_t Serial;
inline _Serial_t Serial;  // definition (inline variable, C++17)

/* Wire stub — not needed for pure-logic modules; declared to satisfy indirect includes */
struct _Wire_t {
    void begin(uint8_t, uint8_t) {}
    void beginTransmission(uint8_t) {}
    void endTransmission(bool = true) {}
    void write(uint8_t) {}
    int  available() { return 0; }
    int  read()      { return 0; }
    int  requestFrom(uint8_t, uint8_t) { return 0; }
};
extern _Wire_t Wire;
inline _Wire_t Wire;

/* snprintf is available in <cstdio>; expose it here for modules that use it via Arduino.h */
#include <cstdio>
