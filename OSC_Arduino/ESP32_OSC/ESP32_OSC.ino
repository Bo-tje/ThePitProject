// --- The Pit Project: Scalable ESP32 Controller ---
// Core: ESP-WROOM-32 with native Wi-Fi
// Feature: Scalable Station table, Baolian Marquee Protocol Driver, Bidirectional OSC

#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>

// ============================================================================
// 1. Wi-Fi & Network Configuration
// ============================================================================
const bool DEBUG_OFFLINE_MODE = false; // Set to true to test buttons & LEDs without Wi-Fi

char ssid[] = "HarmonicInstallation";              // Wi-Fi SSID
char pass[] = "Boislief45";                   // Wi-Fi Password

IPAddress outIp(192, 168, 0, 100);            // Target PC IP on network
const unsigned int outPort = 8000;  // Unreal Engine OSC Server Port
const unsigned int inPort  = 8888;  // Local Port for Incoming Game State

WiFiUDP Udp;

// ============================================================================
// 2. Baolian LED_D V3.1 Working Protocol Commands
// ============================================================================
#define CMD_BREATHING     0x00  // Multi-color breathing fade
#define CMD_BLUE          0x01  // Solid Blue
#define CMD_RED           0x02  // Solid Red
#define CMD_GREEN         0x03  // Solid Green
#define CMD_PURPLE        0x04  // Station 3 (Magenta / Purple)
#define CMD_CYAN          0x05  // Station 1 (Cyan / Sky Blue)
#define CMD_YELLOW        0x06  // Station 2 (Amber / Yellow)
#define CMD_WHITE         0x07  // Solid White
#define CMD_JUMP          0x08  // Color jump
#define CMD_MARQUEE       0x09  // Rotating rainbow chase
#define CMD_ALWAYS_ON     0x10  // Solid on
#define CMD_OFF           0x11  // Off

// --- Mode Mapping ---
#define CMD_ATTRACT_RAINBOW   CMD_MARQUEE    // 0x09: Rotating rainbow pattern in Attract
#define CMD_BREATHING_MODE    CMD_BREATHING  // 0x00: Breathing light mode upon press/release

// --- Configurable State Delays ---
const unsigned long HOLD_TO_SOLID_MS       = 2500; // Hold button for 2.5s -> goes to Solid Color
const unsigned long RELEASE_TO_ATTRACT_MS   = 6000; // After release, wait 6.0s idle -> goes to Attract Rainbow

// ============================================================================
// 3. Scalable Station Table
// ============================================================================
struct Station {
  int btnPin;                   // KEY wire (INPUT_PULLUP to GND)
  int ledPin;                   // Status LED (-1 if none)
  int dataPin;                  // DAT pin for Baolian ring (-1 if none)
  int clockPin;                 // CLK pin for Baolian ring (-1 if none)
  uint8_t colorCmd;             // Station assigned solid color command
  const char* oscPath;          // OSC address (e.g. /player1)
  int lastState;                // Debounce tracker
  unsigned long lastDebounce;
  bool isPressed;
  unsigned long pressStartTime;
};

// Physical Station Configuration (ESP32 Pins):
// Station 1: Button on GPIO 4, Status LED on GPIO 2, Baolian Ring on GPIO 22 (DAT) and GPIO 23 (CLK)
// Station 2: Button on GPIO 19 (Tactile button only)
// Station 3: Button on GPIO 21 (Tactile button only)
Station STATIONS[] = {
  {  4,  2, 22, 23, CMD_CYAN,   "/player1", HIGH, 0, false, 0 }, // Station 1 (Baolian Ring + Status LED)
  { 19, -1, -1, -1, CMD_YELLOW, "/player2", HIGH, 0, false, 0 }, // Station 2 (Tactile button only)
  { 21, -1, -1, -1, CMD_PURPLE, "/player3", HIGH, 0, false, 0 }  // Station 3 (Tactile button only)
};
const int NUM_STATIONS = sizeof(STATIONS) / sizeof(STATIONS[0]);
const unsigned long DEBOUNCE_MS = 30;

// Hardware Unit & Reliable OSC Protocol Configuration
const int UNIT_ID = 1;
const char* UNIT_OSC_PATH = "/pit/unit1/inputs";

uint32_t currentSequence = 1; // Increments on every state change
uint32_t lastAckedSequence = 0;
bool needsAck = true; // Send initial state upon boot until ACKed
unsigned long lastSendTime = 0;
int retryCount = 0;

const unsigned long RETRY_INTERVAL_MS = 40; // Low-latency retry interval (40ms)
const int MAX_RETRIES = 50;                 // 50 * 40ms = 2.0s maximum attempt window to preserve battery

// ============================================================================
// 4. Low-Level Marquee Protocol Driver
// ============================================================================
unsigned long lastCmdTime = 0;
uint8_t currentRingCmd = 0xFF; // Prevents re-sending identical commands

void sendMarqueeCommand(int dataPin, int clockPin, uint8_t cmd) {
  if (dataPin < 0 || clockPin < 0) return;

  // Ensure CLK has been HIGH for >= 250ms since last transmission (Datasheet spec)
  unsigned long elapsed = millis() - lastCmdTime;
  if (elapsed < 260) {
    delay(260 - elapsed);
  }

  // Transmit 8 bits MSB first (B7 to B0)
  for (int i = 7; i >= 0; i--) {
    // CLK goes LOW for > 1ms while data is set
    digitalWrite(clockPin, LOW);
    digitalWrite(dataPin, (cmd & (1 << i)) ? HIGH : LOW);
    delayMicroseconds(1500); // 1.5ms (> 1ms spec)

    // CLK goes HIGH for > 1ms (chip latches on rising edge)
    digitalWrite(clockPin, HIGH);
    delayMicroseconds(1500); // 1.5ms (> 1ms spec)
  }

  digitalWrite(dataPin, LOW);
  digitalWrite(clockPin, HIGH); // CLK idles HIGH
  lastCmdTime = millis();
}

void setRingMode(int dataPin, int clockPin, uint8_t cmd) {
  if (cmd == currentRingCmd) return; // Only transmit on real state change
  sendMarqueeCommand(dataPin, clockPin, cmd);
  currentRingCmd = cmd;
}

// ============================================================================
// 5. High-Level Ring State Machine
// ============================================================================
enum PitState {
  STATE_ATTRACT,
  STATE_ACTIVE
};
PitState currentState = STATE_ATTRACT;

// Ring sequence tracking
bool isSolidActive = false;
bool isWaitingForAttract = false;
unsigned long lastReleaseTime = 0;

void setStationRingState(int stationIdx) {
  if (stationIdx < 0 || stationIdx >= NUM_STATIONS) return;
  int dPin = STATIONS[stationIdx].dataPin;
  int cPin = STATIONS[stationIdx].clockPin;
  if (dPin < 0 || cPin < 0) return;

  if (currentState == STATE_ATTRACT) {
    setRingMode(dPin, cPin, CMD_ATTRACT_RAINBOW);
  } else if (STATIONS[stationIdx].isPressed) {
    if (isSolidActive) {
      setRingMode(dPin, cPin, STATIONS[stationIdx].colorCmd);
    } else {
      setRingMode(dPin, cPin, CMD_BREATHING_MODE);
    }
  } else {
    // Active but released
    setRingMode(dPin, cPin, CMD_BREATHING_MODE);
  }
}

// ============================================================================
// 6. Reliable OSC Transport (State Snapshot & ACK Protocol)
// ============================================================================
void sendStatePacket() {
  uint8_t packet[96];
  int len = 0;

  // 1. OSC Address (4-byte null padded)
  int addrLen = strlen(UNIT_OSC_PATH);
  memcpy(&packet[len], UNIT_OSC_PATH, addrLen);
  len += addrLen;
  packet[len++] = '\0';
  while (len % 4 != 0) packet[len++] = '\0';

  // 2. OSC Type Tags: ",i" + "f" * NUM_STATIONS (4-byte null padded)
  packet[len++] = ',';
  packet[len++] = 'i';
  for (int i = 0; i < NUM_STATIONS; i++) {
    packet[len++] = 'f';
  }
  packet[len++] = '\0';
  while (len % 4 != 0) packet[len++] = '\0';

  // 3. Sequence ID (int32 big-endian)
  packet[len++] = (currentSequence >> 24) & 0xFF;
  packet[len++] = (currentSequence >> 16) & 0xFF;
  packet[len++] = (currentSequence >> 8) & 0xFF;
  packet[len++] = currentSequence & 0xFF;

  // 4. Button states (float big-endian IEEE 754)
  for (int i = 0; i < NUM_STATIONS; i++) {
    union { float f; uint8_t b[4]; } u;
    u.f = STATIONS[i].isPressed ? 1.0f : 0.0f;
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

  Serial.print(DEBUG_OFFLINE_MODE ? ">>> [DEBUG OSC (Offline)] " : ">>> [OSC Sent] ");
  Serial.print(UNIT_OSC_PATH);
  Serial.print(" seq=");
  Serial.print(currentSequence);
  Serial.print(" states=[");
  for (int i = 0; i < NUM_STATIONS; i++) {
    Serial.print(STATIONS[i].isPressed ? "1" : "0");
    if (i < NUM_STATIONS - 1) Serial.print(", ");
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
// 7. Setup
// ============================================================================
void setup() {
  // Power Optimization 1: Clock down from 240 MHz to 80 MHz
  // Drops CPU baseline current by ~40mA while remaining nearly 2x faster than the Nano 33 IoT
  setCpuFrequencyMhz(80);

  // Power Optimization 2: Turn off Bluetooth radio hardware completely
  btStop();

  delay(1000);

  Serial.begin(115200);
  unsigned long startWait = millis();
  while (!Serial && (millis() - startWait < 3000)) {
    delay(10);
  }

  for (int i = 0; i < NUM_STATIONS; i++) {
    pinMode(STATIONS[i].btnPin, INPUT_PULLUP);
    if (STATIONS[i].ledPin >= 0) {
      pinMode(STATIONS[i].ledPin, OUTPUT);
      digitalWrite(STATIONS[i].ledPin, LOW);
    }
    if (STATIONS[i].dataPin >= 0 && STATIONS[i].clockPin >= 0) {
      pinMode(STATIONS[i].dataPin, OUTPUT);
      pinMode(STATIONS[i].clockPin, OUTPUT);
      digitalWrite(STATIONS[i].dataPin, LOW);
      digitalWrite(STATIONS[i].clockPin, HIGH); // CLK idles HIGH per datasheet
    }
  }

  Serial.println("==========================================");
  Serial.println(" THE PIT: DOME RING CONTROLLER (ESP32)    ");
  Serial.println(" Sequence: Rainbow (0x09) -> Breathing (0x00) -> Solid -> Breathing -> Rainbow ");
  Serial.println("==========================================");

  // Initialize ring in Attract Mode (Rotating Rainbow)
  currentState = STATE_ATTRACT;
  for (int i = 0; i < NUM_STATIONS; i++) {
    if (STATIONS[i].dataPin >= 0) {
      setRingMode(STATIONS[i].dataPin, STATIONS[i].clockPin, CMD_ATTRACT_RAINBOW);
    }
  }

  if (DEBUG_OFFLINE_MODE) {
    Serial.println("\n*** DEBUG_OFFLINE_MODE IS ACTIVE ***");
    Serial.println("- Wi-Fi connection skipped for fast bench testing.");
    Serial.println("- Press buttons, send 1, 2, 3, or 'a' / 'g' in Serial.");
    Serial.println("- Send 'k' to simulate incoming ACK from Unreal Engine.\n");
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Disable modem sleep for ultra low-latency OSC response
    // Power Optimization 3: Reduce Wi-Fi TX power from 20dBm to 15dBm (reduces peak TX current spikes)
    WiFi.setTxPower(WIFI_POWER_15dBm);
    WiFi.begin(ssid, pass);
    
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(ssid);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();

    Udp.begin(inPort);
    Serial.print("[Wi-Fi Connected!] ESP32 IP: ");
    Serial.println(WiFi.localIP());

    // Transmit initial state upon boot until acknowledged
    sendStatePacket();
  }
}

// ============================================================================
// 8. Main Loop
// ============================================================================
void loop() {
  unsigned long now = millis();

  // Reconnect Wi-Fi if dropped in online mode
  if (!DEBUG_OFFLINE_MODE && WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[!] Wi-Fi lost, reconnecting...");
    WiFi.disconnect();
    delay(200);
    WiFi.begin(ssid, pass);
    unsigned long reconnStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - reconnStart < 4000) {
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
    return;
  }

  // --- Serial Simulation ---
  if (Serial.available()) {
    char c = Serial.read();
    int idx = -1;
    if (c >= '1' && c <= ('0' + NUM_STATIONS)) {
      idx = (c - '1');
    } else if (c == 'k' || c == 'K') {
      Serial.print("<<< [Serial Simulated ACK] seq ");
      Serial.print(currentSequence);
      Serial.println(" -> Entering REST mode");
      needsAck = false;
      lastAckedSequence = currentSequence;
    } else if (c == 'a' || c == 'A') {
      currentState = STATE_ATTRACT;
      isWaitingForAttract = false;
      isSolidActive = false;
      Serial.println("<<< [Serial] State: ATTRACT MODE (Rainbow)");
      for (int i = 0; i < NUM_STATIONS; i++) setStationRingState(i);
    } else if (c == 'g' || c == 'G') {
      currentState = STATE_ACTIVE;
      isWaitingForAttract = false;
      Serial.println("<<< [Serial] State: ACTIVE GAMEPLAY");
      for (int i = 0; i < NUM_STATIONS; i++) setStationRingState(i);
    }

    if (idx >= 0 && idx < NUM_STATIONS) {
      STATIONS[idx].isPressed = !STATIONS[idx].isPressed;
      Serial.print("[Serial Toggle] ");
      Serial.print(STATIONS[idx].oscPath);
      Serial.println(STATIONS[idx].isPressed ? " => ON (1.0)" : " => OFF (0.0)");
      triggerStateChange();

      if (STATIONS[idx].isPressed) {
        STATIONS[idx].pressStartTime = now;
        currentState = STATE_ACTIVE;
        isSolidActive = false;
        isWaitingForAttract = false;
        setStationRingState(idx);
      } else {
        isSolidActive = false;
        lastReleaseTime = now;
        isWaitingForAttract = true;
        setStationRingState(idx);
      }
    }
  }

  // --- Incoming OSC Poll (Unthrottled for sub-millisecond ACK capture) ---
  if (!DEBUG_OFFLINE_MODE) {
    int packetSize = Udp.parsePacket();
    if (packetSize > 0) {
      OSCMessage inMsg;
      while (packetSize--) {
        inMsg.fill(Udp.read());
      }
      if (!inMsg.hasError()) {
        if (inMsg.match("/pit/ack")) {
          int32_t ackSeq = 0;
          if (inMsg.isInt(0)) {
            ackSeq = inMsg.getInt(0);
          }
          if (ackSeq >= currentSequence) {
            if (needsAck) {
              Serial.print("<<< [ACK Received] seq ");
              Serial.print(ackSeq);
              Serial.println(" -> Entering REST mode");
            }
            needsAck = false;
            lastAckedSequence = ackSeq;
          }
        } else if (inMsg.match("/pit/sync")) {
          Serial.println("<<< [Sync Request Received] Resending current state snapshot");
          triggerStateChange();
        } else if (inMsg.match("/pit/attract")) {
          currentState = STATE_ATTRACT;
          isWaitingForAttract = false;
          isSolidActive = false;
          Serial.println("<<< State: ATTRACT MODE (Rainbow)");
          for (int i = 0; i < NUM_STATIONS; i++) {
            setStationRingState(i);
          }
        } else if (inMsg.match("/pit/gameplay")) {
          currentState = STATE_ACTIVE;
          isWaitingForAttract = false;
          Serial.println("<<< State: ACTIVE GAMEPLAY");
          for (int i = 0; i < NUM_STATIONS; i++) {
            setStationRingState(i);
          }
        }
      }
    }
  }

  // --- Button Read & Physical Inputs ---
  for (int i = 0; i < NUM_STATIONS; i++) {
    int reading = digitalRead(STATIONS[i].btnPin);

    if (reading != STATIONS[i].lastState) {
      if ((now - STATIONS[i].lastDebounce) > DEBOUNCE_MS) {
        STATIONS[i].lastDebounce = now;
        STATIONS[i].lastState = reading;

        if (reading == LOW) {
          // --- BUTTON PRESSED (Charging / Breathing) ---
          STATIONS[i].isPressed = true;
          STATIONS[i].pressStartTime = now;
          currentState = STATE_ACTIVE;
          isSolidActive = false;
          isWaitingForAttract = false;

          Serial.print("[Button Pressed] ");
          Serial.print(STATIONS[i].oscPath);
          Serial.println(" -> Step 1: Charging (Breathing Mode 0x00)");
          triggerStateChange();

          setStationRingState(i);
        } else {
          // --- BUTTON RELEASED ---
          STATIONS[i].isPressed = false;
          isSolidActive = false;
          lastReleaseTime = now;
          isWaitingForAttract = true;

          Serial.print("[Button Released] ");
          Serial.print(STATIONS[i].oscPath);
          Serial.println(" -> Step 3: Return to Breathing (0x00) - Inactivity timer started");
          triggerStateChange();

          setStationRingState(i);
        }
      }
    }

    // --- Check Hold Duration while Button is Held (Step 2: Charge Reached -> Solid Color) ---
    if (STATIONS[i].isPressed && !isSolidActive) {
      if (now - STATIONS[i].pressStartTime >= HOLD_TO_SOLID_MS) {
        isSolidActive = true;
        Serial.print("[Charge Complete] ");
        Serial.print(STATIONS[i].oscPath);
        Serial.println(" -> Step 2: Locked Solid Color");
        setStationRingState(i);
      }
    }

    // Status LED mirror
    if (STATIONS[i].ledPin >= 0) {
      digitalWrite(STATIONS[i].ledPin, STATIONS[i].isPressed ? HIGH : LOW);
    }
  }

  // --- Check Inactivity Timeout after Release (Step 4: Return to Attract Rainbow) ---
  if (isWaitingForAttract && currentState == STATE_ACTIVE) {
    bool anyPressed = false;
    for (int i = 0; i < NUM_STATIONS; i++) {
      if (STATIONS[i].isPressed) {
        anyPressed = true;
        break;
      }
    }

    if (!anyPressed && (now - lastReleaseTime >= RELEASE_TO_ATTRACT_MS)) {
      isWaitingForAttract = false;
      currentState = STATE_ATTRACT;
      Serial.println("[Inactivity Timeout] -> Step 4: Return to Attract Mode (Rainbow)");
      for (int i = 0; i < NUM_STATIONS; i++) {
        setStationRingState(i);
      }
    }
  }

  // --- Retransmission Loop (Battery-Saver Sleep when Timed Out or ACKed) ---
  if (needsAck) {
    if (now - lastSendTime >= RETRY_INTERVAL_MS) {
      if (retryCount < MAX_RETRIES) {
        lastSendTime = now;
        retryCount++;
        sendStatePacket();
      } else {
        // Battery Saver Hard Stop: give up after 2.0s without ACK.
        // Halts all radio transmission; self-heals on next button press or incoming /pit/sync.
        needsAck = false;
        Serial.println("[OSC Battery Saver] No ACK after 2.0s. Stopping radio transmission until next press or sync.");
      }
    }
  }

  // Power Optimization 4: Yield 2ms to FreeRTOS IDLE task to enable CPU core clock-gating
  delay(2);
}
