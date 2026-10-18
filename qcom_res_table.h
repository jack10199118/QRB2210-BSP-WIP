/* Resource table for the QRB2210 RB1 board (qrb2210-rb1.dts, qcm2290.dtsi).
 * Today it is compiled in; the intent is to build the same table from ACPI _DSD at start-up. */
#ifndef QCOM_RES_TABLE_H
#define QCOM_RES_TABLE_H
#include "qcom_res.h"

/* backend hardware identifiers (service-internal; clients only use names) */
enum { QHW_CLK_XO = 0, QHW_CLK_SDC1_AHB, QHW_CLK_SDC1_APPS, QHW_CLK_SDC1_ICE_CORE, QHW_CLK_SDC2_AHB, QHW_CLK_SDC2_APPS };
enum { QHW_RST_SDC1_BCR = 0, QHW_RST_SDC2_BCR };
enum { QHW_REG_PM4125_L20 = 0, QHW_REG_PM4125_L14, QHW_REG_PM4125_L21, QHW_REG_PM4125_L4 };

static const RES_DESC g_Rb1Resources[] = {
    { "xo",            RES_CLOCK,     QHW_CLK_XO,           0,       0,          0 },
    { "sdc1_ahb",      RES_CLOCK,     QHW_CLK_SDC1_AHB,     0,       0,          0 },
    { "sdc1_apps",     RES_CLOCK,     QHW_CLK_SDC1_APPS,    100000,  384000000u, 0 },
    { "sdc1_ice_core", RES_CLOCK,     QHW_CLK_SDC1_ICE_CORE,0,       0,          0 },
    { "sdc2_ahb",      RES_CLOCK,     QHW_CLK_SDC2_AHB,     0,       0,          0 },
    { "sdc2_apps",     RES_CLOCK,     QHW_CLK_SDC2_APPS,    100000,  384000000u, 0 },
    { "sdc1_bcr",      RES_RESET,     QHW_RST_SDC1_BCR,     0,       0,          0 },
    { "sdc2_bcr",      RES_RESET,     QHW_RST_SDC2_BCR,     0,       0,          0 },
    /* PM4125 LDOs; limits from the board DTS */
    { "sdc1_vmmc",     RES_REGULATOR, QHW_REG_PM4125_L20,   2400000, 3600000,    0 },
    { "sdc1_vqmmc",    RES_REGULATOR, QHW_REG_PM4125_L14,   1800000, 1800000,    1 },  /* "never turn it off" */
    { "sdc2_vmmc",     RES_REGULATOR, QHW_REG_PM4125_L21,   2960000, 3300000,    0 },
    { "sdc2_vqmmc",    RES_REGULATOR, QHW_REG_PM4125_L4,    1800000, 2700000,    0 },  /* 3 V signalling disabled */
};
#define RB1_RES_COUNT (sizeof(g_Rb1Resources) / sizeof(g_Rb1Resources[0]))
#endif
