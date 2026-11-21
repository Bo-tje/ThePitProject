#!/usr/bin/env python3
"""
Harmonic Convergence - 24-Button Interactive OSC Sound & Convergence Tester
Sends OSC messages to Unreal Engine 5 on Port 8000.

Usage:
  python3 test_convergence.py              # Interactive Keyboard Mode (Play live)
  python3 test_convergence.py dorian       # Lock in Bass & Beat stations (steers 4x4 Scale Matrix toward Dorian)
  python3 test_convergence.py aeolian      # Lock in Warm Pad stations (steers 4x4 Scale Matrix toward Aeolian)
  python3 test_convergence.py lydian       # Lock in Chime & Sweep stations (steers 4x4 Scale Matrix toward Lydian)
  python3 test_convergence.py ionian       # Lock in Full Mix across sections (steers 4x4 Scale Matrix toward Ionian)
  python3 test_convergence.py shockwave    # Trigger Simultaneous Co-op Ignition Shockwave (<0.6s lock-in)
  python3 test_convergence.py clear        # Tap-to-Cut all 24 crystals (reset active loops)
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
CHARGE_LOCK_DURATION_SECONDS = 2.15
TAP_CUT_DURATION_SECONDS = 0.12

# Modal Exploration Presets (locks in specific timbre groups to steer the 4x4 Scale Mode Weight Matrix & lighting)
MODAL_EXPLORATION_PRESETS = {
    "dorian": {
        "title": "Dorian Cyber-Groove Bias (Bass & Beats: Units 1 & 2)",
        "stations": [1, 3, 5, 6],
    },
    "aeolian": {
        "title": "Aeolian Nocturnal Bias (Warm Pads: Unit 4)",
        "stations": [11, 13, 14, 15],
    },
    "lydian": {
        "title": "Lydian Celestial Bias (Chimes & Vortex Sweeps: Units 3, 5 & 6)",
        "stations": [9, 10, 18, 21],
    },
    "ionian": {
        "title": "Ionian Euphoric Full-Mix Bias (Bass + Pads + Chimes Collaboration)",
        "stations": [1, 5, 9, 13, 18, 21],
    },
    "shockwave": {
        "title": "Simultaneous Co-op Ignition Shockwave (2 players lock in within <0.6s)",
        "stations": [1, 13],
    },
}


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


def tap_cut_all_crystals():
    """Sends a quick 120ms tap to all 24 stations to deactivate any currently locked-in crystals."""
    print("\n--- Tap-to-Cut: Clearing All Active Crystals ---")
    for station_id in range(1, TOTAL_BUTTONS + 1):
        send_station_osc_event(f"player{station_id}", STATION_BUTTON_PRESSED)
    time.sleep(TAP_CUT_DURATION_SECONDS)
    for station_id in range(1, TOTAL_BUTTONS + 1):
        send_station_osc_event(f"player{station_id}", STATION_BUTTON_RELEASED)
    time.sleep(0.15)
    print("  🧹 All active crystals cut.\n")


def trigger_modal_preset(preset_key: str, clear_existing: bool = True):
    """Charges and locks in a timbre group to steer the 4x4 Scale Mode Weight Matrix & lighting."""
    preset = MODAL_EXPLORATION_PRESETS[preset_key]
    if clear_existing:
        tap_cut_all_crystals()

    print(f"\n--- Locking In: {preset['title']} ---")
    print(f"  ⚡ Holding stations {preset['stations']} for {CHARGE_LOCK_DURATION_SECONDS:.2f}s to ignite crystals...")
    for station_id in preset["stations"]:
        send_station_osc_event(f"player{station_id}", STATION_BUTTON_PRESSED)

    time.sleep(CHARGE_LOCK_DURATION_SECONDS)

    for station_id in preset["stations"]:
        send_station_osc_event(f"player{station_id}", STATION_BUTTON_RELEASED)
    print("  🌟 Crystals locked in!\n")


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
    """Provides a live keyboard interface to trigger and toggle individual stations, crowds, or modal timbre groups."""
    print("\n" + "=" * 75)
    print("  🎹 24-BUTTON INTERACTIVE HARMONIC CONVERGENCE KEYBOARD")
    print("  Keys 1-9, 0: Toggle buttons 1-10   | Keys a-n: Toggle buttons 11-24")
    print("  --- 4x4 SCALE MATRIX & CO-OP TRIGGERS (Auto-charges 2.15s & locks in) ---")
    print("  'D' (Shift+D) = Dorian Bias (Bass & Beats: Units 1 & 2)")
    print("  'A' (Shift+A) = Aeolian Bias (Warm Pads: Unit 4)")
    print("  'L' (Shift+L) = Lydian Bias (Chimes & Sweeps: Units 3, 5 & 6)")
    print("  'I' (Shift+I) = Ionian Bias (Full Collaboration Mix across all timbres)")
    print("  'S' (Shift+S) = Co-op Simultaneous Ignition Shockwave")
    print("  'z' = Tap-to-Cut All Active Crystals | 'r' = Random Crowd")
    print("  'x' = Release Held Buttons           | 'q' = Quit")
    print("=" * 75)

    keyboard_to_station_map = {
        '1': 'player1',  '2': 'player2',  '3': 'player3',  '4': 'player4',  '5': 'player5',
        '6': 'player6',  '7': 'player7',  '8': 'player8',  '9': 'player9',  '0': 'player10',
        'a': 'player11', 'b': 'player12', 'c': 'player13', 'd': 'player14', 'e': 'player15',
        'f': 'player16', 'g': 'player17', 'h': 'player18', 'i': 'player19', 'j': 'player20',
        'k': 'player21', 'l': 'player22', 'm': 'player23', 'n': 'player24',
    }

    shortcut_to_preset_map = {
        'D': 'dorian',
        'A': 'aeolian',
        'L': 'lydian',
        'I': 'ionian',
        'S': 'shockwave',
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
            elif pressed_character in shortcut_to_preset_map:
                for station_num in range(1, TOTAL_BUTTONS + 1):
                    station_activation_states[f"player{station_num}"] = False
                trigger_modal_preset(shortcut_to_preset_map[pressed_character], clear_existing=True)
            elif pressed_character == 'z':
                for station_num in range(1, TOTAL_BUTTONS + 1):
                    station_activation_states[f"player{station_num}"] = False
                tap_cut_all_crystals()
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
    if selected_mode in MODAL_EXPLORATION_PRESETS:
        trigger_modal_preset(selected_mode, clear_existing=True)
    elif selected_mode in ("clear", "cut"):
        tap_cut_all_crystals()
    elif selected_mode in ("chord", "cascade"):
        test_24_button_cascade()
    elif selected_mode in ("bass", "subbass"):
        test_sub_bass_anchors()
    elif selected_mode in ("chimes", "chime"):
        test_crystalline_chimes()
    elif selected_mode in ("random", "crowd"):
        test_random_crowd_simulation()
    else:
        run_interactive_keyboard_controller()
