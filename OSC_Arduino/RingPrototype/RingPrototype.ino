// ============================================================================
// THE PIT: Addressable LED Ring Button Controller (Prototype)
// Hardware: Arduino Nano 33 IoT (or any 5V/3.3V Arduino)
// LED Chipset: APA102 / SK9822 (2-wire SPI: Clock + Data)
// ============================================================================
//
// KEYSTONE WIRING:
//   Pin 1 (VCC):    5V or 12V (Ensure LED ring matches supply voltage!)
//   Pin 2 (CLOCK):  Arduino Pin D4
//   Pin 3 (DATA):   Arduino Pin D3
//   Pin 4 (KEY):    Arduino Pin D2 (Button switch to GND)
//   Pin 5 (GND):    Shared Ground with Arduino & Power Supply
//
// STATES IMPLEMENTED:
//   1. ATTRACT:         All 12 LEDs breathe in unison with a smooth sine wave.
//   2. PLAYING (IDLE):    Orbiting comet tail (bright head, fading blur) in station color.
//   3. PLAYING (PRESSED): Instant white contact flash (70ms) + hyper-speed vortex spin while held.
//   4. CRESCENDO (BONUS): Rapid synchronized strobe for installation climax.
// ============================================================================

#include <Arduino.h>

// --- Pin Definitions ---
const int PIN_KEY   = 2;  // Button switch connected to GND (uses INPUT_PULLUP)
const int PIN_DATA  = 2;  // Addressable LED Data line
const int PIN_CLOCK = 3;  // Addressable LED Clock line

// --- LED Configuration ---
const int NUM_LEDS = 12;  // 12-LED ring
const uint8_t GLOBAL_BRIGHTNESS = 20; // 0 to 31 (APA102 hardware 5-bit scale)

// RGB Color structure
struct ColorRGB {
  uint8_t r, g, b;
};

// Preset Player Colors
const ColorRGB COLOR_CYAN    = {   0, 220, 255 }; // Player 1 (Cyan)
const ColorRGB COLOR_AMBER   = { 255, 140,   0 }; // Player 2 (Amber)
const ColorRGB COLOR_MAGENTA = { 255,  20, 180 }; // Player 3 (Magenta)
const ColorRGB COLOR_GREEN   = {   0, 255,  60 }; // Player 4 (Green)

ColorRGB playerColors[] = { COLOR_CYAN, COLOR_AMBER, COLOR_MAGENTA, COLOR_GREEN };
const char* playerNames[] = { 
  "Player 1 (Cyan)", "Player 2 (Amber)", "Player 3 (Magenta)", "Player 4 (Green)" 
};
int currentColorIdx = 0;

// LED Frame Buffer
ColorRGB ringLeds[NUM_LEDS];

// --- State Machine ---
enum ButtonState {
  STATE_ATTRACT,
  STATE_PLAYING,
  STATE_CRESCENDO
};
ButtonState currentState = STATE_ATTRACT;

// Button Debounce & Hold Tracking
bool isPressed = false;
int lastButtonReading = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_MS = 30;
unsigned long pressStartTime = 0;

// Serial simulation flag
bool simulateHold = false;

// ----------------------------------------------------------------------------
// Low-Level Fast Bit-Bang Driver for APA102 / SK9822 (Zero Dependencies)
// ----------------------------------------------------------------------------
static inline void sendByte(uint8_t b) {
  for (uint8_t mask = 0x80; mask; mask >>= 1) {
    digitalWrite(PIN_DATA, (b & mask) ? HIGH : LOW);
    digitalWrite(PIN_CLOCK, HIGH);
    digitalWrite(PIN_CLOCK, LOW);
  }
}

void showRing(uint8_t brightness = GLOBAL_BRIGHTNESS) {
  // 1. Start Frame: 32 zero bits
  for (int i = 0; i < 4; i++) sendByte(0x00);

  // 2. LED Frames: [111BBBBB] [Blue] [Green] [Red]
  uint8_t brightByte = 0xE0 | (brightness & 0x1F);
  for (int i = 0; i < NUM_LEDS; i++) {
    sendByte(brightByte);
    sendByte(ringLeds[i].b);
    sendByte(ringLeds[i].g);
    sendByte(ringLeds[i].r);
  }

  // 3. End Frame: 32 bits (latches data through all shift registers)
  for (int i = 0; i < 4; i++) sendByte(0xFF);
}

void clearRing() {
  for (int i = 0; i < NUM_LEDS; i++) {
    ringLeds[i] = { 0, 0, 0 };
  }
}

// ----------------------------------------------------------------------------
// Procedural LED Ring Renderers
// ----------------------------------------------------------------------------

// 1. ATTRACT STATE: Smooth rhythmic sine-wave breathing
void renderAttract(unsigned long now, ColorRGB color) {
  float breath = (sin(now * 0.0025f) + 1.0f) * 0.5f; // 0.0 to 1.0
  float intensity = 0.08f + 0.92f * breath;          // Minimum baseline glow 8%

  for (int i = 0; i < NUM_LEDS; i++) {
    ringLeds[i].r = (uint8_t)(color.r * intensity);
    ringLeds[i].g = (uint8_t)(color.g * intensity);
    ringLeds[i].b = (uint8_t)(color.b * intensity);
  }
}

// 2. PLAYING (IDLE / READY): Orbiting comet tail with soft falloff
void renderPlayingIdle(unsigned long now, ColorRGB color) {
  float head = fmod(now * 0.0109f, (float)NUM_LEDS);

  for (int i = 0; i < NUM_LEDS; i++) {
    float dist = fmod((head - (float)i + (float)NUM_LEDS), (float)NUM_LEDS);
    
    float brightness = 0.04f; 
    if (dist < 4.0f) {
      float falloff = 1.0f - (dist / 4.0f);
      brightness = max(brightness, falloff * falloff);
    }

    ringLeds[i].r = (uint8_t)(color.r * brightness);
    ringLeds[i].g = (uint8_t)(color.g * brightness);
    ringLeds[i].b = (uint8_t)(color.b * brightness);
  }
}

// 3. PLAYING (PRESSED / ACTIVE HOLD): 0ms White Flash + Hyper-Speed Vortex
void renderPlayingPressed(unsigned long now, ColorRGB color) {
  unsigned long holdDuration = now - pressStartTime;

  if (holdDuration < 70) {
    for (int i = 0; i < NUM_LEDS; i++) {
      ringLeds[i] = { 255, 255, 255 };
    }
    return;
  }

  float fastHead = fmod((float)holdDuration * 0.054f, (float)NUM_LEDS);

  for (int i = 0; i < NUM_LEDS; i++) {
    float dist = fmod((fastHead - (float)i + (float)NUM_LEDS), (float)NUM_LEDS);

    if (dist < 0.9f) {
      ringLeds[i] = { 255, 255, 255 };
    } else if (dist < 5.0f) {
      float b = 1.0f - (dist / 5.0f);
      ringLeds[i].r = (uint8_t)min(255.0f, color.r * 1.3f);
      ringLeds[i].g = (uint8_t)min(255.0f, color.g * 1.3f);
      ringLeds[i].b = (uint8_t)min(255.0f, color.b * 1.3f);
    } else {
      ringLeds[i].r = (uint8_t)(color.r * 0.20f);
      ringLeds[i].g = (uint8_t)(color.g * 0.20f);
      ringLeds[i].b = (uint8_t)(color.b * 0.20f);
    }
  }
}

// 4. CRESCENDO (BONUS): Rapid pit-wide strobe
void renderCrescendo(unsigned long now, ColorRGB color) {
  bool strobe = (now % 100) < 50; 
  for (int i = 0; i < NUM_LEDS; i++) {
    if (strobe) {
      ringLeds[i] = { 255, 255, 255 }; 
    } else {
      ringLeds[i].r = (uint8_t)(color.r * 0.15f);
      ringLeds[i].g = (uint8_t)(color.g * 0.15f);
      ringLeds[i].b = (uint8_t)(color.b * 0.15f);
    }
  }
}

// ----------------------------------------------------------------------------
// Setup & Main Loop
// ----------------------------------------------------------------------------
void setup() {
  pinMode(PIN_KEY, INPUT_PULLUP);
  pinMode(PIN_DATA, OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);

  digitalWrite(PIN_DATA, LOW);
  digitalWrite(PIN_CLOCK, LOW);

  clearRing();
  showRing();

  Serial.begin(115200);
  delay(1000);

  Serial.println("=================================================");
  Serial.println("   THE PIT: ADDRESSABLE LED RING PROTOTYPE");
  Serial.println("=================================================");
  Serial.println("Controls:");
  Serial.println("   [1] Attract Mode (Smooth Breathing)");
  Serial.println("   [2] Playing Mode (Orbiting Comet Tail)");
  Serial.println("   [3] Toggle Simulated Button Hold");
  Serial.println("   [4] Crescendo Mode (Rapid Strobe)");
  Serial.println("   [n] Next Station Color");
  Serial.println("Or physically press the button on Pin D2!");
  Serial.println("-------------------------------------------------");
  Serial.print("Initial Color: ");
  Serial.println(playerNames[currentColorIdx]);
  Serial.println("Initial State: ATTRACT");
}

void loop() {
  unsigned long now = millis();

  // 1. Serial Command Parser
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '1' || c == 'a' || c == 'A') {
      currentState = STATE_ATTRACT;
      simulateHold = false;
      Serial.println(">>> State switched: ATTRACT");
    } else if (c == '2' || c == 'p' || c == 'P') {
      currentState = STATE_PLAYING;
      simulateHold = false;
      Serial.println(">>> State switched: PLAYING (Idle)");
    } else if (c == '3' || c == 'h' || c == 'H') {
      simulateHold = !simulateHold;
      if (simulateHold) {
        pressStartTime = now;
        currentState = STATE_PLAYING;
        Serial.println(">>> Simulated Button: PRESSED / HELD (Vortex On)");
      } else {
        Serial.println(">>> Simulated Button: RELEASED");
      }
    } else if (c == '4' || c == 'c' || c == 'C') {
      currentState = STATE_CRESCENDO;
      simulateHold = false;
      Serial.println(">>> State switched: CRESCENDO");
    } else if (c == 'n' || c == 'N') {
      currentColorIdx = (currentColorIdx + 1) % 4;
      Serial.print(">>> Station Color changed to: ");
      Serial.println(playerNames[currentColorIdx]);
    }
  }

  // 2. Physical Button Scanning & Debouncing (INPUT_PULLUP: LOW = Pressed)
  int reading = digitalRead(PIN_KEY);
  if (reading != lastButtonReading) {
    if ((now - lastDebounceTime) > DEBOUNCE_MS) {
      lastDebounceTime = now;
      lastButtonReading = reading;

      if (reading == LOW) {
        isPressed = true;
        pressStartTime = now;
        currentState = STATE_PLAYING; 
        Serial.println("[Button Event] PRESSED! (White Flash -> Vortex)");
      } else {
        isPressed = false;
        Serial.println("[Button Event] RELEASED! (Returning to Orbit)");
      }
    }
  }

  bool currentlyHeld = isPressed || simulateHold;

  // 3. Render Appropriate Animation at 40 Hz (every 25ms)
  static unsigned long lastRender = 0;
  if (now - lastRender >= 25) {
    lastRender = now;

    ColorRGB activeColor = playerColors[currentColorIdx];

    if (currentlyHeld && currentState == STATE_PLAYING) {
      renderPlayingPressed(now, activeColor);
    } else if (currentState == STATE_ATTRACT) {
      renderAttract(now, activeColor);
    } else if (currentState == STATE_PLAYING) {
      renderPlayingIdle(now, activeColor);
    } else if (currentState == STATE_CRESCENDO) {
      renderCrescendo(now, activeColor);
    }

    showRing();
  }
}