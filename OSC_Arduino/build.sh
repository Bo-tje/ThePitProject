#!/usr/bin/env bash
set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
export PATH="$HOME/.local/bin:$PATH"

echo "Compiling MidiTest.ino for ESP32 OSC Controller..."
arduino-cli compile --fqbn esp32:esp32:esp32 --output-dir "$DIR/build" "$DIR/MidiTest.ino"
echo "Build complete! Firmware updated in build/MidiTest.ino.merged.bin"
