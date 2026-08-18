#pragma once
#include <Arduino.h>

namespace prefs {

constexpr uint32_t kResetCountdownStartMs =  5000;
constexpr uint32_t kResetTotalMs          = 15000;
constexpr uint32_t kMuteShortPressMs      =  1000;

bool   is_provisioned();

String wifi_ssid();
String wifi_pass();
String home_room();
void   set_home_room(const String& uuid);
bool   group_volume_mode();                   // false = home room only (default); true = whole group
void   set_group_volume_mode(bool whole_group);
bool   lyrics_enabled();                       // true = fetch & show synced lyrics (default)
void   set_lyrics_enabled(bool on);
String ota_pass();

// All-or-nothing write of all provisioning fields.
void store_provisioning(const String& ssid, const String& pass,
                        const String& ota);

// Clears the opensonos NVS namespace and reboots to portal mode.
// Short-press mute is derivable from an EncoderButtonDown/Up pair;
// this reset path is the only consumer of the 30-second hold logic.
void factory_reset();

} // namespace prefs
