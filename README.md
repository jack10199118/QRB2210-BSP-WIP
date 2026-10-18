# Windows BSP scaffold: Arduino UNO Q (Qualcomm Dragonwing QRB2210)

**Target board: Arduino UNO Q ("imola").** Board data comes from the upstream `qrb2210-arduino-imola.dts`
(Linux patch series v3, Nov 2025: re-check it against the final kernel file). The Qualcomm RB1 table is kept for
reference (`-DQCOM_BOARD_RB1`).

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

## Facts taken from qrb2210-arduino-imola.dts (UNO Q, target)
- Console: UART4 (serial0, 115200n8, `qcom,geni-debug-uart`) on the JCTL header, **1.8 V logic**
- Storage: SDC1 eMMC only (non-removable, CQE, HS200/HS400 at 1.8 V, no SD/SDIO). **No SD slot.**
  VMMC = PM4125 L20 (2.4-3.6 V); VQMMC = L14 (1.8 V, "Broken hardware, never turn it off!")
- Always-on rails the service must never switch off: L2 (LPDDR4X VDD2), L3 (VDDQ), L6 (DRAM PLL), L11, L14, L15, plus
  their parents S3 and S4. L21 is USB HS, L4 is unused.
- Regulators: PM4125 behind the **RPM** (`qcom,rpm-pm2250-regulators`), not RPMh
- UART3 = Bluetooth (WCN3988, 3 Mbaud), enable GPIO 87; UART2 and SPI5 (CS GPIO 17) link to the STM32U585 MCU
- GPIOs: volume keys 96 and 36; LEDs 47 (BT), 39 (panic), 40 (WLAN), 60/42/41 (user RGB)
- Display: no MDSS node enabled upstream and the continuous-splash region is deleted: display bring-up is unknown
- RAM: 2 or 4 GB LPDDR4X; XO board clock 38.4 MHz
- Boot chain observed on a real board (zenn.dev log): XBL, Qualcomm UEFI, then **U-Boot 2025.07** ("Arduino SA,Imola"),
  then the Linux EFI stub. Secure Boot: Off; boot device eMMC. Recovery: EDL with `qdl` and a Firehose programmer.

## Facts that were RB1-only (do not apply to the UNO Q)
SD slot with card-detect GPIO 88, SDC2 resources, display/HDMI bridge, FTDI console, I2C2 bit-bang.

## Layout
```
acpi/dsdt.asl            ACPI skeleton: CPUs, power service, TLMM, UART4, SDC1 (no SD slot)
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
acpi/tables/iort.asl     IORT: SMMU-500 + SDC1 stream ID (made by tools/gen_iort.py; unvalidated by iasl)
docs/EDK2_PLAN_UNOQ.md   Plan for an EDK2 payload loaded by the board's U-Boot
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
