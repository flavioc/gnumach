
#include <syscalls.h>
#include <testlib.h>

typedef mach_port_t exec_startup_t;
typedef mach_port_t *portarray_t;
typedef char *data_t;
typedef int *intarray_t;
kern_return_t exec_startup_get_info
  (exec_startup_t bootstrap,
   vm_address_t * user_entry,
   vm_address_t * phdr_data,
   vm_size_t * phdr_size,
   vm_address_t * stack_base,
   vm_size_t * stack_size,
   int *flags,
   data_t * argv,
   mach_msg_type_number_t * argvCnt,
   data_t * envp,
   mach_msg_type_number_t * envpCnt,
   portarray_t * dtable,
   mach_msg_type_number_t * dtableCnt,
   portarray_t * portarray,
   mach_msg_type_number_t * portarrayCnt,
   intarray_t * intarray, mach_msg_type_number_t * intarrayCnt);

#include <gnumach.user.h>
#include <mach_port.user.h>
//#include <exec_startup.user.h>

int
main (int argc, char *argv[], int envc, char *envp[])
{
  int err;
  mach_port_t in_bootstrap;

  printf ("TEST startup run\n");
  if (err = task_get_special_port (mach_task_self (), TASK_BOOTSTRAP_PORT,
				   &in_bootstrap))
    perror (err, "task_get_special_port");

  if (in_bootstrap != MACH_PORT_NULL)
    {
      // hurd startup data
      int flags;
      mach_port_t *dtable;
      mach_msg_type_number_t dtablesize = 0;
      mach_port_t *portarray;
      mach_msg_type_number_t portarraysize = 0;
      int *intarray;
      mach_msg_type_number_t intarraysize = 0;
      vm_address_t stack_base;
      vm_size_t stack_size;
      vm_address_t phdr;
      vm_size_t phdrsz;
      vm_address_t user_entry;
      // args etc
      char *args, *env;
      mach_msg_type_number_t argslen = 0, envlen = 0;

      err = exec_startup_get_info (in_bootstrap,
				   &user_entry,
				   &phdr, &phdrsz,
				   &stack_base, &stack_size,
				   &flags,
				   &args, &argslen,
				   &env, &envlen,
				   &dtable, &dtablesize,
				   &portarray, &portarraysize,
				   &intarray, &intarraysize);
      mach_port_deallocate (mach_task_self (), in_bootstrap);
      if (err)
	perror (err, "exec_startup_get_info");
      else
	{
	  printf ("EXEC startup data:\n");
	  printf (" user_entry: %p\n", user_entry);
	  printf (" phdr (size): %p (%d)\n", phdr, phdrsz);
	  printf (" stack (size): %p (%d)\n", stack_base, stack_size);
	  printf (" flags: %x\n", flags);
	  printf (" args (size): %p (%d)\n", args, argslen);
	  printf (" env (size): %p (%d)\n", env, envlen);
	  printf (" dtable (size): %p (%d)\n", dtable, dtablesize);
          printf (" portarray (size): %p (%d)\n", portarray, portarraysize);
          printf (" intarray (size): %p (%d)\n", intarray, intarraysize);
	}
    }

  return 0;
}

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif


#define EXPORT_BOOLEAN
#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <mach/mach_types.h>
#include <mach/mig_errors.h>
#include <mach/mig_support.h>
#include <mach/msg_type.h>
#include <stdint.h>

#ifndef	mig_internal
#define	mig_internal	static
#endif

#ifndef	mig_external
#define mig_external
#endif

#ifndef	mig_unlikely
#define	mig_unlikely(X)	__builtin_expect (!! (X), 0)
#endif

#ifndef	TypeCheck
#define	TypeCheck 1
#endif

#ifndef	UseExternRCSId
#define	UseExternRCSId		1
#endif

#define BAD_TYPECHECK(type, check) mig_unlikely (({\
  union { mach_msg_type_t t; uint32_t w; } _t, _c;\
  _t.t = *(type); _c.t = *(check);_t.w != _c.w; }))
#define msgh_request_port	msgh_remote_port
#define msgh_reply_port		msgh_local_port

#include <mach/std_types.h>
#include <mach/mach_types.h>
#include <device/device_types.h>
#include <device/net_status.h>

kern_return_t exec_startup_get_info
  (mach_port_t bootstrap,
   vm_address_t * user_entry,
   vm_address_t * phdr_data,
   vm_size_t * phdr_size,
   vm_address_t * stack_base,
   vm_size_t * stack_size,
   int *flags,
   data_t * argv,
   mach_msg_type_number_t * argvCnt,
   data_t * envp,
   mach_msg_type_number_t * envpCnt,
   portarray_t * dtable,
   mach_msg_type_number_t * dtableCnt,
   portarray_t * portarray,
   mach_msg_type_number_t * portarrayCnt,
   intarray_t * intarray, mach_msg_type_number_t * intarrayCnt)
{
#pragma pack(push,4)
  typedef struct
  {
    mach_msg_header_t Head;
  } Request;
#pragma pack(pop)

#pragma pack(push,4)
  typedef struct
  {
    mach_msg_header_t Head;
    mach_msg_type_t RetCodeType;
    kern_return_t RetCode;
    mach_msg_type_t user_entryType;
    vm_address_t user_entry;
    mach_msg_type_t phdr_dataType;
    vm_address_t phdr_data;
    mach_msg_type_t phdr_sizeType;
    vm_size_t phdr_size;
    mach_msg_type_t stack_baseType;
    vm_address_t stack_base;
    mach_msg_type_t stack_sizeType;
    vm_size_t stack_size;
    mach_msg_type_t flagsType;
    int flags;
    mach_msg_type_long_t argvType;
    union
    {
      char argv[2048];
      char *argvP;
    };
    mach_msg_type_long_t envpType;
    union
    {
      char envp[2048];
      char *envpP;
    };
    mach_msg_type_long_t dtableType;
    union
    {
      mach_port_t dtable[512];
      mach_port_t *dtableP;
    };
    mach_msg_type_long_t portarrayType;
    union
    {
      mach_port_t portarray[512];
      mach_port_t *portarrayP;
    };
    mach_msg_type_long_t intarrayType;
    union
    {
      int intarray[512];
      int *intarrayP;
    };
  } Reply;
#pragma pack(pop)

  union
  {
    Request In;
    Reply Out;
  } Mess;

  Request *InP = &Mess.In;
  Reply *OutP = &Mess.Out;

  mach_msg_return_t msg_result;
#if	TypeCheck
  boolean_t msgh_simple;
#endif /* TypeCheck */
#if	TypeCheck
  unsigned int msgh_size;
#endif /* TypeCheck */
  unsigned int msgh_size_delta;

  const mach_msg_type_t RetCodeCheck = {
    /* msgt_name = */ (unsigned char) MACH_MSG_TYPE_INTEGER_32,
    /* msgt_size = */ 32,
    /* msgt_number = */ 1,
    /* msgt_inline = */ TRUE,
    /* msgt_longform = */ FALSE,
    /* msgt_deallocate = */ FALSE,
    /* msgt_unused = */ 0
  };

  const mach_msg_type_t user_entryCheck = {
    /* msgt_name = */ (unsigned char) MACH_MSG_TYPE_INTEGER_32,
    /* msgt_size = */ 32,
    /* msgt_number = */ 1,
    /* msgt_inline = */ TRUE,
    /* msgt_longform = */ FALSE,
    /* msgt_deallocate = */ FALSE,
    /* msgt_unused = */ 0
  };

  const mach_msg_type_t phdr_dataCheck = {
    /* msgt_name = */ (unsigned char) MACH_MSG_TYPE_INTEGER_32,
    /* msgt_size = */ 32,
    /* msgt_number = */ 1,
    /* msgt_inline = */ TRUE,
    /* msgt_longform = */ FALSE,
    /* msgt_deallocate = */ FALSE,
    /* msgt_unused = */ 0
  };

  const mach_msg_type_t phdr_sizeCheck = {
    /* msgt_name = */ (unsigned char) MACH_MSG_TYPE_INTEGER_32,
    /* msgt_size = */ 32,
    /* msgt_number = */ 1,
    /* msgt_inline = */ TRUE,
    /* msgt_longform = */ FALSE,
    /* msgt_deallocate = */ FALSE,
    /* msgt_unused = */ 0
  };

  const mach_msg_type_t stack_baseCheck = {
    /* msgt_name = */ (unsigned char) MACH_MSG_TYPE_INTEGER_32,
    /* msgt_size = */ 32,
    /* msgt_number = */ 1,
    /* msgt_inline = */ TRUE,
    /* msgt_longform = */ FALSE,
    /* msgt_deallocate = */ FALSE,
    /* msgt_unused = */ 0
  };

  const mach_msg_type_t stack_sizeCheck = {
    /* msgt_name = */ (unsigned char) MACH_MSG_TYPE_INTEGER_32,
    /* msgt_size = */ 32,
    /* msgt_number = */ 1,
    /* msgt_inline = */ TRUE,
    /* msgt_longform = */ FALSE,
    /* msgt_deallocate = */ FALSE,
    /* msgt_unused = */ 0
  };

  const mach_msg_type_t flagsCheck = {
    /* msgt_name = */ (unsigned char) MACH_MSG_TYPE_INTEGER_32,
    /* msgt_size = */ 32,
    /* msgt_number = */ 1,
    /* msgt_inline = */ TRUE,
    /* msgt_longform = */ FALSE,
    /* msgt_deallocate = */ FALSE,
    /* msgt_unused = */ 0
  };

  InP->Head.msgh_bits =
    MACH_MSGH_BITS (MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
  /* msgh_size filled below */
  InP->Head.msgh_request_port = bootstrap;
  InP->Head.msgh_reply_port = mig_get_reply_port ();
  InP->Head.msgh_seqno = 0;
  InP->Head.msgh_id = 30500;

  _Static_assert (sizeof (Request) == 24, "Request expected to be 24 bytes");
  InP->Head.msgh_size = 24;

  msg_result =
    mach_msg (&InP->Head, MACH_SEND_MSG | MACH_RCV_MSG | MACH_MSG_OPTION_NONE,
	      24, sizeof (Reply), InP->Head.msgh_reply_port,
	      MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (msg_result != MACH_MSG_SUCCESS)
    {
      mig_dealloc_reply_port (InP->Head.msgh_reply_port);
      return msg_result;
    }
  mig_put_reply_port (InP->Head.msgh_reply_port);

  printf("msgh_bits %x\n", OutP->Head.msgh_bits);
  printf("msgh_size %d\n", OutP->Head.msgh_size);
  printf("msgh_remote_port %d\n", OutP->Head.msgh_remote_port);
  printf("msgh_local_port %d\n", OutP->Head.msgh_local_port);
  printf("msgh_id %d\n", OutP->Head.msgh_id);

  if (mig_unlikely (OutP->Head.msgh_id != 30600))
    {
      if (OutP->Head.msgh_id == MACH_NOTIFY_SEND_ONCE)
	return MIG_SERVER_DIED;
      else
	{
	  mig_dealloc_reply_port (InP->Head.msgh_reply_port);
	  return MIG_REPLY_MISMATCH;
	}
    }

#if	TypeCheck
  msgh_size = OutP->Head.msgh_size;
  msgh_simple = !(OutP->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX);

  if (mig_unlikely (((msgh_size < 140) || msgh_simple) &&
		    ((msgh_size != sizeof (mig_reply_header_t)) ||
		     !msgh_simple || (OutP->RetCode == KERN_SUCCESS))))
    return MIG_TYPE_ERROR;
#endif /* TypeCheck */

#if	TypeCheck
  if (BAD_TYPECHECK (&OutP->RetCodeType, &RetCodeCheck))
    return MIG_TYPE_ERROR;
#endif /* TypeCheck */

  if (OutP->RetCode != KERN_SUCCESS)
    return OutP->RetCode;

#if	TypeCheck
  if (BAD_TYPECHECK (&OutP->user_entryType, &user_entryCheck))
    return MIG_TYPE_ERROR;
#endif /* TypeCheck */

  *user_entry = OutP->user_entry;

#if	TypeCheck
  if (BAD_TYPECHECK (&OutP->phdr_dataType, &phdr_dataCheck))
    return MIG_TYPE_ERROR;
#endif /* TypeCheck */

  *phdr_data = OutP->phdr_data;

#if	TypeCheck
  if (BAD_TYPECHECK (&OutP->phdr_sizeType, &phdr_sizeCheck))
    return MIG_TYPE_ERROR;
#endif /* TypeCheck */

  *phdr_size = OutP->phdr_size;

#if	TypeCheck
  if (BAD_TYPECHECK (&OutP->stack_baseType, &stack_baseCheck))
    return MIG_TYPE_ERROR;
#endif /* TypeCheck */

  *stack_base = OutP->stack_base;

#if	TypeCheck
  if (BAD_TYPECHECK (&OutP->stack_sizeType, &stack_sizeCheck))
    return MIG_TYPE_ERROR;
#endif /* TypeCheck */

  *stack_size = OutP->stack_size;

#if	TypeCheck
  if (BAD_TYPECHECK (&OutP->flagsType, &flagsCheck))
    return MIG_TYPE_ERROR;
#endif /* TypeCheck */

  *flags = OutP->flags;

#if	TypeCheck
  if (mig_unlikely ((OutP->argvType.msgtl_header.msgt_longform != TRUE) ||
		    (OutP->argvType.msgtl_name != MACH_MSG_TYPE_CHAR) ||
		    (OutP->argvType.msgtl_size != 8)))
    return MIG_TYPE_ERROR;
#endif /* TypeCheck */

  msgh_size_delta =
    (OutP->argvType.msgtl_header.msgt_inline) ? (OutP->argvType.msgtl_number +
						 3) & ~3 : sizeof (char *);
#if	TypeCheck
  if (mig_unlikely (msgh_size < 140 + msgh_size_delta))
    return MIG_TYPE_ERROR;
  msgh_size -= msgh_size_delta;
#endif /* TypeCheck */

  if (!OutP->argvType.msgtl_header.msgt_inline)
    *argv = OutP->argvP;
  else if (OutP->argvType.msgtl_number > *argvCnt)
    {
      mig_allocate ((vm_offset_t *) argv, OutP->argvType.msgtl_number);
      memcpy (*argv, OutP->argv, OutP->argvType.msgtl_number);
    }
  else
    {
      memcpy (*argv, OutP->argv, OutP->argvType.msgtl_number);
    }

  *argvCnt = OutP->argvType.msgtl_number;

  OutP = (Reply *) ((char *) OutP + msgh_size_delta - 2048);

#if	TypeCheck
  if (mig_unlikely ((OutP->envpType.msgtl_header.msgt_longform != TRUE) ||
		    (OutP->envpType.msgtl_name != MACH_MSG_TYPE_CHAR) ||
		    (OutP->envpType.msgtl_size != 8)))
    return MIG_TYPE_ERROR;
#endif /* TypeCheck */

  msgh_size_delta =
    (OutP->envpType.msgtl_header.msgt_inline) ? (OutP->envpType.msgtl_number +
						 3) & ~3 : sizeof (char *);
#if	TypeCheck
  if (mig_unlikely (msgh_size < 140 + msgh_size_delta))
    return MIG_TYPE_ERROR;
  msgh_size -= msgh_size_delta;
#endif /* TypeCheck */

  if (!OutP->envpType.msgtl_header.msgt_inline)
    *envp = OutP->envpP;
  else if (OutP->envpType.msgtl_number > *envpCnt)
    {
      mig_allocate ((vm_offset_t *) envp, OutP->envpType.msgtl_number);
      memcpy (*envp, OutP->envp, OutP->envpType.msgtl_number);
    }
  else
    {
      memcpy (*envp, OutP->envp, OutP->envpType.msgtl_number);
    }

  *envpCnt = OutP->envpType.msgtl_number;

  OutP = (Reply *) ((char *) OutP + msgh_size_delta - 2048);

#if	TypeCheck
  if (mig_unlikely ((OutP->dtableType.msgtl_header.msgt_longform != TRUE) ||
		    (OutP->dtableType.msgtl_name != MACH_MSG_TYPE_PORT_SEND)
		    || (OutP->dtableType.msgtl_size != 32)))
    return MIG_TYPE_ERROR;
#endif /* TypeCheck */

  msgh_size_delta =
    (OutP->dtableType.msgtl_header.msgt_inline) ? 4 *
    OutP->dtableType.msgtl_number : sizeof (mach_port_t *);
#if	TypeCheck
  if (mig_unlikely (msgh_size < 140 + msgh_size_delta))
    return MIG_TYPE_ERROR;
  msgh_size -= msgh_size_delta;
#endif /* TypeCheck */

  if (!OutP->dtableType.msgtl_header.msgt_inline)
    *dtable = OutP->dtableP;
  else if (OutP->dtableType.msgtl_number > *dtableCnt)
    {
      mig_allocate ((vm_offset_t *) dtable,
		    4 * OutP->dtableType.msgtl_number);
      memcpy (*dtable, OutP->dtable, 4 * OutP->dtableType.msgtl_number);
    }
  else
    {
      memcpy (*dtable, OutP->dtable, 4 * OutP->dtableType.msgtl_number);
    }

  *dtableCnt = OutP->dtableType.msgtl_number;

  OutP = (Reply *) ((char *) OutP + msgh_size_delta - 2048);

#if	TypeCheck
  if (mig_unlikely
      ((OutP->portarrayType.msgtl_header.msgt_longform != TRUE)
       || (OutP->portarrayType.msgtl_name != MACH_MSG_TYPE_PORT_SEND)
       || (OutP->portarrayType.msgtl_size != 32)))
    return MIG_TYPE_ERROR;
#endif /* TypeCheck */

  msgh_size_delta =
    (OutP->portarrayType.msgtl_header.msgt_inline) ? 4 *
    OutP->portarrayType.msgtl_number : sizeof (mach_port_t *);
#if	TypeCheck
  if (mig_unlikely (msgh_size < 140 + msgh_size_delta))
    return MIG_TYPE_ERROR;
  msgh_size -= msgh_size_delta;
#endif /* TypeCheck */

  if (!OutP->portarrayType.msgtl_header.msgt_inline)
    *portarray = OutP->portarrayP;
  else if (OutP->portarrayType.msgtl_number > *portarrayCnt)
    {
      mig_allocate ((vm_offset_t *) portarray,
		    4 * OutP->portarrayType.msgtl_number);
      memcpy (*portarray, OutP->portarray,
	      4 * OutP->portarrayType.msgtl_number);
    }
  else
    {
      memcpy (*portarray, OutP->portarray,
	      4 * OutP->portarrayType.msgtl_number);
    }

  *portarrayCnt = OutP->portarrayType.msgtl_number;

  OutP = (Reply *) ((char *) OutP + msgh_size_delta - 2048);

#if	TypeCheck
  if (mig_unlikely ((OutP->intarrayType.msgtl_header.msgt_longform != TRUE) ||
		    (OutP->intarrayType.msgtl_name !=
		     MACH_MSG_TYPE_INTEGER_32)
		    || (OutP->intarrayType.msgtl_size != 32)))
    return MIG_TYPE_ERROR;
#endif /* TypeCheck */

#if	TypeCheck
  if (mig_unlikely
      (msgh_size !=
       140 +
       ((OutP->intarrayType.msgtl_header.msgt_inline) ? 4 *
	OutP->intarrayType.msgtl_number : sizeof (int *))))
    return MIG_TYPE_ERROR;
#endif /* TypeCheck */

  if (!OutP->intarrayType.msgtl_header.msgt_inline)
    *intarray = OutP->intarrayP;
  else if (OutP->intarrayType.msgtl_number > *intarrayCnt)
    {
      mig_allocate ((vm_offset_t *) intarray,
		    4 * OutP->intarrayType.msgtl_number);
      memcpy (*intarray, OutP->intarray, 4 * OutP->intarrayType.msgtl_number);
    }
  else
    {
      memcpy (*intarray, OutP->intarray, 4 * OutP->intarrayType.msgtl_number);
    }

  *intarrayCnt = OutP->intarrayType.msgtl_number;

  return KERN_SUCCESS;
}
