/*
 *  Copyright (C) 2006-2009, 2011 Free Software Foundation
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

#ifndef XEN_XEN_H
#define XEN_XEN_H

void hyp_init(void);
void hyp_dev_init(void);
void hyp_idle(void);
void hyp_p2m_init(void);

struct i386_interrupt_state;
void hypclock_machine_intr(int old_ipl, void *ret_addr, struct i386_interrupt_state *regs, uint64_t delta);

struct failsafe_callback_regs {
	unsigned int ds;
	unsigned int es;
	unsigned int fs;
	unsigned int gs;
	unsigned int ip;
	unsigned int cs_and_mask;
	unsigned int flags;
};

void hyp_failsafe_c_callback(struct failsafe_callback_regs *regs);

#endif /* XEN_XEN_H */
