#!/usr/bin/env python3
"""Exercise the exact persistent_ram_save_old circular export operation."""

import struct

SIG = 0x43474244
MAGIC = 0x41535453
GENERATION = 1
SEQUENCE = 0x53544147
HEADER = 12
RECORD = 32
STAGES = (0, 0x5544, 0x5545, 0x5546)


def save_old(image, payload_size):
    sig, start, size = struct.unpack_from("<III", image)
    assert sig == SIG and start < payload_size and size <= payload_size
    payload = image[HEADER:HEADER + payload_size]
    return payload[start:start + size] + payload[:start]


def record(stage):
    words = (MAGIC, ~MAGIC & 0xffffffff, GENERATION,
             ~GENERATION & 0xffffffff, stage, ~stage & 0xffffffff,
             SEQUENCE, ~SEQUENCE & 0xffffffff)
    return struct.pack("<8I", *words)


def place(payload_size, start, size, blob):
    image = bytearray(HEADER + payload_size)
    struct.pack_into("<III", image, 0, SIG, start, size)
    payload = memoryview(image)[HEADER:]
    for i, value in enumerate(blob):
        payload[(start + i) % payload_size] = value
    return image


def check(stage, start, size, description):
    payload_size = 0x100
    exported = save_old(place(payload_size, start, size, record(stage)),
                        payload_size)
    assert exported[:RECORD] == record(stage), description
    print(f"PASS {description}: stage=0x{stage:04x} start=0x{start:x} size=0x{size:x}")


def main():
    for stage in STAGES:
        check(stage, 0, RECORD, "non-wrapped")
        check(stage, 0x40, 0x80, "wrapped logical start")
        check(stage, 0xf0, 0x80, "record split at payload end")
    print("PASS exact persistent_ram_save_old simulator")


if __name__ == "__main__":
    main()
