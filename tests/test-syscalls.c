
#include <syscalls.h>
#include <testlib.h>

#include <mach/mig_errors.h>
#include <mach.user.h>
#include <mach_port.user.h>
#include <exc.user.h>

void* test_syscall_bad_arg_on_stack(void *arg)
{  
  /* mach_msg() has 7 arguments, so the last one should be passed on the stack
     make RSP points the the wrong place to check the access check
  */
#ifdef __x86_64__
  asm volatile("movq	$0x123,%rsp;"			\
               "movq	$-25,%rax;"                     \
               "syscall;"                               \
               );
#else
  asm volatile("mov	$0x123,%esp;"			\
               "mov	$-25,%eax;"                     \
               "lcall	$0x7,$0x0;"                     \
               );
#endif
  printf("ERROR, we shouldn't be here!\n");
  return NULL;
}

void* test_bad_syscall_num(void *arg)
{  
#ifdef __x86_64__
  asm volatile("movq	$0x123456,%rax;"                \
               "syscall;"                               \
               );
#else
  asm volatile("mov	$0x123456,%eax;"                \
               "lcall	$0x7,$0x0;"                     \
               );
#endif
  printf("ERROR, we shouldn't be here!\n");
  return NULL;
}

int main(int argc, char *argv[], int envc, char *envp[])
{
  /* test_syscall_bad_arg_on_stack(0); */
  /* test_bad_syscall_num(0); */
  /* return 1; */
#ifdef __x86_64__ // FIXME when rpc works
  //test_syscall_bad_arg_on_stack(0);
  test_bad_syscall_num(0);
#else
  int err;
  mach_port_t excp;

  err = mach_port_allocate(mach_task_self (), MACH_PORT_RIGHT_RECEIVE, &excp);
  if (err)
    perror(err, "creating exception port");

  err = mach_port_insert_right(mach_task_self(), excp, excp,
                               MACH_MSG_TYPE_MAKE_SEND);
  if (err)
    perror(err, "inserting sr into exception port");

  err = task_set_special_port(mach_task_self(), TASK_EXCEPTION_PORT, excp);
  if (err)
    perror(err, "setting task exception port");

  test_thread_start(mach_task_self(), test_syscall_bad_arg_on_stack, NULL);

  // TODO: user mig server
  // FIXME: receiving with small size causes GP...
  mig_reply_header_t msg;
  err = mach_msg(&msg.Head,	/* The header */
                 MACH_RCV_MSG,
                 0,
                 sizeof (msg),	/* Max receive Size */
                 excp,
                 1000,
                 MACH_PORT_NULL);

  if (err)
    perror(err, "receiving exc");
#endif
  return 0;
}
