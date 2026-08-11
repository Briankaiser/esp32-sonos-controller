#include "soap.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Serializes all SOAP calls across tasks (cmd_task, sonos_task, favorites, etc.)
// so no two connections to the same speaker are ever open at once. Held only for
// the duration of a single call — a higher-priority waiter (cmd_task) acquires it
// the instant the current call finishes, via priority inheritance.
static SemaphoreHandle_t s_net_mutex = nullptr;

void sonos::soap::init() {
    if (!s_net_mutex) s_net_mutex = xSemaphoreCreateMutex();
}

// Wrap-safe deadline check — millis() < deadline inverts at the 32-bit rollover
// (~49.7 days uptime), which would make every SOAP call return instantly empty
// for the ~1 minute it takes millis() to cross back past `deadline`.
static inline bool time_before(uint32_t deadline) {
    return (int32_t)(millis() - deadline) < 0;
}

// Read one line (up to and including '\n') from the client, yielding while it
// waits for data. Replaces Stream::readStringUntil, which busy-spins in
// Stream::timedRead with no task yield and can starve the IDLE task (→ watchdog).
// Returns the line without the trailing CRLF. Stops at the deadline.
static String read_line(WiFiClient& client, uint32_t deadline) {
    String line;
    while (time_before(deadline)) {
        if (client.available()) {
            char c = (char)client.read();
            if (c == '\n') break;
            if (c != '\r') line += c;
        } else if (client.connected()) {
            delay(1);  // yield to IDLE / other tasks while waiting for data
        } else {
            break;  // connection closed and no buffered data
        }
    }
    return line;
}

String sonos::soap::call(const String& ip, uint16_t port,
                         const String& path, const String& service_type,
                         const String& action, const String& args_xml) {
    String body =
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body>"
        "<u:" + action + " xmlns:u=\"" + service_type + "\">" +
        args_xml +
        "</u:" + action + ">"
        "</s:Body></s:Envelope>";

    if (s_net_mutex) xSemaphoreTake(s_net_mutex, portMAX_DELAY);

    WiFiClient client;
    client.setTimeout(5000);
    if (!client.connect(ip.c_str(), port, 5000)) {
        Serial.printf("[SOAP] connect failed %s:%u\n", ip.c_str(), (unsigned)port);
        if (s_net_mutex) xSemaphoreGive(s_net_mutex);
        return "";
    }

    client.printf(
        "POST %s HTTP/1.1\r\n"
        "HOST: %s:%u\r\n"
        "Content-Type: text/xml; charset=\"utf-8\"\r\n"
        "SOAPAction: \"%s#%s\"\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n",
        path.c_str(), ip.c_str(), (unsigned)port,
        service_type.c_str(), action.c_str(),
        (unsigned)body.length());
    client.print(body);

    // Read status line
    uint32_t deadline = millis() + 5000;
    String status_line = read_line(client, deadline);
    int status_code = 0;
    {
        int sp = status_line.indexOf(' ');
        if (sp >= 0) status_code = status_line.substring(sp + 1, sp + 4).toInt();
    }

    // Consume headers; detect chunked transfer encoding
    bool is_chunked = false;
    while (client.connected() && time_before(deadline)) {
        String line = read_line(client, deadline);
        line.trim();
        if (line.isEmpty()) break;
        if (line.indexOf("chunked") >= 0) is_chunked = true;
    }

    // Read body — decode chunked encoding if present. Bulk readBytes (instead
    // of appending one char at a time) avoids the repeated small String
    // reallocations that fragment the heap over long uptime.
    String response = "";
    char   buf[512];
    if (is_chunked) {
        while ((client.connected() || client.available()) && time_before(deadline)) {
            String size_line = read_line(client, deadline);
            size_line.trim();
            if (size_line.isEmpty()) continue;
            uint32_t chunk_size = strtoul(size_line.c_str(), nullptr, 16);
            if (chunk_size == 0) break;
            for (uint32_t i = 0; i < chunk_size && time_before(deadline); ) {
                int av = client.available();
                if (av > 0) {
                    size_t want = chunk_size - i;
                    if (want > sizeof(buf)) want = sizeof(buf);
                    if ((size_t)av < want) want = (size_t)av;
                    size_t got = client.readBytes(buf, want);
                    response.concat(buf, got);
                    i += (uint32_t)got;
                } else if (client.connected()) {
                    delay(1);
                } else {
                    break;
                }
            }
            read_line(client, deadline);  // trailing CRLF after chunk
        }
    } else {
        while ((client.connected() || client.available()) && time_before(deadline)) {
            int av = client.available();
            if (av > 0) {
                size_t want = (size_t)av > sizeof(buf) ? sizeof(buf) : (size_t)av;
                size_t got = client.readBytes(buf, want);
                response.concat(buf, got);
            } else {
                delay(1);
            }
        }
    }
    client.stop();

    if (s_net_mutex) xSemaphoreGive(s_net_mutex);

    if (status_code != 200) {
        Serial.printf("[SOAP] HTTP %d: %s#%s\n",
                      status_code, service_type.c_str(), action.c_str());
        return "";
    }
    return response;
}
