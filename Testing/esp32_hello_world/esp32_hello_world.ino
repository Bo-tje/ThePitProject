/*
  ESP32 Hello World Test Sketch
  Compatible with standard Arduino IDE
*/

#define LED_PIN 2  // GPIO 2 is the onboard blue LED on most ESP32 DevKit / WROOM-32 boards

unsigned long counter = 0;

void setup() {
  // Initialize serial communication at 115200 baud
  Serial.begin(115200);
  
  // Wait briefly for serial connection to stabilize
  delay(1000);

  // Configure onboard LED pin as output
  pinMode(LED_PIN, OUTPUT);

  Serial.println();
  Serial.println("======================================");
  Serial.println("   Hello World from ESP32!           ");
  Serial.println("======================================");
  Serial.printf("Chip Model    : %s\n", ESP.getChipModel());
  Serial.printf("Chip Revision : %d\n", ESP.getChipRevision());
  Serial.printf("CPU Frequency : %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash Size    : %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.println("--------------------------------------");
  Serial.println("Loop starting...\n");
}

void loop() {
  counter++;

  // Toggle onboard LED ON
  digitalWrite(LED_PIN, HIGH);
  Serial.printf("[%lu] Hello World! LED ON  (Uptime: %lu ms)\n", counter, millis());
  delay(1000);

  // Toggle onboard LED OFF
  digitalWrite(LED_PIN, LOW);
  Serial.printf("[%lu] Hello World! LED OFF (Uptime: %lu ms)\n", counter, millis());
  delay(1000);
}
