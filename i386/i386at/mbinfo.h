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

#ifndef _I386AT_MBINFO_H_
#define _I386AT_MBINFO_H_	1

#include <sys/types.h>

#include <vm/vm_types.h>
#include <mach/vm_prot.h>
#include <device/device_types.h>
#include <device/io_req.h>
#include <mach/machine/multiboot.h>

void mbinfo_register_boot_data(const struct multiboot_raw_info *mbi);

io_return_t mbinforead(dev_t dev, io_req_t ior);

#endif /* !_I386AT_MBINFO_H_ */
