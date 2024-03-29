/* auto_irq.c: Auto-configure IRQ lines for linux. */
/*
   Written 1994 by Donald Becker.

   The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
   Center of Excellence in Space Data and Information Sciences
   Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771

   This code is a general-purpose IRQ line detector for devices with
   jumpered IRQ lines.  If you can make the device raise an IRQ (and
   that IRQ line isn't already being used), these routines will tell
   you what IRQ line it's using -- perfect for those oh-so-cool boot-time
   device probes!

   To use this, first call autoirq_setup(timeout). TIMEOUT is how many
   'jiffies' (1/100 sec.) to detect other devices that have active IRQ lines,
   and can usually be zero at boot.  'autoirq_setup()' returns the bit
   vector of nominally-available IRQ lines (lines may be physically in-use,
   but not yet registered to a device).
   Next, set up your device to trigger an interrupt.
   Finally call autoirq_report(TIMEOUT) to find out which IRQ line was
   most recently active.  The TIMEOUT should usually be zero, but may
   be set to the number of jiffies to wait for a slow device to raise an IRQ.

   The idea of using the setup timeout to filter out bogus IRQs came from
   the serial driver.
 */


#ifdef version
static const char *version =
"auto_irq.c:v1.11 Donald Becker (becker@cesdis.gsfc.nasa.gov)";
#endif

#include <sys/types.h>
#include <mach/mach_types.h>
#include <mach/vm_param.h>
#include <mach/message.h>
#include <vm/vm_map.h>

#define MACH_INCLUDE
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/netdevice.h>

void *irq2dev_map[NR_IRQS] = {0, 0, /* ... zeroed */ };

unsigned long irqs_busy = 0x2147;	/* The set of fixed IRQs (keyboard, timer, etc) */
unsigned long irqs_used = 0x0001;	/* The set of fixed IRQs sometimes enabled. */
unsigned long irqs_reserved = 0x0000;	/* An advisory "reserved" table. */
unsigned long irqs_shared = 0x0000;	/* IRQ lines "shared" among conforming cards. */

static volatile unsigned long irq_bitmap;	/* The irqs we actually found. */
static unsigned long irq_handled;	/* The irq lines we have a handler on. */
static volatile int irq_number;	/* The latest irq number we actually found. */

static void 
autoirq_probe (int irq, void *dev_id, struct pt_regs *regs)
{
  irq_number = irq;
  set_bit (irq, (void *) &irq_bitmap);	/* irq_bitmap |= 1 << irq; */
  /* This code used to disable the irq. However, the interrupt stub
   * would then re-enable the interrupt with (potentially) disastrous
   * consequences
   */
  free_irq (irq, dev_id);
  return;
}

int 
autoirq_setup (int waittime)
{
  int i;
  unsigned long timeout = jiffies + waittime;
  unsigned long boguscount = (waittime * loops_per_sec) / 100;

  irq_handled = 0;
  irq_bitmap = 0;

  for (i = 0; i < 16; i++)
    {
      if (test_bit (i, &irqs_busy) == 0
	  && request_irq (i, autoirq_probe, SA_INTERRUPT, "irq probe", NULL) == 0)
	set_bit (i, (void *) &irq_handled);	/* irq_handled |= 1 << i; */
    }
  /* Update our USED lists. */
  irqs_used |= ~irq_handled;

  /* Hang out at least <waittime> jiffies waiting for bogus IRQ hits. */
  while (timeout > jiffies && --boguscount > 0)
    ;

  irq_handled &= ~irq_bitmap;

  irq_number = 0;		/* We are interested in new interrupts from now on */

  return irq_handled;
}

int 
autoirq_report (int waittime)
{
  int i;
  unsigned long timeout = jiffies + waittime;
  unsigned long boguscount = (waittime * loops_per_sec) / 100;

  /* Hang out at least <waittime> jiffies waiting for the IRQ. */

  while (timeout > jiffies && --boguscount > 0)
    if (irq_number)
      break;

  irq_handled &= ~irq_bitmap;	/* This eliminates the already reset handlers */

  /* Retract the irq handlers that we installed. */
  for (i = 0; i < 16; i++)
    {
      if (test_bit (i, (void *) &irq_handled))
	free_irq (i, NULL);
    }
  return irq_number;
}
