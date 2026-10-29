#pragma once

#include <stdint.h>

// Only one TLS session at a time across art and lyrics: each WiFiClientSecure
// holds ~34 KB of internal RAM (two 16 KB record buffers plus handshake state),
// and two overlapping sessions plus fragmentation exhaust the internal heap.
namespace tls_gate {

bool acquire(uint32_t timeout_ms);
void release();

class Guard {
public:
    // engage=false makes the guard a no-op (plain-HTTP fetches don't need it).
    explicit Guard(bool engage, uint32_t timeout_ms = 30000)
        : held_(engage && acquire(timeout_ms)), engaged_(engage) {}
    ~Guard() { if (held_) release(); }
    bool ok() const { return !engaged_ || held_; }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
private:
    bool held_;
    bool engaged_;
};

}  // namespace tls_gate
