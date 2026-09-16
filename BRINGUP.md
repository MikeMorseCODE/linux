# Asteroids first-shell bring-up

## Safety and scope

Temporary boot only: `fastboot boot ~/asteroids-firstboot/asteroids-boot.img`.
Never flash partitions, change slots, or relock. Stock backups must remain intact.
First milestone is persistent PID1, then an interactive shell; optional hardware is deferred.
Do not run `~/asteroids-clean-usb-build.sh` (it replaces diagnostic initramfs).

## Takeover baseline (2026-09-16)

- Branch: wip-next-20250417; inherited HEAD: 6910f3f61000a291d2eb22d19a4b63a2569dac6a.
- Inherited dirty source patch and original ramoops source saved under `bringup/probe-01/` before modification. Existing loose backups left untouched.
- Reported proven stage from handoff: kernel_init_freeable returned, built-in initramfs unpacked, /init accessible. Not independently reproduced this session. EL0 execution remains unproven.
- Baseline Image SHA256: 0b97034aff1b896603fbff9e8c738746ae9f89f4820eb11b440d55a4cbcaca22.
- Baseline DTB SHA256: bcb20efadd3bd28e078978ad9ea7b4c00a76794785e7494dae8685f4375c7416.
- Baseline boot.img SHA256: f3a77151590f54e1f35424a891a6966ab228999fa5478aaa1cab0797eaa49d3e.
- Config SHA256: 24123d1bcbc5617c3754efcea54d868d620bd2bb61171620dedc08cd9ec0e027.
- Verified linked Image bytes at __initramfs_start match usr/initramfs_inc_data and configured userspace-proof.cpio.gz exactly.
- Archive contains only root directory and executable static AArch64 /init, SHA256: 61f33ce7c62c9b115aaffaeb264427617a2f9f2a1c0e64da62ced150d2389313.
- Disassembly confirms write, repeated getpid and nanosleep, no exit. No /dev/console exists: console_on_rootfs cannot open it, so stdout is not trustworthy. Write length is 20 for an 18-byte message; a diagnostic defect, not evidence of an exec failure.
- Baseline Image size 23068672 equals ELF _edata - _text.
- DTS reserves stock console 0x81f20000+2MiB and uses 0x82120000+2MiB for custom console, mem-type=2, no-map. No DTS change this experiment.
- persistent_ram_write(prz, const void *, unsigned int) returns count. Console and pmsg use signature zero; header signature is DBGC. Stock format/ECC compatibility and retention still need runtime verification.
- Region is outside declared ordinary RAM. ram_core may use ioremap (Device mapping) rather than cached mem-type=2 vmap; do not infer cache behavior from DT alone.

## probe-01

Experiment: direct persistent_ram_write after successful console allocation, before pmsg setup and pstore_register. Marker: `ASTEROIDS RAMOOPS PROBE OK experiment=probe-01`.
Scoped to non-null console at configured physical base 0x82120000. No panic, reboot, raw RAM writes, or additional driver changes.
Existing exec printk markers retained. Successful pstore registration can capture subsequent exec markers; direct marker does not depend on registration.

Build/source commit: 2b25c67acc1293f5c37579f28d24c0c397fbfa17.
Image SHA256: 099d77cdadfa926e3d176b464f96b99d4cdaceec199b8b8a2f9bbf7829a5662c.
DTB SHA256: bcb20efadd3bd28e078978ad9ea7b4c00a76794785e7494dae8685f4375c7416.
Appended SHA256: 1ffe1b3a6986f537da86747e00f986c4f6de3bec118f882b64f1517bffe9f95b.
Build passed without compiler warnings. Verified Image size equals ELF span (23068672), DTB magic exactly at that boundary, embedded initramfs unchanged, direct marker present in Image.
Boot.img SHA256: 7efe82b12c86d31b8f26bcd920016d06d25e4d3a533343661ba72ebbf6db7c07.
Specified packaging script passed all content comparisons. Previous output backed up to /home/morsecode/asteroids-firstboot-backups/probe-01-20260916-075104.
Result: temporary fastboot boot completed: transfer OKAY (2.243 s), Booting OKAY (10.320 s), total 12.589 s. Phone disconnected from USB; no userspace or persistent marker yet observed. Awaiting stock pstore recovery.
Conclusion: bootloader accepted candidate; no new proven kernel/EL0 stage.
Next hypothesis: recover stock pmsg-ramoops after temporary boot to establish persistence and locate final exec marker. Absence alone cannot distinguish probe failure from retention/recovery incompatibility.

Packaging script inspected: recreates ~/asteroids-firstboot, generates boot and init_boot files, never flashes. Back up that output before invoking. Only boot.img is used for testing; built-in initramfs supplies /init.

Recovery safety finding: ~/asteroids-recover-and-pstore.sh contains `fastboot set_active a`; NEVER execute it under this task. Read pstore directly when stock ADB is available.

## probe-01 recovery

User observed Nothing logo and used established button reset. Rooted stock ADB recovered.
Stock kernel: 6.1.157-android14-11-g82d681c9b06b-ab14634535.
Recovered pmsg (6800 bytes) SHA256: 5d015de7283d4c6cbde9b6678b184bfa894a754670819331f2044e4f9294ad27.
Recovered console (2097136 bytes) SHA256: 8a09eacc7871b2c6f345e0a7605e270faef574cffba77ff98b9fa0cca229b99b.
Neither file contains ASTEROIDS or exec markers. Raw logs retained locally, excluded from commits because stock logs include personal information.
Stock live parameters confirm base 0x81f20000, size 4MiB, console/pmsg each 2MiB, ECC=0, record/ftrace=0, mem_type=0.
Conclusion: persistence unproven; absence does not prove ramoops_probe was never reached.

## probe-02

Hypothesis: match known stock mem_type=0 (write-combining/Normal noncached mapping) instead of 2. ram_core's non-RAM iomap branch treats any nonzero type as ioremap Device memory. This is a mapping discrepancy, not yet a demonstrated failure cause.
Change: only ramoops DT memory type and experiment marker; add direct pstore_register return marker. Preserve addresses, sizes, initramfs and all other hardware settings.
Backups: bringup/probe-02/*.before.
Build/source commit: 007d2e3cd355dea907d988e9d821e2796feeb646.
Boot.img SHA256: da3554db6eebdd35c15a1d47d499c04ac0fa6d9f2a2655ad9307ecd627703d6d.
Image/DTB/appended hashes and backup path: bringup/probe-02/artifacts.txt. Build passed without compiler warnings; Image span and DTB boundary verified, linked initramfs unchanged; packaging content comparisons passed.
Result: temporary fastboot boot transfer OKAY (2.241 s), Booting OKAY (10.270 s), total 12.525 s. Awaiting manual reset and stock pstore recovery; no new proven kernel stage.
Next: temporary boot and recover pmsg using rooted stock ADB after user's manual reset.

## probe-02 recovery

Rooted stock ADB returned after manual reset. No ASTEROIDS marker in either recovered file.
Pmsg: 727 bytes, SHA256 5cc89b007f5b579e5244e6798ed31e0df8b91b8c7896a047fa5ef1f2daf7eab8.
Console: 928121 bytes, SHA256 59d4f6cc1c475a4fbfd495370ee50a8dfdbca41a866775cc13771e67d665bac7.
Console contains previous stock kernel text with visibly corrupted characters. Matching stock mapping type did not establish telemetry; failure boundary still unknown.

## probe-03

Calibrate the independent aligned-I/O breadcrumb technique from prior experiments, alongside the upper-half ramoops API markers.
Writer checks machine compatibility, PID1, exact reserved stock-console guard node address/size before mapping its first 4KiB. Writes bounded 96-byte records with DBGC header using aligned writel stores and barriers. No normal ramoops driver owns this lower half in the current DTS. Stock console logs may be replaced by this intentional diagnostic; no persistent storage/partition writes.
Checkpoints: kernel_init_freeable entry, before/after do_basic_setup, ramoops probe entry, console allocation, pstore registration, before/after run_init_process and START_THREAD, first and 16th PID1 getpid syscall. Existing /init sleeps one second between getpid calls; 16 calls would establish repeated EL0 round trips and sustained PID1 execution, not shell availability.
Raw lower-half checkpoint and upper-half ramoops marker are independent; recover both stock pstore files. No forced panic/reboot or timing-based inference.
Backups: bringup/probe-03/*.before (local).
Source commit: 8e5c99aec106a756b68ed2986d1ffd9ceff8cb31.
Boot.img SHA256: cfd8ec0299a49e7e55cab5630ce95ab560d9b5787edf8182601a42af9b23dfe7.
Build and packaging succeeded; exact Image/ELF span and DTB boundary verified, embedded archive unchanged. Artifact hashes in bringup/probe-03/artifacts.txt. Recompiled binfmt_elf reveals three inherited printk format warnings (%lx versus u64); these are not build failures.
Result: temporary boot transfer OKAY (2.240 s), Booting OKAY (10.270 s), total 12.528 s. Awaiting stock recovery after >=20 seconds for PID1 loop checkpoints.
Next hypothesis: distinguish failure before device init, within ramoops, within exec, or missing observable userspace output.
