#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
export PATH="$HOME/.local/bin:$PATH"
SKETCH="$DIR/Nano33IoT_OSC/Nano33IoT_OSC.ino"
FQBN="arduino:samd:nano_33_iot"

echo "=================================================="
echo " Building Sketch for Arduino Nano 33 IoT..."
echo "=================================================="

arduino-cli compile --fqbn "$FQBN" "$SKETCH"

echo "Build Succeeded!"
echo ""

# Find available serial port
PORT=$(arduino-cli board list | grep "nano_33_iot" | awk '{print $1}' || true)
if [ -z "$PORT" ]; then
    PORT=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n 1 || true)
fi

if [ -z "$PORT" ]; then
    echo "[!] No port found. Please double-tap the reset button on your Nano 33 IoT so the LED pulses, then re-run."
    exit 1
fi

echo "Uploading to $PORT..."
arduino-cli upload -p "$PORT" --fqbn "$FQBN" -v "$SKETCH"
echo ""
echo "=================================================="
echo " Upload Succeeded!"
echo " Open Serial Monitor with:"
echo " arduino-cli monitor -p $PORT -c baudrate=115200"
echo "=================================================="
