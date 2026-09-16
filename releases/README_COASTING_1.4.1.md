# ORBT V203 coasting 1.4.1 — DShot timer rollover fix

This is package 1.4.0 plus the one-line `Src/signal.c` fix supplied by Julian:

```c
average_packet_length = average_packet_length + (uint16_t)(dma_buffer[31] - dma_buffer[0]);
```

The internal ESC identity remains **ORBT_V203 2.19**; the package version is
**1.4.1**. The coast/release protocol, voltage compensation, and core test
firmware compatibility are unchanged. This build has **not been flashed**.

## Why the cast is needed

Capture timestamps come from a 16-bit timer, but `dma_buffer` and the frame
length accumulator are 32-bit unsigned values. Across timer rollover, the
subtraction wraps modulo 2^32 rather than modulo 2^16. The explicit cast
recovers the correct elapsed timer ticks before accumulating the eight-frame
average. For example, start=65000 and end=1039 represent 1575 ticks, not a
nearly-2^32 frame duration. A corrupted average produces bad acceptance limits
and can prevent subsequent DShot frames from being accepted.

The current CH32V203 bidirectional-DShot path returns earlier from
`transfercomplete()`, before this frame-length-learning block. The fix is
correct for the ordinary DShot path, but is not a demonstrated explanation
or cure for `rpm_feedback_lost` during the current bidirectional bench test.

## Branch/history check

The live `Powerhouse-Engineering/AM32-Orbt-Fork` remote has five heads:
`main`, `Orbt-main`, `feature/voltage-compensation`, `test-debug-with-Pa6`, and
`very-high-starting-torque`. No Julian branch or PR was found in that fork.
The API returned no PRs; its listed fork was `Powerhouse-Engineering/BU01-Firmware`.
No AM32-named repository appeared in the accessible OrbtSpin organization list.
This does not rule out an unpublished branch or a separate personal fork.

The **exact same fix already exists upstream** in
[4849bc54894dd8c58cd6e2b3ecbc0ef41b3f4a82](https://github.com/am32-firmware/AM32/commit/4849bc54894dd8c58cd6e2b3ecbc0ef41b3f4a82),
“fix dshot packet length underflow,” authored by Edward Li on 2025-09-04 and
committed by AlkaMotors on 2025-10-28. It was absent from our working branch.
Only the one-line fix was applied; no unrelated upstream changes were merged.

## Validation

- All **12 source-backed host regression groups pass**.
- The new regression runs the real `transfercomplete()`/DShot decode code on
  normal frames, all-wrapped frames, a mixed window with one wrapped frame,
  and a frame starting at 65535. It checks the accumulated lengths, learned
  limits, and acceptance of a subsequent valid frame.
- Removing the cast from the generated test translation unit reproduces the
  accumulator assertion failure; the working source is not modified by that
  negative test.
- Standalone WCH GCC 12 build succeeds without warnings. FLASH: **20592 bytes**
  (+4 from 1.4.0). RAM: **4440 bytes** (unchanged).
- Comparison of build input hashes against the 1.4.0 manifest shows only
  **Src/signal.c** changed among firmware inputs.
- Combined HEX files validate and contain the exact new app. All packaged
  1.4.0 bootloader/configuration bytes are preserved, including each motor's
  configuration. This is a comparison to the previous release files, not a
  readback of the connected ESC.

Build command:

```text
python env_setup_scripts/build_orbt_v203.py --output obj/standalone-v203-coasting-1.4.1
```

## Files

| File | SHA-256 |
| --- | --- |
| [Application](Am32V203_Orbt_ESC__2026_9_17__1.4.1_coasting.hex) | `fd303a8e5da595e46f534ac7d22fd0ce3488fde08912bbc414d69e28dad971fa` |
| [M1 with bootloader/config](Am32V203_Orbt_ESC__2026_9_17__1.4.1_coasting_With_Bootloader_M1.hex) | `4263d4db27df9f10c346c07bde076dec4cb3a85cd6cd7f0d9e1bf8b23bc4e270` |
| [M2 with bootloader/config](Am32V203_Orbt_ESC__2026_9_17__1.4.1_coasting_With_Bootloader_M2.hex) | `fc71fc851d91c76c04d2ce2f406f911f230a89e0bdf06d60c761491debbbf3c9` |

App-only, M1 combined, and manifest copies are in Downloads. The earlier
1.4.0 files are retained. Use the app-only image to preserve an ESC's currently
installed bootloader/configuration rather than replacing them with the
packaged versions.
