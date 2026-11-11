// ============================================================================
// THE PIT PROJECT: 4-BUTTON STATION CONTROLLER (ESP32)
// ============================================================================
// Hardware:
//  - ESP-WROOM-32 (Station Unit 1 to 6)
//  - 4x Arcade Microswitch Buttons arranged in a straight line
//  - 4x 12V LEDs mounted inside the microswitches, driven via MOSFET board (PWM)
//
// Features:
//  - Zero-latency local 60 FPS lighting engine with 256-step perceptual Gamma curve
//  - Alternating Attract Modes: Smooth Comet Ping-Pong & Inward-Outward Accordion
//  - Autonomous "Ghost Player" Demo Bursts to attract passersby in the room
//  - Siphon Focus & Hold-to-Charge crystal mechanic with energetic shimmer
//  - Crystal Lock with music beat-pulse & multi-button stem stacking
//  - Reliable OSC snapshot transport (/pit/unitX/inputs) with ACK & battery saver
// ============================================================================

#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <esp_arduino_version.h>

// ============================================================================
// 1. Station Identity & Network Configuration
// ============================================================================
// Change UNIT_ID for each physical station (1 through 6)
const int UNIT_ID = 1; 

const bool DEBUG_OFFLINE_MODE = false; // Set true for bench testing without Wi-Fi

char ssid[] = "HarmonicInstallation";              // Wi-Fi SSID
char pass[] = "Boislief45";                   // Wi-Fi Password

IPAddress outIp(192, 168, 0, 100);            // Target PC (Unreal Engine) IP
const unsigned int outPort = 8000;            // Unreal Engine OSC Server Port
const unsigned int inPort  = 8888;            // Local Port for Incoming Game State & Sync

WiFiUDP Udp;

// ============================================================================
// 2. Hardware Pinout & Timing Configuration
// ============================================================================
// 4 Microswitch inputs (Internal pullup to GND when pressed)
const int BTN_PINS[4] = { 18, 19, 21, 22 };

// 4 MOSFET Gate control pins (PWM driven)
// Note: Pins 25, 26, 27, 32 are all hardware PWM capable outputs (Avoid GPI 34-39 which are input-only!)
const int LED_PINS[4] = { 25, 26, 27, 33 };

// PWM Setup (1000 Hz = optimal for PC817 optocoupled MOSFET modules)
const int PWM_FREQ       = 1000;
const int PWM_RESOLUTION = 8;     // 8-bit resolution (0 - 255)
const int PWM_CHANNELS[4] = { 0, 1, 2, 3 };

// --- Gameplay & Interaction Timings ---
const unsigned long DEBOUNCE_MS          = 25;    // Button debounce window
const unsigned long CHARGE_TIME_MS       = 2000;  // Hold duration to fully charge crystal (2.0s)
const unsigned long FLASH_DURATION_MS    = 120;   // Bright lock flash duration on full charge
const unsigned long DECAY_TIME_MS        = 350;   // Early release fizzle/decay duration
const unsigned long IDLE_TO_ATTRACT_MS   = 4000;  // Idle wait after release before returning to Attract

// --- Active Crystal & Deactivation Limits (Tunable Variables) ---
int maxActivePerStation                  = 2;     // Maximum active stems on this 4-button station (set 1 to 4)
const unsigned long TAP_CUT_MAX_MS       = 400;   // Max press duration to trigger Tap-to-Cut on an active button

// --- Attract Mode Cycle Timings ---
const unsigned long ATTRACT_CYCLE_MS     = 16000; // Switch between Comet and In-Out every 16 seconds
const unsigned long DEMO_TRIGGER_IDLE_MS = 32000; // Trigger "Ghost Player" demo burst every 32s of pure idle

// ============================================================================
// 3. Perceptual Gamma Correction Table (gamma = 2.4)
// ============================================================================
// Maps linear 0-255 brightness values to logarithmic human eye perception
const uint8_t PROGMEM GAMMA_LUT[256] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   3,
    3,   3,   3,   3,   4,   4,   4,   4,   5,   5,   5,   5,   6,   6,   6,   7,
    7,   7,   8,   8,   8,   9,   9,   9,  10,  10,  11,  11,  11,  12,  12,  13,
   13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,  20,  21,  21,
   22,  22,  23,  24,  24,  25,  26,  26,  27,  28,  28,  29,  30,  31,  31,  32,
   33,  34,  34,  35,  36,  37,  38,  38,  39,  40,  41,  42,  43,  44,  45,  46,
   47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,
   64,  65,  66,  67,  68,  70,  71,  72,  73,  75,  76,  77,  79,  80,  81,  83,
   84,  85,  87,  88,  90,  91,  93,  94,  96,  97,  99, 100, 102, 103, 105, 107,
  108, 110, 112, 113, 115, 117, 119, 120, 122, 124, 126, 128, 130, 131, 133, 135,
  137, 139, 141, 143, 145, 147, 149, 152, 154, 156, 158, 160, 163, 165, 167, 169,
  172, 174, 176, 179, 181, 184, 186, 189, 191, 194, 196, 199, 201, 204, 207, 209,
  212, 215, 217, 220, 223, 226, 228, 231, 234, 237, 240, 243, 246, 249, 252, 255
};

// ============================================================================
// 4. Data Structures & State Management
// ============================================================================
enum ButtonLightMode {
  LIGHT_ATTRACT,       // Ambient wave/comet
  LIGHT_CHARGING,      // Button held, ramp up with energy rumble
  LIGHT_LOCKED_ACTIVE, // Reached 100% charge, active stem with beat pulse
  LIGHT_DECAY          // Early release before 100%, fast fizzle
};

struct ButtonStation {
  int btnPin;
  int ledPin;
  int pwmChannel;

  // Debouncing
  int lastRawState;
  unsigned long lastDebounceTime;
  bool isPressed;

  // Interaction State
  ButtonLightMode lightMode;
  unsigned long pressStartTime;
  unsigned long releaseTime;
  unsigned long lockTime;
  float currentBrightness; // 0.0 to 1.0 (linear)
  float targetBrightness;
};

ButtonStation BUTTONS[4];

// Global Installation State
enum GlobalState {
  STATE_ATTRACT_MODE,
  STATE_INTERACTIVE_MODE
};
GlobalState globalState = STATE_ATTRACT_MODE;

unsigned long lastGlobalActivityTime = 0;
unsigned long attractPhaseStartTime  = 0;
unsigned long lastBeatTickTime        = 0;
int demoActiveButton                  = -1;
unsigned long demoStartTime           = 0;

// ============================================================================
// 5. Reliable OSC Transport (State Snapshot & ACK Protocol)
// ============================================================================
char unitOscPath[32];
uint32_t currentSequence    = 1;
uint32_t lastAckedSequence  = 0;
bool needsAck               = true;
unsigned long lastSendTime  = 0;
int retryCount              = 0;

const unsigned long RETRY_INTERVAL_MS = 40;
const int MAX_RETRIES                 = 50; // 2.0 seconds maximum attempt window

void sendStatePacket() {
  uint8_t packet[96];
  int len = 0;

  // 1. OSC Address (e.g. "/pit/unit1/inputs")
  int addrLen = strlen(unitOscPath);
  memcpy(&packet[len], unitOscPath, addrLen);
  len += addrLen;
  packet[len++] = '\0';
  while (len % 4 != 0) packet[len++] = '\0';

  // 2. OSC Type Tags: ",i" + 4x "f"
  packet[len++] = ',';
  packet[len++] = 'i';
  for (int i = 0; i < 4; i++) packet[len++] = 'f';
  packet[len++] = '\0';
  while (len % 4 != 0) packet[len++] = '\0';

  // 3. Sequence ID (int32 big-endian)
  packet[len++] = (currentSequence >> 24) & 0xFF;
  packet[len++] = (currentSequence >> 16) & 0xFF;
  packet[len++] = (currentSequence >> 8) & 0xFF;
  packet[len++] = currentSequence & 0xFF;

  // 4. Button states (4x float big-endian)
  for (int i = 0; i < 4; i++) {
    union { float f; uint8_t b[4]; } u;
    u.f = BUTTONS[i].isPressed ? 1.0f : 0.0f;
    packet[len++] = u.b[3];
    packet[len++] = u.b[2];
    packet[len++] = u.b[1];
    packet[len++] = u.b[0];
  }

  if (!DEBUG_OFFLINE_MODE) {
    Udp.beginPacket(outIp, outPort);
    Udp.write(packet, len);
    Udp.endPacket();
  }

  Serial.print(DEBUG_OFFLINE_MODE ? ">>> [OFFLINE OSC] " : ">>> [OSC Sent] ");
  Serial.print(unitOscPath);
  Serial.print(" seq=");
  Serial.print(currentSequence);
  Serial.print(" states=[");
  for (int i = 0; i < 4; i++) {
    Serial.print(BUTTONS[i].isPressed ? "1" : "0");
    if (i < 3) Serial.print(", ");
  }
  Serial.print("] (retry=");
  Serial.print(retryCount);
  Serial.println(")");
}

void triggerStateChange() {
  currentSequence++;
  needsAck = true;
  retryCount = 0;
  lastSendTime = millis();
  sendStatePacket();
}

// ============================================================================
// 6. Low-Level PWM Hardware Driver
// ============================================================================
void writePwmDuty(int idx, uint8_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcWrite(BUTTONS[idx].ledPin, duty);
#else
  analogWrite(BUTTONS[idx].ledPin, duty);
#endif
}

void applyLedLinear(int idx, float linearBrightness) {
  // If zero or near-zero, cut completely
  if (linearBrightness <= 0.005f) {
    writePwmDuty(idx, 0);
    return;
  }

  // Clamp 0.0 to 1.0
  if (linearBrightness > 1.0f) linearBrightness = 1.0f;
  
  uint8_t index = (uint8_t)(linearBrightness * 255.0f);
  uint8_t rawDuty = pgm_read_byte(&GAMMA_LUT[index]);

  // Optocoupler threshold offset: start at duty 12 so PC817 conducts immediately
  uint8_t optoDuty = 12 + (uint8_t)(((float)rawDuty / 255.0f) * (255 - 12));
  writePwmDuty(idx, optoDuty);
}

// ============================================================================
// 7. High-Level Animation & Lighting Engine
// ============================================================================
void updateLighting(unsigned long now) {
  // Check if any button is currently being pressed or actively held
  bool anyPressed = false;
  bool anyLocked  = false;
  for (int i = 0; i < 4; i++) {
    if (BUTTONS[i].isPressed) anyPressed = true;
    if (BUTTONS[i].lightMode == LIGHT_LOCKED_ACTIVE) anyLocked = true;
  }

  if (anyPressed || anyLocked) {
    globalState = STATE_INTERACTIVE_MODE;
    demoActiveButton = -1; // Cancel any autonomous demo
  } else if (globalState == STATE_INTERACTIVE_MODE) {
    if (now - lastGlobalActivityTime >= IDLE_TO_ATTRACT_MS) {
      globalState = STATE_ATTRACT_MODE;
      attractPhaseStartTime = now;
      for (int i = 0; i < 4; i++) BUTTONS[i].lightMode = LIGHT_ATTRACT;
      Serial.println("[State] Entering ATTRACT MODE");
    }
  }

  // --- ATTRACT MODE RENDERING ---
  if (globalState == STATE_ATTRACT_MODE) {
    unsigned long idleDuration = now - lastGlobalActivityTime;

    // Check Autonomous "Ghost Player" Demo Trigger
    if (idleDuration >= DEMO_TRIGGER_IDLE_MS && demoActiveButton < 0) {
      demoActiveButton = random(0, 4);
      demoStartTime = now;
      Serial.print("[Demo] Triggering Ghost Player Demo on Button ");
      Serial.println(demoActiveButton + 1);
    }

    if (demoActiveButton >= 0) {
      // Run Ghost Demo Sequence: 1.8s charge ramp -> 200ms flash -> 800ms fadeout
      unsigned long demoElapsed = now - demoStartTime;
      if (demoElapsed < 1800) {
        float ramp = (float)demoElapsed / 1800.0f;
        for (int i = 0; i < 4; i++) {
          applyLedLinear(i, (i == demoActiveButton) ? (ramp * ramp) : (1.0f - ramp) * 0.05f);
        }
      } else if (demoElapsed < 2000) {
        // Flash!
        for (int i = 0; i < 4; i++) {
          applyLedLinear(i, (i == demoActiveButton) ? 1.0f : 0.0f);
        }
      } else if (demoElapsed < 2800) {
        float fade = 1.0f - ((float)(demoElapsed - 2000) / 800.0f);
        for (int i = 0; i < 4; i++) {
          applyLedLinear(i, (i == demoActiveButton) ? (fade * 0.7f) : 0.0f);
        }
      } else {
        // Demo finished, return to ambient attract
        demoActiveButton = -1;
        lastGlobalActivityTime = now; // Reset idle timer
      }
      return;
    }

    // Alternating Ambient Wave: Comet Ping-Pong vs Inward-Outward Pulse
    unsigned long attractElapsed = now - attractPhaseStartTime;
    bool isCometPhase = ((attractElapsed / ATTRACT_CYCLE_MS) % 2) == 0;

    if (isCometPhase) {
      // --- PATTERN 1: COMET PING-PONG (0 -> 1 -> 2 -> 3 -> 2 -> 1) ---
      float cycle = fmod((float)now / 350.0f, 6.0f);
      float cometPos = (cycle <= 3.0f) ? cycle : (6.0f - cycle); // 0.0 to 3.0 ping pong

      for (int i = 0; i < 4; i++) {
        float dist = fabs((float)i - cometPos);
        // Gaussian falloff
        float brightness = expf(-dist * dist * 1.8f);
        applyLedLinear(i, brightness * 0.85f);
      }
    } else {
      // --- PATTERN 2: INWARD-OUTWARD ACCORDION PULSE ---
      // Outer buttons (0 & 3) swell, then inner buttons (1 & 2) swell
      float wave = (sinf((float)now * 0.0035f) + 1.0f) * 0.5f; // 0.0 to 1.0
      float outer = wave;
      float inner = 1.0f - wave;

      applyLedLinear(0, outer * 0.8f);
      applyLedLinear(1, inner * 0.8f);
      applyLedLinear(2, inner * 0.8f);
      applyLedLinear(3, outer * 0.8f);
    }
    return;
  }

  // --- INTERACTIVE / GAMEPLAY RENDERING ---
  for (int i = 0; i < 4; i++) {
    switch (BUTTONS[i].lightMode) {
      case LIGHT_CHARGING: {
        unsigned long holdDuration = now - BUTTONS[i].pressStartTime;
        float progress = (float)holdDuration / (float)CHARGE_TIME_MS;
        if (progress > 1.0f) progress = 1.0f;

        // Subtle energetic shimmer/vibration that intensifies as charge nears 100%
        float shimmer = sinf((float)now * 0.045f) * (0.08f * progress);
        float currentB = (progress * progress) + shimmer;
        if (currentB < 0.0f) currentB = 0.0f;
        if (currentB > 1.0f) currentB = 1.0f;
        applyLedLinear(i, currentB);

        // Check if full charge reached (Step 2: Lock)
        if (holdDuration >= CHARGE_TIME_MS) {
          // If already at maxActivePerStation, auto-swap the oldest active crystal
          int activeCount = 0;
          int oldestIdx = -1;
          unsigned long oldestLockTime = 0xFFFFFFFF;

          for (int j = 0; j < 4; j++) {
            if (BUTTONS[j].lightMode == LIGHT_LOCKED_ACTIVE) {
              activeCount++;
              if (BUTTONS[j].lockTime < oldestLockTime) {
                oldestLockTime = BUTTONS[j].lockTime;
                oldestIdx = j;
              }
            }
          }

          if (activeCount >= maxActivePerStation && oldestIdx >= 0) {
            BUTTONS[oldestIdx].lightMode = LIGHT_DECAY;
            Serial.print("[Capacity] Max active (");
            Serial.print(maxActivePerStation);
            Serial.print(") reached -> Swapping oldest Button ");
            Serial.println(oldestIdx + 1);
          }

          BUTTONS[i].lightMode = LIGHT_LOCKED_ACTIVE;
          BUTTONS[i].lockTime = now;
          Serial.print("[Charge Complete] Button ");
          Serial.print(i + 1);
          Serial.println(" -> CRYSTAL LOCKED ACTIVE!");
        }
        break;
      }

      case LIGHT_LOCKED_ACTIVE: {
        // Step 3: Crystal Locked. Initial flash, then steady glow with beat pulse
        unsigned long timeSinceLock = now - BUTTONS[i].lockTime;
        if (timeSinceLock < FLASH_DURATION_MS) {
          applyLedLinear(i, 1.0f); // Peak flash!
        } else {
          // Ambient active glow (70%) + musical beat pulse
          float beatDecay = 0.0f;
          if (lastBeatTickTime > 0) {
            unsigned long beatElapsed = now - lastBeatTickTime;
            if (beatElapsed < 250) {
              beatDecay = (1.0f - ((float)beatElapsed / 250.0f)) * 0.30f;
            }
          } else {
            // Local fallback 120 BPM beat pulse if no OSC clock received
            float localBeat = fmod((float)now, 500.0f) / 500.0f;
            if (localBeat < 0.35f) {
              beatDecay = (1.0f - (localBeat / 0.35f)) * 0.20f;
            }
          }
          applyLedLinear(i, 0.70f + beatDecay);
        }
        break;
      }

      case LIGHT_DECAY: {
        // Early release fizzle out over DECAY_TIME_MS
        unsigned long decayElapsed = now - BUTTONS[i].releaseTime;
        if (decayElapsed < DECAY_TIME_MS) {
          float decay = 1.0f - ((float)decayElapsed / (float)DECAY_TIME_MS);
          applyLedLinear(i, decay * 0.5f);
        } else {
          BUTTONS[i].lightMode = LIGHT_ATTRACT;
          applyLedLinear(i, 0.0f);
        }
        break;
      }

      case LIGHT_ATTRACT:
      default: {
        // When other buttons are actively locked, unpressed buttons have a subtle "ready" glow
        if (anyLocked) {
          applyLedLinear(i, 0.08f); // 8% dim ready guide
        } else {
          // Siphoned to zero during active charge of sibling
          applyLedLinear(i, 0.0f);
        }
        break;
      }
    }
  }
}

// ============================================================================
// 8. Setup
// ============================================================================
void setup() {
  // Power Optimizations
  setCpuFrequencyMhz(80); // Drop baseline power draw while keeping 80MHz performance
  btStop();               // Disable Bluetooth hardware completely

  delay(500);
  Serial.begin(115200);
  unsigned long startWait = millis();
  while (!Serial && (millis() - startWait < 2500)) delay(10);

  snprintf(unitOscPath, sizeof(unitOscPath), "/pit/unit%d/inputs", UNIT_ID);

  Serial.println("\n==============================================");
  Serial.print(" THE PIT PROJECT: STATION CONTROLLER (UNIT ");
  Serial.print(UNIT_ID);
  Serial.println(")");
  Serial.println(" Hardware: 4 Buttons + 4x 12V MOSFET LED Channels");
  Serial.println(" Lighting: Local 60FPS Gamma Ramped Engine");
  Serial.println("==============================================");

  // Initialize Hardware Pins & PWM
  for (int i = 0; i < 4; i++) {
    BUTTONS[i].btnPin        = BTN_PINS[i];
    BUTTONS[i].ledPin        = LED_PINS[i];
    BUTTONS[i].pwmChannel    = PWM_CHANNELS[i];
    BUTTONS[i].lastRawState  = HIGH;
    BUTTONS[i].isPressed     = false;
    BUTTONS[i].lightMode     = LIGHT_ATTRACT;

    pinMode(BUTTONS[i].btnPin, INPUT_PULLUP);
    pinMode(BUTTONS[i].ledPin, OUTPUT);
    digitalWrite(BUTTONS[i].ledPin, LOW);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcAttach(BUTTONS[i].ledPin, PWM_FREQ, PWM_RESOLUTION);
    ledcWrite(BUTTONS[i].ledPin, 0);
#else
    analogWriteFrequency(BUTTONS[i].ledPin, PWM_FREQ);
    analogWriteResolution(BUTTONS[i].ledPin, PWM_RESOLUTION);
    analogWrite(BUTTONS[i].ledPin, 0);
#endif
  }

  randomSeed(analogRead(34) + micros());
  attractPhaseStartTime = millis();

  if (DEBUG_OFFLINE_MODE) {
    Serial.println("\n*** DEBUG_OFFLINE_MODE ACTIVE ***");
    Serial.println("- Wi-Fi connection skipped for fast bench testing.");
    Serial.println("- Press buttons or type '1', '2', '3', '4' in Serial.");
    Serial.println("- Send 'k' to simulate ACK, 'b' to simulate beat tick.\n");
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Low-latency packet transmission
    WiFi.setTxPower(WIFI_POWER_15dBm);
    WiFi.begin(ssid, pass);

    Serial.print("Connecting to Wi-Fi '");
    Serial.print(ssid);
    Serial.print("'");
    while (WiFi.status() != WL_CONNECTED) {
      delay(300);
      Serial.print(".");
    }
    Serial.println("\n[Wi-Fi Connected!] ESP32 IP: " + WiFi.localIP().toString());

    Udp.begin(inPort);
    sendStatePacket();
  }
}

// ============================================================================
// 9. Main Loop
// ============================================================================
void loop() {
  unsigned long now = millis();

  // --- 1. Wi-Fi Reconnection Watchdog ---
  if (!DEBUG_OFFLINE_MODE && WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    delay(200);
    WiFi.begin(ssid, pass);
    unsigned long reconnStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - reconnStart < 3000) {
      delay(200);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.setSleep(false);
      WiFi.setTxPower(WIFI_POWER_15dBm);
      Udp.begin(inPort);
      Serial.println("\n[Wi-Fi Reconnected!]");
      triggerStateChange();
    }
  }

  // --- 2. Serial Simulation for Bench Testing ---
  if (Serial.available()) {
    char c = Serial.read();
    if (c >= '1' && c <= '4') {
      int idx = c - '1';
      BUTTONS[idx].isPressed = !BUTTONS[idx].isPressed;
      Serial.print("[Serial Toggle] Button ");
      Serial.print(idx + 1);
      Serial.println(BUTTONS[idx].isPressed ? " => PRESSED" : " => RELEASED");

      if (BUTTONS[idx].isPressed) {
        BUTTONS[idx].pressStartTime = now;
        if (BUTTONS[idx].lightMode != LIGHT_LOCKED_ACTIVE) {
          BUTTONS[idx].lightMode = LIGHT_CHARGING;
        }
      } else {
        BUTTONS[idx].releaseTime = now;
        unsigned long pressHold = now - BUTTONS[idx].pressStartTime;
        if (BUTTONS[idx].lightMode == LIGHT_LOCKED_ACTIVE && pressHold <= TAP_CUT_MAX_MS) {
          BUTTONS[idx].lightMode = LIGHT_DECAY;
          Serial.print("[Serial Tap to Cut] Button ");
          Serial.print(idx + 1);
          Serial.println(" -> Crystal Deactivated (Cut)");
        } else if (BUTTONS[idx].lightMode == LIGHT_CHARGING) {
          BUTTONS[idx].lightMode = LIGHT_DECAY;
        }
      }
      lastGlobalActivityTime = now;
      triggerStateChange();
    } else if (c == 'm' || c == 'M') {
      maxActivePerStation = (maxActivePerStation % 4) + 1;
      Serial.print("<<< [Serial Config] maxActivePerStation = ");
      Serial.println(maxActivePerStation);
    } else if (c == 'b' || c == 'B') {
      lastBeatTickTime = now;
      Serial.println("<<< [Serial Beat Tick]");
    } else if (c == 'k' || c == 'K') {
      needsAck = false;
      lastAckedSequence = currentSequence;
      Serial.println("<<< [Serial Simulated ACK]");
    } else if (c == 'a' || c == 'A') {
      globalState = STATE_ATTRACT_MODE;
      for (int i = 0; i < 4; i++) BUTTONS[i].lightMode = LIGHT_ATTRACT;
      Serial.println("<<< [Serial Force ATTRACT MODE]");
    }
  }

  // --- 3. Incoming OSC Poll ---
  if (!DEBUG_OFFLINE_MODE) {
    int packetSize = Udp.parsePacket();
    if (packetSize > 0) {
      OSCMessage inMsg;
      while (packetSize--) inMsg.fill(Udp.read());

      if (!inMsg.hasError()) {
        if (inMsg.match("/pit/ack")) {
          int32_t ackSeq = 0;
          if (inMsg.isInt(0)) ackSeq = inMsg.getInt(0);
          if (ackSeq >= currentSequence) {
            needsAck = false;
            lastAckedSequence = ackSeq;
          }
        } else if (inMsg.match("/pit/sync")) {
          triggerStateChange();
        } else if (inMsg.match("/pit/beat") || inMsg.match("/pit/clock/bar")) {
          lastBeatTickTime = now; // Synchronize beat pulses with Unreal Quartz clock
        } else if (inMsg.match("/pit/attract")) {
          globalState = STATE_ATTRACT_MODE;
          attractPhaseStartTime = now;
          for (int i = 0; i < 4; i++) BUTTONS[i].lightMode = LIGHT_ATTRACT;
        } else if (inMsg.match("/pit/led/all")) {
          float v = 0.0f;
          if (inMsg.isFloat(0)) v = inMsg.getFloat(0);
          if (v <= 0.01f) {
            for (int i = 0; i < 4; i++) BUTTONS[i].lightMode = LIGHT_ATTRACT;
          }
        } else {
          // Check for channel-specific crystal expiration (e.g. /pit/led/player1)
          char inAddress[48];
          inMsg.getAddress(inAddress);
          if (strncmp(inAddress, "/pit/led/player", 15) == 0) {
            int playerNum = atoi(&inAddress[15]);
            int baseChannel = (UNIT_ID - 1) * 4;
            if (playerNum >= (baseChannel + 1) && playerNum <= (baseChannel + 4)) {
              int localBtn = playerNum - (baseChannel + 1);
              float val = 0.0f;
              if (inMsg.isFloat(0)) val = inMsg.getFloat(0);
              if (val <= 0.01f) {
                BUTTONS[localBtn].lightMode = LIGHT_ATTRACT;
                Serial.print("[OSC Expiry] Station Button ");
                Serial.print(localBtn + 1);
                Serial.println(" -> Released from Lock back to Attract");
              }
            }
          }
        }
      }
    }
  }

  // --- 4. Physical Button Debounce & Batching ---
  bool stateChanged = false;

  for (int i = 0; i < 4; i++) {
    int reading = digitalRead(BUTTONS[i].btnPin);

    if (reading != BUTTONS[i].lastRawState) {
      if ((now - BUTTONS[i].lastDebounceTime) > DEBOUNCE_MS) {
        BUTTONS[i].lastDebounceTime = now;
        BUTTONS[i].lastRawState     = reading;

        if (reading == LOW) {
          // --- PRESSED ---
          BUTTONS[i].isPressed      = true;
          BUTTONS[i].pressStartTime = now;
          lastGlobalActivityTime    = now;
          stateChanged              = true;

          // If button was NOT already active, begin charging
          if (BUTTONS[i].lightMode != LIGHT_LOCKED_ACTIVE) {
            BUTTONS[i].lightMode = LIGHT_CHARGING;
            Serial.print("[Press] Button ");
            Serial.print(i + 1);
            Serial.println(" -> Charging Crystal...");
          } else {
            Serial.print("[Press Active] Button ");
            Serial.print(i + 1);
            Serial.println(" -> Tap to Cut or Hold to Recharge");
          }
        } else {
          // --- RELEASED ---
          BUTTONS[i].isPressed   = false;
          BUTTONS[i].releaseTime = now;
          lastGlobalActivityTime = now;
          stateChanged           = true;

          unsigned long pressHold = now - BUTTONS[i].pressStartTime;

          if (BUTTONS[i].lightMode == LIGHT_LOCKED_ACTIVE && pressHold <= TAP_CUT_MAX_MS) {
            // TAP TO CUT: Quick tap on active button deactivates/cuts it!
            BUTTONS[i].lightMode = LIGHT_DECAY;
            Serial.print("[Tap to Cut] Button ");
            Serial.print(i + 1);
            Serial.println(" -> Crystal Deactivated (Cut)");
          } else if (BUTTONS[i].lightMode == LIGHT_CHARGING) {
            // Early release before 100% -> fizzle decay
            BUTTONS[i].lightMode = LIGHT_DECAY;
            Serial.print("[Release] Button ");
            Serial.print(i + 1);
            Serial.println(" -> Incomplete Charge, Decay");
          } else {
            Serial.print("[Release] Button ");
            Serial.print(i + 1);
            Serial.println(" -> Active Stem Sustained");
          }
        }
      }
    }
  }

  // Dispatch at most ONE state snapshot packet even if multiple buttons changed at once
  if (stateChanged) {
    triggerStateChange();
  }

  // --- 5. Run Local 60 FPS Lighting Engine ---
  updateLighting(now);

  // --- 6. Reliable OSC Retransmission Loop ---
  if (needsAck) {
    if (now - lastSendTime >= RETRY_INTERVAL_MS) {
      if (retryCount < MAX_RETRIES) {
        lastSendTime = now;
        retryCount++;
        sendStatePacket();
      } else {
        // Battery Saver Stop after 2.0s without ACK
        needsAck = false;
        Serial.println("[OSC Battery Saver] Radio paused until next button press or sync.");
      }
    }
  }

  // FreeRTOS CPU core clock-gating yield
  delay(2);
}
