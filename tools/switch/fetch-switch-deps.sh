#!/bin/sh
# Fetch the pinned Dawn (new-coke/dawn-switch) into extern/dawn-switch and apply this port's patches
# to Dawn, its third-party dependencies and Aurora. Safe to run again: applied patches are skipped.
# Adapted from new-coke/strikers (CC0).
set -e

PORT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
PATCHES="$PORT/platforms/switch/patches"

DAWN_REPO=https://github.com/new-coke/dawn-switch.git
DAWN_REF=8d6453c82de3c1511aa2993e53b2a5b148407900
DAWN_DEST="$PORT/extern/dawn-switch"
DAWN_DEPTH=50

AURORA_DAWN_REF=$(sed -n 's/^_aurora_dependency_version(AURORA_DAWN_REF "\([0-9a-f]*\)".*/\1/p' \
    "$PORT/extern/aurora/cmake/AuroraDependencyVersions.cmake")
DAWN_STAMP="$DAWN_REF $AURORA_DAWN_REF"

# apply_once <repo dir> <patch>: applies the patch unless the tree already has it.
apply_once() {
    if git -C "$1" apply --reverse --check --whitespace=nowarn "$PATCHES/$2" 2>/dev/null; then
        echo "==> $2 already applied"
    elif git -C "$1" apply --whitespace=nowarn "$PATCHES/$2"; then
        echo "==> applied $2"
    else
        echo "fetch-switch-deps.sh: $2 does not apply to $1" >&2
        exit 1
    fi
}

if [ -f "$DAWN_DEST/CMakeLists.txt" ] && [ "$(cat "$DAWN_DEST/.mp4-dawn" 2>/dev/null)" = "$DAWN_STAMP" ]; then
    echo "==> Dawn $DAWN_REF already in $DAWN_DEST"
else
    # A stale tree may hold changes not yet exported to platforms/switch/patches.
    if [ -e "$DAWN_DEST" ] && [ -n "$(ls -A "$DAWN_DEST" 2>/dev/null)" ] && [ "$MP4_FORCE_REFETCH" != "1" ]; then
        echo "fetch-switch-deps.sh: $DAWN_DEST exists but is not Dawn $DAWN_REF." >&2
        echo "Export any local changes, then rerun with MP4_FORCE_REFETCH=1 to replace it." >&2
        exit 1
    fi

    echo "==> fetching Dawn $DAWN_REF"
    rm -rf "$DAWN_DEST"
    mkdir -p "$DAWN_DEST"
    git -C "$DAWN_DEST" init -q
    git -C "$DAWN_DEST" remote add origin "$DAWN_REPO"
    git -C "$DAWN_DEST" fetch -q --depth "$DAWN_DEPTH" origin "$DAWN_REF"
    git -C "$DAWN_DEST" -c advice.detachedHead=false checkout -q FETCH_HEAD

    if ! git -C "$DAWN_DEST" merge-base --is-ancestor "$AURORA_DAWN_REF" HEAD 2>/dev/null; then
        echo "fetch-switch-deps.sh: $DAWN_REF does not build on Aurora's Dawn $AURORA_DAWN_REF" >&2
        exit 1
    fi

    echo "==> fetching Dawn's third-party dependencies"
    PY=python3
    command -v $PY >/dev/null 2>&1 || PY=python
    (cd "$DAWN_DEST" && $PY tools/fetch_dawn_dependencies.py --shallow)

    echo "$DAWN_STAMP" > "$DAWN_DEST/.mp4-dawn"
fi

apply_once "$DAWN_DEST" dawn-switch.patch
apply_once "$DAWN_DEST/third_party/abseil-cpp" abseil-switch.patch
apply_once "$DAWN_DEST/third_party/EGL-Registry/src" egl-registry-switch.patch
apply_once "$PORT/extern/aurora" aurora-switch.patch
apply_once "$PORT/extern/libco" libco-switch.patch
echo "==> ok"
