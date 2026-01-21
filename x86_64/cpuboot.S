/*
 *  Copyright (C) 2025 Free Software Foundation
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

#if NCPUS > 1
#include <mach/machine/asm.h>
#include <i386/i386asm.h>
#include <i386/proc_reg.h>
#include <i386/apic.h>
#include <i386/cpu_number.h>
#include <i386/seg.h>
#include <i386/msr.h>
#include <i386/gdt.h>

#define RELOC(addr)	(addr - apboot)
#define CR0_CLEAR_FLAGS_CACHE_ENABLE	(CR0_CD | CR0_NW)
#define CR0_SET_FLAGS	(CR0_CLEAR_FLAGS_CACHE_ENABLE | CR0_PE)
#define CR0_CLEAR_FLAGS	(CR0_PG | CR0_AM | CR0_WP | CR0_NE | CR0_TS | CR0_EM | CR0_MP)
#define BOOT_CS		0x8
#define BOOT_DS		0x10

/* Number of 64 bit words */
#define GDT_DESCR_M64	2
#define GDT_TABLE_M64	14

#define SEG_ACCESS_OFS	40
#define SEG_FLAGS_OFS	52

.globl apboot, apbootend, gdt_descr_tmp, apboot_jmp_offset

/* NOTE: apboot to apbootend is copied at runtime to a region above 64k
 * so it can be called as a SIPI vector.  Manual relocations are thus required
 * on instructions that take absolute addresses.  Since we need to patch it,
 * we use the data section to put this block of code.
 */

/* NB: cannot use (%rip) in below relocated code */
.section .boot.text
.align 4096
	.code16
apboot:
	/* This is now address CS:0 in real mode */

	/* Set data seg same as code seg */
	mov	%cs, %dx
	mov	%dx, %ds

	cli
	xorl	%eax, %eax
	movl    %eax, %cr3

	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss


	movl	%cr0, %eax
	andl	$~CR0_CLEAR_FLAGS, %eax
	orl	$CR0_SET_FLAGS, %eax
	movl	%eax, %cr0

	lgdt	RELOC(gdt_descr_tmp)
	ljmpl	*RELOC(apboot_jmp_offset)
apboot32:
	.code32
	xorl	%eax, %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	$BOOT_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss

	/* Protected mode! */

	/*
	 * Prepare minimal page mapping to jump to 64 bit and to C code.
	 * The first 4GB is identity mapped, and the first 2GB are re-mapped
	 * to high addresses at KERNEL_MAP_BASE
	 */

	movl	$AP_p3table,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(AP_p4table)
	/*
	 * Fill 4 entries in L3 table to cover the whole 32-bit 4GB address
	 * space. Part of it might be remapped later if the kernel is mapped
	 * below 4G.
	 */
	movl	$AP_p2table,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(AP_p3table)
	movl	$AP_p2table1,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(AP_p3table + 8)
	movl	$AP_p2table2,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(AP_p3table + 16)
	movl	$AP_p2table3,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(AP_p3table + 24)
	/* point each page table level two entry to a page */
	mov	$0,%ecx
.map_p2_table:
	mov	$0x200000,%eax	 // 2MiB page, should be always available
	mul	%ecx
	or	$(PTE_V|PTE_W|PTE_S),%eax  // enable 2MiB page instead of 4k
	mov	%eax,AP_p2table(,%ecx,8)
	inc	%ecx
	cmp	$2048,%ecx  // 512 entries per table, map 4 L2 tables
	jne	.map_p2_table

	/*
	 * KERNEL_MAP_BASE must me aligned to 2GB.
	 * Depending on kernel starting address, we might need to add another
	 * entry in the L4 table (controlling 512 GB chunks). In any case, we
	 * add two entries in L3 table to make sure we map 2GB for the kernel.
	 * Note that this may override part of the mapping create above.
	 */
.kernel_map:
#if KERNEL_MAP_BASE >= (1U << 39)
	movl	$AP_p3ktable,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(AP_p4table + (8 * ((KERNEL_MAP_BASE >> 39) & 0x1FF)))  // select 512G block
	movl	$AP_p2ktable1,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(AP_p3ktable + (8 * ((KERNEL_MAP_BASE >> 30) & 0x1FF) ))  // select first 1G block
	movl	$AP_p2ktable2,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(AP_p3ktable + (8 * (((KERNEL_MAP_BASE >> 30) & 0x1FF) + 1) ))  // select second 1G block
#else
	movl	$AP_p2ktable1,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(AP_p3table + (8 * ((KERNEL_MAP_BASE >> 30) & 0x1FF) ))  // select first 1G block
	movl	$AP_p2ktable2,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(AP_p3table + (8 * (((KERNEL_MAP_BASE >> 30) & 0x1FF) + 1) ))  // select second 1G block
#endif

	mov	$0,%ecx
.map_p2k_table:
	mov	$0x200000,%eax	 // 2MiB page, should be always available
	mul	%ecx
	or	$(PTE_V|PTE_W|PTE_S),%eax  // enable 2MiB page instead of 4K
	mov	%eax,AP_p2ktable1(,%ecx,8)
	inc	%ecx
	cmp	$1024,%ecx  // 512 entries per table, map 2 L2 tables
	jne	.map_p2k_table

switch64:
	/*
	 * Jump to 64 bit mode, we have to
	 * - enable PAE
	 * - enable long mode
	 * - enable paging and load the tables filled above in CR3
	 * - jump to a 64-bit code segment at a 32-bit address
	 */
	mov	%cr4,%eax
	or	$CR4_PAE,%eax
	mov	%eax,%cr4
	mov	$MSR_REG_EFER,%ecx
	rdmsr
	or	$MSR_REG_EFER_LONG_MODE_EN,%eax
	wrmsr
	mov	$AP_p4table,%eax
	mov	%eax,%cr3
	mov	%cr0,%eax
	or	$CR0_PG,%eax
	or	$CR0_WP,%eax
	mov	%eax,%cr0

	/* Load null Interrupt descriptor table */
	movl	$apboot_idt_ptr, %eax
	lidtl	(%eax)

	lgdtl	apboot_gdt64_descr
	ljmpl	$KERNEL_CS, $1f
1:
	.code64
	xorl	%eax, %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	$KERNEL_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	/* Long mode! */

	/* Get CPU number into rbp */
	movq	$1, %rax
	cpuid
	shrl	$24, %ebx
	andb	%cs:apic_id_mask, %bl
	movq	$cpu_id_lut, %rdi
	movl	%cs:(%rdi, %rbx, 4), %ebp

	/* set up mini stack to do far return */
	movq	$EXT(int_stack_top), %rdi
	movq	(%rdi, %rbp, 8), %rsp

	/* jump to a 64-bit code segment at a 64-bit address */
	pushq   $KERNEL_CS
	pushq   $(start64 + KERNELBASE)
	retfq

start64:
	xorl	%eax, %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	$KERNEL_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss

	/* Get CPU number into rbp */
	movl	$1, %eax
	cpuid
	shrl	$24, %ebx
	andb	%cs:apic_id_mask, %bl
	movq	$cpu_id_lut, %rdi
	movl	%cs:(%rdi, %rbx, 4), %ebp

	/* Access per_cpu area */
	movq	%rbp, %rax
	movq	$PC_SIZE,%rbx
	mul	%rbx
	addq	$percpu_array, %rax

	/* Record our cpu number */
	movl	%ebp, (PERCPU_CPU_ID)(%rax)

	/* Set up GS base */
	movq	%rax, %rdx
	shrq	$32, %rdx
	movl	$MSR_REG_GSBASE, %ecx
	wrmsr

	/* Set up Kernel GS base to 0 */
	xorl	%eax, %eax
	xorl	%edx, %edx
	movl	$MSR_REG_KGSBASE, %ecx
	wrmsr

	/* Enable local apic in xAPIC mode */
	xorl	%eax, %eax
	xorl	%edx, %edx
	movl	$APIC_MSR, %ecx
	rdmsr
	orl	$APIC_MSR_ENABLE, %eax
	andl	$(~(APIC_MSR_BSP | APIC_MSR_X2APIC)), %eax
	movl	$APIC_MSR, %ecx
	wrmsr

	/* Load int_stack_top[cpu] -> esp */
	CPU_NUMBER(%edx)
	movq	$EXT(int_stack_top), %rdi
	movq	(%rdi, %rdx, 8), %rsp

	/* Ensure stack alignment */
	andq	$(~0xf), %rsp

	/* Reset EFLAGS to a known state */
	pushq	$0
	popfq

	/* Finish the cpu configuration */
	call	EXT(cpu_ap_main) - KERNELBASE

3:
	/* NOT REACHED */
	hlt
	jmp	3b

.align 16
    .word 0
gdt_descr_tmp:
    .short 3*8-1
    .long RELOC(gdt_tmp)

.align 16
gdt_tmp:
    /* 0 */
    .quad 0
    /* BOOT_CS */
    .word 0xffff
    .word 0x0000
    .byte 0x00
    .byte ACC_PL_K | ACC_CODE_R | ACC_P
    .byte ((SZ_32 | SZ_G) << 4) | 0xf
    .byte 0x00
    /* BOOT_DS */
    .word 0xffff
    .word 0x0000
    .byte 0x00
    .byte ACC_PL_K | ACC_DATA_W | ACC_P
    .byte ((SZ_32 | SZ_G) << 4) | 0xf
    .byte 0x00

.align 16
apboot_jmp_offset:
	.long RELOC(apboot32)
	.word BOOT_CS

apbootend:

.section .boot.data,"ax",@progbits

.align 4096
AP_p4table:	.space 4096
AP_p3table:	.space 4096
AP_p2table:	.space 4096
AP_p2table1:	.space 4096
AP_p2table2:	.space 4096
AP_p2table3:	.space 4096
AP_p3ktable:	.space 4096
AP_p2ktable1:	.space 4096
AP_p2ktable2:	.space 4096

.align 16
/* Make this long enough to hold 32b null IDT, but
 * also long enough to inherit 64b null IDT
 * so we don't have to reload it */
apboot_idt_ptr:
	.quad	0
	.word	0

.code64
.align 16
apboot_gdt64_top:
	/* 6 bytes of padding */
	.long	0
	.word	0
apboot_gdt64_descr:
	.word	(GDT_TABLE_M64 * 8) - 1
apboot_gdt64_descr_addr:
	.quad	apboot_gdt64
apboot_gdt64:
	/* NULL segment */
	.quad	0

	/* KERNEL_CS */
	.quad	((ACC_P | ACC_CODE_R) << SEG_ACCESS_OFS) | (SZ_64 << SEG_FLAGS_OFS)

	/* KERNEL_DS */
	.quad	((ACC_P | ACC_DATA_W) << SEG_ACCESS_OFS) | (SZ_64 << SEG_FLAGS_OFS)
#endif
