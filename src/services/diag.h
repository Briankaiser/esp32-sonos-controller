#pragma once

#include <Arduino.h>
#include <stdint.h>

// Heap-pressure diagnostics for long-unattended runs. A low-priority watcher
// records every dip of internal free heap below ~16 KB (and every new all-time
// low) together with which activities were in flight, plus any failed heap
// allocation. Results are served on /status; nothing here changes behavior.
namespace diag {

enum Activity : uint32_t {
    kArtFetch   = 1u << 0,  // album-art HTTP(S) fetch in flight
    kArtTls     = 1u << 1,  // ...and it is using TLS (https URL)
    kArtDecode  = 1u << 2,  // JPEG decode
    kLyricsTls  = 1u << 3,  // lyrics HTTPS fetch
    kSoap       = 1u << 4,  // SOAP call to a speaker
    kDiscovery  = 1u << 5,  // discovery cycle
    kGenaNotify = 1u << 6,  // inbound GENA NOTIFY handling
};

void begin();  // call once, as early in setup() as possible

void mark(uint32_t bits);
void unmark(uint32_t bits);

struct Scope {
    uint32_t bits;
    explicit Scope(uint32_t b) : bits(b) { mark(bits); }
    ~Scope() { unmark(bits); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};

// Per-fetch internal-RAM cost. A FetchProbe lives for one network fetch; each
// `at()` records how far internal free heap has fallen since the probe was
// created, keeping the largest drop seen per kind and phase since boot. The
// reading is device-wide, so it is an upper bound on the fetch's own cost.
enum FetchKind  : uint8_t { kFetchArtTls = 0, kFetchArtPlain, kFetchLyrics, kFetchKinds };
enum FetchPhase : uint8_t { kPhaseConnect = 0, kPhaseFirstRead, kPhaseDone, kFetchPhases };

class FetchProbe {
public:
    explicit FetchProbe(FetchKind kind);
    ~FetchProbe();
    void at(FetchPhase phase);
    FetchProbe(const FetchProbe&) = delete;
    FetchProbe& operator=(const FetchProbe&) = delete;
private:
    FetchKind kind_;
    uint32_t  base_;
};

// JSON object members (no leading/trailing comma, no braces) for /status.
String json_fragment();

}  // namespace diag
