# suboros — project status

Toy OS for Famicom (NES). Targeting SXROM (MMC1): 128KB PRG ROM, 32KB PRG RAM, 8KB CHR RAM.
Application logic written in C (CC65); interrupt handler and startup in ca65 assembly.

---

## Toolchain

- **cc65** — C compiler (C → 6502 asm). Present at `tools/cc65/bin/cc65.exe`.
- **ca65 / ld65** — assembler and linker from the cc65 suite.
- **Mesen** — emulator at `E:\Portable Software\mesen\`.
- **build.bat** — compiles `src/main.c`, assembles `src/*.asm`, links to `build/suboros.nes`.
- **run.bat** — calls build.bat then launches Mesen.

---

## Project files

| File | What it does |
|------|-------------|
| `suboros.cfg` | ld65 linker script. SXROM layout: 7 filler banks + 1 fixed bank (PRG_FIXED at $C000). Defines `__STACKSTART__` and CONDES features required by `none.lib`. |
| `src/header.asm` | iNES header. 8×16KB PRG ROM (128KB), 0 CHR ROM (CHR RAM), MMC1 mapper, 4×8KB battery PRG RAM. |
| `src/startup.asm` | Reset handler: sei/cld, stack init, 2× vblank wait, zero RAM, init CC65 software stack (`c_sp=$0800`), then `jsr _main`. Also holds the VECTORS table. |
| `src/nmi.asm` | NMI handler (~60Hz): preserves registers, increments `_frame_count`, sets `_nmi_ready`. IRQ stub. References CC65 BSS symbols. |
| `src/zp.asm` | CC65 runtime zero-page slots: `c_sp`, `sreg`, `regsave`, `regbank`, `ptr1–4`, `tmp1–4`. |
| `src/chr_data.asm` | Exports `_ascii_chr_data` via `.incbin "../chr/ascii.chr"`. |
| `src/nes.h` | Hardware register macros: `PPU_*`, `APU_*`, `JOYPAD1`, `PPU_SCROLL`, `CTRL_*` / `MASK_*` flags. |
| `src/main.c` | All application logic: `chr_load`, `load_palette`, `ppu_clear_nt0`, `read_joypad`, `main`. |
| `chr/ascii.chr` | 8KB CHR binary. ASCII-mapped font: first ~$0580 bytes are placeholder tiles for non-printable chars; printable ASCII ($20–$7F) follows. |
| `reference/ascii.png` | Source image the chr file is exported from (Aseprite). |

---

## Memory map (runtime)

```
$0000-$00FF   Zero page      — CC65 runtime slots (c_sp, ptr1-4, tmp1-4, etc.)
$0100-$01FF   Stack          — hardware-fixed, 256 bytes
$0200-$07FF   RAM            — CC65 BSS (globals: nmi_ready, frame_count, joypad state)
$6000-$7FFF   WRAM (PRG RAM) — 8KB window into 32KB physical PRG RAM
$8000-$BFFF   PRG ROM banks 0-6 — switchable via MMC1 (unused, filler $FF)
$C000-$FFFF   PRG ROM bank 7  — fixed. All code/data here. Vectors at $FFFA.
```

PPU / VRAM:
```
$0000-$0FFF   Pattern table 0 — CHR RAM, loaded with ascii.chr at boot
$1000-$1FFF   Pattern table 1 — CHR RAM, unused
$2000-$23FF   Nametable 0     — 32x30 tile indices (visible screen)
$23C0-$23FF   Attribute table — palette select per 2x2 tile block
$3F00-$3F1F   Palettes        — 4 BG + 4 sprite palettes, 4 colours each
```

---

## Boot sequence

1. `sei` / `cld` — disable IRQs, clear decimal
2. Stack → $01FF
3. Disable PPU (`PPUCTRL=0`, `PPUMASK=0`)
4. Wait for first vblank
5. Zero all RAM ($0000–$07FF)
6. Wait for second vblank (PPU ready)
7. Init CC65 software stack (`c_sp = $0800`)
8. `main()` — all remaining init runs in C:
   a. Disable PPU output
   b. Clear APU, enable channels
   c. `chr_load(0x0000, ascii_chr_data, 8192)` — copy font into CHR RAM
   d. `load_palette()` — write 32-byte palette to PPU $3F00
   e. `ppu_clear_nt0()` — fill nametable with tile $00
   f. Write "> CHR OK" tile indices to nametable at $2021 (row 1, col 1)
   g. Reset scroll
   h. Enable NMI + rendering
9. Main loop: spin on `nmi_ready`, call `read_joypad` each frame
