
#include <mach/machine/asm.h>
#include <mach/machine/multiboot.h>
#include <i386/apic.h>
#include <i386/seg.h>
#include <i386/i386asm.h>

#define BOOT_CS 0x8
#define BOOT_DS 0x10

#ifdef __ELF__
#define AOUT_KLUDGE	0
#else
#define AOUT_KLUDGE	MULTIBOOT_AOUT_KLUDGE
#endif /* __ELF__ */
#define MULTIBOOT_FLAGS	MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_MODE | AOUT_KLUDGE

	/*
	 * This section will be put first into .text.  See also	i386/ldscript.
	 */
	.section .text.start,"ax"
	
	/* We should never be entered this way.  */
	.globl	start,_start
start:
_start:
	jmp	boot_entry

	/* MultiBoot header - see multiboot.h.  */
	P2ALIGN(2)
boot_hdr:
	.long	MULTIBOOT_MAGIC
	.long	MULTIBOOT_FLAGS
		/*
		 * The next item here is the checksum.
		 * XX this works OK until we need at least the 30th bit.
		 */
	.long	- (MULTIBOOT_MAGIC+MULTIBOOT_FLAGS)
#ifndef __ELF__	/* a.out kludge */
	.long	boot_hdr	/* header_addr */
	.long	_start		/* load_addr */
	.long	_edata		/* load_end_addr */
	.long	_end		/* bss_end_addr */
	.long	boot_entry	/* entry */
#else
        .long	0
        .long	0
        .long	0
        .long	0
        .long	0
#endif /* __ELF__ */
        .long	MULTIBOOT_VIDEO_MODE_TYPE_EGA_TEXT
        .long	80
        .long	25
        .long	MULTIBOOT_VIDEO_PARAM_NO_PREFERENCE

boot_entry:
	movl	$percpu_array - KERNELBASE, %eax
	movw	%ax, boot_percpu_low - KERNELBASE
	shr	$16, %eax
	movb	%al, boot_percpu_med - KERNELBASE
	shr	$8, %ax
	movb	%al, boot_percpu_high - KERNELBASE

	/* use segmentation to offset ourself. */
	lgdt	boot_gdt_descr - KERNELBASE
	ljmp	$BOOT_CS,$0f
0:
	xorw	%ax,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%fs
	movw	%ax,%gs
	movw	$BOOT_DS,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss
	movw	$0x68,%ax
	movw	%ax,%gs

	/* Switch to our own interrupt stack.  */
	movl	$solid_intstack+INTSTACK_SIZE-4, %esp
	andl	$0xfffffff0,%esp

#ifdef APIC
	/* Enable local apic in xAPIC mode */
	xorl	%eax, %eax
	xorl	%edx, %edx
	movl	$APIC_MSR, %ecx
	rdmsr
	orl	$APIC_MSR_ENABLE, %eax
	orl	$APIC_MSR_BSP, %eax
	andl	$(~APIC_MSR_X2APIC), %eax
	movl	$APIC_MSR, %ecx
	wrmsr
#endif

	/* Reset EFLAGS to a known state.  */
	pushl	$0
	popf

	/* Clear uninitialized data.  */
	lea     _edata,%edi
	lea     _end,%ecx
	subl    %edi,%ecx
	xorl    %eax,%eax
	rep
	stosb

	/* Push the boot_info pointer to be the second argument.  */
	pushl	%ebx

	/* Fix ifunc entries */
	movl    $__rel_iplt_start,%esi
	movl    $__rel_iplt_end,%edi
iplt_cont:
	cmpl    %edi,%esi
	jae     iplt_done
	movl    (%esi),%ebx	/* r_offset */
	movb    4(%esi),%al	/* info */
	cmpb    $42,%al		/* IRELATIVE */
	jnz     iplt_next
	call    *(%ebx)		/* call ifunc */
	movl    %eax,(%ebx)	/* fixed address */
iplt_next:
	addl    $8,%esi
	jmp     iplt_cont
iplt_done:

	/* Jump into C code.  */
	call	EXT(c_boot_entry)

.align 16
	.word 0
boot_gdt_descr:
	.word 14*8-1
	.long boot_gdt - KERNELBASE
.align 16
boot_gdt:
	/* 0 */
	.quad 0

	/* boot CS = 0x08 */
	.word 0xffff
	.word (-KERNELBASE) & 0xffff
	.byte ((-KERNELBASE) >> 16) & 0xff
	.byte ACC_PL_K | ACC_CODE_R | ACC_P
	.byte ((SZ_32 | SZ_G) << 4) | 0xf
	.byte ((-KERNELBASE) >> 24) & 0xff

	/* boot DS = 0x10 */
	.word 0xffff
	.word (-KERNELBASE) & 0xffff
	.byte ((-KERNELBASE) >> 16) & 0xff
	.byte ACC_PL_K | ACC_DATA_W | ACC_P
	.byte ((SZ_32 | SZ_G) << 4) | 0xf
	.byte ((-KERNELBASE) >> 24) & 0xff

	/* LDT = 0x18 */
	.quad 0

	/* TSS = 0x20 */
	.quad 0

	/* USER_LDT = 0x28 */
	.quad 0

	/* USER_TSS = 0x30 */
	.quad 0

	/* LINEAR = 0x38 */
	.quad 0

	/* FPREGS = 0x40 */
	.quad 0

	/* USER_GDT = 0x48 and 0x50 */
	.quad 0
	.quad 0

	/* USER_TSS64 = 0x58 */
	.quad 0

	/* USER_TSS64 = 0x60 */
	.quad 0

	/* boot GS = 0x68 */
	.word 0xffff
boot_percpu_low:
	.word 0
boot_percpu_med:
	.byte 0
	.byte ACC_PL_K | ACC_DATA_W | ACC_P
	.byte ((SZ_32 | SZ_G) << 4) | 0xf
boot_percpu_high:
	.byte 0
