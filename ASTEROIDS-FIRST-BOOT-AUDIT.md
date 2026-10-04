# Asteroids first-boot audit

Date: 2026-09-15. Scope: local source, Git state and existing output only. No build, source modification, commit, device access or flash was performed. This report is the sole created file.

## Executive summary

**NOT FIRST-BOOT READY.** The working source and existing Image contain an unconditional branch to itself in `preserve_boot_args`. Every normal primary-entry boot stops before DT parsing, memory setup, earlycon, pstore and init. This is a definite blocker, not an inferred driver issue.

After that blocker, the board disables GCC and TLMM while enabling consumers. Normal GENI UART cannot acquire its clocks/pins; USB cannot acquire its GCC clocks, reset and power domain. The supplied DTB has zero-sized RAM and needs bootloader fixups. The built-in initramfs contains no executable init. Neither simplefb nor simpledrm is enabled. UFS has no controller/PHY node. There is no demonstrated working interactive debug channel.

The USB tuning changes plausibly reproduce board-specific settings, but their necessity is unproven: stock register data and successful comparative boots are absent. They cannot solve the earlier entry/provider failures.

## Repository/build state

- Branch: `wip-next-20250417`.
- HEAD: `6910f3f61000a291d2eb22d19a4b63a2569dac6a`, `Reapply "Apply ARM64_APPENDED_DTB patch"` (committed 2025-04-21).
- Source version: **6.15.0-rc2**, `Makefile:2-5`.
- Output release: `6.15.0-rc2-next-20250417-g6910f3f61000-dirty`, output `include/config/kernel.release:1`.
- Compiler: `clang version 22.1.8, LLD 22.1.8`, output `include/generated/compile.h:4`; `.config:5` independently records Clang 22.1.8.
- No staged diff. Four tracked files modified: 44 insertions, 7 deletions. Initial status follows (before creating this report).

```text
 M arch/arm64/boot/dts/qcom/sm7635-nothing-asteroids.dts
 M arch/arm64/kernel/head.S
 M drivers/phy/qualcomm/phy-qcom-eusb2-repeater.c
 M drivers/phy/qualcomm/phy-qcom-snps-eusb2.c
?? arch/arm64/boot/dts/qcom/sm7635-nothing-asteroids.dts.before-nothing-tuning-final
?? arch/arm64/boot/dts/qcom/sm7635-nothing-asteroids.dts.before-nothing-usb-tuning
?? arch/arm64/boot/dts/qcom/sm7635-nothing-asteroids.dts.before-usb-gadget
?? arch/arm64/kernel/head.S.before-pba-entry-canary
?? arch/arm64/kernel/head.S.before-psci-canary
?? asteroids-region-source.txt
?? drivers/phy/qualcomm/phy-qcom-eusb2-repeater.c.before-nothing-tuning
?? drivers/phy/qualcomm/phy-qcom-snps-eusb2.c.before-downstream-sequence
?? drivers/phy/qualcomm/phy-qcom-snps-eusb2.c.before-nothing-tuning
?? init/main.c.before-reboot-canary
```

| Artifact | Source-tree path exists | Existing output |
|---|---|---|
| `arch/arm64/boot/Image` | No | Yes, 39,845,888 bytes; Sep 14 03:41 |
| `arch/arm64/boot/dts/qcom/sm7635-nothing-asteroids.dtb` | No | Yes, 44,989 bytes; Sep 14 01:50 |

Output root: `/home/morsecode/android-kernel/out/sm7635-mainline`. Times above are filesystem display times, not proof of reproducibility. `.config` is dated Sep 14 03:10. Existing DTB was decoded in memory using dtc: it has the debug UART compatible/enables, peripheral USB, disabled GCC/TLMM and zero-sized RAM described below. No full source-to-artifact rebuild was performed.

`vmlinux` disassembly at `0xffff800081d4a524 <preserve_boot_args>` is `14000000 b <preserve_boot_args>`. The raw Image at offset `0x1d4a524` contains bytes `00 00 00 14`, confirming the same self-branch in the bootable binary. The presence of these artifacts is not a successful-build/boot certification for all current source changes.

The output `usr/initramfs_data.cpio` was parsed read-only: entries are `dev`, `dev/console`, `root`, `TRAILER!!!`; no `/init` or shell. No `.ko` files or top-level `modules.order` were found in this output. Thus `=m` below means configured as a module, **not presently available from this output**.

## Uncommitted-change analysis

### Kernel entry — definite global blocker

`arch/arm64/kernel/head.S:85-87` calls `preserve_boot_args` immediately after recording MMU state. Added lines 172-174 (`ASTEROIDS PBA ENTRY CANARY`, `991: b 991b`) loop forever. This is silent and does not reboot or record a canary value. It blocks all later bring-up and is already in the existing Image. The surrounding appended-DTB code at lines 180-195 is committed baseline code, not this diff; it looks for FDT magic at `_edata` and can replace the bootloader x0 pointer. Packaging and memory-fixup behavior must be checked against this nonstandard path.

### Board DTS

`arch/arm64/boot/dts/qcom/sm7635-nothing-asteroids.dts:285-287`: changes `dr_mode` from `otg` to `peripheral` and removes `usb-role-switch`. This selects a fixed device role without waiting for a role-switch provider. It is a reasonable first-boot gadget simplification because no PMIC GLINK/connector graph is configured. It does not supply Type-C attach/CC handling, power, PHY readiness or a gadget function.

Lines 302-310 enable QUP wrapper 0 and UART5, adding `qcom,geni-debug-uart` before the generic compatible. This connects the existing `serial0`/stdout alias to the earlycon match and normal console driver. It is relevant to UART debugging, not required for USB electrical operation. GCC remains disabled (196); TLMM remains disabled (254), so normal UART probing remains blocked.

### eUSB2 repeater

`drivers/phy/qualcomm/phy-qcom-eusb2-repeater.c:85-93` changes the common PM8550B initialization table:

| Field | Before | After |
|---|---|---|
| IUSB2 / transmit amplitude | 8 | 6 |
| HSDISC / disconnect threshold | 0 (implicit) | 1 |
| SQUELCH_U | 3 | 5 |
| USB2_SLEW | 0 (implicit) | 3 |
| USB2_PREEM | 5 | 6 |
| EUSB_SLEW | 0 (implicit) | 3 |

These values are written in `eusb2_repeater_init`, lines 154-166. They affect **all users of the PM8550B table**, not only Asteroids or peripheral mode. Amplitude, disconnect and pre-emphasis can already be overridden through DT properties (144-146). The board selects this table via `sm7635-pmics.dtsi:34-38`, despite the hardware label being PMIV0104. Matching by PM8550B compatibility does not prove electrical equivalence. The “stock” comment is a provenance assertion, not corroborating evidence.

The repeater requests `vdd18` and `vdd3` (81-82, 115-128), but its node has neither supply. This may use dummy regulators and depend on bootloader-powered rails; it does not establish power control. The local binding does not require those properties (`Documentation/devicetree/bindings/phy/qcom,snps-eusb2-repeater.yaml:58-63`). Treat this as a hardware dependency gap, not a guaranteed schema failure.

### Synopsys eUSB2 PHY

`drivers/phy/qualcomm/phy-qcom-snps-eusb2.c` adds:

1. A 10 us delay after asserting POR (270-277).
2. A full-register write of zero, then readback, to offset `0x58` / `USB_PHY_CFG_CTRL_1` (323-329), after default parameters. This overrides the earlier PLL charge-pump-bias masked setting (293-295) and clears every bit of that register, not just the defined bias field (`33-34`).
3. Clearing `CMN_CTRL_OVERRIDE_EN` in `USB_PHY_CFG0` after initialization (348-355).

All three execute for every device bound to this driver; the match table uses the SM8550 fallback (446-448), not a board-specific quirk. The readback orders/completes the access but does not validate a PLL lock or successful link. The delay/override release may be required by the downstream sequence, and the zero write may be required by this board, but the local comments alone cannot establish either claim. Do not infer necessity from matching numeric tuning. Obtain the actual stock DT sequence and downstream implementation, then compare behavior after basic providers work.

Reference-clock check: board XO is 76.8 MHz (DTS 298-300); SM7635 RPMh maps to the SM8650 clock table (`drivers/clk/qcom/clk-rpmh.c:950`), whose CXO is divided by two (`clk-rpmh.c:653-655`): the configured PHY reference is therefore 38.4 MHz, a supported rate. The PHY accepts only 19.2/38.4 MHz (`phy-qcom-snps-eusb2.c:190-230`). Check the actual divided rate; the board XO value alone is not evidence of an unsupported 76.8 MHz PHY input.

### Untracked files

The `.before-*` files listed in status are snapshot copies of the DTS, PHY drivers, head.S and init/main.c. They are not the canonical source paths built by Kbuild. `init/main.c` itself has no tracked diff. Their names suggest prior debugging/tuning checkpoints, not active patches. `asteroids-region-source.txt:1-12` contains a stock-property excerpt identifying `AsteroidsProEEA`, `ProEEA`, `volcano`, EEA; the bootconfig/cmdline/chosen sections are empty. This conflicts with assuming all evidence describes the non-Pro Phone (3a): board model says Phone (3a), but OEM ID is 2 and the inline comment assigns 2 to Pro (DTS 36-52). Resolve SKU before trusting splash geometry or DT selection IDs.

## Subsystem matrix

Notation: **B** = `arch/arm64/boot/dts/qcom/sm7635-nothing-asteroids.dts`; **S** = `arch/arm64/boot/dts/qcom/sm7635.dtsi`; **P** = `arch/arm64/boot/dts/qcom/sm7635-pmics.dtsi`. Line numbers refer to audited working files. Full Kconfig line references are in the appendix.

READY means statically complete for the stated role, not hardware-tested. PARTIAL means at least one dependency/configuration/validation gap. DISABLED means explicitly disabled in DT or required configuration. MISSING means no functional node in the complete board include closure. UNKNOWN means evidence is insufficient. For absence, cited lines identify the closest declarations; the entire include closure was searched, not just those lines. A reserved carveout or clock provider is not a device node. “Driver exists” refers to this upstream-derived tree; it does not assert that local SM7635 additions are merged in current upstream Linux.

| Subsystem | Status | Driver → SoC → board → configuration and consequence |
|---|---|---|
| /chosen | PARTIAL | S:33; B:59-75 supplies bootargs/FB/stdout. No initrd bounds in static tree; external initramfs needs handoff. Memory placeholder S:435-438 needs fixup. |
| stdout-path | READY | B:54-65 resolves serial0 to UART5 at S:1618. Correct static reference; endpoint usability remains PARTIAL below. |
| GENI/debug UART | PARTIAL | `drivers/tty/serial/qcom_geni_serial.c:1543-1544`; S:1302-1321,1618-1636; B:302-310 enables wrapper/UART, console built-in. GCC/TLMM disabled. Pins GPIO25/26, qup0_se5 (S:2346-2351); physical routing unverified. |
| Early console | PARTIAL | Same driver:1490-1544 supports qcom_geni debug compatible; B:65,308 selects it; SERIAL_EARLYCON and GENI_CONSOLE=y. Needs preconfigured clocks/pins/firmware and accessible MMIO at 0xa94000. Entry loop prevents reaching it. |
| simple-framebuffer | DISABLED | `drivers/video/fbdev/simplefb.c:669` match exists; B:67-74 configured, but FB_SIMPLE=n and DRM_SIMPLEDRM=n. Node unit address e1000000 disagrees with reg e3940000. fbcon=y alone cannot bind it. |
| reserved-memory | PARTIAL | S:517-710 extensive firmware carveouts; B:30-33 shrinks trusted_apps to end 0xeea00000; B:103-134 adds ramoops/splash/rmtfs. Stock map/SKU and bootloader allocations unverified; RAM size is zero until fixed. |
| ramoops/pstore | PARTIAL | B:108-118, `fs/pstore/ram.c:690-693,787-801`; PSTORE/RAM/CONSOLE=y. 4 MiB at 0x81f20000 split 2 MiB console + 2 MiB pmsg; PMSG=n. No record-size, no remaining dmesg-record space. Console retention possible after driver initialization, not at the entry loop. |
| UFS storage | MISSING | Generic `drivers/ufs/host/ufs-qcom.c:2073` exists, UFS_QCOM=y, QMP_UFS=m. S:78-97 symbol clocks and S:1705 ICE do not instantiate UFS controller/PHY. No board supplies/reset/lanes. Cannot mount a UFS root with this DT. |
| USB controller | PARTIAL | `drivers/usb/dwc3/dwc3-qcom.c:917` matches fallback qcom,dwc3; S:1875-1955; B:281-287 enables peripheral; DWC3/QCOM/GADGET=y. GCC clocks/reset/GDSC missing at runtime; SMMU SID 0x40; no bandwidth paths. |
| USB2/eUSB2 PHY | PARTIAL | S:1860-1872, B:289-295, P:28-38; both PHY drivers=y. SoC SM8550 fallback matches; board supplies ldo2/ldo4 plus repeater. GCC reset disabled, repeater supplies absent, tuning unvalidated. No USB3 PHY configured. |
| Type-C / PMIC GLINK | MISSING | `drivers/soc/qcom/pmic_glink.c:383` generic match exists; QCOM_PMIC_GLINK=m, TYPEC_QCOM_PMIC=m. No connector/GLINK node; USB endpoint empty (S:1948-1953); fixed peripheral B:285-287. Bootloader Type-C state cannot be presumed. |
| Regulators | PARTIAL | B:136-180 fixed VPH and PM7550 RPMh ldo2/4; `drivers/regulator/qcom-rpmh-regulator.c:1694-1695` matches, fixed/RPMh built-in. Parent supply wiring is only comments (B:153-165); repeater rails absent. PMIC identity/resources require stock confirmation. |
| GPIO/pinctrl | DISABLED | TLMM S:2055-2070; B:248-268 disables it. `drivers/pinctrl/qcom/pinctrl-sm7635.c:1422-1428` exists, PINCTRL_SM7635=y. PMIC GPIO providers P:17-25 remain enabled and their driver=y; this does not replace TLMM pins. |
| RPMh | PARTIAL | S:552-557 cmd-db,2620-2660 RSC/BCM/clock/PD nodes; B:148-180 regulators. `drivers/pmdomain/qcom/rpmhpd.c:747`, `drivers/clk/qcom/clk-rpmh.c:950` match; RPMH, COMMAND_DB, RPMHPD, CLK_RPMH=y. Firmware cmd-db resources/acknowledgements untested. |
| Clocks | DISABLED | GCC S:816-833, `drivers/clk/qcom/gcc-sm7635.c:3129-3131`, SM_GCC_7635=y; B:196 disables it. B:199-202 disables camera/display/video/GPU clocks, those SM7635 drivers also n. RPMh/fixed/TCSR remain described, but cannot replace GCC. |
| Interconnects | MISSING | `drivers/interconnect/qcom/sm7635.c:1330-1550` contains fabric descriptions and config=y; S has BCM voter (2642-2644) but no fabric provider nodes. UART explicitly TODO at S:1627; USB S:1875-1955 has no paths. Missing optional paths may permit probe, but no Linux bandwidth voting is established. |
| SMMU/IOMMU | PARTIAL | S:2408-2516 apps_smmu with generic MMU-500 fallback; S:1922 USB iommus. `drivers/iommu/arm/arm-smmu/arm-smmu.c:1950` provides the match table; ARM_SMMU/QCOM=y, default bypass disabled. Mapping/stream IDs/firmware ownership untested; a functioning IOMMU path is needed for USB DMA. |
| Remoteproc | MISSING | `drivers/remoteproc/qcom_q6v5_pas.c:1437-1504` has a match table but no SM7635 entry; generic driver exists, Q6V5_PAS=m; no remoteproc devices/firmware-names in S or B. S:713-806 SMP2P nodes disabled. Reserved memory is insufficient. |
| ADSP | MISSING | S:636-645 DTB/adspslpi carveouts; SMP2P disabled S:726. Generic Q6V5_ADSP/PAS=m, no ADSP node or firmware wiring. |
| CDSP | MISSING | S:656-665 carveouts, SMP2P disabled S:754. Generic PAS=m, no CDSP remoteproc. |
| MPSS/modem | MISSING | S:626-635 carveouts, SMP2P disabled S:782; B:126-133 rmtfs region (RMTFS_MEM=m). Q6V5_MSS/PAS=m does not supply a modem node. |
| WPSS | MISSING | S:646-650 carveout only; no WPSS remoteproc or signaling node. Generic remoteproc stack configured as modules, no verified SM7635 firmware match. |
| GPU | MISSING | S:676-680 microcode memory,1752-1764 GPUCC; B:202 disables GPUCC. DRM_MSM=m, but no Adreno GPU/IOMMU/firmware/OPP node. |
| Display | PARTIAL | Only inherited splash path B:67-74,120-124; S:1841-1857 DISPCC disabled B:200. DRM_MSM=m but no MDSS/DPU/DSI/panel graph. Splash driver disabled; no Linux scanout control. |
| Input keys | PARTIAL | B:77-101 gpio-keys, KEYBOARD_GPIO=y; AI pin depends on disabled TLMM, including shared default pinctrl, potentially preventing the whole device probing. Power/resin explicitly disabled in `pmk8550.dtsi:33-43`. Volume-up PMIC GPIO configured B:230-238; touchscreen is comment B:222-224. |
| Glyph LEDs | MISSING | B:212-216 only aw20036 comment under disabled i2c7. `drivers/leds/leds-aw200xx.c:649` has exact chip match; LEDS_AW200XX=n; no LED channels/power graph. |
| Wi-Fi | MISSING | S:686-690 wlan_msa reserve plus WPSS reserve only. `drivers/net/wireless/ath/ath11k/ahb.c:24` has the generic match table, ATH11K_AHB=m; no Wi-Fi node, power sequence or board firmware. |
| Bluetooth | DISABLED | S:1132-1151 UART11 and wrapper S:877-894 disabled. B:273-279 adds interrupt and comment only, no Bluetooth child. `drivers/bluetooth/hci_qca.c:2705` has the QCA match table; BT_HCIUART=m, QCA=y; rails/enable/firmware missing. |
| Audio | MISSING | S:24-25 includes sound bindings,636-645 ADSP reserve only; no sound card/LPASS/codec graph. `sound/soc/qcom/sm8250.c:208` is an example Qualcomm machine-driver match table, not an Asteroids match, SND_SOC_QCOM=m; no verified Asteroids machine configuration. |
| Camera | MISSING | S:681-685 camera reserve,1828-1838 CAMCC; B:199 disables CAMCC; B:256-260 is a pin comment. `drivers/media/platform/qcom/camss/camss.c:3864-3874` has supported-SoC matches, VIDEO_QCOM_CAMSS=m; no sensors/CSI/ISP graph. |

## First-boot dependency chain and likely blockers

1. **Bootloader → kernel entry (BLOCKED):** authentication/container/header/load address must first be accepted; that is UNKNOWN from this tree. Once `primary_entry` runs, the canary loop definitively stops it (head.S:85-87,170-174).
2. **Kernel entry → usable DTB/RAM (PARTIAL):** ARM64_APPENDED_DTB=y changes DT selection. S:435-438 encodes memory base 0x80000000 and size zero, also present in the DTB. Bootloader must populate actual RAM banks and preserve reservations. If an appended pristine DT supersedes a fixed-up x0 DT, memory/initrd fixups can be lost. Confirm which DT reaches Linux. Correct DTB location/alignment and no image/initrd/reservation overlap are essential.
3. **Clocks/regulators (PARTIAL/BLOCKED for I/O):** fixed clocks → RPMh cmd-db/RSC → RPMh clocks/PD/regulators → GCC/reset/GDSC → consumers. GCC is disabled. `clk_ignore_unused`, `pd_ignore_unused`, `regulator_ignore_unused` only affect unused-resource shutdown; they do not register missing providers or satisfy phandles. CPU execution may continue using firmware state, but normal UART/USB cannot rely on this.
4. **Interrupts/timers (statically READY, untested):** S:2518-2539 GICv3/ITS, S:2756-2764 architectural timer, S:452-515 PSCI SMC. ARM_GIC_V3/ITS, ARM_ARCH_TIMER, ARM_PSCI_FW=y. Firmware execution level, counter frequency, CPU release and interrupt ownership remain boot assumptions. No definite local failure identified here.
5. **Console/debugging (PARTIAL):** earlycon can precede ordinary driver probes if UART hardware is already usable; otherwise normal UART needs GCC/TLMM fixed. Pstore cannot diagnose the present pre-initialization loop. Framebuffer drivers are absent. USB shell requires the complete USB path and userspace gadget setup.
6. **UFS and/or initramfs (BLOCKED unless externally supplied):** UFS is absent. Prefer an external self-contained AArch64 initramfs for the initial shell. BLK_DEV_INITRD and decompressors=y; embedded INITRAMFS_SOURCE is empty and the existing archive has no init. A bootloader-supplied ramdisk may solve this, but none is established by audited data.
7. **init (UNKNOWN/external dependency):** provide executable `/init`, a static shell (or all ELF interpreter/libraries), device nodes or devtmpfs mounting, and console I/O. `init/main.c:1496-1530` runs init/fallbacks, and 1592-1593 handles absent ramdisk init. The present archive cannot satisfy this. For initramfs, `/init` should explicitly mount devtmpfs; DEVTMPFS_MOUNT=y alone is not an initramfs setup script.

Additional USB defect to resolve: S:1879-1880 supplies FOUR cells per GIC specifier in `interrupts-extended`, but the GIC declares three (S:2526). The trailing zero is an invalid subsequent phandle in the flattened list and can break parsing of following wake interrupts. The DWC3 core's separate three-cell interrupt at S:1920 does not validate the wrapper list. This is independent of PHY tuning.

Additional board-data uncertainties: all four PM8550VS instances at USIDs 2/3/4/6 are inherited from `pm8550vs.dtsi:95-189`, while B:23-27 lists only one PM8550VS at 2. Verify actual PMIC topology rather than assuming every enabled inherited node exists. The 32 kHz sleep clock (B:244-246) and splash location/format likewise require stock corroboration.

## Debugging strategy

1. **GENI earlycon first, conditional on physical UART access.** Existing `earlycon` plus debug compatible selects the implementation at `qcom_geni_serial.c:1543`. An explicit candidate is `earlycon=qcom_geni,0xa94000`; normal debug UART console is `console=ttyMSM0,115200n8`. Confirm bootloader pinmux, SE firmware/protocol, baud clock and actual test-point routing before treating lack of text as lack of entry. Earlycon is a logging channel, not by itself a userspace shell. Nothing here bypasses the entry loop.
2. **Normal GENI UART** becomes the simplest interactive initramfs channel after GCC/TLMM dependencies are satisfied. Preserve logs across the early-to-normal console transition and confirm the final bootargs actually received.
3. **Ramoops** is the best persistent channel already configured, once kernel/driver initialization is reached. Console area is 0x81f20000-based with total reserved span ending 0x82320000. Check firmware retention/stock reservation and read pstore on a subsequent working boot. PMSG=n wastes the allocated pmsg purpose; absent record-size means no dedicated panic records. Do not expect evidence from the current entry spin.
4. **Framebuffer** needs a future config change to a matching built-in driver and verified stock splash geometry/address/format. Existing fbcon cannot draw by itself. Firmware must keep scanout/panel clocks and rails alive; no native display takeover is configured.
5. **USB ACM gadget** is a later interactive fallback: RPMh/regulators/SPMI → GCC reset/GDSC/clocks → repeater/eUSB2 → SMMU → QCOM wrapper/DWC3 UDC → configfs ACM → initramfs shell on ttyGS0. Required gadget components are built-in, but `/init` must mount configfs, create descriptors/configuration/ACM function, bind an available UDC and run a shell/getty. USB_ACM=m is the host-side driver, not the device-side ACM solution; USB_G_SERIAL=n means no automatic legacy serial gadget. No configured function means no enumeration even if the UDC probes. Type-C attachment and VBUS must also be established.

## Boot-image information still needed from stock Nothing OS

- Exact Phone (3a) versus Pro hardware/SKU/region, SoC and PMIC IDs, DTB/DTBO matching tuples; reconcile ProEEA excerpt with requested device.
- Offline stock boot/vendor_boot/init_boot/dtbo/vbmeta headers and unpacked metadata: header versions, compression, page/alignment requirements, DTB placement, load addresses/offsets, ramdisk fragments, bootconfig and authenticated boot requirements. Their availability/layout is UNKNOWN here; do not assume a specific Android boot-header generation.
- Actual stock `/proc/cmdline`, bootconfig, and final bootloader-patched DT `/chosen`, memory banks and reserved-memory. Existing text capture has empty sections for these.
- Kernel entry register/handoff contract, appended versus x0 DT choice, initrd start/end and whether both paths receive fixups.
- Stock UART engine/pins/SE protocol and baud source; physical accessible debug connection.
- Stock splash base, size, stride, pixel format and display retention behavior for the exact SKU.
- Stock PMIC rail mapping/voltages and USB controller/PHY/repeater compatible data, tuning sequence, timing, clock rates and Type-C/CC/role mechanism; downstream code that interprets the override pairs.
- Stock ramoops extent/format, firmware preservation and reset behavior. Firmware files and UFS wiring are needed for later storage/remoteproc bring-up, not for a self-contained initial RAM shell.

## Prioritized next steps (recommendations only; not executed)

1. Remove the entry spin in a separately authorized implementation task, then rebuild and verify the resulting entry instructions. Do not use the currently audited Image for a shell attempt.
2. Resolve SKU and bootloader DT/memory/initrd handoff. Verify that the actual selected DT has nonzero RAM and preserves all firmware reservations.
3. Prepare a self-contained initramfs with working `/init` and shell; validate archive contents and image packaging offline.
4. Restore and validate minimal RPMh/GCC/TLMM provider paths and UART routing. Establish one reliable log channel before enabling broad peripherals.
5. Validate reserved-memory against stock; make ramoops allocation/configuration coherent. Consider framebuffer only with verified stock splash and a built-in driver.
6. Correct USB wrapper interrupt encoding and verify regulators, reset, reference clock, IOMMU and Type-C attach. Then assess the tuning changes individually with stock-source evidence and enumeration logs; scope eventual board-specific tuning appropriately.
7. Provide configfs ACM setup in initramfs and verify UDC/ttyGS0 readiness before expecting a USB shell.
8. Add UFS later if persistent storage is needed; leave remote processors, GPU/display takeover, camera/audio/radios out of the minimal dependency chain until basic boot is repeatable.

## Generated configuration evidence

All references below are to the existing output `.config`, not merely defconfig or `arch/arm64/configs/sm7635.config`. `y` means linked-in by configuration, `m` means module selected, `not set` means unavailable. Module files were not found, so a module cannot be assumed usable in the first initramfs.

```text
5: CONFIG_CC_VERSION_TEXT="clang version 22.1.8"
218: CONFIG_BLK_DEV_INITRD=y
219: CONFIG_INITRAMFS_SOURCE=""
220: CONFIG_RD_GZIP=y
221: CONFIG_RD_BZIP2=y
222: CONFIG_RD_LZMA=y
223: CONFIG_RD_XZ=y
224: CONFIG_RD_LZO=y
225: CONFIG_RD_LZ4=y
226: CONFIG_RD_ZSTD=y
572: CONFIG_ARM64_APPENDED_DTB=y
574: CONFIG_CMDLINE=""
956: CONFIG_BINFMT_ELF=y
1594: CONFIG_BT_HCIUART=m
1595: CONFIG_BT_HCIUART_SERDEV=y
1596: CONFIG_BT_HCIUART_H4=y
1597: # CONFIG_BT_HCIUART_NOKIA is not set
1598: # CONFIG_BT_HCIUART_BCSP is not set
1599: # CONFIG_BT_HCIUART_ATH3K is not set
1600: CONFIG_BT_HCIUART_LL=y
1601: # CONFIG_BT_HCIUART_3WIRE is not set
1602: # CONFIG_BT_HCIUART_INTEL is not set
1603: CONFIG_BT_HCIUART_BCM=y
1604: # CONFIG_BT_HCIUART_RTL is not set
1605: CONFIG_BT_HCIUART_QCA=y
1606: # CONFIG_BT_HCIUART_AG6XX is not set
1607: CONFIG_BT_HCIUART_MRVL=y
1608: # CONFIG_BT_HCIUART_AML is not set
1876: CONFIG_DEVTMPFS=y
1877: CONFIG_DEVTMPFS_MOUNT=y
1878: # CONFIG_DEVTMPFS_SAFE is not set
2048: CONFIG_ARM_PSCI_FW=y
2054: CONFIG_QCOM_SCM=y
3273: CONFIG_ATH11K=m
3274: CONFIG_ATH11K_AHB=m
3275: CONFIG_ATH11K_PCI=m
3276: # CONFIG_ATH11K_DEBUG is not set
3445: CONFIG_KEYBOARD_GPIO=y
3667: CONFIG_SERIAL_EARLYCON=y
3733: CONFIG_SERIAL_QCOM_GENI=y
3734: CONFIG_SERIAL_QCOM_GENI_CONSOLE=y
3933: CONFIG_I2C_QCOM_GENI=m
4043: CONFIG_SPI_QCOM_GENI=m
4086: CONFIG_SPMI_MSM_PMIC_ARB=y
4256: CONFIG_PINCTRL_SM7635=y
4265: CONFIG_PINCTRL_QCOM_SPMI_PMIC=y
4856: CONFIG_QCOM_SPMI_TEMP_ALARM=m
5023: CONFIG_MFD_SPMI_PMIC=y
5106: CONFIG_REGULATOR_FIXED_VOLTAGE=y
5177: CONFIG_REGULATOR_QCOM_RPMH=y
5493: CONFIG_VIDEO_QCOM_CAMSS=m
5858: # CONFIG_DRM_SIMPLEDRM is not set
5950: CONFIG_DRM_MSM=m
6243: # CONFIG_FB_SIMPLE is not set
6303: CONFIG_FRAMEBUFFER_CONSOLE=y
6566: CONFIG_SND_SOC_QCOM=m
7218: CONFIG_USB_ACM=m
7282: CONFIG_USB_DWC3=y
7283: # CONFIG_USB_DWC3_ULPI is not set
7284: # CONFIG_USB_DWC3_HOST is not set
7285: # CONFIG_USB_DWC3_GADGET is not set
7286: CONFIG_USB_DWC3_DUAL_ROLE=y
7291: CONFIG_USB_DWC3_EXYNOS=y
7292: CONFIG_USB_DWC3_PCI=y
7293: CONFIG_USB_DWC3_HAPS=y
7294: CONFIG_USB_DWC3_KEYSTONE=y
7295: CONFIG_USB_DWC3_MESON_G12A=y
7296: CONFIG_USB_DWC3_OF_SIMPLE=y
7297: CONFIG_USB_DWC3_QCOM=y
7298: CONFIG_USB_DWC3_IMX8MP=y
7299: CONFIG_USB_DWC3_XILINX=y
7300: CONFIG_USB_DWC3_AM62=y
7301: CONFIG_USB_DWC3_RTK=y
7433: CONFIG_USB_GADGET=y
7466: CONFIG_USB_LIBCOMPOSITE=y
7467: CONFIG_USB_F_ACM=y
7468: CONFIG_USB_U_SERIAL=y
7479: CONFIG_USB_CONFIGFS=y
7481: CONFIG_USB_CONFIGFS_ACM=y
7510: # CONFIG_USB_G_SERIAL is not set
7529: CONFIG_TYPEC_QCOM_PMIC=m
7639: CONFIG_SCSI_UFSHCD=y
7640: CONFIG_SCSI_UFS_BSG=y
7641: # CONFIG_SCSI_UFS_HWMON is not set
7642: # CONFIG_SCSI_UFSHCD_PCI is not set
7643: CONFIG_SCSI_UFSHCD_PLATFORM=y
7644: CONFIG_SCSI_UFS_CDNS_PLATFORM=m
7645: # CONFIG_SCSI_UFS_DWC_TC_PLATFORM is not set
7646: CONFIG_SCSI_UFS_QCOM=y
7647: # CONFIG_SCSI_UFS_MEDIATEK is not set
7648: CONFIG_SCSI_UFS_HISI=y
7649: CONFIG_SCSI_UFS_RENESAS=m
7650: CONFIG_SCSI_UFS_TI_J721E=m
7651: CONFIG_SCSI_UFS_EXYNOS=y
7652: # CONFIG_SCSI_UFS_SPRD is not set
7653: CONFIG_SCSI_UFS_ROCKCHIP=y
7665: # CONFIG_LEDS_AW200XX is not set
7904: CONFIG_RTC_DRV_PM8XXX=m
7976: CONFIG_QCOM_GPI_DMA=m
8482: CONFIG_QCOM_CLK_RPMH=y
8554: # CONFIG_SM_CAMCC_7635 is not set
8562: # CONFIG_SM_DISPCC_7635 is not set
8573: CONFIG_SM_GCC_7635=y
8586: # CONFIG_SM_GPUCC_7635 is not set
8594: CONFIG_SM_TCSRCC_8550=y
8599: # CONFIG_SM_VIDEOCC_7635 is not set
8689: CONFIG_HWSPINLOCK_QCOM=y
8709: CONFIG_ARM_ARCH_TIMER=y
8758: CONFIG_QCOM_IPCC=y
8787: CONFIG_ARM_SMMU=y
8788: # CONFIG_ARM_SMMU_LEGACY_DT_BINDINGS is not set
8789: CONFIG_ARM_SMMU_DISABLE_BYPASS_BY_DEFAULT=y
8790: CONFIG_ARM_SMMU_MMU_500_CPRE_ERRATA=y
8791: CONFIG_ARM_SMMU_QCOM=y
8792: # CONFIG_ARM_SMMU_QCOM_DEBUG is not set
8793: CONFIG_ARM_SMMU_V3=y
8794: # CONFIG_ARM_SMMU_V3_SVA is not set
8812: CONFIG_QCOM_Q6V5_COMMON=m
8813: CONFIG_QCOM_Q6V5_ADSP=m
8814: CONFIG_QCOM_Q6V5_MSS=m
8815: CONFIG_QCOM_Q6V5_PAS=m
8816: # CONFIG_QCOM_Q6V5_WCSS is not set
8935: CONFIG_QCOM_AOSS_QMP=y
8936: CONFIG_QCOM_COMMAND_DB=y
8937: CONFIG_QCOM_GENI_SE=y
8946: CONFIG_QCOM_PMIC_GLINK=m
8949: CONFIG_QCOM_RMTFS_MEM=m
8951: CONFIG_QCOM_RPMH=y
8952: CONFIG_QCOM_SMEM=y
8959: CONFIG_QCOM_STATS=m
9079: CONFIG_QCOM_RPMHPD=y
9847: CONFIG_ARM_GIC_V3=y
9848: CONFIG_ARM_GIC_V3_ITS=y
9849: CONFIG_ARM_GIC_V3_ITS_FSL_MC=y
9879: CONFIG_QCOM_PDC=y
10011: CONFIG_PHY_QCOM_QMP_UFS=m
10015: CONFIG_PHY_QCOM_SNPS_EUSB2=y
10016: CONFIG_PHY_QCOM_EUSB2_REPEATER=y
10086: CONFIG_ARM_SMMU_V3_PMU=m
10153: CONFIG_NVMEM_SPMI_SDAM=m
10256: CONFIG_INTERCONNECT_QCOM_SM7635=y
10397: CONFIG_CONFIGFS_FS=y
10446: CONFIG_PSTORE=y
10447: CONFIG_PSTORE_DEFAULT_KMSG_BYTES=10240
10448: CONFIG_PSTORE_COMPRESS=y
10449: CONFIG_PSTORE_CONSOLE=y
10450: # CONFIG_PSTORE_PMSG is not set
10451: CONFIG_PSTORE_RAM=y
10452: # CONFIG_PSTORE_BLK is not set
10888: CONFIG_CRYPTO_DEV_QCOM_RNG=m
```

Ancillary enabled nodes: PMIC RTC (`pmk8550.dtsi:47-52`), SDAM (`54-65`), PMIC thermal alarms (`pm8550vs.dtsi:101-105` and peers; `pmr735b.dtsi:40-44`), RNG (`S:1699-1703`), RPMh stats (`S:2023-2026`) use module-selected drivers in the evidence above, with no corresponding built modules found. They are not prerequisites for an external initramfs shell. SPMI/MFD/GPIO, SCM, SMEM/hwspinlock, IPCC, AOSS, GIC/PDC and CPU frequency infrastructure have built-in selections; presence of config does not verify hardware resources. GPI/I2C/SPI drivers are module-selected but their nodes are disabled in this board configuration.

### Artifact SHA-256

- `arch/arm64/boot/Image`: `371045102a2de36647b7701709b55c4f8db0ee4f07ef44ac478e3edf33912d6a`
- `arch/arm64/boot/dts/qcom/sm7635-nothing-asteroids.dtb`: `b9e327d9dd08bb06bacb920592267f64c0856e9556882ff4cc75d0d81a158503`
- `.config`: `1d7fdeb08403e02df5750770e0237407f9af0ac2cdd521df5cac45a7b6a1269c`

### Additional configuration references

```text
670: CONFIG_ARM_QCOM_CPUFREQ_HW=y
2054: CONFIG_QCOM_SCM=y
2055: CONFIG_QCOM_TZMEM=y
2056: # CONFIG_QCOM_TZMEM_MODE_GENERIC is not set
2057: CONFIG_QCOM_TZMEM_MODE_SHMBRIDGE=y
4503: CONFIG_POWER_RESET_QCOM_PON=m
5023: CONFIG_MFD_SPMI_PMIC=y
8468: CONFIG_QCOM_GDSC=y
8469: CONFIG_COMMON_CLK_QCOM=y
8482: CONFIG_QCOM_CLK_RPMH=y
8955: CONFIG_QCOM_SMP2P=y
```

Final read-only verification: original tracked diff still totals four files, 44 insertions and 7 deletions. The only new status entry introduced by the audit is this report. No runtime success is claimed.

### Snapshot comparison

Read-only comparisons against their canonical working files confirm:

- Both DTS `.before-nothing-tuning-final` and `.before-nothing-usb-tuning` copies are identical to the current DTS.
- DTS `.before-usb-gadget` differs only in the OTG/role-switch to peripheral change.
- Both head.S snapshots differ from current head.S by the four added canary lines.
- Repeater `.before-nothing-tuning` differs by the tuning-table change.
- Synopsys `.before-downstream-sequence` differs by the delay/override-release sequence edits; `.before-nothing-tuning` also predates the register-zero override.
- `init/main.c.before-reboot-canary` is identical to current `init/main.c`; its filename is not evidence of an active reboot canary.
