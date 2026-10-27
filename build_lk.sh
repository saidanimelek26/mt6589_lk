#!/usr/bin/env bash
#
# Build the MT6589 LK bootloader (ASUS project) from this standalone tree.
#
set -euo pipefail

TREE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LK_DIR="$TREE_DIR/bootable/bootloader/lk"
PROJECT="${PROJECT:-asus}"
TOOLCHAIN_BIN="${TOOLCHAIN_BIN:-$HOME/toolchains/arm-linux-androideabi-4.9/bin}"
TOOLCHAIN_PREFIX="${TOOLCHAIN_PREFIX:-arm-linux-androideabi-}"

log() { printf '[build_lk] %s\n' "$*"; }

[ -x "$TOOLCHAIN_BIN/${TOOLCHAIN_PREFIX}gcc" ] || {
    log "toolchain not found: $TOOLCHAIN_BIN/${TOOLCHAIN_PREFIX}gcc"; exit 1; }

cd "$LK_DIR"

log "tree      : $TREE_DIR"
log "project   : $PROJECT"
log "toolchain : $TOOLCHAIN_BIN/$TOOLCHAIN_PREFIX"

export LC_ALL=C
make "$PROJECT" TOOLCHAIN_PREFIX="$TOOLCHAIN_BIN/$TOOLCHAIN_PREFIX"

OUT="$LK_DIR/build/build-${PROJECT}"
mkdir -p "$TREE_DIR/artifacts"
cp -f "$OUT/lk" "$TREE_DIR/artifacts/"
cp -f "$OUT/lk.bin" "$TREE_DIR/artifacts/"
cp -f "$OUT/lk-no-mtk-header.bin" "$TREE_DIR/artifacts/"

log "artifacts:"
ls -l "$TREE_DIR/artifacts"
log "done. A built image is not a verified boot: see README.md."
