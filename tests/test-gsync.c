
#include <syscalls.h>
#include <testlib.h>

#include <mach/machine/vm_param.h>
#include <mach/std_types.h>
#include <mach/mach_types.h>
#include <mach/vm_wire.h>

#include <mach.user.h>
#include <gnumach.user.h>

/* Gsync flags.  */
#ifndef GSYNC_SHARED
# define GSYNC_SHARED      0x01
# define GSYNC_QUAD        0x02
# define GSYNC_TIMED       0x04
# define GSYNC_BROADCAST   0x08
# define GSYNC_MUTATE      0x10
#endif

static uint32_t single_shared;
static uint64_t single_shared_quad;

static void* single_t2(void *arg)
{
  int err;
  msleep(100);
  err = gsync_wake(mach_task_self(), (vm_offset_t)&single_shared, 0, 0);
  if (err)
    perror(err, "gsync_wake t2");

  err = thread_terminate(mach_thread_self());
  if (err)
    perror(err, "thread_terminate");
  return NULL;  // not reached
}

static void test_single()
{
  int err;
  single_shared = 0;
  err = gsync_wait(mach_task_self(), (vm_offset_t)&single_shared, 0, 0, 100, GSYNC_TIMED);
  if (err != KERN_TIMEDOUT)
    perror(err, "gsync_wait 1");

  single_shared = 1;
  err = gsync_wait(mach_task_self(), (vm_offset_t)&single_shared, 0, 0, 100, GSYNC_TIMED);
  if (err != KERN_INVALID_ARGUMENT)
    perror(err, "gsync_wait 2");
  err = gsync_wait(mach_task_self(), (vm_offset_t)&single_shared, 1, 0, 100, GSYNC_TIMED);
  if (err != KERN_TIMEDOUT)
    perror(err, "gsync_wait 2b");

  err = gsync_wake(mach_task_self(), (vm_offset_t)&single_shared, 0, 0);
  if (err)
    perror(err, "gsync_wake 1");

  err = gsync_wake(mach_task_self(), (vm_offset_t)&single_shared, 0, GSYNC_BROADCAST);
  if (err)
    perror(err, "gsync_wake 2");

  err = gsync_wake(mach_task_self(), (vm_offset_t)&single_shared, 2, GSYNC_MUTATE);
  if (err)
    perror(err, "gsync_wake 2");
  if (single_shared != 2)
    perror(single_shared, "gsync_wake single_shared");

  err = gsync_wake(mach_task_self(), (vm_offset_t)&single_shared, 0, 0);
  if (err)
    perror(err, "gsync_wake 3");
  err = gsync_wake(mach_task_self(), (vm_offset_t)&single_shared, 0, 0);
  if (err)
    perror(err, "gsync_wake 3a");

  single_shared = 1;
  err = test_thread_start(mach_task_self(), single_t2, 0);
  if (!err)
    {
      err = gsync_wait(mach_task_self(), (vm_offset_t)&single_shared, 1, 0, 0, 0);
      if (err)
        perror(err, "gsync_wait 3");
    }
}

static void test_single2()
{
  int err;
  err = test_thread_start(mach_task_self(), single_t2, 0);
  if (!err)
    {
      single_shared = 2;
      err = gsync_wait(mach_task_self(), (vm_offset_t)&single_shared, 2, 0, 0, 0);
      if (err)
        perror(err, "gsync_wait 1");
      // should this fail?
      err = gsync_wake(mach_task_self(), (vm_offset_t)&single_shared, 0, 0);
      if (err)
        perror(err, "gsync_wake 2");
    }  
}

int main(int argc, char *argv[], int envc, char *envp[])
{
  test_single2();
  test_single();
  return 0;
}
