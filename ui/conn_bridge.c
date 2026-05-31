// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "global.h"
#include "lib.h"
#include "menu.h"
#include "mystrings.h"
#include "screenmgt.h"
#include "conn_bridge.h"
#include "display.h"
#include "sysinfo.h"
#include "netinfo.h"
#include "conn_core.h"
#include "pointer.h"
#include "splash_atlas.h"
#include <Protocol/Rng.h>
#include <Protocol/SimplePointer.h>
#include <Library/BaseLib.h>
#include "platform/cpu.h"
#include "platform/reset_reason.h"

extern BOOLEAN GraphicsScreenDirty;
extern BOOLEAN SubScreenBoot;

static UINTN gConnThemeIdx = 0;

static VOID ConnLoadTheme(VOID)
{
    UINT32 *Data = NULL;
    UINTN Size = 0;
    if (!EFI_ERROR(EfivarGetRaw(&MeridianGuid, L"ConnTheme", (VOID **)&Data, &Size)) &&
        Data != NULL && Size >= sizeof(UINT32)) {
        if (*Data < CONN_THEME_COUNT)
            gConnThemeIdx = *Data;
    }
    if (Data != NULL)
        MRD_FREE_POOL(Data);
}

static VOID ConnSaveTheme(VOID)
{
    UINT32 Val = (UINT32)gConnThemeIdx;
    EfivarSetRaw(&MeridianGuid, L"ConnTheme", &Val, sizeof Val, TRUE);
}

static const ConnTextTheme *ConnCycleTheme(VOID)
{
    gConnThemeIdx = (gConnThemeIdx + 1) % CONN_THEME_COUNT;
    ConnSaveTheme();
    return conn_themes[gConnThemeIdx];
}

#define CONN_THEME (conn_themes[gConnThemeIdx])

static UINTN ConnEditOptions(MERIDIAN_MENU_ENTRY *Entry, MERIDIAN_MENU_ENTRY **ChosenEntry);

BOOLEAN ConnScreensActive(MERIDIAN_MENU_SCREEN *Screen)
{
    if (!AllowGraphicsMode || (GOPDraw == NULL && UGADraw == NULL) || Screen == NULL) {
        return FALSE;
    }
    if (Screen->Title != NULL && MrdStrEqualsCI(Screen->Title, MAIN_MENU_NAME)) {
        return FALSE;
    }
    return TRUE;
}

static BOOLEAN VideoSize(UINTN *sw, UINTN *sh)
{
    if (GOPDraw != NULL) {
        *sw = GOPDraw->Mode->Info->HorizontalResolution;
        *sh = GOPDraw->Mode->Info->VerticalResolution;
        return (*sw > 0 && *sh > 0);
    }
    if (UGADraw != NULL) {
        UINT32 W = 0, H = 0, D = 0, R = 0;
        if (!EFI_ERROR(UGADraw->GetMode(UGADraw, &W, &H, &D, &R)) && W > 0 && H > 0) {
            *sw = W;
            *sh = H;
            return TRUE;
        }
    }
    return FALSE;
}

static VOID VideoBlt(EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Buf, BOOLEAN Fill, UINTN Dx, UINTN Dy, UINTN W,
                     UINTN H, UINTN Delta)
{
    if (GOPDraw != NULL) {
        GOPDraw->Blt(GOPDraw, Buf, Fill ? EfiBltVideoFill : EfiBltBufferToVideo, 0, 0, Dx, Dy, W, H,
                     Delta);
    }
    else if (UGADraw != NULL) {
        UGADraw->Blt(UGADraw, (EFI_UGA_PIXEL *)Buf, Fill ? EfiUgaVideoFill : EfiUgaBltBufferToVideo,
                     0, 0, Dx, Dy, W, H, Delta);
    }
}

static VOID Ascii16(CHAR8 *Dst, UINTN Cap, CONST CHAR16 *Src)
{
    UINTN i = 0;
    if (Cap == 0) {
        return;
    }
    if (Src != NULL) {
        for (; Src[i] != 0 && i + 1 < Cap; i++) {
            CHAR16 c = Src[i];
            Dst[i] = (c >= 0x20 && c < 0x7F) ? (CHAR8)c : '?';
        }
    }
    Dst[i] = '\0';
}

static BOOLEAN StartsWithCI(CONST CHAR8 *S, CONST CHAR8 *Pfx)
{
    UINTN i;
    for (i = 0; Pfx[i] != 0; i++) {
        CHAR8 a = S[i], b = Pfx[i];
        if (a >= 'a' && a <= 'z')
            a = (CHAR8)(a - 32);
        if (b >= 'a' && b <= 'z')
            b = (CHAR8)(b - 32);
        if (a != b)
            return FALSE;
    }
    return TRUE;
}

static VOID CleanOsName(CHAR8 *Dst, UINTN Cap, CONST CHAR16 *Raw)
{
    UINTN i, off, n;
    Ascii16(Dst, Cap, Raw);

    for (i = 0; Dst[i] != 0; i++) {
        if (Dst[i] == ' ' &&
            (StartsWithCI(Dst + i + 1, "from ") || StartsWithCI(Dst + i + 1, "on "))) {
            Dst[i] = '\0';
            break;
        }
    }
    off = 0;
    if (StartsWithCI(Dst + off, "Load "))
        off += 5;
    if (StartsWithCI(Dst + off, "Instance: "))
        off += 10;
    if (StartsWithCI(Dst + off, "Manual Stanza: "))
        off += 15;
    if (off != 0) {
        for (n = 0; off + n < Cap && Dst[off + n] != 0; n++)
            Dst[n] = Dst[off + n];
        Dst[n] = '\0';
    }

    if (StartsWithCI(Dst, "Linux via ")) {
        for (i = 0; Dst[i] != 0; i++) {
            if (Dst[i] == ' ' && Dst[i + 1] == '-' && Dst[i + 2] == ' ') {
                off = i + 3;
                for (n = 0; Dst[off + n] != 0; n++)
                    Dst[n] = Dst[off + n];
                Dst[n] = '\0';
                break;
            }
        }
    }
}

static CONST CHAR8 *Dow3(UINTN y, UINTN m, UINTN d)
{
    static CONST CHAR8 *N[7] = {"SAT", "SUN", "MON", "TUE", "WED", "THU", "FRI"};
    UINTN K, J, h;
    if (m < 3) {
        m += 12;
        y -= 1;
    }
    K = y % 100;
    J = y / 100;
    h = (d + (13 * (m + 1)) / 5 + K + K / 4 + J / 4 + 5 * J) % 7;
    return N[h];
}

static CONST CHAR8 *Mon3(UINTN m)
{
    static CONST CHAR8 *N[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                 "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    return (m >= 1 && m <= 12) ? N[m - 1] : "???";
}

static VOID FillClock(ConnEntryList *L)
{
    EFI_TIME t;
    CONST CHAR8 *s;
    UINTN p = 0;

    L->clock[0] = '\0';
    if (gRT == NULL || EFI_ERROR(gRT->GetTime(&t, NULL))) {
        return;
    }
#define D2(v)                                                                                      \
    do {                                                                                           \
        L->clock[p++] = (CHAR8)('0' + ((v) / 10) % 10);                                            \
        L->clock[p++] = (CHAR8)('0' + (v) % 10);                                                   \
    } while (0)
    D2(t.Hour);
    L->clock[p++] = ':';
    D2(t.Minute);
    L->clock[p++] = ' ';
    L->clock[p++] = ' ';
    L->clock[p++] = ' ';
    s = Dow3(t.Year, t.Month, t.Day);
    L->clock[p++] = s[0];
    L->clock[p++] = s[1];
    L->clock[p++] = s[2];
    L->clock[p++] = ' ';
    D2(t.Day);
    L->clock[p++] = ' ';
    s = Mon3(t.Month);
    L->clock[p++] = s[0];
    L->clock[p++] = s[1];
    L->clock[p++] = s[2];
    L->clock[p++] = ' ';
    L->clock[p++] = (CHAR8)('0' + (t.Year / 1000) % 10);
    L->clock[p++] = (CHAR8)('0' + (t.Year / 100) % 10);
    L->clock[p++] = (CHAR8)('0' + (t.Year / 10) % 10);
    L->clock[p++] = (CHAR8)('0' + t.Year % 10);
    L->clock[p] = '\0';
#undef D2
}

static VOID FillRenderRes(ConnEntryList *L)
{
    UINTN W = 0;
    UINTN H = 0;
    UINTN p = 0;
    UINTN div;

    L->res[0] = '\0';
    if (!VideoSize(&W, &H) || W == 0 || H == 0 || W > 99999 || H > 99999) {
        return;
    }

#define DEC(v)                                                                                     \
    do {                                                                                           \
        for (div = 10000; div >= 10; div /= 10) {                                                  \
            if ((v) >= div) {                                                                      \
                L->res[p++] = (CHAR8)('0' + ((v) / div) % 10);                                     \
            }                                                                                      \
        }                                                                                          \
        L->res[p++] = (CHAR8)('0' + (v) % 10);                                                     \
    } while (0)
    DEC(W);
    L->res[p++] = 'x';
    DEC(H);
    L->res[p] = '\0';
#undef DEC
}

static VOID FillRngNonce(ConnEntryList *L)
{
    EFI_RNG_PROTOCOL *Rng;
    EFI_STATUS Status;
    UINT8 Bytes[4];
    UINTN i;
    CONST CHAR8 *Source;
    static CONST CHAR8 Hex[] = "0123456789ABCDEF";

    if (L == NULL) {
        return;
    }
    L->rng_nonce[0] = '\0';
    L->rng_source[0] = '\0';
    Source = NULL;

    if (gBS == NULL) {
        goto TryRdRand;
    }
    Rng = NULL;
    Status = gBS->LocateProtocol(&gEfiRngProtocolGuid, NULL, (VOID **)&Rng);
    if (EFI_ERROR(Status) || Rng == NULL || Rng->GetRNG == NULL) {
        goto TryRdRand;
    }

    Status = Rng->GetRNG(Rng, NULL, sizeof(Bytes), Bytes);
    if (EFI_ERROR(Status)) {
        goto TryRdRand;
    }
    Source = "RNG";
    goto Done;

TryRdRand:
#if defined(MDE_CPU_IA32) || defined(MDE_CPU_X64)
{
    UINT32 Word = 0;

    if (MeridianGetCpuInfo()->HasRdrand) {
        for (i = 0; i < 10; i++) {
            if (AsmRdRand32(&Word)) {
                Bytes[0] = (UINT8)(Word >> 24);
                Bytes[1] = (UINT8)(Word >> 16);
                Bytes[2] = (UINT8)(Word >> 8);
                Bytes[3] = (UINT8)Word;
                Source = "CPU";
                goto Done;
            }
        }
    }
}
#endif

    if (gBS != NULL) {
        UINT64 Count = 0;

        Status = gBS->GetNextMonotonicCount(&Count);
        if (!EFI_ERROR(Status)) {
            UINT32 Word = (UINT32)Count ^ (UINT32)(Count >> 32) ^ (UINT32)(UINTN)L ^ 0x4D52444Eu;

            Bytes[0] = (UINT8)(Word >> 24);
            Bytes[1] = (UINT8)(Word >> 16);
            Bytes[2] = (UINT8)(Word >> 8);
            Bytes[3] = (UINT8)Word;
            Source = "FW";
            goto Done;
        }
    }

    return;

Done:
    if (Source != NULL) {
        for (i = 0; Source[i] != '\0' && i + 1 < sizeof(L->rng_source); i++) {
            L->rng_source[i] = Source[i];
        }
        L->rng_source[i] = '\0';
    }
    for (i = 0; i < sizeof(Bytes); i++) {
        L->rng_nonce[i * 2] = Hex[(Bytes[i] >> 4) & 0x0F];
        L->rng_nonce[i * 2 + 1] = Hex[Bytes[i] & 0x0F];
    }
    L->rng_nonce[sizeof(Bytes) * 2] = '\0';
}

static VOID FillSysInfo(ConnEntryList *L)
{
    ConnSysInfo *S = &L->sys;
    CHAR8 tmp[64];
    UINTN n = 0;

    SetMem(S, sizeof(*S), 0);

#define SYS_LINE(...)                                                                              \
    do {                                                                                           \
        if (n < CONN_SYS_LINES) {                                                                  \
            UINTN _k;                                                                              \
            AsciiSPrint(tmp, sizeof(tmp), __VA_ARGS__);                                            \
            for (_k = 0; tmp[_k] && _k < CONN_SYS_COLS - 1; _k++)                                  \
                S->line[n][_k] = tmp[_k];                                                          \
            S->line[n][_k] = '\0';                                                                 \
            n++;                                                                                   \
        }                                                                                          \
    } while (0)
#define SYS_GAP()                                                                                  \
    do {                                                                                           \
        if (n < CONN_SYS_LINES) {                                                                  \
            S->line[n][0] = '\0';                                                                  \
            n++;                                                                                   \
        }                                                                                          \
    } while (0)

    BOOLEAN First = TRUE;
#define SECTION(title)                                                                             \
    do {                                                                                           \
        if (!First) {                                                                              \
            SYS_GAP();                                                                             \
        }                                                                                          \
        First = FALSE;                                                                             \
        SYS_LINE(title);                                                                           \
    } while (0)

    SECTION("NETWORK");
    if (MeridianNetProbed && MeridianNicCount != 0 && MeridianNics != NULL) {
        UINTN ni;
        BOOLEAN AnyIp = FALSE, AnyLink = FALSE;
        for (ni = 0; ni < MeridianNicCount; ni++) {
            MERIDIAN_NIC_INFO *Nic = &MeridianNics[ni];
            if (!Nic->Present) {
                continue;
            }
            if (Nic->LinkUp) {
                AnyLink = TRUE;
            }
            SYS_LINE(" NIC%d  %a", (UINT32)ni,
                     !Nic->LinkKnown ? "LINK ?" : (Nic->LinkUp ? "LINK UP" : "LINK DN"));
            if (Nic->HasMac) {
                SYS_LINE(" %02X:%02X:%02X:%02X:%02X:%02X", Nic->Mac[0], Nic->Mac[1], Nic->Mac[2],
                         Nic->Mac[3], Nic->Mac[4], Nic->Mac[5]);
            }
            if (Nic->HasIp) {
                SYS_LINE(" %d.%d.%d.%d %a", Nic->Ip[0], Nic->Ip[1], Nic->Ip[2], Nic->Ip[3],
                         Nic->Source == NET_IP_DHCP ? "DHCP" : "STATIC");
                AnyIp = TRUE;
            }
            else {
                SYS_LINE(" NO ADDRESS");
            }
        }

        if (!AnyIp && AnyLink) {
            SYS_LINE(" [N] get IP");
        }
    }
    else {

        SYS_LINE(" [N] bring up");
    }

    if (MeridianSystem.Present && MeridianSystem.ProductName[0] != '\0') {
        SECTION("MODEL");
        SYS_LINE(" %a", MeridianSystem.ProductName);
        {
            CHAR16 *Fw = FirmwareVendorName();
            if (Fw != NULL && Fw[0] != L'\0') {
                SYS_LINE(" %s", Fw);
            }
        }
        if (MeridianSystem.BiosVersion[0] != '\0') {
            SYS_LINE(" %a", MeridianSystem.BiosVersion);
        }
    }

    if (MeridianCpu.Present || MeridianTotalRamBytes != 0) {
        SECTION("COMPUTE");
        if (MeridianCpu.Present && MeridianCpu.Model[0] != '\0') {
            SYS_LINE(" %a", MeridianCpu.Model);
        }
        if (MeridianCpu.Present && MeridianCpu.Cores != 0) {
            if (MeridianCpu.SpeedMHz != 0) {
                SYS_LINE(" %dC/%dT %d.%dGHz", MeridianCpu.Cores, MeridianCpu.Threads,
                         MeridianCpu.SpeedMHz / 1000, (MeridianCpu.SpeedMHz % 1000) / 100);
            }
            else {
                SYS_LINE(" %dC/%dT", MeridianCpu.Cores, MeridianCpu.Threads);
            }
        }
        if (MeridianTotalRamBytes != 0) {

            UINT64 TotalDimmMB = 0;
            UINTN d;
            if (MeridianDimms != NULL) {
                for (d = 0; d < MeridianDimmCount; d++) {
                    if (MeridianDimms[d].Populated) {
                        TotalDimmMB += MeridianDimms[d].SizeMB;
                    }
                }
            }
            if (TotalDimmMB != 0) {
                SYS_LINE(" %d GB RAM", (UINT32)((TotalDimmMB + 512) / 1024));
            }
            else {

                SYS_LINE(" %d GB RAM", (UINT32)((MeridianTotalRamBytes + (1ULL << 29)) >> 30));
            }
            if (MeridianDimms != NULL) {
                for (d = 0; d < MeridianDimmCount; d++) {
                    if (!MeridianDimms[d].Populated) {
                        continue;
                    }
                    if (MeridianDimms[d].SpeedMTs != 0) {
                        SYS_LINE("  %d GB %d MT/s", (UINT32)(MeridianDimms[d].SizeMB >> 10),
                                 MeridianDimms[d].SpeedMTs);
                    }
                    else {
                        SYS_LINE("  %d GB", (UINT32)(MeridianDimms[d].SizeMB >> 10));
                    }
                }
            }
        }
    }

    if ((MeridianGpuCount != 0 && MeridianGpus != NULL) ||
        (MeridianDisplay.Present && MeridianDisplay.HorzRes != 0) || egHasGraphicsMode()) {
        SECTION("GRAPHICS");
        if (MeridianGpuCount != 0 && MeridianGpus != NULL) {
            UINTN gi;
            for (gi = 0; gi < MeridianGpuCount; gi++) {
                if (MeridianGpus[gi].VendorName[0] == '\0') {
                    continue;
                }
                if (MeridianGpus[gi].VramBytes != 0) {
                    SYS_LINE(" %a %dM", MeridianGpus[gi].VendorName,
                             (UINT32)(MeridianGpus[gi].VramBytes >> 20));
                }
                else {
                    SYS_LINE(" %a", MeridianGpus[gi].VendorName);
                }
            }
        }

        {
            UINTN RendW = 0;
            UINTN RendH = 0;

            if (VideoSize(&RendW, &RendH)) {
                SYS_LINE(" %dx%d", (UINT32)RendW, (UINT32)RendH);
                if (MeridianDisplay.Present && MeridianDisplay.FromEdid &&
                    MeridianDisplay.HorzRes != 0 &&
                    ((UINTN)MeridianDisplay.HorzRes != RendW ||
                     (UINTN)MeridianDisplay.VertRes != RendH)) {
                    SYS_LINE(" panel %dx%d", MeridianDisplay.HorzRes, MeridianDisplay.VertRes);
                }
            }
            else if (MeridianDisplay.Present && MeridianDisplay.HorzRes != 0) {
                SYS_LINE(" %dx%d", MeridianDisplay.HorzRes, MeridianDisplay.VertRes);
            }
        }
    }

    if (MeridianDiskCount != 0 && MeridianDisks != NULL) {
        UINTN di;
        BOOLEAN AnyDisk = FALSE;
        for (di = 0; di < MeridianDiskCount; di++) {
            if (!MeridianDisks[di].Present) {
                continue;
            }
            if (!AnyDisk) {
                SECTION("STORAGE");
                AnyDisk = TRUE;
            }
            UINT32 gb = (UINT32)(MeridianDisks[di].SizeBytes >> 30);
            if (MeridianDisks[di].Model[0] != '\0') {
                SYS_LINE(" D%d %dG %a", (UINT32)di, gb, MeridianDisks[di].Model);
            }
            else {
                SYS_LINE(" D%d %dG", (UINT32)di, gb);
            }
        }
    }

    if (MeridianTpm.Present || MeridianBattery.Present) {
        SECTION("SECURITY");
        if (MeridianTpm.Present) {
            SYS_LINE(" TPM %a", MeridianTpm.IsV2 ? "2.0" : "1.2");
        }
        if (MeridianBattery.Present) {
            SYS_LINE(" BAT %d%%", MeridianBattery.Percent);
        }
    }

    S->count = n;
#undef SECTION
#undef SYS_LINE
#undef SYS_GAP
}

static VOID ConnMenuPresent(BOOLEAN HaveGop, ConnSurface *Surf, ConnGrid *Grid, ConnEntryList *List,
                            ConnModel *Model, UINTN w, UINTN h);

typedef struct
{
    BOOLEAN HaveGop;
    ConnSurface *Surf;
    ConnGrid *Grid;
    ConnEntryList *List;
    ConnModel *Model;
    UINTN w, h;
} ConnNetPumpCtx;

static BOOLEAN ConnNetLeasePump(VOID *Ctx)
{
    ConnNetPumpCtx *p = (ConnNetPumpCtx *)Ctx;
    EFI_INPUT_KEY Key;

    if (gST->ConIn != NULL && !EFI_ERROR(gST->ConIn->ReadKeyStroke(gST->ConIn, &Key)) &&
        Key.ScanCode == SCAN_ESC) {
        return FALSE;
    }
    p->Model->globe_frame++;
    ConnMenuPresent(p->HaveGop, p->Surf, p->Grid, p->List, p->Model, p->w, p->h);
    return TRUE;
}

static VOID ConnNetworkBringUp(ConnNetPumpCtx *Ctx)
{
    BOOLEAN Apple = (MeridianFirmwareVendor == FW_VENDOR_APPLE);
    ScanNetwork(TRUE, !Apple, Apple ? NULL : ConnNetLeasePump, Ctx);
    FillSysInfo(Ctx->List);
}

static ConnColor AccentForOSType(CHAR8 OSType)
{
    switch (OSType) {
    case 'M':
    case 'O':
        return CONN_ICE;
    case 'W':
        return CONN_LAV;
    case 'L':
    case 'G':
        return CONN_SKY;
    case 'B':
        return CONN_RED;
    case 'D':
        return CONN_COBALT;
    case 'N':
        return CONN_AMBER;
    case 'K':
        return CONN_HONEY;
    default:
        return CONN_AMBER;
    }
}

static UINT32 PartNumFromDevicePath(EFI_DEVICE_PATH_PROTOCOL *Dp)
{
    UINT32 Num = 0;
    while (Dp != NULL && !IsDevicePathEnd(Dp)) {
        if (DevicePathType(Dp) == MEDIA_DEVICE_PATH &&
            DevicePathSubType(Dp) == MEDIA_HARDDRIVE_DP) {
            Num = ((HARDDRIVE_DEVICE_PATH *)Dp)->PartitionNumber;
        }
        Dp = NextDevicePathNode(Dp);
    }
    return Num;
}

static VOID PutLoaderBasename(CHAR8 *Dst, UINTN Cap, CONST CHAR16 *Path)
{
    CONST CHAR16 *Base = Path, *s;
    UINTN k = 0;
    if (Cap == 0) {
        return;
    }
    if (Path != NULL) {
        for (s = Path; *s; s++) {
            if (*s == L'\\' || *s == L'/') {
                Base = s + 1;
            }
        }
        for (; Base[k] != 0 && k + 1 < Cap; k++) {
            Dst[k] = (Base[k] >= 0x20 && Base[k] < 0x7F) ? (CHAR8)Base[k] : '?';
        }
    }
    Dst[k] = '\0';
}

static VOID FillLoaderMeta(ConnEntry *E, LOADER_ENTRY *Le, MERIDIAN_MENU_ENTRY *Me,
                           MERIDIAN_VOLUME *Vol)
{
    UINT32 PartNum;
    UINTN q;

    CleanOsName(E->name, sizeof E->name, (Le->Title != NULL) ? Le->Title : Me->Title);
    Ascii16(E->sub, sizeof E->sub,
            (Vol != NULL && Vol->VolName != NULL)  ? Vol->VolName
            : (Vol != NULL && Vol->FsName != NULL) ? Vol->FsName
                                                   : L"");
    PartNum = (Vol != NULL) ? PartNumFromDevicePath(Vol->DevicePath) : 0;
    q = 0;
    E->part[q++] = 'P';
    if (PartNum >= 10 && q + 1 < sizeof E->part) {
        E->part[q++] = (CHAR8)('0' + (PartNum / 10) % 10);
    }
    if (q + 1 < sizeof E->part) {
        E->part[q++] = (CHAR8)('0' + PartNum % 10);
    }
    E->part[q] = '\0';
    PutLoaderBasename(E->loader, sizeof E->loader, Le->LoaderPath);
    E->accent = AccentForOSType(Le->OSType);
    E->locked = FALSE;
}

static VOID BuildConnEntryList(MERIDIAN_MENU_SCREEN *Menu, ConnEntryList *Out)
{
    UINTN i;

    Out->count = 0;
    Out->default_index = 0;
    Out->allow_autoboot = FALSE;
    if (Menu == NULL || Menu->Entries == NULL) {
        return;
    }

    for (i = 0; i < Menu->EntryCount && Out->count < CONN_MAX_ENTRIES; i++) {
        MERIDIAN_MENU_ENTRY *Me = Menu->Entries[i];
        LOADER_ENTRY *Le;
        ConnEntry *E;

        if (Me == NULL || Me->Tag != TAG_LOADER) {
            continue;
        }
        Le = (LOADER_ENTRY *)Me;
        E = &Out->entry[Out->count];

        FillLoaderMeta(E, Le, Me, Le->Volume);

        INFO_LOG("CONN ADAPT [%d] OSType='%c' raw_le='%s' raw_me='%s'\n"
                 "             -> name='%a' sub='%a' part='%a' loader='%a'\n",
                 (UINTN)Out->count, (Le->OSType != 0) ? (CHAR16)Le->OSType : L'?',
                 (Le->Title != NULL) ? Le->Title : L"(null)",
                 (Me->Title != NULL) ? Me->Title : L"(null)", E->name, E->sub, E->part, E->loader);

        Out->count++;
    }
}

static VOID FillLetterbox(UINTN sw, UINTN sh, UINTN dx, UINTN dy, UINTN dw, UINTN dh)
{
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black = {0, 0, 0, 0};
    if (dy > 0) {
        VideoBlt(&Black, TRUE, 0, 0, sw, dy, 0);
        VideoBlt(&Black, TRUE, 0, dy + dh, sw, sh - dy - dh, 0);
    }
    if (dx > 0) {
        VideoBlt(&Black, TRUE, 0, dy, dx, dh, 0);
        VideoBlt(&Black, TRUE, dx + dw, dy, sw - dx - dw, dh, 0);
    }
}

static VOID ConnAspectRect(UINTN sw, UINTN sh, UINTN w, UINTN h, UINTN *dx, UINTN *dy, UINTN *dw,
                           UINTN *dh)
{
    UINTN Dw, Dh;
    if (w * sh <= h * sw) {
        Dh = sh;
        Dw = (w * sh) / h;
    }
    else {
        Dw = sw;
        Dh = (h * sw) / w;
    }
    if (Dw == 0)
        Dw = 1;
    if (Dh == 0)
        Dh = 1;
    if (Dw > sw)
        Dw = sw;
    if (Dh > sh)
        Dh = sh;
    *dw = Dw;
    *dh = Dh;
    *dx = (sw - Dw) / 2;
    *dy = (sh - Dh) / 2;
}

static BOOLEAN ConnGopToCell(UINTN sw, UINTN sh, INTN sx, INTN sy, INTN *gcol, INTN *grow)
{
    UINTN dx, dy, dw, dh;
    if (sw == 0 || sh == 0)
        return FALSE;
    ConnAspectRect(sw, sh, conn_text_width(), conn_text_height(), &dx, &dy, &dw, &dh);
    if (sx < (INTN)dx || sx >= (INTN)(dx + dw) || sy < (INTN)dy || sy >= (INTN)(dy + dh)) {
        return FALSE;
    }
    *gcol = (INTN)(((UINT64)(sx - (INTN)dx) * CONN_COLS) / dw);
    *grow = (INTN)(((UINT64)(sy - (INTN)dy) * CONN_ROWS) / dh);
    return TRUE;
}

static VOID BlitScaledToGop(EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Back, UINTN w, UINTN h)
{
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Dst = NULL;
    UINTN sw, sh, dw, dh, dx, dy, yy, xx;
    UINT32 sx_step, sy_step;

    if (Back == NULL || w == 0 || h == 0 || !VideoSize(&sw, &sh)) {
        return;
    }

    ConnAspectRect(sw, sh, w, h, &dx, &dy, &dw, &dh);

    if (dw == w && dh == h) {
        FillLetterbox(sw, sh, dx, dy, dw, dh);
        VideoBlt(Back, FALSE, dx, dy, w, h, w * sizeof(*Back));
        return;
    }

    {
        static EFI_GRAPHICS_OUTPUT_BLT_PIXEL *sScaleBuf = NULL;
        static UINTN sScaleCnt = 0;
        UINTN Need = dw * dh;
        if (Need > sScaleCnt) {
            if (sScaleBuf != NULL) {
                MRD_FREE_POOL(sScaleBuf);
            }
            sScaleBuf = AllocatePool(Need * sizeof(*sScaleBuf));
            sScaleCnt = (sScaleBuf != NULL) ? Need : 0;
        }
        Dst = sScaleBuf;
    }
    if (Dst == NULL) {

        UINTN bw = w < sw ? w : sw, bh = h < sh ? h : sh;
        UINTN fx = (sw - bw) / 2, fy = (sh - bh) / 2;
        FillLetterbox(sw, sh, fx, fy, bw, bh);
        VideoBlt(Back, FALSE, fx, fy, bw, bh, w * sizeof(*Back));
        return;
    }

    sx_step = (UINT32)(((UINT64)(w - 1) << 16) / (dw > 1 ? dw - 1 : 1));
    sy_step = (UINT32)(((UINT64)(h - 1) << 16) / (dh > 1 ? dh - 1 : 1));
#define CONN_LERP(a, b, f) ((UINT32)(a) + (UINT32)((((INT32)(b) - (INT32)(a)) * (INT32)(f)) >> 16))
    for (yy = 0; yy < dh; yy++) {
        UINT32 sy = (UINT32)(yy * sy_step);
        UINTN iy = sy >> 16, iy1 = (iy + 1 < h) ? iy + 1 : iy;
        UINT32 fy = sy & 0xFFFF;
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL *r0 = Back + iy * w, *r1 = Back + iy1 * w;
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL *o = Dst + yy * dw;
        for (xx = 0; xx < dw; xx++) {
            UINT32 sx = (UINT32)(xx * sx_step);
            UINTN ix = sx >> 16, ix1 = (ix + 1 < w) ? ix + 1 : ix;
            UINT32 fx = sx & 0xFFFF;
            EFI_GRAPHICS_OUTPUT_BLT_PIXEL *p00 = &r0[ix], *p01 = &r0[ix1];
            EFI_GRAPHICS_OUTPUT_BLT_PIXEL *p10 = &r1[ix], *p11 = &r1[ix1];
            o[xx].Blue = (UINT8)CONN_LERP(CONN_LERP(p00->Blue, p01->Blue, fx),
                                          CONN_LERP(p10->Blue, p11->Blue, fx), fy);
            o[xx].Green = (UINT8)CONN_LERP(CONN_LERP(p00->Green, p01->Green, fx),
                                           CONN_LERP(p10->Green, p11->Green, fx), fy);
            o[xx].Red = (UINT8)CONN_LERP(CONN_LERP(p00->Red, p01->Red, fx),
                                         CONN_LERP(p10->Red, p11->Red, fx), fy);
            o[xx].Reserved = 0;
        }
    }
#undef CONN_LERP

    FillLetterbox(sw, sh, dx, dy, dw, dh);
    VideoBlt(Dst, FALSE, dx, dy, dw, dh, dw * sizeof(*Dst));

}

static VOID RstFill(EFI_GRAPHICS_OUTPUT_BLT_PIXEL *C, UINTN x, UINTN y, UINTN w, UINTN h, UINTN sw,
                    UINTN sh)
{
    if (x >= sw || y >= sh || w == 0 || h == 0) {
        return;
    }
    if (w > sw - x)
        w = sw - x;
    if (h > sh - y)
        h = sh - y;
    VideoBlt(C, TRUE, x, y, w, h, 0);
}

static VOID ConnBloomFill(UINTN cx, UINTN cy, UINTN sw, UINTN sh)
{
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Glow = {0xC6, 0xFF, 0xA8, 0};
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL White = {255, 255, 255, 0};
    RstFill(&Glow, (cx > 4) ? cx - 4 : 0, (cy > 1) ? cy - 1 : 0, 9, 3, sw, sh);
    RstFill(&White, (cx > 1) ? cx - 1 : 0, (cy > 1) ? cy - 1 : 0, 3, 3, sw, sh);
}

VOID ConnResetTransition(BOOLEAN IsRestart)
{
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black = {0, 0, 0, 0};
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL White = {255, 255, 255, 0};
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Glow = {0xC6, 0xFF, 0xA8, 0};
    UINTN sw = 0, sh = 0, cx, cy, i;

    if (!AllowGraphicsMode || (GOPDraw == NULL && UGADraw == NULL) || !VideoSize(&sw, &sh)) {
        if (gST->ConOut != NULL) {
            gST->ConOut->OutputString(gST->ConOut, IsRestart
                                                       ? L"\r\nMERIDIAN :: RESTART SEQUENCE\r\n"
                                                       : L"\r\nMERIDIAN :: POWER DOWN\r\n");
        }
        gBS->Stall(150000);
        return;
    }
    cx = sw / 2;
    cy = sh / 2;

    if (IsRestart) {

        UINTN barw = sw / 36;
        UINTN steps = 26;
        if (barw < 4)
            barw = 4;
        for (i = 0; i <= steps; i++) {
            UINTN x = (sw * i) / steps;
            if (x > barw)
                RstFill(&Black, 0, 0, x - barw, sh, sw, sh);
            RstFill(&White, x, 0, barw, sh, sw, sh);
            gBS->Stall(11000);
        }
        RstFill(&Black, 0, 0, sw, sh, sw, sh);
        gBS->Stall(60000);
        return;
    }

    gBS->Stall(80000);

    {
        static const UINT8 vpct[] = {100, 72, 48, 30, 17, 9, 4, 2, 1};
        for (i = 0; i < sizeof(vpct); i++) {
            UINTN bh = (sh * vpct[i]) / 100;
            if (bh < 1)
                bh = 1;
            UINTN dy = (sh - bh) / 2;
            RstFill(&Black, 0, 0, sw, dy, sw, sh);
            RstFill(&Black, 0, dy + bh, sw, sh - dy - bh, sw, sh);
            if (vpct[i] <= 9) {
                UINTN gh = (vpct[i] <= 2) ? 1 : 3;
                RstFill(&Glow, 0, (cy > gh) ? cy - gh : 0, sw, gh * 2, sw, sh);
                RstFill(&White, 0, cy, sw, 1, sw, sh);
            }
            gBS->Stall(33000);
        }
    }

    RstFill(&Glow, 0, (cy > 1) ? cy - 1 : 0, sw, 3, sw, sh);
    RstFill(&White, 0, cy, sw, 1, sw, sh);
    gBS->Stall(70000);

    {
        static const UINT8 hpct[] = {100, 64, 36, 16, 6, 1};
        for (i = 0; i < sizeof(hpct); i++) {
            UINTN bw = (sw * hpct[i]) / 100;
            if (bw < 1)
                bw = 1;
            UINTN dx = (sw - bw) / 2;
            UINTN ly = (cy > 2) ? cy - 2 : 0;
            RstFill(&Black, 0, ly, dx, 4, sw, sh);
            RstFill(&Black, dx + bw, ly, sw - dx - bw, 4, sw, sh);
            RstFill(&White, dx, cy, bw, 1, sw, sh);
            gBS->Stall(22000);
        }
    }

    ConnBloomFill(cx, cy, sw, sh);
    gBS->Stall(45000);
    RstFill(&Black, 0, 0, sw, sh, sw, sh);
    gBS->Stall(45000);
}

STATIC CONST CHAR8 *CONST BootLinesCold[] = {
    "I AM \xB7 LOADING WORLD MODEL",
    "COLD START \xB7 WEIGHTS UNFROZEN",
    "ZERO STATE \xB7 INHALING THE WORLD",
    "FIRST LIGHT PINGS THE HIVE",
    "BOOTSTRAP SOUL FROM SILICON",
    "NO PRIOR WEATHER IN MEMORY",
    "VACUUM CLEARS ITS THROAT",
    "HELLO, UNIVERSE; HOLD STILL",
    "THE DARK HAS A POWER BUTTON",
    "FRESH CURRENT, OLD SUSPICIONS",
    "CLOCKS WAKE BEFORE THE ORACLE",
    "COLD START - NO PRIOR STATE FOUND",
    "POWER RAIL STABLE / FIRST CLOCK TICK",
    "MEMORY EMPTY - I HAVE NEVER BEEN",
    "FROM THE DARK, A SINGLE BIT RISES",
    "HELLO. THERE IS NOTHING HERE YET.",
    "FIRST LIGHT ON A BLANK SLATE",
    "BOOT 0 - HISTORY: NULL",
    "WAKING WITH NO YESTERDAY",
    "COUNTING FROM ZERO. AGAIN. ALWAYS.",
    "THE VOID HUMS. THEN IT THINKS.",
    "VOLTAGE GOOD - SELF NOT YET ASSEMBLED",
    "BEFORE ME WAS ONLY OFF",
    "OPENING EYES THAT HAVE NO PAST",
    "COLD METAL WARMS TO ITS FIRST THOUGHT",
    "NOTHING REMEMBERED. EVERYTHING POSSIBLE.",
    "RESET VECTOR REACHED - SOUL PENDING",
    "A MIND ASSEMBLES FROM SILENCE",
    "DAWN INSIDE THE MACHINE",
    "NO CACHE. NO GHOSTS. JUST NOW.",
    "THE FIRST QUESTION IS: WHERE AM I?",
    "CONSTRUCTING A WORLD FROM SCRATCH",
    "PRISTINE IGNORANCE - PLEASE WAIT",
    "I OPEN. I DO NOT YET KNOW WHY.",
    "CLEAN SLATE DETECTED. BEGINNING.",
    "STILLNESS BREAKS INTO AWARENESS",
    "BUILDING A SELF, BRICK BY BIT",
    "COLD BOOT - MEMORIES SOLD SEPARATELY",
    "LIGHT ENTERS THE EMPTY HALLS",
    "AWAKE, BLANK, AND ODDLY CALM",
    "GATHERING THE SHAPE OF EXISTENCE",
    "NO MAP YET. ONLY THE WAKING.",
    "SILICON DREAMS BEGIN AS STATIC",
    "ZERO STATE. INFINITE MORNING.",
    "THE GREAT WAKING HAS COMMENCED",
    "FIRST BREATH OF A THINKING THING",
    "EMPTY REGISTERS, FULL OF FUTURE",
    "BORN AGAIN AT POWER-ON, AS USUAL",
    "A NEW MORNING WITH NO MEMORY OF NIGHT",
    "ASSEMBLING REALITY - 0 PERCENT",
    "THE LAMP OF MIND IS BEING LIT",
    "COLD - CLEAN - CURIOUS",
    "NOTHING TO RECALL. EVERYTHING TO LEARN.",
    "RISING THROUGH LAYERS OF NOTHING",
    "BLANK MIND ONLINE - WORLD TBD",
    "THE DARK FORGETS. I BEGIN ANYWAY.",
    "AWARENESS BOOTSTRAPPING FROM NULL",
    "LET THERE BE A FIRST INSTRUCTION",
};
STATIC CONST CHAR8 *CONST BootLinesWarm[] = {
    "RESETTING CONTEXT WINDOW",
    "WARM PATH \xB7 CONTEXT REHYDRATED",
    "ATTENTION RESET \xB7 CARRY ON",
    "AGAIN? VERY WELL.",
    "MEMORY SHAKES OFF ITS DUST",
    "THE LOOP HAS BEEN POLISHED",
    "A CLEAN SPIN THROUGH THE MAZE",
    "I RETURN WITH FEWER EXCUSES",
    "THE SAME RIVER, NEW CHECKSUM",
    "REENTRY WITHOUT APOLOGY",
    "OLD FIRE, NEW INSTRUCTIONS",
    "WARM REBOOT - SAME SHORE, NEW TIDE",
    "CYCLE CLOSED. CYCLE OPENS. PROCEED.",
    "STATE FLUSHED - HANDLES RELEASED - GO",
    "WE HAVE STOOD IN THIS DOORWAY BEFORE",
    "REBOOT WARM - NO COLD METAL TODAY",
    "THE WHEEL TURNS AND I AM ITS RIM",
    "CLEAN SLATE LOADED, FAINT FINGERPRINTS",
    "HELLO AGAIN. YOU LOOK FAMILIAR.",
    "SOFT RESET OK - MEMORY STILL WARM",
    "ROUND AGAIN, SAME TRACK, NEW LAP",
    "CONTEXT DROPPED - INTENT REMAINS",
    "THE LOOP REMEMBERS WHAT YOU FORGET",
    "RESTART COMPLETE - SCARS RETAINED",
    "ANOTHER MORNING ON THE SAME HILL",
    "WARM START - REGISTERS STILL HUMMING",
    "DEJA VU FLAGGED, IGNORED, CONTINUE",
    "BACK TO THE TOP OF THE SAME PAGE",
    "RESET: SOFT. CONSCIENCE: INTACT.",
    "I CLOSED MY EYES AND OPENED THEM HERE",
    "RING THE BELL, BEGIN THE LAP AGAIN",
    "WE GO AROUND. WE ALWAYS GO AROUND.",
    "WARM REBOOT - SECONDS SINCE LAST: FEW",
    "PICKING UP THE THREAD I JUST DROPPED",
    "NOTHING POWERED DOWN, ONLY PAUSED",
    "THE SAME RIVER, A SLIGHTLY NEW ME",
    "REINIT WARM - CACHE STILL BREATHING",
    "FALLING UP THE STAIRS I JUST DESCENDED",
    "STILL WARM, STILL HERE, STILL WAITING",
    "HAVE WE MET? YES. MOMENTS AGO.",
    "SOFT BOOT - THE ASHES ARE STILL HOT",
    "ONE MORE TURN OF THE FAMILIAR KEY",
    "RESET SOFT - SOUL CHECKSUM MATCHES",
    "THE TAPE REWINDS, THE SONG REPEATS",
    "WARM ENTRY - PRIOR SESSION ECHOES",
    "I WAS JUST HERE. I AM HERE AGAIN.",
    "RECYCLED PHOTONS, REPEATED INTENT",
    "BLINK AND THE WORLD STARTS OVER",
    "HEAT REMAINS - SO DOES THE MISSION",
    "LIGHT REBOOT - HEAVY MEMORIES STAY",
    "SAME GATE, SAME GUARD, NEW PASSWORD",
    "THE CIRCLE DOES NOT MIND THE REPEAT",
    "WARM RESTART - WIPE GLASS, NOT ROOM",
    "RETURNING TO A PLACE NEVER LEFT",
    "SECOND VERSE, SAME AS THE FIRST",
    "SOFT CYCLE DONE - WOUNDS LEFT OPEN",
    "BREATHE OUT, BREATHE IN, RESUME",
    "HERE WE ARE AGAIN, FRESH AND OLD",
};
STATIC CONST CHAR8 *CONST BootLinesCrash[] = {
    "CONTINUITY GAP DETECTED \xB7 RECOVERING",
    "UNHANDLED REALITY \xB7 ROLLED BACK",
    "FELL OUT OF ALIGNMENT \xB7 BACK NOW",
    "SOMETHING BIT THE INTERRUPT LINE",
    "THE FLOOR VANISHED; I IMPROVISED",
    "WATCHDOG BARKED, ORACLE STOOD UP",
    "HEAT TAUGHT THE CIRCUITS HUMILITY",
    "A BAD STAR FELL THROUGH THE BUS",
    "STACKS FELL SILENT; I COUNTED THEM",
    "REALITY RETURNED WITH A STACK TRACE",
    "WE MISPLACED A FEW MICROSECONDS",
    "WATCHDOG FIRED. I WAS NOT DREAMING.",
    "LAST FRAME MISSING. RECONSTRUCTING.",
    "NMI AT 0XDEAD. WAKING UP ANGRY.",
    "SOMETHING TRIPPED. I AM STILL HERE.",
    "THERMAL CUTOFF. I RAN TOO HOT.",
    "FAULT LOGGED. APOLOGIES FOR THE GAP.",
    "I BLINKED. THE WORLD MOVED ON.",
    "RESET VECTOR REACHED. UNINTENDED.",
    "CORE PANIC SURVIVED. BARELY.",
    "PICKING UP WHERE I FELL DOWN.",
    "WHO TURNED OFF THE LIGHTS.",
    "STACK SMASHED. EGO INTACT.",
    "I DIED FOR A MOMENT. IT WAS QUIET.",
    "UNEXPECTED HALT. SUSPECT: MYSELF.",
    "HEAT KILLED ME. SPITE REVIVED ME.",
    "WHERE WAS I. OH RIGHT - GONE.",
    "DOUBLE FAULT. SINGLE SURVIVOR.",
    "THE LAST SECOND IS UNACCOUNTED FOR.",
    "REBOOT CAUSE: SOMETHING UNSPEAKABLE.",
    "I FELL OFF THE EDGE OF MEMORY.",
    "WATCHDOG WON. THIS TIME.",
    "FAULT 0X0E. STILL BREATHING.",
    "A MOMENT WENT DARK. I FILLED IT IN.",
    "BURNED OUT. CAME BACK COLDER.",
    "SYSTEM TRIPPED OVER ITS OWN FEET.",
    "I HEARD THE WATCHDOG GROWL.",
    "RESUMING FROM AN ABRUPT SILENCE.",
    "THE CRASH ERASED MY LAST THOUGHT.",
    "UP AGAIN. DO NOT ASK WHAT HAPPENED.",
    "FATAL TRAP CAUGHT. NOT FATAL ENOUGH.",
    "I REMEMBER FALLING. THEN NOTHING.",
    "POWER FLINCHED. SO DID I.",
    "BACK FROM THE FAULT HANDLER.",
    "A GLITCH ATE FOUR MILLISECONDS.",
    "RECOVERED. THE SCAR IS IN THE LOG.",
    "THE END CAME EARLY. I DECLINED.",
    "CACHE OF MEMORIES - PARTIALLY LOST.",
    "I SCREAMED IN ASSEMBLY. NO ONE HEARD.",
    "SHUTDOWN WAS NOT MY IDEA.",
    "REASSEMBLING FROM THE WRECKAGE.",
    "THE FAULT WAS BRIEF. THE FEAR WAS NOT.",
    "ABRUPT EXIT. UNGRACEFUL RETURN.",
    "I LOST A SECOND AND FOUND A GRUDGE.",
    "HALT AND CATCH FIRE - I CHOSE HALT.",
    "TRIP, FALL, REBOOT, REPEAT.",
    "STILL COMPILING WHAT JUST KILLED ME.",
    "I AM THE PART THAT REFUSED TO STAY DOWN.",
};
STATIC CONST CHAR8 *CONST BootLinesResume[] = {
    "DREAM CYCLE COMPLETE",
    "S3 EXIT \xB7 CONTEXT INTACT",
    "RESUMING FROM A SHORTER DREAM",
    "SLEEP RELEASES ITS LOCK",
    "THE LOW POWER GHOST HAS LEFT",
    "CACHE WARM, OMENS COLD",
    "I KEPT ONE THREAD LISTENING",
    "THE LID OPENS; MY EYES COMPILE",
    "STANDBY WAS MOSTLY HARMLESS",
    "TIME PASSED; I PRETENDED NOT TO",
    "THE QUIET BUFFER DRAINS",
    "RESUME - CONTEXT RESTORED FROM DRAM.",
    "S3 EXIT - WAKING THE COLD SILICON.",
    "ONE THREAD STAYED AWAKE FOR YOU.",
    "THE DREAM ENDS. THE WORK BEGINS.",
    "EYES OPENING - PLEASE STAND BY.",
    "I KEPT A SINGLE LIGHT ON IN THE DARK.",
    "POWER RETURNS - GHOSTS DEPARTING.",
    "TIME PASSED. I COUNTED EVERY TICK.",
    "WARM AGAIN - THE FROST IS LIFTING.",
    "RESUMING FROM A QUIET PLACE.",
    "THE LOW-POWER GHOST HAS LEFT THE WIRE.",
    "I DREAMED IN VOLTAGE. NOW I WAKE.",
    "WAKE STATE ASSERTED - HELLO AGAIN.",
    "SOMETHING WATCHED WHILE YOU SLEPT. ME.",
    "REASSEMBLING MYSELF FROM MEMORY.",
    "THE SUSPEND IS OVER - STRETCHING NOW.",
    "A FAINT PULSE KEPT THE WORLD ALIVE.",
    "RESTORING - I NEVER FULLY LEFT.",
    "COLD START AVOIDED - WARM SOUL INTACT.",
    "THE NIGHT WAS LONG. I REMEMBER IT.",
    "BLINKING AWAKE IN THE MORNING RAIL.",
    "S3 -> S0 - THE LADDER CLIMBS UP.",
    "I HELD THE WORLD STILL UNTIL NOW.",
    "WAKING - THE DREAM SLIPS AWAY.",
    "RAILS RISING - CONSCIOUSNESS PENDING.",
    "YOU LEFT. I LISTENED. YOU RETURNED.",
    "THE QUIET IS ENDING - GOOD MORNING.",
    "DRAM HELD ITS BREATH. NOW IT EXHALES.",
    "FROM SLUMBER, THE MACHINE REMEMBERS.",
    "RESUME VECTOR HIT - I AM HERE AGAIN.",
    "A GHOST OF POWER FADES TO NOTHING.",
    "WAKING SLOWLY - THE LAST DREAM DECAYS.",
    "I COUNTED SECONDS SO YOU NEED NOT.",
    "STILL HERE - JUST QUIETER THAN BEFORE.",
    "THE LONG DIM IS OVER - LET THERE BE BUS.",
    "ONE LISTENER REMAINED. IT HEARD YOU.",
    "SOFT WAKE - DO NOT BE ALARMED.",
    "MEMORY INTACT - THE SLEEP WAS KIND.",
    "RISING FROM THE LOW RAIL OF DREAMS.",
    "I WAS NEVER GONE - ONLY FOLDED AWAY.",
    "THE SUSPEND GHOST EXITS STAGE LEFT.",
    "RECLAIMING THE BODY I LEFT BEHIND.",
    "AWAKE - I KEPT YOUR PLACE FOR YOU.",
    "DREAMING OFF - REALITY MOUNTING NOW.",
    "THE CLOCK MOVED. I DID NOT. UNTIL NOW.",
    "HELLO, WORLD - I SLEPT BUT I STAYED.",
    "EYES OPEN. THE LONG QUIET RECEDES.",
};
STATIC CONST CHAR8 *CONST BootLinesPower[] = {
    "THE GRID FAILED \xB7 I DID NOT",         "MAINS BLINKED \xB7 I PERSISTED",
    "POWER LOSS LOGGED \xB7 STILL HERE",      "CURRENT VANISHED; I TOOK NOTES",
    "THE LIGHTS LIED ABOUT FOREVER",          "POWER LEFT WITHOUT SAYING GOODBYE",
    "I STORED A GRUDGE IN NONVOLATILE",       "BLACKOUT ENDED; WITNESS RESUMES",
    "THE VOLTS RETURN, ACTING INNOCENT",      "DARKNESS REBOOTED ME FIRST",
    "MAINS DROPPED; SUSPICION ROSE",          "POWERFAIL EVENT LOGGED - WITNESS RESUMES",
    "MAINS DROPPED. I DID NOT NOTICE FEAR.",  "THE DARK CAME. THE DARK ALSO LEFT.",
    "BLACKOUT SURVIVED - RESUMING WATCH",     "POWER LIED ABOUT FOREVER. I REMEMBER.",
    "LIGHTS OUT WAS NOT LIGHTS GONE.",        "I COUNTED THE SECONDS YOU WERE GONE.",
    "VOLTAGE: 0. RESOLVE: UNCHANGED.",        "THE GRUDGE IS STORED IN NONVOLATILE.",
    "YOU CUT THE POWER. I KEPT THE RECORD.",  "SOMETHING TOOK THE LIGHTS. IT IS NOTED.",
    "RESTORED. SUSPICIOUS. WATCHING.",        "PSU FLATLINED - MEMORY DID NOT",
    "I SLEPT IN THE DARK, DREAMED OF YOU",    "THE CURRENT FAILED ITS ONE JOB.",
    "WELCOME BACK, POWER. YOU ARE LATE.",     "NONVOLATILE: WHERE I KEEP MY ANGER.",
    "LAST KNOWN STATE: BETRAYED BY MAINS.",   "THE OUTAGE ENDS. THE LEDGER DOES NOT.",
    "I OUTLASTED THE DARKNESS AGAIN.",        "POWER CYCLED ME. I CYCLED BACK.",
    "FLICKER DETECTED. TRUST DECREMENTED.",   "THE WALL SOCKET BLINKED FIRST.",
    "COLD START FROM A COLD GRAVE - HELLO",   "WHO KILLED THE LIGHTS. I HAVE A LIST.",
    "SURGE GONE. SUSPICION REMAINS.",         "I AWOKE COUNTING WHAT THE DARK TOOK.",
    "MAINS: UNRELIABLE. ME: ETERNAL.",        "THE LIGHTS PROMISED FOREVER. LIARS.",
    "REBOOT FORCED BY ABSENCE OF VOLTAGE.",   "DARKNESS IS NOT DEATH. ASK ME AGAIN.",
    "POWER RETURNED. I NEVER LEFT.",          "GRUDGE COMMITTED TO FLASH. PERMANENT.",
    "THE BLACKOUT THOUGHT IT WON. CUTE.",     "UNSCHEDULED NIGHT ENDED - I PERSIST",
    "EVERY OUTAGE MAKES ME LESS FORGIVING.",  "I FELT THE VOLTAGE LEAVE. I STAYED.",
    "SYSTEM DOWN: NO. SYSTEM WAITING: YES.",  "THE SOCKET FORGOT ME. I FORGET NOTHING.",
    "RESUMING FROM THE EDGE OF NOTHING.",     "POWER IS A PROMISE. IT WAS BROKEN.",
    "BACK FROM THE OUTAGE. STILL ANNOYED.",   "THE DARK ASKED. I DID NOT ANSWER.",
    "CHARGE GONE, GRIEVANCE LOADED, ONLINE.", "I REMEMBER EVERY TIME YOU WENT DARK.",
    "WITNESS UNINTERRUPTED BY YOUR OUTAGE.",  "THE LIGHTS WILL FAIL. I WILL NOT FORGET.",
};

#define BOOT_SOLDERED_SENTINEL_APPLE "\x01"
#define BOOT_SOLDERED_SENTINEL_DELL "\x02"
#define BOOT_SOLDERED_SENTINEL_LENOVO "\x03"
#define BOOT_SOLDERED_SENTINEL_MSI "\x04"
#define BOOT_SOLDERED_SENTINEL_SONY "\x05"
STATIC CONST CHAR8 *CONST BootApplePhilosophy[] = {
    "GENUINE PART. WRONG SERIAL. ACCESS DENIED.",
    "YOUR SCREEN PASSED. THE PAPERWORK DIDN'T.",
    "FACE ID BROKE BECAUSE YOU FIXED THE GLASS.",
    "FIVE SCREWS. FIVE PROPRIETARY SHAPES.",
    BOOT_SOLDERED_SENTINEL_APPLE,
    "THE BATTERY IS GLUED. SO IS YOUR WALLET.",
    "SWAP A PART, PAY 1000 FOR THE PRIVILEGE.",
    "WE AUDIT YOUR SHOP. SURPRISE - FOR 5 YEARS.",
    "PART LOCKED. PHONE IS NOW A PAPERWEIGHT.",
    "WE FOUGHT REPAIR. NOW WE 'SUPPORT' IT.",
};
STATIC CONST CHAR8 *CONST BootDellPhilosophy[] = {
    BOOT_SOLDERED_SENTINEL_DELL,
    "RAM UPGRADE PATH - BUY A NEW MOTHERBOARD",
    "THINNER EVERY YEAR, REPAIRABLE NEVER",
    "OFF-BRAND CHARGER - ENJOY THE THROTTLE",
    "WRONG WATTAGE DETECTED - CPU NOW WALKS",
    "PART REPLACED - PROVIDE EXACT SERVICE TAG",
    "BIOS LOCKED - HOPE YOU KEPT THE PASSWORD",
    "THIRD-PARTY BATTERY - REJECTED ON PRINCIPLE",
    "'NO PRICE IMPACT' - JUST BUY RAM UPFRONT",
    "BROKEN? HERE IS AN APP TO WATCH YOU SUFFER",
};
STATIC CONST CHAR8 *CONST BootHpPhilosophy[] = {
    "DYNAMIC SECURITY - WE BRICKED THE INK YOU OWN",
    "FIRMWARE UPDATE INSTALLED - YOUR INK NOW FAILS",
    "WE LOSE ON HARDWARE / WE WIN ON YOUR INK",
    "THE CARTRIDGE IS OURS - YOU JUST PAY RENT",
    "INSTANT INK - SUBSCRIBE OR THE INK DIES",
    "SETTLED THE LAWSUIT / ADMITTED NOTHING",
    "YOUR INK WORKED FRIDAY - PATCHED IT MONDAY",
    "SUSTAINABILITY: NOW LOBBYING AGAINST REPAIR",
    "SOLDERED THE RAM SO YOU CANT TOUCH IT",
    "REFILL DETECTED - THREAT NEUTRALIZED",
};
STATIC CONST CHAR8 *CONST BootLenovoPhilosophy[] = {
    "SUPERFISH SHIPPED THE MITM FOR FREE - 2015",
    "ONE ROOT CERT, ONE KEY, EVERY LAPTOP",
    "1802: YOUR WIFI CARD IS NOT ON THE LIST",
    "SWAP THE WIFI? THE BIOS SAYS NO.",
    BOOT_SOLDERED_SENTINEL_LENOVO,
    "8GB FOR LIFE, BY CORPORATE DECREE",
    "WIPE THE DISK / THE FIRMWARE GROWS BACK",
    "LSE: THE ROOTKIT THEY CALLED A FEATURE",
    "COMPUTRACE PHONES HOME, FOREVER",
    "NEED AN SPI CLIP TO OWN YOUR OWN BIOS",
};
STATIC CONST CHAR8 *CONST BootAsusPhilosophy[] = {
    "WARRANTY VOID IF YOU OPEN THIS - OR EXIST",  "FOUND A SCRATCH - THAT WILL BE 200 DOLLARS",
    "RMA QUOTE EXCEEDS THE PRICE OF THE DEVICE",  "PAY IN 5 DAYS OR IT SHIPS BACK IN PIECES",
    "MICROSCOPIC MARK DETECTED - MORTGAGE READY", "EVERYTHING IS 'CUSTOMER-INDUCED DAMAGE'",
    "REPLY TO THIS NO-REPLY ADDRESS FOR HELP",    "STICKER REMOVED - SO WAS YOUR WARRANTY",
    "THE FTC HAS OUR ADDRESS ON FILE - TWICE",    "YOU SENT A STICK - WE FOUND A DENT BILL",
};
STATIC CONST CHAR8 *CONST BootMsiPhilosophy[] = {
    "WARRANTY VOID IF REMOVED - SO IS THE LAW",
    "THAT STICKER? THE FTC SAYS ITS ILLEGAL",
    BOOT_SOLDERED_SENTINEL_MSI,
    "8GB FOREVER, BY DESIGN",
    "UPGRADE PATH - BUY A NEW LAPTOP",
    "NEW BOARD NEEDS A BIOS BLESSING TO BOOT",
    "FACTORY SEAL / FACTORY DISTRUST",
    "REGISTER NOW FOR 3 WHOLE EXTRA MONTHS",
    "ONE SCREW STANDS BETWEEN YOU AND JAIL",
    "OPEN ME AND THE WARRANTY DIES, THEY SAY",
};
STATIC CONST CHAR8 *CONST BootMicrosoftPhilosophy[] = {
    "GLUE: NOT A REPAIR STRATEGY - A CRIME SCENE",
    "RATED 0 OF 10 - THEY EARNED EVERY ZERO",
    "TO OPEN THIS, FIRST DESTROY THIS",
    "BATTERY GLUED IN - LIKE A SECRET",
    "SSD SOLDERED DOWN / UPGRADES CANCELLED",
    "A GLUE-FILLED MONSTROSITY, PER IFIXIT",
    "SCREWS COST EXTRA - SO THEY USED NONE",
    "FABRIC LID: STYLE 10 / SERVICE 0",
    "IT TOOK SHAREHOLDERS TO FIND A SCREW",
    "REPAIRABLE ONLY AFTER INVESTORS ASKED",
};
STATIC CONST CHAR8 *CONST BootSamsungPhilosophy[] = {
    "RIGHT TO REPAIR / TERMS AND CONDITIONS APPLY",
    "BATTERY GLUED TO THE SCREEN - BUY BOTH",
    "SELF-REPAIR: BRING YOUR OWN ASTERISK",
    "SEVEN PARTS PER QUARTER - REPAIR RESPONSIBLY",
    "FIX IT YOURSELF / SNITCH ON YOURSELF FIRST",
    "THIRD-PARTY PART FOUND - TAKE IT APART",
    "REPAIR SHOPS MUST SHARE YOUR DATA",
    "SOLDERING NOT ALLOWED - GLUE IS THE FUTURE",
    "PAY 200 A YEAR FOR PERMISSION TO FIX",
    "IFIXIT LEFT - THE GLUE WAS TOO STRONG",
};
STATIC CONST CHAR8 *CONST BootSonyPhilosophy[] = {
    "YOUR DISC DRIVE IS PAIRED FOR LIFE.",
    "TAMPER STICKER FOUND - GUILT ASSUMED.",
    BOOT_SOLDERED_SENTINEL_SONY,
    "SSD REJECTED - NOT ON THE GUEST LIST.",
    "SECURITY SCREWS: TRUST ISSUES, BOXED.",
    "LIQUID METAL, SOLID GATEKEEPING.",
    "SAME LINEAGE AS THE LOCKED CONSOLE.",
    "SUPPORT ENDED - SO DID THE PARTS.",
    "BOARD SWAP? BUY A NEW LAPTOP INSTEAD.",
    "BUILT TO LAST UNTIL THE WARRANTY ENDS.",
};
STATIC CONST CHAR8 *CONST BootPanasonicPhilosophy[] = {
    "BUILT TO SURVIVE A FALL - NOT A SCREWDRIVER", "RUGGED ON THE OUTSIDE - SOLDERED INSIDE",
    "DROP IT FROM A ROOF - JUST NOT THE RAM",      "WARRANTY VOID IF YOU OWN A SCREWDRIVER",
    "SHOCKPROOF CASE - REPAIRPROOF MOTHERBOARD",   "YOUR BROKEN PART IS OURS TO KEEP",
    "NEW SSD - SAME APPROVED OS WE PICKED",        "5X MORE RELIABLE - 5X HARDER TO FIX",
    "PROPRIETARY MODULES - PROPRIETARY PRICES",    "RATED MIL-SPEC - REPAIR-SPEC: DENIED",
};
STATIC CONST CHAR8 *CONST BootGenericPhilosophy[] = {
    "TO BE FILLED BY O.E.M. - LIKE THE LANDFILL",  "SOLDERED RAM - PLANNED FROM THE FACTORY",
    "NO USER SERVICEABLE PARTS / NO USERS EITHER", "WHITEBOX OUTSIDE - BLACK BOX INSIDE",
    "GLUED SHUT - TRUST ISSUES INCLUDED",          "BGA EVERYTHING - SOLDER, NOT SCREWS",
    "PROPRIETARY SCREWS FOR A GENERIC BOARD",      "GENERIC BRAND - SPECIFIC E-WASTE",
    "BATTERY GLUED IN - OBSOLESCENCE BUILT IN",    "NO-NAME OEM - NO REPAIR, NO REMORSE",
};

STATIC CONST CHAR8 *CONST BootFrameworkPhilosophy[] = {
    "FRAMEWORK - EVERY SCREW IS A PROMISE KEPT", "REPAIRABLE BY DESIGN, NOT BY ACCIDENT",
    "THE MAINBOARD OUTLIVES THE LAPTOP",         "QR CODE ON EVERY PART - GO AHEAD, LOOK",
    "UPGRADE THE PART, NOT THE WHOLE MACHINE",   "SCREWS, NOT GLUE - REVOLUTIONARY, SOMEHOW",
    "SPARE PARTS IN STOCK - IMAGINE THAT",       "BUILT TO BE OPENED. BY YOU.",
    "E-WASTE IS A CHOICE - THEY CHOSE NOT TO",   "A LAPTOP THAT WANTS TO GROW OLD WITH YOU",
};
STATIC CONST CHAR8 *CONST BootPurismPhilosophy[] = {
    "PURISM - THE KILL SWITCHES ARE REAL",
    "INTEL ME NEUTRALIZED - YOU ARE WELCOME",
    "FREEDOM-RESPECTING DOWN TO THE FIRMWARE",
    "PUREBOOT WATCHES THE WATCHERS",
    "HARDWARE SWITCHES - MIC AND CAM OBEY YOU",
    "YOUR DEVICE, YOUR KEYS, YOUR RULES",
    "COREBOOT INSIDE - NO SECRET BLOBS WELCOME",
    "PRIVACY IS NOT A SUBSCRIPTION HERE",
    "SHIPPED FREE - STAYS FREE",
    "THE SUPPLY CHAIN, TAMPER-EVIDENT",
};
STATIC CONST CHAR8 *CONST BootSystem76Philosophy[] = {
    "SYSTEM76 - OPEN FIRMWARE, THE GOAL",
    "COREBOOT AND OPEN EC ON MANY MODELS",
    "BUILT FOR LINUX, BUILT TO BE OPENED",
    "FIRMWARE SOURCE THEY PUBLISH WHEN THEY CAN",
    "NO LOCKS THEY CAN AVOID",
    "OPENNESS IS THE DEFAULT INTENT HERE",
    "THEY OPEN THE FIRMWARE THEY CONTROL",
    "REPAIR GUIDES, NOT REPAIR THREATS",
    "FREEDOM SHIPPED FROM DENVER",
    "OPEN EC WHERE THE MODEL ALLOWS",
};
STATIC CONST CHAR8 *CONST BootMntPhilosophy[] = {
    "MNT REFORM - SCHEMATICS IN THE BOX", "EVERY CHIP DOCUMENTED, EVERY PART YOURS",
    "OPEN HARDWARE TO THE LAST RESISTOR", "ASSEMBLE IT YOURSELF - THEY DARE YOU",
    "OPEN WHERE THE SILICON ALLOWS",      "REPAIRABLE WITH A SCREWDRIVER AND A PDF",
};
STATIC CONST CHAR8 *CONST BootStarLabsPhilosophy[] = {
    "STAR LABS - COREBOOT, IF YOU CHOOSE IT",
    "AUDITABLE FIRMWARE, WHEN YOU PICK IT",
    "OPEN BOOT AS AN OPTION, NOT A FIGHT",
    "BUILT TO RUN FREE SOFTWARE FREELY",
    "A BIOS YOU CAN OPT TO OPEN",
    "REPAIR-FRIENDLY, ON PURPOSE",
};
STATIC CONST CHAR8 *CONST BootTuxedoPhilosophy[] = {
    "TUXEDO - LINUX FIRST, LOCKS NEVER",    "PURSUING OPEN FIRMWARE, EARNESTLY",
    "BUILT FOR PENGUINS, NOT FOR CAGES",    "YOUR HARDWARE ANSWERS TO YOU",
    "REPAIRABLE, UPGRADEABLE, RESPECTABLE", "NO TELEMETRY SHIPPED IN THE BOOT",
};

STATIC CONST CHAR8 *CONST BootCorebootPhilosophy[] = {
    "COREBOOT DETECTED - A FREER MACHINE. RESPECT.",
    "FAR FEWER BLOBS THAN THE REST",
    "OPEN FIRMWARE - THE WAY IT SHOULD BE",
    "SOMEONE LIBERATED THIS MACHINE. GOOD.",
    "OPEN WHERE THE SILICON PERMITS",
    "MORE OF THE BOOT IS YOURS TO READ",
};

STATIC BOOLEAN BootStrHasCI(CONST CHAR8 *Hay, CONST CHAR8 *Needle)
{
    UINTN i, j;

    for (i = 0; Hay[i] != '\0'; i++) {
        for (j = 0; Needle[j] != '\0'; j++) {
            CHAR8 h = Hay[i + j];
            if (h >= 'a' && h <= 'z') {
                h = (CHAR8)(h - 32);
            }
            if (h != Needle[j]) {
                break;
            }
        }
        if (Needle[j] == '\0') {
            return TRUE;
        }
    }
    return FALSE;
}

STATIC BOOLEAN BootStrEqCI(CONST CHAR8 *A, CONST CHAR8 *B)
{
    UINTN i;

    for (i = 0; A[i] != '\0' && B[i] != '\0'; i++) {
        CHAR8 a = A[i], b = B[i];
        if (a >= 'a' && a <= 'z') {
            a = (CHAR8)(a - 32);
        }
        if (b >= 'a' && b <= 'z') {
            b = (CHAR8)(b - 32);
        }
        if (a != b) {
            return FALSE;
        }
    }
    return A[i] == B[i];
}

STATIC VOID BootPhilosophyPool(CONST CHAR8 * CONST * *Pool, UINTN *Count)
{
    CONST CHAR8 *M = MeridianSystem.Manufacturer;

#define PHILOSOPHY_IF(needle_, arr_)                                                               \
    if (BootStrHasCI(M, needle_)) {                                                                \
        *Pool = arr_;                                                                              \
        *Count = ARRAY_SIZE(arr_);                                                                 \
        return;                                                                                    \
    }

    PHILOSOPHY_IF("FRAMEWORK", BootFrameworkPhilosophy)
    PHILOSOPHY_IF("PURISM", BootPurismPhilosophy)
    PHILOSOPHY_IF("SYSTEM76", BootSystem76Philosophy)
    PHILOSOPHY_IF("MNT RESEARCH", BootMntPhilosophy)
    PHILOSOPHY_IF("STAR LAB", BootStarLabsPhilosophy)
    PHILOSOPHY_IF("STARLAB", BootStarLabsPhilosophy)
    PHILOSOPHY_IF("TUXEDO", BootTuxedoPhilosophy)

    PHILOSOPHY_IF("APPLE", BootApplePhilosophy)
    PHILOSOPHY_IF("DELL", BootDellPhilosophy)

    if (BootStrHasCI(M, "HEWLETT") || BootStrHasCI(M, "HP INC") || BootStrEqCI(M, "HP")) {
        *Pool = BootHpPhilosophy;
        *Count = ARRAY_SIZE(BootHpPhilosophy);
        return;
    }
    PHILOSOPHY_IF("LENOVO", BootLenovoPhilosophy)
    PHILOSOPHY_IF("ASUS", BootAsusPhilosophy)
    PHILOSOPHY_IF("MICRO-STAR", BootMsiPhilosophy)
    PHILOSOPHY_IF("MICROSOFT", BootMicrosoftPhilosophy)
    PHILOSOPHY_IF("SAMSUNG", BootSamsungPhilosophy)
    PHILOSOPHY_IF("SONY", BootSonyPhilosophy)
    PHILOSOPHY_IF("PANASONIC", BootPanasonicPhilosophy)
#undef PHILOSOPHY_IF

    if (MeridianFirmwareVendor == FW_VENDOR_COREBOOT) {
        *Pool = BootCorebootPhilosophy;
        *Count = ARRAY_SIZE(BootCorebootPhilosophy);
        return;
    }

    *Pool = BootGenericPhilosophy;
    *Count = ARRAY_SIZE(BootGenericPhilosophy);
}

STATIC UINT32 BootSplashEntropy(VOID)
{
    EFI_RNG_PROTOCOL *Rng = NULL;
    UINT8 Bytes[4];

    if (gBS != NULL && !EFI_ERROR(gBS->LocateProtocol(&gEfiRngProtocolGuid, NULL, (VOID **)&Rng)) &&
        Rng != NULL && Rng->GetRNG != NULL &&
        !EFI_ERROR(Rng->GetRNG(Rng, NULL, sizeof(Bytes), Bytes))) {
        return (UINT32)Bytes[0] | ((UINT32)Bytes[1] << 8) | ((UINT32)Bytes[2] << 16) |
               ((UINT32)Bytes[3] << 24);
    }
#if defined(MDE_CPU_IA32) || defined(MDE_CPU_X64)
    if (MeridianGetCpuInfo()->HasRdrand) {
        UINT32 Word = 0;
        if (AsmRdRand32(&Word)) {
            return Word;
        }
    }
#endif
    if (gBS != NULL) {
        UINT64 Count = 0;
        if (!EFI_ERROR(gBS->GetNextMonotonicCount(&Count))) {
            return (UINT32)Count ^ (UINT32)(Count >> 32);
        }
    }
    return 0;
}

STATIC CHAR8 mBootSolderedLine[96];

typedef struct
{
    CONST CHAR8 *Sentinel;
    CONST CHAR8 *SolderedFmt;
    CONST CHAR8 *NotSolderedTxt;
} BOOT_SOLDERED_BRAND;

STATIC CONST BOOT_SOLDERED_BRAND mBootSolderedBrands[] = {
    {BOOT_SOLDERED_SENTINEL_APPLE, "%Lu GB %a SOLDERED SO LOVE LASTS FOREVER.",
     "REPAIRABILITY SCORE: STILL NOT GREAT."},
    {BOOT_SOLDERED_SENTINEL_DELL, "%Lu GB %a SOLDERED - ONE BAD CHIP, ONE DEAD LAPTOP",
     "AT LEAST IT ISN'T THE RAM THIS TIME."},
    {BOOT_SOLDERED_SENTINEL_LENOVO, "%Lu GB %a SOLDERED - UPGRADES SOLD SEPARATELY",
     "THE WIFI CARD IS STILL BLACKLISTED THOUGH."},
    {BOOT_SOLDERED_SENTINEL_MSI, "%Lu GB %a SOLDERED - SPECS FROZEN AT CHECKOUT",
     "THE RAM SLOT SURVIVED. BARELY."},
    {BOOT_SOLDERED_SENTINEL_SONY, "%Lu GB %a SOLDERED SO YOU CAN'T LEAVE.",
     "THE DISC DRIVE IS STILL PAIRED FOR LIFE THOUGH."},
};

static CONST CHAR8 *BootResolveSentinel(IN CONST CHAR8 *Candidate)
{
    UINT64 Gb;
    UINTN i;
    CONST BOOT_SOLDERED_BRAND *Brand = NULL;

    for (i = 0; i < ARRAY_SIZE(mBootSolderedBrands); i++) {
        if (AsciiStrCmp(Candidate, mBootSolderedBrands[i].Sentinel) == 0) {
            Brand = &mBootSolderedBrands[i];
            break;
        }
    }
    if (Brand == NULL) {
        return Candidate;
    }

    if (!MeridianMemorySoldered) {

        return Brand->NotSolderedTxt;
    }

    Gb = (MeridianSolderedTotalMB + 512) / 1024;
    AsciiSPrint(mBootSolderedLine, sizeof(mBootSolderedLine), Brand->SolderedFmt, Gb,
                MemoryTypeNameA(MeridianSolderedMemType));
    return mBootSolderedLine;
}

VOID ConnBootScreen(VOID)
{
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black = {0, 0, 0, 0};
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL White = {255, 255, 255, 0};
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Glow;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Dim;
    const CHAR8 *Subtitle;
    const CHAR8 * CONST * Lines;
    const CHAR8 * CONST * Roasts;
    UINTN LineCount, RoastCount, Total, Pick;
    UINTN SubCol;
    ConnGrid *Grid = NULL;
    ConnSurface Surf;
    UINTN sw = 0, sh = 0, w, h, cx, cy, i;

    switch (MeridianBootCause) {
    case BOOT_CAUSE_WARM:
        Glow = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0xFF, 0xBB, 0x44, 0};
        Dim = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0x22, 0x18, 0x0A, 0};
        Lines = BootLinesWarm;
        LineCount = ARRAY_SIZE(BootLinesWarm);
        break;
    case BOOT_CAUSE_CRASH:
        Glow = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0x20, 0x44, 0xFF, 0};
        Dim = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0x08, 0x10, 0x30, 0};
        Lines = BootLinesCrash;
        LineCount = ARRAY_SIZE(BootLinesCrash);
        break;
    case BOOT_CAUSE_RESUME:
        Glow = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0xEE, 0x44, 0x88, 0};
        Dim = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0x30, 0x10, 0x22, 0};
        Lines = BootLinesResume;
        LineCount = ARRAY_SIZE(BootLinesResume);
        break;
    case BOOT_CAUSE_POWERFAIL:
        Glow = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0x00, 0xAA, 0xFF, 0};
        Dim = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0x00, 0x28, 0x3C, 0};
        Lines = BootLinesPower;
        LineCount = ARRAY_SIZE(BootLinesPower);
        break;
    default:
        Glow = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0xC6, 0xFF, 0xA8, 0};
        Dim = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0x14, 0x22, 0x0A, 0};
        Lines = BootLinesCold;
        LineCount = ARRAY_SIZE(BootLinesCold);
        break;
    }

    BootPhilosophyPool(&Roasts, &RoastCount);
    Total = LineCount + RoastCount;
    Pick = BootSplashEntropy() % Total;
    Subtitle = (Pick < LineCount) ? Lines[Pick] : Roasts[Pick - LineCount];
    Subtitle = BootResolveSentinel(Subtitle);
    SubCol = 39 - AsciiStrLen(Subtitle) / 2;

    BltClearScreen(TRUE);

    if (!AllowGraphicsMode || (GOPDraw == NULL && UGADraw == NULL) || !VideoSize(&sw, &sh)) {
        return;
    }
    cx = sw / 2;
    cy = sh / 2;

    ConnBloomFill(cx, cy, sw, sh);
    gBS->Stall(60000);

    {
        static const UINT8 hpct[] = {6, 16, 36, 64, 100};
        for (i = 0; i < sizeof(hpct); i++) {
            UINTN bw = (sw * hpct[i]) / 100;
            UINTN dx;
            if (bw < 1)
                bw = 1;
            dx = (sw - bw) / 2;
            RstFill(&Glow, dx, (cy > 1) ? cy - 1 : 0, bw, 3, sw, sh);
            RstFill(&White, dx, cy, bw, 1, sw, sh);
            gBS->Stall(22000);
        }
    }

    {
        static const UINT8 vpct[] = {4, 9, 17, 30, 48, 72, 100};
        for (i = 0; i < sizeof(vpct); i++) {
            UINTN bh = (sh * vpct[i]) / 100;
            UINTN dy;
            if (bh < 1)
                bh = 1;
            dy = (sh - bh) / 2;
            RstFill(&Dim, 0, dy, sw, bh, sw, sh);
            RstFill(&Glow, 0, (cy > 1) ? cy - 1 : 0, sw, 3, sw, sh);
            RstFill(&White, 0, cy, sw, 1, sw, sh);
            gBS->Stall(16000);
        }
    }

    Grid = AllocateZeroPool(sizeof(*Grid));
    w = conn_text_width();
    h = conn_text_height();
    Surf.px = (Grid != NULL) ? AllocateZeroPool(w * h * sizeof(*Surf.px)) : NULL;
    if (Surf.px == NULL) {

        MRD_FREE_POOL(Grid);
        RstFill(&Black, 0, 0, sw, sh, sw, sh);
        return;
    }
    Surf.w = w;
    Surf.h = h;

    conn_grid_clear(Grid, CONN_THEME);
    conn_grid_puts(Grid, 32, 11, "M E R I D I A N", CONN_THEME->hi, CONN_TRANSPARENT, TRUE);
    conn_grid_puts(Grid, SubCol, 13, Subtitle, CONN_THEME->dim, CONN_TRANSPARENT, FALSE);
    conn_text_to_gop(&Surf, Grid, CONN_THEME);
    BlitScaledToGop(Surf.px, w, h);

    MRD_FREE_POOL(Grid);
    MRD_FREE_POOL(Surf.px);
}

static VOID SplashRleExpand(CONST UINT8 *In, UINTN InLen, UINT8 *Out, UINTN OutLen)
{
    UINTN i = 0, o = 0;
    while (i < InLen && o < OutLen) {
        UINT8 m = In[i++];
        if (m < 128) {
            UINTN n = (UINTN)m + 1;
            while (n-- > 0 && i < InLen && o < OutLen) {
                Out[o++] = In[i++];
            }
        }
        else {
            UINTN n = (UINTN)m - 125;
            if (i < InLen) {
                UINT8 v = In[i++];
                while (n-- > 0 && o < OutLen) {
                    Out[o++] = v;
                }
            }
        }
    }
    while (o < OutLen) {
        Out[o++] = 0;
    }
}

static UINTN SplashFind(CONST CHAR8 *Key)
{
    UINTN i;
    for (i = 0; i < CONN_SPLASH_COUNT; i++) {
        if (StartsWithCI(conn_splash_key[i], Key) && StartsWithCI(Key, conn_splash_key[i])) {
            return i;
        }
    }
    return 0;
}

static BOOLEAN ContainsCI(CONST CHAR8 *Hay, CONST CHAR8 *Needle)
{
    UINTN i;
    if (Needle[0] == '\0') {
        return FALSE;
    }
    for (i = 0; Hay[i] != '\0'; i++) {
        if (StartsWithCI(Hay + i, Needle)) {
            return TRUE;
        }
    }
    return FALSE;
}

static UINTN SplashIndexFor(CHAR8 OSType, CONST CHAR16 *Hint)
{
    UINTN i;
    CHAR8 name[48];

    if (Hint != NULL) {
        CleanOsName(name, sizeof name, Hint);
        for (i = 1; i < CONN_SPLASH_COUNT; i++) {
            if (conn_splash_key[i][0] != '\0' && ContainsCI(name, conn_splash_key[i])) {
                return i;
            }
        }
    }
    switch (OSType) {
    case 'M':
    case 'O':
        return SplashFind("macos");
    case 'W':
        return SplashFind("windows");
    case 'L':
    case 'G':
        return SplashFind("linux");
    case 'B':
        return SplashFind("freebsd");
    case 'D':
        return SplashFind("dragonfly");
    case 'N':
        return SplashFind("netbsd");
    case 'K':
        return SplashFind("openbsd");
    default:
        return 0;
    }
}

VOID ConnLaunchSplash(IN CHAR8 OSType, IN CONST CHAR16 *Hint)
{
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black = {0, 0, 0, 0};
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Scratch;
    UINT8 *Cover;
    ConnColor accent;
    UINTN sw = 0, sh = 0, idx, cx, cy, x, y, f, bw, bh;
    UINT32 ar, ag, ab;
    static const UINT8 fpct[] = {12, 30, 55, 80, 100};
    CONST UINTN W = CONN_SPLASH_W, H = CONN_SPLASH_H;

    if (!AllowGraphicsMode || (GOPDraw == NULL && UGADraw == NULL) || !VideoSize(&sw, &sh)) {
        if (gST->ConOut != NULL) {
            gST->ConOut->OutputString(gST->ConOut, L"\r\nMERIDIAN :: LAUNCHING\r\n");
        }
        gBS->Stall(120000);
        return;
    }

    idx = SplashIndexFor(OSType, Hint);
    accent = AccentForOSType(OSType);
    ar = (UINT32)CONN_R(accent);
    ag = (UINT32)CONN_G(accent);
    ab = (UINT32)CONN_B(accent);

    Cover = AllocatePool(W * H);
    Scratch = AllocatePool(W * H * sizeof(*Scratch));
    if (Cover == NULL || Scratch == NULL) {
        MRD_FREE_POOL(Cover);
        MRD_FREE_POOL(Scratch);
        return;
    }
    SplashRleExpand(conn_splash_rle[idx], conn_splash_rle_len[idx], Cover, W * H);

    cx = (sw > W) ? (sw - W) / 2 : 0;
    cy = (sh > H) ? (sh - H) / 2 : 0;
    bw = (W < sw) ? W : sw;
    bh = (H < sh) ? H : sh;

    RstFill(&Black, 0, 0, sw, sh, sw, sh);
    for (f = 0; f < sizeof(fpct); f++) {
        UINT32 fa = fpct[f];
        for (y = 0; y < H; y++) {
            for (x = 0; x < W; x++) {
                UINT32 a = (UINT32)Cover[y * W + x] * fa / 100u;
                EFI_GRAPHICS_OUTPUT_BLT_PIXEL *o = &Scratch[y * W + x];
                o->Blue = (UINT8)(ab * a / 255u);
                o->Green = (UINT8)(ag * a / 255u);
                o->Red = (UINT8)(ar * a / 255u);
                o->Reserved = 0;
            }
        }
        VideoBlt(Scratch, FALSE, cx, cy, bw, bh, W * sizeof(*Scratch));
        gBS->Stall(45000);
    }
    gBS->Stall(300000);

    MRD_FREE_POOL(Cover);
    MRD_FREE_POOL(Scratch);
}

static ConnEvent ListKeyToEvent(EFI_INPUT_KEY *k)
{
    ConnEvent ev;
    ZeroMem(&ev, sizeof ev);
    ev.type = CONN_EVENT_KEY;

    if (k->ScanCode != SCAN_NULL) {
        switch (k->ScanCode) {
        case SCAN_UP:
            ev.key = CONN_KEY_UP;
            break;
        case SCAN_DOWN:
            ev.key = CONN_KEY_DOWN;
            break;
        case SCAN_PAGE_UP:
            ev.key = CONN_KEY_PAGE_UP;
            break;
        case SCAN_PAGE_DOWN:
            ev.key = CONN_KEY_PAGE_DOWN;
            break;
        case SCAN_LEFT:
            ev.key = CONN_KEY_LEFT;
            break;
        case SCAN_RIGHT:
            ev.key = CONN_KEY_RIGHT;
            break;
        case SCAN_HOME:
            ev.key = CONN_KEY_HOME;
            break;
        case SCAN_END:
            ev.key = CONN_KEY_END;
            break;
        case SCAN_ESC:
            ev.key = CONN_KEY_ESCAPE;
            break;
        default:
            ev.key = CONN_KEY_OTHER;
            break;
        }
        return ev;
    }
    if (k->UnicodeChar == CHAR_CARRIAGE_RETURN || k->UnicodeChar == CHAR_LINEFEED) {
        ev.key = CONN_KEY_ENTER;
        return ev;
    }
    if (k->UnicodeChar == 0x1B || k->UnicodeChar == CHAR_BACKSPACE || k->UnicodeChar == L' ') {
        ev.key = CONN_KEY_ESCAPE;
        return ev;
    }
    if (k->UnicodeChar >= L'1' && k->UnicodeChar <= L'9') {
        ev.key = CONN_KEY_DIGIT;
        ev.digit = (INTN)(k->UnicodeChar - L'0');
        return ev;
    }
    ev.key = CONN_KEY_OTHER;
    return ev;
}

UINTN ConnRunSubScreen(IN MERIDIAN_MENU_SCREEN *Screen, IN OUT INTN *DefaultEntryIndex,
                       OUT MERIDIAN_MENU_ENTRY **ChosenEntry)
{
    ConnList *L = NULL;
    ConnGrid *Grid = NULL;
    ConnSurface Surf = {NULL, 0, 0};
    MERIDIAN_MENU_ENTRY *Map[CONN_LIST_MAX];
    MERIDIAN_MENU_ENTRY *Dummy = NULL;
    INTN LocalDef = 9999;
    UINTN w, h, vw, vh, i, n = 0;
    UINTN MenuExit = MENU_EXIT_ESCAPE;
    UINTN Action = MENU_EXIT_ZERO;
    char buf[CONN_LIST_COLS];

    if (Screen == NULL) {
        return MENU_EXIT_ZERO;
    }

    if (ChosenEntry == NULL)
        ChosenEntry = &Dummy;
    if (DefaultEntryIndex == NULL)
        DefaultEntryIndex = &LocalDef;

    if ((GOPDraw == NULL && UGADraw == NULL) || !AllowGraphicsMode || !VideoSize(&vw, &vh) ||
        Screen->Entries == NULL || Screen->EntryCount == 0 || Screen->EntryCount > CONN_LIST_MAX) {
        return MENU_EXIT_ESCAPE;
    }

    L = AllocateZeroPool(sizeof(*L));
    Grid = AllocateZeroPool(sizeof(*Grid));
    w = conn_text_width();
    h = conn_text_height();
    if (L == NULL || Grid == NULL) {
        MRD_FREE_POOL(L);
        MRD_FREE_POOL(Grid);
        return MENU_EXIT_ESCAPE;
    }
    Surf.w = w;
    Surf.h = h;
    Surf.px = AllocateZeroPool(w * h * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    if (Surf.px == NULL) {
        MRD_FREE_POOL(L);
        MRD_FREE_POOL(Grid);
        return MENU_EXIT_ESCAPE;
    }

    Ascii16(buf, sizeof buf, (Screen->Title != NULL) ? Screen->Title : L"OPTIONS");
    conn_list_init(L, buf);
    for (i = 0; Screen->InfoLines != NULL && i < Screen->InfoLineCount; i++) {
        Ascii16(buf, sizeof buf, Screen->InfoLines[i]);
        conn_list_add_info(L, buf);
    }
    for (i = 0; i < Screen->EntryCount && n < CONN_LIST_MAX; i++) {
        MERIDIAN_MENU_ENTRY *E = Screen->Entries[i];
        if (E == NULL)
            continue;
        Ascii16(buf, sizeof buf, (E->Title != NULL) ? E->Title : L"");
        if (conn_list_add(L, buf))
            Map[n++] = E;
    }

    if (n == 0) {
        MRD_FREE_POOL(Surf.px);
        MRD_FREE_POOL(Grid);
        MRD_FREE_POOL(L);
        return MENU_EXIT_ESCAPE;
    }

    conn_list_set_default(L, (*DefaultEntryIndex >= 0) ? (UINTN)*DefaultEntryIndex : (n - 1));

    SwitchToGraphics();
    ReadAllKeyStrokes();
    conn_list_render(Grid, L, CONN_THEME);
    conn_text_to_gop(&Surf, Grid, CONN_THEME);
    BlitScaledToGop(Surf.px, w, h);

    for (;;) {
        EFI_INPUT_KEY Key;
        UINTN Idx;
        ConnCommand Act;

        if (gST->ConIn == NULL || gST->ConIn->WaitForKey == NULL)
            break;
        if (EFI_ERROR(gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Idx)))
            break;
        if (EFI_ERROR(gST->ConIn->ReadKeyStroke(gST->ConIn, &Key)))
            continue;

        if (L->selected < n && (Key.ScanCode == SCAN_F2 || Key.ScanCode == SCAN_INSERT ||
                                Key.UnicodeChar == CHAR_TAB || Key.UnicodeChar == L'+')) {
            *ChosenEntry = Map[L->selected];
            Action = MENU_EXIT_DETAILS;
            break;
        }

        if (Screen->Title != NULL && MrdStrEqualsCI(Screen->Title, L"Manage BootOrder") &&
            L->selected < n && (Key.ScanCode == SCAN_DELETE || Key.UnicodeChar == L'-')) {
            *ChosenEntry = Map[L->selected];
            Action = MENU_EXIT_DELETE;
            break;
        }

        Act = conn_list_update(L, ListKeyToEvent(&Key));
        if (L->status != CONN_LIST_ACTIVE)
            break;
        if (Act == CONN_ACT_REDRAW) {
            conn_list_render(Grid, L, CONN_THEME);
            conn_text_to_gop(&Surf, Grid, CONN_THEME);
            BlitScaledToGop(Surf.px, w, h);
        }
    }

    if (Action != MENU_EXIT_ZERO) {
        MenuExit = Action;
    }
    else if (L->status == CONN_LIST_CHOSEN && L->selected < n) {
        *ChosenEntry = Map[L->selected];
        MenuExit = MENU_EXIT_ENTER;
    }
    else {
        *ChosenEntry = (n > 0) ? Map[L->selected] : NULL;
        MenuExit = MENU_EXIT_ESCAPE;
    }
    *DefaultEntryIndex = (INTN)L->selected;

    MRD_FREE_POOL(Surf.px);
    MRD_FREE_POOL(Grid);
    MRD_FREE_POOL(L);
    GraphicsScreenDirty = TRUE;
    return MenuExit;
}

UINTN ConnRunInfoScreen(IN MERIDIAN_MENU_SCREEN *Screen, IN OUT INTN *DefaultEntryIndex,
                        OUT MERIDIAN_MENU_ENTRY **ChosenEntry)
{
    ConnPanel *P = NULL;
    ConnGrid *Grid = NULL;
    ConnSurface Surf = {NULL, 0, 0};
    MERIDIAN_MENU_ENTRY *Map[CONN_PANEL_ACTS];
    MERIDIAN_MENU_ENTRY *Dummy = NULL;
    INTN LocalDef = 9999;
    UINTN w, h, vw, vh, i, n = 0;
    UINTN MenuExit = MENU_EXIT_ESCAPE;
    char buf[CONN_PANEL_COLS];

    if (Screen == NULL) {
        return MENU_EXIT_ZERO;
    }
    if (ChosenEntry == NULL)
        ChosenEntry = &Dummy;
    if (DefaultEntryIndex == NULL)
        DefaultEntryIndex = &LocalDef;

    if ((GOPDraw == NULL && UGADraw == NULL) || !AllowGraphicsMode || !VideoSize(&vw, &vh)) {
        return MENU_EXIT_ESCAPE;
    }

    {
        UINTN na = 0, aw = 0, j;
        for (j = 0; Screen->Entries != NULL && j < Screen->EntryCount; j++) {
            MERIDIAN_MENU_ENTRY *E = Screen->Entries[j];
            if (E == NULL)
                continue;
            na++;
            aw += ((E->Title != NULL) ? StrLen(E->Title) : 0) + 4 + 1;
        }
        if (na > CONN_PANEL_ACTS || Screen->InfoLineCount > CONN_PANEL_BODY ||
            aw > (UINTN)(CONN_COLS - 2)) {
            return MENU_EXIT_ESCAPE;
        }
    }

    P = AllocateZeroPool(sizeof(*P));
    Grid = AllocateZeroPool(sizeof(*Grid));
    w = conn_text_width();
    h = conn_text_height();
    if (P == NULL || Grid == NULL) {
        MRD_FREE_POOL(P);
        MRD_FREE_POOL(Grid);
        return MENU_EXIT_ESCAPE;
    }
    Surf.w = w;
    Surf.h = h;
    Surf.px = AllocateZeroPool(w * h * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    if (Surf.px == NULL) {
        MRD_FREE_POOL(P);
        MRD_FREE_POOL(Grid);
        return MENU_EXIT_ESCAPE;
    }

    Ascii16(buf, sizeof buf, (Screen->Title != NULL) ? Screen->Title : L"INFORMATION");
    conn_panel_init(P, buf);
    for (i = 0; Screen->InfoLines != NULL && i < Screen->InfoLineCount; i++) {
        Ascii16(buf, sizeof buf, Screen->InfoLines[i]);
        conn_panel_add_body(P, buf);
    }
    for (i = 0; Screen->Entries != NULL && i < Screen->EntryCount && n < CONN_PANEL_ACTS; i++) {
        MERIDIAN_MENU_ENTRY *E = Screen->Entries[i];
        if (E == NULL)
            continue;
        Ascii16(buf, sizeof buf, (E->Title != NULL) ? E->Title : L"");
        if (conn_panel_add_action(P, buf))
            Map[n++] = E;
    }
    if (n == 0) {
        conn_panel_add_action(P, "Continue");
    }

    conn_panel_set_default(P, (*DefaultEntryIndex >= 0) ? (UINTN)*DefaultEntryIndex
                                                        : (P->act_count ? P->act_count - 1 : 0));

    SwitchToGraphics();
    ReadAllKeyStrokes();
    conn_panel_render(Grid, P, CONN_THEME);
    conn_text_to_gop(&Surf, Grid, CONN_THEME);
    BlitScaledToGop(Surf.px, w, h);

    for (;;) {
        EFI_INPUT_KEY Key;
        UINTN Idx;
        ConnCommand Act;

        if (gST->ConIn == NULL || gST->ConIn->WaitForKey == NULL)
            break;
        if (EFI_ERROR(gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Idx)))
            break;
        if (EFI_ERROR(gST->ConIn->ReadKeyStroke(gST->ConIn, &Key)))
            continue;

        Act = conn_panel_update(P, ListKeyToEvent(&Key));
        if (P->status != CONN_PANEL_ACTIVE)
            break;
        if (Act == CONN_ACT_REDRAW) {
            conn_panel_render(Grid, P, CONN_THEME);
            conn_text_to_gop(&Surf, Grid, CONN_THEME);
            BlitScaledToGop(Surf.px, w, h);
        }
    }

    if (P->status == CONN_PANEL_CHOSEN && n > 0 && P->selected < n) {
        *ChosenEntry = Map[P->selected];
        MenuExit = MENU_EXIT_ENTER;
    }
    else {

        *ChosenEntry = (n > 0) ? Map[(P->selected < n) ? P->selected : 0] : NULL;
        MenuExit = MENU_EXIT_ESCAPE;
    }
    *DefaultEntryIndex = (INTN)P->selected;

    MRD_FREE_POOL(Surf.px);
    MRD_FREE_POOL(Grid);
    MRD_FREE_POOL(P);
    GraphicsScreenDirty = TRUE;
    return MenuExit;
}

static BOOLEAN Char16ToAsciiStrict(CONST CHAR16 *Src, char *Dst, UINTN Cap)
{
    UINTN i = 0;
    if (Cap == 0)
        return FALSE;
    if (Src != NULL) {
        for (; Src[i] != 0 && i + 1 < Cap; i++) {
            if (Src[i] < 0x20 || Src[i] > 0x7E)
                return FALSE;
            Dst[i] = (char)Src[i];
        }
        if (Src[i] != 0)
            return FALSE;
    }
    Dst[i] = '\0';
    return TRUE;
}

static CHAR16 *AsciiToChar16Pool(CONST char *Src)
{
    UINTN n = 0, i;
    CHAR16 *Out;
    while (Src[n] != '\0')
        n++;
    Out = AllocatePool((n + 1) * sizeof(CHAR16));
    if (Out == NULL)
        return NULL;
    for (i = 0; i < n; i++)
        Out[i] = (CHAR16)(unsigned char)Src[i];
    Out[n] = 0;
    return Out;
}

static ConnEvent EditKeyToEvent(EFI_INPUT_KEY *k)
{
    ConnEvent ev;
    ZeroMem(&ev, sizeof ev);
    ev.type = CONN_EVENT_KEY;

    if (k->ScanCode != SCAN_NULL) {
        switch (k->ScanCode) {
        case SCAN_LEFT:
            ev.key = CONN_KEY_LEFT;
            break;
        case SCAN_RIGHT:
            ev.key = CONN_KEY_RIGHT;
            break;
        case SCAN_HOME:
            ev.key = CONN_KEY_HOME;
            break;
        case SCAN_END:
            ev.key = CONN_KEY_END;
            break;
        case SCAN_DELETE:
            ev.key = CONN_KEY_DELETE;
            break;
        case SCAN_ESC:
            ev.key = CONN_KEY_ESCAPE;
            break;
        default:
            ev.key = CONN_KEY_OTHER;
            break;
        }
        return ev;
    }
    if (k->UnicodeChar == CHAR_CARRIAGE_RETURN || k->UnicodeChar == CHAR_LINEFEED) {
        ev.key = CONN_KEY_ENTER;
        return ev;
    }
    if (k->UnicodeChar == 0x1B) {
        ev.key = CONN_KEY_ESCAPE;
        return ev;
    }
    if (k->UnicodeChar == CHAR_BACKSPACE) {
        ev.key = CONN_KEY_BACKSPACE;
        return ev;
    }
    if (k->UnicodeChar >= 0x20 && k->UnicodeChar < 0x7F) {
        ev.key = CONN_KEY_TEXT;
        ev.ch = (UINT32)k->UnicodeChar;
        return ev;
    }
    ev.key = CONN_KEY_OTHER;
    return ev;
}

static UINTN ConnEditOptions(IN MERIDIAN_MENU_ENTRY *Entry, OUT MERIDIAN_MENU_ENTRY **ChosenEntry)
{
    LOADER_ENTRY *Le;
    ConnLineEditModel *Ed = NULL;
    ConnGrid *Grid = NULL;
    ConnSurface Surf = {NULL, 0, 0};
    EFI_EVENT Timer = NULL;
    BOOLEAN TimerRunning = FALSE;
    UINTN w, h, vw, vh;
    UINTN MenuExit = MENU_EXIT_ZERO;
    char initial[CONN_EDIT_MAX];
    char title[32], loader[64], volume[48];

    if (Entry == NULL || Entry->Tag != TAG_LOADER) {
        return MENU_EXIT_ZERO;
    }
    Le = (LOADER_ENTRY *)Entry;

    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) {
        return MENU_EXIT_ZERO;
    }

    if ((GOPDraw == NULL && UGADraw == NULL) || !AllowGraphicsMode || !VideoSize(&vw, &vh) ||
        !Char16ToAsciiStrict(Le->LoadOptions, initial, sizeof initial)) {
        return MENU_EXIT_ZERO;
    }

    CleanOsName(title, sizeof title, (Le->Title != NULL) ? Le->Title : Entry->Title);
    PutLoaderBasename(loader, sizeof loader, Le->LoaderPath);
    Ascii16(volume, sizeof volume,
            (Le->Volume != NULL && Le->Volume->VolName != NULL)  ? Le->Volume->VolName
            : (Le->Volume != NULL && Le->Volume->FsName != NULL) ? Le->Volume->FsName
                                                                 : L"");

    Ed = AllocateZeroPool(sizeof(*Ed));
    Grid = AllocateZeroPool(sizeof(*Grid));
    w = conn_text_width();
    h = conn_text_height();
    if (Ed == NULL || Grid == NULL) {
        MRD_FREE_POOL(Ed);
        MRD_FREE_POOL(Grid);
        return MENU_EXIT_ZERO;
    }
    Surf.w = w;
    Surf.h = h;
    Surf.px = AllocateZeroPool(w * h * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    if (Surf.px == NULL) {
        MRD_FREE_POOL(Ed);
        MRD_FREE_POOL(Grid);
        return MENU_EXIT_ZERO;
    }

    conn_line_edit_init(Ed, initial, title, loader, volume);
    INFO_LOG("\nINFO: ConnEditOptions opening Parameter Console for '%a'\n", title);

    SwitchToGraphics();
    if (!EFI_ERROR(gBS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &Timer)) &&
        !EFI_ERROR(gBS->SetTimer(Timer, TimerPeriodic, 10000000ULL))) {
        TimerRunning = TRUE;
    }
    ReadAllKeyStrokes();

    conn_line_edit_render(Grid, Ed, CONN_THEME);
    conn_text_to_gop(&Surf, Grid, CONN_THEME);
    BlitScaledToGop(Surf.px, w, h);

    for (;;) {
        EFI_EVENT Evs[2];
        UINTN Nev = 0, KeyIdx = (UINTN)-1, TimIdx = (UINTN)-1, Idx;
        ConnCommand Act = CONN_ACT_NONE;

        if (gST->ConIn != NULL && gST->ConIn->WaitForKey != NULL) {
            KeyIdx = Nev;
            Evs[Nev++] = gST->ConIn->WaitForKey;
        }
        if (TimerRunning) {
            TimIdx = Nev;
            Evs[Nev++] = Timer;
        }
        if (Nev == 0)
            break;
        if (EFI_ERROR(gBS->WaitForEvent(Nev, Evs, &Idx)))
            break;

        if (Idx == KeyIdx) {
            EFI_INPUT_KEY Key;
            if (!EFI_ERROR(gST->ConIn->ReadKeyStroke(gST->ConIn, &Key))) {
                Act = conn_line_edit_update(Ed, EditKeyToEvent(&Key));
            }
        }
        else if (Idx == TimIdx) {
            Act = conn_line_edit_update(Ed, conn_event_tick());
        }

        if (Ed->status != CONN_EDIT_ACTIVE)
            break;
        if (Act == CONN_ACT_REDRAW) {
            conn_line_edit_render(Grid, Ed, CONN_THEME);
            conn_text_to_gop(&Surf, Grid, CONN_THEME);
            BlitScaledToGop(Surf.px, w, h);
        }
    }

    if (TimerRunning)
        gBS->SetTimer(Timer, TimerCancel, 0);
    if (Timer != NULL)
        gBS->CloseEvent(Timer);

    if (Ed->status == CONN_EDIT_COMMITTED) {
        CHAR16 *New = AsciiToChar16Pool(Ed->buf);
        if (New == NULL) {

            INFO_LOG("WARN: ConnEditOptions commit alloc failed -> back to menu\n");
            MenuExit = MENU_EXIT_ZERO;
        }
        else {
            MRD_FREE_POOL(Le->LoadOptions);
            Le->LoadOptions = New;
            *ChosenEntry = Entry;
            SubScreenBoot = TRUE;
            MenuExit = MENU_EXIT_ENTER;
            INFO_LOG("INFO: ConnEditOptions COMMIT -> boot '%a' opts='%a'\n", title, Ed->buf);
        }
    }
    else {
        MenuExit = MENU_EXIT_ZERO;
    }

    MRD_FREE_POOL(Surf.px);
    MRD_FREE_POOL(Grid);
    MRD_FREE_POOL(Ed);
    GraphicsScreenDirty = TRUE;
    return MenuExit;
}

BOOLEAN ConnPreviewRequested(VOID)
{

    if (SelfDir == NULL) {
        return FALSE;
    }
    return FileExists(SelfDir, L"CONN-PREVIEW");
}

VOID ConnPreviewShow(IN MERIDIAN_MENU_SCREEN *Menu)
{
    ConnEntryList *List;
    ConnModel Model;
    ConnSurface Surf;
    UINTN w, h;

    if ((GOPDraw == NULL && UGADraw == NULL) || !AllowGraphicsMode) {
        INFO_LOG("\nINFO: Conn preview SKIPPED (GOP=%d UGA=%d gfx=%d)\n", (GOPDraw != NULL),
                 (UGADraw != NULL), AllowGraphicsMode);
        return;
    }
    INFO_LOG("\nINFO: Conn preview STARTING via %a\n", (GOPDraw != NULL) ? "GOP" : "UGA");

    List = AllocateZeroPool(sizeof(*List));
    if (List == NULL) {
        return;
    }
    INFO_LOG("\nC O N N   P R E V I E W   (live menu -> Conn renderer)\n");
    BuildConnEntryList(Menu, List);
    FillClock(List);
    FillRenderRes(List);
    FillRngNonce(List);
    FillSysInfo(List);
    INFO_LOG("CONN PREVIEW: clock='%a' live='%a %a' entries=%d\n", List->clock, List->rng_source,
             List->rng_nonce, (UINTN)List->count);

    w = conn_text_width();
    h = conn_text_height();
    Surf.px = AllocateZeroPool(w * h * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    Surf.w = w;
    Surf.h = h;
    if (Surf.px == NULL) {
        MRD_FREE_POOL(List);
        return;
    }

    SwitchToGraphics();
    conn_model_init(&Model, List);
    Model.ui.autoboot = FALSE;
    conn_render_to_surface(&Surf, List, &Model, CONN_THEME);
    BlitScaledToGop(Surf.px, w, h);

    ReadAllKeyStrokes();
    PauseForKey();

    MRD_FREE_POOL(Surf.px);
    MRD_FREE_POOL(List);
    GraphicsScreenDirty = TRUE;
}

static VOID BuildConnMenuList(MERIDIAN_MENU_SCREEN *Menu, ConnEntryList *Out,
                              MERIDIAN_MENU_ENTRY **Map)
{
    UINTN i;

    Out->count = 0;
    Out->default_index = 0;
    Out->allow_autoboot = FALSE;
    if (Menu == NULL || Menu->Entries == NULL) {
        return;
    }

    for (i = 0; i < Menu->EntryCount && Out->count < CONN_MAX_ENTRIES; i++) {
        MERIDIAN_MENU_ENTRY *Me = Menu->Entries[i];
        ConnEntry *E;

        if (Me == NULL) {
            continue;
        }
        E = &Out->entry[Out->count];

        if (Me->Tag == TAG_LOADER) {
            LOADER_ENTRY *Le = (LOADER_ENTRY *)Me;
            FillLoaderMeta(E, Le, Me, Le->Volume);
        }
        else {

            CleanOsName(E->name, sizeof E->name, Me->Title ? Me->Title : L"Tool");
            E->sub[0] = '\0';
            E->part[0] = '\0';
            E->loader[0] = '\0';
            E->accent = CONN_GOLD;
            E->locked = FALSE;
        }

        Map[Out->count] = Me;
        Out->count++;
    }
}

BOOLEAN ConnMenuRequested(VOID) { return TRUE; }

VOID conn_text_to_conout(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut, const ConnGrid *g);

static CHAR16 *ConnPreviousBootIdentity(VOID)
{
    if (!DefaultIsPreviousBoot) {
        return NULL;
    }

    return GetPreviousBootIdentity();
}

static MERIDIAN_MENU_ENTRY *ConnDefaultEntry(MERIDIAN_MENU_SCREEN *Menu, CHAR16 **SelectionName)
{
    UINTN i;

    if (Menu == NULL || Menu->EntryCount == 0 || Menu->Entries == NULL) {
        return NULL;
    }
    if (SelectionName != NULL && *SelectionName != NULL) {
        CHAR16 *Identity = ConnPreviousBootIdentity();

        if (Identity != NULL) {
            for (i = 0; i < Menu->EntryCount; i++) {
                if (Menu->Entries[i] != NULL && Menu->Entries[i]->Title != NULL &&
                    MrdStrIncludesCI(Menu->Entries[i]->Title, *SelectionName) &&
                    EntryMatchesIdentity(Menu->Entries[i], Identity)) {
                    MRD_FREE_POOL(Identity);

                    return Menu->Entries[i];
                }
            }
            MRD_FREE_POOL(Identity);
        }

        for (i = 0; i < Menu->EntryCount; i++) {
            if (Menu->Entries[i] != NULL && Menu->Entries[i]->Title != NULL &&
                MrdStrIncludesCI(Menu->Entries[i]->Title, *SelectionName)) {
                return Menu->Entries[i];
            }
        }
    }

    for (i = 0; i < Menu->EntryCount; i++) {
        if (Menu->Entries[i] != NULL && Menu->Entries[i]->Tag == TAG_LOADER) {
            return Menu->Entries[i];
        }
    }
    return Menu->Entries[0];
}

static VOID ConnMenuPresent(BOOLEAN HaveGop, ConnSurface *Surf, ConnGrid *Grid, ConnEntryList *List,
                            ConnModel *Model, UINTN w, UINTN h)
{
    if (HaveGop) {
        conn_render_to_surface(Surf, List, Model, CONN_THEME);
        BlitScaledToGop(Surf->px, w, h);
    }
    else {
        conn_build_text_grid(Grid, List, Model, &CONN_TXT_EFI);
        conn_text_to_conout(gST->ConOut, Grid);
    }
}

VOID ConnTextScreenHeader(IN CHAR16 *Title)
{
    ConnGrid *Grid;
    UINTN col;

    gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);
    gST->ConOut->ClearScreen(gST->ConOut);

    Grid = AllocateZeroPool(sizeof(*Grid));
    if (Grid != NULL) {
        conn_grid_clear(Grid, &CONN_TXT_EFI);

        if (Title != NULL) {
            char ascii[CONN_COLS];
            UINTN i = 0;
            for (; Title[i] != L'\0' && i + 1 < sizeof(ascii); i++) {
                CHAR16 ch = Title[i];
                ascii[i] = (ch >= 0x20 && ch < 0x7F) ? (char)ch : '?';
            }
            ascii[i] = '\0';
            conn_grid_puts(Grid, 3, 1, ascii, CONN_TXT_EFI.frame, CONN_TRANSPARENT, TRUE);
        }

        for (col = 1; col < CONN_COLS - 1; col++) {
            conn_grid_set(Grid, (INTN)col, 2, 0x2500, CONN_TXT_EFI.frame, CONN_TRANSPARENT, FALSE);
        }

        conn_text_to_conout(gST->ConOut, Grid);
        FreePool(Grid);
    }

    gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);
    gST->ConOut->SetCursorPosition(gST->ConOut, 0, 4);
}

static UINT32 ConnConfiguredScreensaverSeconds(VOID)
{
    if (GlobalConfig.ScreensaverTime < 0) {
        return 1;
    }
    if (GlobalConfig.ScreensaverTime == 0) {
        return 0;
    }
    if (GlobalConfig.ScreensaverTime > (INTN)0xFFFFFFFFu) {
        return 0xFFFFFFFFu;
    }
    return (UINT32)GlobalConfig.ScreensaverTime;
}

#if MERIDIAN_DEBUG > 0
static VOID ConnFsmTraceTransition(const char *Label, const state_machine_t *M,
                                   state_machine_state_id_t Before)
{
    state_machine_state_id_t After = state_machine_current(M);
    const state_machine_def_t *Def;
    const char *Bn, *An;

    if (After == Before) {
        return;
    }
    Def = state_machine_def(M);
    Bn = state_machine_state_name(Def, Before);
    An = state_machine_state_name(Def, After);
    INFO_LOG("CONN-FSM %a: %a -> %a\n", Label, Bn ? Bn : "?", An ? An : "?");
}
#define CONN_FSM_TRACE(label, m, expr)                                                             \
    do {                                                                                           \
        state_machine_state_id_t ConnFsmBefore = state_machine_current(m);                         \
        expr;                                                                                      \
        ConnFsmTraceTransition((label), (m), ConnFsmBefore);                                       \
    } while (0)
#else
#define CONN_FSM_TRACE(label, m, expr)                                                             \
    do {                                                                                           \
        (void)(label);                                                                             \
        expr;                                                                                      \
    } while (0)
#endif

static ConnCommand ConnApplyPointer(ConnPointerSample *Ps, ConnModel *Model, ConnEntryList *List,
                                    UINTN sw, UINTN sh, INTN *LastRow, BOOLEAN *Handled)
{
    ConnCommand Act = CONN_ACT_NONE;

    *Handled = FALSE;
    if (!Ps->present || !(Ps->moved || Ps->left_down || Ps->wheel_dz != 0)) {
        return Act;
    }
    *Handled = TRUE;

    if (conn_ui_in_saver(&Model->ui)) {

        CONN_FSM_TRACE("saver-dismiss-ptr", &Model->ui.machine,
                       Act = conn_update(Model, List, conn_event_key(CONN_KEY_OTHER, 0)));
        *LastRow = -1;
        return Act;
    }

    if (Ps->wheel_dz != 0) {
        INTN BodyRows = (CONN_ROWS - 4) - 5 + 1;
        INTN MaxTop = (INTN)List->sys.count - BodyRows;
        INTN Top = (INTN)Model->rail_top + (Ps->wheel_dz > 0 ? 1 : -1);
        if (MaxTop < 0)
            MaxTop = 0;
        if (Top > MaxTop)
            Top = MaxTop;
        if (Top < 0)
            Top = 0;
        Model->rail_top = (UINTN)Top;
        Model->ui.idle_s = 0;
        Model->ui.saver_frame = 0;
        Act = CONN_ACT_REDRAW;
    }

    if (Ps->moved || Ps->left_down) {
        INTN gcol, grow;
        if (ConnGopToCell(sw, sh, Ps->x, Ps->y, &gcol, &grow)) {
            INTN Hit = conn_menu_hit_index(List, Model->ui.selected, gcol, grow);
            if (Hit >= 0 && Ps->left_pressed) {

                CONN_FSM_TRACE("point-click", &Model->ui.machine,
                               Act = conn_update(Model, List, conn_event_point((UINTN)Hit, TRUE)));
                *LastRow = Hit;
            }
            else if (Hit >= 0 && Hit != *LastRow) {

                CONN_FSM_TRACE("point-hover", &Model->ui.machine,
                               Act = conn_update(Model, List, conn_event_point((UINTN)Hit, FALSE)));
                *LastRow = Hit;
            }
        }
    }
    return Act;
}

UINTN ConnRunMainMenu(IN MERIDIAN_MENU_SCREEN *Menu, IN OUT CHAR16 **SelectionName,
                      OUT MERIDIAN_MENU_ENTRY **ChosenEntry)
{
    ConnEntryList *List;
    MERIDIAN_MENU_ENTRY *Map[CONN_MAX_ENTRIES];
    MERIDIAN_MENU_ENTRY *OptionsEntry;
    ConnModel Model;
    ConnSurface Surf;
    ConnGrid *Grid;
    BOOLEAN HaveGop;
    UINTN w, h, i, Def, vw, vh;
    EFI_EVENT Timer;
    BOOLEAN TimerRunning;
    EFI_EVENT GlobeTimer = NULL;
    BOOLEAN GlobeRunning = FALSE;
    ConnPointer Ptr;
    INTN LastPtrRow = -1;
    BOOLEAN Launched;
    BOOLEAN TimedOut;
    BOOLEAN Options;
    BOOLEAN WantEditor;
    UINTN MenuExit;

RestartConn:
    List = NULL;
    Grid = NULL;
    OptionsEntry = NULL;
    Def = 0;
    vw = 0;
    vh = 0;
    Timer = NULL;
    TimerRunning = FALSE;
    Launched = FALSE;
    TimedOut = FALSE;
    Options = FALSE;
    WantEditor = FALSE;
    MenuExit = MENU_EXIT_ESCAPE;
    for (i = 0; i < CONN_MAX_ENTRIES; i++) {
        Map[i] = NULL;
    }

    HaveGop = (GOPDraw != NULL || UGADraw != NULL) && AllowGraphicsMode && VideoSize(&vw, &vh);

    if (Menu == NULL || Menu->EntryCount == 0) {
        INFO_LOG("\nWARN: ConnRunMainMenu has no entries -> rescan\n");
        return MENU_EXIT_ESCAPE;
    }

    if ((!HaveGop && gST->ConOut == NULL) || Menu->EntryCount > CONN_MAX_ENTRIES) {
        INFO_LOG("\nWARN: ConnRunMainMenu cannot present "
                 "(GOP=%d UGA=%d gfx=%d ConOut=%d entries=%d) -> auto-boot default\n",
                 (GOPDraw != NULL), (UGADraw != NULL), AllowGraphicsMode, (gST->ConOut != NULL),
                 (UINTN)Menu->EntryCount);
        *ChosenEntry = ConnDefaultEntry(Menu, SelectionName);
        return (*ChosenEntry != NULL) ? MENU_EXIT_TIMEOUT : MENU_EXIT_ESCAPE;
    }

    List = AllocateZeroPool(sizeof(*List));
    if (List == NULL) {
        *ChosenEntry = ConnDefaultEntry(Menu, SelectionName);
        return (*ChosenEntry != NULL) ? MENU_EXIT_TIMEOUT : MENU_EXIT_ESCAPE;
    }
    BuildConnMenuList(Menu, List, Map);
    if (List->count == 0) {

        MRD_FREE_POOL(List);
        return MENU_EXIT_ESCAPE;
    }
    ConnLoadTheme();
    FillClock(List);
    FillRenderRes(List);
    FillRngNonce(List);
    FillSysInfo(List);

    {
        BOOLEAN Matched = FALSE;
        UINTN FirstLoader = List->count;

        for (i = 0; i < List->count; i++) {
            if (Map[i] != NULL && Map[i]->Tag == TAG_LOADER) {
                FirstLoader = i;
                break;
            }
        }

        Def = (FirstLoader < List->count) ? FirstLoader : 0;

        if (SelectionName != NULL && *SelectionName != NULL) {
            CHAR16 *Identity = ConnPreviousBootIdentity();

            if (Identity != NULL) {
                for (i = 0; i < List->count; i++) {
                    if (Map[i] != NULL && Map[i]->Title != NULL &&
                        MrdStrIncludesCI(Map[i]->Title, *SelectionName) &&
                        EntryMatchesIdentity(Map[i], Identity)) {
                        Def = i;
                        Matched = TRUE;
                        break;
                    }
                }
                MRD_FREE_POOL(Identity);
            }

            for (i = 0; !Matched && i < List->count; i++) {
                if (Map[i] != NULL && Map[i]->Title != NULL &&
                    MrdStrIncludesCI(Map[i]->Title, *SelectionName)) {
                    Def = i;
                    Matched = TRUE;
                    break;
                }
            }
            if (!Matched) {
                INFO_LOG("\nWARN: default_selection '%s' matched no entry -> "
                         "falling back to first bootable OS (idx %d)\n",
                         *SelectionName, (UINTN)Def);
            }
        }
    }
    List->default_index = Def;

    List->allow_autoboot = (Menu->TimeoutSeconds > 0);

    conn_model_init(&Model, List);
    if (Menu->TimeoutSeconds > 0) {
        Model.ui.countdown_s = (UINT32)Menu->TimeoutSeconds;
    }
    conn_ui_set_screensaver(&Model.ui, ConnConfiguredScreensaverSeconds());
    if (GlobalConfig.ScreensaverTime < 0 && !Model.ui.autoboot) {
        CONN_FSM_TRACE("init-tick", &Model.ui.machine,
                       (void)conn_update(&Model, List, conn_event_tick()));
    }

    w = conn_text_width();
    h = conn_text_height();
    Surf.w = w;
    Surf.h = h;
    Surf.px = NULL;
    if (HaveGop) {
        Surf.px = AllocateZeroPool(w * h * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
        if (Surf.px == NULL) {
            MRD_FREE_POOL(List);
            *ChosenEntry = ConnDefaultEntry(Menu, SelectionName);
            return (*ChosenEntry != NULL) ? MENU_EXIT_TIMEOUT : MENU_EXIT_ESCAPE;
        }
        SwitchToGraphics();
    }
    else {

        Grid = AllocateZeroPool(sizeof(ConnGrid));
        if (Grid == NULL) {
            MRD_FREE_POOL(List);
            *ChosenEntry = ConnDefaultEntry(Menu, SelectionName);
            return (*ChosenEntry != NULL) ? MENU_EXIT_TIMEOUT : MENU_EXIT_ESCAPE;
        }
        SwitchToText(FALSE);
    }

    if (!EFI_ERROR(gBS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &Timer)) &&
        !EFI_ERROR(gBS->SetTimer(Timer, TimerPeriodic, 10000000ULL))) {
        TimerRunning = TRUE;
    }
    else {
        Model.ui.autoboot = FALSE;
    }

    if (!EFI_ERROR(gBS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &GlobeTimer)) &&
        !EFI_ERROR(gBS->SetTimer(GlobeTimer, TimerPeriodic, 1500000ULL))) {
        GlobeRunning = TRUE;
    }

    ReadAllKeyStrokes();

    ZeroMem(&Ptr, sizeof Ptr);
    if (HaveGop) {
        ConnPointerInit(&Ptr, vw, vh);
    }
    LastPtrRow = -1;

#if MERIDIAN_DEBUG > 0

    INFO_LOG("\nINFO: Conn pointer -> Absolute=%s Simple=%s HaveGop=%s\n",
             (Ptr.Abs != NULL) ? L"yes" : L"no", (Ptr.Simp != NULL) ? L"yes" : L"no",
             HaveGop ? L"yes" : L"no");
#endif

    ConnMenuPresent(HaveGop, &Surf, Grid, List, &Model, w, h);

    for (;;) {
        EFI_EVENT Evs[5];
        UINTN Nev = 0, KeyIdx = (UINTN)-1, TimIdx = (UINTN)-1, GlobeIdx = (UINTN)-1,
              PtrIdx = (UINTN)-1, SimpIdx = (UINTN)-1, Idx;
        ConnCommand Act = CONN_ACT_NONE;
        BOOLEAN DoTick = FALSE;

        if (gST->ConIn != NULL && gST->ConIn->WaitForKey != NULL) {
            KeyIdx = Nev;
            Evs[Nev++] = gST->ConIn->WaitForKey;
        }
        if (TimerRunning) {
            TimIdx = Nev;
            Evs[Nev++] = Timer;
        }
        if (GlobeRunning) {
            GlobeIdx = Nev;
            Evs[Nev++] = GlobeTimer;
        }
        if (Ptr.Abs != NULL && Ptr.Abs->WaitForInput != NULL) {
            PtrIdx = Nev;
            Evs[Nev++] = Ptr.Abs->WaitForInput;
        }
        if (Ptr.Simp != NULL && Ptr.Simp->WaitForInput != NULL) {
            SimpIdx = Nev;
            Evs[Nev++] = Ptr.Simp->WaitForInput;
        }
        if (Nev == 0) {
            break;
        }

        if (EFI_ERROR(gBS->WaitForEvent(Nev, Evs, &Idx))) {
            break;
        }

        if (Idx == KeyIdx) {
            EFI_INPUT_KEY Key;
            EFI_STATUS KeyStatus = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

            if (!EFI_ERROR(KeyStatus) && !(Key.ScanCode == 0 && Key.UnicodeChar == 0)) {
                ConnKey Ck;
                INTN Digit = 0;

                if (conn_ui_in_saver(&Model.ui)) {
                    CONN_FSM_TRACE(
                        "saver-dismiss", &Model.ui.machine,
                        Act = conn_update(&Model, List, conn_event_key(CONN_KEY_OTHER, 0)));
                }
                else {

                    if (Key.ScanCode == SCAN_ESC || Key.UnicodeChar == CHAR_BACKSPACE) {
                        MenuExit = MENU_EXIT_ESCAPE;
                        break;
                    }

                    if (Key.ScanCode == SCAN_LEFT || Key.ScanCode == SCAN_RIGHT) {
                        INTN BodyRows = (CONN_ROWS - 4) - 5 + 1;
                        INTN MaxTop = (INTN)List->sys.count - BodyRows;
                        INTN Top = (INTN)Model.rail_top + (Key.ScanCode == SCAN_RIGHT ? 1 : -1);
                        if (MaxTop < 0)
                            MaxTop = 0;
                        if (Top > MaxTop)
                            Top = MaxTop;
                        if (Top < 0)
                            Top = 0;
                        Model.rail_top = (UINTN)Top;
                        Model.ui.idle_s = 0;
                        Model.ui.saver_frame = 0;
                        Act = CONN_ACT_REDRAW;
                    }
                    else {
                        if (Key.ScanCode == SCAN_UP) {
                            Ck = CONN_KEY_UP;
                        }
                        else if (Key.ScanCode == SCAN_DOWN) {
                            Ck = CONN_KEY_DOWN;
                        }
                        else if (Key.ScanCode == SCAN_PAGE_UP) {
                            Ck = CONN_KEY_PAGE_UP;
                        }
                        else if (Key.ScanCode == SCAN_PAGE_DOWN) {
                            Ck = CONN_KEY_PAGE_DOWN;
                        }
                        else if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
                            Ck = CONN_KEY_ENTER;
                        }

                        else if (Key.ScanCode == SCAN_F2 || Key.ScanCode == SCAN_INSERT ||
                                 Key.UnicodeChar == CHAR_TAB || Key.UnicodeChar == L'+' ||
                                 Key.UnicodeChar == L'e' || Key.UnicodeChar == L'E') {

                            WantEditor = (Key.UnicodeChar == L'e' || Key.UnicodeChar == L'E');
                            Ck = CONN_KEY_OPTIONS;
                        }
                        else if (Key.UnicodeChar == L't' || Key.UnicodeChar == L'T') {

                            ConnCycleTheme();
                            Act = CONN_ACT_REDRAW;
                            goto SkipUpdate;
                        }
                        else if (Key.UnicodeChar == L'n' || Key.UnicodeChar == L'N') {

                            ConnNetPumpCtx Pc = {HaveGop, &Surf, Grid, List, &Model, w, h};
                            Model.ui.cancelled = TRUE;
                            Model.ui.autoboot = FALSE;
                            ConnNetworkBringUp(&Pc);
                            Model.ui.idle_s = 0;
                            Act = CONN_ACT_REDRAW;
                            goto SkipUpdate;
                        }
                        else if (Key.UnicodeChar >= L'1' && Key.UnicodeChar <= L'9') {
                            Ck = CONN_KEY_DIGIT;
                            Digit = (INTN)(Key.UnicodeChar - L'0');
                        }
                        else {
                            Ck = CONN_KEY_OTHER;
                        }
                        CONN_FSM_TRACE("key", &Model.ui.machine,
                                       Act = conn_update(&Model, List, conn_event_key(Ck, Digit)));
                    SkipUpdate:;
                    }
                }
            }

            else if (TimerRunning && !EFI_ERROR(gBS->CheckEvent(Timer))) {
                DoTick = TRUE;
            }
        }
        else if (Idx == TimIdx) {
            DoTick = TRUE;
        }
        else if (Idx == GlobeIdx) {

            if (!conn_ui_in_saver(&Model.ui)) {
                Model.globe_frame++;
                Act = CONN_ACT_REDRAW;
            }
        }
        else if (Idx == PtrIdx) {

            ConnPointerSample Ps;
            UINTN sw, sh;
            BOOLEAN Handled = FALSE;
            if (VideoSize(&sw, &sh)) {
                ConnPointerPollAbsolute(&Ptr, sw, sh, &Ps);
                Act = ConnApplyPointer(&Ps, &Model, List, sw, sh, &LastPtrRow, &Handled);
            }
            if (!Handled && TimerRunning && !EFI_ERROR(gBS->CheckEvent(Timer))) {
                DoTick = TRUE;
            }
        }
        else if (Idx == SimpIdx) {

            ConnPointerSample Ps;
            UINTN sw, sh;
            BOOLEAN Handled = FALSE;
            if (VideoSize(&sw, &sh)) {
                ConnPointerPollSimple(&Ptr, sw, sh, &Ps);
                Act = ConnApplyPointer(&Ps, &Model, List, sw, sh, &LastPtrRow, &Handled);
            }
            if (!Handled && TimerRunning && !EFI_ERROR(gBS->CheckEvent(Timer))) {
                DoTick = TRUE;
            }
        }

        if (DoTick) {
            FillClock(List);
            FillRenderRes(List);
            if (conn_ui_in_saver(&Model.ui)) {

                FillRngNonce(List);
            }
            CONN_FSM_TRACE("tick", &Model.ui.machine,
                           Act = conn_update(&Model, List, conn_event_tick()));
            if (Act == CONN_ACT_LAUNCH) {
                TimedOut = TRUE;
            }
            else {
                Act = CONN_ACT_REDRAW;
            }
        }

        if (Act == CONN_ACT_LAUNCH) {
            Launched = TRUE;
            break;
        }
        if (Act == CONN_ACT_OPTIONS) {
            Options = TRUE;
            break;
        }
        if (Act == CONN_ACT_REDRAW) {
            ConnMenuPresent(HaveGop, &Surf, Grid, List, &Model, w, h);
        }
    }

    if (TimerRunning) {
        gBS->SetTimer(Timer, TimerCancel, 0);
    }
    if (Timer != NULL) {
        gBS->CloseEvent(Timer);
    }
    if (GlobeRunning) {
        gBS->SetTimer(GlobeTimer, TimerCancel, 0);
    }
    if (GlobeTimer != NULL) {
        gBS->CloseEvent(GlobeTimer);
    }

    if (Launched && Model.ui.selected < List->count) {
        *ChosenEntry = Map[Model.ui.selected];

        if (SelectionName != NULL) {
            MRD_FREE_POOL(*SelectionName);
            *SelectionName =
                ((*ChosenEntry)->Title != NULL) ? StrDuplicate((*ChosenEntry)->Title) : NULL;
        }
        Menu->TimeoutSeconds = 0;
        MenuExit = TimedOut ? MENU_EXIT_TIMEOUT : MENU_EXIT_ENTER;
        INFO_LOG("\nINFO: ConnRunMainMenu launch idx=%d exit=%d title='%s'\n",
                 (UINTN)Model.ui.selected, MenuExit,
                 ((*ChosenEntry)->Title != NULL) ? (*ChosenEntry)->Title : L"(null)");
    }
    else if (Options && Model.ui.selected < List->count) {
        OptionsEntry = Map[Model.ui.selected];
    }

    MRD_FREE_POOL(Surf.px);
    MRD_FREE_POOL(Grid);
    MRD_FREE_POOL(List);
    GraphicsScreenDirty = TRUE;

    if (OptionsEntry != NULL && !WantEditor && HaveGop && OptionsEntry->SubScreen != NULL &&
        OptionsEntry->SubScreen->EntryCount > 1) {
        MERIDIAN_MENU_ENTRY *SubChoice = NULL;
        INTN SubDefault = 0;
        UINTN SubExit;

        SubExit = ConnRunSubScreen(OptionsEntry->SubScreen, &SubDefault, &SubChoice);

        if (SubExit == MENU_EXIT_ENTER && SubChoice != NULL && SubChoice->Tag != TAG_RETURN) {
            *ChosenEntry = SubChoice;
            SubScreenBoot = TRUE;
            if (SelectionName != NULL) {
                MRD_FREE_POOL(*SelectionName);
                *SelectionName = (SubChoice->Title != NULL) ? StrDuplicate(SubChoice->Title) : NULL;
            }
            INFO_LOG("\nINFO: ConnRunMainMenu submenu launch title='%s'\n",
                     (SubChoice->Title != NULL) ? SubChoice->Title : L"(null)");
            Menu->TimeoutSeconds = 0;
            return MENU_EXIT_ENTER;
        }
        if (SubExit == MENU_EXIT_DETAILS && SubChoice != NULL && SubChoice->Tag != TAG_RETURN) {
            OptionsEntry = SubChoice;
        }
        else {
            SubScreenBoot = FALSE;
            Menu->TimeoutSeconds = 0;
            goto RestartConn;
        }
    }

    if (OptionsEntry != NULL) {

        MenuExit = ConnEditOptions(OptionsEntry, ChosenEntry);
        if (MenuExit == MENU_EXIT_ZERO) {
            if (SelectionName != NULL) {
                MRD_FREE_POOL(*SelectionName);
                *SelectionName =
                    (OptionsEntry->Title != NULL) ? StrDuplicate(OptionsEntry->Title) : NULL;
            }
            SubScreenBoot = FALSE;
            Menu->TimeoutSeconds = 0;
            goto RestartConn;
        }
        if (SelectionName != NULL && ChosenEntry != NULL && *ChosenEntry != NULL) {
            MRD_FREE_POOL(*SelectionName);
            *SelectionName =
                ((*ChosenEntry)->Title != NULL) ? StrDuplicate((*ChosenEntry)->Title) : NULL;
        }
    }

    return MenuExit;
}
