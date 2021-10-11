/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Realmode pseudo-driver for Boca ISA serial boards
 *
 *    This bef builds device nodes for the serial ports
 *    on ISA Boca multiport serial boards.
 */

#ident "@(#)boca.c   1.2   97/02/11 SMI"

#include <befext.h>
#include <string.h>
#include <biosmap.h>
#include <stdio.h>

/* Baud rate defines */
#define	B1200		0x60
#define	B2400		0x30
#define	B4800		0x18
#define	B9600		0x0c

struct ComRegs {
	/* UART register save area map ...    */

	unsigned char lcr;	/* Line control register */
	unsigned int  lat;	/* Line speed (divisor latch) */
	unsigned char mcr;	/* Modem control register */
	unsigned char ier;	/* Interrupt enable register */
	unsigned char iir;	/* Interrupt ID register */
};

/* Port offsets for various UART registers: */

#define	TXB		0	/* Transmit/receive buffer */
#define	IER		1	/* Interrupt enable register */
#define	FCR		2	/* FIFO control register */
#define	LCR		3	/* Line control register */
#define	MCR		4	/* Modem control register */
#define	LSR		5	/* Line status register	*/
#define	MSR		6	/* Modem status register */

/* Line control flags: */

#define	LC_BMASK	0x03	/* Data bit mask */
#define	LC_BITS5	0x00	/* .. 5 data bits */
#define	LC_BITS6	0x01	/* .. 6 data bits */
#define	LC_BITS7	0x02	/* .. 7 data bits */
#define	LC_BITS8	0x03	/* .. 8 data bits */
#define	LC_STOP2	0x04	/* Two stop bits if set; one if clear. */
#define	LC_PENAB	0x08	/* Parity enable (disable if clear) */
#define	LC_PODD		0x00	/* Odd parity */
#define	LC_PEVEN	0x10	/* Even parity */
#define	LC_PMARK	0x20	/* Mark parity */
#define	LC_PSPACE	0x30	/* Space parity */
#define	LC_BREAK	0x40	/* Transmit break */
#define	LC_DLAB		0x80	/* Divisor latch access bit */

/* Modem control flags:	*/

#define	MC_DTR		0x01	/* Data terminal ready */
#define	MC_RTS		0x02	/* Request to send */
#define	MC_OUT1		0x04	/* Output 1 */
#define	MC_OUT2		0x08	/* Output 2 / Interupt Enable */
#define	MC_LOOP		0x10	/* Loopback mode */

/* Line status flags: */

#define	LS_DR		0x01	/* Data ready */
#define	LS_OERR		0x02	/* Overrun error */
#define	LS_PERR		0x04	/* Parity error */
#define	LS_FERR		0x08	/* Framing error */
#define	LS_BI		0x10	/* Break interrupt */
#define	LS_THRE		0x20	/* TX holding register empty */
#define	LS_TSRE		0x40	/* TX shift register empty */
#define	LS_TXRE		0x60	/* .. Combination of the two! */

/* Modem status flags: */

#define	MS_DCTS		0x01	/* Delta clear to send */
#define	MS_DDSR		0x02	/* Delta data set ready */
#define	MS_TERI		0x04	/* Trailing edge of ring indicator */
#define	MS_DDCD		0x08	/* Delta receiver line signal detect */
#define	MS_CTS		0x10	/* Clear to send */
#define	MS_DSR		0x20	/* Data set ready */
#define	MS_RI		0x40	/* Ring indicator */
#define	MS_DCD		0x80	/* Receiver line signal detect */

/* FIFO status/control flags */

#define	FC_ENAB		0xc0	/* FIFO's enabled */
#define	FC_FON		0x01	/* Turn Xmit/Rcv FIFO's on */
#define	FC_RRST		0x02	/* Reset Recieve FIFO */
#define	FC_XRST		0x04	/* Reset Xmit FIFO */


/*
 * Interrupt controller ports (for IRQ probing):
 */
#define	PIC1_PORT	0x20	/* Master interrupt controller port */
#define	PIC2_PORT	0xa0	/* Slave interrupt controller port */
#define	IRQ_MASTER	0xb8	/* Mask for probing master IRQ list */
#define	IRQ_SLAVE	0x02	/* Mask for probing slave IRQ list */
#define	READ_IRR	0x0a	/* Read interrupt request reg */
#define	READ_ISR	0x0b	/* Read interrupt status reg */
#define	INT_POLL	0x0c	/* Poll interrupt controller */
#define	INT_EOI		0x20	/* Issue EOI to interrupt controller */

void
set_pic_imrs(unsigned short mask)
{
	/*
	 * Do master pic
	 */
	outp(PIC1_PORT+1, mask & 0xff);
	/*
	 * Do slave pic
	 */
	outp(PIC2_PORT+1, mask >> 8);
}

unsigned short
read_pic_regs(int which)
{
	unsigned short regs;

	/*
	 * Do master pic
	 */
	outp(PIC1_PORT, which);
	regs = inp(PIC1_PORT);
	outp(PIC1_PORT, READ_ISR);
	/*
	 * Do slave pic
	 */
	outp(PIC2_PORT, which);
	regs |= inp(PIC2_PORT) << 8;
	outp(PIC2_PORT, READ_ISR);
	return (regs);
}

/*
 * set the proper bit in the mask word for the given irq
 */
#define	imask(irq)	(1 << ((irq) == 2 ? 9 : (irq)))

/*
 * Write a character to serial port:
 *
 * This routine waits for the transmitter shift and hold registers at
 * the specified "port" to clear, then writes a single byte ("c") to
 * the port.
 */
static void
ser_putc(unsigned char c, unsigned port)
{
	int cnt = 60;

	while (cnt-- && ((inp(port+LSR) & LS_TXRE) != LS_TXRE))
		delay(1);
	outp(port+TXB, c);
	cnt = 60;
	while (cnt-- && ((inp(port+LSR) & LS_TXRE) != LS_TXRE)) {
		delay(1);
	}
}

/*
 * Set port speed:
 *
 * This routine sets the line speed at the given "port" to the
 * specified baud "rate".  Not much to it really, just write the
 * "rate" word to the transmit/receive buffer after first turning
 * on the "access latch" line control flag.
 */
static void
SetBaudRate(int port, int rate)
{
	unsigned char lcr = inp(port+LCR);

	outp(port+LCR, (lcr | LC_DLAB));
	if (inp(port+LCR) != (lcr|LC_DLAB))
		return;
	outp(port+TXB+1, (rate >> 8));
	outp(port+TXB, (rate & 0xFF));

	outp(port+LCR, (lcr & ~LC_DLAB)); /* Disable latch access */
	delay(100);  /* Wait for things to settle */
}

/*
 * Save serial port state:
 *
 * This routine saves UART registers in the indicated "buf"fer.  Only
 * those registers that we dink with while probing for serial mice are
 * actually saved.
 */
static void
SavePortState(int port, struct ComRegs *buf)
{
	buf->lcr = inp(port+LCR);
	buf->mcr = inp(port+MCR);
	buf->ier = inp(port+IER);
	buf->iir = inp(port+FCR);
	outp(port+LCR, (buf->lcr | LC_DLAB));
	buf->lat = (inp(port+TXB + 1) << 8) + inp(port+TXB);
	outp(port+LCR, (buf->lcr & ~LC_DLAB)); /* Turn off latch access */
}

/*
 * Restore serial port sanity:
 *
 * This routine restores UART registers saved by its companion routine,
 * above.  It then shuts the port down to as sane a state as possible.
 */
static void
RestorePortSanity(int port, struct ComRegs *buf)
{
	int timeout;

	outp(port+LCR, LC_DLAB);
	outp(port+TXB, (buf->lat & 0xFF));
	outp(port+TXB + 1, (buf->lat >> 8));
	outp(port+LCR, ~LC_DLAB);
	/*
	 * Leave interrupts disabled on the port and any mice powered down
	 */
	outp(port+IER, 0);		/* Disable interrupts at the port */
	outp(port+MCR, 0);		/* Power down the mouse	*/
	/*
	 * Flush any trash that might be hanging around in the FIFOs.
	 */
	timeout = 100;
	while (timeout-- > 0) {
		(void) inp(port+TXB);
		delay(1);
	}
	(void) inp(port+LSR);		/* To reset error state	*/
}

/*
 * See if we can reserve the necessary I/O range at the given address.
 */
int
address_probe(unsigned port, int len)
{
	char *cp;
	unsigned long plen, val[3];

	plen = PortTupleSize;
	val[2] = 0;
	val[1] = len;
	val[0] = port;

	/*
	 * See if the desired I/O address range is available
	 */
	if (set_res("port", val, &plen, 0) != RES_OK) {
		return (0);
	}
	return (1);
}

/*
 * See if we have something that looks like a NS16550 UART at the
 * given address.
 */
int
uart_probe(unsigned port)
{
	struct ComRegs state;

	/*
	 * Check hard wired zeroes are in their proper places
	 */
	if ((inp(port+IER) & 0xf0) ||
		(inp(port+FCR) & 0x30) ||
		(inp(port+MCR) & 0xe0)) {
		return (0);
	}
	/*
	 * Passed first check, see if we can read/write a baud rate divisor
	 */
	SetBaudRate(port, B9600);
	state.lat = 0;
	SavePortState(port, &state);
	if (state.lat != B9600) {
		return (0);
	}
	/*
	 * OK, we probably have a 16550 at the given address
	 * Leave interrupts disabled on the port
	 */
	outp(port+IER, 0);		/* Disable interrupts at the port */
	outp(port+MCR, 0);		/* Shut off external interrupts */
	(void) inp(port+LSR);		/* To reset error state	*/
	return (1);
}

static void
drain_int(unsigned short mask)
{
	_asm { cli };
	set_pic_imrs(~mask);
	outp(PIC1_PORT, INT_POLL);
	(void) inp(PIC1_PORT);
	outp(PIC1_PORT, INT_EOI);
	outp(PIC2_PORT, INT_POLL);
	(void) inp(PIC2_PORT);
	outp(PIC2_PORT, INT_EOI);
	set_pic_imrs(mask);
	_asm { sti };
}

/*
 * Find out what irq is connected to the uart at the given address
 * For Serial ports, we force the port to generate an interrupt
 * and watch which interrupt line goes hi.
 * Routine returns the configured IRQ number, or 0 if it can't figure
 * it out (0 is always invalid as it's reserved for NMIs).
 */
int
irq_probe(unsigned port)
{
	int i, j, irq = 0;
	unsigned short irrs, mask, map, nmap;
	unsigned char SaveMaster, SaveSlave;
	unsigned long len, val[3];

	SaveMaster = inp(PIC1_PORT+1);
	SaveSlave = inp(PIC2_PORT+1);
	/*
	 * Make mask for possible boca board irq's
	 */
	mask = imask(15)|imask(12)|imask(11)|imask(10)|imask(7)|imask(5)|
		imask(4)|imask(3);
	set_pic_imrs(mask);
	/*
	 * Set up port to transmit a character
	 */
	SetBaudRate(port, B9600);
	outp(port+LCR, LC_BITS7);
	outp(port+IER, 0xf);
	outp(port+FCR, 0);
	inp(port+FCR); /* clear any interrupt */
	inp(port+LCR);
	inp(port+MSR);
	inp(port+TXB);
	outp(port+MCR, MC_OUT2); /* Enable interrupts to PIC */
	delay(3);
	irrs = read_pic_regs(READ_IRR) & mask;
	if (irrs) {
		/*
		 * Something is already trying to use an interrupt in
		 * our mask.  Let's try to drain it from the PIC
		 */
		for (i = 0; i < 8; i++) {
			drain_int(mask);
			if ((read_pic_regs(READ_IRR) & mask) == 0)
				break;
		}
	}
	/*
	 * Loop 4 times forcing a transmit complete interrupt from
	 * the serial port on each iteration of the loop.
	 */
	map = mask;
	for (j = 0; j < 4; j++) {
		unsigned short nirr;

		outp(port+MCR, MC_OUT2); /* Enable interrupts to PIC */
		ser_putc(0, port);
		delay(10);
		irrs = read_pic_regs(READ_IRR) & mask;
		outp(port+MCR, 0); /* clear interrupt gate */
		inp(port+LSR); /* clear line error interrupt */
		inp(port+TXB); /* clear char recvd interrupt */
		inp(port+FCR); /* clear tx done interrupt */
		inp(port+MSR); /* clear modem status interrupt */
		/*
		 * map of bits that toggled
		 */
		nirr = read_pic_regs(READ_IRR);
		if ((nirr & mask) == (irrs & mask)) {
			/*
			 * No bits cleared, lets try to clear out PIC
			 * since it may be latching interrupts
			 */
			drain_int(mask);
			nirr = read_pic_regs(READ_IRR);
		}
		nmap = (nirr & mask) ^ irrs;
		/*
		 * now keep only bits that toggled before
		 */
		map &= nmap;
	}
	/*
	 * Disable serial port interrupts
	 */
	outp(port+IER, 0);
	outp(port+MCR, 0);
	/*
	 * Now look for bits set in the interrupt map.  There
	 * should be only one, it corresponds to the IRQ
	 * assigned to the boca board.
	 */
	for (j = 0; j < 16; (j++, map >>= 1)) {
		if (map & 1) {
			irq = j;
			break;
		}
	}
	outp(PIC1_PORT+1, SaveMaster);
	outp(PIC2_PORT+1, SaveSlave);
	if (irq == 0) {
		return (0);
	}
	/*
	 * See if irq we think card uses is available
	 */
	val[2] = 0;
	val[1] = 0;
	val[0] = irq;
	len = IrqTupleSize;
	if (set_res("irq", val, &len, 0) != RES_OK) {
		return (0);
	}
	return (irq);
}

/*
 * Driver Function Dispatcher:
 *
 * This is the realmode driver equivalent of a "main" routine.  It
 * processes the four possible driver functions.
 */
int
dispatch(int func)
{
	unsigned int base;
	int i, irq, nports;
	unsigned long len, val[3];

	if (func == BEF_LEGACYPROBE) {
		/*
		 * Probe the possible locations for the start of a Boca 8
		 * port serial card.
		 */
		for (base = 0x0100; base < 0x0400; base += 0x40) {
			if (node_op(NODE_START) == NODE_OK) {
				if (address_probe(base, 64) == 0)
					goto fail;
				nports = 8;
				for (i = 0; i < nports; i ++)
					if (uart_probe(base + i * 8) == 0)
						if (i == 4)
							nports = 4;
						else
							goto fail;
				if ((irq = irq_probe(base)) == 0)
					goto fail;
				(void) node_op(NODE_FREE);
			}
			/*
			 * Now create 4 or 8 asy ports with a shared interrupt
			 */
			for (i = 0; i < nports; i ++) {
				if (node_op(NODE_START) == NODE_OK) {
					len = PortTupleSize;
					val[2] = 0;
					val[1] = 8;
					val[0] = base + i * 8;

					if (set_res("port", val, &len, 0) !=
						RES_OK)
						goto fail;
					val[1] = 0;
					val[0] = irq;
					len = IrqTupleSize;
					if (set_res("irq", val, &len,
						RES_SHARE) != RES_OK)
						goto fail;
					(void) node_op(NODE_DONE);
				}
			}
			continue;
fail:
			node_op(NODE_FREE);
			continue;
		}
	} else {
		return (BEF_BADFUNC);	/* Not an operation we support! */
	}
	return (BEF_OK);
}
