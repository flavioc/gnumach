
#include <device/cons.h>
#include <mach/message.h>
#include <mach/mig_errors.h>
#include <mach/kern_return.h>
#include <mach/vm_param.h>

#include <syscalls.h>
#include <testlib.h>

#include <mach.user.h>
#include <mach_host.user.h>


static int argc = 0;
static char *argv_unknown[] = {"unknown", "m1"};
static char **argv = argv_unknown;
static char **envp = NULL;
static int envc = 0;

// hardcoded names during bootstrap
static mach_port_t host_priv_port = 1;
static mach_port_t device_master_port = 2;

static const char* e2s(int err);

void cnputc(char c, vm_offset_t cookie)
{
    char buf[2] = {c, 0};
    mach_print(buf);
}

mach_port_t host_priv(void)
{
    return host_priv_port;
}

mach_port_t device_priv(void)
{
    return device_master_port;
}

void halt()
{
  printf("reboot ...\n");
  int ret = host_reboot(host_priv_port, 0);
  perror(ret, "host_reboot() failed!");
  while (1)
    ;
}

// from sysdeps/mach/usleep.c
int msleep(uint32_t timeout)
{
  mach_port_t recv = mach_reply_port();
  return mach_msg(NULL, MACH_RCV_MSG|MACH_RCV_TIMEOUT|MACH_RCV_INTERRUPT,
                  0, 0, recv, timeout, MACH_PORT_NULL);
}

static const char* e2s(int err)
{
    switch (err)
    {
    case MACH_SEND_INVALID_DATA: return "MACH_SEND_INVALID_DATA";
    case MACH_SEND_INVALID_DEST: return "MACH_SEND_INVALID_DEST";
    case MACH_SEND_TIMED_OUT: return "MACH_SEND_TIMED_OUT";
    case MACH_SEND_INVALID_RIGHT: return "MACH_SEND_INVALID_RIGHT";
    case MACH_SEND_INVALID_HEADER: return "MACH_SEND_INVALID_HEADER";
    case MACH_SEND_INVALID_TYPE: return "MACH_SEND_INVALID_TYPE";
    case MACH_SEND_INVALID_REPLY: return "MACH_SEND_INVALID_REPLY";
    case MACH_SEND_MSG_TOO_SMALL: return "MACH_SEND_MSG_TOO_SMALL";
    case MACH_RCV_INVALID_NAME: return "MACH_RCV_INVALID_NAME";
    case MACH_RCV_TOO_LARGE: return "MACH_RCV_TOO_LARGE";
    case MIG_TYPE_ERROR: return "MIG_TYPE_ERROR";
    case MIG_REPLY_MISMATCH: return "MIG_REPLY_MISMATCH";
    case MIG_SERVER_DIED: return "MIG_SERVER_DIED";
    case KERN_NO_SPACE: return "KERN_NO_SPACE";
    case KERN_INVALID_ARGUMENT: return "KERN_INVALID_ARGUMENT";
    case KERN_FAILURE: return "KERN_FAILURE";
    case KERN_TIMEDOUT: return "KERN_TIMEDOUT";
    case KERN_INVALID_TASK: return "KERN_INVALID_TASK";
    case KERN_INVALID_HOST: return "KERN_INVALID_HOST";
    case KERN_INVALID_RIGHT: return "KERN_INVALID_RIGHT";
    case KERN_INVALID_VALUE: return "KERN_INVALID_VALUE";
    case KERN_NAME_EXISTS: return "KERN_NAME_EXISTS";
    default: return "unknown";
    }
}

void perror(int err, const char *s)
{
    printf("error %s (0x%x): %s\n", e2s(err), err, s);
}

void _start()
{
  mach_port_t in_bootstrap;
  int err;
  err = task_get_special_port (mach_task_self(), TASK_BOOTSTRAP_PORT, &in_bootstrap);
  if (err || in_bootstrap == MACH_PORT_NULL)
    {
      // let's assume we are a bootstrap module
      // magic from _hurd_startup, but here somehow we need +1 instead of +2
      void **argptr = (void **) __builtin_frame_address (0) + 1;
      intptr_t* argcptr = (intptr_t*)argptr;
      argc = argcptr[0];
      argv = (char **) &argcptr[1];
      envp = &argv[argc + 1];
      envc = 0;
      while (envp[envc])
        ++envc;
      host_priv_port = 1; //atoi(argv[2]);
      device_master_port = 2; //= atoi(argv[3]);
    }

  printf("started %s", argv[0]);
  for (int i=1; i<argc; i++)
  {
      printf(" %s", argv[i]);
  }
  printf("\n");

  int ret = main(argc, argv, envc, envp);

  printf("module %s %s exit code %x\n", argv[0], argv[1], ret);
  msleep(100);
  halt();
}

// started from https://github.com/dwarfmaster/mach-ipc/blob/master/minimal_threads/main.c
static uint32_t stack_top[PAGE_SIZE]  __attribute__ ((aligned (PAGE_SIZE)));
int test_thread_start(task_t task, void*(*routine)(void*), void* arg) {
  const vm_size_t stack_size = PAGE_SIZE * 16;
  kern_return_t ret;

  /* Create a new stack in the target task */
  vm_address_t stack;
  ret = vm_allocate(task, &stack, stack_size, TRUE);
  if(ret != KERN_SUCCESS) {
    perror(ret, "vm_allocate");
    return 1;
  }
  /* Protect the lowest page against access to detect stack overflow */
  ret = vm_protect(task, stack, PAGE_SIZE, FALSE, VM_PROT_NONE);
  if(ret != KERN_SUCCESS) {
    perror(ret, "vm_protect");
    return 1;
  }

  long *top = (long*)((vm_offset_t)stack_top + PAGE_SIZE) - 2;
  *top = 0;        /* The return address */
  *(top + 1) = (long)arg; /* The argument */
  ret = vm_write(task, stack + stack_size - PAGE_SIZE, (vm_offset_t)stack_top, PAGE_SIZE);
  if(ret != KERN_SUCCESS) {
    perror(ret, "vm_write");
    return 1;
  }

  /* Create the thread and start it with the given routine and the new stack*/
  thread_t thread;
  ret = thread_create(task, &thread);
  if(ret != KERN_SUCCESS) {
    perror(ret, "thread_create");
    return 1;
  }

  struct i386_thread_state state;
  unsigned int count;

  count = i386_THREAD_STATE_COUNT;
  ret = thread_get_state(thread, i386_REGS_SEGS_STATE,
                         (thread_state_t) &state, &count);
  if(ret != KERN_SUCCESS) {
    perror(ret, "thread_get_state");
    return 1;
  }

#ifdef __x86_64__
  state.rip = (long) routine;
  state.ursp = (long) (stack + stack_size - 8);
  state.rbp = 0;             /* Clear frame pointer*/
#else
  state.eip = (long) routine;
  state.uesp = (long) (stack + stack_size - 8);
  state.ebp = 0;             /* Clear frame pointer*/
#endif
  ret = thread_set_state(thread, i386_REGS_SEGS_STATE,
                         (thread_state_t) &state, i386_THREAD_STATE_COUNT);
  if(ret != KERN_SUCCESS) {
    perror(ret, "thread_set_state");
    return 1;
  }

  ret = thread_resume(thread);
  if(ret != KERN_SUCCESS) {
    perror(ret, "thread_resume");
    return 1;
  }
  return 0;
}
