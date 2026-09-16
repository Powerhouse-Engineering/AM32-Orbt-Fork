#!/usr/bin/env python3
"""Build ORBT_ESC_V203 with the WCH GCC 12 release settings, without an IDE.

Requires Python 3.9+ and the WCH RISC-V Embedded GCC12 toolchain. Outputs are
app-only: this script never accesses a programmer, bootloader, or ESC config.
"""

import argparse
import concurrent.futures
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import zipfile


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TOOLCHAIN = Path(
    r"C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32"
    r"\components\WCH\Toolchain\RISC-V Embedded GCC12\bin"
)
# Preserve the release project's object order: section layout affects flash bytes.
SOURCE_DIRS = (
    "Mcu/v203/Startup", "Mcu/v203/Src", "Mcu/v203/Drivers/Peripheral/src",
    "Mcu/v203/Drivers/Debug", "Mcu/v203/Drivers/Core",
    "Src/DroneCAN/libcanard/drivers/stm32", "Src/DroneCAN/libcanard",
    "Src/DroneCAN/dsdl_generated/src", "Src/DroneCAN", "Src",
)
INCLUDE_DIRS = (
    "Mcu/v203/Drivers/Debug", "Mcu/v203/Drivers/Core",
    "Mcu/v203/Drivers/Peripheral/inc", "Mcu/v203/Drivers/Peripheral/src",
    "Mcu/v203/Src", "Mcu/v203/Inc", "Inc", "Src", "Src/DroneCAN",
    "Src/DroneCAN/dsdl_generated/include", "Src/DroneCAN/libcanard",
    "Src/DroneCAN/libcanard/drivers",
)
COMMON_FLAGS = (
    "-march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -msave-restore "
    "-fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections "
    "-fdata-sections -fno-common -Wunused -Wuninitialized -g"
).split()


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_hex(path):
    """Validate Intel HEX checksums and return its sparse address/byte mapping."""
    memory, start_records = {}, []
    base, eof = 0, False
    for number, line in enumerate(path.read_text(encoding="ascii").splitlines(), 1):
        if not line:
            continue
        if eof or not line.startswith(":"):
            raise ValueError(f"{path}:{number}: unexpected record")
        record = bytes.fromhex(line[1:])
        if len(record) < 5 or len(record) != record[0] + 5 or sum(record) % 256:
            raise ValueError(f"{path}:{number}: invalid length/checksum")
        address = int.from_bytes(record[1:3], "big")
        kind, data = record[3], record[4:-1]
        if kind == 0:
            for offset, value in enumerate(data, base + address):
                if offset in memory:
                    raise ValueError(f"{path}:{number}: overlapping data")
                memory[offset] = value
        elif kind == 1 and not data and address == 0:
            eof = True
        elif kind in (2, 4) and len(data) == 2 and address == 0:
            base = int.from_bytes(data, "big") << (4 if kind == 2 else 16)
        elif kind in (3, 5) and len(data) == 4 and address == 0:
            start_records.append((kind, data.hex()))
        else:
            raise ValueError(f"{path}:{number}: unsupported/malformed record")
    if not eof or not memory:
        raise ValueError(f"{path}: missing EOF/data")
    return memory, start_records


def hex_info(path):
    memory, starts = read_hex(path)
    addresses = sorted(memory)
    ranges, first, last = [], addresses[0], addresses[0]
    for address in addresses[1:]:
        if address != last + 1:
            ranges.append(f"0x{first:08X}-0x{last:08X}")
            first = address
        last = address
    ranges.append(f"0x{first:08X}-0x{last:08X}")
    return {"sha256": sha256(path), "bytes": len(memory), "ranges": ranges,
            "start_records": starts}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path,
                        default=Path(os.environ.get("WCH_GCC_BIN", DEFAULT_TOOLCHAIN)))
    parser.add_argument("--output", type=Path, default=ROOT / "obj/standalone-v203")
    parser.add_argument("--revision", help="Build an isolated git snapshot, e.g. 2727008")
    parser.add_argument("--compare", type=Path, help="Require identical HEX file and flash bytes")
    parser.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 1))
    parser.add_argument("--extra-cflag", action="append", default=[])
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    output = args.output.resolve()
    # Never clean recursively or depend on existing .o files: every source rebuilds.
    if output == ROOT or output in ROOT.parents:
        parser.error("--output must be a separate build directory")
    output.mkdir(parents=True, exist_ok=True)
    suffix = ".exe" if os.name == "nt" else ""
    binaries = {name: args.toolchain.resolve() / ("riscv-wch-elf-" + name + suffix)
                for name in ("gcc", "objcopy", "objdump", "size")}
    for binary in binaries.values():
        if not binary.is_file():
            parser.error(f"Tool not found: {binary}; set --toolchain or WCH_GCC_BIN")

    def git(*command):
        return subprocess.check_output(["git", "-C", str(ROOT), *command])

    revision = git("rev-parse", args.revision or "HEAD").decode().strip()
    source = ROOT
    if args.revision:
        source = output / ("source-" + revision)
        # A new directory prevents leftover or edited files contaminating the snapshot.
        if source.exists():
            parser.error(f"Snapshot already exists: {source}; choose a new --output")
        archive = git("archive", "--format=zip", revision, "Inc", "Src", "Mcu/v203")
        with zipfile.ZipFile(io.BytesIO(archive)) as packed:
            packed.extractall(source)

    # A failed rebuild must not leave an older successful firmware looking current.
    for name in ("Am32V203.elf", "Am32V203.hex", "Am32V203.bin", "Am32V203.map",
                 "Am32V203.lst", "commands.json", "manifest.json"):
        (output / name).unlink(missing_ok=True)
    version = subprocess.check_output([str(binaries["gcc"]), "--version"]).decode().strip()
    commands = []
    log_path = output / "build.log"
    log_path.write_text(version + "\n", encoding="utf-8")

    def run(command):
        result = subprocess.run([str(arg) for arg in command], cwd=output,
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return result.returncode, result.stdout

    def record_result(command, result):
        code, message = result
        commands.append([str(arg) for arg in command])
        with log_path.open("a", encoding="utf-8") as log:
            log.write(subprocess.list2cmdline(commands[-1]) + "\n" + message)
        if message:
            print(message, end="", flush=True)
        if code:
            raise RuntimeError(f"Command failed ({code}); see {log_path}")

    sources = [path for directory in SOURCE_DIRS
               for path in sorted((source / directory).iterdir(), key=lambda path: path.name)
               if path.suffix in (".c", ".S")]
    objects, compile_commands = [], []
    for path in sources:
        relative = path.relative_to(source)
        obj = output / "objects" / relative.with_suffix(".o")
        obj.parent.mkdir(parents=True, exist_ok=True)
        objects.append(obj)
        if path.suffix == ".S":
            flags = ["-x", "assembler-with-cpp", "-I" + str(path.parent)]
        else:
            flags = ["-DORBT_ESC_V203=", *["-I" + str(source / d) for d in INCLUDE_DIRS],
                     "-std=gnu99", *args.extra_cflag]
        compile_commands.append([binaries["gcc"], *COMMON_FLAGS, *flags, "-MMD", "-MP",
                                 "-MF", obj.with_suffix(".d"), "-c", "-o", obj, path])
    print(f"Building {len(sources)} sources from {source}\n{version.splitlines()[0]}", flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for command, result in zip(compile_commands, pool.map(run, compile_commands)):
            record_result(command, result)

    elf, ihex = output / "Am32V203.elf", output / "Am32V203.hex"
    link = [binaries["gcc"], *COMMON_FLAGS, "-T", source / "Mcu/v203/Link.ld",
            "-nostartfiles", "-Xlinker", "--gc-sections", "-Xlinker", "--print-memory-usage",
            "-Wl,-Map," + str(output / "Am32V203.map"), "--specs=nano.specs",
            "--specs=nosys.specs", "-o", elf, *objects, "-lm", "-lprintf"]
    record_result(link, run(link))
    for command in ([binaries["objcopy"], "-O", "ihex", elf, ihex],
                    [binaries["objcopy"], "-O", "binary", elf, output / "Am32V203.bin"],
                    [binaries["size"], "--format=berkeley", elf]):
        record_result(command, run(command))
    listing_command = [binaries["objdump"], "--all-headers", "--demangle", "--disassemble",
                       "-M", "xw", elf]
    with (output / "Am32V203.lst").open("w", encoding="utf-8") as listing:
        subprocess.run([str(arg) for arg in listing_command], check=True, stdout=listing)
    commands.append([str(arg) for arg in listing_command])
    manifest = {
        "revision": revision, "snapshot": bool(args.revision), "source": str(source),
        "git_status": "" if args.revision else git("status", "--short").decode(),
        "compiler": version, "compiler_sha256": sha256(binaries["gcc"]),
        "flags": COMMON_FLAGS, "extra_cflags": args.extra_cflag,
        "sources": [str(path.relative_to(source)) for path in sources],
        "input_sha256": {str(path.relative_to(source)): sha256(path)
                         for base in ("Inc", "Src", "Mcu/v203")
                         for path in sorted((source / base).rglob("*"))
                         if path.is_file() and path.suffix in (".c", ".h", ".S", ".ld")},
        "hex": hex_info(ihex),
    }
    if args.compare:
        reference = args.compare.resolve()
        memory, starts = read_hex(ihex)
        expected, expected_starts = read_hex(reference)
        mismatches = sorted(address for address in memory.keys() | expected.keys()
                            if memory.get(address) != expected.get(address))
        manifest["comparison"] = {
            "reference": str(reference), "reference_sha256": sha256(reference),
            "file_identical": ihex.read_bytes() == reference.read_bytes(),
            "flash_identical": not mismatches, "start_records_identical": starts == expected_starts,
            "different_addresses": len(mismatches),
            "first_differences": [f"0x{address:08X}" for address in mismatches[:20]],
        }
    (output / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({key: manifest[key] for key in ("hex", "comparison") if key in manifest}, indent=2))
    print(f"Outputs: {output}")
    if args.compare and not all(manifest["comparison"][key] for key in
                                ("file_identical", "flash_identical", "start_records_identical")):
        raise SystemExit("Release comparison failed; see manifest.json")


if __name__ == "__main__":
    main()
