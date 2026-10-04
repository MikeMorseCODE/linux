#!/bin/sh
set -eu
src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
dest=${1:-/home/morsecode/asteroids-userspace-proof}
cc=${CROSS_COMPILE:-aarch64-linux-gnu-}gcc
mkdir -p "$dest/root/dev" "$dest/root/proc" "$dest/root/sys" "$dest/root/tmp"
"$cc" -static -Os -Wall -Wextra -Werror -o "$dest/root/init" "$src/init.c"
chmod 755 "$dest/root/init"
cp "$src/init.c" "$dest/init.c"
# Explicit list excludes unrelated files from older root directories.
(cd "$dest/root" && printf '%s\n' . dev proc sys tmp init |
    cpio --quiet -o -H newc --owner=0:0) > "$dest/userspace-proof.cpio"
gzip -n -c "$dest/userspace-proof.cpio" > "$dest/userspace-proof.cpio.gz"
gzip -dc "$dest/userspace-proof.cpio.gz" | cpio -itv
