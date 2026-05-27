#!/usr/bin/env python3
"""
ArtNet test: cycle through each strip, lighting the first 5 LEDs.
Usage: python3 artnet_test.py [host] [--fps N] [--color R,G,B]
"""

import socket
import struct
import time
import argparse

ARTNET_PORT = 6454
STRIP_COUNT = 15
LEDS_PER_STRIP = 151
LEDS_ON = 5


def strip_to_universe(strip: int) -> int:
    return strip


def artdmx_packet(universe: int, rgb_data: bytes) -> bytes:
    length = len(rgb_data)
    # Length must be even
    if length % 2:
        rgb_data += b'\x00'
        length += 1
    return (
        b'Art-Net\x00'           # ID
        + struct.pack('<H', 0x5000)  # OpCode ArtDMX (LE)
        + struct.pack('>H', 14)      # ProtVer 14 (BE)
        + b'\x00'                    # Sequence (disabled)
        + b'\x00'                    # Physical
        + struct.pack('<H', universe) # Universe (LE)
        + struct.pack('>H', length)  # Length (BE)
        + rgb_data
    )


def blank_packet(universe: int) -> bytes:
    return artdmx_packet(universe, bytes(LEDS_PER_STRIP * 3))


def poll_node(sock: socket.socket, ip: str) -> None:
    """Send ArtPoll and print any ArtPollReply responses received within 2 seconds."""
    pkt = (
        b'Art-Net\x00'
        + struct.pack('<H', 0x2000)  # OpCode ArtPoll (LE)
        + struct.pack('>H', 14)      # ProtVer 14 (BE)
        + b'\x00'                    # TalkToMe
        + b'\x00'                    # Priority
    )
    sock.sendto(pkt, (ip, ARTNET_PORT))

    sock.settimeout(2.0)
    replies = []
    try:
        while True:
            data, addr = sock.recvfrom(1024)
            if len(data) >= 10 and data[:8] == b'Art-Net\x00':
                opcode = struct.unpack_from('<H', data, 8)[0]
                if opcode == 0x2100 and len(data) >= 212:
                    replies.append((addr, data))
    except socket.timeout:
        pass
    finally:
        sock.settimeout(None)

    if not replies:
        print("ArtPoll: no reply received")
        return

    print(f"ArtPoll: {len(replies)} reply/replies")
    for addr, data in replies:
        node_ip = '.'.join(str(data[10 + i]) for i in range(4))
        short_name = data[26:44].rstrip(b'\x00').decode('ascii', errors='replace')
        long_name  = data[44:108].rstrip(b'\x00').decode('ascii', errors='replace')
        report     = data[108:172].rstrip(b'\x00').decode('ascii', errors='replace')
        num_ports  = (data[172] << 8) | data[173]
        bind_index = data[211]
        sw_out     = list(data[190:190 + min(num_ports, 4)])
        print(f"  [{addr[0]}] bind={bind_index}  '{short_name}' / '{long_name}'")
        print(f"    node IP: {node_ip}  ports: {num_ports}  universes out: {sw_out}")
        print(f"    report: {report}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('host', nargs='?', default='piddle.local')
    parser.add_argument('--fps', type=float, default=10)
    parser.add_argument('--color', default='255,255,255',
                        help='R,G,B of lit LEDs (default: 255,255,255)')
    args = parser.parse_args()

    r, g, b = (int(x) for x in args.color.split(','))
    delay = 1.0 / args.fps

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    sock.bind(('', ARTNET_PORT))

    try:
        ip = socket.gethostbyname(args.host)
    except socket.gaierror:
        print(f"Could not resolve '{args.host}'. Try passing the IP directly.")
        return

    poll_node(sock, ip)
    print(f"Sending to {ip}:{ARTNET_PORT}  color=({r},{g},{b})  fps={args.fps}")
    print("Ctrl-C to stop")

    # Build per-strip DMX buffers: first LEDS_ON lit, rest black
    lit = bytes([r, g, b] * LEDS_ON)
    dark = bytes(3 * (LEDS_PER_STRIP - LEDS_ON))
    blank = [blank_packet(strip_to_universe(s)) for s in range(STRIP_COUNT)]

    active = 0
    try:
        while True:
            # Send blank to previously active strip
            prev = (active - 1) % STRIP_COUNT
            sock.sendto(blank[prev], (ip, ARTNET_PORT))

            # Build and send lit packet for current strip
            pkt = artdmx_packet(strip_to_universe(active), lit + dark)
            sock.sendto(pkt, (ip, ARTNET_PORT))

            u = strip_to_universe(active)
            print(f"Strip {active:2d} (universe 0x{u:04X})", end='\r')
            active = (active + 1) % STRIP_COUNT
            time.sleep(delay)
    except KeyboardInterrupt:
        print("\nClearing all strips...")
        for s in range(STRIP_COUNT):
            sock.sendto(blank[s], (ip, ARTNET_PORT))
    finally:
        sock.close()


if __name__ == '__main__':
    main()
