# What only you can do (in priority order)

## A. Decisions and answers that gate everything (ask Qualcomm / Microsoft)
1. Does any shippable Windows build run on a Cortex-A53 (ARMv8.0)? If not, stop here.
2. Can Qualcomm supply a Windows-capable boot chain (signed XBL/TZ + UEFI) for QRB2210?
3. Does Qualcomm provide, under NDA: the GCC clock/reset register map, the RPM
   (SMD) protocol for PM4125 regulators, and the SDCC v5 core register description?
4. Is there a KD (kernel debugger) transport for the GENI debug UART, or must you debug another way?

## B. Things that need a PC with tools (I could not run them here)
0. Already checked here without those tools: `sh tests/run_tests.sh` (43 checks: simulated
   controller/card, power-policy, ACPI table structure) and `sh tests/syntax_check.sh`.
1. Install ACPICA and run `tools/build_acpi.sh`. Send me every error and warning.
2. Create a KMDF ARM64 driver project in Visual Studio with the WDK, add
   `emmc.c` + `emmc_core.c` (and a second project for `qcom_pwr.c`). Build. Send me errors.
   (`tests/syntax_check.sh` only checked against hand-written stubs.)
3. Check the recalled Qualcomm core offsets (0x200..0x25C) and PWRCTL bits against
   Linux `drivers/mmc/host/sdhci-msm.c` in your kernel tree.

## C. Hardware time
1. Get UEFI printing on UART4, then boot WinPE with test signing.
2. Load `qcompwr` first (DSDT `_DEP` orders it), then `emmc`. Expected debugger output:
   `EMMC: card ready, N sectors` and `EMMC: LBA0 signature bytes 55 aa`.
   Then run `tools/emmctest.exe` (as Administrator; build it with `cl emmctest.c cfgmgr32.lib`)
   to read sectors through the driver's read-only IOCTL and confirm the `55 aa` signature.
   Send me the exact failure text if you see `card init failed, core error -X`
   (-1 timeout, -2 CRC, -3 ADMA, -4 bad arg, -5 card, -6 I/O).

## D. Material I can use to write the next pieces
- The WDK `sdport.h` (to write the SDPORT miniport shell).
- Qualcomm docs from A3 (to replace the NOT_IMPLEMENTED stubs in `qcom_pwr.c`).
