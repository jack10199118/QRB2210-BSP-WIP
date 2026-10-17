# QRB2210-BSP-WIP


# QRB2210 Windows BSP scaffold

**Status: scaffold, not a bootable BSP.** SoC values were taken from the supplied
`qcm2290.dtsi`. Remaining `TODO(HW)`/`TODO(BOARD)` items need the board DTS and
Qualcomm documentation. Nothing here
has been compiled or run on hardware.

## Blocking risks (resolve before writing more code)
1. **CPU level.** QRB2210 = 4x Cortex-A53 (ARMv8.0). Recent Windows 11 ARM64
   builds require newer ISA features (e.g. ARMv8.1 atomics). Confirm against
   current Microsoft hardware requirements. This may be a hard stop.
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

## Layout
```
acpi/dsdt.asl            ACPI skeleton: CPUs, GIC, timer, UART, SDC1
drivers/emmc/emmc.c      KMDF SDHCI-style eMMC host core (MMIO, IRQ, ADMA2)
drivers/emmc/emmc.inf    ARM64 INF
```
Not included (need vendor input): PEP/clock+regulator driver, TLMM GPIO
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
