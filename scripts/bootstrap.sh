#!/usr/bin/env bash
# bootstrap.sh - fetch external header dependencies and Android NDK r28c.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${LEVI_ROOT:-$(cd "$HERE/.." && pwd)}"
REFS="$ROOT/refs"
mkdir -p "$REFS/third_party" "$ROOT/.cache/ndk"

fetch() {
  [ -d "$1" ] && { echo "ok   $1"; return; }
  echo "get  $1"
  timeout 300 git clone --depth 1 --branch "$3" "$2" "$1" >/dev/null 2>&1 || \
    timeout 300 git clone --depth 1 "$2" "$1" >/dev/null 2>&1
}

fetch "$REFS/preloader-android"      https://github.com/LiteLDev/preloader-android.git main
fetch "$REFS/third_party/fmt"        https://github.com/fmtlib/fmt.git 11.2.0
fetch "$REFS/third_party/json"       https://github.com/nlohmann/json.git v3.11.3
fetch "$REFS/third_party/pfr"        https://github.com/boostorg/pfr.git boost-1.88.0
fetch "$REFS/third_party/magic_enum" https://github.com/Neargye/magic_enum.git v0.9.7

NDK="${ANDROID_NDK_HOME:-$ROOT/.cache/ndk/android-ndk-r28c}"
if [ ! -d "$NDK" ]; then
  echo "get  Android NDK r28c"
  (
    cd "$ROOT/.cache/ndk"
    curl -L --retry 3 -o ndk.zip https://dl.google.com/android/repository/android-ndk-r28c-linux.zip
    unzip -q ndk.zip && rm -f ndk.zip
  )
fi
echo "ok   $NDK"
command -v cmake >/dev/null || echo "!! install cmake & ninja (e.g. apt install cmake ninja-build)"
echo "bootstrap complete"
