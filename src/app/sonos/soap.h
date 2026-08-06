#pragma once
#include <Arduino.h>

namespace sonos::soap {
    // Synchronous UPnP SOAP call over plain HTTP.
    // Returns response body on HTTP 200; empty string on any error or timeout.
    String call(const String& ip, uint16_t port,
                const String& path, const String& service_type,
                const String& action, const String& args_xml);
}
