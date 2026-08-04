#pragma once
/* esp_heap_caps.h stub for native host tests.
 * Forwards to the host allocator — capacity flags are ignored. */

#include <cstdlib>
#include <cstdint>

#define MALLOC_CAP_SPIRAM   (1 << 3)
#define MALLOC_CAP_INTERNAL (1 << 4)
#define MALLOC_CAP_8BIT     (1 << 1)
#define MALLOC_CAP_DMA      (1 << 2)

inline void* heap_caps_malloc(size_t size, uint32_t /*caps*/)              { return std::malloc(size); }
inline void* heap_caps_realloc(void* p, size_t size, uint32_t /*caps*/)    { return std::realloc(p, size); }
inline void  heap_caps_free(void* p)                                        { std::free(p); }
inline size_t heap_caps_get_free_size(uint32_t /*caps*/)                   { return SIZE_MAX; }
