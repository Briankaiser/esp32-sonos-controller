#pragma once
#include <WebServer.h>

namespace status {

// Registers GET /status — a cache-only JSON snapshot of device state
// (identity marker, firmware version, network status, home room, and whether
// Sonos state is driven by GENA eventing or polling). Reads only already-held
// state; performs no SSDP, SOAP, or other network I/O.
void register_handler(WebServer& server);

} // namespace status
