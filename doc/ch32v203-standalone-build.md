# ORBT CH32V203 build without the MounRiver IDE

`env_setup_scripts/build_orbt_v203.py` invokes the WCH compiler, linker, and
binutils directly. It needs Python 3.9+ and the complete **WCH RISC-V Embedded
GCC12** toolchain, but does not launch MounRiver Studio, use its generated
makefiles, or require GNU Make. It also does not flash hardware.

The verified compiler is:

- `riscv-wch-elf-gcc.exe (xPack GNU RISC-V Embedded GCC i386) 12.2.0`
- Compiler executable SHA256:
  `701745ee11116d8f9e6beb8029116213947ad690baca62a8832b1fd476f74c7c`
- On this machine it was already installed at
  `C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC12\bin`.

The installed compiler is a command-line executable bundled with Studio; this
verification does not claim to have replaced WCH's compiler with upstream GCC.
An unpacked standalone copy of the complete WCH toolchain can instead be selected
with `--toolchain 'C:\path\to\RISC-V Embedded GCC12\bin'` or `WCH_GCC_BIN`.
Copying only the `bin` directory is insufficient because GCC also needs its
libraries and internal executables.

## Build the working tree

Run from the repository root:

```powershell
python env_setup_scripts/build_orbt_v203.py --output obj/standalone-v203-coasting
```

The build uses the current `Inc`, `Src`, and `Mcu/v203` files, including uncommitted
changes. Every translation unit recompiles. The output directory contains:

- `Am32V203.hex`: application-only Intel HEX for the ORBT V203 target.
- `Am32V203.elf`, `.bin`, `.map`, and `.lst`: debugging and inspection artifacts.
- `manifest.json`: source revision/status, input hashes, compiler identity, output
  hash, and validated address ranges.
- `commands.json` and `build.log`: exact command arguments and compiler output.

The HEX excludes bootloader and EEPROM/config data. The flat `.bin` includes gaps
between app sections; prefer the HEX when the programming workflow accepts it.

## Reproduce the voltage-compensation v1.3.0 release

Use a new output directory for each `--revision` invocation. This option exports
only committed build inputs with `git archive`, isolating them from working-tree
coasting changes and stale objects.

```powershell
python env_setup_scripts/build_orbt_v203.py --revision 2727008 --output obj/standalone-v203-release-verified --compare releases/Am32V203_Orbt_ESC__2026_6_24__1.3.0.hex
```

Verified on 2026-09-17 from commit
`2727008dabf69ad222c4f72f769066b6dc019692`. Firmware source at `41837f8` is identical;
the intervening commits added build notes and release files.

Result: **the complete generated HEX file is byte-for-byte identical to the
v1.3.0 release**, including record structure and start address. The script also
checks each record checksum and compares sparse address/byte maps independently.

| Check | Result |
| --- | --- |
| Both HEX SHA256 hashes | `12e184a361d277dacc2d6b9f23fc39a7855761c5141173f6b85c94c77daa8aa1` |
| Programmed bytes | 19,824 |
| Differing flash addresses | 0 |
| Data ranges | `0x1000–0x1003`, `0x10D0–0x5E1B`, `0xF7E0–0xF7FF` |
| FLASH region usage | 19,788 / 57,136 bytes |
| RAM region usage | 4,436 / 20,480 bytes |
| ELF size | text 18,848; data 976; bss 3,460 |

ELF files can differ in debugging paths when built from different directories;
the comparison covers the entire flashable HEX file, not path-dependent debug
sections in the ELF.

## Release settings that affect reproducibility

Common compiler/linker options:

```text
-march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -msave-restore
-fmax-errors=20 -Os -fmessage-length=0 -fsigned-char
-ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g
```

C files additionally use `-std=gnu99` and the ORBT target selected in
`Inc/config.h`; the script also defines `ORBT_ESC_V203` with the same empty value.
Assembly uses `-x assembler-with-cpp`. Linking uses `Mcu/v203/Link.ld`,
`-nostartfiles`, `--gc-sections`, `--specs=nano.specs`, `--specs=nosys.specs`,
`-lm`, and `-lprintf`.

The runner preserves the release project's object order, including case-sensitive
filename ordering. Ordering changes can alter section addresses and relaxation,
even with identical sources and flags. The repository's top-level Makefile uses
different settings (`-O3`, ordinary `rv32imac`, and different object ordering), so
it is not the recipe used for this release comparison.

The original project's `AIRBOT_V20x` command-line define is unused in these
sources. The actual board selection is `ORBT_ESC_V203`; the exact release match
verifies that explicitly defining the ORBT target preserves the image.
