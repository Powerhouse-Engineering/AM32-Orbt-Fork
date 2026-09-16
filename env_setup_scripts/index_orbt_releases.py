#!/usr/bin/env python3
"""Validate archived HEX files, export address-preserving BINs, and index releases."""
import hashlib
import json
from pathlib import Path
import re

from build_orbt_v203 import read_hex

ROOT = Path(__file__).resolve().parents[1]
RELEASES = ROOT / "releases"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    entries = []
    for path in sorted(RELEASES.glob("*.hex")):
        memory, _ = read_hex(path)
        first, last = min(memory), max(memory)
        blob = bytes(memory.get(address, 0xFF) for address in range(first, last + 1))
        binary = path.with_suffix(".bin")
        binary.write_bytes(blob)
        assert all(blob[address - first] == value for address, value in memory.items())
        version = re.search(r"__(\d+\.\d+\.\d+)", path.name)
        entries.append({
            "version": version.group(1) if version else "bootloader",
            "hex": path.name, "bin": binary.name,
            "load_address": f"0x{first:08X}",
            "mapped_flash_address": f"0x{0x08000000 + first:08X}",
            "payload_bytes": len(memory), "bin_span_bytes": len(blob),
            "gap_fill": "0xFF", "hex_sha256": digest(path), "bin_sha256": digest(binary),
        })
    (RELEASES / "ORBT_RELEASES.json").write_text(json.dumps(entries, indent=2) + "\n")
    checksums = [f"{digest(path)}  {path.name}" for path in sorted(RELEASES.iterdir())
                 if path.is_file() and path.name != "SHA256SUMS.txt"]
    (RELEASES / "SHA256SUMS.txt").write_text("\n".join(checksums) + "\n")
    print(f"Validated and indexed {len(entries)} HEX/BIN pairs")


if __name__ == "__main__":
    main()
