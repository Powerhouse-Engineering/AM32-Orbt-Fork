# ORBT coasting review and test build

The voltage-compensation v1.3.0 baseline was rebuilt from commit `2727008` without
the MounRiver IDE or generated makefiles. The command-line WCH GCC12 toolchain
already installed with Studio was used. The entire resulting HEX is byte-for-byte
identical to the released HEX, SHA256
`12e184a361d277dacc2d6b9f23fc39a7855761c5141173f6b85c94c77daa8aa1`.
See [build instructions](ch32v203-standalone-build.md).

## Review changes

The existing bridge-off approach is appropriate for this six-gate V203 target:
preload the GPIO OFF levels, disconnect the PWM alternate functions, and inhibit
every drive/brake primitive for the entire coast. Setting PWM duty to zero alone
would not provide that behavior with complementary PWM.

The review fixed these issues in the initial uncommitted implementation:

- **Canceled release could still execute.** A DMA error between main's request
  check and `releaseCoast()` could cancel the request, but the function did not
  recheck it. It now checks both the request and latch with interrupts masked.
- **Bidirectional DShot could not re-arm after release.** Release cleared
  `zero_input_count`, but the telemetry path returned before incrementing it.
  Valid wire-zero counting now occurs in the decoder for both DShot modes.
- **Deferred bidirectional decoding could use a stale or overwritten capture.**
  V203 now decodes each completed capture in the DMA handler after starting the
  response from the separate telemetry buffer. DMA-error rejection also clears
  any deferred decode flag.
- **A renewed coast did not cancel an already queued release.** Command 15 now
  supersedes a command-16 release that main has not committed yet.
- **Critical-section compiler ordering was implicit.** A common helper now uses
  atomic QingKe interrupt save/disable and restore with memory clobbers. Nested
  OFF/drive transitions retain the previous interrupt state. The QingKe CSR
  mapping follows the [WCH processor manual, section 8.3](https://www.wch.cn/uploads/file/20220411/1649641123177990.pdf).

The existing power-on latch, silent startup/arming, command values 15/16, 1D
value-48 stop alias, voltage compensation, and dead-time compensation were kept.
This build therefore requires a controller that implements the
[custom protocol](dshot-coast-command.md). After every boot or coast, send six
accepted command-16 frames followed by at least 31 raw-zero frames and a full
one-second interval of continuing raw-zero traffic before applying throttle.
Send six accepted command-15 frames to coast. The first five retain prior input.

## Verification and artifacts

- Final target: `ORBT_ESC_V203`; firmware identity `ORBT_V203`, version `2.19`.
  Package name `1.4.0_coasting` identifies this bench-test build.
- Build completed without compiler warnings. FLASH: 20,588 bytes (+800 versus
  v1.3.0); RAM: 4,440 bytes (+4).
- Eleven source-backed host test groups pass with mocked peripherals, including
  all 2,047 non-release wire values while latched and every drive/brake primitive.
  Run `wsl -e python3 tests/run_coast_tests.py` on this machine.
- The release-cancellation and bidirectional re-arming tests reproduced failures
  before the corresponding fixes.
- Final assembly was inspected for IRQ masking before latch checks, conditional
  drive rejection, and IRQ restoration after bridge/state writes.
- All 202 firmware input hashes in the build manifest match the current source.
- App-only and combined M1/M2 HEX checksums and payloads were independently
  checked. Combined images retain bootloader V14 and the respective existing
  48-byte motor configuration at `0xF800`, plus `0xF830 = 0x39`.

Files are under `releases/`, prefixed
`Am32V203_Orbt_ESC__2026_9_17__1.4.0_coasting`:

- `.hex`: application only; SHA256
  `77f53b72371ca303f0d57b40d59fe395cf000bbdb3e83fc541e9fa4e7a1c7279`.
- `_With_Bootloader_M1.hex`: bootloader, application, M1 configuration.
- `_With_Bootloader_M2.hex`: bootloader, application, M2 configuration.
- `.manifest.json`: build provenance, input hashes, and package checks.

No hardware was flashed. Scope the six gates at entry, through link loss, and
through release/reset; also check the changed bidirectional telemetry timing.
Gate-OFF means no commanded MOSFET conduction, not guaranteed zero current or
braking: body diodes can still conduct and regenerate into the DC bus. Passive
eRPM tracking is best effort; zero telemetry is not proof of mechanical rest.
The protocol document contains the full bench matrix.
