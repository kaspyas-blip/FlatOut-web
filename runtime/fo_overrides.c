/* FlatOut: guest functions replaced by native code (gen with OVERRIDE=<addr>:
 * the lifted body is then sub_<addr>_orig and these take its place).
 *
 * 0x004d96c0  memcpy(dst, src, n), cdecl, returns dst.  AMD's MMX block copy:
 *             it enters its head/tail byte and dword runs with computed jumps
 *             (jmp ecx into the middle of movsb/movsd sequences, 0x4d96ff and
 *             0x4d977e), which a static lift cannot follow.  The guest calls
 *             it through the pointer at 0x694368 (picked at start-up). */
#include "cpu.h"
#include <string.h>
#include <stdlib.h>

void sub_004d96c0_orig(CPU *c);
void sub_004d96c0(CPU *c){
    uint32_t dst = MR32(c->esp + 4u), src = MR32(c->esp + 8u), n = MR32(c->esp + 12u);
    if (n) memmove(g_mem + dst, g_mem + src, n);
    c->eax = dst;
    POP32(c->eip);
}

/* 0x005e62a0, 0x005eb890  the CRT's memmove/memcpy (MSVC 7.1 memcpy.asm), cdecl,
 *             return dst.  Their dword/byte tails are jump tables, and the
 *             overlapping (backward) copy indexes its table with a negated
 *             count (neg ecx; jmp [ecx*4+0x5e6538]): slots BELOW the table's
 *             address, which no forward scan of the table finds.  A race hit
 *             it (0x5e6564 unresolved, then a fault). */
static void fo_memmove(CPU *c){
    uint32_t dst = MR32(c->esp + 4u), src = MR32(c->esp + 8u), n = MR32(c->esp + 12u);
    if (n) memmove(g_mem + dst, g_mem + src, n);
    c->eax = dst;
    POP32(c->eip);
}
void sub_005e62a0_orig(CPU *c);
void sub_005e62a0(CPU *c){ fo_memmove(c); }
void sub_005eb890_orig(CPU *c);
void sub_005eb890(CPU *c){ fo_memmove(c); }

/* 0x0053f625  D3DX's (statically linked) compiler/assembler message sink:
 * (ctx, location, code, fmt, ...).  Logged, then the original runs. */
#include <stdio.h>
void sub_0053f625_orig(CPU *c);
void sub_0053f625(CPU *c){
    static int n = 0;
    if (n < 200 && getenv("FO_D3DXLOG")){ n++;
        uint32_t code = MR32(c->esp + 12u), fmt = MR32(c->esp + 16u);
        uint32_t a1 = MR32(c->esp + 20u), a2 = MR32(c->esp + 24u);
        char f[160] = "?";
        if (fmt) snprintf(f, sizeof f, "%s", (const char*)(g_mem + fmt));
        char s1[64] = "";
        if (a1 > 0x400000u && a1 < 0x30000000u) snprintf(s1, sizeof s1, "%s", (const char*)(g_mem + a1));
        fprintf(stderr, "[d3dx] msg code=%u fmt=\"%s\" a1=%08x(\"%.40s\") a2=%08x from %08x\n", code, f, a1, s1, a2, MR32(c->esp));
    }
    sub_0053f625_orig(c);
}

/* D3DX effect: create the vertex / pixel shader of one shader parameter (thiscall, 1 arg) */
void sub_0055800a_orig(CPU *c);
void sub_0055800a(CPU *c){
    static int n = 0; if (getenv("FO_D3DXLOG") && n++ < 40) fprintf(stderr, "[d3dx] create VS param %08x from %08x\n", MR32(c->esp + 4u), MR32(c->esp));
    sub_0055800a_orig(c);
}
void sub_00558068_orig(CPU *c);
void sub_00558068(CPU *c){
    static int n = 0; if (getenv("FO_D3DXLOG") && n++ < 40) fprintf(stderr, "[d3dx] create PS param %08x from %08x\n", MR32(c->esp + 4u), MR32(c->esp));
    sub_00558068_orig(c);
}

/* D3DX effect: a VertexShader/PixelShader state assignment (thiscall, 4 args) */
void sub_00561948_orig(CPU *c);
void sub_00561948(CPU *c){
    static int n = 0;
    if (n < 80 && getenv("FO_D3DXLOG")){
        uint32_t th = c->ecx, a1 = MR32(c->esp + 4u), a2 = MR32(c->esp + 8u);
        uint32_t pe = MR32(th + 0x2cu) + a1 * 0x64u, pb = MR32(pe + 4u) + a2 * 0x64u;
        uint32_t st = MR16(pb + 0x52u), ty = MR32(0x640b44u + st * 0x1cu);
        fprintf(stderr, "[d3dx] state assign param %u state %u type 0x%x a3=%08x from %08x\n", a1, st, ty, MR32(c->esp + 12u), MR32(c->esp));
        n++; }
    sub_00561948_orig(c);
    if (n < 80 && getenv("FO_D3DXLOG")) fprintf(stderr, "[d3dx]   -> %08x\n", c->eax);
}

/* D3DXCreateEffect(device, data, len, defines, include, flags, pool, ppEffect, ppErrors), stdcall */
void sub_005273a2_orig(CPU *c);
void sub_005273a2(CPU *c){
    uint32_t data = MR32(c->esp + 8u), len = MR32(c->esp + 12u), pperr = MR32(c->esp + 36u);
    char head[200] = ""; uint32_t k = 0;
    /* the first line naming the file is the comment "// pro_x.sha" */
    for (uint32_t i = 0; i + 4 < len && i < 400; i++)
        if (MR8(data+i) == '/' && MR8(data+i+1) == '/' && MR8(data+i+3) != '/'){ 
            for (k = 0; k < 60 && MR8(data+i+3+k) >= 0x20; k++) head[k] = (char)MR8(data+i+3+k); head[k] = 0; break; }
    sub_005273a2_orig(c);
    if ((int32_t)c->eax < 0 || getenv("FO_D3DXLOG")) fprintf(stderr, "[d3dx] D3DXCreateEffect \"%s\" (%u bytes) -> %08x\n", head, len, c->eax);
    if ((int32_t)c->eax < 0 && pperr && MR32(pperr)){
        uint32_t b = MR32(pperr), vt = MR32(b);
        (void)vt;
        /* ID3DXBuffer: data pointer kept by D3DX's own buffer object; print what looks like text */
        for (uint32_t off = 4; off < 32; off += 4){ uint32_t p = MR32(b + off);
            if (p > 0x400000u && p < 0x30000000u && MR8(p) >= 0x20 && MR8(p) < 0x7f){
                fprintf(stderr, "[d3dx]   errors: %.400s\n", (const char*)(g_mem + p)); break; } }
    }
}

/* renderer vtable +0x84 (called right before FlatOut picks shaderlib_pro/emu at 0x438d0c, edi = device entry) */
void sub_005df660_orig(CPU *c);
void sub_005df660(CPU *c){
    if (MR32(c->esp) == 0x00438d0cu && getenv("FO_D3DXLOG"))
        fprintf(stderr, "[fo] shaderlib check: edi=%08x [edi+0x18]=%08x table=%08x idx=%u\n",
                c->edi, MR32(c->edi + 0x18u), MR32(0x6c0220u), MR32(0x6bffa4u));
    sub_005df660_orig(c);
}

/* FO_REGCHECK: which of the calls between 0x438b14 and 0x438d0c returns with another edi */
#define REGWRAP(addr) void sub_##addr##_orig(CPU *c); void sub_##addr(CPU *c){ \
    uint32_t b = c->ebx, si = c->esi, di = c->edi, bp = c->ebp, sp = c->esp; \
    sub_##addr##_orig(c); \
    if (c->ebx != b || c->esi != si || c->edi != di || c->ebp != bp){ static int n = 0; if (n++ < 20) \
        fprintf(stderr, "[regcheck] sub_%s: ebx %08x->%08x esi %08x->%08x edi %08x->%08x ebp %08x->%08x (esp %08x->%08x)\n", \
                #addr, b, c->ebx, si, c->esi, di, c->edi, bp, c->ebp, sp, c->esp); } }
REGWRAP(00504de0)
REGWRAP(00506880)
REGWRAP(00506480)
