#!/usr/bin/env bash
# build.sh - compile this LeviLaunchroid native mod and package as .levipack
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
CLEAN="${1:-}"
NDK="${ANDROID_NDK_HOME:-$ROOT/.cache/ndk/android-ndk-r28c}"
API="${ANDROID_API:-28}"
ABI="${ANDROID_ABI:-arm64-v8a}"
REFS="${LEVI_REFS_DIR:-$ROOT/refs}"
STUB_DIR="$ROOT/.preloader-stub"
CXX_HOST="${CXX_HOST:-g++}"

[ -d "$NDK" ] || { echo "!! NDK not found at $NDK (run ./scripts/bootstrap.sh)"; exit 1; }
[ -f "$ROOT/manifest.json" ] || { echo "!! $ROOT/manifest.json missing"; exit 1; }
command -v "$CXX_HOST" >/dev/null || { echo "!! host compiler '$CXX_HOST' not found"; exit 1; }

read -r ENTRY LIB_BASE VERSION MOD_NAME < <(python3 - "$ROOT/manifest.json" <<'PY'
import json, sys
m = json.load(open(sys.argv[1]))
entry = m["entry"]
lib = entry.split("/")[-1][:-3]
if lib.startswith("lib"):
    lib = lib[3:]
name = "".join(c for c in m.get("name", "Mod") if c.isalnum() or c in "-_") or "Mod"
print(entry, lib, m.get("version", "0.0.0"), name)
PY
)

if [ "$CLEAN" = "--clean" ]; then rm -rf "$ROOT/build" "$STUB_DIR"; fi

echo "=== [1/5] host config probe (ASan + UBSan) ==="
if [ -f "$ROOT/tools/probe_config.hpp" ]; then
  PROBE_TMP="$(mktemp -d)"
  trap 'rm -rf "$PROBE_TMP"' EXIT
  PROBE_BIN="$PROBE_TMP/config_probe"
  "$CXX_HOST" -std=c++20 -O1 -g \
    -fsanitize=address,undefined -fsanitize-address-use-after-scope \
    -fno-sanitize-recover=all \
    -DPL_CONFIG_NO_RUNTIME \
    -I"$ROOT/src" -I"$ROOT/tools" \
    -I"$REFS/preloader-android/include" \
    -I"$REFS/third_party/json/single_include" \
    -I"$REFS/third_party/pfr/include" \
    -I"$REFS/third_party/fmt/include" \
    -I"$REFS/third_party/magic_enum/include" \
    "$ROOT/tools/config_probe.cpp" -o "$PROBE_BIN"
  "$PROBE_BIN"
fi

configure() {
  cmake -G Ninja -S "$ROOT" -B "$ROOT/build" \
    -DLEVI_PRELOADER_INCLUDE="$REFS/preloader-android/include" \
    -DLEVI_THIRD_PARTY="$REFS/third_party" \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM="android-$API" \
    -DANDROID_STL=c++_shared \
    -DCMAKE_BUILD_TYPE=Release \
    -DLEVI_PRELOADER_STUB_DIR="$1" >/dev/null
}

echo "=== [2/5] compiling ($ABI, API $API) ==="
configure ""
ninja -C "$ROOT/build" -j"$(nproc)" 2>&1 | tail -20

SO="$ROOT/build/out/lib${LIB_BASE}.so"
[ -f "$SO" ] || { echo "!! build produced no $SO"; exit 1; }

echo "=== [3/5] relinking against libpreloader.so ==="
python3 "$HERE/gen_preloader_stub.py" "$SO" "$STUB_DIR" --ndk "$NDK"
configure "$STUB_DIR"
ninja -C "$ROOT/build" -j"$(nproc)" 2>&1 | tail -5

STRIP="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip"
[ -x "$STRIP" ] && "$STRIP" --strip-unneeded "$SO"

mkdir -p "$ROOT/packages"
OUT="$ROOT/packages/${MOD_NAME}-${VERSION}-${ABI}.levipack"
echo "=== [4/5] packaging ==="
python3 "$HERE/pack.py" "$ROOT" "$SO" "$OUT" >/dev/null
echo "       $OUT"

echo "=== [5/5] release gate ==="
python3 "$HERE/verify_pack.py" "$OUT"
