#pragma once

// Route mbedTLS heap allocations to PSRAM. The prebuilt IDF libs force all TLS
// memory into internal RAM (CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC), and one https
// session (record buffers + handshake) drains the ~80 KB of internal heap that
// WiFi, lwIP and DMA share. Call once, early in setup(), before any TLS use.
namespace tls_mem {
void install();

// Counters for /status: {psram_allocs, psram_bytes, internal_allocs, internal_bytes, fallbacks}.
void stats(unsigned out[5]);
}
