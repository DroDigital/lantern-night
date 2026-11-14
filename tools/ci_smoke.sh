#!/usr/bin/env bash
# CI: run the smoke test where a GL 3.3 context can be created, otherwise --validate.
# Usage: tools/ci_smoke.sh <build_bin_dir>
set -u
BIN="$1"
OS="$(uname -s)"
USERDIR="${RUNNER_TEMP:-/tmp}/lk-smoke-user"
rm -rf "$USERDIR"; mkdir -p "$USERDIR"

run_validate() {
  echo "::notice::Smoke test skipped on $OS ($1); ran --validate instead."
  "$2" --validate
  exit $?
}

case "$OS" in
  Linux*)
    export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
    xvfb-run -a -s "-screen 0 1280x720x24" "$BIN/LanternKnight" --smoke-test --user-dir "$USERDIR"
    rc=$?
    ;;
  Darwin*)
    EXE="$BIN/LanternKnight.app/Contents/MacOS/LanternKnight"
    "$EXE" --smoke-test --user-dir "$USERDIR"
    rc=$?
    if [ $rc -eq 3 ]; then run_validate "no OpenGL 3.3 context on the runner" "$EXE"; fi
    ;;
  MINGW*|MSYS*|CYGWIN*)
    # Mesa llvmpipe (mesa-dist-win) dropped next to a copy of the test binary only - never packaged.
    SMOKE_DIR="$BIN/../smoke"
    rm -rf "$SMOKE_DIR"; mkdir -p "$SMOKE_DIR"
    cp "$BIN/LanternKnight.exe" "$SMOKE_DIR/"
    if [ -n "${MESA_DIR:-}" ] && [ -d "$MESA_DIR/x64" ]; then
      cp "$MESA_DIR"/x64/*.dll "$SMOKE_DIR/"
      echo "Mesa DLLs next to the smoke-test exe:"; ls "$SMOKE_DIR"
    else
      echo "::warning::MESA_DIR not usable: '${MESA_DIR:-}'"
    fi
    export GALLIUM_DRIVER=llvmpipe MESA_GL_VERSION_OVERRIDE=3.3
    "$SMOKE_DIR/LanternKnight.exe" --smoke-test --user-dir "$(cygpath -w "$USERDIR" 2>/dev/null || echo "$USERDIR")"
    rc=$?
    if [ $rc -eq 3 ]; then run_validate "no OpenGL 3.3 context" "$BIN/LanternKnight.exe"; fi
    ;;
  *) echo "unknown OS $OS"; exit 1 ;;
esac
echo "smoke exit code: $rc"
exit $rc
