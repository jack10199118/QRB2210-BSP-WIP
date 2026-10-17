/*
 * KMDF eMMC host controller core for QRB2210 SDC1 (SDHCI-compatible window).
 * Scaffold: resource mapping, reset, interrupts, ADMA2 common buffer.
 * NOT a complete storage driver; see README for SDPORT integration.
 * All offsets below are from the SD Host Controller Simplified Spec; verify
 * the Qualcomm host-register base offset (TODO(HW)).
 */
#include <ntddk.h>
#include <wdf.h>

#define EMMC_TAG 'cMME'

/* SDHCI register offsets (SD Host Controller Simplified Spec 4.x) */
#define SDHCI_ARGUMENT      0x08
#define SDHCI_XFER_MODE     0x0C
#define SDHCI_COMMAND       0x0E
#define SDHCI_RESPONSE      0x10
#define SDHCI_PRESENT_STATE 0x24
#define SDHCI_HOST_CTRL1    0x28
#define SDHCI_CLOCK_CTRL    0x2C
#define SDHCI_TIMEOUT_CTRL  0x2E
#define SDHCI_SW_RESET      0x2F
#define SDHCI_INT_STATUS    0x30
#define SDHCI_INT_ENABLE    0x34
#define SDHCI_SIGNAL_ENABLE 0x38
#define SDHCI_ADMA_ADDR     0x58

#define SDHCI_RESET_ALL     0x01
#define SDHCI_INT_ALL_MASK  0xFFFFFFFFu

/* From qcm2290.dtsi sdhc_1 (mmc@4744000), "hc" region: standard SDHCI regs
 * start at the base of the window. Qualcomm core regs (DLL, PWRCTL, vendor
 * specific) are NOT standard: mirror Linux drivers/mmc/host/sdhci-msm.c. */
#define EMMC_HOST_REG_OFFSET 0x0

/* qcm2290.dtsi: hc_irq = SPI 348, pwr_irq = SPI 352; ACPI GSIV = SPI + 32 */
#define EMMC_GSIV_HC   380
#define EMMC_GSIV_PWR  384
#define EMMC_BASE_HC   0x04744000ULL

#define ADMA_TABLE_BYTES    (PAGE_SIZE)

typedef struct _EMMC_CONTEXT {
    PUCHAR        Regs;           /* mapped host registers */
    SIZE_T        RegLength;
    WDFINTERRUPT  HcInterrupt;    /* hc_irq: SDHCI events */
    WDFINTERRUPT  PwrInterrupt;   /* pwr_irq: bus power / voltage-switch handshake */
    ULONG         PendingStatus;  /* written by ISR, consumed by DPC */
    WDFDMAENABLER DmaEnabler;
    WDFCOMMONBUFFER AdmaBuffer;
    PVOID         AdmaVa;
    PHYSICAL_ADDRESS AdmaLa;      /* device logical address (SMMU-aware) */
} EMMC_CONTEXT, *PEMMC_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(EMMC_CONTEXT, EmmcCtx)

/* ---- register accessors (always use READ/WRITE_REGISTER_*) ---- */
static FORCEINLINE ULONG  Rd32(PEMMC_CONTEXT c, ULONG o){ return READ_REGISTER_ULONG((PULONG)(c->Regs+EMMC_HOST_REG_OFFSET+o)); }
static FORCEINLINE VOID   Wr32(PEMMC_CONTEXT c, ULONG o, ULONG v){ WRITE_REGISTER_ULONG((PULONG)(c->Regs+EMMC_HOST_REG_OFFSET+o), v); }
static FORCEINLINE UCHAR  Rd8 (PEMMC_CONTEXT c, ULONG o){ return READ_REGISTER_UCHAR(c->Regs+EMMC_HOST_REG_OFFSET+o); }
static FORCEINLINE VOID   Wr8 (PEMMC_CONTEXT c, ULONG o, UCHAR v){ WRITE_REGISTER_UCHAR(c->Regs+EMMC_HOST_REG_OFFSET+o, v); }

DRIVER_INITIALIZE               DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD       EmmcDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE EmmcPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE EmmcReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY         EmmcD0Entry;
EVT_WDF_INTERRUPT_ISR           EmmcIsr;
EVT_WDF_INTERRUPT_ISR           EmmcPwrIsr;
EVT_WDF_INTERRUPT_DPC           EmmcDpc;

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT Drv, _In_ PUNICODE_STRING Reg)
{
    WDF_DRIVER_CONFIG cfg;
    WDF_DRIVER_CONFIG_INIT(&cfg, EmmcDeviceAdd);
    return WdfDriverCreate(Drv, Reg, WDF_NO_OBJECT_ATTRIBUTES, &cfg, WDF_NO_HANDLE);
}

NTSTATUS EmmcDeviceAdd(_In_ WDFDRIVER Drv, _Inout_ PWDFDEVICE_INIT Init)
{
    NTSTATUS st;
    WDFDEVICE dev;
    WDF_OBJECT_ATTRIBUTES attr;
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDF_DMA_ENABLER_CONFIG dc;
    PEMMC_CONTEXT c;
    UNREFERENCED_PARAMETER(Drv);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = EmmcPrepareHardware;
    pnp.EvtDeviceReleaseHardware = EmmcReleaseHardware;
    pnp.EvtDeviceD0Entry         = EmmcD0Entry;
    WdfDeviceInitSetPnpPowerEventCallbacks(Init, &pnp);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, EMMC_CONTEXT);
    st = WdfDeviceCreate(&Init, &attr, &dev);          /* context: non-paged */
    if (!NT_SUCCESS(st)) return st;
    c = EmmcCtx(dev);

    /* Two interrupts: created in PrepareHardware, one per resource descriptor. */
    /* ADMA2 supports 32-bit (or 64-bit with v4 mode); start with 32-bit. */
    WDF_DMA_ENABLER_CONFIG_INIT(&dc, WdfDmaProfileScatterGather, 0x10000 * 32);
    st = WdfDmaEnablerCreate(dev, &dc, WDF_NO_OBJECT_ATTRIBUTES, &c->DmaEnabler);
    return st;
}

NTSTATUS EmmcPrepareHardware(_In_ WDFDEVICE Dev, _In_ WDFCMRESLIST Raw, _In_ WDFCMRESLIST Tr)
{
    PEMMC_CONTEXT c = EmmcCtx(Dev);
    ULONG i, n = WdfCmResourceListGetCount(Tr);
    BOOLEAN gotMem = FALSE;
    ULONG nInt = 0;
    NTSTATUS st;

    for (i = 0; i < n; i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d = WdfCmResourceListGetDescriptor(Tr, i);
        if (!d) return STATUS_DEVICE_CONFIGURATION_ERROR;
        if (d->Type == CmResourceTypeMemory && !gotMem) {
            if (d->u.Memory.Start.QuadPart != (LONGLONG)EMMC_BASE_HC)
                return STATUS_DEVICE_CONFIGURATION_ERROR;
            c->RegLength = d->u.Memory.Length;
            c->Regs = MmMapIoSpaceEx(d->u.Memory.Start, c->RegLength,
                                     PAGE_READWRITE | PAGE_NOCACHE);
            if (!c->Regs) return STATUS_INSUFFICIENT_RESOURCES;
            gotMem = TRUE;
        } else if (d->Type == CmResourceTypeInterrupt) {
            /* _CRS order: [0] hc_irq, [1] pwr_irq. */
            if (nInt == 0 || nInt == 1) {
                WDF_INTERRUPT_CONFIG ic;
                WDFINTERRUPT *slot = (nInt == 0) ? &c->HcInterrupt : &c->PwrInterrupt;
                ULONG want = (nInt == 0) ? EMMC_GSIV_HC : EMMC_GSIV_PWR;
                if (d->u.Interrupt.Vector != want)
                    return STATUS_DEVICE_CONFIGURATION_ERROR;
                if (*slot == NULL) {          /* PrepareHardware can run again on restart */
                    WDF_INTERRUPT_CONFIG_INIT(&ic,
                        nInt == 0 ? EmmcIsr : EmmcPwrIsr,
                        nInt == 0 ? EmmcDpc : NULL);
                    ic.InterruptRaw        = WdfCmResourceListGetDescriptor(Raw, i);
                    ic.InterruptTranslated = d;
                    st = WdfInterruptCreate(Dev, &ic, WDF_NO_OBJECT_ATTRIBUTES, slot);
                    if (!NT_SUCCESS(st)) return st;
                }
            }
            nInt++;
        }
    }
    if (!gotMem || nInt != 2) return STATUS_DEVICE_CONFIGURATION_ERROR;

    /* ADMA2 descriptor table: physically contiguous, DMA-mapped, non-paged. */
    st = WdfCommonBufferCreate(c->DmaEnabler, ADMA_TABLE_BYTES,
                               WDF_NO_OBJECT_ATTRIBUTES, &c->AdmaBuffer);
    if (!NT_SUCCESS(st)) return st;
    c->AdmaVa = WdfCommonBufferGetAlignedVirtualAddress(c->AdmaBuffer);
    c->AdmaLa = WdfCommonBufferGetAlignedLogicalAddress(c->AdmaBuffer);
    RtlZeroMemory(c->AdmaVa, ADMA_TABLE_BYTES);
    return STATUS_SUCCESS;
}

NTSTATUS EmmcReleaseHardware(_In_ WDFDEVICE Dev, _In_ WDFCMRESLIST Tr)
{
    PEMMC_CONTEXT c = EmmcCtx(Dev);
    UNREFERENCED_PARAMETER(Tr);
    if (c->Regs) { MmUnmapIoSpace(c->Regs, c->RegLength); c->Regs = NULL; }
    return STATUS_SUCCESS;                /* common buffer freed with parent */
}

NTSTATUS EmmcD0Entry(_In_ WDFDEVICE Dev, _In_ WDF_POWER_DEVICE_STATE Prev)
{
    PEMMC_CONTEXT c = EmmcCtx(Dev);
    LARGE_INTEGER d; ULONG tries;
    UNREFERENCED_PARAMETER(Prev);

    /* TODO(HW): enable clocks + regulators (PEP / power framework) BEFORE this. */
    Wr8(c, SDHCI_SW_RESET, SDHCI_RESET_ALL);
    d.QuadPart = -10 * 1000;              /* 1 ms */
    for (tries = 0; tries < 100 && (Rd8(c, SDHCI_SW_RESET) & SDHCI_RESET_ALL); tries++)
        KeDelayExecutionThread(KernelMode, FALSE, &d);
    if (Rd8(c, SDHCI_SW_RESET) & SDHCI_RESET_ALL) return STATUS_IO_TIMEOUT;

    Wr32(c, SDHCI_INT_ENABLE,    SDHCI_INT_ALL_MASK);
    Wr32(c, SDHCI_SIGNAL_ENABLE, SDHCI_INT_ALL_MASK);
    Wr32(c, SDHCI_ADMA_ADDR,     c->AdmaLa.LowPart);
    /* TODO: clock divider setup, bus power, card init (CMD0/1/2/3/7), HS200 tuning */
    return STATUS_SUCCESS;
}

BOOLEAN EmmcIsr(_In_ WDFINTERRUPT Int, _In_ ULONG Msg)
{
    PEMMC_CONTEXT c = EmmcCtx(WdfInterruptGetDevice(Int));
    ULONG s;
    UNREFERENCED_PARAMETER(Msg);
    s = Rd32(c, SDHCI_INT_STATUS);
    if (s == 0 || s == 0xFFFFFFFF) return FALSE;     /* not ours / device gone */
    Wr32(c, SDHCI_INT_STATUS, s);                    /* ack (W1C) */
    c->PendingStatus |= s;                           /* ISR-synchronized with DPC */
    WdfInterruptQueueDpcForIsr(Int);
    return TRUE;
}

VOID EmmcDpc(_In_ WDFINTERRUPT Int, _In_ WDFOBJECT Obj)
{
    PEMMC_CONTEXT c = EmmcCtx(WdfInterruptGetDevice(Int));
    ULONG s;
    UNREFERENCED_PARAMETER(Obj);
    WdfInterruptAcquireLock(Int);
    s = c->PendingStatus; c->PendingStatus = 0;
    WdfInterruptReleaseLock(Int);
    /* TODO: dispatch command-complete / transfer-complete / error bits to request state machine */
    UNREFERENCED_PARAMETER(s);
}

/* pwr_irq: Qualcomm-specific power-control interrupt. The controller raises it
 * when software changes bus power / I/O voltage; the driver must read the
 * PWRCTL status, apply the regulator change, then write PWRCTL_CTL to ack.
 * TODO(HW): implement from sdhci-msm.c (sdhci_msm_handle_pwr_irq). Until then
 * voltage switching (needed for HS200/HS400) will hang. */
BOOLEAN EmmcPwrIsr(_In_ WDFINTERRUPT Int, _In_ ULONG Msg)
{
    UNREFERENCED_PARAMETER(Int); UNREFERENCED_PARAMETER(Msg);
    return FALSE;
}
