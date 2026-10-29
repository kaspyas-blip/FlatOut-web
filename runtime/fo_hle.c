/* FlatOut (2004) imports that NFSU2's HLE does not answer.
 *
 * win32.c's import dispatcher tries this after win32_ext_call.  Everything
 * here is plain Win32 semantics for what FlatOut.exe (Steam v1.1, Dec 2004)
 * imports and NFSU2 did not: user32 dialog/hook odds and ends, CompareString,
 * locale queries, CreateFileW, D3DX's file mapping (D3DXCreate*FromFile maps
 * the file it loads: 0x53dd0c), ole32, winmm's waveIn/waveOut (no devices:
 * FMOD then takes DirectSound, which it finds with LoadLibrary("dsound.dll")
 * + GetProcAddress -- the synthetic DSOUND entries of gen/all/impargs.c),
 * msacm32 (no codecs) and fo_zoom.dll's ZOOM_Init (the Zoom Platform
 * re-release's injected compatibility layer; nothing calls it).
 *
 * The overlapped Winsock calls (WSASendTo/WSARecvFrom/WSA events) used by the
 * LAN code are stubs here: the network comes after the game boots. */
#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

uint32_t gheap_alloc(int heap, uint32_t n, int zero);
void gheap_free(int heap, uint32_t p);
void win32_set_last_error(CPU *c, uint32_t e);
uint32_t call_guest(CPU *c, uint32_t fn, int nargs, const uint32_t *args);
uint32_t com_directsoundcreate(uint32_t ppDS);
int fileio_call(CPU *c, const char *n, uint32_t *out);
int fileio_fd(uint32_t h);

uint32_t g_fo_kbhook;          /* the guest's WH_KEYBOARD procedure, 0 = none */
uint8_t g_fo_keys[256];         /* key down (0x80) by virtual key, from the messages the game took */

static uint32_t ARG(CPU *c, int i){ return MR32(c->esp + 4u * (uint32_t)i); }
#define GSTR(a) ((const char *)(g_mem + (uint32_t)(a)))

/* ---- file mappings (D3DX) ---------------------------------------------- */
#define MAP_BASE 0x00A80000u
#define MAP_MAX  64
static struct { int used; uint32_t file; uint32_t view; } g_map[MAP_MAX];

static uint32_t map_new(uint32_t file){
    for (int i = 1; i < MAP_MAX; i++) if (!g_map[i].used){
        g_map[i].used = 1; g_map[i].file = file; g_map[i].view = 0; return MAP_BASE + (uint32_t)i; }
    return 0;
}
static int map_idx(uint32_t h){
    if (h < MAP_BASE + 1 || h >= MAP_BASE + MAP_MAX) return -1;
    int i = (int)(h - MAP_BASE); return g_map[i].used ? i : -1;
}
/* CloseHandle on a mapping handle (win32_ext's CloseHandle does not know it) */
int fo_close_mapping(uint32_t h){
    int i = map_idx(h); if (i < 0) return 0;
    g_map[i].used = 0; return 1;
}

static int env_langid(void){
    const char *e = getenv("FO_LANGID");          /* e.g. 0x405 for Czech */
    return e && *e ? (int)strtol(e, 0, 0) : 0x0409;
}

static uint8_t vk_ascii(uint32_t vk, int shift){
    if (vk >= 'A' && vk <= 'Z') return (uint8_t)(shift ? vk : vk + 32);
    if (vk >= '0' && vk <= '9'){
        static const char sh[] = ")!@#$%^&*(";
        return (uint8_t)(shift ? sh[vk - '0'] : vk); }
    switch (vk){
    case 0x20: return ' ';  case 0x08: return 8;  case 0x0D: return 13; case 0x1B: return 27; case 0x09: return 9;
    case 0xBD: return shift ? '_' : '-';  case 0xBB: return shift ? '+' : '=';
    case 0xBE: return shift ? '>' : '.';  case 0xBC: return shift ? '<' : ',';
    case 0xBF: return shift ? '?' : '/';  case 0xBA: return shift ? ':' : ';';
    case 0xDE: return shift ? '"' : '\''; case 0xDB: return shift ? '{' : '[';
    case 0xDD: return shift ? '}' : ']';  case 0xDC: return shift ? '|' : '\\';
    }
    return 0;
}

static int cmp_ci(const char *a, const char *b, int na, int nb, int ci){
    for (int i = 0;; i++){
        int ea = i >= na, eb = i >= nb;
        if (ea || eb) return ea && eb ? 0 : ea ? -1 : 1;
        int x = (unsigned char)a[i], y = (unsigned char)b[i];
        if (ci){ x = tolower(x); y = tolower(y); }
        if (x != y) return x < y ? -1 : 1;
    }
}

int fo_hle_call(CPU *c, const char *dll, const char *n, uint32_t *out){
    uint32_t r = 0;
    (void)dll;
    /* ---- user32 ---- */
    if      (!strcmp(n, "SetWindowsHookExA")){
        /* WH_KEYBOARD (2): FlatOut's keyboard input (0x4fae56 -> proc 0x4fb170);
         * win32_ext.c calls it for every key message the game takes */
        if (ARG(c,0) == 2u){ g_fo_kbhook = ARG(c,1); r = 0x00A7F002u; }
        else r = 0x00A7F000u + (ARG(c,0) & 0xFFu);
        fprintf(stderr, "[fo] SetWindowsHookExA(%u, 0x%08x)\n", ARG(c,0), ARG(c,1)); }
    else if (!strcmp(n, "UnhookWindowsHookEx")){ if (ARG(c,0) == 0x00A7F002u) g_fo_kbhook = 0; r = 1; }
    else if (!strcmp(n, "CallNextHookEx"))      r = 0;
    else if (!strcmp(n, "DialogBoxParamA")){
        /* Nothing to show in a browser.  Template 101 (0x4388a5) is the
         * launcher: IDOK (1) starts the game, anything else quits.  Template
         * 131 (0x4a6fb8) warns that DirectX 9.0c ("4.09.00.0904") is
         * missing: IDOK quits, IDCANCEL carries on. */
        uint32_t t = ARG(c,1);
        r = t == 101u ? 1u : 2u;
        fprintf(stderr, "[fo] DialogBoxParamA(template %u) from 0x%08x -> %s\n", t, c->eip, r == 1u ? "IDOK" : "IDCANCEL"); }
    else if (!strcmp(n, "EndDialog"))           r = 1;
    else if (!strcmp(n, "EnableWindow"))        r = 0;
    else if (!strcmp(n, "GetDlgItem"))          r = 0x00A7E000u + (ARG(c,1) & 0xFFFu);
    else if (!strcmp(n, "SendDlgItemMessageA") || !strcmp(n, "SendDlgItemMessageW")) r = 0;
    else if (!strcmp(n, "GetClassNameA")){
        uint32_t p = ARG(c,1); int sz = (int)ARG(c,2); const char *s = "FlatOut";
        if (p && sz > 0){ int l = (int)strlen(s); if (l >= sz) l = sz - 1; memcpy(g_mem + p, s, (size_t)l); MR8(p + (uint32_t)l) = 0; r = (uint32_t)l; } }
    else if (!strcmp(n, "GetKeyState"))         r = g_fo_keys[ARG(c,0) & 255u] ? 0xFF80u : 0u;   /* SHORT, high bit = down */
    else if (!strcmp(n, "GetKeyboardLayout"))   r = 0x04090409u;
    else if (!strcmp(n, "GetTitleBarInfo")){
        uint32_t p = ARG(c,1); if (p){ uint32_t cb = MR32(p); if (cb < 4 || cb > 64) cb = 44; memset(g_mem + p + 4, 0, cb - 4); } r = 1; }
    else if (!strcmp(n, "LoadImageA"))          r = 0x00A7D000u;
    else if (!strcmp(n, "ShellExecuteA")){
        fprintf(stderr, "[fo] ShellExecuteA(\"%s\") ignored\n", ARG(c,2) ? GSTR(ARG(c,2)) : "");
        r = 2; }                                                        /* <= 32: failed */
    else if (!strcmp(n, "ToAscii")){
        uint32_t vk = ARG(c,0), ks = ARG(c,2), buf = ARG(c,3);
        int shift = ks && (MR8(ks + 0x10) & 0x80);
        uint8_t ch = vk_ascii(vk, shift);
        if (ch && buf){ MR16(buf) = ch; r = 1; } else r = 0; }
    else if (!strcmp(n, "WaitMessage")){ usleep(1000); r = 1; }
    /* ---- gdi32 ---- */
    else if (!strcmp(n, "GetStockObject"))      r = 0x00A7C000u + (ARG(c,0) & 0xFFu);
    else if (!strcmp(n, "DeleteObject"))        r = 1;
    /* ---- kernel32 ---- */
    else if (!strcmp(n, "CompareStringA") || !strcmp(n, "CompareStringW")){
        int wide = n[13] == 'W'; uint32_t fl = ARG(c,1);
        uint32_t s1 = ARG(c,2), s2 = ARG(c,4); int n1 = (int)ARG(c,3), n2 = (int)ARG(c,5);
        char a[1024], b[1024];
        if (n1 < 0){ n1 = 0; while (n1 < 1023 && (wide ? MR16(s1 + 2u*(uint32_t)n1) : MR8(s1 + (uint32_t)n1))) n1++; }
        if (n2 < 0){ n2 = 0; while (n2 < 1023 && (wide ? MR16(s2 + 2u*(uint32_t)n2) : MR8(s2 + (uint32_t)n2))) n2++; }
        if (n1 > 1023) n1 = 1023;
        if (n2 > 1023) n2 = 1023;
        for (int i = 0; i < n1; i++) a[i] = (char)(wide ? MR16(s1 + 2u*(uint32_t)i) : MR8(s1 + (uint32_t)i));
        for (int i = 0; i < n2; i++) b[i] = (char)(wide ? MR16(s2 + 2u*(uint32_t)i) : MR8(s2 + (uint32_t)i));
        r = (uint32_t)(2 + cmp_ci(a, b, n1, n2, (fl & 1u) != 0)); }      /* CSTR_LESS_THAN=1 .. GREATER=3 */
    else if (!strcmp(n, "lstrcmpiA"))            r = (uint32_t)strcasecmp(GSTR(ARG(c,0)), GSTR(ARG(c,1)));
    else if (!strcmp(n, "EnumSystemLocalesA"))   r = 1;
    else if (!strcmp(n, "IsValidLocale"))        r = 1;
    else if (!strcmp(n, "GetUserDefaultLCID"))   r = (uint32_t)env_langid();
    else if (!strcmp(n, "GetUserDefaultLangID")) r = (uint32_t)env_langid();
    else if (!strcmp(n, "GetDateFormatA") || !strcmp(n, "GetTimeFormatA")){
        uint32_t st = ARG(c,2), p = ARG(c,4); int cch = (int)ARG(c,5); char s[64];
        int y, mo, d, h, mi, se;
        if (st){ y = MR16(st); mo = MR16(st + 2); d = MR16(st + 6); h = MR16(st + 8); mi = MR16(st + 10); se = MR16(st + 12); }
        else { time_t t = time(0); struct tm tm; localtime_r(&t, &tm);
               y = tm.tm_year + 1900; mo = tm.tm_mon + 1; d = tm.tm_mday; h = tm.tm_hour; mi = tm.tm_min; se = tm.tm_sec; }
        if (n[3] == 'D') snprintf(s, sizeof s, "%02d/%02d/%04d", mo, d, y);
        else snprintf(s, sizeof s, "%02d:%02d:%02d", h, mi, se);
        int l = (int)strlen(s) + 1;
        if (cch == 0) r = (uint32_t)l;
        else if (p && cch >= l){ memcpy(g_mem + p, s, (size_t)l); r = (uint32_t)l; }
        else { win32_set_last_error(c, 122); r = 0; } }
    else if (!strcmp(n, "InterlockedCompareExchange")){
        uint32_t p = ARG(c,0), x = ARG(c,1), cmp = ARG(c,2);
        r = MR32(p); if (r == cmp) MR32(p) = x; }
    else if (!strcmp(n, "SetEnvironmentVariableA")) r = 1;
    else if (!strcmp(n, "SetStdHandle"))         r = 1;
    else if (!strcmp(n, "VirtualLock"))          r = 1;
    else if (!strcmp(n, "CreateFileW")){
        /* D3DX tries the wide name first: narrow it into scratch memory and
         * let fileio's CreateFileA open it (same seven arguments). */
        static uint32_t scratch = 0;
        if (!scratch) scratch = gheap_alloc(1, 1024, 1);
        uint32_t w = ARG(c,0), k = 0;
        for (; k < 1023 && MR16(w + 2u*k); k++) MR8(scratch + k) = (uint8_t)MR16(w + 2u*k);
        MR8(scratch + k) = 0;
        MR32(c->esp) = scratch;
        fileio_call(c, "CreateFileA", &r);
        MR32(c->esp) = w; }
    else if (!strcmp(n, "CreateFileMappingA")){
        uint32_t f = ARG(c,0);
        r = fileio_fd(f) >= 0 ? map_new(f) : 0;
        if (!r) win32_set_last_error(c, 6); }
    else if (!strcmp(n, "MapViewOfFile")){
        int i = map_idx(ARG(c,0)); r = 0;
        if (i >= 0){
            int fd = fileio_fd(g_map[i].file); struct stat st;
            uint64_t off = ((uint64_t)ARG(c,2) << 32) | ARG(c,3); uint32_t want = ARG(c,4);
            if (fd >= 0 && fstat(fd, &st) == 0 && (uint64_t)st.st_size > off){
                uint64_t len = (uint64_t)st.st_size - off;
                if (want && want < len) len = want;
                uint32_t p = gheap_alloc(1, (uint32_t)len + 16u, 0);
                if (p){
                    uint64_t got = 0;
                    while (got < len){ ssize_t k = pread(fd, g_mem + p + got, (size_t)(len - got), (off_t)(off + got));
                        if (k <= 0) break; got += (uint64_t)k; }
                    if (got == len){ g_map[i].view = p; r = p; } else gheap_free(1, p);
                }
            }
        }
        if (!r) win32_set_last_error(c, 8); }
    else if (!strcmp(n, "UnmapViewOfFile")){
        uint32_t p = ARG(c,0);
        for (int i = 1; i < MAP_MAX; i++) if (g_map[i].used && g_map[i].view == p) g_map[i].view = 0;
        if (p) gheap_free(1, p);
        r = 1; }
    /* ---- ole32 ---- */
    else if (!strcmp(n, "CoInitialize") || !strcmp(n, "CoInitializeEx")) r = 0;
    else if (!strcmp(n, "CoUninitialize"))       r = 0;
    else if (!strcmp(n, "CoCreateInstance")){
        uint32_t ppv = ARG(c,4); if (ppv) MR32(ppv) = 0;
        r = 0x80040154u; }                                               /* REGDB_E_CLASSNOTREG */
    /* ---- winmm: no wave devices ---- */
    else if (!strcmp(n, "waveOutGetNumDevs") || !strcmp(n, "waveInGetNumDevs")) r = 0;
    else if (!strncmp(n, "waveOut", 7) || !strncmp(n, "waveIn", 6)) r = 6;   /* MMSYSERR_NODRIVER */
    /* ---- msacm32: no codecs ---- */
    else if (!strncmp(n, "acm", 3))              r = 512;                    /* ACMERR_NOTPOSSIBLE */
    /* ---- dsound.dll via GetProcAddress (FMOD) ---- */
    else if (!strcmp(n, "DirectSoundCreate8"))   r = com_directsoundcreate(ARG(c,1));
    else if (!strcmp(n, "DirectSoundEnumerateA")){
        static uint32_t desc = 0, mod = 0;
        if (!desc){ desc = gheap_alloc(1, 64, 1); strcpy((char *)(g_mem + desc), "Primary Sound Driver");
                    mod = gheap_alloc(1, 16, 1); }
        uint32_t args[4] = { 0, desc, mod, ARG(c,1) };
        if (ARG(c,0)) call_guest(c, ARG(c,0), 4, args);
        r = 0; }
    else if (!strcmp(n, "DirectSoundCaptureCreate")) { if (ARG(c,1)) MR32(ARG(c,1)) = 0; r = 0x88780078u; }  /* DSERR_NODRIVER */
    else if (!strcmp(n, "DirectSoundCaptureEnumerateA")) r = 0;
    /* ---- ws2_32 overlapped I/O: not yet (LAN comes later) ---- */
    else if (!strcmp(n, "WSACreateEvent"))       { static uint32_t ev = 0x00A7B000u; r = ++ev; }
    else if (!strcmp(n, "WSACloseEvent") || !strcmp(n, "WSAResetEvent")) r = 1;
    else if (!strcmp(n, "WSAWaitForMultipleEvents")){
        uint32_t to = ARG(c,3); usleep((to > 10u ? 10u : to) * 1000u); r = 258; }   /* WSA_WAIT_TIMEOUT */
    else if (!strcmp(n, "WSAGetOverlappedResult")) r = 0;
    else if (!strcmp(n, "WSARecvFrom") || !strcmp(n, "WSASendTo") || !strcmp(n, "WSAStringToAddressA")){
        win32_set_last_error(c, 10050); r = 0xFFFFFFFFu; }                  /* WSAENETDOWN */
    /* ---- fo_zoom.dll ---- */
    else if (!strcmp(n, "ZOOM_Init"))            r = 1;
    else return 0;
    *out = r;
    return 1;
}
