
#include <mach/message.h>
#include <mach/mach_types.h>
#include <mach/vm_param.h>

#include <syscalls.h>
#include <testlib.h>

#include <mach.user.h>
#include <mach_port.user.h>
#include <mach_host.user.h>

#define ECHO_MAX_BODY_LEN 256

static uint32_t align(uint32_t val, size_t aln)
{
  // we should check aln is a power of 2
  aln--;
  return (val + aln) & (~aln);
}

#define align_inline(val, n) { val = align(val, n); }

struct echo_params
{
  mach_port_t rx_port;
  mach_msg_size_t rx_size;
  mach_msg_size_t rx_number;
};

void *
echo_thread (void *arg)
{
  struct echo_params *params = arg;
  int err;
  struct message
  {
    mach_msg_header_t header;
    char body[ECHO_MAX_BODY_LEN];
  } message;

  printf ("thread echo started\n");
  for (mach_msg_size_t i=0; i<params->rx_number; i++)
    {
      message.header.msgh_local_port = params->rx_port;
      message.header.msgh_size = sizeof (message);

      err = mach_msg (&message.header,
                      MACH_RCV_MSG,
                      0, sizeof (message),
                      params->rx_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
      if (err)
        perror (err, "mach_msg echo rx");
      if (message.header.msgh_size != params->rx_size)
        printf ("error: wrong size in echo rx: %d expected %d\n",
                message.header.msgh_size,params->rx_size);

      message.header.msgh_local_port = MACH_PORT_NULL;
      printf ("echo: msgh_id %d\n", message.header.msgh_id);

      err = mach_msg (&message.header,
                      MACH_SEND_MSG,
                      message.header.msgh_size, 0,
                      MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
      if (err)
        perror (err, "mach_msg echo tx");
    }
  printf ("thread echo stopped\n");
  thread_terminate (mach_thread_self ());
  return NULL;
}

#define TEST_ITERATIONS 3

// TODO run_test_iterations
void
test_iterations (void)
{
  mach_port_t port, receive;
  int err;
  struct message
  {
    mach_msg_header_t header;
    mach_msg_type_t type;
    char data[64];
  } message;

  err = mach_port_allocate (mach_task_self (),
			    MACH_PORT_RIGHT_RECEIVE, &port);
  if (err)
    perror (err, "mach_port_allocate");

  err = mach_port_allocate (mach_task_self (),
			    MACH_PORT_RIGHT_RECEIVE, &receive);
  if (err)
    perror (err, "mach_port_allocate 2");

  struct echo_params params;
  params.rx_port = port;
  params.rx_size = 36;
  params.rx_number = TEST_ITERATIONS;
  test_thread_start (mach_task_self (), echo_thread, &params);

  time_value_t start_time;
  err = host_get_time (mach_host_self (), &start_time);
  if (err)
    perror (err, "host_get_time");

  /* Send a message down the port */
  for (int i = 0; i < TEST_ITERATIONS; i++)
    {
      struct message message;

      memset (&message, 0, sizeof (message));
      strcpy (message.data, "ciao");
      size_t datalen = strlen (message.data) + 1;

      message.header.msgh_bits
	= MACH_MSGH_BITS (MACH_MSG_TYPE_MAKE_SEND,
			  MACH_MSG_TYPE_MAKE_SEND_ONCE);
      message.header.msgh_remote_port = port;	/* Request port */
      message.header.msgh_local_port = receive;	/* Reply port */
      message.header.msgh_id = 123;	/* Message id */
      message.header.msgh_size = sizeof (message.header) + sizeof (message.type) + datalen;	/* Message size */
      align_inline(message.header.msgh_size, 4);
      message.type.msgt_name = MACH_MSG_TYPE_STRING;	/* Parameter type */
      message.type.msgt_size = 8 * datalen;	/* # Bits */
      message.type.msgt_number = 1;	/* Number of elements */
      message.type.msgt_inline = TRUE;	/* Inlined */
      message.type.msgt_longform = FALSE;	/* Shortform */
      message.type.msgt_deallocate = FALSE;	/* Do not deallocate */
      message.type.msgt_unused = 0;	/* = 0 */

      /* Send the message on its way and wait for a reply.  */
      err = mach_msg (&message.header,	/* The header */
		      MACH_SEND_MSG | MACH_RCV_MSG,	/* Flags */
		      message.header.msgh_size,	/* Send size */
		      sizeof (message),	/* Max receive Size */
		      receive,	/* Receive port */
		      MACH_MSG_TIMEOUT_NONE,	/* No timeout */
		      MACH_PORT_NULL);	/* No notification */
      if (err)
	perror (err, "mach_msg txrx");
    }

  time_value_t stop_time;
  err = host_get_time (mach_host_self (), &stop_time);
  if (err)
    perror (err, "host_get_time");

  printf ("start: %d.%06d\n", start_time.seconds, start_time.microseconds);
  printf ("stop: %d.%06d\n", stop_time.seconds, stop_time.microseconds);
}

/*
  Test a specific message type on tx, rx and rx-continue paths
  We need to be able to create a thread for this, so some rpc must work.
*/
void
run_test_simple(void *msg, mach_msg_size_t msglen, mach_msg_id_t msgid)
{
  mach_msg_header_t *head = msg;
  mach_port_t port, receive;
  int err;

  err = syscall_mach_port_allocate (mach_task_self (),
                                    MACH_PORT_RIGHT_RECEIVE, &port);
  if (err)
    perror (err, "syscall_mach_port_allocate");

  err = syscall_mach_port_allocate (mach_task_self (),
                                    MACH_PORT_RIGHT_RECEIVE, &receive);
  if (err)
    perror (err, "syscall_mach_port_allocate 2");

  struct echo_params params;
  params.rx_port = port;
  params.rx_size = msglen;
  params.rx_number = 1;
  test_thread_start (mach_task_self (), echo_thread, &params);
  msleep(100);

  head->msgh_bits
    = MACH_MSGH_BITS (MACH_MSG_TYPE_MAKE_SEND,
                      MACH_MSG_TYPE_MAKE_SEND_ONCE);
  head->msgh_remote_port = port;
  head->msgh_local_port = receive;
  head->msgh_id = msgid;
  head->msgh_size = msglen;

  err = mach_msg (msg,
                  MACH_SEND_MSG | MACH_RCV_MSG,
                  msglen,
                  msglen,
                  receive,
                  MACH_MSG_TIMEOUT_NONE,
                  MACH_PORT_NULL);
  if (err)
    perror (err, "mach_msg txrx");

  if (head->msgh_size != msglen)
    printf ("error: wrong size in final rx: %d expected %d\n",
            head->msgh_size, msglen);
}

void
run_test_simple_self(void *msg, mach_msg_size_t msglen, mach_msg_id_t msgid)
{
  mach_msg_header_t *head = msg;
  mach_port_t port, receive;
  int err;

  err = syscall_mach_port_allocate (mach_task_self (),
                                    MACH_PORT_RIGHT_RECEIVE, &port);
  if (err)
    perror (err, "syscall_mach_port_allocate");

  head->msgh_bits
    = MACH_MSGH_BITS (MACH_MSG_TYPE_MAKE_SEND,
                      MACH_MSG_TYPE_MAKE_SEND_ONCE);
  /* head->msgh_bits */
  /*   = MACH_MSGH_BITS (MACH_MSG_TYPE_MAKE_SEND_ONCE, */
  /*                     MACH_MSG_TYPE_COPY_SEND); */

  head->msgh_bits |= MACH_MSGH_BITS_COMPLEX;
  head->msgh_remote_port = port;
  head->msgh_local_port = port;
  head->msgh_id = msgid;
  head->msgh_size = msglen;

  err = mach_msg (msg,
                  MACH_SEND_MSG | MACH_RCV_MSG,
                  msglen,
                  msglen,
                  port,
                  MACH_MSG_TIMEOUT_NONE,
                  MACH_PORT_NULL);
  if (err)
    perror (err, "mach_msg txrx");

  if (head->msgh_size != msglen)
    printf ("error: wrong size in final rx: %d expected %d\n",
            head->msgh_size, msglen);
}


void test_msg_string(void)
{
  struct message
  {
    mach_msg_header_t header;
    mach_msg_type_t type;
    char data[64];
  } msg;
  char *test_strings[] = {"123", "12345", "ciaociao"};

  memset (&msg, 0, sizeof (struct message));
  strcpy (msg.data, "ciao");
  size_t datalen = strlen (msg.data) + 1;

  int msgid = 123;
  int msglen = sizeof (msg.header) + sizeof (msg.type) + datalen;
  align_inline(msglen, 4);
  msg.type.msgt_name = MACH_MSG_TYPE_STRING;
  msg.type.msgt_size = 8 * datalen;
  msg.type.msgt_number = 1;
  msg.type.msgt_inline = TRUE;
  msg.type.msgt_longform = FALSE;
  msg.type.msgt_deallocate = FALSE;
  msg.type.msgt_unused = 0;

  run_test_simple_self(&msg, msglen, msgid);
  run_test_simple(&msg, msglen, msgid);
}

void test_msg_string2(void)
{
  struct message
  {
    mach_msg_header_t header;
    mach_msg_type_t type;
    char data[10];
    mach_msg_type_t type2;
    char data2[5];
  } msg;

  memset (&msg, 0, sizeof (struct message));
  int msgid = 123;
  int msglen = sizeof (msg.header) + sizeof (msg.type) + 10;
  align_inline(msglen, 4);
  msglen += sizeof (msg.type2) + 5;
  align_inline(msglen, 4);
  msg.type.msgt_name = MACH_MSG_TYPE_STRING;
  msg.type.msgt_size = 8 * 10;
  msg.type.msgt_number = 1;
  msg.type.msgt_inline = TRUE;
  msg.type.msgt_longform = FALSE;
  msg.type.msgt_deallocate = FALSE;
  msg.type.msgt_unused = 0;
  memset (msg.data, 'c', 10);
  msg.type2.msgt_name = MACH_MSG_TYPE_CHAR;
  msg.type2.msgt_size = 8;
  msg.type2.msgt_number = 5;
  msg.type2.msgt_inline = TRUE;
  msg.type2.msgt_longform = FALSE;
  msg.type2.msgt_deallocate = FALSE;
  msg.type2.msgt_unused = 0;
  memset (msg.data2, 'x', 5);

  run_test_simple_self(&msg, msglen, msgid);
  run_test_simple(&msg, msglen, msgid);
}


void test_msg_ports(void)
{
  struct message
  {
    mach_msg_header_t head;
    mach_msg_type_t type;
    mach_port_t *portp;
  } msg;
  mach_port_t msgports[3];

  memset (&msg, 0, sizeof (struct message));

  int msgid = 123;
  int msglen = sizeof (msg.head) + sizeof (msg.type) + sizeof(msg.portp);
  msg.type.msgt_name = MACH_MSG_TYPE_MOVE_SEND;
  msg.type.msgt_size = 8*sizeof(mach_port_t);
  msg.type.msgt_number = 3;
  msg.type.msgt_inline = FALSE;
  msg.type.msgt_longform = FALSE;
  msg.type.msgt_deallocate = FALSE;
  msg.type.msgt_unused = 0;
  msg.portp = msgports;
  msgports[0] = mach_host_self();
  msgports[1] = mach_task_self();
  msgports[2] = mach_thread_self();

  run_test_simple_self(&msg, msglen, msgid);
  run_test_simple(&msg, msglen, msgid);
}

void test_msg_emptydesc(void)
{
  struct message
  {
    mach_msg_header_t header;
    mach_msg_type_t type_empty;
    vm_offset_t addr_empty;
    mach_msg_type_t type;
    char data[64];
  } msg;

  memset (&msg, 0, sizeof (struct message));
  strcpy (msg.data, "ciao");
  size_t datalen = strlen (msg.data) + 1;

  int msgid = 123;
  int msglen = sizeof (msg.header);
  msglen += sizeof (msg.type_empty)+ sizeof (msg.addr_empty);
  msglen += sizeof (msg.type) + datalen;
  align_inline(msglen, 4);
  msg.type_empty.msgt_name = MACH_MSG_TYPE_STRING;
  msg.type_empty.msgt_size = 8;
  msg.type_empty.msgt_number = 0;
  msg.type_empty.msgt_inline = FALSE;
  msg.type_empty.msgt_longform = FALSE;
  msg.type_empty.msgt_deallocate = FALSE;
  msg.type_empty.msgt_unused = 0;
  msg.addr_empty = 0;

  msg.type.msgt_name = MACH_MSG_TYPE_STRING;
  msg.type.msgt_size = 8 * datalen;
  msg.type.msgt_number = 1;
  msg.type.msgt_inline = TRUE;
  msg.type.msgt_longform = FALSE;
  msg.type.msgt_deallocate = FALSE;
  msg.type.msgt_unused = 0;

  run_test_simple_self(&msg, msglen, msgid);
  run_test_simple(&msg, msglen, msgid);
}


int
main (int argc, char *argv[], int envc, char *envp[])
{
  test_msg_string();
  test_msg_string2();
  test_msg_ports();
  test_msg_emptydesc();
  test_iterations();
  return 0;
}
