#include "qcom_res.h"

static int streq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int hw_apply(RES_REGISTRY *R, int Idx, u32 Enable, u32 Val)
{
    const RES_DESC *d = &R->Desc[Idx];
    if (d->Type == RES_CLOCK     && R->Be.Clock)     return R->Be.Clock(R->Be.Ctx, d->HwId, Enable, Val);
    if (d->Type == RES_REGULATOR && R->Be.Regulator) return R->Be.Regulator(R->Be.Ctx, d->HwId, Enable, Val);
    return RES_E_NOTIMPL;
}

/* Value the hardware should be programmed to given all current holders. */
static int effective(const RES_REGISTRY *R, int Idx, u32 *Eff)
{
    const RES_DESC *d = &R->Desc[Idx];
    const RES_STATE *s = &R->St[Idx];
    u32 i, v = 0; int any = 0;
    for (i = 0; i < RES_MAX_CLIENTS; i++) {
        if (!s->Held[i]) continue;
        if (!any) { v = s->Req[i]; any = 1; }
        else if (d->Type == RES_CLOCK) { if (s->Req[i] > v) v = s->Req[i]; }   /* highest requested rate wins */
        else if (s->Req[i] != v) return RES_E_BUSY;                            /* rails need one agreed voltage */
    }
    *Eff = v;
    return RES_OK;
}

static int valid_client(const RES_REGISTRY *R, int C)
{
    return C >= 0 && C < RES_MAX_CLIENTS && R->ClientOpen[C];
}

int res_acquire(RES_REGISTRY *R, int Client, int Idx, u32 Value)
{
    const RES_DESC *d; RES_STATE *s;
    u32 oldCount, oldReq, eff = 0; int wasHeld, rc;

    if (Idx < 0 || (u32)Idx >= R->NumDesc) return RES_E_NOENT;
    if (!valid_client(R, Client)) return RES_E_NOCLIENT;
    d = &R->Desc[Idx]; s = &R->St[Idx];
    if (d->Type == RES_RESET) return RES_E_TYPE;
    if (d->MinVal || d->MaxVal) { if (Value < d->MinVal || Value > d->MaxVal) return RES_E_RANGE; }
    else Value = 0;

    wasHeld = s->Held[Client]; oldReq = s->Req[Client]; oldCount = s->Count;
    s->Req[Client] = Value;
    if (!wasHeld) { s->Held[Client] = 1; s->Count++; }

    rc = effective(R, Idx, &eff);
    if (rc == RES_OK && (oldCount == 0 || eff != s->Value))
        rc = hw_apply(R, Idx, 1, eff);

    if (rc != RES_OK) {                       /* roll back: state must match the hardware */
        s->Req[Client] = oldReq;
        if (!wasHeld) { s->Held[Client] = 0; s->Count--; }
        return rc;
    }
    s->Value = eff;
    return RES_OK;
}

int res_release(RES_REGISTRY *R, int Client, int Idx)
{
    RES_STATE *s; u32 eff = 0; int rc;
    if (Idx < 0 || (u32)Idx >= R->NumDesc) return RES_E_NOENT;
    if (!valid_client(R, Client)) return RES_E_NOCLIENT;
    s = &R->St[Idx];
    if (!s->Held[Client]) return RES_E_NOTHELD;

    s->Held[Client] = 0; s->Count--;
    if (s->Count == 0) {
        rc = hw_apply(R, Idx, 0, 0);
        if (rc != RES_OK) { s->Held[Client] = 1; s->Count++; return rc; }   /* still on: keep the hold */
        s->Value = 0;
        return RES_OK;
    }
    rc = effective(R, Idx, &eff);                       /* remaining holders may allow a lower rate */
    if (rc == RES_OK && eff != s->Value) {
        rc = hw_apply(R, Idx, 1, eff);
        if (rc == RES_OK) s->Value = eff;
    }
    return rc;                                          /* the hold is gone regardless */
}

int res_reset(RES_REGISTRY *R, int Client, int Idx)
{
    if (Idx < 0 || (u32)Idx >= R->NumDesc) return RES_E_NOENT;
    if (!valid_client(R, Client)) return RES_E_NOCLIENT;
    if (R->Desc[Idx].Type != RES_RESET) return RES_E_TYPE;
    if (!R->Be.Reset) return RES_E_NOTIMPL;
    return R->Be.Reset(R->Be.Ctx, R->Desc[Idx].HwId);
}

int res_lookup(const RES_REGISTRY *R, const char *Name)
{
    u32 i;
    if (!Name) return RES_E_NOENT;
    for (i = 0; i < R->NumDesc; i++) if (streq(R->Desc[i].Name, Name)) return (int)i;
    return RES_E_NOENT;
}

int res_client_open(RES_REGISTRY *R)
{
    int c;
    for (c = 1; c < RES_MAX_CLIENTS; c++) if (!R->ClientOpen[c]) { R->ClientOpen[c] = 1; return c; }
    return RES_E_NOCLIENT;
}

int res_client_close(RES_REGISTRY *R, int Client)
{
    u32 i; int first = RES_OK, rc;
    if (Client <= 0 || Client >= RES_MAX_CLIENTS || !R->ClientOpen[Client]) return RES_E_NOCLIENT;
    for (i = 0; i < R->NumDesc; i++) {
        if (R->St[i].Held[Client]) { rc = res_release(R, Client, (int)i); if (rc && !first) first = rc; }
    }
    R->ClientOpen[Client] = 0;
    return first;
}

int res_init(RES_REGISTRY *R, const RES_DESC *Desc, u32 Num, const RES_BACKEND *Be)
{
    u32 i; int rc;
    u8 *p = (u8 *)R; u32 n;
    for (n = 0; n < sizeof(*R); n++) p[n] = 0;
    if (Num > RES_MAX) return RES_E_RANGE;
    R->Desc = Desc; R->NumDesc = Num; R->Be = *Be;
    R->ClientOpen[0] = 1;                                 /* the service itself */
    for (i = 0; i < Num; i++) {
        if (!Desc[i].AlwaysOn) continue;
        rc = res_acquire(R, 0, (int)i, Desc[i].MinVal);
        if (rc == RES_E_NOTIMPL) {
            /* Backend not written yet: assume firmware left the rail on, but still count the hold so
             * no client can ever turn it off. This is a deliberate bring-up decision. */
            R->St[i].Held[0] = 1; R->St[i].Req[0] = Desc[i].MinVal;
            R->St[i].Count = 1; R->St[i].Value = Desc[i].MinVal; R->Adopted++;
        } else if (rc != RES_OK) {
            return rc;                                    /* cannot guarantee the rail: refuse to start */
        }
    }
    return RES_OK;
}
