#pragma once

// Firmware version string. extra_scripts/fw_version.py generates
// fw_version_generated.h at build time (from `git describe --tags --always
// --dirty`) defining OPENSONOS_FW_VERSION. fw::version() returns it.
//
// The same value is embedded in the app image behind a greppable
// "OPENSONOS_FW_VERSION:" marker (see fw_version.cpp), so a distributed
// firmware.bin is self-describing: tools/fleet_update.py reads the target
// version straight from the image without a companion file.
namespace fw {

const char* version();

} // namespace fw
