
#include <syscalls.h>
#include <testlib.h>

#include <mach/machine/vm_param.h>
#include <mach/std_types.h>
#include <mach/mach_types.h>
#include <mach/vm_wire.h>
#include <mach/vm_param.h>

#include <gnumach.user.h>
#include <mach.user.h>


void test_task()
{
  mach_port_t ourtask = mach_task_self();
  mach_msg_type_number_t count;
  int err;

  struct task_basic_info binfo;
  count = TASK_BASIC_INFO_COUNT;
  printf("TASK_INFO\n");
  err = task_info(ourtask, TASK_BASIC_INFO, (task_info_t)&binfo, &count);
  if (err)
    perror(err, "TASK_BASIC_INFO");

  struct task_events_info einfo;
  count = TASK_EVENTS_INFO_COUNT;
  err = task_info(ourtask, TASK_EVENTS_INFO, (task_info_t)&einfo, &count);
  if (err)
    perror(err, "TASK_EVENTS_INFO");

  struct task_thread_times_info ttinfo;
  count = TASK_THREAD_TIMES_INFO_COUNT;
  err = task_info(ourtask, TASK_THREAD_TIMES_INFO, (task_info_t)&ttinfo, &count);
  if (err)
    perror(err, "TASK_THREAD_TIMES_INFO");
}


void* dummy_thread(void *arg)
{
  printf("started dummy thread\n");
  while (1)
    ;
}

void check_threads(thread_t *threads, mach_msg_type_number_t nthreads)
{  
  for (int tid=0; tid<nthreads; tid++)
    {
      struct thread_basic_info tinfo;
      mach_msg_type_number_t thcount = THREAD_BASIC_INFO_COUNT;
      int err = thread_info(threads[tid], THREAD_BASIC_INFO, (thread_info_t)&tinfo, &thcount);
      if (err)
        perror(err, "thread_info");
      if (thcount != THREAD_BASIC_INFO_COUNT)
        perror(thcount, "thcount");
      printf("th%d (port %d):\n", tid, threads[tid]);
      printf(" user time %d.%06d\n", tinfo.user_time.seconds, tinfo.user_time.microseconds);
      printf(" system time %d.%06d\n", tinfo.system_time.seconds, tinfo.system_time.microseconds);
      printf(" cpu usage %d\n", tinfo.cpu_usage);
      printf(" creation time %d.%06d\n", tinfo.creation_time.seconds, tinfo.creation_time.microseconds);
    }
}

static void test_task_threads()
{
  thread_t *threads;
  mach_msg_type_number_t nthreads;
  int err;

  err = task_threads(mach_task_self(), &threads, &nthreads);
  if (err)
    perror(err, "task_threads");
  if (nthreads != 1)
    perror(nthreads, "nthreads");
  check_threads(threads, nthreads);

  err = test_thread_start(mach_task_self(), dummy_thread, 0);
  if (err)
    perror(err, "start thread");

  err = test_thread_start(mach_task_self(), dummy_thread, 0);
  if (err)
    perror(err, "start thread");

  // let the threads run
  msleep(100);

  err = task_threads(mach_task_self(), &threads, &nthreads);
  if (err)
    perror(err, "task_threads");
  if (nthreads != 3)
    perror(nthreads, "nthreads");
  check_threads(threads, nthreads);
}

void test_new_task()
{
  int err;
  task_t newtask;
  err = task_create(mach_task_self(), 1, &newtask);
  if (err)
    perror(err, "task_create");

  err = task_suspend(newtask);
  if (err)
    perror(err, "task_suspend");

  err = task_set_name(newtask, "newtask");
  if (err)
    perror(err, "task_set_name");

  err = test_thread_start(newtask, dummy_thread, 0);
  if (err)
    perror(err, "start thread");

  err = task_resume(newtask);
  if (err)
    perror(err, "task_resume");

  // we should check it really runs
  msleep(100);

  err = task_terminate(newtask);
  if (err)
    perror(err, "task_terminate");
}

int test_errors()
{
    int err;
    err = task_resume(MACH_PORT_NAME_DEAD);
    if (!err)
        perror(err, "task DEAD");
}


int main(int argc, char *argv[], int envc, char *envp[])
{
	intptr_t x;
	printf("X addr %x\n", &x);
  test_task();
  //test_task_threads();
  //test_new_task();
  //test_errors();
  return 0;
}
