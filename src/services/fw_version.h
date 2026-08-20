#pragma once

// Firmware version string. extra_scripts/fw_version.py generates
// fw_version_generated.h at build time (from `git describe --tags --always
// --dirty`) defining OPENSONOS_FW_VERSION. fw::version() returns it.
//
// The same value is embedded in the app image behind a greppable
// "OPENSONOS_FW_VERSION:" marker (see fw_version.cpp), so a distributed
// firmware.bin is self-describing: tools/fleet_update.py reads the target
// version straight from the image without a companion file.
//
// board() returns the board identifier the image was built for (the PlatformIO
// env name, e.g. "crowpanel_21_round"), set from the -DOPENSONOS_BOARD_NAME
// build flag. It is embedded the same way behind an "OPENSONOS_BOARD:" marker so
// the updater can tell which board an image targets and flash only matching
// devices. Falls back to "unknown" when the build flag is absent.
namespace fw {

const char* version();
const char* board();

} // namespace fw
