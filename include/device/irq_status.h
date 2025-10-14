/*
 * Copyright (c) 2025 Free Software Foundation
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * FSF ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  FSF DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */

#ifndef _DEVICE_IRQ_STATUS_H_
#define _DEVICE_IRQ_STATUS_H_

/* Flavor for device_get_status */

#define IRQGETPICMODE		0
# define ACPI_PICMODE_PIC	0
# define ACPI_PICMODE_APIC	1
# define ACPI_PICMODE_SAPIC	2

#endif /* _DEVICE_IRQ_STATUS_H_ */
