#pragma once

// Firmware version string. extra_scripts/fw_version.py generates
// fw_version_generated.h at build time (from `git describe --tags --always
// --dirty`) defining OPENSONOS_FW_VERSION. If that generated header is absent
// (e.g. a build that skips the script), fall back to "unknown" so the reported
// version is never empty.
#if __has_include("fw_version_generated.h")
#include "fw_version_generated.h"
#endif

#ifndef OPENSONOS_FW_VERSION
#define OPENSONOS_FW_VERSION "unknown"
#endif

namespace fw {

inline const char* version() { return OPENSONOS_FW_VERSION; }

} // namespace fw
