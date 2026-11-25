#include <Matter.h>
#include <Adafruit_NeoPixel.h>

// --- Pin Definitions (Seeed Studio XIAO ESP32-C6) ---
#define LED_PIN     D2   // GPIO 2: WS2812B Data In
#define NUM_LEDS    8    // 8 RGB LEDs (2 LEDs per physical button)

const uint8_t BTN_PINS[4] = {
  D1, // Physical Button 1 -> GPIO 1
  D3, // Physical Button 2 -> GPIO 21
  D6, // Physical Button 3 -> GPIO 16
  D7  // Physical Button 4 -> GPIO 17
};

// --- WS2812B Setup ---
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

const uint32_t SHORT_COLORS[4] = {
  strip.Color(0, 180, 255),   // Button 1 Short: Cyan
  strip.Color(0, 220, 90),    // Button 2 Short: Emerald Green
  strip.Color(255, 140, 0),   // Button 3 Short: Warm Amber
  strip.Color(180, 60, 255)   // Button 4 Short: Violet
};
const uint32_t LONG_COLOR    = strip.Color(255, 255, 255); // Bright White for Long Press
const uint32_t COLOR_STANDBY = strip.Color(8, 8, 8);       // Dim standby glow

uint32_t activeLedColor[4] = {COLOR_STANDBY, COLOR_STANDBY, COLOR_STANDBY, COLOR_STANDBY};
unsigned long ledFlashUntil[4] = {0, 0, 0, 0};

// --- 8 Matter Smart Button Endpoints ---
// Index 0..3 = Buttons 1..4 Short Press
// Index 4..7 = Buttons 1..4 Long Press
MatterGenericSwitch matterButtons[8];

// --- Timing & Debounce Constants ---
const unsigned long DEBOUNCE_MS   = 50;
const unsigned long LONG_PRESS_MS = 700;   // Hold 0.7s for Long Press
const unsigned long RESET_HOLD_MS = 5000;  // Hold Btn 1 + Btn 4 for 5s to reset

bool lastReading[4]        = {HIGH, HIGH, HIGH, HIGH};
bool stableState[4]        = {HIGH, HIGH, HIGH, HIGH};
unsigned long lastDebounce[4]   = {0, 0, 0, 0};
unsigned long pressStartTime[4] = {0, 0, 0, 0};
bool longPressFired[4]     = {false, false, false, false};
unsigned long comboResetStart   = 0;

void updateButtonLEDs() {
  unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    uint32_t col = (now < ledFlashUntil[i]) ? activeLedColor[i] : COLOR_STANDBY;
    strip.setPixelColor(i * 2, col);
    strip.setPixelColor(i * 2 + 1, col);
  }
  strip.show();
}

void showStatusAnimation(uint8_t r, uint8_t g, uint8_t b) {
  static uint8_t step = 0;
  strip.clear();
  strip.setPixelColor(step % NUM_LEDS, strip.Color(r, g, b));
  strip.setPixelColor((step + 1) % NUM_LEDS, strip.Color(r / 4, g / 4, b / 4));
  strip.show();
  step++;
}

void triggerFlash(uint8_t btnIndex, uint32_t color, unsigned long durationMs) {
  activeLedColor[btnIndex] = color;
  ledFlashUntil[btnIndex]  = millis() + durationMs;
}

void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.setBrightness(60);
  strip.clear();
  strip.show();

  for (int i = 0; i < 4; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLUP);
  }

  // Register all 8 Matter Generic Switch endpoints (1-4: Short, 5-8: Long)
  for (int i = 0; i < 8; i++) {
    matterButtons[i].begin();
  }

  Matter.begin();

  if (!Matter.isDeviceCommissioned()) {
    Serial.println("Device not paired yet.");
    Serial.printf("Manual Pairing Code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR Code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
  }
}

void loop() {
  unsigned long now = millis();
  static unsigned long lastAnimTime = 0;

  bool isCommissioned = Matter.isDeviceCommissioned();
  bool isConnected = Matter.isDeviceConnected();

  // 1. Check Pairing & Wi-Fi Connection Status (Non-blocking)
  if (!isCommissioned) {
    if (now - lastAnimTime > 120) {
      showStatusAnimation(255, 160, 0); // Yellow chase = Ready to pair
      lastAnimTime = now;
    }
  } else if (!isConnected) {
    if (now - lastAnimTime > 120) {
      showStatusAnimation(0, 80, 255);  // Blue chase = Connecting to Wi-Fi/Echo
      lastAnimTime = now;
    }
  }

  // 2. Poll & Debounce the 4 Physical Buttons
  for (int i = 0; i < 4; i++) {
    bool reading = digitalRead(BTN_PINS[i]);

    if (reading != lastReading[i]) {
      lastDebounce[i] = now;
    }

    if ((now - lastDebounce[i]) > DEBOUNCE_MS) {
      if (reading != stableState[i]) {
        stableState[i] = reading;

        if (stableState[i] == LOW) {
          // Button just pressed down
          pressStartTime[i] = now;
          longPressFired[i] = false;
        } else {
          // Button released: if Long Press hasn't fired yet, trigger Short Press
          if (!longPressFired[i]) {
            matterButtons[i].click(); // Endpoints 0..3
            triggerFlash(i, SHORT_COLORS[i], 400);
            Serial.printf("Button %d SHORT press -> Alexa Virtual Button %d\r\n", i + 1, i + 1);
          }
        }
      }
    }

    // Check if button is being held down past LONG_PRESS_MS (0.7s)
    if (stableState[i] == LOW && !longPressFired[i] && (now - pressStartTime[i] >= LONG_PRESS_MS)) {
      longPressFired[i] = true;
      matterButtons[i + 4].click(); // Endpoints 4..7
      triggerFlash(i, LONG_COLOR, 550);
      Serial.printf("Button %d LONG press  -> Alexa Virtual Button %d\r\n", i + 1, i + 5);
    }

    lastReading[i] = reading;
  }

  // Only run standard LED logic if fully connected, taaki animation overlap na ho
  if (isCommissioned && isConnected) {
    updateButtonLEDs();
  }

  // 3. Hold Button 1 (D1) + Button 4 (D7) together for 5s to Factory Reset Matter
  if (stableState[0] == LOW && stableState[3] == LOW) {
    if (comboResetStart == 0) {
      comboResetStart = now;
    } else if (now - comboResetStart >= RESET_HOLD_MS) {
      Serial.println("Factory resetting Matter pairing...");
      for (int k = 0; k < NUM_LEDS; k++) strip.setPixelColor(k, strip.Color(255, 0, 0));
      strip.show();
      delay(800); // Yahan delay theek hai kyunki iske baad device restart hona hi hai
      Matter.decommission();
      ESP.restart();
    }
  } else {
    comboResetStart = 0;
  }
}