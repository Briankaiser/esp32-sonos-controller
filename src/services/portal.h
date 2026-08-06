#pragma once

namespace portal {

// Blocking captive-portal loop.
// Starts WiFi AP, DNS redirect, and HTTP server; serves a setup form.
// Calls ESP.restart() after successful form submission and never returns.
void run();

} // namespace portal
