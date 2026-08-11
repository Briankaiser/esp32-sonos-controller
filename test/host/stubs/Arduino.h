#pragma once
/* Arduino.h stub for native host tests.
 * Provides the minimum surface that production modules reference.
 * All implementations are no-ops or trivial returns — except String,
 * which is a functional std::string-backed port so the pure-logic
 * parsers (xml_util, gena_parse) run for real under test_native. */

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <cctype>

/* ── Arduino String (functional, std::string-backed) ─────────────────────── */
class String {
    std::string s_;
public:
    String() {}
    String(const char* c) : s_(c ? c : "") {}
    String(const std::string& s) : s_(s) {}
    String(char c) : s_(1, c) {}
    String(int v)           : s_(std::to_string(v)) {}
    String(unsigned v)      : s_(std::to_string(v)) {}
    String(long v)          : s_(std::to_string(v)) {}
    String(unsigned long v) : s_(std::to_string(v)) {}

    unsigned    length()  const { return (unsigned)s_.size(); }
    bool        isEmpty() const { return s_.empty(); }
    const char* c_str()   const { return s_.c_str(); }

    char operator[](unsigned i) const { return i < s_.size() ? s_[i] : '\0'; }
    char operator[](int i)      const { return (i >= 0 && i < (int)s_.size()) ? s_[i] : '\0'; }

    int indexOf(const String& sub, int from = 0) const {
        if (from < 0) from = 0;
        if ((size_t)from > s_.size()) return -1;
        size_t p = s_.find(sub.s_, (size_t)from);
        return p == std::string::npos ? -1 : (int)p;
    }
    int indexOf(char c, int from = 0) const {
        if (from < 0) from = 0;
        if ((size_t)from > s_.size()) return -1;
        size_t p = s_.find(c, (size_t)from);
        return p == std::string::npos ? -1 : (int)p;
    }

    String substring(int a) const {
        if (a < 0) a = 0;
        if ((size_t)a >= s_.size()) return String();
        return String(s_.substr((size_t)a));
    }
    String substring(int a, int b) const {
        if (a < 0) a = 0;
        if (b > (int)s_.size()) b = (int)s_.size();
        if (a >= b) return String();
        return String(s_.substr((size_t)a, (size_t)(b - a)));
    }

    bool startsWith(const String& p) const {
        return s_.compare(0, p.s_.size(), p.s_) == 0;
    }
    bool endsWith(const String& p) const {
        return s_.size() >= p.s_.size() &&
               s_.compare(s_.size() - p.s_.size(), p.s_.size(), p.s_) == 0;
    }

    void replace(const String& from, const String& to) {
        if (from.s_.empty()) return;
        size_t pos = 0;
        while ((pos = s_.find(from.s_, pos)) != std::string::npos) {
            s_.replace(pos, from.s_.size(), to.s_);
            pos += to.s_.size();
        }
    }

    void trim() {
        size_t b = s_.find_first_not_of(" \t\r\n");
        size_t e = s_.find_last_not_of(" \t\r\n");
        s_ = (b == std::string::npos) ? "" : s_.substr(b, e - b + 1);
    }

    void toLowerCase() {
        std::transform(s_.begin(), s_.end(), s_.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
    }

    long toInt() const { return strtol(s_.c_str(), nullptr, 10); }

    void reserve(unsigned n) { s_.reserve(n); }
    void concat(const char* buf, unsigned n) { s_.append(buf, n); }

    String& operator+=(const String& o) { s_ += o.s_; return *this; }
    String& operator+=(const char* o)   { s_ += (o ? o : ""); return *this; }
    String& operator+=(char c)          { s_ += c; return *this; }

    friend String operator+(const String& a, const String& b) { return String(a.s_ + b.s_); }
    friend String operator+(const String& a, const char* b)   { return String(a.s_ + (b ? b : "")); }
    friend String operator+(const char* a, const String& b)   { return String(std::string(a ? a : "") + b.s_); }

    friend bool operator==(const String& a, const String& b) { return a.s_ == b.s_; }
    friend bool operator==(const String& a, const char* b)   { return a.s_ == (b ? b : ""); }
    friend bool operator==(const char* a, const String& b)   { return b == a; }
    friend bool operator!=(const String& a, const String& b) { return !(a == b); }
    friend bool operator!=(const String& a, const char* b)   { return !(a == b); }
};

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
