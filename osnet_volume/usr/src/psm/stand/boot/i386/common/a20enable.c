/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

/* BEGIN CSTYLED */
/*
 * Functions to enable address bit 20 on x86 PC machines.
 *
 * == CAUTION ==== CAUTION ==== CAUTION ==== CAUTION ==
 *
 *	+ a20enable() is called very early before the
 *	+ high address portion of boot has been copied
 *	+ into place. This means that only low memory
 *	+ routines and data may be referenced. Don't
 *	+ try putting any printfs here! The initial cut
 *	+ at this file only called doint_asm() and
 *	+ referenced no external data.
 *
 * == CAUTION ==== CAUTION ==== CAUTION ==== CAUTION ==
 */
/* END CSTYLED */

#pragma ident	"@(#)a20enable.c	1.3	98/07/21 SMI"

#include <sys/types.h>
#include <sys/machine.h>
#include <sys/sysmacros.h>
#include <sys/bootconf.h>
#include <sys/booti386.h>
#include <sys/bootdef.h>
#include <sys/sysenvmt.h>
#include <sys/bootinfo.h>
#include <sys/bootlink.h>
#include <sys/promif.h>
#include <sys/ramfile.h>
#include <sys/dosemul.h>
#include <sys/salib.h>

static void a20enable_doit(void);
static void localwait(u_int);
static void putchar(char);
static void send(u_int, u_int);
static int a20enabled(void);
static u_int read_data(void);
void a20enable(void);

#define	SEND_CMD(x)	send(KB_ICMD, (x))
#define	SEND_DATA(x)	send(KB_IDAT, (x))

#define	KB_AUXBF	0x20	/* Input buffer contains auxiliarly data */
#define	KB_KBDIS	0xAD	/* Disable keyboard interface */

#define	MIMR_PORT	0x21	/* Mask register for master PIC */
#define	MIMR_KB		2	/* Keyboard mask bit in master PIC */

void
a20enable(void)
{
	u_int count;

	/*
	 * The original algorithm for enabling a20 could fail if
	 * the user was actively typing.  The new algorithm should
	 * not have that problem, but there is no harm in doing
	 * a few retries ....
	 */
	for (count = 0; count < 20 && !a20enabled(); count++) {
		a20enable_doit();
	}

	if (!a20enabled()) {
		/*
		 * The a20 enable routine failed.  Put out a message
		 * and hang.  We cannot address strings yet, so we
		 * have to do it the hard way.
		 */
		putchar('A');
		putchar('2');
		putchar('0');
		putchar(' ');
		putchar('e');
		putchar('n');
		putchar('a');
		putchar('b');
		putchar('l');
		putchar('e');
		putchar(' ');
		putchar('f');
		putchar('a');
		putchar('i');
		putchar('l');
		putchar('e');
		putchar('d');
		putchar('\r');
		putchar('\n');
		for (;;)
			continue;
	}
}

/*
 * Test whether the A20 line is enabled by writing a known bit
 * pattern to 0, a different one to 1 Meg, and seeing which one
 * is at 0 afterwards.
 */
static int
a20enabled(void)
{
	u_int *p1meg = (u_int *)0x100000;
	u_int *pzero = (u_int *)0;
	u_int zero_save = *pzero;
	u_int meg_save = *p1meg;
	int answer;

	*pzero = 0xfeedface;
	*p1meg = 0xdeadbeef;
	answer = (*pzero == 0xfeedface);
	*pzero = zero_save;
	*p1meg = meg_save;
	return (answer);
}

static void
a20enable_doit(void)
{
	u_int pic_mask;
	u_int portval;

	/*
	 * The most basic algorithm for enabling A20 is to write
	 * a 1 to the keyboard controller output port bit 1.
	 * That is achieved by sending a KB_WOP (write output
	 * port) command and then sending the output port data.
	 *
	 * The first complication is that we need to know what to
	 * write to the remaining bits and so need to do a KB_ROP
	 * (read output port) command first.
	 *
	 * The next complication is that the data from the KB_ROP
	 * instruction cannot be distinguished from an incoming
	 * keyboard scan code, so we must make sure that the
	 * keyboard is disabled first.
	 *
	 * The next complication is that we cannot determine whether
	 * the keyboard was already disabled, but it is pretty safe
	 * to assume it was not.  So we just blindly disable it
	 * and reenable it.
	 *
	 * The next complication is that bytes coming from the
	 * auxiliary device also show up in the same register.
	 * Again we cannot tell whether the device is already
	 * disabled and it is less clear whether we can make any
	 * assumptions.  Fortunately the status register has a
	 * bit that allows us to distinguish auxiliary device
	 * input.  So we leave it enabled and simply discard any
	 * input that comes from it.
	 *
	 * Finally we must prevent keyboard interrupts from
	 * interfering with this whole procedure.  Otherwise when
	 * the KB_ROP data arrives we will get an interrupt that
	 * causes the keyboard interrupt handler to read the
	 * incoming data and treat it as a scan code.  So we need
	 * to disable keyboard interrupts.  We use the PIC mask
	 * rather than the processor IF bit because BIOS calls
	 * can reenable IF.
	 */

	/* Save the old interrupt mask and disable KB interrupts */
	pic_mask = inb(MIMR_PORT);
	outb(MIMR_PORT, pic_mask | MIMR_KB);

	/* Disable keyboard input within the keyboard controller */
	SEND_CMD(KB_KBDIS);

	/* Read the keyboard controller output port */
	SEND_CMD(KB_ROP);
	portval = read_data();

	/* Write the keyboard controller output port with A20 enabled */
	SEND_CMD(KB_WOP);
	SEND_DATA(portval | KB_GATE20);

	/* Reenable keyboard input within the keyboard controller */
	SEND_CMD(KB_ENAB);

	/* Reenable KB interrupts */
	outb(MIMR_PORT, pic_mask);
}

/* Send a command or some data to the keyboard controller */
static void
send(u_int addr, u_int dat)
{
	u_int stat;

	/*
	 * Wait until both the input and output buffers are empty.
	 * If the output buffer fills, read and discard the contents.
	 */
	while (((stat = inb(KB_STAT)) & (KB_OUTBF | KB_INBF)) != 0) {
		if (stat & KB_OUTBF) {
			(void) inb(KB_IDAT);
		}
		localwait(1);
	}

	/* Send the command or data and allow time to settle */
	outb(addr, dat);
	localwait(1);
}

/* Read data from the keyboard controller */
static u_int
read_data(void)
{
	u_int stat;
	u_int dat;

	/*
	 * Wait until there is an input byte that is not from the
	 * auxiliary device.  If we see one from the auxiliarly
	 * device, flush it.
	 */
	while (((stat = inb(KB_STAT)) & (KB_OUTBF | KB_AUXBF)) != KB_OUTBF) {
		if ((stat & (KB_OUTBF | KB_AUXBF)) == (KB_OUTBF | KB_AUXBF)) {
			(void) inb(KB_IDAT);
		}
		localwait(1);
	}

	/* Read the data; allow the registers to settle */
	dat = inb(KB_IDAT);
	localwait(1);

	return (dat);
}

/*
 * Use the BIOS to delay the requested number of milliseconds.
 * Note that the BIOS routine is implemented in terms of
 * microseconds but typically has a granularity more like
 * 1 millisecond.  This routine uses milliseconds to avoid
 * the illusion of microsecond accuracy.  Timing is also
 * affected by the overhead in calling doint_asm().
 */
static void
localwait(u_int millis)
{
	u_int micros = millis * 1000;
	struct real_regs local_regs, *rr;
	extern int doint_asm();

	rr = &local_regs;

	AX(rr) = 0x8600;
	CX(rr) = (ushort)(micros >> 16);
	DX(rr) = (ushort)(micros & 0xFFFF);

	(void) doint_asm(0x15, rr);
}

static void
putchar(char c)
{
	struct real_regs local_regs, *rr;
	extern int doint_asm();

	rr = &local_regs;

	AH(rr) = 0xe;
	AL(rr) = c;
	BX(rr) = 0;
	CX(rr) = 1;

	(void) doint_asm(0x10, rr);
}
