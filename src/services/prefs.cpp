#include "prefs.h"
#include <Preferences.h>

namespace {
    constexpr const char* kNs    = "opensonos";
    constexpr const char* kSsid  = "wifi_ssid";
    constexpr const char* kPass  = "wifi_pass";
    constexpr const char* kRoom  = "home_room";
    constexpr const char* kLat   = "lat";
    constexpr const char* kLon   = "lon";
    constexpr const char* kOta   = "ota_pass";
}

bool prefs::is_provisioned() {
    Preferences p;
    p.begin(kNs, true);
    String ssid = p.getString(kSsid, "");
    String pass = p.getString(kPass, "");
    p.end();
    return !ssid.isEmpty() && !pass.isEmpty();
}

String prefs::wifi_ssid() {
    Preferences p;
    p.begin(kNs, true);
    String v = p.getString(kSsid, "");
    p.end();
    return v;
}

String prefs::wifi_pass() {
    Preferences p;
    p.begin(kNs, true);
    String v = p.getString(kPass, "");
    p.end();
    return v;
}

String prefs::home_room() {
    Preferences p;
    p.begin(kNs, true);
    String v = p.getString(kRoom, "");
    p.end();
    return v;
}

String prefs::lat() {
    Preferences p;
    p.begin(kNs, true);
    String v = p.getString(kLat, "");
    p.end();
    return v;
}

String prefs::lon() {
    Preferences p;
    p.begin(kNs, true);
    String v = p.getString(kLon, "");
    p.end();
    return v;
}

String prefs::ota_pass() {
    Preferences p;
    p.begin(kNs, true);
    String v = p.getString(kOta, "");
    p.end();
    return v;
}

void prefs::store_provisioning(const String& ssid, const String& pass,
                               const String& lat_val, const String& lon_val,
                               const String& ota) {
    Preferences p;
    p.begin(kNs, false);
    p.putString(kSsid, ssid);
    p.putString(kPass, pass);
    p.putString(kLat,  lat_val);
    p.putString(kLon,  lon_val);
    p.putString(kOta,  ota);
    p.end();
}

void prefs::factory_reset() {
    Preferences p;
    p.begin(kNs, false);
    p.clear();
    p.end();
    ESP.restart();
}
