#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAMP="${1:?usage: build_mlibc.sh STAMP}"
BUILD="$ROOT/mlibc/build"

if [ ! -f "$BUILD/build.ninja" ]; then
	meson setup "$BUILD" "$ROOT/mlibc" \
		--cross-file "$ROOT/cross_x86_64.ini" \
		-Ddefault_library=static -Dlibgcc_dependency=false
fi

ninja -C "$BUILD"
DESTDIR="$ROOT/sysroot" ninja -C "$BUILD" install
touch "$STAMP"
