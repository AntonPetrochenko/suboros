#!/usr/bin/env python3
"""
mkprg.py — Build a SuborOS .PRG executable file.

Usage:
    python tools/mkprg.py <input.bin> <output.prg> [reloc_offset ...]

  input.bin   : raw 6502 binary assembled for $8000, WITHOUT a PRG header.
                The entry point must be at file offset 0 (i.e., $8000).
  output.prg  : output file placed directly into in_fs/<slot>/ for mkfs.py
  reloc_offset: zero or more decimal byte offsets within input.bin where a
                2-byte little-endian ROM address ($8000-$BFFF) exists that
                needs to be adjusted if the program is ever loaded into RAM.

The tool prepends the 8-byte PRG header so the final layout in the ROM bank is:
  $8000: PRG header (8 bytes)
  $8008: program code (= byte 0 of input.bin, entry point)

PRG header format (8 bytes, little-endian):
  [0-1]  magic    'P' 'R'
  [2]    version  1
  [3]    flags    bit 0 = reloc table present
  [4-5]  entry_offset  (= 8 + reloc_table_bytes, LE)
  [6-7]  reloc_table_size (bytes, LE)

Relocation table (if present, immediately after header):
  N/2 entries, each a 2-byte LE offset from $8000 into the code section
  (i.e., from the start of input.bin).
"""

import sys
import struct
import os

PRG_MAGIC   = b'PR'
PRG_VERSION = 1


def build_prg(bin_data: bytes, reloc_offsets: list[int]) -> bytes:
    reloc_table = b''
    for off in reloc_offsets:
        reloc_table += struct.pack('<H', off)

    reloc_size  = len(reloc_table)
    flags       = 0x01 if reloc_size > 0 else 0x00
    entry_off   = 8 + reloc_size   # code starts after header + reloc table

    header = (
        PRG_MAGIC +
        bytes([PRG_VERSION, flags]) +
        struct.pack('<H', entry_off) +
        struct.pack('<H', reloc_size)
    )

    return header + reloc_table + bin_data


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.bin> <output.prg> [reloc_offset ...]",
              file=sys.stderr)
        sys.exit(1)

    in_path  = sys.argv[1]
    out_path = sys.argv[2]
    relocs   = [int(x) for x in sys.argv[3:]]

    with open(in_path, 'rb') as f:
        bin_data = f.read()

    prg = build_prg(bin_data, relocs)

    if len(prg) > 16384:
        print(f"mkprg ERROR: output is {len(prg)} bytes, exceeds 16 KB bank limit.",
              file=sys.stderr)
        sys.exit(1)

    os.makedirs(os.path.dirname(out_path) or '.', exist_ok=True)
    with open(out_path, 'wb') as f:
        f.write(prg)

    entry_off = 8 + len(relocs) * 2
    print(f"mkprg: {os.path.basename(in_path)} → {os.path.basename(out_path)}  "
          f"{len(prg)} bytes  entry_offset={entry_off}  reloc_entries={len(relocs)}")


if __name__ == '__main__':
    main()
