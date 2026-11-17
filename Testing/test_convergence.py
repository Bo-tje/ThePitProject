#!/usr/bin/env python3
"""
Harmonic Convergence - 24-Button Interactive OSC Sound & Convergence Tester
Sends OSC messages to Unreal Engine 5 on Port 8000.

Usage:
  python3 test_convergence.py              # Interactive Keyboard Mode (Play live)
  python3 test_convergence.py chord        # Test 24-button massive orchestral cascade
  python3 test_convergence.py bass         # Test Sub-Bass Anchor stations
  python3 test_convergence.py chimes       # Test Crystalline High Chime stations
  python3 test_convergence.py random       # Simulate 24-button festival crowd
"""

import sys
import time
import socket
import struct
import random

if sys.platform == "win32":
    import msvcrt
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
else:
    import select
    import termios
    import tty

OSC_TARGET_HOST = "127.0.0.1"
OSC_TARGET_PORT = 8000
TOTAL_BUTTONS = 24

STATION_BUTTON_PRESSED = 1.0
STATION_BUTTON_RELEASED = 0.0


def build_osc_float_packet(channel_address: str, float_value: float) -> bytes:
    """Encodes an address and a 32-bit big-endian float value into standard OSC binary format."""
    if not channel_address.startswith('/'):
        channel_address = '/' + channel_address.replace('.', '/')

    address_bytes = channel_address.encode('ascii') + b'\x00'
    while len(address_bytes) % 4 != 0:
        address_bytes += b'\x00'

    return address_bytes + b',f\x00\x00' + struct.pack('>f', float(float_value))


def send_station_osc_event(station_channel_name: str, signal_value: float):
    """Sends a UDP OSC event packet to Unreal Engine for a specific station channel."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_socket:
        udp_socket.sendto(build_osc_float_packet(station_channel_name, signal_value), (OSC_TARGET_HOST, OSC_TARGET_PORT))

    event_state_label = "PRESSED" if signal_value > 0.5 else "RELEASED"
    print(f"  🎵 [{station_channel_name:<9}] = {signal_value:4.1f} ({event_state_label})")


def test_24_button_cascade():
    """Sequentially activates all 24 buttons to test full orchestral harmonic convergence."""
    print(f"\n--- Testing {TOTAL_BUTTONS}-Button Ambient Symphony (Cascade) ---")
    for station_number in range(1, TOTAL_BUTTONS + 1):
        send_station_osc_event(f"player{station_number}", STATION_BUTTON_PRESSED)
        time.sleep(0.25)

    print(f"  ✨ All {TOTAL_BUTTONS} buttons active - full 5-octave symphony resonating in center...")
    time.sleep(3.5)

    for station_number in range(1, TOTAL_BUTTONS + 1):
        send_station_osc_event(f"player{station_number}", STATION_BUTTON_RELEASED)
        time.sleep(0.1)
    print("Done.\n")


def test_sub_bass_anchors():
    """Tests the low-end grounding sub-bass buttons (1, 5, 10, 16, 24)."""
    print("\n--- Testing Tier 1 (Sub-Bass Anchors: 1, 5, 10, 16, 24) ---")
    sub_bass_station_ids = [1, 5, 10, 16, 24]
    for station_id in sub_bass_station_ids:
        send_station_osc_event(f"player{station_id}", STATION_BUTTON_PRESSED)
        time.sleep(0.5)

    time.sleep(3.0)

    for station_id in sub_bass_station_ids:
        send_station_osc_event(f"player{station_id}", STATION_BUTTON_RELEASED)
    print("Done.\n")


def test_crystalline_chimes():
    """Tests the high sparkling chime buttons (4, 7, 9, 12, 15, 18, 21)."""
    print("\n--- Testing Tier 3 (Crystalline High Chimes: 4, 7, 9, 12, 15, 18, 21) ---")
    for station_id in [4, 7, 9, 12, 15, 18, 21]:
        station_channel_name = f"player{station_id}"
        send_station_osc_event(station_channel_name, STATION_BUTTON_PRESSED)
        time.sleep(0.3)
        send_station_osc_event(station_channel_name, STATION_BUTTON_RELEASED)
        time.sleep(0.2)
    print("Done.\n")


def test_random_crowd_simulation():
    """Simulates festival visitors pressing random buttons across all 24 channels over 15 seconds."""
    print(f"\n--- Simulating Festival-Goers Across {TOTAL_BUTTONS} Buttons (15 Seconds) ---")
    active_station_ids = set()
    simulation_start_time = time.time()

    while time.time() - simulation_start_time < 15.0:
        chosen_station_id = random.randint(1, TOTAL_BUTTONS)
        station_channel_name = f"player{chosen_station_id}"

        if chosen_station_id in active_station_ids:
            send_station_osc_event(station_channel_name, STATION_BUTTON_RELEASED)
            active_station_ids.remove(chosen_station_id)
        else:
            send_station_osc_event(station_channel_name, STATION_BUTTON_PRESSED)
            active_station_ids.add(chosen_station_id)

        time.sleep(random.uniform(0.3, 0.8))

    for active_id in list(active_station_ids):
        send_station_osc_event(f"player{active_id}", STATION_BUTTON_RELEASED)
    print("Festival simulation complete.\n")


def run_interactive_keyboard_controller():
    """Provides a live keyboard interface to trigger and toggle individual stations or crowds."""
    print("\n" + "=" * 65)
    print("  🎹 24-BUTTON INTERACTIVE HARMONIC CONVERGENCE KEYBOARD")
    print("  Keys 1-9, 0: Toggle buttons 1-10")
    print("  Keys a-n:    Toggle buttons 11-24")
    print("  'r' = Random Crowd | 'x' = All Off | 'q' = Quit")
    print("=" * 65)

    keyboard_to_station_map = {
        '1': 'player1',  '2': 'player2',  '3': 'player3',  '4': 'player4',  '5': 'player5',
        '6': 'player6',  '7': 'player7',  '8': 'player8',  '9': 'player9',  '0': 'player10',
        'a': 'player11', 'b': 'player12', 'c': 'player13', 'd': 'player14', 'e': 'player15',
        'f': 'player16', 'g': 'player17', 'h': 'player18', 'i': 'player19', 'j': 'player20',
        'k': 'player21', 'l': 'player22', 'm': 'player23', 'n': 'player24',
    }

    station_activation_states = {f"player{station_num}": False for station_num in range(1, TOTAL_BUTTONS + 1)}
    is_windows = sys.platform == "win32"

    if not is_windows:
        stdin_fileno = sys.stdin.fileno()
        previous_terminal_settings = termios.tcgetattr(stdin_fileno)

    try:
        if not is_windows:
            tty.setraw(stdin_fileno)

        while True:
            pressed_character = None
            if is_windows:
                if msvcrt.kbhit():
                    pressed_character = msvcrt.getch().decode('latin1', errors='ignore')
                else:
                    time.sleep(0.05)
            else:
                read_descriptors, _, _ = select.select([sys.stdin], [], [], 0.1)
                if read_descriptors:
                    pressed_character = sys.stdin.read(1)

            if not pressed_character:
                continue

            if pressed_character in ('q', '\x03'):
                break
            elif pressed_character in keyboard_to_station_map:
                target_channel = keyboard_to_station_map[pressed_character]
                station_activation_states[target_channel] = not station_activation_states[target_channel]
                new_signal = STATION_BUTTON_PRESSED if station_activation_states[target_channel] else STATION_BUTTON_RELEASED
                send_station_osc_event(target_channel, new_signal)
            elif pressed_character == 'x':
                for station_num in range(1, TOTAL_BUTTONS + 1):
                    channel_name = f"player{station_num}"
                    station_activation_states[channel_name] = False
                    send_station_osc_event(channel_name, STATION_BUTTON_RELEASED)
            elif pressed_character == 'r':
                if not is_windows:
                    termios.tcsetattr(stdin_fileno, termios.TCSADRAIN, previous_terminal_settings)
                test_random_crowd_simulation()
                if not is_windows:
                    tty.setraw(stdin_fileno)
    finally:
        if not is_windows:
            termios.tcsetattr(stdin_fileno, termios.TCSADRAIN, previous_terminal_settings)
        for station_num in range(1, TOTAL_BUTTONS + 1):
            send_station_osc_event(f"player{station_num}", STATION_BUTTON_RELEASED)
        print(f"\nAll {TOTAL_BUTTONS} button voices released. Goodbye!\n")


if __name__ == "__main__":
    selected_mode = sys.argv[1].lower() if len(sys.argv) > 1 else "interactive"
    if selected_mode in ("chord", "cascade"):
        test_24_button_cascade()
    elif selected_mode in ("bass", "subbass"):
        test_sub_bass_anchors()
    elif selected_mode in ("chimes", "chime"):
        test_crystalline_chimes()
    elif selected_mode in ("random", "crowd"):
        test_random_crowd_simulation()
    else:
        run_interactive_keyboard_controller()
