# Orbt ESC changelog

This log tracks the **Orbt package versions**, independently of upstream AM32
versions. The active Orbt branch is `Orbt-main`; upstream `main` is separate.
Firmware, bootloader/configuration combinations, checksums, and raw BIN files
are archived under [releases](releases/README.md).

## 1.4.1 - DShot timer rollover (2026-09-17)

- Apply the 16-bit capture subtraction fix supplied by Julian, also present
  upstream in `4849bc54894dd8c58cd6e2b3ecbc0ef41b3f4a82` (Edward Li).
  Wrapped captures now produce the correct frame length during ordinary
  DShot frame learning.
- Add a regression covering normal, wrapped, and mixed frame windows.
  All 12 source-backed regression groups pass; removing the cast fails it.
- Keep the coast/release protocol, voltage compensation, and internal
  **ORBT_V203 2.19** identity unchanged. Flash grows by 4 bytes to 20592;
  RAM stays at 4440 bytes. Standalone WCH GCC 12 builds without warnings.
- Archive app-only and M1/M2 bootloader/config HEX and BIN images, checksums,
  and build provenance. The published application reproduces byte-for-byte.

This fix is not on the V203 bidirectional frame-learning path and is not a
proven fix for the bench controller's `rpm_feedback_lost` event. Version
1.4.1 has been compiled and host-tested; it has not been flashed or motor-tested.

## 1.4.0 — Latched coast (2026-09-17)

- Add custom DShot **15 = coast**, **16 = release coast**. Six accepted
  repetitions are required. Coast works while running or disarmed and blocks
  all ordinary throttle until explicitly released.
- Turn all six gate commands off, inhibit drive/brake primitives during
  coast/disarm, and reset control state without disabling passive phase sensing.
- Start ORBT V203 in latched coast; disable startup/arming tunes. Release
  disarms and requires fresh, continuous wire-zero traffic before driving.
- Fix release/DMA races, bidirectional-DShot rearming, and stale DMA decoding.
  Preserve the prior interrupt state across gate GPIO changes.
- Keep 1.3.0 voltage/dead-time compensation. Preserve 3D neutral semantics while
  accepting raw 48 as an armed 1D stop alias.
- Add an IDE-independent WCH GCC 12 build and source-backed host regressions.
  The previous 1.3.0 release reproduces byte-for-byte.
- Internal firmware identity: **ORBT_V203 2.19**. New protocol requires a
  compatible controller; existing zero/throttle startup alone cannot release it.

M1 USB spin, normal-stop requests, and timed-coast requests were exercised on
the bench and rotation was visually confirmed. Passive coast RPM remains best
effort; a stopped/unknown value is not proof of rest. Gate timing and mechanical
coast torque have not been measured. MOSFET body diodes remain connected.

## 1.3.0 — Voltage compensation (archived 2026-06-24)

- Add voltage and dead-time feed-forward compensation with Vrefint ADC
  calibration (`2727008`).
- Add standalone compilation notes and app/M1/M2 release images (`5411260`,
  `41837f8`). Internal AM32 version: **2.18**.
- Archived application was reproduced byte-for-byte with WCH GCC 12.

## 1.2.0 — Startup tuning (archived 2026-04-08)

- Update startup power to improve startup (`29186f2`).
- Archive app-only and bootloader/config images for M1 and M2.
- The archived 1.2.0 image reproduces byte-for-byte from `29186f2`. The older
  tag **Orbt_ESC_1.0.2_2026_4_10** points to that same source revision; its
  historical name is retained, not rewritten. `Orbt_ESC_1.2.0` is the artifact
  version alias.

## 1.0.1 — Startup duty (archived 2025-09-30)

- Increase startup duty (`1b34385`). Retain the historical tag
  **Orbt_ESC_1.0.1_2025_9_30**.
- Archived app-only image reproduces byte-for-byte from that source revision.

## 1.0.0 — Initial Orbt control choice

- Disable speed control (`69841f7`), tagged **Orbt_ESC_1.0.0**.
- A separately labeled `_rebuilt` image is built from that original tag for
  this archive. No original 1.0.0 binary was available for bitwise comparison.
