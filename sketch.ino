#include <Arduino.h>
#include "EspUsbHost.h"

// ============================================================================
// CONFIGURATION & SPEEDS
// ============================================================================
const unsigned long HOLD_THRESHOLD_MS = 200; 

enum JogSpeedMode { PRECISE, NORMAL, RAPID };
JogSpeedMode currentSpeedMode = NORMAL; // Default speed on boot

// Dynamic Feedrates (mm/min) and Step Distances (mm)
uint16_t getXYFeed() { return (currentSpeedMode == RAPID) ? 5000 : (currentSpeedMode == NORMAL ? 3000 : 1000); }
uint16_t getZFeed()  { return (currentSpeedMode == RAPID) ? 5000 : (currentSpeedMode == NORMAL ? 3000 : 1000); }
float getXYStep()    { return (currentSpeedMode == RAPID) ? 20.0 : (currentSpeedMode == NORMAL ? 5.0  : 0.5); }
float getZStep()     { return (currentSpeedMode == RAPID) ? 10.0 : (currentSpeedMode == NORMAL ? 2.0  : 0.1); }

struct JogState {
  bool active = false;
  bool continuousFired = false;
  uint8_t activeKeycode = 0;
  unsigned long pressTime = 0;
  String continuousCommand;
};

JogState currentJog;
EspUsbHost usbHost;

// ============================================================================
// JOG ENGINE
// ============================================================================
void startDirectionalJog(uint8_t keycode, float dx, float dy, float dz) {
  if (currentJog.active) return;

  float step = (dz != 0) ? getZStep() : getXYStep();
  uint16_t feed = (dz != 0) ? getZFeed() : getXYFeed();

  // 1. Immediate step tap command
  String shortCmd = "$J=G91";
  if (dx != 0) shortCmd += " X" + String(dx * step, 1);
  if (dy != 0) shortCmd += " Y" + String(dy * step, 1);
  if (dz != 0) shortCmd += " Z" + String(dz * step, 1);
  shortCmd += " F" + String(feed);

  Serial.println(shortCmd); // Fire tap move instantly

  // 2. Continuous hold command payload (canceled on release)
  String contCmd = "$J=G91";
  if (dx != 0) contCmd += " X" + String(dx * 1000.0, 1);
  if (dy != 0) contCmd += " Y" + String(dy * 1000.0, 1);
  if (dz != 0) contCmd += " Z" + String(dz * 200.0, 1);
  contCmd += " F" + String(feed);

  currentJog.active = true;
  currentJog.continuousFired = false;
  currentJog.activeKeycode = keycode;
  currentJog.pressTime = millis();
  currentJog.continuousCommand = contCmd;
}

void stopJog(uint8_t keycode) {
  if (currentJog.active && currentJog.activeKeycode == keycode) {
    if (currentJog.continuousFired) {
      Serial.println("JOG_CANCEL");
    }
    currentJog.active = false;
    currentJog.continuousFired = false;
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
      case 0x0C: 
        currentSpeedMode = RAPID;   
        Serial.println("PRESET: RAPID [XY:20mm Z:10mm F5000]");   
        break; // I -> Rapid Jog
      case 0x16: 
        currentSpeedMode = NORMAL;  
        Serial.println("PRESET: NORMAL [XY:5mm Z:2mm F3000]");   
        break; // S -> Normal Jog
      case 0x0B: 
        currentSpeedMode = PRECISE; 
        Serial.println("PRESET: PRECISE [XY:0.5mm Z:0.1mm F1000]"); 
        break; // H -> Precise Jog

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

      // --- MACHINE CONTROL & OVERRIDES ---
      case 0x14: Serial.println("$X"); break;                        // Q -> Unlock
      case 0x1A: Serial.println("$H"); break;                        // W -> Home Machine
      case 0x08: Serial.println("RESET"); break;                     // E -> Soft Reset
      case 0x15: Serial.println("G53 G0 Z0\nG53 G0 X0 Y0"); break;  // R -> G53 Zero
      case 0x17: Serial.println("~"); break;                         // T -> Start Job
      case 0x1C: Serial.println("!"); break;                         // Y -> Pause / Hold
      case 0x18: Serial.println("JOG_CANCEL"); break;                // U -> Panic Stop
      case 0x0E: Serial.println("G90 G0 X0 Y0"); break;              // K -> Work Zero
    }
  } else {
    stopJog(keycode);
  }
}

// ============================================================================
// MAIN SETUP & LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  usbHost.onKeyboard([](const EspUsbHostKeyboardEvent &event) {
    handleKeypress(event.keycode, event.modifiers, event.pressed);
  });

  usbHost.begin();
}

void loop() {
  if (currentJog.active && !currentJog.continuousFired) {
    if (millis() - currentJog.pressTime >= HOLD_THRESHOLD_MS) {
      currentJog.continuousFired = true;
      Serial.println(currentJog.continuousCommand);
    }
  }

  delay(2);
}
