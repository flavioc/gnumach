/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	File:	vm_param.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1985
 *
 *	I386 machine dependent virtual memory parameters.
 *	Most of the declarations are preceded by I386_ (or i386_)
 *	which is OK because only I386 specific code will be using
 *	them.
 */

#ifndef	_MACH_I386_VM_PARAM_H_
#define _MACH_I386_VM_PARAM_H_

#include <mach/machine/vm_types.h>

#define BYTE_SIZE	8	/* byte size in bits */

#define I386_PGBYTES	4096	/* bytes per 80386 page */
#define I386_PGSHIFT	12	/* number of bits to shift for pages */

/* Virtual page size is the same as real page size - 4K is big enough.  */
#define PAGE_SHIFT	I386_PGSHIFT

/*
 *	Convert bytes to pages and convert pages to bytes.
 *	No rounding is used.
 */

#define i386_btop(x)		(((phys_addr_t)(x)) >> I386_PGSHIFT)
#define i386_ptob(x)		(((phys_addr_t)(x)) << I386_PGSHIFT)

/*
 *	Round off or truncate to the nearest page.  These will work
 *	for either addresses or counts.  (i.e. 1 byte rounds to 1 page
 *	bytes.)
 */

#define i386_round_page(x)	((((phys_addr_t)(x)) + I386_PGBYTES - 1) & \
					~(I386_PGBYTES-1))
#define i386_trunc_page(x)	(((phys_addr_t)(x)) & ~(I386_PGBYTES-1))

/* User address spaces are 3GB each on a 32-bit kernel, starting at
   virtual and linear address 0.
   On a 64-bit kernel we split the address space in half, with the
   lower 128TB for the user address space and the upper 128TB for the
   kernel address space.

   On a 32-bit kernel VM_MAX_ADDRESS can be reduced to leave more
   space for the kernel.
   */
#define VM_MIN_ADDRESS		(0ULL)

#ifdef __x86_64__
#if defined(KERNEL) && defined(USER32)
#define VM_MAX_ADDRESS		(0xfffff000ULL)
#else /* defined(KERNEL) && defined(USER32) */
#define VM_MAX_ADDRESS		(0x800000000000ULL)
#endif /* defined(KERNEL) && defined(USER32) */
#else /* __x86_64__ */
#define VM_MAX_ADDRESS		(0xc0000000UL)
#endif /* __x86_64__ */

#endif	/* _MACH_I386_VM_PARAM_H_ */
