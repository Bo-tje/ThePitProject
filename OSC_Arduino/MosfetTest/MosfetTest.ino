// ============================================================================
// THE PIT PROJECT: Optocoupled MOSFET & 12V LED Test (ESP32 Core v3+)
// ============================================================================
// Target Pin: GPIO 32 (D32)
//
// Wiring Checklist:
//   ESP32 GPIO 32  ───► MOSFET Board 'S' / Signal
//   ESP32 VIN (5V) ───► MOSFET Board '+' / VCC (Powers Optocoupler input)
//   ESP32 GND      ───► MOSFET Board '-' / GND (Common Ground!)
//   12V PSU (+)    ───► Microswitch LED (+) / Anode
//   LED (-)        ───► MOSFET Board 'LOAD-' / Drain
//   12V PSU GND    ───► MOSFET Board 'VIN-' / Common Ground
// ============================================================================

#include <Arduino.h>

const int MOSFET_PIN     = 32;   // GPIO 32 (D32)
const int PWM_FREQ       = 1000; // 1000 Hz (Optimal for PC817 Optocouplers)
const int PWM_RESOLUTION = 8;    // 8-bit resolution (0 - 255)

// Gamma 2.4 curve with optocoupler threshold offset
// Maps linear 0.0-1.0 to duty cycles that cleanly saturate the optocoupler
uint8_t getOptoDuty(float brightness) {
  if (brightness <= 0.005f) return 0;
  
  // Power 2.4 curve
  float gammaVal = powf(brightness, 2.4f);
  
  // Optocoupler offset: start at duty 12 (so it starts conducting immediately)
  uint8_t duty = 12 + (uint8_t)(gammaVal * (255 - 12));
  return duty;
}

enum TestMode {
  MODE_BREATHE,
  MODE_MANUAL,
  MODE_STROBE
};

TestMode currentMode   = MODE_BREATHE;
float manualBrightness = 0.0f;
unsigned long lastFlash = 0;
bool flashState        = false;

void setPwmPin(uint8_t duty) {
  // ESP32 Core v3 native pin write (passes PIN, not channel)
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcWrite(MOSFET_PIN, duty);
#else
  // Native analogWrite works on modern ESP32 cores and handles pin routing automatically
  analogWrite(MOSFET_PIN, duty);
#endif
}

void printHelp() {
  Serial.println("\n--- Commands ---");
  Serial.println(" [b] : Breathing mode (smooth fade in/out)");
  Serial.println(" [0] : OFF (0%)");
  Serial.println(" [1] : FULL ON (100%)");
  Serial.println(" [2-9] : Set 20% to 90%");
  Serial.println(" [+] : Brightness +10%");
  Serial.println(" [-] : Brightness -10%");
  Serial.println(" [f] : Rapid strobe/flash test");
  Serial.println("----------------\n");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n==============================================");
  Serial.println(" THE PIT: OPTO-MOSFET LED TEST (GPIO 32)      ");
  Serial.println(" Optimized for Core v3 + 1000 Hz Optocoupler ");
  Serial.println("==============================================");

  // Initialize pin & PWM
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, LOW);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcAttach(MOSFET_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcWrite(MOSFET_PIN, 0);
#else
  analogWriteFrequency(MOSFET_PIN, PWM_FREQ);
  analogWriteResolution(PWM_RESOLUTION);
  analogWrite(MOSFET_PIN, 0);
#endif

  Serial.println("PWM initialized on GPIO 32 at 1000 Hz.");
  Serial.println("Starting in Breathing Mode...");
  printHelp();
}

void loop() {
  unsigned long now = millis();

  // --- Process Serial Inputs ---
  if (Serial.available()) {
    char c = Serial.read();

    if (c == 'b' || c == 'B') {
      currentMode = MODE_BREATHE;
      Serial.println(">>> Mode: BREATHING (Auto Fade In/Out)");
    } else if (c == '0') {
      currentMode = MODE_MANUAL;
      manualBrightness = 0.0f;
      setPwmPin(0);
      Serial.println(">>> Manual: 0% (OFF)");
    } else if (c == '1') {
      currentMode = MODE_MANUAL;
      manualBrightness = 1.0f;
      setPwmPin(255);
      Serial.println(">>> Manual: 100% (FULL ON)");
    } else if (c >= '2' && c <= '9') {
      currentMode = MODE_MANUAL;
      manualBrightness = (c - '0') / 10.0f;
      uint8_t d = getOptoDuty(manualBrightness);
      setPwmPin(d);
      Serial.print(">>> Manual: ");
      Serial.print((int)(manualBrightness * 100));
      Serial.print("% (Duty: ");
      Serial.print(d);
      Serial.println(")");
    } else if (c == '+' || c == '=') {
      currentMode = MODE_MANUAL;
      manualBrightness = min(1.0f, manualBrightness + 0.10f);
      uint8_t d = getOptoDuty(manualBrightness);
      setPwmPin(d);
      Serial.print(">>> Manual: ");
      Serial.print((int)(manualBrightness * 100));
      Serial.println("%");
    } else if (c == '-' || c == '_') {
      currentMode = MODE_MANUAL;
      manualBrightness = max(0.0f, manualBrightness - 0.10f);
      uint8_t d = getOptoDuty(manualBrightness);
      setPwmPin(d);
      Serial.print(">>> Manual: ");
      Serial.print((int)(manualBrightness * 100));
      Serial.println("%");
    } else if (c == 'f' || c == 'F') {
      currentMode = MODE_STROBE;
      Serial.println(">>> Mode: RAPID FLASH / STROBE TEST");
    } else if (c == 'h' || c == 'H' || c == '?') {
      printHelp();
    }
  }

  // --- Run Active Test Mode ---
  switch (currentMode) {
    case MODE_BREATHE: {
      // 2.5s sine breathing wave
      float wave = (sinf((float)now * 0.0025f) + 1.0f) * 0.5f;
      setPwmPin(getOptoDuty(wave));
      delay(12);
      break;
    }

    case MODE_STROBE: {
      if (now - lastFlash >= 100) {
        lastFlash = now;
        flashState = !flashState;
        setPwmPin(flashState ? 255 : 0);
      }
      delay(5);
      break;
    }

    case MODE_MANUAL:
    default:
      delay(20);
      break;
  }
}
