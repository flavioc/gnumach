/*
 * Copyright (c) 2024 Free Software Foundation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <syscalls.h>
#include <testlib.h>
#include <mach/exception.h>
#include <mach.user.h>
#include <mach_port.user.h>

#if defined(__i386__) || defined(__x86_64__)
#include <mach_i386.user.h>

static void printx(struct i386_xfloat_state *state, int size)
{
  printf("xfloat init %d fp %d exc %d\n",
         state->initialized, state->fpkind, state->exc_status);
  struct i386_xfp_save *xfp = (struct i386_xfp_save *) &state->hw_state[0];
  printf("xfp %x %x %x %x %x %x %x %x %x\n",
         xfp->fp_control, xfp->fp_status, xfp->fp_tag, xfp->fp_eip,
         xfp->fp_cs, xfp->fp_opcode, xfp->fp_dp, xfp->fp_ds, xfp->fp_mxcsr);
  for (int i=0; i<8; i++)
    {
      printf("fp%d", i);
      for (int j=0; j<16; j++)
        printf(" %02x", xfp->fp_reg_word[i][j]);
      printf("\n");
    }
  for (int i=0; i<16; i++)
    {
      printf("xmm%02d", i);
      for (int j=0; j<16; j++)
        printf(" %02x", xfp->fp_xreg_word[i][j]);
      printf("\n");
    }
  printf("header xfp_features %llx bv %llx\n",
         xfp->header.xfp_features, xfp->header.xcomp_bv);
  int rem = size - sizeof(*state) - sizeof(struct i386_xfp_save);
  if (rem > 0)
    {
      int iter = 0;
      while (rem > 0)
        {
          const int len = 16;
          int n;
          if (rem > len)
            n = len;
          else
            n = rem;
          printf("ext");
          for (int j=0; j<n; j++)
            printf(" %02x", xfp->extended[j + iter*len]);
          printf("\n");
          rem -= n;
          iter++;
        }
    }
}

static void thread_fp_getset(void *arg)
{
  int err;
  thread_t th = *(thread_t*)arg;

  wait_thread_suspended(th);

  mach_msg_type_number_t state_count = i386_FLOAT_STATE_COUNT;
  struct i386_float_state state;

  memset(&state, 0, sizeof(state));
  err = thread_get_state(th, i386_FLOAT_STATE,
                        (thread_state_t) &state, &state_count);
  ASSERT_RET(err, "thread_get_state get failed");
  ASSERT(state_count == i386_FLOAT_STATE_COUNT, "bad state_count");

  struct i386_fp_regs *fpr =
      (struct i386_fp_regs *) (&state.hw_state[0] + sizeof(struct i386_fp_save));

  printf("fp regs get:\n");
  for (int i=0; i<8; i++)
  {
      printf("fp%d", i);
      for (int j=0; j<5; j++)
          printf(" %04x", fpr->fp_reg_word[i][j]);
      printf("\n");
  }

  char tmp[10];
  memcpy(tmp, &fpr->fp_reg_word[1][0], sizeof(tmp));
  memcpy(&fpr->fp_reg_word[1][0], &fpr->fp_reg_word[0][0], sizeof(tmp));
  memcpy(&fpr->fp_reg_word[0][0], tmp, sizeof(tmp));

  printf("fp regs set:\n");
  for (int i=0; i<8; i++)
  {
      printf("fp%d", i);
      for (int j=0; j<5; j++)
          printf(" %04x", fpr->fp_reg_word[i][j]);
      printf("\n");
  }

  err = thread_set_state(th, i386_FLOAT_STATE,
                        (thread_state_t) &state, state_count);
  ASSERT_RET(err, "thread_set_state set failed");

  err = thread_resume(th);
  ASSERT_RET(err, "error in thread_resume");
  thread_terminate(mach_thread_self());
  FAILURE("thread_terminate");
}

static void test_fp_state_getset()
{
  int err;
  thread_t th = mach_thread_self();

  /* load some known value in FP registers */
  int n1[1] = {1111111111};
  float d2[1] = {123.456};
  asm volatile ("fildl  %0\n"
                "fldl   %1\n"
                :: "m" (n1), "m" (d2) :);

  /* then switch to the get/set test thread, and wait to be resumed */
  test_thread_start(mach_task_self(), thread_fp_getset, &th);
  err = thread_suspend(th);
  ASSERT_RET(err, "error in thread_suspend");

  /* and check that now we have the values swapped in FP registers */
  int m1[1] = {0};
  float f2[1] = {0.0};
  asm volatile ("fistpl %0\n"
                "fstpl %1\n"
                :: "m" (m1), "m" (f2):);
  int fint, fdec;
  fint = (int)f2[0];
  fdec = (int)((f2[0] - fint) * 1000);
  printf("fp %d %d.%03d\n", m1[0], fint, fdec);
  ASSERT(n1[0] == m1[0], "error in moving int value in fp regs");
  ASSERT(f2[0] == d2[0], "error in moving fp value in fp regs");
}


static void thread_xfp_getset(void *arg)
{
  int err;
  thread_t th = *(thread_t*)arg;

  wait_thread_suspended(th);

  vm_size_t xfp_size;
  err = i386_get_xstate_size(host_priv(), &xfp_size);
  ASSERT_RET(err, "i386_get_xstate_size");

  mach_msg_type_number_t state_count = xfp_size / sizeof(integer_t);
  struct i386_xfloat_state *state = __builtin_alloca(xfp_size);
  printf("xfp size %u min %u\n", xfp_size, sizeof(struct i386_xfloat_state));

  memset(state, 0, xfp_size);
  err = thread_get_state(th, i386_XFLOAT_STATE,
                        (thread_state_t) state, &state_count);
  ASSERT_RET(err, "thread_get_state get failed");
  ASSERT(state_count == (xfp_size / sizeof(integer_t)), "bad state_count");
  printx(state, xfp_size);

  struct i386_xfp_save *xfp = (struct i386_xfp_save *) &state->hw_state[0];
  printf("xmm3 (after get)");
  for (int j=0; j<16; j++)
    printf(" %02x", xfp->fp_xreg_word[3][j]);
  printf("\n");
  for (int j=0; j<16; j++)
    ASSERT(xfp->fp_xreg_word[3][j] == 0x33,
           "register xmm3 wasn't correctly retrieved from the getset thread");

  printf("mxcsr (after get) %04x\n", xfp->fp_mxcsr);
  ASSERT(xfp->fp_mxcsr == 0x1f80, "mxcsr wasn't correctly retrieved from the getset thread");

  memset(xfp->fp_xreg_word[7], 0x77, 16);
  xfp->fp_mxcsr = 0x1fa0;

  err = thread_set_state(th, i386_XFLOAT_STATE,
                        (thread_state_t) state, state_count);
  ASSERT_RET(err, "thread_set_state set failed");

  err = thread_resume(th);
  ASSERT_RET(err, "error in thread_resume");
  thread_terminate(mach_thread_self());
  FAILURE("thread_terminate");
}

static void test_xfp_state_getset()
{
  int err;
  thread_t th = mach_thread_self();

  /* load some known value in XMM registers */
  char buf3[16];
  memset(buf3, 0x33, 16);
  asm volatile ("movups (%0),%%xmm3" :: "r" (buf3) :);

  /* And to MXCSR */
  unsigned int mxcsr = 0x1f80;
  asm volatile ("ldmxcsr %0" :: "m" (mxcsr));

  /* then switch to the get/set test thread, and wait to be resumed */
  test_thread_start(mach_task_self(), thread_xfp_getset, &th);
  err = thread_suspend(th);
  ASSERT_RET(err, "error in thread_suspend");

  /* and check that now we have different values in XMM registers */
  char buf7[16];
  memset(buf7, 0, 16);
  asm volatile ("movups %%xmm7,(%0)" :: "r" (buf7) :);

  printf("xmm7 (after set)");
  for (int j=0; j<16; j++)
    printf(" %02x", buf7[j]);
  printf("\n");
  for (int j=0; j<16; j++)
    ASSERT(buf7[j] == 0x77,
           "register xmm7 wasn't correctly set by the getset thread");

  asm volatile ("stmxcsr %0" :: "m" (mxcsr));
  ASSERT(mxcsr == 0x1fa0, "mxcsr wasn't updated");
}
#endif

int main(int argc, char *argv[], int envc, char *envp[])
{
#if defined(__i386__) || defined(__x86_64__)
  test_fp_state_getset();
  test_xfp_state_getset();
#else
  FAILURE("FP/XSTATE test missing on this arch!");
#endif
  return 0;
}
