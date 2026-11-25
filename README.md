# Matter-Smart-Button
# Compatibility
Amazon Echo (4th Gen): Fully supports Matter Generic Switch, Other Smart Home Ecosystems: Because Matter supports Multi-Admin, you can pair the ESP32-C6 with Alexa first and then share it with Google Home, Apple Home, or Home Assistant simultaneously.

# 1. Eight Matter Generic Switch Endpoints (Short &amp; Long Press Mapping) using 4 buttons with Seeed Studio XIAO ESP32-C6
Dual-Action Split: Four physical pushbuttons (D1, D3, D6, D7) map to eight independent MatterGenericSwitch,
Endpoint Layout: Indices 0..3 handle short presses for physical buttons 1 through 4, while indices 4..7 handle long presses (700 ms hold threshold) for buttons 1 through 4
Event Dispatch: Short releases prior to 700 ms call matterButtons[i].click(), and holds reaching 700 ms call matterButtons[i + 4].click() once per press cycle.

# 2. Non-Blocking Debouncing & Press Detection
Debounce Window: Each input uses INPUT_PULLUP and a 50 ms software debounce timer (DEBOUNCE_MS) so mechanical switch bounce does not register false presses or releases.
Non-Blocking Loop: Timing relies on millis() comparisons rather than delay() during normal operation, keeping button polling and status animations responsive.
# 3. Eight-LED WS2812B Feedback & Status Animations
Per-Button LED Pairs: Eight NEO_GRB / 800 kHz NeoPixel LEDs on D2 (GPIO 2) are grouped into pairs (i * 2 and i * 2 + 1) for each of the four buttons at a global brightness of 60.
Action & Standby States: When connected, each button pair flashes its assigned short-press color (Cyan, Emerald Green, Warm Amber, or Violet) for 400 ms, or bright white (255, 255, 255) for 550 ms on a long press, returning to a dim standby glow (8, 8, 8).
Commissioning & Connection Chase: Before commissioning, a yellow chase animation (255, 160, 0) runs every 120 ms; after commissioning but before full connection, a blue chase animation (0, 80, 255) runs every 120 ms without overlapping normal button LED updates.

# In the Alexa app, go to Devices -> tap + -> Add Device -> select Matter (or Other -> Matter) -> tap Yes -> Try Numeric Code -> enter the 11-digit code from the Serial Monitor.
Verification: The NeoPixel animation changes from yellow to blue, then settles into the dim white standby glow once connected, and Alexa discovers 8 switch endpoints.
