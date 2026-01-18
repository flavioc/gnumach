/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
 *	Machine-dependent definitions for cpu identification.
 *
 */
#ifndef	_I386_CPU_NUMBER_H_
#define	_I386_CPU_NUMBER_H_

#if	NCPUS > 1

#define MY(stm)		%gs:PERCPU_##stm

#ifdef __i386__
#define	CX(addr, reg)	addr(,reg,4)
#endif
#ifdef __x86_64__
#define	CX(addr, reg)	addr(,reg,8)
#endif

/* Slow version, always works. */
#ifdef __i386__
#define	CPU_NUMBER_NO_STACK(reg)	\
	movl	%cs:lapic, reg		;\
	movl	%cs:APIC_ID(reg), reg	;\
	shrl	$24, reg		;\
	andl	%cs:apic_id_mask, reg	;\
	movl	%cs:CX(cpu_id_lut, reg), reg
#endif
#ifdef __x86_64__
/* Specify 8/32/64 bit variants of clob and 32/64 variants of reg */
#define	CPU_NUMBER_NO_STACK(clob8, clob32, clob64, reg32, reg64)\
	movq	%cs:lapic, clob64	;\
	movl	%cs:APIC_ID(clob64), clob32	;\
	shrl	$24, clob32		;\
	andb	%cs:apic_id_mask, clob8	;\
	leaq	cpu_id_lut(%rip), reg64	;\
	movl	%cs:(reg64, clob64, 8), reg32
#endif

/* Fast version, requires a stack */
#ifdef __i386__
/* Never call CPU_NUMBER_NO_GS(%esi) */
#define CPU_NUMBER_NO_GS(reg)		\
	pushl	%esi		;\
	pushl	%eax		;\
	pushl	%ebx		;\
	pushl	%ecx		;\
	pushl	%edx		;\
	movl	$1, %eax	;\
	cpuid			;\
	shrl	$24, %ebx	;\
	andl	%cs:apic_id_mask, %ebx	;\
	movl	%cs:CX(cpu_id_lut, %ebx), %esi	;\
	popl	%edx		;\
	popl	%ecx		;\
	popl	%ebx		;\
	popl	%eax		;\
	movl	%esi, reg	;\
	popl	%esi
#endif
#ifdef __x86_64__
/* Never call CPU_NUMBER_NO_GS(%rsi) */
#define CPU_NUMBER_NO_GS(reg)		\
	pushq	%rsi		;\
	pushq	%rax		;\
	pushq	%rbx		;\
	pushq	%rcx		;\
	pushq	%rdx		;\
	movl	$1, %eax	;\
	cpuid			;\
	shrl	$24, %ebx	;\
	andb	%cs:apic_id_mask, %bl	;\
	leaq	cpu_id_lut(%rip), %rsi	;\
	movl	%cs:(%rsi, %rbx, 4), %esi ;\
	popq	%rdx		;\
	popq	%rcx		;\
	popq	%rbx		;\
	popq	%rax		;\
	movq	%rsi, reg	;\
	popq	%rsi
#endif

/* Fastest version, requires gs being set up */
#define CPU_NUMBER(reg)	\
	movl    MY(CPU_ID), reg;

#ifndef __ASSEMBLER__
#include <kern/cpu_number.h>
#include <i386/apic.h>
#include <i386/percpu.h>

static inline int cpu_number_slow(void)
{
	return cpu_id_lut[apic_get_current_cpu()];
}

static inline int cpu_number(void)
{
	return percpu_get(int, cpu_id);
}
#endif

#else	/* NCPUS == 1 */

#define MY(stm)		(percpu_array + PERCPU_##stm)

#ifdef __x86_64__
#define	CPU_NUMBER_NO_STACK(clob8, clob32, clob64, reg32, reg64) \
	xorl	reg32, reg32
#endif
#ifdef __i386__
#define CPU_NUMBER_NO_STACK(reg) \
	xorl	reg, reg
#endif
#define	CPU_NUMBER_NO_GS(reg) \
	xor	reg, reg
#define	CPU_NUMBER(reg) \
	xor	reg, reg
#define	CX(addr,reg)	addr

#endif	/* NCPUS == 1 */

#endif	/* _I386_CPU_NUMBER_H_ */
