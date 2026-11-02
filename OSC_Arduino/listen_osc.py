#!/usr/bin/env python3
"""
The Pit Project: OSC Listener / Debugger & Reliable ACK Mock
Listens for Open Sound Control (OSC) UDP packets on port 8000 (UE5 incoming) or port 8888 (MCU incoming).
Automatically sends /pit/ack back to MCU controllers when receiving state snapshots.

Usage:
  py listen_osc.py            # Listens on port 8000 with Auto-ACK enabled
  py listen_osc.py 8888       # Listens on port 8888 (MCU monitor)
  py listen_osc.py --no-ack   # Listens without sending ACKs (tests MCU retry backoff)
"""
import sys
import socket
import struct
import time

AUTO_ACK = "--no-ack" not in sys.argv
PORT = 8000
for arg in sys.argv[1:]:
    if arg.isdigit():
        PORT = int(arg)
        break

HOST = "0.0.0.0"
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind((HOST, PORT))

print("==================================================")
print(f" THE PIT: OSC LISTENER ACTIVE ON PORT {PORT}")
print(f" Auto-ACK Mode: {'ENABLED (Replying /pit/ack on port 8888)' if AUTO_ACK and PORT == 8000 else 'DISABLED'}")
print(" Ready to receive packets...")
print("==================================================")


def make_osc_ack(sequence_id: int) -> bytes:
    """Creates a 4-byte aligned OSC message: /pit/ack with single int32 argument."""
    # Address: 8 chars -> padded to 12 bytes
    addr = b"/pit/ack\x00\x00\x00\x00"
    # Type tag: 2 chars -> padded to 4 bytes
    tags = b",i\x00\x00"
    # Argument: 4 bytes big-endian int32
    val = struct.pack(">i", sequence_id)
    return addr + tags + val


try:
    while True:
        data, addr = sock.recvfrom(2048)
        null_idx = data.find(b"\x00")
        if null_idx == -1:
            print(f">>> [RAW UDP from {addr[0]}:{addr[1]}] {data}")
            continue

        address = data[:null_idx].decode("ascii", errors="ignore")
        addr_padded_len = (null_idx + 4) & ~0x03

        # Parse type tags
        tags = ""
        payload_offset = addr_padded_len
        if addr_padded_len < len(data) and data[addr_padded_len:addr_padded_len+1] == b",":
            tag_null = data.find(b"\x00", addr_padded_len)
            if tag_null != -1:
                tags = data[addr_padded_len:tag_null].decode("ascii", errors="ignore")
                payload_offset = (tag_null + 4) & ~0x03

        # Special snapshot parsing for /pit/unit*/inputs
        if address.startswith("/pit/unit") and "i" in tags:
            try:
                seq = struct.unpack(">i", data[payload_offset:payload_offset+4])[0]
                states = []
                float_offset = payload_offset + 4
                for _ in range(tags.count("f")):
                    if float_offset + 4 <= len(data):
                        val = struct.unpack(">f", data[float_offset:float_offset+4])[0]
                        states.append(1 if val > 0.5 else 0)
                        float_offset += 4

                print(f">>> [OSC Snapshot from {addr[0]}:{addr[1]}] {address} seq={seq} states={states}")

                if AUTO_ACK and PORT == 8000:
                    ack_packet = make_osc_ack(seq)
                    sock.sendto(ack_packet, (addr[0], 8888))
                    print(f"    <<< [ACK Sent] /pit/ack {seq} -> {addr[0]}:8888")
                continue
            except Exception as e:
                print(f"[!] Parse error on snapshot: {e}")

        # Legacy / Single float fallback parsing
        val_str = ""
        if len(data) >= 8:
            try:
                val = struct.unpack(">f", data[-4:])[0]
                state = "PRESSED" if val > 0.5 else "RELEASED"
                val_str = f" => {val:.1f} ({state})"
            except Exception:
                pass
        print(f">>> [OSC from {addr[0]}:{addr[1]}] {address}{val_str} ({len(data)} bytes)")

except KeyboardInterrupt:
    print("\nListener stopped.")
finally:
    sock.close()
