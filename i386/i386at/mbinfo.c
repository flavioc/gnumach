/*
 * Copyright (c) 2024 Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <i386at/mbinfo.h>
#include <mach/vm_param.h>
#include <vm/pmap.h>
#include <device/ds_routines.h>

static struct multiboot_raw_info mb_info;

void
mbinfo_register_boot_data(const struct multiboot_raw_info *mbi)
{
	mb_info = *mbi;
}

io_return_t
mbinforead(dev_t dev, io_req_t ior)
{
	int err;

	/* Check if IO_COUNT is valid */
	if (ior->io_count > sizeof(struct multiboot_raw_info))
	    return D_INVALID_SIZE;

	err = device_read_alloc(ior, (vm_size_t)ior->io_count);
	if (err != KERN_SUCCESS)
	    return (err);

	memcpy ((uint8_t *)ior->io_data, &mb_info, ior->io_count);

	ior->io_residual = 0;
	return (D_SUCCESS);
}
