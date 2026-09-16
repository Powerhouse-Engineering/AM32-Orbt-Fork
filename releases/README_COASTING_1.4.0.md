# ORBT V203 coasting test build — 2026-09-17

Package **1.4.0** contains firmware version **2.19**, identifier **ORBT_V203**.
See the [review and validation report](../doc/coasting-review-2026-09-17.md),
[DShot protocol](../doc/dshot-coast-command.md), and
[standalone build instructions](../doc/ch32v203-standalone-build.md).

## Files

| File | Contents |
| --- | --- |
| [App HEX](Am32V203_Orbt_ESC__2026_9_17__1.4.0_coasting.hex) | Application only; preserve the existing bootloader/config |
| [M1 combined HEX](Am32V203_Orbt_ESC__2026_9_17__1.4.0_coasting_With_Bootloader_M1.hex) | V14 PA0 bootloader + app + existing motor-1 configuration |
| [M2 combined HEX](Am32V203_Orbt_ESC__2026_9_17__1.4.0_coasting_With_Bootloader_M2.hex) | V14 PA0 bootloader + app + existing motor-2 configuration |
| [Build manifest](Am32V203_Orbt_ESC__2026_9_17__1.4.0_coasting.manifest.json) | Source hashes, compiler identity, release comparison, address ranges, assembly review, artifact hashes |

```text
App SHA256: 77f53b72371ca303f0d57b40d59fe395cf000bbdb3e83fc541e9fa4e7a1c7279
M1  SHA256: 801e79f12e19fe48e7e7527b9879bf6c6c523e13edffe74fed47c70cf20648ca
M2  SHA256: 590f9958bb2332df0956bfebd1d5ec182084ab43f60451ca31b8a120029ef120
```

The build completed without warnings; all 11 host-test groups passed. The v1.3.0
baseline reproduced byte-for-byte. Both combined images preserve the new app and
the previous bootloader/config bytes exactly; HEX checksums are valid.

The application starts with the bridge latched off: six accepted command-16
packets and a fresh raw-zero arming sequence are required before drive. Subsequent M1 USB spin and stop/coast-command tests passed; the owner visually
confirmed rotation. Gate waveforms have not been measured. All-gates-off still permits body-diode
current; electrical behavior requires bench validation.
