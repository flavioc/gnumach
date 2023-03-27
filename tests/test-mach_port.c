
#include <mach/message.h>
#include <mach/mach_types.h>
#include <mach/vm_param.h>

#include <syscalls.h>
#include <testlib.h>

#include <mach.user.h>
#include <mach_port.user.h>

void test_mach_port(void)
{
  kern_return_t err;

  mach_port_name_t *namesp;
  mach_msg_type_number_t namesCnt;
  mach_port_type_t *typesp;
  mach_msg_type_number_t typesCnt;
  err = mach_port_names(mach_task_self(), &namesp, &namesCnt, &typesp, &typesCnt);
  if (err)
    perror(err, "mach_port_names");
  if (namesCnt != typesCnt)
    printf("error: mach_port_names: different type/name length: %d %d\n", namesCnt, typesCnt);
  for (int i=0; i<namesCnt; i++)
    printf("port name %d type %x\n", namesp[i], typesp[i]);

  /*
   * test mach_port_type()
   * use the ports we have already as bootstrap modules
   * maybe we could do more checks on the bootstrap ports, on other modules
   */
  mach_port_type_t pt;
  err = mach_port_type(mach_task_self(), host_priv(), &pt);
  if (err)
    perror(err, "mach_port_type host_priv");
  if (pt != MACH_PORT_TYPE_SEND)
      perror(pt, "wrong type for host_priv");

  err = mach_port_type(mach_task_self(), device_priv(), &pt);
  if (err)
    perror(err, "mach_port_type device_priv");
  if (pt != MACH_PORT_TYPE_SEND)
      perror(pt, "wrong type for device_priv");

  err = mach_port_rename(mach_task_self(), device_priv(), 111);
  if (err)
      perror(err, "mach_port_rename");
  // FIXME: it seems we can't restore the old name
  err = mach_port_rename(mach_task_self(), 111, 112);
  if (err)
      perror(err, "mach_port_rename restore orig");

  err = mach_port_allocate_name(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, 1000);
  if (err)
      perror(err, "mach_port_allocate_name");
  err = mach_port_allocate_name(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, 1001);
  if (err)
      perror(err, "mach_port_allocate_name");
  err = mach_port_allocate_name(mach_task_self(), MACH_PORT_RIGHT_DEAD_NAME, 1002);
  if (err)
      perror(err, "mach_port_allocate_name");

  mach_port_t newname = 123;
  err = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &newname);
  if (err)
      perror(err, "mach_port_allocate");
  err = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &newname);
  if (err)
      perror(err, "mach_port_allocate");
  err = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_DEAD_NAME, &newname);
  if (err)
      perror(err, "mach_port_allocate");

  err = mach_port_destroy(mach_task_self(), newname);
  if (err)
      perror(err, "mach_port_destroy");

  err = mach_port_deallocate(mach_task_self(), 1002);
  if (err)
      perror(err, "mach_port_deallocate");

  mach_port_urefs_t urefs;
  err = mach_port_get_refs(mach_task_self(), 1002, MACH_PORT_RIGHT_DEAD_NAME, &urefs);
  if (err)
      perror(err, "mach_port_get_refs");

  // TODO test other rpc
}

int main(int argc, char *argv[], int envc, char *envp[])
{
  test_mach_port();
  return 0;
}
