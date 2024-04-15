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

#ifndef	_MACH_AARCH64_VM_PARAM_H_
#define _MACH_AARCH64_VM_PARAM_H_

#include <mach/machine/vm_types.h>

#define BYTE_SIZE		8	/* byte size in bits */

/*
 *	TODO: Exporting VM_MAX_ADDRESS basically locks in
 *	VM_AARCH64_T0SZ being 48.  Consider dropping it from this
 *	public header once userland no longer depends on it.
 */
#define VM_MIN_ADDRESS		(0ULL)
#define VM_MAX_ADDRESS		(0x1000000000000ULL)

#endif	/* _MACH_AARCH64_VM_PARAM_H_ */
