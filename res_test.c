/* Host tests of the resource registry (replaces the old policy test). */
#include <stdio.h>
#include <string.h>
#include "../drivers/qcompwr/qcom_res_table.h"

typedef struct { int kind; u32 hw, en, val; } CALL;       /* kind: 0 clock, 1 reset, 2 regulator */
static CALL log_[256]; static int nlog; static int fail_next, notimpl_all;
static int rec(int kind, u32 hw, u32 en, u32 val)
{
    if (notimpl_all) return RES_E_NOTIMPL;
    if (fail_next) { fail_next = 0; return RES_E_HW; }
    log_[nlog].kind = kind; log_[nlog].hw = hw; log_[nlog].en = en; log_[nlog].val = val; nlog++; return RES_OK;
}
static int be_clk(void *c, u32 h, u32 e, u32 v) { (void)c; return rec(0, h, e, v); }
static int be_rst(void *c, u32 h)               { (void)c; return rec(1, h, 0, 0); }
static int be_reg(void *c, u32 h, u32 e, u32 v) { (void)c; return rec(2, h, e, v); }

static RES_REGISTRY R;
static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); fails++; } else printf("ok:   %s\n", m); } while (0)
static int count_calls(int kind, u32 hw, int en) { int i, n = 0; for (i = 0; i < nlog; i++) if (log_[i].kind == kind && log_[i].hw == hw && (int)log_[i].en == en) n++; return n; }
static void fresh(void)
{
    RES_BACKEND be = { be_clk, be_rst, be_reg, 0 };
    nlog = 0; fail_next = 0; notimpl_all = 0;
    res_init(&R, g_Rb1Resources, RB1_RES_COUNT, &be);
}
static int idx(const char *n) { return res_lookup(&R, n); }

int main(void)
{
    int a, b, rc, i;
    RES_BACKEND be = { be_clk, be_rst, be_reg, 0 };

    fresh();
    CHECK(count_calls(2, QHW_REG_PM4125_L14, 1) == 1 && log_[0].val == 1800000, "init: always-on L14 enabled at 1.8 V by the service");
    CHECK(idx("sdc1_apps") >= 0 && idx("nope") == RES_E_NOENT && idx(NULL) == RES_E_NOENT, "lookup by name; unknown names rejected");

    /* the always-on rule */
    a = res_client_open(&R);
    CHECK(a == 1, "client ids start at 1 (0 is the service)");
    CHECK(res_acquire(&R, a, idx("sdc1_vqmmc"), 1800000) == RES_OK, "client may acquire L14 at 1.8 V");
    CHECK(res_acquire(&R, a, idx("sdc1_vqmmc"), 2500000) == RES_E_RANGE, "L14 other voltages refused");
    CHECK(res_release(&R, a, idx("sdc1_vqmmc")) == RES_OK && count_calls(2, QHW_REG_PM4125_L14, 0) == 0, "releasing L14 never switches it off");

    /* limits */
    CHECK(res_acquire(&R, a, idx("sdc1_vmmc"), 5000000) == RES_E_RANGE, "L20 above max refused");
    CHECK(res_acquire(&R, a, idx("sdc1_vmmc"), 1800000) == RES_E_RANGE, "L20 below min refused");
    CHECK(res_acquire(&R, a, idx("sdc2_vqmmc"), 3000000) == RES_E_RANGE, "L4 capped at 2.7 V");
    CHECK(res_acquire(&R, a, idx("sdc2_vqmmc"), 2700000) == RES_OK, "L4 at 2.7 V allowed");
    CHECK(res_acquire(&R, a, idx("sdc1_apps"), 500000000u) == RES_E_RANGE, "clock above 384 MHz refused");
    CHECK(res_acquire(&R, a, idx("sdc1_bcr"), 0) == RES_E_TYPE, "acquire on a reset line refused");
    CHECK(res_reset(&R, a, idx("sdc1_apps")) == RES_E_TYPE, "reset on a clock refused");
    CHECK(res_reset(&R, a, idx("sdc1_bcr")) == RES_OK && count_calls(1, QHW_RST_SDC1_BCR, 0) == 1, "reset pulse reaches the backend");
    res_client_close(&R, a);

    /* shared clock arbitration */
    fresh(); a = res_client_open(&R); b = res_client_open(&R);
    res_acquire(&R, a, idx("sdc1_apps"), 100000000u);
    CHECK(count_calls(0, QHW_CLK_SDC1_APPS, 1) == 1 && log_[nlog-1].val == 100000000u, "first holder enables at its rate");
    res_acquire(&R, b, idx("sdc1_apps"), 192000000u);
    CHECK(log_[nlog-1].val == 192000000u, "second holder raises the shared rate (max wins)");
    res_acquire(&R, a, idx("sdc1_apps"), 100000000u);
    CHECK(R.St[idx("sdc1_apps")].Count == 2, "re-acquire by same client does not double-count");
    res_release(&R, b, idx("sdc1_apps"));
    CHECK(log_[nlog-1].en == 1 && log_[nlog-1].val == 100000000u, "release of the fast holder lowers the rate to the remaining request");
    res_release(&R, a, idx("sdc1_apps"));
    CHECK(count_calls(0, QHW_CLK_SDC1_APPS, 0) == 1, "last release turns the clock off");
    CHECK(res_release(&R, a, idx("sdc1_apps")) == RES_E_NOTHELD, "release when not held reported");

    /* shared rail */
    fresh(); a = res_client_open(&R); b = res_client_open(&R);
    CHECK(res_acquire(&R, a, idx("sdc1_vmmc"), 2950000) == RES_OK, "rail acquired");
    CHECK(res_acquire(&R, b, idx("sdc1_vmmc"), 3300000) == RES_E_BUSY, "different voltage from another holder -> BUSY");
    CHECK(R.St[idx("sdc1_vmmc")].Count == 1 && !R.St[idx("sdc1_vmmc")].Held[b], "BUSY leaves no partial state");
    CHECK(res_acquire(&R, b, idx("sdc1_vmmc"), 2950000) == RES_OK && R.St[idx("sdc1_vmmc")].Count == 2, "same voltage shares the rail");
    res_release(&R, a, idx("sdc1_vmmc"));
    CHECK(count_calls(2, QHW_REG_PM4125_L20, 0) == 0, "rail stays on while another holder remains");
    res_release(&R, b, idx("sdc1_vmmc"));
    CHECK(count_calls(2, QHW_REG_PM4125_L20, 0) == 1, "rail off after last release");

    /* crash safety: client disappears */
    fresh(); a = res_client_open(&R);
    res_acquire(&R, a, idx("xo"), 0); res_acquire(&R, a, idx("sdc1_ahb"), 0);
    res_acquire(&R, a, idx("sdc1_vmmc"), 2950000); res_acquire(&R, a, idx("sdc1_vqmmc"), 1800000);
    rc = res_client_close(&R, a);
    CHECK(rc == RES_OK && count_calls(0, QHW_CLK_XO, 0) == 1 && count_calls(0, QHW_CLK_SDC1_AHB, 0) == 1 && count_calls(2, QHW_REG_PM4125_L20, 0) == 1,
          "closing a client releases everything it held");
    CHECK(count_calls(2, QHW_REG_PM4125_L14, 0) == 0, "...but never L14");
    CHECK(res_acquire(&R, a, idx("xo"), 0) == RES_E_NOCLIENT, "closed client id is invalid");

    /* hardware failure handling */
    fresh(); a = res_client_open(&R);
    fail_next = 1;
    CHECK(res_acquire(&R, a, idx("sdc1_vmmc"), 2950000) == RES_E_HW && R.St[idx("sdc1_vmmc")].Count == 0, "failed enable leaves no hold");
    CHECK(res_acquire(&R, a, idx("sdc1_vmmc"), 2950000) == RES_OK, "retry after failure succeeds");
    fail_next = 1;
    CHECK(res_release(&R, a, idx("sdc1_vmmc")) == RES_E_HW && R.St[idx("sdc1_vmmc")].Held[a], "failed disable keeps the hold (rail is still on)");

    /* bring-up mode: backend not written */
    nlog = 0; fail_next = 0; notimpl_all = 1; res_init(&R, g_Rb1Resources, RB1_RES_COUNT, &be);
    CHECK(R.Adopted == 1 && R.St[idx("sdc1_vqmmc")].Count == 1, "backend NOTIMPL: always-on rail adopted as held, init still succeeds");
    a = res_client_open(&R);
    CHECK(res_acquire(&R, a, idx("sdc1_vmmc"), 2950000) == RES_E_NOTIMPL, "runtime requests report NOTIMPL (no fake success)");

    /* client table */
    fresh();
    for (i = 1; i < RES_MAX_CLIENTS; i++) res_client_open(&R);
    CHECK(res_client_open(&R) == RES_E_NOCLIENT, "client table exhaustion reported");

    printf("%s (%d failure%s)\n", fails ? "RESOURCE TESTS FAILED" : "RESOURCE TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails != 0;
}
