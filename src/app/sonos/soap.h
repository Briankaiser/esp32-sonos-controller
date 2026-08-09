#pragma once
#include <Arduino.h>

namespace sonos::soap {
    // Create the SOAP serialization mutex. Call once from setup() before any
    // task that issues SOAP calls is started.
    void init();

    // Synchronous UPnP SOAP call over plain HTTP.
    // Returns response body on HTTP 200; empty string on any error or timeout.
    // Serialized by an internal mutex (per call) so concurrent tasks never open
    // simultaneous connections to the same speaker — some models (e.g. Roam)
    // refuse a second in-flight connection.
    String call(const String& ip, uint16_t port,
                const String& path, const String& service_type,
                const String& action, const String& args_xml);
}
