#!/usr/bin/env python3
"""
The Pit Project: Interactive OSC Test Sender
Sends Open Sound Control (OSC) UDP packets to Arduino Nano 33 IoT (Port 8888) or Unreal Engine 5 (Port 8000)

Usage:
  Interactive Menu (Double-click or run directly):
    py send_test_osc.py

  Command-Line Mode:
    py send_test_osc.py /pit/attract 1.0
    py send_test_osc.py /pit/crescendo 1.0
    py send_test_osc.py /pit/gameplay 1.0
    py send_test_osc.py /player1 1.0 8000 127.0.0.1
"""
import sys
import socket
import struct
import time

# Arduino Nano 33 IoT default target
ARDUINO_IP = "192.168.0.101"
ARDUINO_PORT = 8888

# Unreal Engine 5 default target
UE5_IP = "127.0.0.1"
UE5_PORT = 8000


def make_osc_message(address: str, value: float) -> bytes:
    """Encodes a binary OSC message with 4-byte aligned address, type tag ',f', and big-endian float."""
    if not address.startswith('/'):
        address = '/' + address.replace('.', '/')

    addr_bytes = address.encode('ascii') + b'\x00'
    while len(addr_bytes) % 4 != 0:
        addr_bytes += b'\x00'

    type_tags = b',f\x00\x00'
    payload = struct.pack('>f', float(value))
    return addr_bytes + type_tags + payload


def send_osc(address: str, value: float = 1.0, host: str = ARDUINO_IP, port: int = ARDUINO_PORT):
    data = make_osc_message(address, value)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(data, (host, port))
        print(f">>> [OSC Sent] {address} => {value:.1f} -> {host}:{port}")
    except Exception as e:
        print(f"[!] Send error: {e}")
    finally:
        sock.close()


def interactive_menu():
    print("=" * 60)
    print(f" THE PIT: ARDUINO OSC SENDER")
    print(f" Target Arduino: {ARDUINO_IP}:{ARDUINO_PORT}")
    print("=" * 60)
    print("Commands:")
    print("  [1] Send ATTRACT Mode     (/pit/attract 1.0)   -> Gentle breathing LEDs")
    print("  [2] Send CRESCENDO Mode   (/pit/crescendo 1.0) -> Rapid sync flashing LEDs")
    print("  [3] Send GAMEPLAY Mode    (/pit/gameplay 1.0)  -> Active baseline glow")
    print("  [4] Send UE5 /player1 ON  (to localhost:8000)")
    print("  [5] Send UE5 /player1 OFF (to localhost:8000)")
    print("  [C] Change Target IP")
    print("  [Q] Quit")
    print("=" * 60)

    current_host = ARDUINO_IP

    while True:
        try:
            choice = input(f"\nSelect command [1-5, C, Q] (Target: {current_host}): ").strip().upper()
        except (KeyboardInterrupt, EOFError):
            break

        if choice == '1':
            send_osc("/pit/attract", 1.0, host=current_host, port=ARDUINO_PORT)
        elif choice == '2':
            send_osc("/pit/crescendo", 1.0, host=current_host, port=ARDUINO_PORT)
        elif choice == '3':
            send_osc("/pit/gameplay", 1.0, host=current_host, port=ARDUINO_PORT)
        elif choice == '4':
            send_osc("/player1", 1.0, host=UE5_IP, port=UE5_PORT)
        elif choice == '5':
            send_osc("/player1", 0.0, host=UE5_IP, port=UE5_PORT)
        elif choice == 'C':
            new_ip = input("Enter new Target IP (e.g. 192.168.0.101): ").strip()
            if new_ip:
                current_host = new_ip
                print(f"[OK] Target IP set to: {current_host}")
        elif choice == 'Q':
            print("Exiting.")
            break
        else:
            print("[?] Unknown command. Type 1, 2, 3, 4, 5, or Q.")


if __name__ == "__main__":
    if len(sys.argv) > 1:
        # CLI Mode
        channel_arg = sys.argv[1]
        value_arg = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
        port_arg = int(sys.argv[3]) if len(sys.argv) > 3 else ARDUINO_PORT
        host_arg = sys.argv[4] if len(sys.argv) > 4 else ARDUINO_IP
        send_osc(channel_arg, value_arg, host=host_arg, port=port_arg)
    else:
        # Interactive Menu
        interactive_menu()
