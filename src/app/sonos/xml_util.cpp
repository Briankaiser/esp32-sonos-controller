#include "xml_util.h"

String sonos::xml::field(const String& xml, const String& tag) {
    String open  = "<" + tag + ">";
    String close = "</" + tag + ">";
    int s = xml.indexOf(open);
    if (s < 0) return "";
    s += open.length();
    int e = xml.indexOf(close, s);
    if (e < 0) return "";
    return xml.substring(s, e);
}

String sonos::xml::attr(const String& element, const String& attr_name) {
    String pat = attr_name + "=\"";
    int s = element.indexOf(pat);
    if (s < 0) return "";
    s += pat.length();
    int e = element.indexOf('"', s);
    if (e < 0) return "";
    return element.substring(s, e);
}

String sonos::xml::html_decode(const String& s) {
    String r = s;
    r.replace("&lt;",   "<");
    r.replace("&gt;",   ">");
    r.replace("&quot;", "\"");
    r.replace("&apos;", "'");
    r.replace("&#39;",  "'");
    r.replace("&#x27;", "'");
    r.replace("&amp;",  "&");  // must be last
    // Normalize Unicode smart quotes to ASCII (Montserrat font lacks U+2018/U+2019)
    r.replace("\xe2\x80\x98", "'");  // U+2018 LEFT SINGLE QUOTATION MARK
    r.replace("\xe2\x80\x99", "'");  // U+2019 RIGHT SINGLE QUOTATION MARK
    r.replace("\xe2\x80\x9c", "\""); // U+201C LEFT DOUBLE QUOTATION MARK
    r.replace("\xe2\x80\x9d", "\""); // U+201D RIGHT DOUBLE QUOTATION MARK
    return r;
}

uint32_t sonos::xml::parse_time(const String& hmmss) {
    int first = hmmss.indexOf(':');
    if (first < 0) return 0;
    int second = hmmss.indexOf(':', first + 1);
    if (second < 0) return 0;
    uint32_t h = (uint32_t)hmmss.substring(0, first).toInt();
    uint32_t m = (uint32_t)hmmss.substring(first + 1, second).toInt();
    uint32_t s = (uint32_t)hmmss.substring(second + 1).toInt();
    return h * 3600u + m * 60u + s;
}

String sonos::xml::tag_val(const String& xml, const String& tag) {
    String open = "<" + tag;
    int s = xml.indexOf(open);
    if (s < 0) return "";
    // Guard against prefix matches (<Volume matching <VolumeDB): the next char
    // must terminate the tag name.
    while (s >= 0) {
        char c = xml[s + open.length()];
        if (c == ' ' || c == '>' || c == '/') break;
        s = xml.indexOf(open, s + 1);
    }
    if (s < 0) return "";
    int e = xml.indexOf('>', s);
    if (e < 0) return "";
    return attr(xml.substring(s, e + 1), "val");
}

String sonos::xml::tag_val_channel(const String& xml, const String& tag,
                                   const String& channel, bool& found) {
    found = false;
    String open = "<" + tag;
    int s = 0;
    while (true) {
        s = xml.indexOf(open, s);
        if (s < 0) return "";
        char c = xml[s + open.length()];
        if (c != ' ' && c != '>' && c != '/') { s += 1; continue; }  // prefix match
        int e = xml.indexOf('>', s);
        if (e < 0) return "";
        String element = xml.substring(s, e + 1);
        if (attr(element, "channel") == channel) {
            found = true;
            return attr(element, "val");
        }
        s = e + 1;
    }
}
