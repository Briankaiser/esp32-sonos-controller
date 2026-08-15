#pragma once

// Firmware version string, injected at build time by extra_scripts/fw_version.py
// as OPENSONOS_FW_VERSION (from `git describe --tags --always --dirty`). Falls
// back to "unknown" when the macro is undefined so the value is never empty.
#ifndef OPENSONOS_FW_VERSION
#define OPENSONOS_FW_VERSION "unknown"
#endif

namespace fw {

inline const char* version() { return OPENSONOS_FW_VERSION; }

} // namespace fw
