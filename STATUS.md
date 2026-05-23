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
| `src/header.asm` | NES 2.0 header. 8×16KB PRG ROM, 0 CHR ROM (CHR RAM), mapper 1 (MMC1), 32KB battery NVRAM (NES 2.0 shift-count encoding), 8KB CHR RAM. |
| `src/startup.asm` | Reset handler: sei/cld, stack init, 2× vblank wait, zero RAM, init CC65 software stack (`c_sp=$0800`), then `jsr _main`. Also holds the VECTORS table. |
| `src/nmi.asm` | NMI handler (~60Hz): preserves registers, increments `_frame_count`, sets `_nmi_ready`. IRQ stub. References CC65 BSS symbols. |
| `src/zp.asm` | CC65 runtime zero-page slots: `c_sp`, `sreg`, `regsave`, `regbank`, `ptr1–4`, `tmp1–4`. |
| `src/chr_data.asm` | Exports `_ascii_chr_data` via `.incbin "../chr/ascii.chr"`. |
| `src/nes.h` | Hardware register macros: `PPU_*`, `APU_*`, `JOYPAD1`, `PPU_SCROLL`, `CTRL_*` / `MASK_*` flags. |
| `src/main.c` | All application logic. See boot sequence below. |
| `chr/ascii.chr` | 8KB CHR binary. ASCII-mapped font: first ~$0580 bytes are placeholder tiles for non-printable chars; printable ASCII ($20–$7F) follows. |
| `reference/ascii.png` | Source image the chr file is exported from (Aseprite). |

---

## Memory map (runtime)

```
$0000-$00FF   Zero page      — CC65 runtime slots (c_sp, ptr1-4, tmp1-4, etc.)
$0100-$01FF   Stack          — hardware-fixed, 256 bytes
$0200-$07FF   RAM            — CC65 BSS (globals: nmi_ready, frame_count, joypad state)
$6000-$7FFF   WRAM (PRG RAM) — 8KB window into 32KB physical PRG RAM (banked via MMC1)
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

## Boot sequence / POST

MMC1 is initialised before any PRG RAM access: shift register reset, Control=$0F
(horizontal mirror, fix-last-16KB PRG, 8KB CHR), PRG bank register=0 (RAM enabled).
PRG RAM banks are selected by writing bits 2–3 of the CHR bank 0 register ($A000).
NMI is disabled for the entire POST so MMC1 serial writes cannot be interrupted.
All VRAM writes after rendering is enabled are gated to VBlank via `ppu_wait_vblank()`,
with `PPU_SCROLL`/`PPU_CTRL` restored after each write.

Screen output (one line per test, filled live):

```
> CHR OK
> PRG RAM 7F OK        ← hex count of 256-byte pages verified; OK or FAIL
> EXT 00K              ← extra RAM beyond 32KB found via CHR bit-4 probe
> VER 5
```

Steps in order:

1. `sei` / `cld` — disable IRQs, clear decimal
2. Stack → $01FF
3. Disable PPU (`PPUCTRL=0`, `PPUMASK=0`)
4. Wait for first vblank
5. Zero all RAM ($0000–$07FF)
6. Wait for second vblank (PPU ready)
7. Init CC65 software stack (`c_sp = $0800`)
8. `main()`:
   a. Disable PPU output
   b. Clear APU, enable channels
   c. `chr_load(0x0000, ascii_chr_data, 8192)` — copy font into CHR RAM
   d. `load_palette()` — write 32-byte palette to PPU $3F00
   e. `ppu_clear_nt0()` — fill nametable with tile $00
   f. Write static label rows to nametable (PPU off — free VRAM access)
   g. Enable BG rendering (NMI still off); synchronise to VBlank
   h. **PRG RAM POST** — two-pass write-then-verify across 4 banks (32KB);
      page counter at $204B updates each VBlank during verify pass;
      OK/FAIL written at end
   i. **Extended RAM survey** — canary probe of banks 4–7 (CHR bit 4);
      counts distinct banks, never fails; result written at $2067
   j. Enable NMI + full rendering (BG + sprites)
9. Main loop: spin on `nmi_ready`, call `read_joypad` each frame

---

## Not yet built

- Text printing routine (draw string to nametable)
- MMC1 PRG ROM bank switching
- Any shell / UI layer
