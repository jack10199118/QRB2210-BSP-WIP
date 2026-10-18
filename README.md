# QRB2210 Windows BSP scaffold

**Status: scaffold, not a bootable BSP.** SoC values were taken from the supplied
`qcm2290.dtsi`. Remaining `TODO(HW)`/`TODO(BOARD)` items need the board DTS and
Qualcomm documentation. Nothing here
has been compiled or run on hardware.

## Blocking risks (resolve before writing more code)
1. **Target OS.** Cortex-A53 (ARMv8.0) was officially supported by Windows 10 IoT Core (DragonBoard 410c).
   However, IoT Core is retired and its builds were, as far as I recall, 32-bit ARM, while this project assumes
   ARM64. Newer Windows 11 ARM64 releases may require newer ISA features (unverified). Decide the OS image and
   architecture first.
2. **Firmware.** XBL/TZ/hypervisor/UEFI are Qualcomm-signed and NDA-gated.
   You need Qualcomm to supply (or sign) a boot chain that can load a UEFI
   capable of booting Windows. Secure boot keys/fuses are board specific.
3. **No public Windows drivers** for Adreno 702, modem, Wi-Fi/BT, ADSP audio,
   camera. Plan on Basic Display (UEFI GOP framebuffer) and no radios at first.

## Facts taken from qcm2290.dtsi
- SDC1: hc 0x04744000, hc_irq SPI 348 (GSIV 380), pwr_irq SPI 352 (GSIV 384), 8-bit, SMMU SID 0xC0
- SDC2: 0x04784000, SPI 350/353, 4-bit, SMMU SID 0xA0
- UART0 0x04A80000 SPI 327; UART4 0x04A90000 SPI 331
- GICv3: GICD 0x0F200000, GICR 0x0F300000; GCC 0x01400000; SMMU-500 0x0C600000

## Facts taken from qrb2210-rb1.dts (RB1 board)
- Debug console: UART4 (serial0, 115200n8, `qcom,geni-debug-uart`), FTDI to Micro-USB
- SDC1: non-removable eMMC, CQE supported, no SD/SDIO. VMMC = PM4125 L20, VQMMC = PM4125 L14 (1.8 V, must never be turned off)
- SDC2: SD slot, card-detect GPIO 88 active low, VMMC = L21, VQMMC = L4
- Regulators: PM4125 behind the **RPM** (SMD, `qcom,rpm-pm2250-regulators`), not RPMh
- Bluetooth: UART3 (WCN3950), IRQ SPI 330 (GSIV 362), enable GPIO 87
- Also enabled: GPU, MDSS/DSI, HDMI bridge, USB, Wi-Fi, SPI5, I2C2 (GPIO bit-bang)

## Layout
```
acpi/dsdt.asl            ACPI skeleton: CPUs, UART4, SDC1, SDC2
acpi/tables/madt.asl     MADT: 4x GICC, GICD 0x0F200000, GICR 0x0F300000
acpi/tables/gtdt.asl     GTDT: arch timer INTIDs 17/18/19/16
acpi/tables/dbg2.asl     DBG2: UART4 debug port (subtype TODO)
drivers/emmc/emmc.c      KMDF SDHCI-style eMMC host core (MMIO, IRQ, ADMA2)
drivers/emmc/emmc.inf    ARM64 INF
drivers/tlmm/            TLMM pin/GPIO core: pull, function, drive, direction, pin ownership (host-tested; no GpioClx shell, no interrupts)
drivers/qcompwr/         Resource service: name-based, ref-counted, per-handle ownership (qcom_res.*, host-tested)
acpi/tables/fadt.aslc   FADT (EDK2 C source, HW-reduced, PSCI/SMC; not compiled)
tools/build_acpi.sh      iasl build for the DSDT and data tables
docs/COMPAT_LAYER_DESIGN.md  Layered platform design, milestones, open decisions
docs/SDPORT_PLAN.md      How to split emmc.c for the sdport storage model
acpi/tables/iort.asl     IORT: SMMU-500 + SDC1/SDC2 stream IDs (generated, unvalidated)
```
Not included: FACS, UEFI platform package, GpioClx driver for TLMM, I2C/SPI/USB/display.
Need vendor input: PEP/clock+regulator driver, TLMM GPIO
driver, PMIC, USB DWC3 glue, UEFI platform package, FFU/image config.

## Bring-up order
1. UEFI reaches a serial console; verify GOP framebuffer.
2. ACPI: MADT/GTDT/DBG2/IORT/FADT(HW-reduced) validate with `acpiexec`/ASL compiler.
3. Boot WinPE/Windows with test signing, KD over UART (DBG2 + kdnet-less serial).
4. Bring up clocks/regulators (PEP or custom power framework) for SDC1.
5. eMMC: init sequence CMD0/1/2/3/7, 8-bit DDR/HS200, then SDPORT or Storport integration.
6. Add I2C/SPI/GPIO via SPB/GpioClx.

## Storage architecture decision
Windows expects SD/eMMC host controllers to plug in through the **SDPORT
miniport** model (`sdport.sys`, `SDPORT_INITIALIZATION_DATA`). `emmc.c` is
written as a KMDF hardware core (resources, reset, clock, ADMA2, interrupts)
whose routines map onto SDPORT callbacks (`SlotInitialize`, `SlotIssueRequest`,
`SlotInterrupt`, ...). Port that layer once the hardware core works.
