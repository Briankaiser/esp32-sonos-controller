#include "portal.h"
#include "prefs.h"
#include "../bsp/bsp.h"

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <lvgl.h>
#include <algorithm>
#include <vector>

static const char kSetupPage[] = R"HTML(<!doctype html>
<html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>OpenSonos Setup</title>
<style>
body{font-family:sans-serif;max-width:460px;margin:2em auto;padding:0 1em;background:#f5f5f5}
h1{font-size:1.3em}
label{display:block;margin-top:1em;font-size:.9em;color:#555}
input,select{width:100%;box-sizing:border-box;padding:.5em;font-size:1em;border:1px solid #ccc;border-radius:4px;background:#fff}
.hint{font-size:.78em;color:#888;margin-top:.3em}
.hidden-row{margin-top:.6em}
button{margin-top:1.5em;width:100%;padding:.7em;font-size:1em;background:#1565c0;color:#fff;border:none;border-radius:4px;cursor:pointer}
#scan-status{font-size:.82em;color:#888;margin-top:.3em}
</style>
</head><body>
<h1>OpenSonos Setup</h1>
<form method=POST action=/save id=f>
<input type=hidden name=ssid id=ssid-val>
<label>WiFi Network <span style=color:red>*</span></label>
<select id=ssid-pick><option value=''>Scanning...</option></select>
<div class=hint id=scan-status>Looking for networks&hellip;</div>
<div class=hidden-row>
<label style="margin-top:.5em;font-size:.82em;color:#888">
<input type=checkbox id=manual-cb onchange="toggleManual(this.checked)" style="width:auto;margin-right:.3em">
My network isn&rsquo;t listed (hidden network)
</label>
<input id=ssid-manual type=text placeholder="Enter network name" autocomplete=off
       style="display:none;margin-top:.4em" oninput="syncSsid()">
</div>
<label style="margin-top:1.2em">WiFi Password <span style=color:red>*</span></label>
<input name=pass type=password autocomplete=off required>
<label>OTA Password</label>
<input name=ota type=password autocomplete=off>
<button type=submit>Save &amp; Reboot</button>
</form>
<script>
var nets=[];
function bars(rssi){
  var s=rssi<=-90?0:rssi<=-75?1:rssi<=-60?2:rssi<=-45?3:4;
  return ['▁___','▁▂__','▁▂▃_','▁▂▃▄','▁▂▃▄'][s];
}
function syncSsid(){
  var cb=document.getElementById('manual-cb');
  var val=cb.checked
    ?document.getElementById('ssid-manual').value.trim()
    :document.getElementById('ssid-pick').value;
  document.getElementById('ssid-val').value=val;
}
function toggleManual(on){
  var m=document.getElementById('ssid-manual');
  var p=document.getElementById('ssid-pick');
  m.style.display=on?'block':'none';
  p.disabled=on;
  syncSsid();
}
function populate(list){
  var sel=document.getElementById('ssid-pick');
  sel.innerHTML='';
  if(!list.length){
    sel.innerHTML='<option value="">No networks found</option>';
    document.getElementById('scan-status').textContent='No networks found. Use the manual entry below.';
    document.getElementById('manual-cb').checked=true;
    toggleManual(true);
    return;
  }
  list.forEach(function(n){
    var o=document.createElement('option');
    o.value=n.ssid;
    o.textContent=bars(n.rssi)+' '+n.ssid;
    sel.appendChild(o);
  });
  document.getElementById('scan-status').textContent=list.length+' network'+(list.length!==1?'s':'')+' found';
  syncSsid();
}
sel=document.getElementById('ssid-pick');
sel.addEventListener('change',syncSsid);
document.getElementById('f').addEventListener('submit',function(e){
  if(!document.getElementById('ssid-val').value.trim()){e.preventDefault();alert('Please select or enter a WiFi network name.');}
});
fetch('/scan').then(function(r){return r.json();}).then(populate).catch(function(){
  document.getElementById('scan-status').textContent='Scan failed. Enter network name manually.';
  document.getElementById('manual-cb').checked=true;
  toggleManual(true);
});
</script>
</body></html>)HTML";

static const char kConfirmPage[] = R"(<!doctype html>
<html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Setup Complete</title>
<style>body{font-family:sans-serif;max-width:460px;margin:2em auto;padding:0 1em;background:#f5f5f5;text-align:center}
h1{color:#2e7d32}p{color:#555}</style>
</head><body>
<h1>Setup complete!</h1>
<p>Rebooting now. The device will connect to your WiFi network.</p>
<p>You can close this page.</p>
</body></html>)";

static const char kErrorPage[] = R"(<!doctype html>
<html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Missing fields</title>
<style>body{font-family:sans-serif;max-width:460px;margin:2em auto;padding:0 1em;background:#f5f5f5}
h1{color:#c62828}a{color:#1565c0}</style>
</head><body>
<h1>Missing required fields</h1>
<p>WiFi SSID and password are required.</p>
<p><a href=/>Go back</a></p>
</body></html>)";

static WebServer s_server(80);
static DNSServer s_dns;
static bool      s_reboot_pending  = false;
static bool      s_factory_pending = false;

void portal::run() {
    // Bring up LVGL and the display so the setup screen is visible before WiFi
    // starts. This runs on core 1 (Arduino setup()), so the LVGL-on-core-1
    // constraint is satisfied. bsp::init() was called before us in setup().
    lv_init();
    bsp::display_init();

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "Initial Setup");
    lv_obj_set_width(title, lv_pct(60));
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(title, lv_pct(20), lv_pct(33));

    char ap_ssid[28];
    snprintf(ap_ssid, sizeof(ap_ssid), "OpenSonos-Setup-%04X",
             (unsigned)((ESP.getEfuseMac() >> 32) & 0xFFFF));

    lv_obj_t* body = lv_label_create(scr);
    lv_label_set_text_fmt(body, "Connect to %s to start setup.", ap_ssid);
    lv_obj_set_width(body, lv_pct(60));
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(body, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(body, lv_pct(20), lv_pct(52));

    lv_screen_load(scr);
    lv_timer_handler();  // flush the first frame before backlight comes on
    bsp::backlight(255);

    WiFi.softAP(ap_ssid);
    delay(100);

    // Redirect all DNS queries to the portal IP
    s_dns.start(53, "*", IPAddress(192, 168, 4, 1));

    s_server.on("/", HTTP_GET, []() {
        s_server.send(200, "text/html", kSetupPage);
    });

    s_server.on("/scan", HTTP_GET, []() {
        // Switch to AP+STA so the STA radio can scan while the AP stays up.
        WiFi.mode(WIFI_AP_STA);
        int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/false);

        // Deduplicate by SSID: keep highest RSSI per network (handles mesh APs).
        struct Net { String ssid; int32_t rssi; };
        std::vector<Net> nets;
        for (int i = 0; i < n && i < 30; i++) {
            String s = WiFi.SSID(i);
            if (s.isEmpty()) continue;
            int32_t r = WiFi.RSSI(i);
            bool found = false;
            for (auto& net : nets) {
                if (net.ssid == s) { if (r > net.rssi) net.rssi = r; found = true; break; }
            }
            if (!found) nets.push_back({s, r});
        }
        WiFi.scanDelete();
        // Sort strongest first
        std::sort(nets.begin(), nets.end(),
                  [](const Net& a, const Net& b){ return a.rssi > b.rssi; });

        String json = "[";
        for (size_t i = 0; i < nets.size(); i++) {
            if (i) json += ",";
            String escaped = nets[i].ssid;
            escaped.replace("\\", "\\\\");
            escaped.replace("\"", "\\\"");
            json += "{\"ssid\":\"" + escaped + "\",\"rssi\":" + nets[i].rssi + "}";
        }
        json += "]";
        s_server.send(200, "application/json", json);

        // Restore AP-only mode now that the scan is done.
        WiFi.mode(WIFI_AP);
    });

    s_server.on("/save", HTTP_POST, []() {
        String ssid = s_server.arg("ssid");
        String pass = s_server.arg("pass");
        if (ssid.isEmpty() || pass.isEmpty()) {
            s_server.send(400, "text/html", kErrorPage);
            return;
        }
        prefs::store_provisioning(
            ssid, pass,
            s_server.arg("ota")
        );
        s_server.send(200, "text/html", kConfirmPage);
        s_reboot_pending = true;
    });

    // Debug / hardware-test factory reset endpoint
    s_server.on("/reset", HTTP_GET, []() {
        s_server.send(200, "text/plain",
            "Factory reset initiated. Reconnect to Sonos-Controller-Setup after reboot.");
        s_factory_pending = true;
    });

    // Captive portal: redirect any unmatched path back to /
    s_server.onNotFound([]() {
        s_server.sendHeader("Location", "http://192.168.4.1/");
        s_server.send(302, "text/plain", "");
    });

    s_server.begin();
    Serial.println("[portal] AP up — connect to Sonos-Controller-Setup");

    for (;;) {
        s_dns.processNextRequest();
        s_server.handleClient();
        lv_timer_handler();
        delay(5);
        if (s_reboot_pending) {
            delay(2000);
            ESP.restart();
        }
        if (s_factory_pending) {
            delay(1000);
            prefs::factory_reset();  // clears NVS, then ESP.restart()
        }
    }
}
