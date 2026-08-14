#pragma once
#include <WebServer.h>

namespace ota {

void register_handler(WebServer& server);

// True once a firmware upload has started (authenticated), until the device
// reboots on success or the update fails/aborts. Polled by the UI task to show
// a static overlay and quiesce rendering during flash writes.
bool is_active();

} // namespace ota
