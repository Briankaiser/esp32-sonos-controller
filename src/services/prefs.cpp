#include "prefs.h"
#include <Preferences.h>

namespace {
    constexpr const char* kNs    = "opensonos";
    constexpr const char* kSsid  = "wifi_ssid";
    constexpr const char* kPass  = "wifi_pass";
    constexpr const char* kRoom  = "home_room";
    constexpr const char* kGvol  = "grp_vol";
    constexpr const char* kLyrics = "lyrics_on";
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

void prefs::set_home_room(const String& uuid) {
    Preferences p;
    p.begin(kNs, false);
    p.putString(kRoom, uuid);
    p.end();
    Serial.printf("[Prefs] home_room set to %s\n", uuid.c_str());
}

bool prefs::group_volume_mode() {
    Preferences p;
    p.begin(kNs, true);
    bool v = p.getBool(kGvol, false);
    p.end();
    return v;
}

void prefs::set_group_volume_mode(bool whole_group) {
    Preferences p;
    p.begin(kNs, false);
    p.putBool(kGvol, whole_group);
    p.end();
    Serial.printf("[Prefs] group_volume_mode set to %s\n", whole_group ? "whole_group" : "home_room_only");
}

bool prefs::lyrics_enabled() {
    Preferences p;
    p.begin(kNs, true);
    bool v = p.getBool(kLyrics, true);   // default on
    p.end();
    return v;
}

void prefs::set_lyrics_enabled(bool on) {
    Preferences p;
    p.begin(kNs, false);
    p.putBool(kLyrics, on);
    p.end();
    Serial.printf("[Prefs] lyrics_enabled set to %s\n", on ? "on" : "off");
}

String prefs::ota_pass() {
    Preferences p;
    p.begin(kNs, true);
    String v = p.getString(kOta, "");
    p.end();
    return v;
}

void prefs::store_provisioning(const String& ssid, const String& pass,
                               const String& ota) {
    Preferences p;
    p.begin(kNs, false);
    p.putString(kSsid, ssid);
    p.putString(kPass, pass);
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
