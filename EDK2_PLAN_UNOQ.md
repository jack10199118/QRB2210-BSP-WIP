# EDK2 payload plan for the Arduino UNO Q

## What is verified
From a boot log of a real board (zenn.dev, UNO Q 2 GB) and Arduino's published images:
XBL (BOOT.XF.4.1.2) -> Qualcomm UEFI 5.0 -> **U-Boot 2025.07-rc1 ("Arduino SA,Imola")** -> Linux EFI stub.
Secure Boot: Off. Boot interface: eMMC. U-Boot loads `qrb2210-arduino-imola.dtb` from the ESP
(`/boot/efi/dtb/qcom/`). Flashing and recovery use EDL (`qdl`, Firehose programmer).

So a custom payload can be chain-loaded from U-Boot; Qualcomm's signed firmware is untouched.

## Recommended route
Build EDK2 as an ARM64 payload that U-Boot loads (as a kernel `Image` via `booti`, or a FIT image) and
**take the memory map from the device tree U-Boot passes**, instead of extracting `uefiplat.cfg` from XBL.
(The edk2-msm projects wrap EDK2 to look like a Linux kernel for the stock bootloader; same idea, different loader.)

## Steps
1. Prove the handoff with the smallest possible AArch64 payload before any EDK2 work: boot it with U-Boot,
   confirm load address, that x0 carries the DTB pointer, and that UART4 output appears.
2. Write the EDK2 platform package: PCDs (GICv3 0x0F200000/0x0F300000, UART4 0x04A90000, timer from DT),
   a platform library whose memory map is parsed from the DT, a serial port library for the GENI debug UART.
3. Add the ACPI tables from `acpi/` and a console path. Display: unknown (see below).
4. Boot an EFI shell; then a Linux ACPI boot as a test, then Windows PE.

## Open items (need your input or the real hardware)
- **Display:** no display node upstream; the continuous-splash region is deleted. Check whether the stock UEFI leaves a
  framebuffer for the payload; if not, EDK2 has no graphical console and you depend on UART4 (1.8 V, JCTL header).
- **Reserved memory:** needs `agatti.dtsi`'s reserved-memory nodes (carve-outs for modem, ADSP, GPU zap, etc.).
- **GENI debug UART** register programming for the serial library (not in the DTS; take it from Linux's qcom_geni_serial).
- **U-Boot environment:** run `printenv` and the `help` list at the U-Boot prompt; the exact commands for staging an
  image are not confirmed (variable names like `fastboot_addr_r` were not verified).
- **Windows:** all earlier blockers still apply (boot chain support, drivers, and any ISA-level requirement of the
  Windows build you choose).
