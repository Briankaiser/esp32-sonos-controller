#include "favorites.h"
#include "soap.h"

#include <Arduino.h>
#include <freertos/semphr.h>

// ── Statics ──────────────────────────────────────────────────────────────────

static SemaphoreHandle_t s_mutex = nullptr;
static FavoritesState    s_state;

// ── Helpers ───────────────────────────────────────────────────────────────────

static String html_decode_fav(const String& s) {
    String r = s;
    r.replace("&lt;",   "<");
    r.replace("&gt;",   ">");
    r.replace("&quot;", "\"");
    r.replace("&apos;", "'");
    r.replace("&#39;",  "'");
    r.replace("&#x27;", "'");
    r.replace("&amp;",  "&");  // must be last
    r.replace("\xe2\x80\x98", "'");   // U+2018 LEFT SINGLE QUOTATION MARK
    r.replace("\xe2\x80\x99", "'");   // U+2019 RIGHT SINGLE QUOTATION MARK
    r.replace("\xe2\x80\x9c", "\"");  // U+201C LEFT DOUBLE QUOTATION MARK
    r.replace("\xe2\x80\x9d", "\"");  // U+201D RIGHT DOUBLE QUOTATION MARK
    return r;
}

static String xml_field_fav(const String& xml, const String& tag) {
    String open  = "<" + tag + ">";
    String close = "</" + tag + ">";
    int s = xml.indexOf(open);
    if (s < 0) return "";
    s += open.length();
    int e = xml.indexOf(close, s);
    if (e < 0) return "";
    return xml.substring(s, e);
}

// ── Public interface ──────────────────────────────────────────────────────────

void favorites::init() {
    s_mutex = xSemaphoreCreateMutex();
}

FavoritesState favorites::snapshot() {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    FavoritesState copy = s_state;
    xSemaphoreGive(s_mutex);
    return copy;
}

void favorites::fetch(const SpeakerInfo& speaker) {
    String args =
        "<ObjectID>FV:2</ObjectID>"
        "<BrowseFlag>BrowseDirectChildren</BrowseFlag>"
        "<Filter>*</Filter>"
        "<StartingIndex>0</StartingIndex>"
        "<RequestedCount>100</RequestedCount>"
        "<SortCriteria></SortCriteria>";

    String resp = sonos::soap::call(
        speaker.ip, speaker.port,
        "/MediaServer/ContentDirectory/Control",
        "urn:schemas-upnp-org:service:ContentDirectory:1",
        "Browse", args);

    if (resp.isEmpty()) {
        Serial.println("[Favorites] fetch failed");
        return;
    }

    // Result element contains HTML-entity-encoded DIDL-Lite
    String raw     = xml_field_fav(resp, "Result");
    String decoded = html_decode_fav(raw);


    std::vector<FavoriteItem> items;

    // Parse both <item> and <container> — Sonos playlists use <container>, tracks use <item>
    static const char* kTags[][2] = {
        { "<item ",    "</item>"    },
        { "<container ", "</container>" },
    };

    for (auto& pair : kTags) {
        const char* open_tag  = pair[0];
        const char* close_tag = pair[1];
        int pos = 0;
        while (true) {
            int s = decoded.indexOf(open_tag, pos);
            if (s < 0) break;
            int e = decoded.indexOf(close_tag, s);
            if (e < 0) break;
            String elem = decoded.substring(s, e + strlen(close_tag));

            FavoriteItem fi;
            fi.title = html_decode_fav(xml_field_fav(elem, "dc:title"));

            // <res> has attributes — find <res followed by > to locate content
            {
                int rs = elem.indexOf("<res");
                if (rs >= 0) {
                    int rc = elem.indexOf('>', rs);
                    if (rc >= 0) {
                        int re = elem.indexOf("</res>", rc);
                        if (re >= 0) fi.uri = elem.substring(rc + 1, re);
                    }
                }
            }

            // r:resMD contains the already-HTML-encoded DIDL-Lite that Sonos expects
            // for SetAVTransportURI — use it directly when available
            String res_md = xml_field_fav(elem, "r:resMD");
            if (!res_md.isEmpty()) {
                fi.metadata       = res_md;
                fi.metadata_ready = true;
            } else {
                fi.metadata       = elem;  // raw <item> XML; will be encoded in drain_commands
                fi.metadata_ready = false;
            }

            if (!fi.title.isEmpty() && !fi.uri.isEmpty()) {
                items.push_back(fi);
            }
            pos = e + strlen(close_tag);
        }
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.items   = items;
    s_state.fetched = true;
    s_state.version++;
    xSemaphoreGive(s_mutex);

    Serial.printf("[Favorites] loaded %u favorites\n", (unsigned)items.size());
}
