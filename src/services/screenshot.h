#pragma once
#include <WebServer.h>

namespace screenshot {

void init();
void register_handler(WebServer& server);

// Call from ui_task after each lv_timer_handler() to service pending requests.
void poll();

} // namespace screenshot
