#include "ota.h"

#include <Update.h>
#include "services/prefs.h"

namespace ota {

static WebServer* s_srv     = nullptr;
static bool       s_ota_auth = false;

// Length-constant string comparison to prevent timing oracle on the password.
static bool ct_equal(const char* a, const char* b) {
    size_t la = strlen(a), lb = strlen(b);
    uint8_t diff = (la != lb) ? 1u : 0u;
    size_t  n    = (la < lb) ? la : lb;
    for (size_t i = 0; i < n; i++) {
        diff |= static_cast<uint8_t>(a[i]) ^ static_cast<uint8_t>(b[i]);
    }
    return diff == 0;
}

void register_handler(WebServer& server) {
    s_srv = &server;

    const char* hdrs[] = {"X-OTA-Password", "Content-Type"};
    server.collectHeaders(hdrs, 2);

    // Response handler runs after the upload is fully received.
    auto on_complete = []() {
        if (!s_ota_auth) {
            s_ota_auth = false; // reset for next request
            s_srv->send(401, "text/plain", "Unauthorized");
            return;
        }
        s_ota_auth = false; // reset before restart so it doesn't persist across boots
        if (Update.hasError()) {
            s_srv->send(400, "text/plain", Update.errorString());
        } else {
            s_srv->send(200, "text/plain", "OK");
            delay(100);
            ESP.restart();
        }
    };

    // Upload handler receives firmware chunks.
    // Guard: WebServer also calls _ufn via the raw() path for non-multipart POSTs,
    // where _currentUpload is null.  Calling upload() then crashes.  Return early
    // for non-multipart; s_ota_auth stays false and on_complete sends 401.
    auto on_upload = []() {
        if (s_srv->header("Content-Type").indexOf("multipart") < 0) return;
        HTTPUpload& upload = s_srv->upload();
        if (upload.status == UPLOAD_FILE_START) {
            // Authenticate before writing a single byte.
            s_ota_auth = ct_equal(s_srv->header("X-OTA-Password").c_str(),
                                  prefs::ota_pass().c_str());
            if (s_ota_auth) {
                Update.begin(UPDATE_SIZE_UNKNOWN);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (s_ota_auth) {
                Update.write(upload.buf, upload.currentSize);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (s_ota_auth) {
                Update.end(true);
            }
        }
    };

    server.on("/update", HTTP_POST, on_complete, on_upload);
}

} // namespace ota
