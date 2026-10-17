# Turning emmc.c into a disk Windows can use

Windows expects SD/eMMC host controllers behind **sdport.sys**. A miniport is a
WDM-style driver that links `sdport.lib` and registers callbacks; it is not a KMDF
driver. The current `emmc.c` is KMDF, so it must be split.

## Refactor
1. `emmc_core.c/.h`: no WDF types. Plain functions on a `EMMC_CORE` struct holding
   the mapped register pointer and the ADMA table pointers/addresses:
   reset, set clock, set bus width/timing, issue command, build ADMA2 table,
   read response, handle interrupt status, pwr_irq handshake.
2. `emmc_kmdf.c`: today's file, kept as a bring-up shell (KMDF + IOCTL test
   harness) so the core can be exercised before the storage stack is involved.
3. `emmc_sdport.c`: the SDPORT miniport shell calling the same core.

## SDPORT callbacks to implement (take exact prototypes from the WDK `sdport.h`)
| SDPORT concept | Core function |
|---|---|
| Slot count / capabilities | one slot; 8-bit, ADMA2, HS200 (no HS400 until DLL works); read SDHCI capability regs |
| Initialize | core reset, enable interrupts, program PWRCTL mask |
| Issue request | command + optional ADMA2 data transfer |
| Get response | read SDHCI response regs |
| Interrupt / DPC | read+clear INT_STATUS, `pwr_irq` handshake |
| Bus operations | clock/width/timing/voltage/tuning |
| Card detect | eMMC is non-removable: always present |
| Power control | PM4125 L20/L14 via the power service |
| Save / restore context | registers lost across power-down |

## Questions to settle with the WDK in hand
- Whether DMA in the miniport uses the framework's DMA adapter or only the slot's
  own ADMA2 with addresses supplied by sdport.
- How the miniport gets its ACPI resources (it receives the translated resource list).
- Whether eMMC boot partitions/RPMB need extra handling.
