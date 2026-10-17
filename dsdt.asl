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

        // Clock/regulator service. GCC registers; regulators go through the RPM (TODO(HW)).
        Device (PWR0)
        {
            Name (_HID, "QCOM0002")          // must match qcompwr.inf
            Name (_UID, 0)
            Method (_CRS, 0, NotSerialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite, 0x01400000, 0x001F0000)   // GCC
                })
                Return (RBUF)
            }
        }

        // TLMM GPIO/pinctrl: 0x00500000 (0x300000), SPI 227 -> GSIV 259, 127 pins.
        // Needs a GpioClx driver (not written). Reserved here so other devices can
        // later reference it with GpioInt/GpioIo.
        Device (GPI0)
        {
            Name (_HID, "QCOM0003")          // TODO: INF + GpioClx driver
            Name (_UID, 0)
            Method (_CRS, 0, NotSerialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite, 0x00500000, 0x00300000)
                    Interrupt (ResourceConsumer, Level, ActiveHigh, Shared) { 259 }
                })
                Return (RBUF)
            }
        }

        // GENI UART4: 0x04A90000, SPI 331 -> GSIV 363.
        // RB1: serial0 / stdout, "qcom,geni-debug-uart", wired to the Micro-USB
        // port through an FTDI chip. Use this one for KD (DBG2 entry).
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
            Name (_DEP, Package () { \_SB.PWR0 })   // power service must start first
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
            // PEP/power driver; ACPI cannot do this by itself.
            // RB1 board (qrb2210-rb1.dts): non-removable eMMC, CQE supported.
            //   VMMC  = PM4125 L20 (2.4-3.6 V, switchable)
            //   VQMMC = PM4125 L14 (1.8 V fixed, "broken hardware, never turn it off")
        }

        // SDC2 (SD slot, 4-bit): 0x04784000, hc_irq SPI 350 (GSIV 382), pwr_irq SPI 353 (GSIV 385)
        // Card detect: TLMM GPIO 88, active low. VMMC = PM4125 L21 (2.96-3.3 V),
        // VQMMC = PM4125 L4 (1.8-2.7 V; capped to disable 3 V signalling).
        Device (SDC2)
        {
            Name (_HID, "QCOM0001")          // same controller family, same driver
            Name (_UID, 2)
            Name (_CCA, 0)
            Method (_CRS, 0, NotSerialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite, 0x04784000, 0x1000)
                    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 382 }
                    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 385 }
                })
                Return (RBUF)
            }
        }
    }
}
