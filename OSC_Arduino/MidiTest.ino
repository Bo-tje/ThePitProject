// --- The Pit Project: Scalable ESP32 OSC Controller ---
// Board: ESP32 (Wi-Fi / Wokwi Simulator / UDP OSC)
// Feature: Scalable Station table, Local Non-blocking LED Animation, Bidirectional OSC

#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>

// ============================================================================
// 1. Network Configuration
// ============================================================================
const char* ssid = "Wokwi-GUEST";
const char* password = "";

const unsigned int outPort = 8000; // UE5 OSC Server Port
const unsigned int inPort  = 8888; // Port for Incoming State updates from UE5

WiFiUDP Udp;
IPAddress outIp;

// ============================================================================
// 2. Scalable Station Definition (Add / Remove Stations Here)
// ============================================================================
struct Station {
  int btnPin;
  int ledPin;
  const char* oscPath;
  int lastState;
  unsigned long lastDebounce;
};

// 4-Corner Setup (NW, NE, SE, SW)
Station STATIONS[] = {
  { 18, 13, "/pit/corner/nw", HIGH, 0 },
  { 19, 12, "/pit/corner/ne", HIGH, 0 },
  { 22, 14, "/pit/corner/se", HIGH, 0 },
  { 23, 27, "/pit/corner/sw", HIGH, 0 }
};
const int NUM_STATIONS = sizeof(STATIONS) / sizeof(STATIONS[0]);
const unsigned long DEBOUNCE_DELAY_MS = 20;

// ============================================================================
// 3. Local State Machine
// ============================================================================
enum PitState {
  STATE_ATTRACT,
  STATE_ACTIVE,
  STATE_CRESCENDO
};
PitState currentState = STATE_ATTRACT;

void sendOSC(const char* address, float value) {
  OSCMessage msg(address);
  msg.add(value);
  
  Udp.beginPacket(outIp, outPort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();

  Serial.print(">>> [OSC -> UE5] ");
  Serial.print(address);
  Serial.print(" => ");
  Serial.println(value, 1);
}

void setup() {
  Serial.begin(115200);

  // Initialize all configured stations
  for (int i = 0; i < NUM_STATIONS; i++) {
    pinMode(STATIONS[i].ledPin, OUTPUT);
    digitalWrite(STATIONS[i].ledPin, LOW);
    pinMode(STATIONS[i].btnPin, INPUT_PULLUP);
  }

  Serial.println("==========================================");
  Serial.println(" THE PIT: ESP32 SCALABLE OSC CONTROLLER   ");
  Serial.println("==========================================");

  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }

  Serial.println("\n[Wi-Fi Connected!]");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // Resolve Host IP for Wokwi or direct network
  if (!WiFi.hostByName("host.wokwi.internal", outIp) || outIp == IPAddress(0,0,0,0)) {
    outIp = WiFi.gatewayIP();
  }
  Serial.print("Target UE5 Host IP: ");
  Serial.println(outIp);

  Udp.begin(inPort);
  Serial.print("Listening for State on UDP Port: ");
  Serial.println(inPort);
}

void loop() {
  unsigned long now = millis();

  // -------------------------------------------------------------
  // 1. Process Incoming OSC State Messages from Unreal Engine
  // -------------------------------------------------------------
  int packetSize = Udp.parsePacket();
  if (packetSize > 0) {
    OSCMessage inMsg;
    while (packetSize--) {
      inMsg.fill(Udp.read());
    }
    if (!inMsg.hasError()) {
      if (inMsg.match("/pit/attract")) {
        currentState = STATE_ATTRACT;
        Serial.println("<<< State: ATTRACT MODE");
      } else if (inMsg.match("/pit/crescendo")) {
        currentState = STATE_CRESCENDO;
        Serial.println("<<< State: CRESCENDO");
      } else if (inMsg.match("/pit/gameplay")) {
        currentState = STATE_ACTIVE;
        Serial.println("<<< State: ACTIVE GAMEPLAY");
      }
    }
  }

  // -------------------------------------------------------------
  // 2. Scan and Debounce All Stations
  // -------------------------------------------------------------
  bool anyHeld = false;
  for (int i = 0; i < NUM_STATIONS; i++) {
    int reading = digitalRead(STATIONS[i].btnPin);

    if (reading != STATIONS[i].lastState) {
      if ((now - STATIONS[i].lastDebounce) > DEBOUNCE_DELAY_MS) {
        STATIONS[i].lastDebounce = now;
        STATIONS[i].lastState = reading;

        if (reading == LOW) {
          sendOSC(STATIONS[i].oscPath, 1.0f);
          currentState = STATE_ACTIVE;
        } else {
          sendOSC(STATIONS[i].oscPath, 0.0f);
        }
      }
    }

    if (reading == LOW) {
      anyHeld = true;
    }

    // -----------------------------------------------------------
    // 3. Local Native LED Animations (Zero Network Overhead)
    // -----------------------------------------------------------
    if (reading == LOW) {
      // Station currently pressed: solid bright
      digitalWrite(STATIONS[i].ledPin, HIGH);
    } else if (currentState == STATE_ATTRACT) {
      // Attract mode: gentle rhythmic breathing pattern
      int brightness = (int)((sin(now * 0.003f + (i * 0.8f)) + 1.0f) * 127.0f);
      analogWrite(STATIONS[i].ledPin, brightness);
    } else if (currentState == STATE_CRESCENDO) {
      // Crescendo mode: rapid synchronized pulse
      digitalWrite(STATIONS[i].ledPin, (now % 100 < 50) ? HIGH : LOW);
    } else {
      // Idle in active mode: low baseline glow
      analogWrite(STATIONS[i].ledPin, 30);
    }
  }
}
