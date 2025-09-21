/*
 *  Copyright (C) 2024 Free Software Foundation
 *
 * This program is free software ; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY ; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the program ; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <syscalls.h>
#include <testlib.h>

#include <mach/machine/vm_param.h>
#include <mach/std_types.h>
#include <mach/mach_types.h>
#include <mach/vm_wire.h>
#include <mach/vm_param.h>

#include <device.user.h>
#include <gnumach.user.h>
#include <mach.user.h>
#include <mach_port.user.h>


static void test_memobj()
{
  // this emulates maptime() mapping and reading
  struct mapped_time_value *mtime;
  int64_t secs, usecs;
  mach_port_t device, memobj;
  int err;

  err = device_open (device_priv(), 0, "time", &device);
  ASSERT_RET(err, "device_open");
  err = device_map (device, VM_PROT_READ, 0, sizeof(*mtime), &memobj, 0);
  ASSERT_RET(err, "device_map");
  err = mach_port_deallocate (mach_task_self (), device);
  ASSERT_RET(err, "mach_port_deallocate");
  mtime = 0;
  err = vm_map(mach_task_self (), (vm_address_t *)&mtime, sizeof *mtime, 0, 1,
               memobj, 0, 0, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
  ASSERT_RET(err, "vm_map");

  do
    {
      secs = mtime->seconds;
      __sync_synchronize ();
      usecs = mtime->microseconds;
      __sync_synchronize ();
    }
  while (secs != mtime->check_seconds);
  printf("mapped time is %lld.%lld\n",secs, usecs);

  err = mach_port_deallocate (mach_task_self (), memobj);
  ASSERT_RET(err, "mach_port_deallocate");
  err = vm_deallocate(mach_task_self(), (vm_address_t)mtime, sizeof(*mtime));
  ASSERT_RET(err, "vm_deallocate");
}

static void test_wire()
{
  int err = vm_wire_all(host_priv(), mach_task_self(), VM_WIRE_ALL);
  ASSERT_RET(err, "vm_wire_all-ALL");
  err = vm_wire_all(host_priv(), mach_task_self(), VM_WIRE_NONE);
  ASSERT_RET(err, "vm_wire_all-NONE");
  // TODO check that all memory is actually wired or unwired
}

void test_vm_limit()
{
  kern_return_t err;
  vm_address_t mem, mem2, mem3;
  const size_t M_128K = 128l * 1024l;
  const size_t M_128M = 128l * 1024l * 1024l;
  const size_t M_512M = 512l * 1024l * 1024l;
  vm_size_t cur;
  vm_size_t max;

  printf("set VM memory limitations\n");
  err = vm_set_size_limit(mach_host_self(), mach_task_self(), M_128M, M_512M);
  ASSERT_RET(err, "cannot set VM limits");

  printf("check limits are actually saved\n");
  err = vm_get_size_limit(mach_task_self(), &cur, &max);
  ASSERT_RET(err, "getting the VM limit failed");
  ASSERT(cur == M_128M, "cur limit was not expected");
  ASSERT(max == M_512M, "max limit was not expected");

  printf("check we can no longer increase the max limit\n");
  err = vm_set_size_limit(mach_host_self(), mach_task_self(), M_128M, M_512M * 2);
  ASSERT(err == KERN_NO_ACCESS, "raising VM max limit shall fail with KERN_NO_ACCESS");

  printf("alloc some memory below the limit\n");
  err = vm_allocate(mach_task_self(), &mem, M_128K, TRUE);
  ASSERT_RET(err, "allocating memory below the limit must succeed");
  err = vm_deallocate(mach_task_self(), mem, M_128K);
  ASSERT_RET(err, "deallocation failed");

  printf("alloc a bigger chunk to make it hit the limit\n");
  err = vm_allocate(mach_task_self(), &mem, (M_128M * 2), TRUE);
  ASSERT(err == KERN_NO_SPACE, "allocation must fail with KERN_NO_SPACE");

  printf("check that privileged tasks can increase the hard limit\n");
  err = vm_set_size_limit(host_priv(), mach_task_self(), (M_512M + M_128M), M_512M * 2);
  ASSERT_RET(err, "privileged tasks shall be allowed to increase the max limit");

  printf("check limits are actually saved\n");
  err = vm_get_size_limit(mach_task_self(), &cur, &max);
  ASSERT_RET(err, "getting the VM limit failed");
  ASSERT(cur == (M_512M + M_128M), "cur limit was not expected");
  ASSERT(max == (M_512M * 2), "max limit was not expected");

  printf("allocating the bigger chunk with the new limit shall succeed\n");
  err = vm_allocate(mach_task_self(), &mem, (M_128M * 2), TRUE);
  ASSERT_RET(err, "allocation should now succedd");
  err = vm_deallocate(mach_task_self(), mem, (M_128M * 2));
  ASSERT_RET(err, "deallocation failed");

  printf("check that the limit does not apply to VM_PROT_NONE mappings\n");
  err = vm_map(mach_task_self(),
    &mem, (M_512M * 3), 0, 0, MACH_PORT_NULL, 0, 1,
    VM_PROT_NONE, VM_PROT_NONE, VM_INHERIT_COPY);
  ASSERT_RET(err, "allocation of VM_PROT_NONE areas should not be subject to the limit");

  printf("check that the VM_PROT_NONE allocation does not reduce the limit\n");
  err = vm_allocate(mach_task_self(), &mem2, M_512M, TRUE);
  ASSERT_RET(err, "allocation should succedd in spite of the VM_PROT_NONE map");
  err = vm_deallocate(mach_task_self(), mem2, M_512M);
  ASSERT_RET(err, "deallocation failed");
  err = vm_deallocate(mach_task_self(), mem, (M_512M * 3));
  ASSERT_RET(err, "deallocation failed");

  printf("check that allocations demoted to VM_PROT_NONE no longer counts towards the VM limit\n");
  err = vm_allocate(mach_task_self(), &mem, M_512M, TRUE);
  ASSERT_RET(err, "allocating memory below the limit must succeed");
  err = vm_allocate(mach_task_self(), &mem2, M_128M, TRUE);
  /* the current limit is M_512M + M_128M, this allocation should hit the limit */
  ASSERT(err == KERN_NO_SPACE, "allocation must fail with KERN_NO_SPACE");
  err = vm_protect(mach_task_self(), mem, M_512M, TRUE, VM_PROT_NONE);
  ASSERT_RET(err, "could not drop protection to VM_PROT_NONE");
  /* after dropping the protection there should be enough space again */
  err = vm_allocate(mach_task_self(), &mem2, M_128M, TRUE);
  ASSERT_RET(err, "allocating memory below the limit must succeed");
  /* this allocation purpose is showing the failure message to check size_none value */
  err = vm_allocate(mach_task_self(), &mem3, M_512M, TRUE);
  ASSERT(err == KERN_NO_SPACE, "going above the limit should still fail");
  err = vm_deallocate(mach_task_self(), mem2, M_128M);
  ASSERT_RET(err, "deallocation failed");
  err = vm_deallocate(mach_task_self(), mem, M_512M);
  ASSERT_RET(err, "deallocation failed");

  printf("check that protection cannot be upgraded from VM_PROT_NONE\n");
  err = vm_map(mach_task_self(),
    &mem, M_512M, 0, 0, MACH_PORT_NULL, 0, 1,
    VM_PROT_NONE, VM_PROT_NONE, VM_INHERIT_COPY);
  ASSERT_RET(err, "allocation failed");
  /* attempt to broaden the access to the mapped memory */
  err = vm_protect(mach_task_self(), mem, M_512M, TRUE, VM_PROT_ALL);
  ASSERT(err == KERN_PROTECTION_FAILURE, "max protection can only be set to strictier states");
  err = vm_deallocate(mach_task_self(), mem, M_512M);
  ASSERT_RET(err, "deallocation failed");
}

int main(int argc, char *argv[], int envc, char *envp[])
{
  printf("VM_MIN_ADDRESS=0x%p\n", VM_MIN_ADDRESS);
  printf("VM_MAX_ADDRESS=0x%p\n", VM_MAX_ADDRESS);
  test_wire();
  test_memobj();
  test_vm_limit();
  return 0;
}
