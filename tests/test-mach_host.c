
#include <syscalls.h>
#include <testlib.h>

#include <mach_host.user.h>

void test_host()
{
  int err;
  kernel_version_t kver;
  printf("Calling host_kernel_info\n");
  err = host_get_kernel_version(mach_host_self(), kver);
  if (err)
    perror(err, "host_kernel_info");
  printf("kernel version: %s\n", kver);
}

int main(int argc, char *argv[], int envc, char *envp[])
{
  test_host();
  return 0;
}
