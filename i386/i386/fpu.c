/* 
 * Mach Operating System
 * Copyright (c) 1992-1990 Carnegie Mellon University
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
 *  Copyright (C) 1994 Linus Torvalds
 *
 *  Pentium III FXSR, SSE support
 *  General FPU state handling cleanups
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 */

/*
 * Support for 80387 floating point or FP emulator.
 */

#include <string.h>

#include <mach/exception.h>
#include <mach/machine/thread_status.h>
#include <mach/machine/fp_reg.h>

#include <kern/debug.h>
#include <machine/spl.h>	/* spls */
#include <kern/printf.h>
#include <kern/thread.h>
#include <kern/slab.h>

#include <i386/thread.h>
#include <i386/fpu.h>
#include <i386/pio.h>
#include <i386/irq.h>
#include <i386/locore.h>
#include <i386/trap.h>
#include "cpu_number.h"

#if 0
#include <i386/ipl.h>
#define ASSERT_IPL(L) \
MACRO_BEGIN \
      if (curr_ipl[cpu_number()] != L) { \
	      printf("IPL is %d, expected %d\n", curr_ipl[cpu_number()], L); \
	      panic("fpu: wrong ipl"); \
      } \
MACRO_END
#else
#define ASSERT_IPL(L)
#endif

_Static_assert(sizeof(struct i386_xfp_xstate_header) == 8*8,
	       "struct i386_xfp_xstate_header size");
_Static_assert(sizeof(struct i386_xfp_save) == 512 + 8*8,
	       "struct i386_xfp_save size");

int			fp_kind = FP_387;	/* 80387 present */
enum fp_save_kind	fp_save_kind = FP_FNSAVE;	/* Which instruction we use to save/restore FPU state */
uint64_t		fp_xsave_support;	/* Bitmap of supported XSAVE save areas */
unsigned		fp_xsave_size = sizeof(struct i386_xfp_save);	/* size saved by XSAVE */
struct i386_fpsave_state *fp_default_state;
struct kmem_cache	ifps_cache;	/* cache for FPU save area */
static unsigned long	mxcsr_feature_mask = 0xffffffff;	/* Always AND user-provided mxcsr with this security mask */

#if	NCPUS == 1
volatile thread_t	fp_thread = THREAD_NULL;
					/* thread whose state is in FPU */
					/* always THREAD_NULL if emulating
					   FPU */
volatile thread_t	fp_intr_thread = THREAD_NULL;


#define	clear_fpu() \
    MACRO_BEGIN \
	set_ts(); \
	fp_thread = THREAD_NULL; \
    MACRO_END

#else	/* NCPUS > 1 */
#define	clear_fpu() \
    MACRO_BEGIN \
	set_ts(); \
    MACRO_END

#endif


/*
 * Look for FPU and initialize it.
 * Called on each CPU.
 */
void
init_fpu(void)
{
	unsigned short	status, control;

#ifdef	MACH_RING1
	clear_ts();
#else	/* MACH_RING1 */
	unsigned int native = 0;

	if (machine_slot[cpu_number()].cpu_type >= CPU_TYPE_I486)
		native = CR0_NE;

	/*
	 * Check for FPU by initializing it,
	 * then trying to read the correct bit patterns from
	 * the control and status registers.
	 */
	set_cr0((get_cr0() & ~(CR0_EM|CR0_TS)) | native);	/* allow use of FPU */
#endif	/* MACH_RING1 */

	fninit();
	status = fnstsw();
	fnstcw(&control);

	if ((status & 0xff) == 0 &&
	    (control & 0x103f) == 0x3f)
	{
	    /*
	     * We have a FPU of some sort.
	     * Compare -infinity against +infinity
	     * to check whether we have a 287 or a 387.
	     */
	    volatile double fp_infinity, fp_one, fp_zero;
	    fp_one = 1.0;
	    fp_zero = 0.0;
	    fp_infinity = fp_one / fp_zero;
	    if (fp_infinity == -fp_infinity) {
		/*
		 * We have an 80287.
		 */
		fp_kind = FP_287;
		fp_save_kind = FP_FNSAVE;
		asm volatile(".byte 0xdb; .byte 0xe4");	/* fnsetpm */
	    }
	    else {
		/*
		 * We have a 387.
		 */
		fp_kind = FP_387;
		fp_save_kind = FP_FNSAVE;

		if (CPU_HAS_FEATURE(CPU_FEATURE_XSAVE)) {
		    unsigned eax, ebx, ecx, edx;
		    unsigned xsave_cpu_features;

		    eax = 0xd;
		    ecx = 0x0;
		    cpuid(eax, ebx, ecx, edx);
		    fp_xsave_support = eax + (((uint64_t) edx) << 32);

#ifndef MACH_RING1
		    set_cr4(get_cr4() | CR4_OSFXSR | CR4_OSXSAVE);
		    set_xcr0(fp_xsave_support);
#endif /* MACH_RING1 */

		    eax = 0xd;
		    ecx = 0x1;
		    cpuid(eax, ebx, ecx, edx);
		    xsave_cpu_features = eax;

		    if (xsave_cpu_features & CPU_FEATURE_XSAVES) {
			// all states enabled by XCR0|IA32_XSS
			fp_xsave_size = ebx;
			if (fp_xsave_size < sizeof(struct i386_xfp_save))
				panic("CPU-provided xstate size %d "
				      "is smaller than our minimum %d!\n",
				      fp_xsave_size,
				      (int) sizeof(struct i386_xfp_save));

			fp_save_kind = FP_XSAVES;
		    } else {
			eax = 0xd;
			ecx = 0x0;
			cpuid(eax, ebx, ecx, edx);
			// all states enabled by XCR0
			fp_xsave_size = ebx;
			if(fp_xsave_size < sizeof(struct i386_xfp_save))
				panic("CPU-provided xstate size %d "
				      "is smaller than our minimum %d!\n",
				      fp_xsave_size,
				      (int) sizeof(struct i386_xfp_save));

			if (xsave_cpu_features & CPU_FEATURE_XSAVEOPT)
			    fp_save_kind = FP_XSAVEOPT;
			else if (xsave_cpu_features & CPU_FEATURE_XSAVEC)
			    fp_save_kind = FP_XSAVEC;
			else
			    fp_save_kind = FP_XSAVE;
		    }

		    fp_kind = FP_387X;
		}

		else if (CPU_HAS_FEATURE(CPU_FEATURE_FXSR)) {
#ifndef MACH_RING1
		    set_cr4(get_cr4() | CR4_OSFXSR);
#endif /* MACH_RING1 */
		    fp_kind = FP_387FX;
		    fp_save_kind = FP_FXSAVE;
		}

		if (fp_save_kind != FP_FNSAVE) {
		    /* Compute mxcsr_feature_mask.  */
		    static /* because we _need_ alignment */
		    struct i386_xfp_save save;
		    unsigned long mask;
		    fxsave(&save);
		    mask = save.fp_mxcsr_mask;
		    if (!mask)
			mask = 0x0000ffbf;
		    mxcsr_feature_mask &= mask;
		}
	    }
#ifdef MACH_RING1
	    set_ts();
#else	/* MACH_RING1 */
	    /*
	     * Trap wait instructions.  Turn off FPU for now.
	     */
	    set_cr0(get_cr0() | CR0_TS | CR0_MP);
#endif	/* MACH_RING1 */
	}
	else {
	    /*
	     * NO FPU.
	     */
	    panic("No FPU!");
	}
}

kern_return_t
i386_get_xstate_size(host_t host, vm_size_t *size)
{
	if (host == HOST_NULL)
		return KERN_INVALID_ARGUMENT;

	*size = sizeof(struct i386_xfloat_state) + fp_xsave_size;

	return KERN_SUCCESS;
}

/*
 * Initialize FP handling.
 */
void
fpu_module_init(void)
{
	kmem_cache_init(&ifps_cache, "i386_fpsave_state",
			offsetof(struct i386_fpsave_state, xfp_save_state) + fp_xsave_size,
			alignof(struct i386_fpsave_state),
			NULL, 0);

	fp_default_state = (struct i386_fpsave_state *) kmem_cache_alloc(&ifps_cache);
	memset(fp_default_state, 0, offsetof(struct i386_fpsave_state, xfp_save_state) + fp_xsave_size);

	/* Get default state from CPU.  */
	clear_ts();
	fninit();
	fpu_save(fp_default_state);
	set_ts();
}

/*
 * Free a FPU save area.
 * Called only when thread terminating - no locking necessary.
 */
void
fp_free(struct i386_fpsave_state *fps)
{
ASSERT_IPL(SPL0);
#if	NCPUS == 1
	if ((fp_thread != THREAD_NULL) && (fp_thread->pcb->ims.ifps == fps)) {
		/* 
		 * Make sure we don't get FPU interrupts later for
		 * this thread
		 */
		clear_ts();
		fwait();

		/* Mark it free and disable access */
	    clear_fpu();
	}
#endif	/* NCPUS == 1 */
	kmem_cache_free(&ifps_cache, (vm_offset_t) fps);
}

/* The two following functions were stolen from Linux's i387.c */
static inline unsigned short
twd_i387_to_fxsr (unsigned short twd)
{
	unsigned int tmp; /* to avoid 16 bit prefixes in the code */

	/* Transform each pair of bits into 01 (valid) or 00 (empty) */
	tmp = ~twd;
	tmp = (tmp | (tmp>>1)) & 0x5555; /* 0V0V0V0V0V0V0V0V */
	/* and move the valid bits to the lower byte. */
	tmp = (tmp | (tmp >> 1)) & 0x3333; /* 00VV00VV00VV00VV */
	tmp = (tmp | (tmp >> 2)) & 0x0f0f; /* 0000VVVV0000VVVV */
	tmp = (tmp | (tmp >> 4)) & 0x00ff; /* 00000000VVVVVVVV */
	return tmp;
}

static inline unsigned long
twd_fxsr_to_i387 (struct i386_xfp_save *fxsave)
{
	struct {
		unsigned short significand[4];
		unsigned short exponent;
		unsigned short padding[3];
	} *st = NULL;
	unsigned long tos = (fxsave->fp_status >> 11) & 7;
	unsigned long twd = (unsigned long) fxsave->fp_tag;
	unsigned long tag;
	unsigned long ret = 0xffff0000u;
	int i;

#define FPREG_ADDR(f, n)	((void *)&(f)->fp_reg_word + (n) * 16);

	for (i = 0 ; i < 8 ; i++) {
		if (twd & 0x1) {
			st = FPREG_ADDR (fxsave, (i - tos) & 7);

			switch (st->exponent & 0x7fff) {
			case 0x7fff:
				tag = 2;		 /* Special */
				break;
			case 0x0000:
				if (!st->significand[0] &&
				    !st->significand[1] &&
				    !st->significand[2] &&
				    !st->significand[3] ) {
					tag = 1;	/* Zero */
				} else {
					tag = 2;	/* Special */
				}
				break;
			 default:
				if (st->significand[3] & 0x8000) {
					tag = 0;	/* Valid */
				} else {
					tag = 2;	/* Special */
				}
				break;
			}
		} else {
			tag = 3;			/* Empty */
		}
		ret |= (tag << (2 * i));
		twd = twd >> 1;
	}
	return ret;
}

/*
 * Set the floating-point state for a thread.
 * If the thread is not the current thread, it is
 * not running (held).  Locking needed against
 * concurrent fpu_set_state or fpu_get_state.
 */
kern_return_t
fpu_set_state(const thread_t thread, void *state, int flavor)
{
	pcb_t pcb = thread->pcb;
	struct i386_float_state *fstate = (struct i386_float_state*)state;
	struct i386_xfloat_state *xfstate = (struct i386_xfloat_state*)state;
	struct i386_fpsave_state *ifps;
	struct i386_fpsave_state *new_ifps;

ASSERT_IPL(SPL0);
	if (fp_kind == FP_NO)
	    return KERN_FAILURE;
	if (flavor == i386_XFLOAT_STATE && xfstate->initialized && xfstate->fp_save_kind != fp_save_kind)
	    return KERN_INVALID_ARGUMENT;

#if	NCPUS == 1

	/*
	 * If this thread`s state is in the FPU,
	 * discard it; we are replacing the entire
	 * FPU state.
	 */
	if (fp_thread == thread) {
	    clear_ts();
	    fwait();			/* wait for possible interrupt */
	    clear_fpu();		/* no state in FPU */
	}
#endif

	if ((flavor == i386_FLOAT_STATE && fstate->initialized == 0) ||
	    (flavor == i386_XFLOAT_STATE && xfstate->initialized == 0)) {
	    /*
	     * new FPU state is 'invalid'.
	     * Deallocate the fp state if it exists.
	     */
	    simple_lock(&pcb->lock);
	    ifps = pcb->ims.ifps;
	    pcb->ims.ifps = 0;
	    simple_unlock(&pcb->lock);

	    if (ifps != 0) {
		kmem_cache_free(&ifps_cache, (vm_offset_t) ifps);
	    }
	}
	else {
	    /*
	     * Valid state.  Allocate the fp state if there is none.
	     */
	    new_ifps = 0;
	Retry:
	    simple_lock(&pcb->lock);
	    ifps = pcb->ims.ifps;
	    if (ifps == 0) {
		if (new_ifps == 0) {
		    simple_unlock(&pcb->lock);
		    new_ifps = (struct i386_fpsave_state *) kmem_cache_alloc(&ifps_cache);
		    goto Retry;
		}
		ifps = new_ifps;
		new_ifps = 0;
		pcb->ims.ifps = ifps;
	    }

	    /*
	     * Ensure that reserved parts of the environment are 0.
	     */
	    memset(ifps, 0, offsetof(struct i386_fpsave_state, xfp_save_state) + fp_xsave_size);
	    ifps->fp_valid = TRUE;

	    if (flavor == i386_FLOAT_STATE) {
		struct i386_fp_save *user_fp_state;
		struct i386_fp_regs *user_fp_regs;

		user_fp_state = (struct i386_fp_save *) &fstate->hw_state[0];
		user_fp_regs  = (struct i386_fp_regs *)
		    &fstate->hw_state[sizeof(struct i386_fp_save)];

		if (fp_save_kind != FP_FNSAVE) {
		    int i;

		    ifps->xfp_save_state.fp_control = user_fp_state->fp_control;
		    ifps->xfp_save_state.fp_status  = user_fp_state->fp_status;
		    ifps->xfp_save_state.fp_tag	    = twd_i387_to_fxsr(user_fp_state->fp_tag);
		    ifps->xfp_save_state.fp_eip	    = user_fp_state->fp_eip;
		    ifps->xfp_save_state.fp_cs	    = user_fp_state->fp_cs;
		    ifps->xfp_save_state.fp_opcode  = user_fp_state->fp_opcode;
		    ifps->xfp_save_state.fp_dp	    = user_fp_state->fp_dp;
		    ifps->xfp_save_state.fp_ds	    = user_fp_state->fp_ds;
		    for (i=0; i<8; i++)
			memcpy(&ifps->xfp_save_state.fp_reg_word[i], &user_fp_regs->fp_reg_word[i], sizeof(user_fp_regs->fp_reg_word[i]));
		} else {
		    ifps->fp_save_state.fp_control = user_fp_state->fp_control;
		    ifps->fp_save_state.fp_status  = user_fp_state->fp_status;
		    ifps->fp_save_state.fp_tag	   = user_fp_state->fp_tag;
		    ifps->fp_save_state.fp_eip	   = user_fp_state->fp_eip;
		    ifps->fp_save_state.fp_cs	   = user_fp_state->fp_cs;
		    ifps->fp_save_state.fp_opcode  = user_fp_state->fp_opcode;
		    ifps->fp_save_state.fp_dp	   = user_fp_state->fp_dp;
		    ifps->fp_save_state.fp_ds	   = user_fp_state->fp_ds;
		    ifps->fp_regs = *user_fp_regs;
		}
	    } else if (flavor == i386_XFLOAT_STATE) {
		int i;
		struct i386_xfp_save *user_fp_state = (struct i386_xfp_save *) &xfstate->hw_state[0];
		ifps->xfp_save_state.fp_control = user_fp_state->fp_control;
		ifps->xfp_save_state.fp_status  = user_fp_state->fp_status;
		ifps->xfp_save_state.fp_tag     = user_fp_state->fp_tag;
		ifps->xfp_save_state.fp_eip     = user_fp_state->fp_eip;
		ifps->xfp_save_state.fp_cs      = user_fp_state->fp_cs;
		ifps->xfp_save_state.fp_opcode  = user_fp_state->fp_opcode;
		ifps->xfp_save_state.fp_dp      = user_fp_state->fp_dp;
		ifps->xfp_save_state.fp_ds      = user_fp_state->fp_ds;
		ifps->xfp_save_state.fp_dp3     = user_fp_state->fp_dp3;
		ifps->xfp_save_state.fp_mxcsr   = user_fp_state->fp_mxcsr & mxcsr_feature_mask;
		ifps->xfp_save_state.fp_mxcsr_mask = user_fp_state->fp_mxcsr_mask & mxcsr_feature_mask;;
		for (i=0; i<8; i++)
                    memcpy(&ifps->xfp_save_state.fp_reg_word[i], &user_fp_state->fp_reg_word[i], sizeof(user_fp_state->fp_reg_word[i]));
		for (i=0; i<16; i++)
                    memcpy(&ifps->xfp_save_state.fp_xreg_word[i], &user_fp_state->fp_xreg_word[i], sizeof(user_fp_state->fp_xreg_word[i]));

		memcpy(&ifps->xfp_save_state.header, &user_fp_state->header,
		       sizeof(ifps->xfp_save_state.header));
		if (fp_xsave_size > sizeof(struct i386_xfp_save)) {
			memcpy(&ifps->xfp_save_state.extended, &user_fp_state->extended,
			       fp_xsave_size - sizeof(struct i386_xfp_save));
		}

	    }

	    simple_unlock(&pcb->lock);
	    if (new_ifps != 0)
		kmem_cache_free(&ifps_cache, (vm_offset_t) new_ifps);
	}

	return KERN_SUCCESS;
}

/*
 * Get the floating-point state for a thread.
 * If the thread is not the current thread, it is
 * not running (held).  Locking needed against
 * concurrent fpu_set_state or fpu_get_state.
 */
kern_return_t
fpu_get_state(const thread_t thread, void *state, int flavor)
{
	pcb_t pcb = thread->pcb;
	struct i386_float_state *fstate = (struct i386_float_state*)state;
	struct i386_xfloat_state *xfstate = (struct i386_xfloat_state*)state;
	struct i386_fpsave_state *ifps;

ASSERT_IPL(SPL0);
	if (fp_kind == FP_NO)
	    return KERN_FAILURE;
	if (flavor != i386_FLOAT_STATE && fp_save_kind == FP_FNSAVE)
	    return KERN_FAILURE;

	simple_lock(&pcb->lock);
	ifps = pcb->ims.ifps;
	if (ifps == 0) {
	    /*
	     * No valid floating-point state.
	     */
	    simple_unlock(&pcb->lock);
            if (flavor == i386_FLOAT_STATE)
                memset(fstate, 0, sizeof(struct i386_float_state));
            else if (flavor == i386_XFLOAT_STATE)
                memset(xfstate, 0, sizeof(struct i386_xfloat_state) + fp_xsave_size);
	    return KERN_SUCCESS;
	}

	/* Make sure we`ve got the latest fp state info */
	/* If the live fpu state belongs to our target */
#if	NCPUS == 1
	if (thread == fp_thread)
#else
	if (thread == current_thread())
#endif
	{
	    clear_ts();
	    fp_save(thread);
	    clear_fpu();
	}

	if (flavor == i386_FLOAT_STATE) {
	    struct i386_fp_save *user_fp_state;
	    struct i386_fp_regs *user_fp_regs;

	    fstate->fpkind = fp_kind;
	    fstate->exc_status = 0;
	    fstate->initialized = ifps->fp_valid;

	    user_fp_state = (struct i386_fp_save *) &fstate->hw_state[0];
	    user_fp_regs  = (struct i386_fp_regs *)
			&fstate->hw_state[sizeof(struct i386_fp_save)];

	    /*
	     * Ensure that reserved parts of the environment are 0.
	     */
	    memset(user_fp_state,  0, sizeof(struct i386_fp_save));

	    if (fp_save_kind != FP_FNSAVE) {
		int i;

		user_fp_state->fp_control = ifps->xfp_save_state.fp_control;
		user_fp_state->fp_status  = ifps->xfp_save_state.fp_status;
		user_fp_state->fp_tag     = twd_fxsr_to_i387(&ifps->xfp_save_state);
		user_fp_state->fp_eip     = ifps->xfp_save_state.fp_eip;
		user_fp_state->fp_cs      = ifps->xfp_save_state.fp_cs;
		user_fp_state->fp_opcode  = ifps->xfp_save_state.fp_opcode;
		user_fp_state->fp_dp      = ifps->xfp_save_state.fp_dp;
		user_fp_state->fp_ds      = ifps->xfp_save_state.fp_ds;
		for (i=0; i<8; i++)
		    memcpy(&user_fp_regs->fp_reg_word[i], &ifps->xfp_save_state.fp_reg_word[i], sizeof(user_fp_regs->fp_reg_word[i]));
	    } else {
		user_fp_state->fp_control = ifps->fp_save_state.fp_control;
		user_fp_state->fp_status  = ifps->fp_save_state.fp_status;
		user_fp_state->fp_tag     = ifps->fp_save_state.fp_tag;
		user_fp_state->fp_eip     = ifps->fp_save_state.fp_eip;
		user_fp_state->fp_cs      = ifps->fp_save_state.fp_cs;
		user_fp_state->fp_opcode  = ifps->fp_save_state.fp_opcode;
		user_fp_state->fp_dp      = ifps->fp_save_state.fp_dp;
		user_fp_state->fp_ds      = ifps->fp_save_state.fp_ds;
		*user_fp_regs = ifps->fp_regs;
	    }
	} else if (flavor == i386_XFLOAT_STATE) {
	    int i;
	    struct i386_xfp_save *user_fp_state;

	    xfstate->fpkind = fp_kind;
	    xfstate->exc_status = 0;
	    xfstate->initialized = ifps->fp_valid;
	    xfstate->fp_save_kind = fp_save_kind;

            user_fp_state = (struct i386_xfp_save *) &xfstate->hw_state[0];
	    memset(user_fp_state, 0, sizeof(struct i386_xfp_save));

	    user_fp_state->fp_control = ifps->xfp_save_state.fp_control;
	    user_fp_state->fp_status  = ifps->xfp_save_state.fp_status;
	    user_fp_state->fp_tag     = ifps->xfp_save_state.fp_tag;
	    user_fp_state->fp_eip     = ifps->xfp_save_state.fp_eip;
	    user_fp_state->fp_cs      = ifps->xfp_save_state.fp_cs;
	    user_fp_state->fp_opcode  = ifps->xfp_save_state.fp_opcode;
	    user_fp_state->fp_dp      = ifps->xfp_save_state.fp_dp;
	    user_fp_state->fp_ds      = ifps->xfp_save_state.fp_ds;
	    user_fp_state->fp_dp3     = ifps->xfp_save_state.fp_dp3;
	    user_fp_state->fp_mxcsr   = ifps->xfp_save_state.fp_mxcsr;
	    user_fp_state->fp_mxcsr_mask = ifps->xfp_save_state.fp_mxcsr_mask;
	    for (i=0; i<8; i++)
		memcpy(&user_fp_state->fp_reg_word[i], &ifps->xfp_save_state.fp_reg_word[i], sizeof(user_fp_state->fp_reg_word[i]));
	    for (i=0; i<16; i++)
		memcpy(&user_fp_state->fp_xreg_word[i], &ifps->xfp_save_state.fp_xreg_word[i], sizeof(user_fp_state->fp_xreg_word[i]));

            memcpy(&user_fp_state->header, &ifps->xfp_save_state.header,
                   sizeof(ifps->xfp_save_state.header));
            if (fp_xsave_size > sizeof(struct i386_xfp_save)) {
                memcpy(&user_fp_state->extended, &ifps->xfp_save_state.extended,
                       fp_xsave_size - sizeof(struct i386_xfp_save));
            }
	}
	simple_unlock(&pcb->lock);

	return KERN_SUCCESS;
}

/*
 * Initialize FPU for an already-running thread.
 */
static void fpinit(thread_t thread)
{
	unsigned short	control;

ASSERT_IPL(SPL0);
	clear_ts();
	fpu_rstor(fp_default_state);

	control = thread->pcb->init_control;
	if (control)
		fldcw(control);
}

/*
 * Inherit FPU state from a parent to a child, if any
 */
void fpinherit(thread_t parent_thread, thread_t thread)
{
	pcb_t pcb = parent_thread->pcb;
	struct i386_fpsave_state *ifps;

	ifps = pcb->ims.ifps;
	if (ifps) {
		/* Parent does have a state, inherit it */
		if (ifps->fp_valid == TRUE)
			thread->pcb->init_control = ifps->fp_save_state.fp_control;
		else
			/* State is in the FPU, fetch from there */
			fnstcw(&thread->pcb->init_control);
	}
}

/*
 * Coprocessor not present.
 */
void
fpnoextflt(void)
{
	/*
	 * Enable FPU use.
	 */
ASSERT_IPL(SPL0);
	clear_ts();
#if	NCPUS == 1

	/*
	 * If this thread`s state is in the FPU, we are done.
	 */
	if (fp_thread == current_thread())
	    return;

	/* Make sure we don't do fpsave() in fp_intr while doing fpsave()
	 * here if the current fpu instruction generates an error.
	 */
	fwait();
	/*
	 * If another thread`s state is in the FPU, save it.
	 */
	if (fp_thread != THREAD_NULL) {
	    fp_save(fp_thread);
	}

	/*
	 * Give this thread the FPU.
	 */
	fp_thread = current_thread();

#endif	/* NCPUS == 1 */

	/*
	 * Load this thread`s state into the FPU.
	 */
	fp_load(current_thread());
}

/*
 * FPU overran end of segment.
 * Re-initialize FPU.  Floating point state is not valid.
 */
void
fpextovrflt(void)
{
	thread_t	thread = current_thread();
	pcb_t		pcb;
	struct i386_fpsave_state *ifps;

#if	NCPUS == 1

	/*
	 * Is exception for the currently running thread?
	 */
	if (fp_thread != thread) {
	    /* Uh oh... */
	    panic("fpextovrflt");
	}
#endif

	/*
	 * This is a non-recoverable error.
	 * Invalidate the thread`s FPU state.
	 */
	pcb = thread->pcb;
	simple_lock(&pcb->lock);
	ifps = pcb->ims.ifps;
	pcb->ims.ifps = 0;
	simple_unlock(&pcb->lock);

	/*
	 * Re-initialize the FPU.
	 */
	clear_ts();
	fninit();

	/*
	 * And disable access.
	 */
	clear_fpu();

	if (ifps)
	    kmem_cache_free(&ifps_cache, (vm_offset_t) ifps);

	/*
	 * Raise exception.
	 */
	i386_exception(EXC_BAD_ACCESS, VM_PROT_READ|VM_PROT_EXECUTE, 0);
	/*NOTREACHED*/
}

static int
fphandleerr(void)
{
	thread_t	thread = current_thread();

	/*
	 * Save the FPU context to the thread using it.
	 */
#if	NCPUS == 1
	if (fp_thread == THREAD_NULL) {
		printf("fphandleerr: FPU not belonging to anyone!\n");
		clear_ts();
		fninit();
		clear_fpu();
		return 1;
	}

	if (fp_thread != thread) {
	    /*
	     * FPU exception is for a different thread.
	     * When that thread again uses the FPU an exception will be
	     * raised in fp_load. Remember the condition in fp_valid (== 2).
	     */
	    clear_ts();
	    fp_save(fp_thread);
	    fp_thread->pcb->ims.ifps->fp_valid = 2;
	    fninit();
	    clear_fpu();
	    /* leave fp_intr_thread THREAD_NULL */
	    return 1;
	}
#endif	/* NCPUS == 1 */

	/*
	 * Save the FPU state and turn off the FPU.
	 */
	clear_ts();
	fp_save(thread);
	fninit();
	clear_fpu();

	return 0;
}

/*
 * FPU error. Called by exception handler.
 */
void
fpexterrflt(void)
{
	thread_t	thread = current_thread();

	if (fphandleerr())
		return;

	/*
	 * Raise FPU exception.
	 * Locking not needed on pcb->ims.ifps,
	 * since thread is running.
	 */
	i386_exception(EXC_ARITHMETIC,
		       EXC_I386_EXTERR,
		       fp_save_kind != FP_FNSAVE ?
		           thread->pcb->ims.ifps->xfp_save_state.fp_status :
		           thread->pcb->ims.ifps->fp_save_state.fp_status);
	/*NOTREACHED*/
}

#ifndef MACH_RING1
/*
 * FPU error. Called by AST.
 */
void
fpastintr(void)
{
	thread_t	thread = current_thread();

ASSERT_IPL(SPL0);
#if	NCPUS == 1
	/*
	 * Since FPU errors only occur on ESC or WAIT instructions,
	 * the current thread should own the FPU.  If it didn`t,
	 * we should have gotten the task-switched interrupt first.
	 */
	if (fp_thread != THREAD_NULL) {
	    panic("fpexterrflt");
		return;
	}

	/*
	 * Check if we got a context switch between the interrupt and the AST
	 * This can happen if the interrupt arrived after the FPU AST was
	 * checked. In this case, raise the exception in fp_load when this
	 * thread next time uses the FPU. Remember exception condition in
	 * fp_valid (extended boolean 2).
	 */
	if (fp_intr_thread != thread) {
		if (fp_intr_thread == THREAD_NULL) {
			panic("fpexterrflt: fp_intr_thread == THREAD_NULL");
			return;
		}
		fp_intr_thread->pcb->ims.ifps->fp_valid = 2;
		fp_intr_thread = THREAD_NULL;
		return;
	}
	fp_intr_thread = THREAD_NULL;
#else	/* NCPUS == 1 */
	/*
	 * Save the FPU state and turn off the FPU.
	 */
	fp_save(thread);
#endif	/* NCPUS == 1 */

	/*
	 * Raise FPU exception.
	 * Locking not needed on pcb->ims.ifps,
	 * since thread is running.
	 */
	i386_exception(EXC_ARITHMETIC,
		       EXC_I386_EXTERR,
		       fp_save_kind != FP_FNSAVE ?
		           thread->pcb->ims.ifps->xfp_save_state.fp_status :
		           thread->pcb->ims.ifps->fp_save_state.fp_status);
	/*NOTREACHED*/
}
#endif /* MACH_RING1 */

/*
 * Save FPU state.
 *
 * Locking not needed:
 * .	if called from fpu_get_state, pcb already locked.
 * .	if called from fpnoextflt or fp_intr, we are single-cpu
 * .	otherwise, thread is running.
 */
void
fp_save(thread_t thread)
{
	pcb_t pcb = thread->pcb;
	struct i386_fpsave_state *ifps = pcb->ims.ifps;

	if (ifps != 0 && !ifps->fp_valid)
	    /* registers are in FPU */
	    fpu_save(ifps);
}

/*
 * Restore FPU state from PCB.
 *
 * Locking not needed; always called on the current thread.
 */
void
fp_load(thread_t thread)
{
	pcb_t pcb = thread->pcb;
	struct i386_fpsave_state *ifps;

ASSERT_IPL(SPL0);
	ifps = pcb->ims.ifps;
	if (ifps == 0) {
	    ifps = (struct i386_fpsave_state *) kmem_cache_alloc(&ifps_cache);
	    memcpy(ifps, fp_default_state, offsetof(struct i386_fpsave_state, xfp_save_state) + fp_xsave_size);
	    pcb->ims.ifps = ifps;
	    fpinit(thread);
#if 1
/* 
 * I'm not sure this is needed. Does the fpu regenerate the interrupt in
 * frstor or not? Without this code we may miss some exceptions, with it
 * we might send too many exceptions.
 */
	} else if (ifps->fp_valid == 2) {
		/* delayed exception pending */

		ifps->fp_valid = TRUE;
		clear_fpu();
		/*
		 * Raise FPU exception.
		 * Locking not needed on pcb->ims.ifps,
		 * since thread is running.
		 */
		i386_exception(EXC_ARITHMETIC,
			       EXC_I386_EXTERR,
			       fp_save_kind != FP_FNSAVE ?
			           thread->pcb->ims.ifps->xfp_save_state.fp_status :
			           thread->pcb->ims.ifps->fp_save_state.fp_status);
		/*NOTREACHED*/
#endif
	} else if (! ifps->fp_valid) {
		printf("fp_load: invalid FPU state!\n");
		fninit ();
	} else {
		fpu_rstor(ifps);
	}
	ifps->fp_valid = FALSE;		/* in FPU */
}

#if	(defined(AT386) || defined(ATX86_64)) && !defined(MACH_XEN)
/*
 *	Handle a coprocessor error interrupt on the AT386.
 *	This comes in on line 5 of the slave PIC at SPL1.
 */
void
fpintr(int unit)
{
	spl_t	s;
#if	NCPUS == 1
	thread_t thread = current_thread();
#endif	/* NCPUS == 1 */

ASSERT_IPL(SPL1);
	/*
	 * Turn off the extended 'busy' line.
	 */
	outb(0xf0, 0);

	if (fphandleerr())
		return;

#if	NCPUS == 1
	if (fp_intr_thread != THREAD_NULL && fp_intr_thread != thread)
	    panic("fp_intr: already caught intr");
	fp_intr_thread = thread;
#endif	/* NCPUS == 1 */

	/*
	 * Since we are running on the interrupt stack, we must
	 * signal the thread to take the exception when we return
	 * to user mode.  Use an AST to do this.
	 *
	 * Don`t set the thread`s AST field.  If the thread is
	 * descheduled before it takes the AST, it will notice
	 * the FPU error when it reloads its FPU state.
	 */
	s = splsched();
	ast_on(cpu_number(), AST_I386_FP);
	splx(s);
}
#endif	/* AT386 */
