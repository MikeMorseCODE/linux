# Nothing Phone (3a) Pro (Asteroids) upstream status

This is a Phase 1 audit of `sm7635-nothing-asteroids.dts` and its included
`sm7635.dtsi` on branch `wip-next-20250417`.  “Present” below means present in
this checkout; several SM7635 drivers and bindings are local WIP commits and
are not evidence that the corresponding support is already in upstream Linux.
The DTS and kernel tree were already modified by the bring-up work before this
audit.  This document does not attempt to normalize those changes.

## Executive summary

The boot-critical platform is substantially described: eight CPUs, PSCI, GICv3
and ITS, timer, RPMh RSC/clock/power domains, SPMI arbiter, TLMM, SMMU, QUPv3,
USB2 and the Qualcomm DWC3 wrapper are present.  The reported milestones prove
the CPU/PSCI/MMU/kernel/userspace path, but do not prove that each platform
driver has probed.

The board DTS currently has no UFS controller or UFS PHY node, no MDSS/DPU/DSI
graph, no Adreno node, no modem/WLAN/Bluetooth/audio remoteproc nodes, no
interconnect provider nodes or consumer paths, and no battery/charger/Type-C,
sensor, camera, or haptics child devices.  Those are description and/or
platform-support tasks, not status-only enablements.  Stock 6.1 modules are
not considered compatible.

The smallest defensible first activation is the existing SM7635 TSENS support:
enable `tsens0` and `tsens1` after this audit.  It uses an existing in-tree
driver and binding, does not require a new downstream driver, and does not
change bootloader or storage behavior.  Thermal zones still need to be added
from verified board calibration before this is considered product thermal
management.

## Complete subsystem matrix

| Subsystem | Current Asteroids nodes / compatibles | Driver and Kconfig | Status and dependencies | Enable now / upstreaming |
|---|---|---|---|---|
| CPU / SMP | `cpus`; Cortex-A520/A720; `qcom,sm7635-cpufreq-epss`; idle states | `drivers/cpufreq/qcom-cpufreq-epss.c`, `CONFIG_ARM_QCOM_CPUFREQ_NVMEM`/EPSS support; generic arm64 SMP and cpuidle | Working by hardware milestone. Requires PSCI, RPMh CPU power domains, GCC/RPMh clocks. | Already enabled. No device driver upstreaming needed; SM7635 EPSS support is local WIP if not yet upstream. |
| Interrupt controller | `intc`: `arm,gic-v3`; child `arm,gic-v3-its`; `pdc`: `qcom,sm7635-pdc`, `qcom,pdc` | `drivers/irqchip/irq-gic-v3.c`, `drivers/irqchip/qcom-pdc.c`; `CONFIG_ARM_GIC_V3`, `CONFIG_QCOM_PDC` | Working enough for boot/SMP. Asteroids adds a redistributor layout override; validate against hardware. | Already active. No new driver needed. |
| PSCI | `psci`: `arm,psci-1.0`; CPU `enable-method = "psci"` | `drivers/firmware/psci/`; `CONFIG_ARM_PSCI_FW` | Working by milestone; firmware-provided implementation. | No DTS work needed. |
| Clocks | `gcc`, `camcc`, `dispcc`, `videocc`, `gpucc`, `tcsr`, `rpmhcc`; fixed XO/sleep/pipe clocks | `drivers/clk/qcom/gcc-sm7635.c`, `camcc-sm7635.c`, `dispcc-sm7635.c`, `videocc-sm7635.c`, `gpucc-sm7635.c`, `clk-rpmh.c`, `tcsrcc-sm8550.c`; `CONFIG_SM_*_7635`, `CONFIG_QCOM_RPMH` | Present, but SM7635 clock drivers and RPMh compatible are local WIP. CAM/DISP/VIDEO/GPU nodes are disabled in the board DTS. Consumers need correct power domains and rates. | GCC/RPMh are needed now and already used. Do not enable display/GPU clock blocks without their complete consumers. Upstreaming of the local SM7635 clock series appears necessary. |
| RPMh | `apps_rsc`, `rpmhcc`, `rpmhpd`, BCM voter, `qcom,sm7635-rpmh-clk`, `qcom,sm7635-rpmhpd` | `drivers/soc/qcom/rpmh-rsc.c`, `drivers/clk/qcom/clk-rpmh.c`, `drivers/pmdomain/qcom/rpmhpd.c`; `CONFIG_QCOM_RPMH`, `CONFIG_QCOM_RPMHPD` | Likely usable for boot and regulators. Depends on firmware CMD DB/SMEM reserved regions and correct RPMh clock tables. | Already active. The SM7635 data needs upstream review; no vendor module is required. |
| Regulators / PMIC | SPMI arbiter; PMIC includes `pmk8550.dtsi`, `pm8550vs.dtsi`, `pmr735b.dtsi`, local `pmxr2230` and `pmiv0104`; RPMh `qcom,pm7550-rpmh-regulators`; fixed `vph_pwr` | `drivers/spmi/spmi.c`, `drivers/mfd/qcom-spmi-pmic.c`, `drivers/regulator/qcom-rpmh-regulator.c`, `drivers/pinctrl/qcom/pinctrl-spmi-gpio.c`; `CONFIG_SPMI`, `CONFIG_MFD_QCOM_SPMI_PMIC`, `CONFIG_REGULATOR_QCOM_RPMH` | Basic PMIC/RPMh path is likely usable. Only a small set of PM7550 rails is described. Charger, ADC, PON, RTC, GPIOs and PMIC children are incomplete or absent. | Existing rail definitions can be used. Board-specific rail additions require schematics/measurement; PM7550 support is local WIP and needs upstreaming if retained. |
| Interconnect | No SM7635 interconnect provider nodes and no `interconnects` properties in active consumers; dtsi comments say TODO for SDHCI/crypto | `drivers/interconnect/qcom/sm7635.c`; `CONFIG_INTERCONNECT_QCOM_SM7635`, `CONFIG_INTERCONNECT_QCOM_OSM_L3` | Driver/bindings exist locally, but the DTS does not instantiate the provider topology or vote paths. | Not a status-only enable. Add provider/consumer data only after validating the local binding and bandwidth IDs; upstreaming appears necessary. |
| SMMU / IOMMU | `apps_smmu`: `qcom,sm7635-smmu-500`, `qcom,smmu-500`, `arm,mmu-500`; QUP, SDHCI, USB, crypto use stream IDs | `drivers/iommu/arm/arm-smmu/arm-smmu.c`; `CONFIG_ARM_SMMU`, optional `CONFIG_ARM_SMMU_QCOM` | Driver exists and the node is present. Usability depends on correct stream IDs and secure-world ownership; duplicated SMMU interrupt 423 is suspicious and should be checked against binding/hardware. | Can be used by already-described consumers. Do not “fix” stream IDs speculatively. No new driver expected. |
| UFS / storage | No UFS host node, no QMP UFS PHY node. ICE node exists: `qcom,sm7635-inline-crypto-engine`, `qcom,inline-crypto-engine`, but is not enough for storage | `drivers/ufs/host/ufshcd-pltfrm.c`, `ufs-qcom.c`, `drivers/phy/qualcomm/phy-qcom-qmp-ufs.c`; `CONFIG_SCSI_UFSHCD_PLATFORM`, `CONFIG_SCSI_UFS_QCOM`, `CONFIG_PHY_QCOM_QMP_UFS`, `CONFIG_SCSI_UFS_CRYPTO` | Missing board/SoC description. Existing UFS QMP driver has no `qcom,sm7635-qmp-ufs-phy` match in this tree despite the local binding commit. | Cannot enable now without a verified controller/PHY node, clocks, regulators, resets and interconnects. A matching UFS PHY/host support series appears necessary; do not copy vendor code. |
| USB / DWC3 | `usb_1`: `qcom,sm8650-dwc3`, `qcom,dwc3`; child `snps,dwc3`; `usb_1_hsphy`: `qcom,sm7635-snps-eusb2-phy`, `qcom,sm8550-snps-eusb2-phy`; PMIC repeater `qcom,pm8550b-eusb2-repeater` | `drivers/usb/dwc3/dwc3-qcom.c`, `drivers/usb/dwc3/core.c`, `drivers/phy/qualcomm/phy-qcom-snps-eusb2.c`, `phy-qcom-eusb2-repeater.c`; `CONFIG_USB_DWC3`, `CONFIG_USB_DWC3_QCOM`, `CONFIG_PHY_QCOM_SNPS_EUSB2`, `CONFIG_PHY_QCOM_EUSB2_REPEATER` | Likely usable and already exercised in peripheral/high-speed configuration. SuperSpeed is not described by a USB3 PHY node; board DTS forces peripheral and high-speed. | Existing code can be used now for USB2 gadget validation. Keep role/speed changes separate; no vendor DWC3 port needed. The SM7635 EUSB2 match is local/incomplete in this branch. |
| USB-C / role switching | No Type-C controller, PMIC GLINK, UCSI or connector node in Asteroids DTS | `drivers/usb/typec/tcpm`, `drivers/usb/typec/qcom-pmic-typec.c`, PMIC GLINK/UCSI drivers; `CONFIG_TYPEC`, `CONFIG_TYPEC_TCPM`, `CONFIG_TYPEC_QCOM_PMIC`, `CONFIG_UCSI_PMIC_GLINK` | Missing board nodes and power/role graph. USB currently has fixed `dr_mode = "peripheral"`; this is not role switching. | Not enableable without verified PMIC/firmware interface. Use upstream Type-C/PMIC GLINK only after hardware topology is known; no vendor stack. |
| Display / DRM / DSI | Only simple framebuffer in `/chosen`; `dispcc` disabled; no MDSS/DPU/DSI nodes or graph | `drivers/gpu/drm/msm/`, DPU/DSI; `CONFIG_DRM_MSM`, `CONFIG_DRM_MSM_DPU`, `CONFIG_DRM_MSM_DSI`; SM7635 display catalog support is not present as an Asteroids node | Boot splash is firmware framebuffer, not DRM. Display hardware is incomplete. | Cannot enable with a clock toggle. Requires MDSS/DPU/DSI SoC description, board graph and panel supplies/reset/GPIOs. Existing upstream DRM framework should be used; SoC catalog/display support may need upstreaming. |
| Panel | No panel node. Kernel contains local `VXN RM69220` driver | `drivers/gpu/drm/panel/panel-vxn-rm69220.c`; `CONFIG_DRM_PANEL_VXN_RM69220` | Driver is local and unverified here; no DSI endpoint means it cannot probe. | Not now. Bind only after panel ID, DSI lanes, reset, bias and backlight are verified. Upstreaming likely needed for the panel driver. |
| GPU | `gpucc` exists but is disabled; no Adreno GPU node | `drivers/gpu/drm/msm/adreno/`, `msm_drv.c`; `CONFIG_DRM_MSM`, `CONFIG_DRM_MSM_GPU_STATE` as needed | Missing GPU node, GMU, power domains, IOMMU context and firmware description. `gpu_microcode` reserved memory alone is not a device. | Not now. Requires matching upstream Adreno/GMU support and firmware; no KGSL/vendor compatibility layer. |
| Touchscreen / input | `gpio-keys`; empty `spi0` comment for `focaltech,fts`; PMIC GPIO volume key; no touchscreen child | `drivers/input/keyboard/gpio_keys.c`, `drivers/input/touchscreen/` (FTS driver must be confirmed); `CONFIG_KEYBOARD_GPIO`, `CONFIG_TOUCHSCREEN_FTS_SPI` | GPIO keys are likely usable; touchscreen is only a comment and has no GPIOs/regulators/IRQ/compatible. | GPIO keys already enabled. Add Focaltech only from verified bus/pins/IRQ and binding; no speculative node. |
| Wi-Fi | No WLAN/PCIe/WCNSS node; reserved WLAN memory only | Potential upstream `ath11k` PCI/AHB or Qualcomm WLAN/remoteproc paths; `CONFIG_ATH11K*`, `CONFIG_QCOM_WCNSS_*` | Missing hardware node, PCIe/firmware/board data and transport. Vendor CNSS stack is explicitly out of scope. | Not now. First determine whether this SKU uses PCIe ath11k or a Qualcomm remoteproc/WCNSS design. Upstream driver/data work likely needed. |
| Bluetooth | `uart11` is enabled and commented Bluetooth; no BT child/firmware protocol node | `drivers/bluetooth/hci_qca.c` and generic serial HCI; `CONFIG_BT_HCIUART`, `CONFIG_BT_HCIUART_QCA` | UART transport is described, but no child binding, enable/reset GPIOs or firmware/serdev hookup. | Possibly enableable after verified wiring with upstream HCI QCA. No vendor module required; DTS child and pin/power data are missing. |
| Audio / DSP | No LPASS, ADSP, APR/GLINK audio, SoundWire, codec or machine node; only reserved DSP memory and disabled SMP2P | `drivers/remoteproc/qcom_q6v5*`, `sound/soc/qcom/qdsp6/`, SoundWire; `CONFIG_QCOM_Q6V5_*`, `CONFIG_SND_SOC_QDSP6`, `CONFIG_SOUNDWIRE` | Incomplete. Firmware memory alone does not create a remoteproc. Codec/machine topology is unknown. | Not now. Requires verified firmware handoff, GLINK/SMP2P, LPASS clocks/interconnects and codec graph. Use upstream Q6DSP/SoundWire only. |
| Modem / QRTR | Disabled `smp2p-modem`; reserved MPSS/rmtfs/IPA memory; no MPSS remoteproc, MHI/QRTR or IPA node | `drivers/remoteproc/qcom_q6v5_mss.c`, `drivers/soc/qcom/rmtfs_mem.c`, `net/qrtr/`, MHI; `CONFIG_QCOM_Q6V5_MSS`, `CONFIG_QCOM_RMTFS_MEM`, `CONFIG_QRTR` | Reserved regions are preparatory only. No modem device graph or firmware policy. | Not now. Need verified modem firmware ownership and remoteproc/SMEM/SMP2P/GLINK sequence. Do not use vendor modem modules. |
| Battery / charging | Only fixed `vph_pwr`; no charger, fuel gauge, PMIC GLINK or USB power-supply nodes | PMIC charger/fuel-gauge or PMIC GLINK power supply drivers; symbols vary by verified PMIC binding | Missing. A fixed rail is not battery management. | Not now. Requires PMIC schematic and runtime interface identification. Existing upstream power-supply drivers preferred. |
| Thermal | `tsens0/1`: `qcom,sm7635-tsens`, `qcom,tsens-v2`, both disabled; no thermal zones | `drivers/thermal/qcom/tsens.c` and `drivers/thermal/qcom/tsens-v2.c`; `CONFIG_QCOM_TSENS` plus `CONFIG_NVMEM_QCOM_QFPROM` | Driver and binding are present; node data is complete enough to try probing. No board thermal zones are defined. | Yes: enable both sensors and their QFPROM dependency as the first minimal activation. Add thermal zones only after calibrated sensor mapping. |
| Sensors | No IIO ADC/IMU/proximity/ALS nodes; PMIC ADC blocks are not instantiated in Asteroids PMIC includes | IIO/PMIC-specific drivers; `CONFIG_IIO`, relevant `CONFIG_QCOM_SPMI_*` | Missing board child nodes and calibration. | Not now; identify exact parts and buses first. |
| Haptics | Empty `i2c8` comment for `ics haptic@5f`; GPIO comment for `ics_haptic_rst`; no child | Part-specific I2C haptics driver, if supported; likely `CONFIG_INPUT`/driver-specific | Missing compatible, reset/power/interrupt properties and driver identification. | Not now; no speculative compatible. |
| Camera | `camcc` disabled; no CSIPHY/CSID/VFE/CCI/sensor/EEPROM/OIS nodes; comments only mention I2C/GPIO | `drivers/media/platform/qcom/camss/` where applicable, Qualcomm clock/CCI/CSIPHY | Incomplete and device-specific. | Not now. Requires complete camera graph and SoC support; likely upstream camera work. |
| Suspend / power management | PSCI, CPU power domains, RPMh PD, AOSS QMP and idle states are present; no board wake/thermal/charger policy | PSCI, RPMh PD, AOSS QMP, cpuidle; `CONFIG_ARM_PSCI_FW`, `CONFIG_QCOM_RPMHPD`, `CONFIG_QCOM_AOSS_QMP` | Runtime boot is proven; suspend is untested and dependencies are incomplete. Bootargs currently ignore unused clocks/PD/regulators, masking omissions. | Do not pursue until clocks/regulators/thermal/wake sources are correct. No security disabling is acceptable. |

## Compatible-to-driver audit

The following are all non-generic device compatibles used by the Asteroids
board or inherited SM7635 DTSI.  Generic compatibles such as `simple-bus`,
`fixed-clock`, `gpio-keys`, `ramoops`, `cache`, `arm,psci-1.0`, `arm,armv8-
timer`, `arm,gic-v3`, `arm,gic-v3-its`, `arm,mmu-500`, `snps,dwc3`, `qcom,
spmi-pmic`, and `qcom,spmi-gpio` resolve to their normal subsystem drivers.

| Compatible | Node | Driver path | Kconfig / result |
|---|---|---|---|
| `qcom,sm7635-gcc` | `gcc` | `drivers/clk/qcom/gcc-sm7635.c` | `CONFIG_SM_GCC_7635`; local WIP, present |
| `qcom,sm7635-camcc` | `camcc` | `drivers/clk/qcom/camcc-sm7635.c` | `CONFIG_SM_CAMCC_7635`; local WIP, board-disabled |
| `qcom,sm7635-dispcc` | `dispcc` | `drivers/clk/qcom/dispcc-sm7635.c` | `CONFIG_SM_DISPCC_7635`; local WIP, board-disabled |
| `qcom,sm7635-videocc` | `videocc` | `drivers/clk/qcom/videocc-sm7635.c` | `CONFIG_SM_VIDEOCC_7635`; local WIP, board-disabled |
| `qcom,sm7635-gpucc` | `gpucc` | `drivers/clk/qcom/gpucc-sm7635.c` | `CONFIG_SM_GPUCC_7635`; local WIP, board-disabled |
| `qcom,sm7635-rpmh-clk` | `rpmhcc` | `drivers/clk/qcom/clk-rpmh.c` | `CONFIG_QCOM_RPMH`; local match/data extension |
| `qcom,sm7635-rpmhpd` | `rpmhpd` | `drivers/pmdomain/qcom/rpmhpd.c` | `CONFIG_QCOM_RPMHPD`; local SM7635 data |
| `qcom,sm7635-gpi-dma` / `qcom,sm6350-gpi-dma` | `gpi_dma0/1` | `drivers/dma/qcom/qcom_gpi.c` | `CONFIG_QCOM_GPI_DMA`; local compatible plus fallback |
| `qcom,geni-se-qup` | `qupv3_id_0/1` | `drivers/soc/qcom/qcom-geni-se.c` | `CONFIG_QCOM_GENI_SE`; standard upstream framework |
| `qcom,geni-i2c` / `qcom,geni-spi` / `qcom,geni-uart` | QUP children | `drivers/i2c/busses/i2c-qcom-geni.c`, `drivers/spi/spi-geni-qcom.c`, `drivers/tty/serial/qcom_geni_serial.c` | `CONFIG_I2C_QCOM_GENI`, `CONFIG_SPI_QCOM_GENI`, `CONFIG_SERIAL_QCOM_GENI`; UART/I2C/SPI framework present |
| `qcom,sm7635-trng` / `qcom,trng` | `rng` | `drivers/char/hw_random/qcom-rng.c` | `CONFIG_HW_RANDOM_QCOM`; binding/driver present |
| `qcom,sm7635-inline-crypto-engine` | `ice` | `drivers/soc/qcom/ice.c` | `CONFIG_QCOM_ICE`; storage consumer absent |
| `qcom,sm7635-sdhci` / `qcom,sdhci-msm-v5` | `sdhc_2` | `drivers/mmc/host/sdhci-msm.c` | `CONFIG_MMC_SDHCI_MSM`; disabled and lacks interconnects/regulators |
| `qcom,sm7635-snps-eusb2-phy` / `qcom,sm8550-snps-eusb2-phy` | `usb_1_hsphy` | `drivers/phy/qualcomm/phy-qcom-snps-eusb2.c` | `CONFIG_PHY_QCOM_SNPS_EUSB2`; SM7635 match is local/incomplete |
| `qcom,sm8650-dwc3` / `qcom,dwc3` | `usb_1` | `drivers/usb/dwc3/dwc3-qcom.c` | `CONFIG_USB_DWC3_QCOM`; generic wrapper match is present |
| `qcom,sm7635-pdc` / `qcom,pdc` | `pdc` | `drivers/irqchip/qcom-pdc.c` | `CONFIG_QCOM_PDC`; present |
| `qcom,sm7635-tsens` / `qcom,tsens-v2` | `tsens0/1` | `drivers/thermal/qcom/tsens.c` + `tsens-v2.c` | `CONFIG_QCOM_TSENS`; disabled only in DTS |
| `qcom,sm7635-aoss-qmp` / `qcom,aoss-qmp` | `aoss_qmp` | `drivers/soc/qcom/qcom_aoss.c` | `CONFIG_QCOM_AOSS_QMP`; present |
| `qcom,spmi-pmic-arb` | `spmi_bus` | `drivers/spmi/spmi-pmic-arb.c` | `CONFIG_SPMI`, `CONFIG_SPMI_MSM_PMIC_ARB`; present |
| `qcom,sm7635-tlmm` | `tlmm` | `drivers/pinctrl/qcom/pinctrl-sm7635.c` | `CONFIG_PINCTRL_SM7635`; local WIP, present |
| `qcom,sm7635-smmu-500` | `apps_smmu` | `drivers/iommu/arm/arm-smmu/arm-smmu.c` | `CONFIG_ARM_SMMU`; generic `arm,mmu-500` fallback |
| `qcom,sm7635-ipcc` | `ipcc` | `drivers/mailbox/qcom-ipcc.c` | `CONFIG_QCOM_IPCC`; present |
| `qcom,smp2p` | ADSP/CDSP/modem links | `drivers/soc/qcom/smp2p.c` | `CONFIG_QCOM_SMP2P`; all three links disabled |
| `qcom,smem` | reserved SMEM | `drivers/soc/qcom/smem.c` | `CONFIG_QCOM_SMEM`; present in SoC DTSI |
| `qcom,rmtfs-mem` | board `rmtfs-region` | `drivers/soc/qcom/rmtfs_mem.c` | `CONFIG_QCOM_RMTFS_MEM`; board node present |
| `qcom,pmxr2230` / `qcom,pmxr2230-gpio` | PMIC @1 | `drivers/mfd/qcom-spmi-pmic.c`, `drivers/pinctrl/qcom/pinctrl-spmi-gpio.c` | PMIC/GPIO match present locally |
| `qcom,pm8550` / `qcom,pm8550b-eusb2-repeater` | PMIC @7/repeater | PMIC MFD and `drivers/phy/qualcomm/phy-qcom-eusb2-repeater.c` | repeater is usable for USB2; PMIC address/topology needs board verification |
| `qcom,pm7550-rpmh-regulators` | `regulators-0` | `drivers/regulator/qcom-rpmh-regulator.c` | `CONFIG_REGULATOR_QCOM_RPMH`; local PM7550 addition |
| `qcom,rpmh-rsc` / `qcom,bcm-voter` | `apps_rsc` children | `drivers/soc/qcom/rpmh-rsc.c`, `drivers/interconnect/qcom/bcm-voter.c` | `CONFIG_QCOM_RPMH`, `CONFIG_INTERCONNECT_QCOM_BCM_VOTER`; present |

There is no matching Asteroids node in this tree for UFS, MDSS/DPU/DSI,
Adreno/GMU, modem remoteproc, WLAN, Type-C, charger/fuel gauge, camera,
audio/LPASS, or board sensors.  A reserved-memory entry or a commented bus
child is not treated as a device node.

## Reusable SM7635/Volcano patterns and history

This checkout has only one real SM7635 board DTS: Asteroids.  The reusable
SoC-level pattern is therefore `sm7635.dtsi` plus the PMIC includes.  Similar
Qualcomm boards show the expected layering: SoC DTSI owns clocks, buses,
interrupts, SMMU and generic pin states; the board DTS owns PMIC instances,
regulator consumers, GPIOs, child peripherals, endpoints and thermal zones.
The Asteroids file currently follows that layering for the console, GPIO keys,
USB2 and PMIC repeater, but leaves many board buses as comments.

Relevant local history is:

* `3993c6ec363d` — initial SM7635/Volcano DTSI.
* `35b1ec1cf298` — initial Asteroids board DTS.
* `d662a266f4db`, `51dbb5d6d7d0`, `be7f6bda2b08`, `f9f3c39df846`, `f22e0c27ac08` — local SM7635 GCC/CAMCC/DISPCC/GPUCC/VIDEOCC drivers.
* `ad60bccc7c12` — local SM7635 interconnect provider.
* `9a4c57bdd4a7` — local RPMh clock compatible extension.
* `c2674c4e431b` — local PM7550 RPMh regulator support.
* `afa303bb5450` — local SM7635 UFS PHY binding work, without a matching UFS PHY node or driver match in this tree.

These commits are useful implementation references but should be reviewed as
upstream submissions, not assumed to be stable ABI.

## Prioritized bring-up plan

1. Remove diagnostic `*_ignore_unused` boot arguments as each dependency is
   proven; keep security and firmware ownership intact.
2. Complete RPMh regulator consumers and add validated interconnect provider
   and paths.  Fix or confirm SMMU stream IDs and interrupt list.
3. Add the verified UFS controller and QMP UFS PHY description, including
   clocks, regulators, resets, ICE and bandwidth paths.  This is the storage
   blocker for a useful postmarketOS/Droidian root filesystem.
4. Keep the already-described USB2 gadget path; then add verified USB3 PHY and
   Type-C/role-switch graph separately.
5. Add MDSS/DPU/DSI and the RM69220 panel only after the display graph,
   supplies, reset and panel timing are verified.
6. Add GPIO/SPI touchscreen and other input devices from measured IRQ/reset
   wiring.  Then add GPU/GMU and its firmware/IOMMU/power dependencies.
7. Determine the WLAN transport and Bluetooth UART protocol; use ath11k/QCA
   upstream paths and documented firmware, not CNSS/vendor modules.
8. Bring up remoteproc/QRTR/modem and Q6DSP/audio after firmware ownership,
   SMP2P/GLINK and memory regions are verified.
9. Add charger/fuel gauge, calibrated TSENS thermal zones, sensors and haptics.
10. Add camera graphs and finally validate suspend/resume, wake sources and
    power collapse.

## First patch set

The first patch is intentionally small:

* Enable `tsens0` and `tsens1` in the Asteroids DTS and enable
  `CONFIG_NVMEM_QCOM_QFPROM` plus `CONFIG_QCOM_TSENS` in the SM7635 config
  fragment.  TSENS needs calibration data from QFPROM.
* This activates the existing `qcom,sm7635-tsens` / `qcom,tsens-v2` driver
  (`drivers/thermal/qcom/tsens-v2.c`, `CONFIG_QCOM_TSENS`).
* The relevant binding is `Documentation/devicetree/bindings/thermal/qcom-tsens.yaml`.
* No thermal zone is invented in this patch; zone trips and sensor mapping are
  a separate board-calibration change.

No UFS, display, GPU, modem, WLAN, charger, camera, or vendor driver is added.

## Validation commands

For the affected tree, use the configured cross compiler and a clean output
directory as appropriate for the local build environment:

```sh
make O=out ARCH=arm64 LLVM=1 LLVM_IAS=1 dtbs_check DT_SCHEMA_FILES=Documentation/devicetree/bindings/thermal/qcom-tsens.yaml
make O=out ARCH=arm64 LLVM=1 LLVM_IAS=1 Image qcom/sm7635-nothing-asteroids.dtb
```

Schema warnings must be reported separately from errors.  On-device
validation, after the new DTB is supplied by the user through the existing
temporary boot workflow, should check:

```sh
adb shell 'dmesg | grep -i -E "tsens|thermal|thermal_zone"'
adb shell 'ls -l /sys/class/thermal'
adb shell 'for z in /sys/class/thermal/thermal_zone*; do echo "$z"; cat "$z/type" "$z/temp"; done'
adb shell 'find /sys/bus/platform/devices -maxdepth 1 -iname "*c228000*" -o -iname "*c229000*"'
```

The commands intentionally do not include flashing, relocking, fastboot, or
adb reboot operations.
