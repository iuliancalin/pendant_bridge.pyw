#include <Arduino.h>
#include "EspUsbHost.h"
#include "esp_log.h"

// ============================================================================
// CONFIGURATION & SPEEDS
// ============================================================================
enum JogSpeedMode { PRECISE, NORMAL, RAPID };
JogSpeedMode currentSpeedMode = NORMAL;

uint16_t getXYFeed() { return (currentSpeedMode == RAPID) ? 5000 : (currentSpeedMode == NORMAL ? 2000 : 1000); }
uint16_t getZFeed()  { return (currentSpeedMode == RAPID) ? 5000 : (currentSpeedMode == NORMAL ? 2000 : 1000); }

struct JogState {
  bool active = false;
  uint8_t activeKeycode = 0;
};

JogState currentJog;
EspUsbHost usbHost;

// ============================================================================
// JOG ENGINE
// ============================================================================
void startDirectionalJog(uint8_t keycode, float dx, float dy, float dz) {
  if (currentJog.active) return;

  uint16_t feed = (dz != 0) ? getZFeed() : getXYFeed();

  String contCmd = "$J=G91";
  if (dx != 0) contCmd += " X" + String(dx * 1000.0, 1);
  if (dy != 0) contCmd += " Y" + String(dy * 1000.0, 1);
  if (dz != 0) contCmd += " Z" + String(dz * 200.0, 1);
  contCmd += " F" + String(feed);

  Serial.println(contCmd);

  currentJog.active = true;
  currentJog.activeKeycode = keycode;
}

void stopJog(uint8_t keycode) {
  if (currentJog.active && currentJog.activeKeycode == keycode) {
    Serial.println("JOG_CANCEL");
    currentJog.active = false;
    currentJog.activeKeycode = 0;
  }
}

// ============================================================================
// KEYBOARD MAPPER
// ============================================================================
void handleKeypress(uint8_t keycode, uint8_t modifier, bool isPressed) {
  if (isPressed) {
    switch (keycode) {
      // --- JOG SPEED SELECTORS ---
      case 0x0C: currentSpeedMode = RAPID;   Serial.println("PRESET: RAPID");   break; // I
      case 0x16: currentSpeedMode = NORMAL;  Serial.println("PRESET: NORMAL");  break; // S
      case 0x0B: currentSpeedMode = PRECISE; Serial.println("PRESET: PRECISE"); break; // H

      // --- DIRECTIONAL JOGS ---
      case 0x07: startDirectionalJog(keycode, -1.0,  1.0,  0.0); break; // D -> X- Y+
      case 0x09: startDirectionalJog(keycode,  0.0,  1.0,  0.0); break; // F -> Y+
      case 0x0A: startDirectionalJog(keycode,  1.0,  1.0,  0.0); break; // G -> X+ Y+
      case 0x0D: startDirectionalJog(keycode, -1.0,  0.0,  0.0); break; // J -> X-
      case 0x0F: startDirectionalJog(keycode,  1.0,  0.0,  0.0); break; // L -> X+
      case 0x1B: startDirectionalJog(keycode, -1.0, -1.0,  0.0); break; // X -> X- Y-
      case 0x06: startDirectionalJog(keycode,  0.0, -1.0,  0.0); break; // C -> Y-
      case 0x19: startDirectionalJog(keycode,  1.0, -1.0,  0.0); break; // V -> X+ Y-
      case 0x1D: startDirectionalJog(keycode,  0.0,  0.0,  1.0); break; // Z -> Z+
      case 0x05: startDirectionalJog(keycode,  0.0,  0.0, -1.0); break; // B -> Z-

      // --- MACHINE CONTROL ---
      case 0x14: Serial.println("$X"); break;                        // Q
      case 0x1A: Serial.println("$H"); break;                        // W
      case 0x08: Serial.println("RESET"); break;                     // E
      case 0x15: Serial.println("G53 G0 Z0\nG53 G0 X0 Y0"); break;  // R
      case 0x17: Serial.println("~"); break;                         // T
      case 0x1C: Serial.println("!"); break;                         // Y
      case 0x18: Serial.println("JOG_CANCEL"); break;                // U
      case 0x0E: Serial.println("G90 G0 X0 Y0"); break;              // K
    }
  } else {
    stopJog(keycode);
  }
}

// ============================================================================
// MAIN SETUP & LOOP
// ============================================================================
void setup() {
  esp_log_level_set("*", ESP_LOG_NONE); // Suppress low-level system logs
  
  Serial.begin(115200);
  delay(1500);

  Serial.println(">>> PENDANT BRIDGE READY <<<");

  usbHost.onKeyboard([](const EspUsbHostKeyboardEvent &event) {
    handleKeypress(event.keycode, event.modifiers, event.pressed);
  });

  usbHost.begin();
}

void loop() {
  delay(10);
}
