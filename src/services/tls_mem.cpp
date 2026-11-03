#include "tls_mem.h"

#include <esp_heap_caps.h>
#include <atomic>
#include <mbedtls/platform.h>

namespace {

// Every mbedTLS allocation goes to PSRAM first: the record buffers (16 KB x2)
// and the many small handshake/x509 allocations all count against the ~80 KB of
// internal heap that WiFi, lwIP and DMA share. Falls back to internal RAM if
// PSRAM is exhausted, so TLS degrades instead of failing.

std::atomic<unsigned> s_psram{0}, s_fallback{0};

void* tls_calloc(size_t n, size_t size) {
    void* p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p) { s_psram++; return p; }
    s_fallback++;
    return heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

void tls_free(void* p) { heap_caps_free(p); }

}  // namespace

void tls_mem::install() {
    mbedtls_platform_set_calloc_free(tls_calloc, tls_free);
}

void tls_mem::stats(unsigned& psram_allocs, unsigned& fallback_allocs) {
    psram_allocs    = s_psram;
    fallback_allocs = s_fallback;
}
