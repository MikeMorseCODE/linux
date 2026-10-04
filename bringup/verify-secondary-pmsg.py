#!/usr/bin/env python3
"""Validate the exporter-visible secondary bring-up commit in pmsg."""

import argparse
import struct
import sys


SESSION_MAGIC = 0x41535453
RECORD_BYTES = 32
CHECKPOINTS = {0: "custom boot, no secondary checkpoint",
               0x5544: "immediately before __enable_mmu",
               0x5545: "immediately after __enable_mmu",
               0x5546: "first operation in __secondary_switched",
               0x5547: "VBAR_EL1 installed and ISB completed",
               0x5548: "secondary_data.task loaded",
               0x5549: "stack-pointer and stack frame installed",
               0x554a: "immediately before secondary_start_kernel"}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("record")
    args = parser.parse_args()
    data = open(args.record, "rb").read()

    candidates = []
    for off in range(0, max(0, len(data) - RECORD_BYTES + 1)):
        if off + RECORD_BYTES > len(data):
            break
        session, session_complement, generation, generation_complement, \
            stage, stage_complement, sequence, sequence_complement = \
            struct.unpack_from("<IIIIIIII", data, off)
        if (session == SESSION_MAGIC and
            session_complement == ((~session) & 0xffffffff) and
                generation_complement == ((~generation) & 0xffffffff) and
                sequence_complement == ((~sequence) & 0xffffffff)):
            candidates.append((off, generation, stage, stage_complement))

    print(f"record_bytes={len(data)} session_candidates={len(candidates)}")
    valid = [(off, generation, stage, stage_complement)
             for off, generation, stage, stage_complement
             in candidates
             if stage_complement == ((~stage) & 0xffffffff) and
             stage in CHECKPOINTS]
    if not valid:
        print("UNRECOVERABLE: invalid session or stage commit")
        return 2

    highest_generation = max(generation for _, generation, _, _ in valid)
    survivors = [(off, generation, stage) for off, generation, stage, _
                 in valid if generation == highest_generation]
    stages = {stage for _, _, stage in survivors}
    if len(stages) != 1:
        print("survivors=" + ", ".join(
            f"0x{off:x}:0x{stage:08x}" for off, _, stage in survivors))
        print("UNRECOVERABLE: disagreement between surviving copies")
        return 2

    off, generation, stage = survivors[0]
    session, session_complement = struct.unpack_from("<II", data, off)
    print(f"offset=0x{off:x} session=0x{session:08x} "
          f"session_complement=0x{session_complement:08x} "
          f"generation=0x{generation:08x} stage=0x{stage:08x} "
          f"surviving_copies={len(survivors)}")
    print(f"VALID: {CHECKPOINTS[stage]} (0x{stage:04x})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
