#include "soap.h"
#include <WiFi.h>
#include <WiFiClient.h>

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

    WiFiClient client;
    client.setTimeout(5000);
    if (!client.connect(ip.c_str(), port, 5000)) {
        Serial.printf("[SOAP] connect failed %s:%u\n", ip.c_str(), (unsigned)port);
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
    while (!client.available() && millis() < deadline) delay(10);
    String status_line = client.readStringUntil('\n');
    int status_code = 0;
    {
        int sp = status_line.indexOf(' ');
        if (sp >= 0) status_code = status_line.substring(sp + 1, sp + 4).toInt();
    }

    // Consume headers
    while (client.connected() && millis() < deadline) {
        String line = client.readStringUntil('\n');
        if (line == "\r" || line.length() <= 1) break;
    }

    // Read body until connection closes
    String response = "";
    while ((client.connected() || client.available()) && millis() < deadline) {
        while (client.available()) response += (char)client.read();
        delay(1);
    }
    client.stop();

    if (status_code != 200) {
        Serial.printf("[SOAP] HTTP %d: %s %s#%s\n",
                      status_code, ip.c_str(), service_type.c_str(), action.c_str());
        return "";
    }
    return response;
}
