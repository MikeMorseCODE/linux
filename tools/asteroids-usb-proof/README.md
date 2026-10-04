# Asteroids ConfigFS USB proof

`init.c` is a static PID 1. It mounts devtmpfs, proc, sysfs and configfs,
waits indefinitely for a real directory in `/sys/class/udc`, creates one
ACM function, and retries configuration/binding until successful. It never
exits. No shell or dynamic libraries are required.

Expected USB identity: `1d6b:0104`, serial `USERSPACE-EXEC-OK`, manufacturer
`Asteroids`, product `Asteroids Userspace Proof`. ACM enumeration proves
userspace execution; the serial port does not provide a shell.

## Rebuild

From the kernel repository:

```sh
tools/asteroids-usb-proof/build-initramfs.sh
scripts/kconfig/merge_config.sh -m -O /home/morsecode/android-kernel/out/sm7635-mainline /home/morsecode/android-kernel/out/sm7635-mainline/.config tools/asteroids-usb-proof/required.config
make O=/home/morsecode/android-kernel/out/sm7635-mainline ARCH=arm64 LLVM=1 olddefconfig
make O=/home/morsecode/android-kernel/out/sm7635-mainline ARCH=arm64 LLVM=1 -j16 Image qcom/sm7635-nothing-asteroids.dtb
```

Keep `CONFIG_ARM64_APPENDED_DTB=y` and concatenate the resulting Image and
board DTB. Package as boot header v4 with command line `nokaslr`, matching
the existing temporary boot image. Built-in initramfs is required because
the appended tree replaces the bootloader FDT and loses external-initrd
addresses. Leave stock vendor_boot installed. Use only `fastboot boot`.

## Validation

```sh
cc -Wall -Wextra -Werror -o /tmp/test-asteroids-init tools/asteroids-usb-proof/test-init.c
/tmp/test-asteroids-init
```

The host test simulates a mount failure, five UDC polling delays and a failed
first bind. It checks IDs, serial, ACM link and permanent PID 1 lifetime.
It does not emulate ConfigFS or hardware. Static ELF inspection and archive
inspection are separate required checks.

## USB dependency review

The initial output config had appended DTB, Qualcomm clocks, RPMh, PDC,
interconnect and SMMU disabled. `required.config` restores the built-in
provider chain, as well as ConfigFS/ACM, SPMI, PMIC MFD and both eUSB2 drivers.
`PINCTRL_MSM` must be enabled before `PINCTRL_SM7635` survives olddefconfig.
DWC3 dual-role includes gadget support; `CONFIG_USB_ACM=m` is a host driver
and does not prevent the built-in gadget ACM function from working.

The board enables GCC, TLMM, USB and HS PHY, selects peripheral mode, and
supplies PHY rails through PM7550 RPMh regulators. Local drivers match these
compatibles and PM8550B repeater compatibility. Only a USB2 PHY is described,
so the board sets maximum-speed to high-speed and selects UTMI as PIPE clock.

The repeater supplies are now mapped to stock LDO7 (1.8 V) and LDO17
(3.104 V), and the PM7550 LDO2/LDO17 voltage types are corrected. Remaining
hardware uncertainties: upstream regulator inputs are not mapped; no USB
interconnect paths are described, so there are no explicit bandwidth votes;
Type-C CC handling is not described. Successful provider compilation is not proof of
UDC registration or electrical enumeration. No new persistent diagnostics
are used.

Stale source instrumentation was removed from head.S, early_map_kernel,
init/main.c, PMIC MFD, both eUSB2 PHYs and dwc3-qcom. The tracked changes in
those files were entirely probes; committed functional code was preserved.
Historical untracked backup files were left in place and are not built.

## Build and temporary boot result (2026-09-15)

Kernel Image and board DTB built successfully with Clang/LLVM. Existing
compiler warnings occurred; no build errors. All 51 fragment settings
survived olddefconfig. System.map confirms the PMIC, eUSB2, GCC, RPMh, DWC3
gadget and ConfigFS ACM code is linked into vmlinux. The embedded
`usr/initramfs_inc_data` matches the verified archive exactly.

Appended image checks passed: Image is 2 MiB aligned and its file length
equals `_edata - _text`; the board FDT begins at that exact offset.
Unpacking the final boot image reproduced the concatenated kernel/DTB
exactly. Header v4, `nokaslr`, and the external ramdisk were taken from
`firstboot-mainline-v4-before-exec-init.img`; PID 1 is supplied by the
built-in archive.

Artifact: `/home/morsecode/android-kernel/out/sm7635-mainline/asteroids-userspace-proof-boot.img`

SHA-256: `dd25731ad7e894575ec8497624553e17d88fb5d7996e1b5c29bfa98222163fa2`

`fastboot -s 00167158P003453 boot ...` completed successfully (10.796 s).
The phone disappeared from the host USB list, but repeated checks over
roughly two minutes did not show `1d6b:0104`. The user observed a frozen
boot logo. **Host enumeration has not been achieved.** This observation
does not distinguish an earlier kernel stall from PID 1 waiting for UDC.
Host dmesg access was denied. No partitions were flashed, stock vendor_boot
was untouched, and no persistent diagnostic experiments were performed.

The phone may need manual recovery to the bootloader before another
temporary test. Further fixes need live evidence about provider probing
or UDC registration; the frozen logo alone cannot identify the blocker.

## Targeted USB power correction (2026-09-15)

The live stock FDT (`phone/rooted-dump/live-stock.dtb`) resolves the repeater
phandles to PM7550 LDO7 and LDO17. Raw `fdtget` values confirm 1,800,000 and
3,104,000 microvolts. Stock LDO2 is 880,000–912,000 microvolts, LDO4 is
1,200,000 microvolts. All four built-DTB constraints match stock exactly.

Two PM7550 driver table entries were incompatible with those constraints:

| Regulator | Previous description | Corrected description | Consumer |
| --- | --- | --- | --- |
| LDO2 | PLDO, minimum 1.504 V | NLDO, 0.320–1.304 V | PHY vdd, 0.880–0.912 V |
| LDO17 | NLDO, maximum 1.304 V | PLDO, 1.504–3.544 V | Repeater vdd3, 3.104 V |

The regulator core rejects a constraint with no supported voltage
(`machine_constraints_voltage`, `drivers/regulator/core.c`). Therefore
LDO2 was a definite registration blocker before any USB signal testing.
The new LDO17 DT entry also requires its type correction. These corrections
affect PM7550 entries only. Stock mode thresholds (30 mA for LDO2 and 10 mA
for LDO17) also agree with the selected NLDO/PLDO descriptions.

The board now declares LDO7/LDO17 and links `vdd18-supply`/`vdd3-supply`
on the repeater. The built DTB was checked for both phandle connections,
and all four voltage constraints were checked against driver ranges.

### Saved configuration comparison

The config extracted from `firstboot-mainline-v4-before-exec-init.img` is
an earlier broad configuration, not a verified snapshot of the user's
latest deep-boot test. It has no built-in initramfs and both eUSB2 drivers
as modules. Unlike the pre-proof output config, it already has GCC/RPMh,
PDC, SMMU and Qualcomm pinctrl support. It also uses TZMEM SHMBRIDGE instead
of the current GENERIC mode and enables the GENI UART console. Those
differences remain candidates if the focused power fix does not enumerate;
they are not proven causes. Restoring the entire old config would remove
the current required built-in initramfs.

For this comparison, kernel configuration and built-in initramfs are
byte-identical to the first ConfigFS proof image. Only the regulator table
and board supply connections changed. Kernel/DTB build passed. Appended
DTB alignment and location checks passed.

Image: `/home/morsecode/android-kernel/out/sm7635-mainline/asteroids-userspace-proof-power-boot.img`

SHA-256: `d706cca3a5e97d5260658d1648f363fb0a3896d6ad625991dc4cab35f7d91561`

The power-only image booted with fastboot success (10.840 s), but did not
appear as `1d6b:0104`; the user again reported a frozen logo.

## RPMh parent power-domain correction

Further dependency tracing found `/soc@0/rsc@17a00000` has
`power-domains = <&cluster_pd>`, whose provider is the PSCI CPU idle domain
driver. `CONFIG_ARM_PSCI_CPUIDLE` was disabled in the proof configuration,
so no driver registered that domain. Platform probing attaches the domain
before invoking the RPMh driver's probe (`drivers/base/platform.c`), making
this a missing dependency ahead of RPMh clocks/regulators and USB.

Enabled `ARM_PSCI_CPUIDLE` and `ARM_PSCI_CPUIDLE_DOMAIN`, as in the saved
before-exec configuration. Kconfig also selects `CPU_IDLE_MULTIPLE_DRIVERS`,
`DT_IDLE_STATES` and `DT_IDLE_GENPD`. No other config values changed.
The corrected board DTB and built-in initramfs are byte-identical to the
power-only test. Build passed, all 54 fragment entries survived olddefconfig,
and System.map contains `psci_cpuidle_domain_probe` and
`psci_idle_init_domains`. Appended DTB packaging checks passed.

Image: `/home/morsecode/android-kernel/out/sm7635-mainline/asteroids-userspace-proof-domain-boot.img`

SHA-256: `4608d9b33beeb36f8077397a0faf80be358e88b8a4f6be3f0e85fb23552699eb`

The domain-fix image completed fastboot boot successfully (10.838 s), but
repeated host USB checks still did not show `1d6b:0104`.

## Repeater SPMI address correction

The raw live-stock DTB has `reg = <7 0>` on
`/soc/qcom,spmi@c42d000/qcom,pmiv0104@8`. Its unit name misleadingly says 8;
SPMI transactions use the `reg` property, which says 7. The child BCL
interrupts also use SID 7. The custom include had `reg = <8 SPMI_USID>`, so
it targeted a different slave. Renamed its node/label to `pmic@7` /
`pmiv0104_7` and corrected reg to 7. Repeater compatibility and register
base 0xfd00 are preserved. SPMI resource base addresses match stock (the
stock list orders cnfg first, but names identify the same regions).

This test changes the DTB only: kernel Image, configuration and built-in
initramfs hashes match the domain-fix image. Built DTB reports `7 0` on the
PMIC parent and retains `qcom,pm8550b-eusb2-repeater` on the PHY child.
Build and appended-DTB packaging checks passed.

Image: `/home/morsecode/android-kernel/out/sm7635-mainline/asteroids-userspace-proof-spmi-boot.img`

SHA-256: `663afc5bc39141d41abd93bf7cf2cc60111076af586b671979603841b024dc10`
