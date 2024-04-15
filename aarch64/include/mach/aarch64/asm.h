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

#ifndef _MACH_AARCH64_ASM_H_
#define _MACH_AARCH64_ASM_H_

#define EXT(x)		x
#define LEXT(x)		x ## :
#define SEXT(x)		#x

#define TEXT_ALIGN	4
#define DATA_ALIGN	4

#define SVC		svc # (0)
#define ENTRY(x)	.globl EXT(x); .type EXT(x), @function; .p2align TEXT_ALIGN; LEXT(x)
#define END(x)		.size x,.-x

#ifdef __ARM_FEATURE_BTI_DEFAULT
#define MACH_BTI_C	bti c
#else
#define MACH_BTI_C
#endif

#endif /* _MACH_AARCH64_ASM_H_ */
