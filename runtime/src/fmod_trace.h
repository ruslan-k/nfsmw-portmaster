#ifndef NFSMW_FMOD_TRACE_H
#define NFSMW_FMOD_TRACE_H

#include "elf32_loader.h"

#include <stddef.h>
#include <stdint.h>

int nfsmw_fmod_trace_bind(const struct elf32_image *fmod_image,
                          char *error, size_t error_size);
uintptr_t nfsmw_fmod_trace_resolve(const char *requesting_soname,
                                   const char *name);

#endif
