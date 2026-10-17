/*
 * QRB2210 (QCM2290) DSDT skeleton.
 * Values come from qcm2290.dtsi (Linux). ACPI GSIV = DT SPI number + 32.
 * Not in the DSDT (belongs in other tables, see README):
 *   GICv3  GICD 0x0F200000, GICR 0x0F300000 (0x100000, stride 0x20000) -> MADT
 *   Arch timer PPIs (DT PPI 1,2,3,0 => INTID 17,18,19,16)               -> GTDT
 *   MMIO timer 0x0F120000, SPI 8/7 (GSIV 40/39)                         -> GTDT
 *   SMMU-500 0x0C600000 (SDC1 SID 0xC0, SDC2 SID 0xA0)                  -> IORT
 *   PSCI 1.0 (HVC/SMC per firmware), 4x Cortex-A53 MPIDR 0..3           -> FADT/MADT
 */
DefinitionBlock ("dsdt.aml", "DSDT", 2, "QCOM  ", "QRB2210 ", 1)
{
    Scope (\_SB)
    {
        Device (CPU0) { Name (_HID, "ACPI0007") Name (_UID, 0) }
        Device (CPU1) { Name (_HID, "ACPI0007") Name (_UID, 1) }
        Device (CPU2) { Name (_HID, "ACPI0007") Name (_UID, 2) }
        Device (CPU3) { Name (_HID, "ACPI0007") Name (_UID, 3) }

        // GENI UART4: 0x04A90000, SPI 331 -> GSIV 363.
        // TODO(BOARD): confirm which UART is the debug console (board DTS).
        Device (UAR4)
        {
            Name (_HID, "QCOM0000")          // TODO: real HID + serial driver
            Name (_UID, 4)
            Method (_CRS, 0, NotSerialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite, 0x04A90000, 0x4000)
                    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 363 }
                })
                Return (RBUF)
            }
        }

        // SDC1 (eMMC, 8-bit): hc @ 0x04744000, CQHCI @ 0x04745000, ICE @ 0x04748000
        // Order of interrupts matters: [0]=hc_irq, [1]=pwr_irq (driver depends on it).
        Device (SDC1)
        {
            Name (_HID, "QCOM0001")          // must match emmc.inf
            Name (_UID, 1)
            Name (_CCA, 0)                   // TODO(HW): confirm DMA coherency (0 = non-coherent, safe)
            Method (_CRS, 0, NotSerialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite, 0x04744000, 0x1000)   // hc
                    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 380 } // hc_irq  SPI 348
                    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 384 } // pwr_irq SPI 352
                })
                Return (RBUF)
            }
            // Clocks (GCC): SDCC1_AHB, SDCC1_APPS, XO, SDCC1_ICE_CORE; reset GCC_SDCC1_BCR;
            // power domain VDDCX; OPPs 100/192/384 MHz. All must be sequenced by a
            // PEP/power driver; ACPI cannot do this by itself. TODO(BOARD): eMMC
            // vcc/vccq regulators from the board DTS.
        }
    }
}
