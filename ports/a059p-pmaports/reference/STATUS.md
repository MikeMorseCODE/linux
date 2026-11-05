# A059P bring-up status — 2026-11-02

Verified on the phone with the original UniXNU / Linux 7.3 test boot image:
- Linux 7.3.0 aarch64 BusyBox userspace via custom UEFI/UniXNU chain
- USB ACM console and USB ECM networking
- EFI framebuffer `/dev/fb0`, 1080×2392, 32 bpp, stride 4320
- Volume button GPIO events through `/dev/input/event0`
- Basic Alpine 3.24.3 userspace and framebuffer X11

Not yet verified:
- Independently booted Nura or postmarketOS initramfs
- pmbootstrap-built kernel package or installed rootfs
- Touchscreen, DRM/GPU, wireless, modem, battery/charging, suspend
- Internal storage integration, camera, audio or security policy

Known-good boot-image SHA256: `fdd1d848f2d6aedf251aa73fb5e27cddf14467cf244b26130c3afa81fb2b59b7`

The image is an Android boot header v4 wrapper. It carries a custom shim + Project Silicium UEFI + FAT16 UniXNU payload containing an EFI kernel, DTB, BusyBox initramfs and configuration. No generic Android boot.img generated from these pmaports files is known to work. Never claim postmarketOS boots until a new initramfs/rootfs is observed executing on hardware.
