#!/bin/sh
set -eu
BASE=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
APKBUILD="$BASE/device/testing/device-nothing-asteroidspro/APKBUILD"
grep -q '^pkgname=device-nothing-asteroidspro$' "$APKBUILD"
sh -n "$APKBUILD"
python3 -m py_compile "$BASE/a059p-port" "$BASE/scripts/extract-working-payload.py"
python3 "$BASE/a059p-port" verify
printf 'PASS: Stage 6.2 host-side static checks\n'
