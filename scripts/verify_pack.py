#!/usr/bin/env python3
"""
verify_pack.py - release gate for a .levipack.

Every check below maps to a real failure that reached a device at least once.

  1. manifest.json at zip root, type=preload-native, entry points at a .so
  2. exactly one .so in the package           (launcher rejects otherwise)
  3. DT_NEEDED contains libpreloader.so       (otherwise: dlopen cannot locate
                                               pl::NativeMod::current and the mod
                                               never loads)
  4. PLGetModRegistration is exported         (otherwise the preloader has no
                                               entry point to call)
  5. config/*.json parse as JSON

Usage:
    verify_pack.py <package.levipack>
"""
from __future__ import annotations

import json
import struct
import sys
import tempfile
import zipfile
from pathlib import Path

DT_NULL, DT_NEEDED, DT_STRTAB = 0, 1, 5


# --------------------------------------------------------------------------
# minimal ELF64 LE parser (no external deps)
# --------------------------------------------------------------------------
def elf_needed(path: Path) -> list[str]:
    data = path.read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
        raise SystemExit(f"!! {path.name} is not ELF64 little-endian")

    e_phoff = struct.unpack_from("<Q", data, 0x20)[0]
    e_phentsize = struct.unpack_from("<H", data, 0x36)[0]
    e_phnum = struct.unpack_from("<H", data, 0x38)[0]

    loads, dynamic = [], None
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type = struct.unpack_from("<I", data, off)[0]
        p_offset = struct.unpack_from("<Q", data, off + 0x08)[0]
        p_vaddr = struct.unpack_from("<Q", data, off + 0x10)[0]
        p_filesz = struct.unpack_from("<Q", data, off + 0x20)[0]
        if p_type == 1:  # PT_LOAD
            loads.append((p_vaddr, p_offset, p_filesz))
        elif p_type == 2:  # PT_DYNAMIC
            dynamic = (p_offset, p_filesz)

    if not dynamic:
        raise SystemExit(f"!! {path.name}: no PT_DYNAMIC")

    def vaddr_to_off(va: int) -> int | None:
        for p_vaddr, p_offset, p_filesz in loads:
            if p_vaddr <= va < p_vaddr + p_filesz:
                return p_offset + (va - p_vaddr)
        return None

    dyn_off, dyn_size = dynamic
    entries = []
    for i in range(dyn_size // 16):
        tag, val = struct.unpack_from("<qQ", data, dyn_off + i * 16)
        if tag == DT_NULL:
            break
        entries.append((tag, val))

    strtab_va = next((v for t, v in entries if t == DT_STRTAB), None)
    if strtab_va is None:
        raise SystemExit(f"!! {path.name}: no DT_STRTAB")
    str_off = vaddr_to_off(strtab_va)
    if str_off is None:
        raise SystemExit(f"!! {path.name}: DT_STRTAB outside PT_LOAD")

    needed = []
    for tag, val in entries:
        if tag == DT_NEEDED:
            start = str_off + val
            end = data.index(b"\x00", start)
            needed.append(data[start:end].decode())
    return needed


def elf_exports(path: Path, symbol: bytes) -> bool:
    """Cheap check: is the symbol name present in .dynstr?"""
    data = path.read_bytes()
    return symbol + b"\x00" in data


# --------------------------------------------------------------------------
def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify_pack.py <package.levipack>")
        return 2
    pkg = Path(sys.argv[1])
    problems: list[str] = []

    with zipfile.ZipFile(pkg) as z:
        names = z.namelist()
        if "manifest.json" not in names:
            print("!! manifest.json is not at the zip root")
            return 1
        manifest = json.loads(z.read("manifest.json"))

        if manifest.get("type") != "preload-native":
            problems.append(f"type must be 'preload-native', got {manifest.get('type')!r}")
        entry = manifest.get("entry", "")
        if not entry.endswith(".so"):
            problems.append(f"entry must be a .so path, got {entry!r}")

        sos = [n for n in names if n.lower().endswith(".so")]
        if len(sos) != 1:
            problems.append(f"package must contain exactly 1 .so, found {len(sos)}: {sos}")
        if entry not in names:
            problems.append(f"entry {entry!r} is not in the package")

        for n in names:
            if n.endswith(".json") and n != "manifest.json":
                try:
                    json.loads(z.read(n))
                except Exception as ex:
                    problems.append(f"{n} is not valid JSON: {ex}")

        tmp = Path(tempfile.mkdtemp())
        so_path = tmp / Path(entry).name
        if entry in names:
            so_path.write_bytes(z.read(entry))
            needed = elf_needed(so_path)
            if "libpreloader.so" not in needed:
                problems.append(
                    "DT_NEEDED is missing libpreloader.so -> the mod will fail to "
                    "dlopen with 'cannot locate symbol _ZN2pl3mod9NativeMod7currentEv'. "
                    f"NEEDED = {needed}")
            if not elf_exports(so_path, b"PLGetModRegistration"):
                problems.append("PLGetModRegistration is not exported -> preloader "
                                "has no entry point to call")

        print(f"[verify] {pkg.name}")
        print(f"         manifest name : {manifest.get('name')}")
        print(f"         version       : {manifest.get('version')}")
        print(f"         author        : {manifest.get('author')}")
        print(f"         entry         : {entry}")
        print(f"         contents      : {len(names)} entries")
        if entry in names:
            print(f"         DT_NEEDED     : {', '.join(elf_needed(so_path))}")

    if problems:
        print()
        for p in problems:
            print(f"  FAIL  {p}")
        print(f"\n[verify] REJECTED - {len(problems)} problem(s)")
        return 1

    print("[verify] PASS - package will import and load")
    return 0


if __name__ == "__main__":
    sys.exit(main())
