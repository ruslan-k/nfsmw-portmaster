#include "softfp_symbols.h"

#include "softfp_bridge.h"

#include <stddef.h>
#include <string.h>

struct softfp_symbol {
    const char *name;
    uintptr_t address;
};

#define SOFTFP_SYMBOL(guest_name, bridge_name) \
    { guest_name, (uintptr_t)&bridge_name }

/*
 * nfsmw_runtime itself is an ARM32/armhf process.  The Android guest DSOs are
 * ARM32 too, but some of them import ARM EABI runtime helpers that are not
 * exported by glibc.  Resolve those imports to the implementation supplied by
 * the ARM Linux toolchain runtime (libgcc/compiler-rt) linked into this same
 * process.  These are the ABI-defined entry points, which is especially
 * important for the *divmod helpers whose results span multiple core
 * registers; replacing them with ordinary C functions would not be safe.
 *
 * Use private C identifiers with assembler names so we only take the helper
 * addresses; their C prototypes are deliberately irrelevant here.
 */
extern void nfsmw_rt_aeabi_uidiv(void) __asm__("__aeabi_uidiv");
extern void nfsmw_rt_aeabi_ui2d(void) __asm__("__aeabi_ui2d");
extern void nfsmw_rt_aeabi_uidivmod(void) __asm__("__aeabi_uidivmod");
extern void nfsmw_rt_aeabi_uldivmod(void) __asm__("__aeabi_uldivmod");
extern void nfsmw_rt_aeabi_dmul(void) __asm__("__aeabi_dmul");
extern void nfsmw_rt_aeabi_dcmplt(void) __asm__("__aeabi_dcmplt");
extern void nfsmw_rt_aeabi_f2d(void) __asm__("__aeabi_f2d");
extern void nfsmw_rt_aeabi_ul2d(void) __asm__("__aeabi_ul2d");
extern void nfsmw_rt_aeabi_idivmod(void) __asm__("__aeabi_idivmod");
extern void nfsmw_rt_aeabi_idiv(void) __asm__("__aeabi_idiv");
extern void nfsmw_rt_aeabi_dadd(void) __asm__("__aeabi_dadd");

static const struct softfp_symbol symbols[] = {
    SOFTFP_SYMBOL("glBlendColor", nfsmw_glBlendColor),
    SOFTFP_SYMBOL("glClearColor", nfsmw_glClearColor),
    SOFTFP_SYMBOL("glClearDepthf", nfsmw_glClearDepthf),
    SOFTFP_SYMBOL("glDepthRangef", nfsmw_glDepthRangef),
    SOFTFP_SYMBOL("glLineWidth", nfsmw_glLineWidth),
    SOFTFP_SYMBOL("glPolygonOffset", nfsmw_glPolygonOffset),
    SOFTFP_SYMBOL("glSampleCoverage", nfsmw_glSampleCoverage),
    SOFTFP_SYMBOL("glTexParameterf", nfsmw_glTexParameterf),
    SOFTFP_SYMBOL("glUniform1f", nfsmw_glUniform1f),
    SOFTFP_SYMBOL("glUniform2f", nfsmw_glUniform2f),
    SOFTFP_SYMBOL("glUniform3f", nfsmw_glUniform3f),
    SOFTFP_SYMBOL("glUniform4f", nfsmw_glUniform4f),
    SOFTFP_SYMBOL("glVertexAttrib1f", nfsmw_glVertexAttrib1f),
    SOFTFP_SYMBOL("glVertexAttrib2f", nfsmw_glVertexAttrib2f),
    SOFTFP_SYMBOL("glVertexAttrib3f", nfsmw_glVertexAttrib3f),
    SOFTFP_SYMBOL("glVertexAttrib4f", nfsmw_glVertexAttrib4f),
    SOFTFP_SYMBOL("acos", nfsmw_acos),
    SOFTFP_SYMBOL("acosf", nfsmw_acosf),
    SOFTFP_SYMBOL("asinf", nfsmw_asinf),
    SOFTFP_SYMBOL("atan2", nfsmw_atan2),
    SOFTFP_SYMBOL("atan2f", nfsmw_atan2f),
    SOFTFP_SYMBOL("ceil", nfsmw_ceil),
    SOFTFP_SYMBOL("ceilf", nfsmw_ceilf),
    SOFTFP_SYMBOL("cos", nfsmw_cos),
    SOFTFP_SYMBOL("cosf", nfsmw_cosf),
    SOFTFP_SYMBOL("exp", nfsmw_exp),
    SOFTFP_SYMBOL("expf", nfsmw_expf),
    SOFTFP_SYMBOL("floor", nfsmw_floor),
    SOFTFP_SYMBOL("floorf", nfsmw_floorf),
    SOFTFP_SYMBOL("fmod", nfsmw_fmod),
    SOFTFP_SYMBOL("fmodf", nfsmw_fmodf),
    SOFTFP_SYMBOL("frexp", nfsmw_frexp),
    SOFTFP_SYMBOL("ldexp", nfsmw_ldexp),
    SOFTFP_SYMBOL("log", nfsmw_log),
    SOFTFP_SYMBOL("log10", nfsmw_log10),
    SOFTFP_SYMBOL("log10f", nfsmw_log10f),
    SOFTFP_SYMBOL("lrintf", nfsmw_lrintf),
    SOFTFP_SYMBOL("modf", nfsmw_modf),
    SOFTFP_SYMBOL("pow", nfsmw_pow),
    SOFTFP_SYMBOL("powf", nfsmw_powf),
    SOFTFP_SYMBOL("rint", nfsmw_rint),
    SOFTFP_SYMBOL("roundf", nfsmw_roundf),
    SOFTFP_SYMBOL("sin", nfsmw_sin),
    SOFTFP_SYMBOL("sinf", nfsmw_sinf),
    SOFTFP_SYMBOL("sqrt", nfsmw_sqrt),
    SOFTFP_SYMBOL("sqrtf", nfsmw_sqrtf),
    SOFTFP_SYMBOL("strtod", nfsmw_strtod),
    SOFTFP_SYMBOL("tan", nfsmw_tan),
    SOFTFP_SYMBOL("tanf", nfsmw_tanf),
    SOFTFP_SYMBOL("__aeabi_uidiv", nfsmw_rt_aeabi_uidiv),
    SOFTFP_SYMBOL("__aeabi_ui2d", nfsmw_rt_aeabi_ui2d),
    SOFTFP_SYMBOL("__aeabi_uidivmod", nfsmw_rt_aeabi_uidivmod),
    SOFTFP_SYMBOL("__aeabi_uldivmod", nfsmw_rt_aeabi_uldivmod),
    SOFTFP_SYMBOL("__aeabi_dmul", nfsmw_rt_aeabi_dmul),
    SOFTFP_SYMBOL("__aeabi_dcmplt", nfsmw_rt_aeabi_dcmplt),
    SOFTFP_SYMBOL("__aeabi_f2d", nfsmw_rt_aeabi_f2d),
    SOFTFP_SYMBOL("__aeabi_ul2d", nfsmw_rt_aeabi_ul2d),
    SOFTFP_SYMBOL("__aeabi_idivmod", nfsmw_rt_aeabi_idivmod),
    SOFTFP_SYMBOL("__aeabi_idiv", nfsmw_rt_aeabi_idiv),
    SOFTFP_SYMBOL("__aeabi_dadd", nfsmw_rt_aeabi_dadd),
};

uintptr_t nfsmw_softfp_resolve(const char *name)
{
    size_t index;

    for (index = 0U; index < sizeof(symbols) / sizeof(symbols[0]); ++index) {
        if (strcmp(name, symbols[index].name) == 0) {
            return symbols[index].address;
        }
    }
    return 0U;
}
