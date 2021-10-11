/*
 *  Copyright (c) 1999 by Sun Microsystems, Inc.
 *  All rights reserved.
 *
 *  Realmode pseudo-driver for serial ports:
 *
 *    This routine builds device nodes for the standard serial ports at I/O
 *    addresses 3F8/3E8 and 2F8/2E8.
 */

#ident "@(#)com.c   1.30   99/03/23 SMI"

#include <befext.h>
#include <string.h>
#include <biosmap.h>
#include <stdio.h>

char *comprop = "comX-noprobe";
char *inprop = "input-device";
char *outprop = "output-device";
char *ignprop = "comX-ignore-cd";
char *offprop = "comX-dtr-rts-off";
char *ttyname = "ttyX";
char *comname = "comX";

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
#define	MC_OUT2		0x08	/* Output 2 */
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
 * Read a character from serial port:
 *
 * This routine spin waits until the "data ready" flag is high on the
 * indicated serial port, then reads the next input character and re-
 * turns it to the caller.  Returns -1 if no data appears within 60
 * milliseconds.
 */
static unsigned char
ser_getc(int port)
{

	int cnt = 60;

	while (cnt-- && !(inp(port+LSR) & LS_DR))
		delay(1);
#ifdef DEBUG
	if (cnt < 0)
		printf("ser_getc timed out\n");
#endif
	return ((cnt >= 0) ? inp(port+TXB) : 0xff);
}

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
	/*
	 * If FIFO's are enabled, turn them off and clear the FIFO's
	 */
	if ((buf->iir & FC_ENAB) == FC_ENAB)
		outp(port+FCR, 0);
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
 * This function Implements the Plug and Play External COM Device
 * Specification rev 1.00 (Feb 28, 1995) probe function to search
 * for any devices connected to the specified COM port.
 * If one is found, it returns true, else false.
 */
static int
PnPSerProbe(int port)
{
	int i;
	unsigned char lsr, c;
	int offset = FALSE;
	int cnt;
	int idstart = 0;
	char PnPID[256];
	char far *fptr;
	int plen;

	/*
	 * Set DTR, clear RTS, wait 200ms for DSR
	 */
	outp(port+MCR, MC_DTR);
	for (i = 0; i < 200; i++) {
		if (inp(port+MSR) & MS_DSR)
			break;
		delay(1);
	}
	if (i == 200)
		goto fail;
	/*
	 * Set Port to 1200 bits/s, 7 data bits, no parity, 1 stop bit
	 * Clear DTR and RTS, wait 200ms
	 */
	SetBaudRate(port, B1200);
	outp(port+LCR, LC_BITS7);
	outp(port+MCR, 0);
	delay(200);
	/*
	 * Set DTR and wait 200ms
	 */
	outp(port+MCR, MC_DTR);
	delay(200);
	/*
	 * Set RTS and wait 200ms for a character
	 */
	outp(port+MCR, MC_DTR|MC_RTS);
	for (i = 0; i < 200; i++) {
		if (inp(port+LSR) & LS_DR)
			goto collectid;
		delay(1);
	}
	/*
	 * Clear DTR and RTS and wait 200ms
	 */
	outp(port+MCR, 0);
	delay(200);
	/*
	 * Set DTR and RTS and wait 200ms for a character
	 */
	outp(port+MCR, MC_DTR|MC_RTS);
	for (i = 0; i < 200; i++) {
		if (inp(port+LSR) & LS_DR)
			goto collectid;
		delay(1);
	}
	goto fail;
collectid:
	/*
	 * Character detected on port, collect PnP ID
	 */
	cnt = 0;
	/*
	 * Max length of a PnP Id is 256 characters
	 */
	while (cnt < 257) {
		for (i = 0; i < 200; i++) {
			if ((lsr = inp(port+LSR)) & LS_DR)
				break;
			/*
			 * check for errors
			 */
			if (lsr & (LS_OERR|LS_PERR|LS_FERR))
				goto fail;
			delay(1);
		}
		if (i == 200)
			goto fail;
		c = inp(port+TXB);
		if (c == 0x08) {
			offset = TRUE;
			idstart = cnt;
		}
		if (c == 0x28)
			idstart = cnt;
		PnPID[cnt++] = c;
		/*
		 * check for EndPnP char
		 */
		if ((!offset && c == 0x29) || (offset && c == 0x9))
			break;
	}
	if (offset)
		for (i = 0; i < 7; i++)
			PnPID[idstart + i + 3] += 0x20;
	PnPID[idstart + 7 + 3] = '\0';
	/*
	 * We have detected a serial PnP device attached, set the ID
	 * as a property (Note: maybe we need to set the whole PnP id
	 * here rather than just the Manufacturer and product ID's as
	 * we do now).  However, interested parties can just inqure for
	 * the whole thing if they really need it.
	 */
	plen = 8;
	fptr = (char far *)&PnPID[idstart + 3];
	set_prop("external-com-pnp-id", &fptr, &plen, 0);
	return (1);
fail:
	return (0);
}

/*
 * This routine is used to determine whether or not a mouse responding
 * to one of several serial protocols is attached to the
 * given serial "port".  We do this by power cycling the mouse and
 * then checking the first input character to appear after we turn it
 * back on.
 */
static int
MouseSerProbe(int port, int timeout)
{
	int j, k;
	int ret = 0;
	char far *protocol;
	unsigned char ier = inp(port+IER);

	outp(port+IER, 0);		/* Disable interrupts at the port */
	outp(port+MCR, MC_DTR);		/* Power down the mouse	(drop RTS) */
	delay(200);			/* wait 200 ms. */

	/*
	 * Flush any trash that might be hanging around in the FIFOs.
	 */
	while (timeout-- > 0) {
		inp(port+TXB);
		delay(1);
	}

	inp(port+LSR);			/* To reset error state	*/
	outp(port+MCR, MC_DTR+MC_RTS);	/* Turn the mouse back on again */

	for (j = 5; j-- > 0; k = 0) {
		/*
		 * Try up to 5 times to read a character from the serial line.
		 * If we have a Microsoft/MM mouse on the other end, it will
		 * respond with an appropriate type byte.  Of course, other
		 * serial devices may respond with these codes as well, but
		 * we're very trusting!
		 */
		k = ser_getc(port);
		if (k == 'M' || k == 'B') {
			/*
			 * Device on the other end of the serial line knows
			 * the proper magic words.  Assume it's a Microsoft
			 * protocol serial mouse
			 */
			protocol = "MM";
			ret = 2;
			break;
		}
		if (k == 'm' || k == 'H') {
			/*
			 * Device on the other end of the serial line looks
			 * like a Mouse Systems Corp Serial 3 button mouse.
			 */
			protocol = "MSC";
			ret = 3;
			goto done;
		}
	}
	for (j = 0; j < 5; j++) {
		/*
		 * Try up to 5 times to read a 2nd character from the serial
		 * line.  A logitech type V or type W serial mouse will send
		 * a '3' if it is a 3 button mouse.
		 */
		if ((k = ser_getc(port)) != -1) {
			if (k == '3') {
				protocol = "M+";
				ret = 3;
				break;
			}
		}
	}

done:
	outp(port+IER, ier);
	if (ret) {
		int plen;

		plen = strlen(protocol) + 1;
		set_prop("serial-mouse-protocol", &protocol, &plen, 0);
	}
	return (ret);
}

/*
 * Probe for devices on a serial line:
 *
 * This routine probes for a serial mouse at the given I/O "port".
 * It also probes for any Serial PnP compliant device attached to
 * the port.
 */
int
SerialProbe(unsigned port)
{
	int j, k;
	long nbuttons = 0;
	unsigned char c;
	struct ComRegs state;
	static const char BaudRates[] = {
		/* Cycle thru these baud rates */
		B1200, B2400, B4800, B9600
	};
	unsigned char status;
	unsigned char mcr;
	int plen;
	char far *fptr;
	char far *protocol;

	SavePortState(port, &state);
	SetBaudRate(port, B1200);
	outp(port+LCR, LC_BITS7);
	outp(port+IER, 0);
	inp(port+LCR);

	/*
	 * probe for PnP serial devices
	 */
	if (PnPSerProbe(port)) {
		goto devicefound;
	}
	SetBaudRate(port, B1200);
	outp(port+LCR, LC_BITS7);
	outp(port+IER, 0);
	inp(port+LCR);
	if (nbuttons = MouseSerProbe(port, 6)) {
		goto devicefound;
	}
	if (nbuttons = MouseSerProbe(port, 18)) {
		goto devicefound;
	}
	/*
	 * The "MouseSerProbe" routine will detect
	 * mice that follow the Microsoft/MM, Logitech M+  or
	 * Mouse Systems Corp serial protocols.
	 * We have to be a bit sneakier to recognize a Logitech Type C
	 * serial mouse ...
	 */
	mcr = inp(port+MCR);
	if (!(mcr & (MC_DTR+MC_RTS))) {
		/*
		 * If DTR and RTS aren't high, set them active
		 * here and wait for the line to come up.
		 */

		outp(port+MCR, MC_DTR+MC_RTS);
		delay(500);
	}

	for (k = 0; k < sizeof (BaudRates); k++) {
		/*
		 * For each of the four possible Logitech baud
		 * rates, try putting the mouse in prompt mode
		 * and then ask for the status byte:
		 */

		outp(port+LCR, LC_BITS8+LC_PENAB);
		SetBaudRate(port, BaudRates[k]);
		ser_putc('D', port);
		ser_putc('S', port);

		outp(port+LCR, LC_BITS8+LC_PENAB);
		ser_putc('s', port);

		if ((c = ser_getc(port)) == 0x4F) {
			/*
			 * The device at the other end of this
			 * port was able to recognize the com-
			 * mand sequence we just sent it.  If
			 * not a Logitech mouse, it's a pretty
			 * good imitation.  Reset the baud rate
			 * to 1200 bps and take the mouse out
			 * of prompt mode.
			 */

			k = 10;
			ser_putc('*', port);
			ser_putc('c', port);

			while (k-- && ((inp(port+LSR) & LS_TXRE) != LS_TXRE)) {
				/* Wait for lines to settle  */
				delay(1);
			}

			SetBaudRate(port, B1200);
			ser_putc('N', port);
			/*
			 * assume 3 buttons
			 */
			protocol = "MSC";
			plen = strlen(protocol) + 1;
			set_prop("serial-mouse-protocol", &protocol, &plen, 0);
			nbuttons = 3;
			goto devicefound;
		}
#ifdef DEBUG
		else
			printf("got 0x%x from port\n", c);
#endif
	}
	RestorePortSanity(port, &state);
	return (0);

devicefound:
	RestorePortSanity(port, &state);
	if (nbuttons) {
		plen = 4;
		fptr = (char far *)&nbuttons;
		set_prop("mouse-nbuttons", &fptr, &plen, PROP_BIN);
	}
	return (1);
}

int
com_probe(unsigned port)
{
	char *cp;
	char far *fptr;
	unsigned long len, val[3];
	int flag, plen;

	len = PortTupleSize;
	val[2] = 0;
	val[1] = 8;
	val[0] = port;

	if (set_res("port", val, &len, 0) != RES_OK)
		return (0);
	/*
	 * We reserved the port on behalf of the current device.
	 * For each COM port, set the default IRQ value before
	 * reading the comX-irq property value. If the property
	 * exists and the value is a digit, set the IRQ with the
	 * property value.
	 */
	fptr = (char far *)0;
	switch (port) {
	case 0x3F8: /* COM1 */
		val[0] = 4;
		(void) get_prop("com1-irq", &fptr, &plen);
		if (fptr && isdigit(*fptr)) {
			val[0] = atoin(fptr, plen);
		}
		flag = 0;
		break;
	case 0x3E8: /* COM3 */
		val[0] = 4;
		(void) get_prop("com3-irq", &fptr, &plen);
		if (fptr && isdigit(*fptr)) {
			val[0] = atoin(fptr, plen);
		}
		flag = 0;
		break;
	case 0x2F8: /* COM2 */
		val[0] = 3;
		(void) get_prop("com2-irq", &fptr, &plen);
		if (fptr && isdigit(*fptr)) {
			val[0] = atoin(fptr, plen);
		}
		flag = 0;
		break;
	case 0x2E8: /* COM4 */
		val[0] = 3;
		(void) get_prop("com4-irq", &fptr, &plen);
		if (fptr && isdigit(*fptr)) {
			val[0] = atoin(fptr, plen);
		}
		flag = 0;
		break;
	}
	val[1] = 0;
	len = IrqTupleSize;
	if (set_res("irq", val, &len, flag) != RES_OK)
		return (0);
	else
		return (1);
};

char *
strstr(char *as1, char *as2)
{
	char *s1;
	char *s2;
	char *tptr;
	char c;

	s1 = as1;
	s2 = as2;

	if (s2 == (char *)0 || *s2 == '\0')
		return ((char *)s1);
	c = *s2;

	while (*s1)
		if (*s1++ == c) {
			tptr = s1;
			while ((c = *++s2) == *s1++ && c)
				/* loop */;

			if (c == 0)
				return ((char *)tptr - 1);
			s1 = tptr;
			s2 = as2;
			c = *s2;
		}
	return ((char *)0);
}

void
com_install(unsigned port)
{
	char *cp;
	unsigned long len, val[3];
	char com, tty;
	char far *fptr;
	char far *iptr;
	int plen;
	int isconsole = 0;
	int isprop = 0;
	char tname[80];
	int i;

	switch (port) {
	/*
	 * Standard COM1/COM3 ports use IRQ 4.
	 */
	case 0x3E8: /* COM3 */
		com = '3';
		tty = 'c';
		break;
	case 0x3F8: /* COM1 */
		com = '1';
		tty = 'a';
		break;
	case 0x2E8: /* COM4 */
		com = '4';
		tty = 'd';
		break;
	case 0x2F8: /* COM2 */
		com = '2';
		tty = 'b';
		break;
	}
	comprop[3] = com;
	ignprop[3] = com;
	offprop[3] = com;
	comname[3] = com;
	ttyname[3] = tty;
	fptr = (char far *)0;
	/*
	 * See if the "-noprobe", "-ignore-cd", or "rts-dtr-off"
	 * properties are set for this port
	 */
	(void) get_prop(&comprop[0], &fptr, &plen);
	if (fptr)
		isprop++;
	for (i = 0; ttyname[i]; i++)
		comprop[i] = ttyname[i];
	fptr = (char far *)0;
	(void) get_prop(&comprop[0], &fptr, &plen);
	if (fptr)
		isprop++;

	fptr = (char far *)0;
	(void) get_prop(&ignprop[0], &fptr, &plen);
	if (fptr)
		isprop++;
	for (i = 0; ttyname[i]; i++)
		ignprop[i] = ttyname[i];
	fptr = (char far *)0;
	(void) get_prop(&ignprop[0], &fptr, &plen);
	if (fptr)
		isprop++;

	fptr = (char far *)0;
	(void) get_prop(&offprop[0], &fptr, &plen);
	if (fptr)
		isprop++;
	for (i = 0; ttyname[i]; i++)
		offprop[i] = ttyname[i];
	fptr = (char far *)0;
	(void) get_prop(&offprop[0], &fptr, &plen);
	if (fptr)
		isprop++;

	/*
	 * Get the "input-device" property.  If it is set to this com
	 * port we do not want to probe it since it is currently the
	 * console
	 */
	(void) get_prop(&inprop[0], &iptr, &plen);
	for (i = 0; i < plen && i < 80; i++) {	/* remove embedded '\n' */
		tname[i] = iptr[i];
		if (tname[i] == '\n')
			tname[i] = '\0';
	}
	tname[i] = '\0';

	if (iptr != (char far *)0) {
		isconsole |= (strstr(tname, comname) != 0);
		isconsole |= (strstr(tname, ttyname) != 0);
	}
	/*
	 * Now do the same thing for "output-device"
	 */
	(void) get_prop(&outprop[0], &iptr, &plen);
	for (i = 0; i < plen && i < 80; i++) {	/* remove embedded '\n' */
		tname[i] = iptr[i];
		if (tname[i] == '\n')
			tname[i] = '\0';
	}
	tname[i] = '\0';

	if (iptr != (char far *)0) {
		isconsole |= (strstr(tname, comname) != 0);
		isconsole |= (strstr(tname, ttyname) != 0);
	}
	/*
	 * If "comX-noprobe" is not set and this port is not currently the
	 * console, probe port for Serial mice and other Serial PnP
	 * compatible devices
	 */
	if (!isprop && !isconsole)
		SerialProbe(port);
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
	return (1);
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
	int j;
	unsigned int port;
	unsigned long val[3], len;
	const int maxports = sizeof (bdap->ComPort)/sizeof (bdap->ComPort[0]);

	if (func == BEF_LEGACYPROBE) {
		/*
		 * Caller wants to probe for serial ports.  Do a simple
		 * check that com ports listed in the BIOS data area are
		 * really 16550-type uarts.
		 */
		for (j = 0; j < maxports; j++) {
			/*
			 * Check all possible serial ports.  If there's a
			 * non-zero I/O address listed in the BIOS data area,
			 * we check that the corresponding serial I/O port
			 * looks sane.
			 */
			if ((port = bdap->ComPort[j]) && uart_probe(port)) {
				if (node_op(NODE_START) != NODE_OK)
					continue;
				/*
				 * We've found another serial I/O port and
				 * established a device node for it.  Now
				 * let's see if the port address has already
				 * been reserved to some other device!
				 */
				(void) node_op(com_probe(port) ? NODE_DONE :
					NODE_FREE);
			}
		}
	} else if (func == BEF_INSTALLONLY) {
		/*
		 * Go through the probed for ports and do "install" operations,
		 * in this case looking for attached devices of interest and
		 * attaching appropriate properties.
		 */
		while (node_op(NODE_START) != NODE_FAIL) {
			len = PortTupleSize;
			if (get_res("port", val, &len) != RES_OK) {
				node_op(NODE_FREE);
				return (BEF_FAIL);
			}
			port = val[0];
			com_install(port);
			node_op(NODE_DONE);
		}
	} else {
		return (BEF_BADFUNC);	/* Not an operation we support! */
	}
	return (BEF_OK);
}
