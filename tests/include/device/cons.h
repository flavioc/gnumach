
#ifndef CONS_H
#define CONS_H

#include <mach/machine/vm_types.h>

void cnputc(char c, vm_offset_t cookie);
static inline int cngetc() { return 0; }

#endif  /* CONS_H */
