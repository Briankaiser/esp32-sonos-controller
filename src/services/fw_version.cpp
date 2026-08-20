#include "fw_version.h"

#if __has_include("fw_version_generated.h")
#include "fw_version_generated.h"
#endif
#ifndef OPENSONOS_FW_VERSION
#define OPENSONOS_FW_VERSION "unknown"  // e.g. a build with no git metadata
#endif

// Embedded, greppable version marker so a distributed firmware.bin is
// self-describing. The string lives in .rodata inside the app image, so the
// build-time SHA-256 already covers it — no post-build patching, unlike the
// ESP32 app-descriptor slot whose version field is fixed by the prebuilt core.
// tools/fleet_update.py scans the .bin for this sentinel to learn the target
// version. fw::version() returns the value part, so this single string serves
// both the runtime accessor and the on-disk marker (and is referenced here, so
// the linker never garbage-collects it).
static const char kMarker[] = "OPENSONOS_FW_VERSION:" OPENSONOS_FW_VERSION;

// Board identifier, embedded the same way. Set from -DOPENSONOS_BOARD_NAME per
// PlatformIO env (see platformio.ini); "unknown" when a build omits the flag.
// The board is fixed config, so a plain build define reaches this TU directly —
// unlike the git-derived version, which needs a generated header.
#ifndef OPENSONOS_BOARD_NAME
#define OPENSONOS_BOARD_NAME "unknown"
#endif
static const char kBoardMarker[] = "OPENSONOS_BOARD:" OPENSONOS_BOARD_NAME;

namespace fw {

const char* version() {
    return kMarker + (sizeof("OPENSONOS_FW_VERSION:") - 1);  // skip the prefix
}

const char* board() {
    return kBoardMarker + (sizeof("OPENSONOS_BOARD:") - 1);  // skip the prefix
}

} // namespace fw
