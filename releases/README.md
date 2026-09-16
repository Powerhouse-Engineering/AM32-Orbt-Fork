# Orbt ESC release archive

See [CHANGELOG_ORBT.md](../CHANGELOG_ORBT.md) for the Orbt-specific history.
Orbt package versions and embedded AM32 versions are separate identifiers.
Older tag names are retained; version 1.2.0 is the binary-version alias of
the source previously tagged `Orbt_ESC_1.0.2_2026_4_10`.

Each `.hex` has a matching raw `.bin`. **Prefer HEX when possible**, because it
encodes addresses. A BIN does not: use its exact `load_address` from
[ORBT_RELEASES.json](ORBT_RELEASES.json). Addresses there are flash offsets;
`mapped_flash_address` gives the corresponding `0x08000000` mapping for tools
that expect it. Typical app-only offsets are `0x1000`; combined M1/M2 images
start at `0x0`. Gaps in BIN files are filled with `0xFF`.

- App-only images preserve the existing bootloader/config when programmed only
  into their specified regions. HEX preserves sparse gaps; a BIN spans them.
- `_With_Bootloader_M1` and `_M2` replace the bootloader and the respective
  packaged motor configuration as well as the application. Select the correct
  motor image; these are not interchangeable configuration files.
- The 1.0.0 `_rebuilt` image matches the original 2025-08-03 HEX recovered
  from Downloads byte-for-byte. The filename is retained for build provenance.
  Archived 1.0.1, 1.2.0, and 1.3.0 app HEX files also reproduce byte-for-byte.
- The 1.4.x coast protocol starts latched off and needs an explicit release
  and rearm. Read [the protocol](../doc/dshot-coast-command.md) before use.

[SHA256SUMS.txt](SHA256SUMS.txt) covers archived files; the JSON index also
records both image hashes, payload lengths, and BIN span lengths.

To regenerate BIN files and the index after adding a release:

```text
python env_setup_scripts/index_orbt_releases.py
```

To build with WCH GCC 12, without running MounRiver Studio:

```text
python env_setup_scripts/build_orbt_v203.py --output obj/release
```
