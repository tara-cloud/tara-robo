#include "ota4h.h"
#include <WiFiClient.h>
#include <HTTPUpdate.h>
#include <log4c.h>

static Ota4hStateCallback _stateCb = nullptr;

void ota4h_on_state(Ota4hStateCallback cb) {
    _stateCb = cb;
}

void ota4h_handle(const String& url, const String& version) {
    if (url.length() == 0) { LERROR("ota4h: no url"); return; }

    if (_stateCb) _stateCb("start", -1);
    LINFO("ota4h: starting %s from %s", version.c_str(), url.c_str());

    WiFiClient client;
    httpUpdate.onStart([](){ if (_stateCb) _stateCb("downloading", 0); });
    httpUpdate.onProgress([](int cur, int total) {
        static int last = -1;
        int pct = (total > 0) ? (cur * 100 / total) : 0;
        if (pct / 10 != last / 10) {
            last = pct;
            if (_stateCb) _stateCb("progress", pct);
        }
    });
    httpUpdate.onEnd([](){ if (_stateCb) _stateCb("ok", 100); });
    httpUpdate.onError([](int e) {
        LERROR("ota4h: error %d", e);
        if (_stateCb) _stateCb("failed", e);
    });

    t_httpUpdate_return ret = httpUpdate.update(client, url);
    if (ret == HTTP_UPDATE_FAILED) {
        LERROR("ota4h: failed — %s", httpUpdate.getLastErrorString().c_str());
        if (_stateCb) _stateCb("failed", -1);
    }
}
