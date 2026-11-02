#!/usr/bin/env python3
"""
pack.py - build a LeviLaunchroid .levipack from a built mod directory.

Launcher requirements (verified in LeviLaunchroid/core/mods/FileHandler.java):
  * zip container, extension .levipack
  * exactly ONE .so entry
  * manifest.json at the zip root (or one shared parent folder)
  * manifest "entry" must point at that .so

Usage:
  pack.py <moddir> <built.so> <out.levipack>
"""
from __future__ import annotations

import json
import sys
import zipfile
from pathlib import Path


def main() -> int:
    moddir, sofile, out = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    manifest_path = moddir / "manifest.json"
    if not manifest_path.is_file():
        print(f"!! missing {manifest_path}")
        return 1
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    entry = manifest.get("entry", "")
    if not entry.endswith(".so"):
        print(f"!! manifest.entry must be a .so path, got {entry!r}")
        return 1
    if not sofile.is_file():
        print(f"!! missing built library {sofile}")
        return 1

    # Payload: everything the launcher consumes at runtime.
    files: list[tuple[Path, str]] = []
    files.append((manifest_path, "manifest.json"))
    files.append((sofile, entry))
    for sub in ("config", "resources"):
        d = moddir / sub
        if d.is_dir():
            for p in sorted(d.rglob("*")):
                if p.is_file():
                    files.append((p, p.relative_to(moddir).as_posix()))
    icon = manifest.get("icon")
    if icon and (moddir / icon).is_file():
        files.append((moddir / icon, icon))

    out.parent.mkdir(parents=True, exist_ok=True)
    if out.exists():
        out.unlink()
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for src, arc in files:
            z.write(src, arc)

    # Validate against the launcher's own rules.
    with zipfile.ZipFile(out) as z:
        names = z.namelist()
        so_count = sum(1 for n in names if n.lower().endswith(".so"))
        has_manifest = "manifest.json" in names
    print(f"[pack] {out.name}: {len(names)} entries, so={so_count}, manifest={has_manifest}")
    for n in names:
        print(f"       {n}")
    if so_count != 1 or not has_manifest:
        print("!! package would be REJECTED by the launcher import dialog")
        return 1
    print(f"[pack] OK -> {out} ({out.stat().st_size/1024:.1f} KiB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
