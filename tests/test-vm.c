
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
  // this emulates maptime()
  struct mapped_time_value *mtime;
  mach_port_t device, memobj;
  int err = device_open_new (device_priv(), 0, "time", &device);
  if (err)
    perror(err, "device_open_new");
  err = device_map (device, VM_PROT_READ, 0, sizeof(*mtime), &memobj, 0);
  if (err)
    perror(err, "device_map");
  mach_port_deallocate (mach_task_self (), device);
  if (!err)
    {
      mtime = 0;
      err =
	vm_map (mach_task_self (), (vm_address_t *)&mtime, sizeof *mtime, 0, 1,
		memobj, 0, 0, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
      if (err)
        perror(err, "vm_map");
      mach_port_deallocate (mach_task_self (), memobj);
    }
  if (!err)
    {
      err = vm_deallocate(mach_task_self(), (vm_address_t)mtime, sizeof(*mtime));
      if (err)
        perror(err, "vm_deallocate");
    }
}

static void test_wire()
{
  int err = vm_wire_all(host_priv(), mach_task_self(), VM_WIRE_ALL);
  if (err)
    perror(err, "vm_wire_all");
}

int main(int argc, char *argv[], int envc, char *envp[])
{
  printf("VM_MIN_ADDRESS=0x%p\n", VM_MIN_ADDRESS);
  printf("VM_MAX_ADDRESS=0x%p\n", VM_MAX_ADDRESS);
  test_wire();
  test_memobj();
  return 0;
}
