
#ifndef TESTLIB_H
#define TESTLIB_H

// in freestanding we can only use a few standard headers
//   float.h iso646.h limits.h stdarg.h stdbool.h stddef.h stdint.h

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

//#include <string.h> // we shouldn't include this, but it seems to be ok

#include <mach/mach_types.h>
#include <kern/printf.h>

int msleep(uint32_t timeout);
void perror(int err, const char *s);
int test_thread_start(task_t task, void*(*routine)(void*), void* arg);

mach_port_t host_priv(void);
mach_port_t device_priv(void);

int main(int argc, char *argv[], int envc, char *envp[]);

#endif /* TESTLIB_H */
