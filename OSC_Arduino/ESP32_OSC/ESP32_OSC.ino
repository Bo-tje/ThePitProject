#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <esp_arduino_version.h>

const int UNIT_ID = 1;
const bool DEBUG_OFFLINE_MODE = false;

char ssid[] = "HarmonicInstallation";
char pass[] = "Boislief45";

IPAddress outIp(192, 168, 0, 102);
const unsigned int outPort = 8000;
const unsigned int inPort  = 8888;

WiFiUDP Udp;

const int BUTTON_COUNT = 4;
const int BTN_PINS[BUTTON_COUNT] = { 21, 19, 18, 22 };
const int LED_PINS[BUTTON_COUNT] = { 27, 26, 25, 33 };

const int PWM_FREQ = 1000;
const int PWM_RESOLUTION = 8;
const uint8_t OPTOCOUPLER_MIN_CONDUCT_DUTY = 12;

const unsigned long DEBOUNCE_MS          = 25;
unsigned long chargeTimeMs               = 2000;
const unsigned long FLASH_DURATION_MS    = 120;
const unsigned long DECAY_TIME_MS        = 350;
unsigned long idleToAttractMs            = 4000;

int maxActivePerStation                  = 2;
unsigned long tapCutMaxMs                = 400;
float fallbackBeatMs                     = 500.0f;

const unsigned long ATTRACT_CYCLE_MS     = 16000;

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

enum ButtonLightMode {
  LIGHT_ATTRACT,
  LIGHT_CHARGING,
  LIGHT_LOCKED_ACTIVE,
  LIGHT_DECAY
};

struct ButtonStation {
  int btnPin;
  int ledPin;
  int lastRawState;
  unsigned long lastDebounceTime;
  bool isPressed;
  ButtonLightMode lightMode;
  unsigned long pressStartTime;
  unsigned long releaseTime;
  unsigned long lockTime;
};

ButtonStation BUTTONS[BUTTON_COUNT];

enum GlobalState {
  STATE_ATTRACT_MODE,
  STATE_INTERACTIVE_MODE
};
GlobalState globalState = STATE_ATTRACT_MODE;

unsigned long lastGlobalActivityTime = 0;
unsigned long attractPhaseStartTime  = 0;
unsigned long lastBeatTickTime       = 0;

char unitOscPath[32];
uint32_t currentSequence   = 1;
bool needsAck              = true;
unsigned long lastSendTime = 0;
int retryCount             = 0;

const unsigned long RETRY_INTERVAL_MS = 40;
const int MAX_RETRIES                 = 50;

void sendStatePacket() {
  if (!DEBUG_OFFLINE_MODE) {
    OSCMessage msg(unitOscPath);
    msg.add((int32_t)currentSequence);
    for (int buttonIndex = 0; buttonIndex < BUTTON_COUNT; buttonIndex++) {
      msg.add(BUTTONS[buttonIndex].isPressed ? 1.0f : 0.0f);
    }
    Udp.beginPacket(outIp, outPort);
    msg.send(Udp);
    Udp.endPacket();
  }

  Serial.print(DEBUG_OFFLINE_MODE ? ">>> [OFFLINE OSC] " : ">>> [OSC Sent] ");
  Serial.print(unitOscPath);
  Serial.print(" seq=");
  Serial.print(currentSequence);
  Serial.print(" states=[");
  for (int buttonIndex = 0; buttonIndex < BUTTON_COUNT; buttonIndex++) {
    Serial.print(BUTTONS[buttonIndex].isPressed ? "1" : "0");
    if (buttonIndex < BUTTON_COUNT - 1) Serial.print(", ");
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

void applyButtonPressState(int buttonIndex, bool isNowPressed, unsigned long now) {
  ButtonStation& button = BUTTONS[buttonIndex];
  button.isPressed = isNowPressed;
  lastGlobalActivityTime = now;

  if (isNowPressed) {
    button.pressStartTime = now;
    if (button.lightMode != LIGHT_LOCKED_ACTIVE) {
      button.lightMode = LIGHT_CHARGING;
      Serial.printf("[Press] Button %d -> Charging Crystal...\n", buttonIndex + 1);
    } else {
      Serial.printf("[Press Active] Button %d -> Tap to Cut or Hold to Recharge\n", buttonIndex + 1);
    }
  } else {
    button.releaseTime = now;
    unsigned long pressHoldDurationMs = now - button.pressStartTime;
    bool wasQuickTapOnActiveCrystal = (button.lightMode == LIGHT_LOCKED_ACTIVE && pressHoldDurationMs <= tapCutMaxMs);
    bool releasedBeforeFullCharge = (button.lightMode == LIGHT_CHARGING);

    if (wasQuickTapOnActiveCrystal) {
      button.lightMode = LIGHT_DECAY;
      Serial.printf("[Tap to Cut] Button %d -> Crystal Deactivated (Cut)\n", buttonIndex + 1);
    } else if (releasedBeforeFullCharge) {
      button.lightMode = LIGHT_DECAY;
      Serial.printf("[Release] Button %d -> Incomplete Charge, Decay\n", buttonIndex + 1);
    } else {
      Serial.printf("[Release] Button %d -> Active Stem Sustained\n", buttonIndex + 1);
    }
  }
}

void writePwmDuty(int buttonIndex, uint8_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcWrite(BUTTONS[buttonIndex].ledPin, duty);
#else
  analogWrite(BUTTONS[buttonIndex].ledPin, duty);
#endif
}

void applyLedLinear(int buttonIndex, float linearBrightness) {
  if (linearBrightness <= 0.005f) {
    writePwmDuty(buttonIndex, 0);
    return;
  }

  float clampedBrightness = min(linearBrightness, 1.0f);
  uint8_t lutIndex = (uint8_t)(clampedBrightness * 255.0f);
  uint8_t gammaCorrectedDuty = pgm_read_byte(&GAMMA_LUT[lutIndex]);

  uint8_t optocouplerCompensatedDuty = OPTOCOUPLER_MIN_CONDUCT_DUTY +
    (uint8_t)(((float)gammaCorrectedDuty / 255.0f) * (255 - OPTOCOUPLER_MIN_CONDUCT_DUTY));
  writePwmDuty(buttonIndex, optocouplerCompensatedDuty);
}

void updateLighting(unsigned long now) {
  bool anyButtonPressed = false;
  bool anyCrystalLocked = false;
  for (int buttonIndex = 0; buttonIndex < BUTTON_COUNT; buttonIndex++) {
    if (BUTTONS[buttonIndex].isPressed) anyButtonPressed = true;
    if (BUTTONS[buttonIndex].lightMode == LIGHT_LOCKED_ACTIVE) anyCrystalLocked = true;
  }

  if (anyButtonPressed || anyCrystalLocked) {
    globalState = STATE_INTERACTIVE_MODE;
  } else if (globalState == STATE_INTERACTIVE_MODE && (now - lastGlobalActivityTime >= idleToAttractMs)) {
    globalState = STATE_ATTRACT_MODE;
    attractPhaseStartTime = now;
    for (int buttonIndex = 0; buttonIndex < BUTTON_COUNT; buttonIndex++) {
      BUTTONS[buttonIndex].lightMode = LIGHT_ATTRACT;
    }
    Serial.println("[State] Entering ATTRACT MODE");
  }

  if (globalState == STATE_ATTRACT_MODE) {
    unsigned long attractElapsedMs = now - attractPhaseStartTime;
    bool isCometPingPongPhase = ((attractElapsedMs / ATTRACT_CYCLE_MS) % 2) == 0;

    if (isCometPingPongPhase) {
      float pingPongCycle = fmod((float)now / 350.0f, 6.0f);
      float cometPosition = (pingPongCycle <= 3.0f) ? pingPongCycle : (6.0f - pingPongCycle);

      for (int buttonIndex = 0; buttonIndex < BUTTON_COUNT; buttonIndex++) {
        float distanceToComet = fabs((float)buttonIndex - cometPosition);
        float gaussianBrightness = expf(-distanceToComet * distanceToComet * 1.8f);
        applyLedLinear(buttonIndex, gaussianBrightness * 0.85f);
      }
    } else {
      float accordionWave = (sinf((float)now * 0.0035f) + 1.0f) * 0.5f;
      float outerButtonBrightness = accordionWave * 0.8f;
      float innerButtonBrightness = (1.0f - accordionWave) * 0.8f;

      applyLedLinear(0, outerButtonBrightness);
      applyLedLinear(1, innerButtonBrightness);
      applyLedLinear(2, innerButtonBrightness);
      applyLedLinear(3, outerButtonBrightness);
    }
    return;
  }

  for (int buttonIndex = 0; buttonIndex < BUTTON_COUNT; buttonIndex++) {
    ButtonStation& button = BUTTONS[buttonIndex];

    switch (button.lightMode) {
      case LIGHT_CHARGING: {
        unsigned long holdDurationMs = now - button.pressStartTime;
        float chargeProgress = min((float)holdDurationMs / (float)chargeTimeMs, 1.0f);
        float chargeShimmer = sinf((float)now * 0.045f) * (0.08f * chargeProgress);
        float chargingBrightness = constrain((chargeProgress * chargeProgress) + chargeShimmer, 0.0f, 1.0f);
        applyLedLinear(buttonIndex, chargingBrightness);

        if (holdDurationMs >= chargeTimeMs) {
          int activeCrystalCount = 0;
          int oldestActiveButtonIndex = -1;
          unsigned long oldestLockTime = 0xFFFFFFFF;

          for (int otherIndex = 0; otherIndex < BUTTON_COUNT; otherIndex++) {
            if (BUTTONS[otherIndex].lightMode == LIGHT_LOCKED_ACTIVE) {
              activeCrystalCount++;
              if (BUTTONS[otherIndex].lockTime < oldestLockTime) {
                oldestLockTime = BUTTONS[otherIndex].lockTime;
                oldestActiveButtonIndex = otherIndex;
              }
            }
          }

          if (activeCrystalCount >= maxActivePerStation && oldestActiveButtonIndex >= 0) {
            BUTTONS[oldestActiveButtonIndex].lightMode = LIGHT_DECAY;
            Serial.printf("[Capacity] Max active (%d) reached -> Swapping oldest Button %d\n",
                          maxActivePerStation, oldestActiveButtonIndex + 1);
          }

          button.lightMode = LIGHT_LOCKED_ACTIVE;
          button.lockTime = now;
          Serial.printf("[Charge Complete] Button %d -> CRYSTAL LOCKED ACTIVE!\n", buttonIndex + 1);
        }
        break;
      }

      case LIGHT_LOCKED_ACTIVE: {
        unsigned long timeSinceLockMs = now - button.lockTime;
        if (timeSinceLockMs < FLASH_DURATION_MS) {
          applyLedLinear(buttonIndex, 1.0f);
        } else {
          float beatPulseBonus = 0.0f;
          if (lastBeatTickTime > 0) {
            unsigned long beatElapsedMs = now - lastBeatTickTime;
            if (beatElapsedMs < 250) {
              beatPulseBonus = (1.0f - ((float)beatElapsedMs / 250.0f)) * 0.30f;
            }
          } else {
            float localBeatPhase = fmod((float)now, fallbackBeatMs) / fallbackBeatMs;
            if (localBeatPhase < 0.35f) {
              beatPulseBonus = (1.0f - (localBeatPhase / 0.35f)) * 0.20f;
            }
          }
          applyLedLinear(buttonIndex, 0.70f + beatPulseBonus);
        }
        break;
      }

      case LIGHT_DECAY: {
        unsigned long decayElapsedMs = now - button.releaseTime;
        if (decayElapsedMs < DECAY_TIME_MS) {
          float decayRemaining = 1.0f - ((float)decayElapsedMs / (float)DECAY_TIME_MS);
          applyLedLinear(buttonIndex, decayRemaining * 0.5f);
        } else {
          button.lightMode = LIGHT_ATTRACT;
          applyLedLinear(buttonIndex, 0.0f);
        }
        break;
      }

      case LIGHT_ATTRACT:
      default: {
        float idleGuideBrightness = anyCrystalLocked ? 0.08f : 0.0f;
        applyLedLinear(buttonIndex, idleGuideBrightness);
        break;
      }
    }
  }
}

void setup() {
  setCpuFrequencyMhz(80);
  btStop();

  delay(500);
  Serial.begin(115200);
  unsigned long serialWaitStartMs = millis();
  while (!Serial && (millis() - serialWaitStartMs < 2500)) delay(10);

  snprintf(unitOscPath, sizeof(unitOscPath), "/pit/unit%d/inputs", UNIT_ID);

  Serial.println("\n==============================================");
  Serial.printf(" THE PIT PROJECT: STATION CONTROLLER (UNIT %d)\n", UNIT_ID);
  Serial.println("==============================================");

  for (int buttonIndex = 0; buttonIndex < BUTTON_COUNT; buttonIndex++) {
    BUTTONS[buttonIndex].btnPin       = BTN_PINS[buttonIndex];
    BUTTONS[buttonIndex].ledPin       = LED_PINS[buttonIndex];
    BUTTONS[buttonIndex].lastRawState = HIGH;
    BUTTONS[buttonIndex].isPressed    = false;
    BUTTONS[buttonIndex].lightMode    = LIGHT_ATTRACT;

    pinMode(BUTTONS[buttonIndex].btnPin, INPUT_PULLUP);
    pinMode(BUTTONS[buttonIndex].ledPin, OUTPUT);
    digitalWrite(BUTTONS[buttonIndex].ledPin, LOW);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcAttach(BUTTONS[buttonIndex].ledPin, PWM_FREQ, PWM_RESOLUTION);
    ledcWrite(BUTTONS[buttonIndex].ledPin, 0);
#else
    analogWriteFrequency(BUTTONS[buttonIndex].ledPin, PWM_FREQ);
    analogWriteResolution(BUTTONS[buttonIndex].ledPin, PWM_RESOLUTION);
    analogWrite(BUTTONS[buttonIndex].ledPin, 0);
#endif
  }

  attractPhaseStartTime = millis();

  if (DEBUG_OFFLINE_MODE) {
    Serial.println("\n*** DEBUG_OFFLINE_MODE ACTIVE ***");
    Serial.println("- Wi-Fi connection skipped for fast bench testing.");
    Serial.println("- Press buttons or type '1', '2', '3', '4' in Serial.");
    Serial.println("- Send 'k' to simulate ACK, 'b' to simulate beat tick.\n");
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_15dBm);
    WiFi.begin(ssid, pass);

    Serial.printf("Connecting to Wi-Fi '%s'", ssid);
    while (WiFi.status() != WL_CONNECTED) {
      delay(300);
      Serial.print(".");
    }
    Serial.println("\n[Wi-Fi Connected!] ESP32 IP: " + WiFi.localIP().toString());

    Udp.begin(inPort);
    sendStatePacket();
  }
}

void loop() {
  unsigned long now = millis();

  if (!DEBUG_OFFLINE_MODE && WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    delay(200);
    WiFi.begin(ssid, pass);
    unsigned long reconnectStartMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - reconnectStartMs < 3000) {
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

  // Serial Simulation for Bench Testing
  if (Serial.available()) {
    char serialCommand = Serial.read();
    if (serialCommand >= '1' && serialCommand <= '4') {
      int buttonIndex = serialCommand - '1';
      applyButtonPressState(buttonIndex, !BUTTONS[buttonIndex].isPressed, now);
      triggerStateChange();
    } else if (serialCommand == 'm' || serialCommand == 'M') {
      maxActivePerStation = (maxActivePerStation % BUTTON_COUNT) + 1;
      Serial.printf("<<< [Serial Config] maxActivePerStation = %d\n", maxActivePerStation);
    } else if (serialCommand == 'b' || serialCommand == 'B') {
      lastBeatTickTime = now;
      Serial.println("<<< [Serial Beat Tick]");
    } else if (serialCommand == 'k' || serialCommand == 'K') {
      needsAck = false;
      Serial.println("<<< [Serial Simulated ACK]");
    } else if (serialCommand == 'a' || serialCommand == 'A') {
      globalState = STATE_ATTRACT_MODE;
      for (int buttonIndex = 0; buttonIndex < BUTTON_COUNT; buttonIndex++) {
        BUTTONS[buttonIndex].lightMode = LIGHT_ATTRACT;
      }
      Serial.println("<<< [Serial Force ATTRACT MODE]");
    }
  }

  if (!DEBUG_OFFLINE_MODE) {
    int packetSize = Udp.parsePacket();
    if (packetSize > 0) {
      OSCMessage incomingOsc;
      while (packetSize--) incomingOsc.fill(Udp.read());

      if (!incomingOsc.hasError()) {
        if (incomingOsc.match("/pit/ack")) {
          int32_t ackedSequence = incomingOsc.isInt(0) ? incomingOsc.getInt(0) : 0;
          if (ackedSequence >= (int32_t)currentSequence) {
            needsAck = false;
          }
        } else if (incomingOsc.match("/pit/sync")) {
          if (incomingOsc.isInt(0)) chargeTimeMs = (unsigned long)max(100, (int)incomingOsc.getInt(0));
          if (incomingOsc.isInt(1)) tapCutMaxMs = (unsigned long)max(50, (int)incomingOsc.getInt(1));
          if (incomingOsc.isInt(2)) idleToAttractMs = (unsigned long)max(500, (int)incomingOsc.getInt(2));
          if (incomingOsc.isInt(3)) maxActivePerStation = constrain((int)incomingOsc.getInt(3), 1, BUTTON_COUNT);
          if (incomingOsc.isFloat(4)) {
            float syncedBpm = constrain(incomingOsc.getFloat(4), 40.0f, 240.0f);
            fallbackBeatMs = 60000.0f / syncedBpm;
          }
          Serial.printf("<<< [Sync] charge=%lums tapCut=%lums idle=%lums maxActive=%d beatMs=%.1f\n",
                        chargeTimeMs, tapCutMaxMs, idleToAttractMs, maxActivePerStation, fallbackBeatMs);
          triggerStateChange();
        } else if (incomingOsc.match("/pit/beat") || incomingOsc.match("/pit/clock/bar")) {
          lastBeatTickTime = now;
        } else if (incomingOsc.match("/pit/attract")) {
          globalState = STATE_ATTRACT_MODE;
          attractPhaseStartTime = now;
          for (int buttonIndex = 0; buttonIndex < BUTTON_COUNT; buttonIndex++) {
            BUTTONS[buttonIndex].lightMode = LIGHT_ATTRACT;
          }
        } else if (incomingOsc.match("/pit/led/all")) {
          float targetLedLevel = incomingOsc.isFloat(0) ? incomingOsc.getFloat(0) : 0.0f;
          if (targetLedLevel <= 0.01f) {
            for (int buttonIndex = 0; buttonIndex < BUTTON_COUNT; buttonIndex++) {
              BUTTONS[buttonIndex].lightMode = LIGHT_ATTRACT;
            }
          }
        } else {
          char incomingAddress[48];
          incomingOsc.getAddress(incomingAddress);
          if (strncmp(incomingAddress, "/pit/led/player", 15) == 0) {
            int globalPlayerNumber = atoi(&incomingAddress[15]);
            int unitBaseChannel = (UNIT_ID - 1) * BUTTON_COUNT;
            if (globalPlayerNumber >= (unitBaseChannel + 1) && globalPlayerNumber <= (unitBaseChannel + BUTTON_COUNT)) {
              int localButtonIndex = globalPlayerNumber - (unitBaseChannel + 1);
              float targetLedLevel = incomingOsc.isFloat(0) ? incomingOsc.getFloat(0) : 0.0f;
              if (targetLedLevel <= 0.01f) {
                BUTTONS[localButtonIndex].lightMode = LIGHT_ATTRACT;
                Serial.printf("[OSC Expiry] Station Button %d -> Released from Lock back to Attract\n", localButtonIndex + 1);
              }
            }
          }
        }
      }
    }
  }

  bool anyButtonStateChanged = false;

  for (int buttonIndex = 0; buttonIndex < BUTTON_COUNT; buttonIndex++) {
    int rawPinState = digitalRead(BUTTONS[buttonIndex].btnPin);

    if (rawPinState != BUTTONS[buttonIndex].lastRawState &&
        (now - BUTTONS[buttonIndex].lastDebounceTime) > DEBOUNCE_MS) {
      BUTTONS[buttonIndex].lastDebounceTime = now;
      BUTTONS[buttonIndex].lastRawState = rawPinState;

      bool isNowPressed = (rawPinState == LOW);
      applyButtonPressState(buttonIndex, isNowPressed, now);
      anyButtonStateChanged = true;
    }
  }

  if (anyButtonStateChanged) {
    triggerStateChange();
  }

  updateLighting(now);

  if (needsAck && (now - lastSendTime >= RETRY_INTERVAL_MS)) {
    if (retryCount < MAX_RETRIES) {
      lastSendTime = now;
      retryCount++;
      sendStatePacket();
    } else {
      needsAck = false;
      Serial.println("[OSC Battery Saver] Radio paused until next button press or sync.");
    }
  }

  delay(2);
}
