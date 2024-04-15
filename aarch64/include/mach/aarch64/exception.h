/*
 * Copyright (c) 2023-2024 Free Software Foundation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 *	Codes and subcodes for AArch64 exceptions.
 */
#ifndef	_MACH_AARCH64_EXCEPTION_H_
#define _MACH_AARCH64_EXCEPTION_H_


/*
 *	EXC_BAD_INSTRUCTION
 */

#define EXC_AARCH64_UNK			1	/* unknown reason (unallocated instruction) */
#define EXC_AARCH64_IL			2	/* illegal execution state (PSTATE.IL set) */

/*
 *	EXC_ARITHMETIC
 */

#define EXC_AARCH64_IDF			1	/* floating-point input denormal */
#define EXC_AARCH64_IXF			2	/* floating-point inexact */
#define EXC_AARCH64_UFF			3	/* floating-point underflow */
#define EXC_AARCH64_OFF			4	/* floating-point overflow */
#define EXC_AARCH64_DZF			5	/* floating-point divide by zero */
#define EXC_AARCH64_IOF			6	/* floating-point invalid operation */

/*
 *	EXC_SOFTWARE
 */

#define EXC_AARCH64_SVC			1	/* SVC that's not a valid syscall, subcode contains immediate */

/*
Not yet:
#define EXC_AARCH64_HVC			2	HVC, subcode contains immediate
#define EXC_AARCH64_SMC			3	SMC, subcode contains immediate
*/

/*
 *	EXC_BAD_ACCESS
 *
 *	Exception code normally holds a kern_return_t value (such as
 *	KERN_INVALID_ADDRESS), but for AArch64-specific exceptions these
 *	values can be used as exception code instead; they must not conflict
 *	with kern_return_t values.
 */

#define EXC_AARCH64_AL			100	/* alignment fault */
#define EXC_AARCH64_AL_PC		101	/* misaligned pc */
#define EXC_AARCH64_AL_SP		102	/* misaligned sp */
#define EXC_AARCH64_PAC			103	/* PAC failure, subcode describes the key */
#define EXC_AARCH64_BTI			104	/* BTI failure, subcode contains BTYPE */

/*
Not yet:
#define EXC_AARCH64_MTE			105	MTE failure
*/

/*
 *	EXC_BREAKPOINT
 */

#define EXC_AARCH64_BRK			1	/* BRK instruction, subcode contains immediate */
#define EXC_AARCH64_SS			2	/* software single step, subcode contains EX flag, or -1 if unknown */
#define EXC_AARCH64_BREAKPT		3	/* hardware breakpoint */

/*
Not yet:
#define EXC_AARCH64_WATCHPT_READ	4	hardware watchpoint (read), subcode contains accessed address
#define EXC_AARCH64_WATCHPT_WRITE	5	hardware watchpoint (write), subcode contains accessed address
*/

#endif	/* _MACH_AARCH64_EXCEPTION_H_ */
