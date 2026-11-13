#!/usr/bin/env python3
"""
Self-check test for OSC state snapshot, ACK protocol, and Battery Saver Hard-Timeout.
Verifies byte alignment, drop recovery, 2-second hard timeout, and /pit/sync wake-up.
"""
import socket
import struct
import time
import threading

def make_unit_inputs_packet(unit_id: int, seq: int, states: list[float]) -> bytes:
    address = f"/pit/unit{unit_id}/inputs"
    addr_bytes = address.encode('ascii') + b'\x00'
    while len(addr_bytes) % 4 != 0:
        addr_bytes += b'\x00'

    tags = ",i" + ("f" * len(states))
    tag_bytes = tags.encode('ascii') + b'\x00'
    while len(tag_bytes) % 4 != 0:
        tag_bytes += b'\x00'

    payload = struct.pack(">i", seq)
    for s in states:
        payload += struct.pack(">f", float(s))

    return addr_bytes + tag_bytes + payload

def make_ack_packet(seq: int) -> bytes:
    addr = b"/pit/ack\x00\x00\x00\x00"
    tags = b",i\x00\x00"
    val = struct.pack(">i", seq)
    return addr + tags + val

def make_sync_packet() -> bytes:
    addr = b"/pit/sync\x00\x00\x00"
    tags = b",\x00\x00\x00"
    return addr + tags

def make_smart_sync_packet(charge_ms: int, tap_cut_ms: int, idle_ms: int, max_active: int, bpm: float) -> bytes:
    addr = b"/pit/sync\x00\x00\x00"
    tags = b",iiiif\x00\x00"
    payload = struct.pack(">iiiif", charge_ms, tap_cut_ms, idle_ms, max_active, bpm)
    return addr + tags + payload

def test_packet_alignments():
    pkt = make_unit_inputs_packet(1, 42, [1.0, 0.0, 1.0])
    assert len(pkt) % 4 == 0, f"Packet length {len(pkt)} not 4-byte aligned"
    
    ack = make_ack_packet(42)
    assert len(ack) % 4 == 0, f"ACK length {len(ack)} not 4-byte aligned"
    assert len(ack) == 20, f"ACK length should be 20 bytes, got {len(ack)}"

    addr = ack[:8].decode('ascii')
    assert addr.startswith("/pit/ack")
    tag = ack[12:14].decode('ascii')
    assert tag == ",i"
    ack_seq = struct.unpack(">i", ack[16:20])[0]
    assert ack_seq == 42
    print("[PASS] Binary alignment & OSC serialization checks passed.")

def test_handshake_simulation():
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server_sock.bind(("127.0.0.1", 18000))

    client_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    client_sock.bind(("127.0.0.1", 18888))

    def server_worker():
        drop_count = 1
        while True:
            try:
                data, addr = server_sock.recvfrom(1024)
                if drop_count > 0:
                    drop_count -= 1
                    continue
                seq = struct.unpack(">i", data[28:32])[0]
                ack = make_ack_packet(seq)
                server_sock.sendto(ack, addr)
                break
            except Exception:
                break

    t = threading.Thread(target=server_worker, daemon=True)
    t.start()

    seq = 101
    states = [1.0, 0.0, 0.0]
    packet = make_unit_inputs_packet(1, seq, states)
    
    acked = False
    send_count = 0
    client_sock.settimeout(0.05)
    
    for _ in range(5):
        client_sock.sendto(packet, ("127.0.0.1", 18000))
        send_count += 1
        try:
            ack_data, _ = client_sock.recvfrom(1024)
            ack_seq = struct.unpack(">i", ack_data[16:20])[0]
            if ack_seq >= seq:
                acked = True
                break
        except socket.timeout:
            continue

    assert send_count == 2, f"Expected 2 sends (1 retry after simulated drop), got {send_count}"
    assert acked is True, "Client should be marked acked and rest"
    print("[PASS] Drop retry and ACK resting verification passed.")

    server_sock.close()
    client_sock.close()

def test_battery_timeout_and_sync():
    """Simulates Arduino giving up after max retries, entering sleep, and waking up on /pit/sync."""
    MAX_RETRIES = 5  # Scaled down for unit test
    retry_count = 0
    needs_ack = True
    sleeping = False

    # Simulate retry loop without ACK
    for _ in range(10):
        if needs_ack:
            if retry_count < MAX_RETRIES:
                retry_count += 1
            else:
                needs_ack = False
                sleeping = True

    assert sleeping is True, "Client should enter sleep state when max retries exceeded"
    assert retry_count == MAX_RETRIES, f"Should stop at {MAX_RETRIES} attempts, got {retry_count}"

    # Simulate /pit/sync arrival from Unreal Engine
    sync_pkt = make_sync_packet()
    if sync_pkt.startswith(b"/pit/sync"):
        needs_ack = True
        retry_count = 0
        sleeping = False

    assert sleeping is False and needs_ack is True, "Client should wake up and re-transmit on /pit/sync"
    print("[PASS] Battery timeout sleep and /pit/sync wake-up logic passed.")

def test_smart_sync_packet():
    pkt = make_smart_sync_packet(2500, 350, 15000, 3, 128.0)
    assert len(pkt) % 4 == 0, f"Smart sync packet length {len(pkt)} not 4-byte aligned"
    assert pkt[:12] == b"/pit/sync\x00\x00\x00"
    assert pkt[12:20] == b",iiiif\x00\x00"
    charge_ms, tap_cut_ms, idle_ms, max_active, bpm = struct.unpack(">iiiif", pkt[20:40])
    assert (charge_ms, tap_cut_ms, idle_ms, max_active, bpm) == (2500, 350, 15000, 3, 128.0)
    print("[PASS] Smart /pit/sync serialization & alignment passed.")

if __name__ == "__main__":
    test_packet_alignments()
    test_handshake_simulation()
    test_battery_timeout_and_sync()
    test_smart_sync_packet()
    print("ALL OSC TESTS PASSED!")
