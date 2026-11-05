# Proposed postmarketOS pmaports merge request — DRAFT ONLY

**Not submitted. Independent postmarketOS boot is not verified.**

Proposed title: `device-nothing-asteroidspro: initial non-flashable A059P mainline bring-up scaffold`

Target repository: https://gitlab.postmarketos.org/postmarketOS/pmaports, target branch `main` (confirm against latest upstream).

## Summary

Experimental metadata for the Nothing Phone (3a) Pro (A059P / AsteroidsPro). A known-good Linux 7.3 test image runs through a custom shim + UEFI + UniXNU, but this is *not* an independent postmarketOS installation. No native kernel package, postmarketOS initramfs boot path, or validated rootfs exists yet. Flashing intentionally disabled.

## Checklist

- [x] Preserve the exact known-good console image SHA256 and known hardware observations
- [x] Device package metadata staged with all flashing paths disabled
- [x] APKBUILD source checksums verified in saved Stage 6.2 host-side tests
- [ ] Current pmaports / pmbootstrap `checksum` and `build -i` successful on host
- [ ] Build and identify reproducible kernel, modules, matching DTB and EFI image
- [ ] Prove independent postmarketOS mkinitfs / Nura handoff and working rootfs
- [ ] Verify hardware and safety: storage, power, touchscreen, DRM, USB, charging, wireless
- [ ] Set a verified maintainer, metadata and package naming in line with pmaports conventions
- [ ] Seek review of whether incomplete metadata-only port meets upstream inclusion criteria

## Sources

- Deviceinfo: https://docs.postmarketos.org/pmaports/main/deviceinfo-reference.html
- Cmdline config: https://docs.postmarketos.org/pmaports/main/kernel-cmdline.html
- Current pmaports branch rename: https://postmarketos.org/edge/2026/03/15/pmaports-default-branch-changed/

**Do not submit until the package and scope meet upstream acceptance requirements.**
