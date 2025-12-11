/*
 * Copyright (C) 2006, 2007 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Barry deFreese and others.
 */

#ifndef _KERN_MACH_CLOCK_H_
#define _KERN_MACH_CLOCK_H_

/*
 * Mach time-out and time-of-day facility.
 */

#include <mach/machine/kern_return.h>
#include <mach/time_value.h>
#include <kern/host.h>
#include <kern/queue.h>
#include <sys/types.h>

struct io_req;
typedef struct io_req *io_req_t;


/* Timers in kernel.  */
extern unsigned long	elapsed_ticks;	/* number of ticks elapsed since bootup */
extern int		hz;		/* number of ticks per second */
extern int		tick;		/* number of usec per tick */

extern time_value64_t	time; 		/* wallclock time (unadjusted) */
extern time_value64_t	uptime;		/* time since bootup */

typedef void timer_func_t(void *);

/* Time-out element.  */
struct timeout {
	queue_chain_t   chain;          /* chain of timeouts, we do not sort by expiry to save time */
	timer_func_t	*fcn;		/* function to call */
	void *		param;		/* with this parameter */
	unsigned long	t_time;		/* expiration time, in ticks elapsed from boot */
	unsigned char	set;		/* active | pending | allocated from pool */
};

typedef	struct timeout	timeout_data_t;
typedef	struct timeout	*timeout_t;

#define	TIMEOUT_ALLOC	0x1		/* timeout allocated from pool */
#define	TIMEOUT_ACTIVE	0x2		/* timeout active */
#define	TIMEOUT_PENDING	0x4		/* timeout waiting for expiry */

extern void clock_interrupt(
   int usec,
   boolean_t usermode,
   boolean_t basepri,
   vm_offset_t pc);

extern void softclock (void);

/* For `private' timer elements.  */
extern void set_timeout(
   timeout_t t,
   unsigned int interval);
extern boolean_t reset_timeout(timeout_t t);

#define	set_timeout_setup(t,fcn,param,interval)		\
	((t)->fcn = (fcn),				\
	 (t)->param = (param),				\
	set_timeout((t), (interval)))

#define	reset_timeout_check(t)				\
	MACRO_BEGIN					\
	if ((t)->set & TIMEOUT_ACTIVE)			\
	    reset_timeout((t));				\
	MACRO_END

extern void init_timeout (void);

/*
 * Record a timestamp in STAMP.  Records values in the boot-time clock
 * frame.
 */
extern void record_time_stamp (time_value64_t *stamp);

/*
 * Read a timestamp in STAMP into RESULT.  Returns values in the
 * real-time clock frame.
 */
extern void read_time_stamp (const time_value64_t *stamp, time_value64_t *result);

extern void mapable_time_init (void);

/* For public timer elements.  */
extern timeout_t timeout(timer_func_t *fcn, void *param, int interval);

extern int timeopen(dev_t dev, int flag, io_req_t ior);
extern void timeclose(dev_t dev, int flag);

/* For high-precision clocks.  */
extern uint32_t hpclock_read_counter(void);
extern uint32_t hpclock_get_counter_period_nsec(void);

#endif /* _KERN_MACH_CLOCK_H_ */
