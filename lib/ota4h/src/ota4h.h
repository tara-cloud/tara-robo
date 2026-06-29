#pragma once
#include <Arduino.h>
#include <functional>

using Ota4hStateCallback = std::function<void(const String& state, int pct)>;

void ota4h_on_state(Ota4hStateCallback cb);
void ota4h_handle(const String& url, const String& version);
