## 3. File format (.prg)

Source: [tools/mkprg.py](tools/mkprg.py), [programs/include/prg_header.mac](programs/include/prg_header.mac).

The file begins with an 8-byte header. When the file is loaded into a PRG bank, file offset 0 sits at `$8000`, so the header occupies `$8000–$8007` and code begins at `$8000 + entry_offset`.

| offset | size | meaning |
|---|---|---|
| 0–1 | 2 bytes | magic `'P' 'R'` |
| 2 | 1 byte | version (currently `1`) |
| 3 | 1 byte | flags; bit 0 = reloc table present |
| 4–5 | u16 LE | `entry_offset` (= 8 when no reloc table; OS jumps to `$8000 + entry_offset`) |
| 6–7 | u16 LE | `reloc_table_size` in bytes |

If `flags & 1` is set, an array of u16 LE offsets-from-`$8000` follows the header, listing the byte positions of every 2-byte ROM pointer that would need patching if the program were relocated. **The kernel does not implement the relocator yet** — programs run from their assigned ROM bank in-place, so emit no relocations. `PRG_HEADER_ROM` already does this.

**Hard size cap:** the entire `.prg` file (header + reloc table + code + RODATA) must be ≤ 16 384 bytes. `mkprg.py` enforces this; `ld65` will fail anyway if you overflow the linker-config `PRG` area.

The header macro you almost always want is `PRG_HEADER_ROM`:

```asm
.macro PRG_HEADER_ROM
    .byte $50, $52      ; magic 'P','R'
    .byte 1             ; version
    .byte 0             ; flags: no relocation table
    .word 8             ; entry_offset = 8  (code starts at $8008)
    .word 0             ; reloc_table_size = 0
.endmacro
```

Drop it at the very start of your `CODE` segment and your entry label is automatically at `$8008`.

---