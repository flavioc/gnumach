/*
 * Mach Operating System
 * Copyright (c) 1994-1988 Carnegie Mellon University.
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
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
 *	File:	mach_clock.c
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1986
 *
 *	Clock primitives.
 */

#include <string.h>

#include <mach/boolean.h>
#include <mach/machine.h>
#include <mach/time_value.h>
#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <kern/counters.h>
#include "cpu_number.h"
#include <kern/debug.h>
#include <kern/host.h>
#include <kern/lock.h>
#include <kern/mach_clock.h>
#include <kern/mach_host.server.h>
#include <kern/processor.h>
#include <kern/queue.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>
#include <kern/thread.h>
#include <kern/timer.h>
#include <kern/priority.h>
#include <vm/vm_kern.h>
#include <machine/mach_param.h>	/* HZ */
#include <machine/spl.h>
#include <machine/model_dep.h>

#if MACH_PCSAMPLE
#include <kern/pc_sample.h>
#endif

#define MICROSECONDS_IN_ONE_SECOND 1000000

/* See Costello & Varghese 1995: https://doi.org/10.7936/K7VM49H5 */

#define TIMEOUT_WHEEL_BITS 8
#define TIMEOUT_WHEEL_SIZE (1 << TIMEOUT_WHEEL_BITS)
#define TIMEOUT_WHEEL_MASK (TIMEOUT_WHEEL_SIZE - 1)
#define TIMEOUT_WHEEL_QUEUE(ticks) (&timeoutwheel[ticks & TIMEOUT_WHEEL_MASK])
#define MAX_SOFTCLOCK_STEPS 100

int		hz = HZ;		/* number of ticks per second */
int		tick = (MICROSECONDS_IN_ONE_SECOND / HZ);	/* number of usec per tick */
time_value64_t	time = { 0, 0 };	/* wallclock time (unadjusted) */
time_value64_t	uptime = { 0, 0 };	/* time since bootup */
unsigned long	elapsed_ticks = 0;	/* ticks elapsed since bootup */
unsigned long	softticks = 0;		/* last tick we have checked for timers */

def_simple_lock_irq_data(static,	timeout_lock)	/* lock for accessing static list of timeouts */
def_simple_lock_irq_data(static,	twheel_lock)	/* lock for accessing timeoutwheel of timeouts */

/* Timeouts are now very fast to look up - dividing the complexity by up to
 * TIMEOUT_WHEEL_SIZE, at the expense of TIMEOUT_WHEEL_SIZE times more storage.
 *
 * This is achieved by storing the elements by index into a circular array
 * as the time to expiry modulo the wheel size.  Each spoke of the wheel
 * stores a short queue containing the elements that match the expiry time.
 * Each element per queue is inserted at the end, so not necessarily sorted by expiry. */
queue_head_t	timeoutwheel[TIMEOUT_WHEEL_SIZE]; /* timeoutwheel of timeout lists */
static timeout_t nextsoftcheck;		/* next timeout to be checked */

int		timedelta = 0;
int		tickdelta = 0;

#if	HZ > 500
unsigned	tickadj = 1;		/* can adjust HZ usecs per second */
#else
unsigned	tickadj = 500 / HZ;	/* can adjust 100 usecs per second */
#endif
unsigned	bigadj = 1000000;	/* adjust 10*tickadj if adjustment
					   > bigadj */
#define NTIMERS	20
timeout_data_t	timeout_timers[NTIMERS] = {0};

/* A high-precision (hardware) clock is taken into account to increase the
 * accuracy of the functions used for getting time (e.g. host_get_time64()).
 * The counter of the clock is read once in every clock interrupt.  When any
 * of the functions used for getting time is called, the counter is read again
 * and the difference between these two read is multiplied by the counter
 * period and added to the read value from time or uptime to get a more
 * accurate time read.  */
#if NCPUS > 1
#warning This needs fixing
#endif
uint32_t	last_hpc_read = 0;

/*
 *	This update protocol, with a check value, allows
 *		do {
 *			secs = mtime->seconds;
 *			__sync_synchronize();
 *			usecs = mtime->microseconds;
 *			__sync_synchronize();
 *		} while (secs != mtime->check_seconds);
 *	to read the time correctly.
 */

volatile mapped_time_value_t *mtime = 0;

#define update_mapped_time(time)					\
MACRO_BEGIN								\
	if (mtime != 0) {						\
		mtime->check_seconds = (time)->seconds;			\
		mtime->check_seconds64 = (time)->seconds;		\
		__sync_synchronize();					\
		mtime->microseconds = (time)->nanoseconds / 1000;	\
		mtime->time_value.nanoseconds = (time)->nanoseconds;		\
		__sync_synchronize();					\
		mtime->seconds = (time)->seconds;			\
		mtime->time_value.seconds = (time)->seconds;			\
	}								\
MACRO_END

#define update_mapped_uptime(uptime)					\
MACRO_BEGIN								\
	if (mtime != 0) {						\
		mtime->check_upseconds64 = (uptime)->seconds;		\
		__sync_synchronize();					\
		mtime->uptime_value.nanoseconds = (uptime)->nanoseconds;\
		__sync_synchronize();					\
		mtime->uptime_value.seconds = (uptime)->seconds;	\
	}								\
MACRO_END

#define read_mapped_time(time)						\
MACRO_BEGIN								\
	uint32_t _last_hpc;						\
	do {								\
		_last_hpc = last_hpc_read;				\
		(time)->seconds = mtime->time_value.seconds;		\
		__sync_synchronize();					\
		(time)->nanoseconds = mtime->time_value.nanoseconds;	\
		__sync_synchronize();					\
	} while (((time)->seconds != mtime->check_seconds64)		\
	      || (_last_hpc != last_hpc_read));				\
	time_value64_add_hpc(time, _last_hpc);				\
MACRO_END

#define read_mapped_uptime(uptime)					\
MACRO_BEGIN								\
	uint32_t _last_hpc;						\
	do {								\
		_last_hpc = last_hpc_read;				\
		(uptime)->seconds = mtime->uptime_value.seconds;	\
		__sync_synchronize();					\
		(uptime)->nanoseconds = mtime->uptime_value.nanoseconds;\
		__sync_synchronize();					\
	} while (((uptime)->seconds != mtime->check_upseconds64)	\
	      || (_last_hpc != last_hpc_read));				\
	time_value64_add_hpc(uptime, _last_hpc);			\
MACRO_END


/*
 *	Handle clock interrupts.
 *
 *	The clock interrupt is assumed to be called at a (more or less)
 *	constant rate.  The rate must be identical on all CPUS (XXX - fix).
 *
 *	Usec is the number of microseconds that have elapsed since the
 *	last clock tick.  It may be constant or computed, depending on
 *	the accuracy of the hardware clock.
 *
 */
void clock_interrupt(
	int		usec,		/* microseconds per tick */
	boolean_t	usermode,	/* executing user code */
	boolean_t	basepri,	/* at base priority */
	vm_offset_t	pc)		/* address of interrupted instruction */
{
	int		my_cpu = cpu_number();
	thread_t	thread = current_thread();

	counter(c_clock_ticks++);
	counter(c_threads_total += c_threads_current);
	counter(c_stacks_total += c_stacks_current);

#if	STAT_TIME
	/*
	 *	Increment the thread time, if using
	 *	statistical timing.
	 */
	if (usermode) {
	    timer_bump(&thread->user_timer, usec);
	}
	else {
	    /* Only bump timer if threads are initialized */
	    if (thread)
		timer_bump(&thread->system_timer, usec);
	}
#endif	/* STAT_TIME */

	/*
	 *	Increment the CPU time statistics.
	 */
	{
	    int		state;

	    if (usermode)
		state = CPU_STATE_USER;
	    else if (!cpu_idle(my_cpu))
		state = CPU_STATE_SYSTEM;
	    else
		state = CPU_STATE_IDLE;

	    machine_slot[my_cpu].cpu_ticks[state]++;

	    /*
	     *	Adjust the thread's priority and check for
	     *	quantum expiration.
	     */

	    thread_quantum_update(my_cpu, thread, 1, state);
	}

#if 	MACH_PCSAMPLE
	/*
	 * Take a sample of pc for the user if required.
	 * This had better be MP safe.  It might be interesting
	 * to keep track of cpu in the sample.
	 */
#ifndef MACH_KERNSAMPLE
	if (usermode)
#endif
	{
	    if (thread)
		take_pc_sample_macro(thread, SAMPLED_PC_PERIODIC, usermode, pc);
	}
#endif /* MACH_PCSAMPLE */

	/*
	 *	Time-of-day and time-out list are updated only
	 *	on the master CPU.
	 */
	if (my_cpu == master_cpu) {

	    spl_t s;
	    boolean_t	needsoft = FALSE;


	    /*
	     *	Update the tick count since bootup, and handle
	     *	timeouts.
	     */

	    s = simple_lock_irq(&twheel_lock);

	    elapsed_ticks++;

	    if (!queue_empty(TIMEOUT_WHEEL_QUEUE(elapsed_ticks)))
		needsoft = TRUE;
	    simple_unlock_irq(s, &twheel_lock);

	    /*
	     *	Increment the time-of-day clock.
	     */
	    if (timedelta == 0) {
		time_value64_add_nanos(&time, usec * 1000);
		time_value64_add_nanos(&uptime, usec * 1000);
	    }
	    else {
		int	delta;

		if (timedelta < 0) {
		    if (usec > tickdelta) {
			delta = usec - tickdelta;
			timedelta += tickdelta;
		    } else {
			/* Not enough time has passed, defer overflowing
			 * correction for later, keep only one microsecond
			 * delta */
			delta = 1;
			timedelta += usec - 1;
		    }
		}
		else {
		    delta = usec + tickdelta;
		    timedelta -= tickdelta;
		}
		time_value64_add_nanos(&time, delta * 1000);
		time_value64_add_nanos(&uptime, delta * 1000);
	    }
	    update_mapped_time(&time);
	    update_mapped_uptime(&uptime);

	    /*
	     *	Schedule soft-interrupt for timeout if needed
	     */
	    if (needsoft) {
		if (basepri) {
		    (void) splsoftclock();
		    softclock();
		}
		else {
		    setsoftclock();
		}
	    } else if (softticks + 1 == elapsed_ticks) {
	        /* When there are no timers expiring in this tick
		 * we catch up softticks so it does not fall behind */
		++softticks;
	    }
	}
	last_hpc_read = hpclock_read_counter();
}

/*
 *	There is a nasty race between softclock and reset_timeout.
 *	For example, scheduling code looks at timer_set and calls
 *	reset_timeout, thinking the timer is set.  However, softclock
 *	has already removed the timer but hasn't called thread_timeout
 *	yet.
 *
 *	Interim solution:  We initialize timers after pulling
 *	them out of the queue, so a race with reset_timeout won't
 *	hurt.  The timeout functions (eg, thread_timeout,
 *	thread_depress_timeout) check timer_set/depress_priority
 *	to see if the timer has been cancelled and if so do nothing.
 *
 *	This still isn't correct.  For example, softclock pulls a
 *	timer off the queue, then thread_go resets timer_set (but
 *	reset_timeout does nothing), then thread_set_timeout puts the
 *	timer back on the queue and sets timer_set, then
 *     thread_timeout finally runs and checks it is unset, then
 *	thread_set_timeout tries to put the timer on the queue again
 *	and corrupts it.
 */

void softclock(void)
{
	/*
	 *	Handle timeouts.
	 */
	timeout_t t;
	queue_head_t *spoke;

	/* Number of timeouts in spoke that were checked
	 * for nothing because they have not expired yet */
	int steps;

	unsigned long curticks;
	spl_t	s;

	steps = 0;
	s = simple_lock_irq(&twheel_lock);
	while (softticks != elapsed_ticks) {
		++softticks;
		curticks = softticks;
		spoke = TIMEOUT_WHEEL_QUEUE(curticks);
		t = (timeout_t)queue_next(spoke);
		/* Iterate over all elements within spoke checking that we did not wrap back to a spoke */
		while (! ((t >= (timeout_t)&timeoutwheel[0])
		       && (t < (timeout_t)&timeoutwheel[TIMEOUT_WHEEL_SIZE])) ) {
			if (t->t_time != curticks) {
				t = (timeout_t)queue_next(&t->chain);

				if (++steps >= MAX_SOFTCLOCK_STEPS) {
					nextsoftcheck = t;
					/* Give hardclock() a chance, so that we don't delay the clock interrupts */
					simple_unlock_irq(s, &twheel_lock);
					s = simple_lock_irq(&twheel_lock);
					t = nextsoftcheck;
					steps = 0;
				}
			} else {
				void (*t_fcn)(void *);
				void *t_arg;

				nextsoftcheck = (timeout_t)queue_next(&t->chain);
				queue_remove(spoke, t, timeout_t, chain);
				t->chain.prev = t->chain.next = NULL;
				t_fcn = t->fcn;
				t_arg = t->param;
				if (t->set & TIMEOUT_ALLOC)
					t->set = TIMEOUT_ALLOC;
				else
					t->set &= ~TIMEOUT_PENDING;
				simple_unlock_irq(s, &twheel_lock);
				assert(t_fcn != 0);
				t_fcn(t_arg); /* call function at elapsed */
				s = simple_lock_irq(&twheel_lock);
				steps = 0;
				t = nextsoftcheck;
			}
		}
	}
	nextsoftcheck = NULL;
	simple_unlock_irq(s, &twheel_lock);
}

/*
 *	Set timeout.
 *
 *	Parameters:
 *		t 	 preallocated timeout. Function and param are already set.
 *		interval time-out interval, in ticks.
 */
void set_timeout(
	timeout_t	t,	/* already loaded */
	unsigned int	interval)
{
	spl_t s;

	/* A timeout is allowed to be already set and set again.
	 * This results in a call to reset_timeout first to remove it
	 * from the wheel before adding it again to ensure every timeout only
	 * exists in the wheel exactly once. */
	if (t->set & (TIMEOUT_ACTIVE | TIMEOUT_PENDING))
		reset_timeout(t);

	s = simple_lock_irq(&twheel_lock);

	assert (!(t->set & (TIMEOUT_ACTIVE | TIMEOUT_PENDING)));

	t->set |= TIMEOUT_ACTIVE | TIMEOUT_PENDING;

	/* Start counting after next tick, to avoid partial ticks.  */
	t->t_time = elapsed_ticks + interval + 1;

	/* Insert new timeout at the end of the corresponding wheel entry.  */
	queue_enter(TIMEOUT_WHEEL_QUEUE(t->t_time), t, timeout_t, chain);

	simple_unlock_irq(s, &twheel_lock);
}

boolean_t reset_timeout(timeout_t t)
{
	queue_head_t *spoke;
	spl_t s;

	s = simple_lock_irq(&twheel_lock);
	if (!(t->set & TIMEOUT_PENDING)) {
		t->set &= ~TIMEOUT_ACTIVE;
		simple_unlock_irq(s, &twheel_lock);
		return FALSE;
	}

	t->set &= ~(TIMEOUT_PENDING | TIMEOUT_ACTIVE);

	spoke = TIMEOUT_WHEEL_QUEUE(t->t_time);

	if (nextsoftcheck == t)
		nextsoftcheck = (timeout_t)queue_next(&t->chain);

	assert (!queue_empty(spoke));

	queue_remove(spoke, t, timeout_t, chain);
	t->chain.prev = t->chain.next = NULL;
	simple_unlock_irq(s, &twheel_lock);
	return TRUE;
}

void init_timeout(void)
{
	int i;

	simple_lock_init_irq(&timeout_lock);
	simple_lock_init_irq(&twheel_lock);
	for (i = 0; i < TIMEOUT_WHEEL_SIZE; i++)
		queue_init(&timeoutwheel[i]);
	elapsed_ticks = 0;
	softticks = 0;
}

/*
 * We record timestamps using the boot-time clock.  We keep track of
 * the boot-time clock by storing the difference to the real-time
 * clock.
 */
struct time_value64 clock_boottime_offset;

/*
 * Update the offset of the boot-time clock from the real-time clock.
 * This function must be called when the real-time clock is updated.
 * This function must be called at SPLHIGH.
 */
static void
clock_boottime_update(const struct time_value64 *new_time)
{
	struct time_value64 delta = time;
	time_value64_sub(&delta, new_time);
	time_value64_add(&clock_boottime_offset, &delta);
}

/*
 * Add the time value since last clock interrupt in nanosecond.
 */
static void
time_value64_add_hpc(time_value64_t *value, uint32_t last_hpc)
{
	uint32_t now = hpclock_read_counter();
	/* Time since last clock interrupt in nanosecond.  */
	int64_t ns = (now - last_hpc) * hpclock_get_counter_period_nsec();

	/* Limit the value of ns under the period of a clock interrupt.  */
	if (ns >= tick * 1000)
	    /* Let ns stuck at the end of the clock interrupt period when
	       something bad happens.  */
	    ns = (tick * 1000) - 1;

	time_value64_add_nanos(value, ns);
}


/*
 * Record a timestamp in STAMP.  Records values in the boot-time clock
 * frame.
 */
void
record_time_stamp(time_value64_t *stamp)
{
	read_mapped_time(stamp);
	time_value64_add(stamp, &clock_boottime_offset);
}

/*
 * Read a timestamp in STAMP into RESULT.  Returns values in the
 * real-time clock frame.
 */
void
read_time_stamp (const time_value64_t *stamp, time_value64_t *result)
{
	*result = *stamp;
	time_value64_sub(result, &clock_boottime_offset);
}


/*
 * Read the time (deprecated version).
 */
kern_return_t
host_get_time(const host_t host, time_value_t *current_time)
{
	if (host == HOST_NULL)
		return(KERN_INVALID_HOST);

	time_value64_t current_time64;
	read_mapped_time(&current_time64);
	TIME_VALUE64_TO_TIME_VALUE(&current_time64, current_time);
	return (KERN_SUCCESS);
}

/*
 * Read the time.
 */
kern_return_t
host_get_time64(const host_t host, time_value64_t *current_time)
{
	if (host == HOST_NULL)
		return(KERN_INVALID_HOST);

	read_mapped_time(current_time);
	return (KERN_SUCCESS);
}

/*
 * Set the time.  Only available to privileged users.
 */
kern_return_t
host_set_time(const host_t host, time_value_t new_time)
{
	time_value64_t new_time64;
	TIME_VALUE_TO_TIME_VALUE64(&new_time, &new_time64);
	return host_set_time64(host, new_time64);
}

kern_return_t
host_set_time64(const host_t host, time_value64_t new_time)
{
	spl_t	s;

	if (host == HOST_NULL)
		return(KERN_INVALID_HOST);

#if	NCPUS > 1
	/*
	 * Switch to the master CPU to synchronize correctly.
	 */
	thread_bind(current_thread(), master_processor);
	if (current_processor() != master_processor)
	    thread_block(thread_no_continuation);
#endif	/* NCPUS > 1 */

	s = splhigh();
	clock_boottime_update(&new_time);
	time = new_time;
	update_mapped_time(&time);
	resettodr();
	splx(s);

#if	NCPUS > 1
	/*
	 * Switch off the master CPU.
	 */
	thread_bind(current_thread(), PROCESSOR_NULL);
#endif	/* NCPUS > 1 */

	return(KERN_SUCCESS);
}

/*
 * Adjust the time gradually.
 */
kern_return_t
host_adjust_time(
	const host_t	host,
	time_value_t	new_adjustment,
	time_value_t	*old_adjustment	/* OUT */)
{
	time_value64_t	old_adjustment64;
	time_value64_t new_adjustment64;
	kern_return_t ret;

	TIME_VALUE_TO_TIME_VALUE64(&new_adjustment, &new_adjustment64);
	ret = host_adjust_time64(host, new_adjustment64, &old_adjustment64);
	if (ret == KERN_SUCCESS) {
		TIME_VALUE64_TO_TIME_VALUE(&old_adjustment64, old_adjustment);
	}
	return ret;
}

/*
 * Adjust the time gradually.
 */
kern_return_t
host_adjust_time64(
	const host_t	host,
	time_value64_t	new_adjustment,
	time_value64_t	*old_adjustment	/* OUT */)
{
	time_value64_t	oadj;
	uint64_t ndelta_microseconds;
	spl_t		s;

	if (host == HOST_NULL)
		return (KERN_INVALID_HOST);

	/* Note we only adjust up to microsecond precision */
	ndelta_microseconds = new_adjustment.seconds * MICROSECONDS_IN_ONE_SECOND
		+ new_adjustment.nanoseconds / 1000;

#if	NCPUS > 1
	thread_bind(current_thread(), master_processor);
	if (current_processor() != master_processor)
	    thread_block(thread_no_continuation);
#endif	/* NCPUS > 1 */

	s = splclock();

	oadj.seconds = timedelta / MICROSECONDS_IN_ONE_SECOND;
	oadj.nanoseconds = (timedelta % MICROSECONDS_IN_ONE_SECOND) * 1000;

	if (timedelta == 0) {
	    if (ndelta_microseconds > bigadj)
		tickdelta = 10 * tickadj;
	    else
		tickdelta = tickadj;
	}
	/* Make ndelta_microseconds a multiple of tickdelta */
	if (ndelta_microseconds % tickdelta)
	    ndelta_microseconds = ndelta_microseconds / tickdelta * tickdelta;

	timedelta = ndelta_microseconds;

	splx(s);
#if	NCPUS > 1
	thread_bind(current_thread(), PROCESSOR_NULL);
#endif	/* NCPUS > 1 */

	*old_adjustment = oadj;

	return (KERN_SUCCESS);
}

/*
 * Read the uptime (the elapsed time since boot up).
 */
kern_return_t
host_get_uptime64(const host_t host, time_value64_t *uptime)
{
	if (host == HOST_NULL)
		return (KERN_INVALID_HOST);

	read_mapped_uptime(uptime);
	return (KERN_SUCCESS);
}

void mapable_time_init(void)
{
	if (kmem_alloc_wired(kernel_map, (vm_offset_t *) &mtime, PAGE_SIZE)
						!= KERN_SUCCESS)
		panic("mapable_time_init");
	memset((void *) mtime, 0, PAGE_SIZE);
	update_mapped_time(&time);
	update_mapped_uptime(&uptime);
}

int timeopen(dev_t dev, int flag, io_req_t ior)
{
	return(0);
}
void timeclose(dev_t dev, int flag)
{
	return;
}

/*
 *	Compatibility for device drivers.
 *	New code should use set_timeout/reset_timeout and private timers.
 *	These code can't use a cache to allocate timers, because
 *	it can be called from interrupt handlers.
 */

/*
 *	Set timeout.
 *
 *	fcn:		function to call
 *	param:		parameter to pass to function
 *	interval:	timeout interval, in hz.
 */
timeout_t timeout(
	void	(*fcn)(void *param),
	void *	param,
	int	interval)
{
	timeout_t t;
	int i;
	spl_t s;

	s = simple_lock_irq(&timeout_lock);
	for (i = 0; i < NTIMERS; i++) {
		t = &timeout_timers[i];
		if (!(t->set & TIMEOUT_ACTIVE))
			break;
	}
	if (i == NTIMERS)
		panic("more than NTIMERS timeouts");

	t->set |= TIMEOUT_ALLOC;
	t->fcn = fcn;
	t->param = param;
	simple_unlock_irq(s, &timeout_lock);

	set_timeout(t, interval);

	return t;
}
