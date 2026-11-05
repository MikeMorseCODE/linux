# Nothing Phone (3a) Pro (A059P) — experimental postmarketOS device scaffold

**Not an installable postmarketOS port. Do not flash.**

This branch stages the independently verifiable **device package metadata** from our Stage 6.2 A059P bring-up. It is intentionally isolated from the Linux repository's default branch until the user creates a dedicated project repository.

## Verified on the phone, not with postmarketOS

Linux **7.3.0** successfully ran on the Nothing Phone (3a) Pro through a custom ARM64 shim → UEFI → UniXNU → EFI-stub kernel and BusyBox initramfs. Observed capabilities included `/dev/fb0` framebuffer 1080×2392, USB ACM/ECM and GPIO volume button events. Basic Alpine userspace and framebuffer X11 were tested. This does **not** mean that a postmarketOS rootfs booted.

SHA256 of the known-good `boot-3a-pro-linux73-console.img`:
`fdd1d848f2d6aedf251aa73fb5e27cddf14467cf244b26130c3afa81fb2b59b7`

## Package layout

The `device/testing/device-nothing-asteroidspro/` folder is a non-flashable packaging scaffold. It intentionally sets `deviceinfo_flash_method="none"`, `deviceinfo_generate_bootimg="false"`, and `deviceinfo_flash_kernel_on_update="false"`. No Linux kernel package is wired to it because the tested kernel is still bundled in a separate UniXNU payload.

## Safe host-side validation

Copy the device directory into a checkout of the current postmarketOS `pmaports` tree under `device/testing/` only if the destination does not exist. Then run on a host with pmbootstrap:

```sh
pmbootstrap checksum device-nothing-asteroidspro
pmbootstrap build -i device-nothing-asteroidspro
```

A metadata-only build is not an install or boot test. Do **not** use `pmbootstrap flasher`, `fastboot flash` or repartition commands based on this scaffold.

## What remains

A reproducible A059P kernel package, mainline EFI kernel/DTB provenance, genuine postmarketOS initramfs handoff through UniXNU, rootfs mount/pivot, and hardware tests. See `UPSTREAM-MR-DRAFT.md` before proposing a merge request.

Full local starter bundle with checksum checker, static selftest and read-only boot reference extractor: `a059p-pmaports-preupstream.zip` (distributed separately).
