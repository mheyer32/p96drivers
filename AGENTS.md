# Agent Instructions

Instructions for AI coding agents working in this repository.

**File name:** use **`AGENTS.md`** at the repo root (this file). Some environments ignore lowercase `agents.md`.

---

## Project overview

- **What it is**: Picasso96 (P96) graphics drivers for Amiga-style systems.
- **Stack**: C and C++14 (AmigaOS-style APIs), hardware drivers (Mach64, Mach32, AT3D, S3 Trio64, etc.).
- **Conventions**: Follow existing style in the codebase; use the project makefile and existing driver layout as reference.

### C++ chip/card drivers

Reference: **`mach32/`** (full method + trampoline migration), also **`mach64/`**, **`at3d/`**, **`s3trio64/`**.

- **Overlay**: `XxxDriver : P96Driver` is a non-virtual view of `BoardInfo` (`static_assert` size/layout). No C++ vtables.
- **Build**: `.cpp` with `-std=c++14 -fno-exceptions -fno-rtti` (`CXXFLAGS_EXTRA` in the makefile). Keep `chip_library.c` / `card_library.c` as C.
- **Registers**: typed apertures in `*_regs.hpp` / `*_reg_apertures.hpp` via `reg_access.hpp` (`AbsRegAperture` / `AtiRegAperture`). Include the driver header **before** chip `#define` register names so `IoReg::NAME` is not macro-clobbered; in `.cpp` after macros, use `static_cast<IoReg::Id>(0x…)` when needed.
- **Hooks**: ASM/`__REGxx` methods on the driver; thin `static` trampolines; `P96_HOOK(bi->Field, Trampoline)` in `InitChip` (only for function pointers — never wrap numeric assigns).
- **Pitfalls**: `extern const` for `LibName` / `LibIdString` / version; wrap C exports in `extern "C"` **after** C++ includes (do not put `p96_driver.hpp` inside `extern "C"`); rename `template` params and avoid `class` as an identifier; do not `goto` across initializations; Amiga g++ can ICE on `movem` asm inside methods and on some error paths — keep register save/restore on trampolines; cast `RGBFTYPE_REG` when calling helpers that take `RGBFTYPE`; TESTEXE ModeInfo tables need field-by-field init (no C99 designated initializers).
- **Helpers**: `tools/cxx_migrate_hooks.py` + `tools/cxx_postprocess.py` (fix gotos / comment-only `goto fallback` before migrating; brace scanner skips `//` and `/* */`).

---

## Dev environment

- **Toolchain**: On this machine, run `source ~/bin/startAmigaGCC.sh` so `m68k-amigaos-gcc` and related tools are on `PATH`.
- **Build**: From the repo root, use the root `makefile` (GNU Make finds `makefile`).
  - Full build: `make` or `make all` — drivers and tests go to `_bin/`, objects to `_o/`.
  - Mach32 chip test only: `make TestMach32` → `_bin/TestMach32`.
- **Dependencies**: Amiga-GCC toolchain, Picasso96 SDK headers (`Picasso96Develop/`), `openpci` for PCI test programs.
- **Key dirs**: `mach32/`, `mach64/`, `at3d/`, `s3trio64/` for driver code; `Picasso96Develop/` for SDK and examples.
- **Mach64 chip fork**: `ATIMach64GX.chip` / `TestMach64GX` (`CONFIG_ATIMACH64_GX`) polls `FIFO_STAT` in `waitFifo` and covers **GX** (external DAC) plus **CT** (integrated DAC/BedRock PLL via `InitMach64CT`); `ATIMach64.chip` / `TestMach64` (`CONFIG_ATIMACH64_VT`, VT+) makes `waitFifo` a no-op and relies on `BUS_PCI_RETRY_EN`. The card opens the matching chip by device family.

---

## Deploy to the Amiga (squirt)

Built binaries are copied over the network with **[squirt.sh](squirt.sh)** (same pattern as a minimal wrapper **[squirt-testmach32.sh](squirt-testmach32.sh)** for a single executable).

- Defaults: `SQUIRT_HOST=192.168.0.110`, `SQUIRT_PATH=~/squirt/build` (must contain `squirt` and `squirt_exec`).
- **Drivers** (`*.chip`, `*.card`) are sent to **`SYS:libs/picasso96/`**.
- **Test tools** (e.g. **`TestMach32`**, **`TestCirrus`**, other `Test*` targets from the `makefile`) are sent to **`SYS:c/`**.

After `make` / `make TestMach32` / `make TestCirrus`:

```sh
./squirt.sh
# or only the Mach32 test binary:
./squirt-testmach32.sh
```

Override the host if needed: `SQUIRT_HOST=192.168.x.x ./squirt-testmach32.sh`.

---

## Run tests on the Amiga and inspect results

- **Run**: Connect to the Amiga with **telnet** to its address (e.g. `telnet 192.168.0.110`, same host as `SQUIRT_HOST` in squirt defaults). In that shell, run the test binary from `C:` (or `SYS:c`), e.g. **`TestMach32`** or **`TestCirrus`**. Programs talk to the board via OpenPCI, program a mode where applicable, and print debug output when built with `TESTEXE`/`DBG` (see `make_exe` in the `makefile`). Other remote shells (serial, etc.) work too if you prefer.

### OpenPCI: standalone chip tests vs card driver

A **card** driver’s **`FindCard`** uses **`SetBoardAttrs(board, PRM_BoardOwner, …)`** so only one client owns the board at a time. That is **coordination between drivers**, not what makes PCI BAR memory decode work. Standalone **`Test*`** programs still need the same **PCI command**, **MMU/cache** (`setCacheMode`), and **chip register** setup as **`InitCard`** before **`InitChip`** touches VRAM. If the board is already owned, unload the card driver for a raw chip test or use the **`*Card`** target.

Before **`InitChip`** touches VRAM from the CPU, match **InitCard**-style setup where applicable: **`PCI_COMMAND`** memory (and I/O if needed) enabled, and **`setCacheMode`** on the framebuffer region via **`mmu.library`** (see **`common.c`** / Mach32 **`InitCard`**).

**Legacy VGA memory (0xA0000 / 0xB8000, …) vs BAR0:** OpenPCI’s **`PRM_PCIToHostOffset`** is the byte delta to add to **PCI bus memory addresses** in the card’s legacy window to obtain the **m68k logical** pointer (`openpci.doc` / `pcitags.h`). Use it to probe whether VRAM responds on the classic VGA apertures when the linear framebuffer (`SR7` segment select) is still disabled — useful to separate “BAR0/LFB programming” from “any VRAM decode at all.”

- **Framebuffer / VRAM checks**: Use **`med`** to dump framebuffer memory after draws. Syntax: `med d<size> <addr> <count>` where `<size>` is `b`/`w`/`l`/`q` (byte/word/long/quad) and `<count>` is the **number of items** to print — not an end address. Example: `med dl 0x7f800000 64` prints 64 longwords (256 bytes) starting at that address. The **PCI BAR0 base** for the Mach32 is whatever OpenPCI reports (example: **`0x7F800000`**; use the address from the test output).
- **BytesPerRow**: For 640×480 CLUT, **`RenderInfo.BytesPerRow` of 640** is normal for an 8‑bpp unpacked row; do not assume padded values like 704 unless your mode math actually requires them.

---

## Testing (general)

- **Integration**: Run on target Amiga with P96; use examples under `Picasso96Develop/Examples/` (e.g. OpenScreen, ModeList) for broader P96 validation.
- **CI / checks**: `make` should succeed with the Amiga-GCC toolchain available.

---

## Code and PR conventions

- **Style**: Match existing C style; `_clang-format` is present—use it if the project is formatted with clang-format.
- **Commits**: Clear, short messages; reference driver or area (e.g. mach64, at3d).
- **PRs**: Describe what driver/feature is changed and how it was tested.

---

## Notes for agents

- Prefer existing patterns (e.g. from `card_common.c`, `chip_library.c`) when adding or changing drivers.
- Hardware/register details often live in driver-specific headers and in `Picasso96_card.h` / `Picasso96_chip.sfd`.
- Mach64 GX VBIOS cold init / `MEM_CNTL` / size-detect notes: **[docs/mach64gx_vbios_hw_init.md](docs/mach64gx_vbios_hw_init.md)** (PCI aperture probe for P96; do not copy BIOS VGA paging).
- One Mach64 GX card’s VRAM / TLC34075 / ICS2595 clock & pixel path (not all Mach64 boards): **[docs/mach64gx_vram_dac_clock.md](docs/mach64gx_vram_dac_clock.md)**.
- Do not commit ROM/binaries or large binary assets unless the project explicitly tracks them.

---
