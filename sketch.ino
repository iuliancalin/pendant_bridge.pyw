#include <Arduino.h>
#include "EspUsbHost.h"
#include "esp_log.h"

// ============================================================================
// CONFIGURATION (LOCKED USER VALUES & MACHINE TRAVEL)
// ============================================================================
const float MAX_TRAVEL_X = 499.0;
const float MAX_TRAVEL_Y = 499.0;
const float MAX_TRAVEL_Z = 137.0;

enum JogSpeedMode { PRECISE, NORMAL, RAPID };
JogSpeedMode currentSpeedMode = NORMAL;

// Feedrates (mm/min)
uint16_t getFeedrate() { 
  switch (currentSpeedMode) {
    case RAPID:   return 5000;
    case NORMAL:  return 2000;
    case PRECISE: return 1000;
    default:      return 2000;
  }
}

// Single-tap distances (mm)
float getXYStep() { 
  switch (currentSpeedMode) {
    case RAPID:   return 20.0;
    case NORMAL:  return 5.0;
    case PRECISE: return 0.5;
    default:      return 5.0;
  }
}

float getZStep() { 
  switch (currentSpeedMode) {
    case RAPID:   return 20.0;
    case NORMAL:  return 5.0;
    case PRECISE: return 0.1;
    default:      return 5.0;
  }
}

struct JogState {
  bool active = false;
  uint8_t activeKeycode = 0;
  float dx = 0;
  float dy = 0;
  float dz = 0;
  unsigned long pressStartTime = 0;
  bool tapPulseSent = false;
  bool continuousStarted = false;
};

JogState currentJog;
EspUsbHost usbHost;

// ============================================================================
// gSender NATIVE JOG ENGINE
// ============================================================================
void startDirectionalJog(uint8_t keycode, float dx, float dy, float dz) {
  if (currentJog.active && currentJog.activeKeycode == keycode) return;

  currentJog.active = true;
  currentJog.activeKeycode = keycode;
  currentJog.dx = dx;
  currentJog.dy = dy;
  currentJog.dz = dz;
  currentJog.pressStartTime = millis();
  currentJog.tapPulseSent = false;
  currentJog.continuousStarted = false;
}

void stopJog(uint8_t keycode) {
  if (currentJog.active && currentJog.activeKeycode == keycode) {
    // Immediate stop on release if in continuous mode
    if (currentJog.continuousStarted) {
      Serial.println("JOG_CANCEL");
    }
    currentJog.active = false;
    currentJog.activeKeycode = 0;
  }
}

void processJogLogic() {
  if (!currentJog.active) return;

  unsigned long holdDuration = millis() - currentJog.pressStartTime;

  // 1. Fire single tap move immediately
  if (!currentJog.tapPulseSent) {
    uint16_t feed = getFeedrate();
    float step = (currentJog.dz != 0) ? getZStep() : getXYStep();

    String cmd = "$J=G91";
    if (currentJog.dx != 0) cmd += " X" + String(currentJog.dx * step, 2);
    if (currentJog.dy != 0) cmd += " Y" + String(currentJog.dy * step, 2);
    if (currentJog.dz != 0) cmd += " Z" + String(currentJog.dz * step, 2);
    cmd += " F" + String(feed);

    Serial.println(cmd);
    currentJog.tapPulseSent = true;
    return;
  }

  // 2. If held >180ms, switch to full-travel continuous move (like gSender screen buttons)
  if (holdDuration >= 180 && !currentJog.continuousStarted) {
    uint16_t feed = getFeedrate();

    String cmd = "$J=G91";
    if (currentJog.dx != 0) cmd += " X" + String(currentJog.dx * MAX_TRAVEL_X, 1);
    if (currentJog.dy != 0) cmd += " Y" + String(currentJog.dy * MAX_TRAVEL_Y, 1);
    if (currentJog.dz != 0) cmd += " Z" + String(currentJog.dz * MAX_TRAVEL_Z, 1);
    cmd += " F" + String(feed);

    Serial.println(cmd);
    currentJog.continuousStarted = true;
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
  esp_log_level_set("*", ESP_LOG_NONE);
  
  Serial.begin(115200);
  delay(1500);

  usbHost.onKeyboard([](const EspUsbHostKeyboardEvent &event) {
    handleKeypress(event.keycode, event.modifiers, event.pressed);
  });

  usbHost.begin();
}

void loop() {
  processJogLogic();
  delay(2);
}
