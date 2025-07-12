/*
 * Copyright (C) 1995 Shantanu Goel
 * Copyright (C) 2020 Free Software Foundation, Inc
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
 */

#include <i386/irq.h>
#include <device/intr.h>
#include <mach/kern_return.h>
#include <kern/queue.h>
#include <kern/assert.h>
#include <machine/spl.h>

extern queue_head_t main_intr_queue;

static void
irq_eoi (struct irqdev *dev, int id)
{
#ifdef APIC
  ioapic_irq_eoi (dev->irq[id]);
#endif
}

/* Each array elem fits in a cache line */
struct nested_irq {
  simple_lock_irq_data_t irq_lock; /* Protects ndisabled */
  int32_t ndisabled;
  uint32_t unused[14];
} __attribute__((packed)) nested_irqs[NINTR];

void
init_irqs (void)
{
  int i;

  for (i = 0; i < NINTR; i++)
    {
      simple_lock_init_irq(&nested_irqs[i].irq_lock);
      nested_irqs[i].ndisabled = 0;
    }
}

void
__disable_irq (irq_t irq_nr)
{
  spl_t s;
  assert (irq_nr < NINTR);
  struct nested_irq *nirq = &nested_irqs[irq_nr];

  s = simple_lock_irq(&nirq->irq_lock);

  nirq->ndisabled++;
  assert (nirq->ndisabled > 0);
  if (nirq->ndisabled == 1)
    mask_irq (irq_nr);

  simple_unlock_irq(s, &nirq->irq_lock);
}

void
__enable_irq (irq_t irq_nr)
{
  spl_t s;
  assert (irq_nr < NINTR);
  struct nested_irq *nirq = &nested_irqs[irq_nr];

  s = simple_lock_irq(&nirq->irq_lock);

  assert (nirq->ndisabled > 0);
  nirq->ndisabled--;
  if (nirq->ndisabled == 0)
    unmask_irq (irq_nr);

  simple_unlock_irq(s, &nirq->irq_lock);
}

struct irqdev irqtab = {
  "irq", irq_eoi, &main_intr_queue, 0,
#ifdef APIC
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
  44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63},
#else
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
#endif
};

