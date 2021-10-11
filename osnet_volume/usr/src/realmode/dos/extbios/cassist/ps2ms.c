/*
 *  Copyright (c) 1998 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Realmode pseudo-driver for ps2 mice:
 *
 *    This file contains a probe-only pseudo-bef that tries to determine
 *    if a ps/2 style mouse is attached to the system.
 *    We also attach a few relevant properties to the mouse node to aid
 *    later device configuration.
 */

#ident "<@(#)ps2ms.c	1.8	99/03/19 SMI>"
#include <befext.h>
#include <biosmap.h>
#include <string.h>
#include <stdio.h>
#include <dos.h>

/* ps/2 mouse resource definitions ... */
static unsigned long ports[] = { 0x60, 1, 0, 0x64, 1, 0 };
static unsigned long portcnt = sizeof (ports)/sizeof (ports[0]);

static unsigned long irqs[]  = { 12, 0};
static unsigned long irqcnt  = sizeof (irqs)/sizeof (irqs[0]);

#define	EISAize(l1, l2, l3, n)				\
	((unsigned long)(l1 & 0x1f) << 2) |		\
	((unsigned long)(l2 & 0x18) >> 3) |		\
	((unsigned long)(l2 & 0x07) << 13) |		\
	((unsigned long)(l3 & 0x1f) << 8) |		\
	((unsigned long)(n & 0xff) << 24) |		\
	((unsigned long)(n & 0xff00) << 8)

static unsigned long slot[] = { 1 };
static unsigned long slotcnt  = sizeof (slot)/sizeof (slot[0]);

static unsigned long name[] = { EISAize('P','N','P',0x0f03),  RES_BUS_I8042 };
static unsigned long namecnt  = sizeof (name)/sizeof (name[0]);

static char compatible[] = "pnpPNP,f03";
static char device_type[] = "mouse";


int tmpAX = 0;	/* temporary for _int86 routine */

extern void delay();

/*
 * Port defines for keyboard controller
 */
#define	KBD_CMD		0x60	/* keyboard command port */
#define	KBD_DATA	0x60	/* keyboard data port */
#define	KBD_CTL_STAT	0x64	/* keyboard controller status port */
#define	KBD_CTL_CMD	0x64	/* keyboard controller command port */

/*
 * Keyboard controller status bits
 */
#define	KBD_OBUF_FULL		0x01	/* kbd controller output buffer full */
#define	KBD_IBUF_FULL		0x02	/* kbd controller input buffer full */
#define	KBD_POST_OK		0x04	/* kbd controller post selftest ok */
#define	KBD_CMD_LAST		0x08	/* command was sent last to ctlr */
#define	KBD_ENABLED		0x10	/* if not 0, keyboard enabled */
#define	KBD_XMIT_TIMEOUT	0x20	/* AT/EISA transmit timed out */
#define	KBD_PS2_MOUSE_OFULL	0x20	/* PS/2 mouse output buffer full */
#define	KBD_RCV_TIMEOUT		0x40	/* receive (ps/2 general) timeout */
#define	KBD_PARITY_ERR		0x80	/* keyboard parity error */

/*
 * Keyboard controller command byte values
 */
#define	KBD_INTERRUPTS_ON	0x47
#define	KBD_INTERRUPTS_OFF	0x44

/*
 * Keyboard controller commands
 */
#define	CTLR_RAM_BASE_RD	0x20	/* 0x20-0x3f read controller RAM */
#define	CTLR_RAM_BASE_WR	0x60	/* 0x60-0x7f write controller RAM */
#define	CTLR_CMD		0x60	/* keyboard controller command byte */
#define	CHK_PWD_INSTALLED	0xa4	/* check if password installed */
#define	LOAD_PASSWORD		0xa5	/* load passwd */
#define	CHECK_PASSWD		0xa6	/* check password */
#define	DISABLE_MOUSE_PORT	0xa7	/* disable mouse port */
#define	ENABLE_MOUSE_PORT	0xa8	/* enable mouse port */
#define	TEST_MOUSE_PORT		0xa9	/* test mouse port */
#define	SELF_TEST		0xaa	/* self test */
#define	INTERFACE_TEST		0xab	/* interface test */
#define	DIAG_DUMP		0xac	/* diagnostic dump */
#define	DISABLE_KEYBOARD	0xad	/* disable keyboard */
#define	ENABLE_KEYBOARD		0xae	/* enable keyboard */
#define	RD_INPUT_PORT		0xc0	/* read input port */
#define	POLL_INPUT_LOW		0xc1	/* continuous input port poll, low */
#define	POLL_INPUT_HIGH		0xc2	/* continuous input port poll, high */
#define	RD_OUTPUT_PORT		0xd0	/* read output port */
#define	WR_OUTPUT_PORT		0xd1	/* write output port */
#define	WR_KBD_BUFFER		0xd2	/* write keyboard output buffer */
#define	WR_MOUSE_BUFFER		0xd3	/* write mouse output buffer */
#define	WR_MOUSE		0xd4	/* write to mouse */
#define	DISABLE_A20		0xdd	/* disable A20 address line */
#define	ENABLE_A20		0xdf	/* enable A20 address line */
#define	RD_TEST_INPUTS		0xe0	/* read test_inputs */
#define	PULSE_OUTPUT_BASE	0xf0	/* 0xf0-0xfd pulse output bits */
#define	SYSTEM_RESET		0xfe	/* reset the system */

/*
 * Keyboard and ps/2 mouse command/response bytes
 */
#define	KBD_ACK			0xfa	/* keyboard command/data acknowledge */

#define	SET_MOUSE_SCALE_1_1	0xe6	/* set mouse scaling to 1:1 */
#define	SET_MOUSE_SCALE_2_1	0xe7	/* set mouse scaling to 2:1 */
#define	SET_MOUSE_RES		0xe8	/* set mouse resolution */
#define	GET_MOUSE_INFO		0xe9	/* get mouse information */
#define	READ_MOUSE_ID		0xf2	/* read mouse id */
#define	SET_MOUSE_RATE		0xf3	/* set mouse sample rate */
#define	MOUSE_ENABLE		0xf4	/* enable mouse */
#define	MOUSE_DISABLE		0xf5	/* disable mouse and set defaults */
#define	MOUSE_RESET		0xff	/* reset mouse */

#define	SET_LEDS		0xed	/* set keyboard leds */
#define	SET_GET_SCAN_CODES	0xf0	/* set/get alternate scan codes */
#define	READ_KBD_ID		0xf2	/* read keyboard id */
#define	SET_TYPEMATIC		0xf3	/* set typematic information */
#define	KBD_ENABLE		0xf4	/* enable keyboard */
#define	KBD_DISABLE		0xf5	/* disable keyboard and set defaults */
#define	KBD_SET_DEFAULTS	0xf6	/* set keyboard to default state */
#define	SETKEYS_TYPEMATIC	0xf7	/* set all keys to typematic */
#define	SETKEYS_MAKE_BREAK	0xf8	/* set all keys to make/break */
#define	SETKEYS_MAKE		0xf9	/* set all keys to make */
#define	SETKEYS_TYPE_MAKE_BREAK	0xfa	/* all keys to typematic/make/break */
#define	SETKEY_TYPEMATIC	0xfb	/* set one key to typematic */
#define	SETKEY_MAKE_BREAK	0xfc	/* set one key to make/break */
#define	SETKEY_MAKE		0xfe	/* resend */
#define	KBD_RESET		0xff	/* reset keyboard */

#define	BIOS_RESET_PS2_MOUSE	0xc201	/* reset mouse bios command */

/*
 * function to write a command to the keyboard controller
 */
int
kbd_cmd_wr(unsigned char adr, unsigned char c)
{
	int i;

	_asm { cli };
	/*
	 * Wait for controller buffer to be empty, time out after
	 * 1/4 of a second.
	 */
	for (i = 0; (inp(KBD_CTL_STAT) & KBD_IBUF_FULL); i++) {
		if (i == 250)
			goto fail;
		else
			delay(1);
	}
	delay(1);
	outp(KBD_CTL_CMD, adr);
	delay(1);
	for (i = 0; (inp(KBD_CTL_STAT) & KBD_IBUF_FULL); i++) {
		if (i == 250)
			goto fail;
		else
			delay(1);
	}
	delay(1);
	outp(KBD_DATA, c);
	for (i = 0; (inp(KBD_CTL_STAT) & KBD_IBUF_FULL); i++) {
		if (i == 250)
			goto fail;
		else
			delay(1);
	}

	_asm { sti };
	return (0);

fail:
	_asm { sti };
	return (-1);
}

/*
 * Function to read a byte from the ps/2 mouse, returns byte read if
 * successful, returns 0xff if it times out after 1/4 of a second.
 */
static unsigned char
ps2_readmouse(void)
{
	int i;
	extern void delay();

	/*
	 * Wait for controller buffer to have a mouse character, time out after
	 * 1/4 of a second.
	 */
	for (i = 0; (inp(KBD_CTL_STAT) & (KBD_OBUF_FULL | KBD_PS2_MOUSE_OFULL))
			!= (KBD_OBUF_FULL | KBD_PS2_MOUSE_OFULL); i++) {
		if (i == 250)
			return (0xff);
		else
			delay(1);
	}
	delay(1);
	return (inp(KBD_DATA));
}

/*
 * Function to send a command to the ps2 mouse, returns 0 if successful,
 * returns -1 if it times out or, does not receive an ack for the command.
 */
static int
ps2_mousecmd(unsigned char c)
{
	int i;
	unsigned char ack;
	extern void delay();

	/*
	 * Wait for controller buffer to be empty, time out after
	 * 1/4 of a second.
	 */
	for (i = 0; (inp(KBD_CTL_STAT) & KBD_IBUF_FULL); i++) {
		if (i == 250)
			return (-1);
		else
			delay(1);
	}
	delay(1);
	/*
	 * Tell controller we want to send to the mouse
	 */
	outp(KBD_CTL_CMD, WR_MOUSE);
	/*
	 * Wait for controller buffer to be empty, time out after
	 * 1/4 of a second.
	 */
	for (i = 0; (inp(KBD_CTL_STAT) & KBD_IBUF_FULL); i++) {
		if (i == 250)
			return (-1);
		else
			delay(1);
	}

	/*
	 * send the command to the mouse
	 */
	delay(1);
	outp(KBD_CMD, c);
	/*
	 * Wait for command to be sent, time out after
	 * 1/4 of a second.
	 */
	for (i = 0; (inp(KBD_CTL_STAT) & KBD_IBUF_FULL); i++) {
		if (i == 250)
			return (-1);
		else
			delay(1);
	}
	ack = ps2_readmouse();
	if (ack != KBD_ACK)
		return (-1);
	return (0);
}

/*
 * Dummy ps/2 mouse handler
 */
static void
ps2ms_hdlr()
{
}

/*
 * Probe for a ps/2 mouse, return 1 if mouse found, else return 0.
 * XXX - NOTE if there is a ps/2 mouse port with no mouse attached,
 * XXX - this function will return 0.  Which means no resources will be
 * XXX - reserved for the mouse.  For the I/O ports this is ok as they are
 * XXX - shared with the keyboard anyway.  The only problem could come with
 * XXX - the IRQ since it may be used later by some other device even though
 * XXX - The ps/2 controller also has the same IRQ connected.  We don't
 * XXX - expect this to cause problems since the mouse should be disabled
 * XXX - in this situation anyway.  If problems do arise, the IRQ can always
 * XXX - be reserved by a dummy device if necessary.
 */
static int
probe_ps2mouse(void)
{
	union _REGS inregs, outregs;
	unsigned char mouse_resp;
	long nbuttons;
	long id;
	char far *fptr;
	int plen, flgs, ret, i;
	unsigned char cmd, ncmd;
	void _far *hdlr;
	unsigned char rv;
	char _far *bptr;
	char _far *sptr;

	ret = 0;

	/*
	 * boot.bin disables keyboard/mouse interrupts.  We turn them
	 * back on while we're doing mouse BIOS stuff, since the BIOS
	 * might depend on them.
	 */
	(void) kbd_cmd_wr(CTLR_CMD, KBD_INTERRUPTS_ON);

	hdlr = ps2ms_hdlr;
	/*
	 * Install a mouse device handler.
	 * If this is not a supported BIOS call, can't have a ps/2 mouse.
	 */
	_asm {
		push	es
		push	bx
		mov	es, word ptr [hdlr+2]
		mov	bx, word ptr [hdlr];	es:bx is handler address
		mov	ax, 0C207h
		int	15h;			Install the handler
		mov	rv, ah;			Set return value
		pushf
		pop	ax
		mov	flgs, ax;		Get flags register
		pop	bx
		pop	es
	}

	if ((flgs & 0x1) || rv != 0)
		goto fail;

	/*
	 * Gross kludge, A Micron Pentium EISA/PCI machine is known to
	 * reset if the following BIOS call is made.  Check for a string
	 * At a specific BIOS location to filter out this machine.  And
	 * unfortunately possibly lots of other machines.
	 */
	bptr = (char _far *)0xf00086d6;
	sptr = "1.00-M5PE-03";
	for (i = 0; i < 12; i++)
		if (bptr[i] != sptr[i])
			break;
	if (i == 12)
		goto fail;

	inregs.x.ax = BIOS_RESET_PS2_MOUSE;
	(void) _int86(0x15, &inregs, &outregs);
	if (outregs.x.cflag || outregs.h.ah != 0)
		goto fail;
	id = outregs.h.bh;

	/*
	 * OK, we've got a mouse.  Now try to figure out how many buttons.
	 */
	ret = 1;

	/*
	 * assume at least a 2 button ps/2 mouse
	 */
	nbuttons = 2;

	/*
	 * We're done doing BIOS stuff.  Turn interrupts back off, the way
	 * that the rest of the boot subsystem wants them.
	 */
	(void) kbd_cmd_wr(CTLR_CMD, KBD_INTERRUPTS_OFF);

	/*
	 * Send magic Logitech PS/2 mouse id sequence.
	 * If it is a Logitech PS/2 style mouse, it will return
	 * its number of buttons to us in the middle byte of
	 * the info returned by the final GET_MOUSE_INFO command.
	 */
	if (ps2_mousecmd(MOUSE_DISABLE) < 0)
		goto seqfailed;
	if (ps2_mousecmd(SET_MOUSE_RES) < 0)
		goto seqfailed;
	if (ps2_mousecmd(0x00) < 0)
		goto seqfailed;
	if (ps2_mousecmd(SET_MOUSE_SCALE_1_1) < 0)
		goto seqfailed;
	if (ps2_mousecmd(SET_MOUSE_SCALE_1_1) < 0)
		goto seqfailed;
	if (ps2_mousecmd(SET_MOUSE_SCALE_1_1) < 0)
		goto seqfailed;
	if (ps2_mousecmd(GET_MOUSE_INFO) < 0)
		goto seqfailed;
	(void) ps2_readmouse(); /* discard first info byte */
	mouse_resp = ps2_readmouse();
	(void) ps2_readmouse(); /* discard third info byte */
	if (mouse_resp)
		nbuttons = mouse_resp;
seqfailed:

	plen = 4;
	fptr = (char far *)&id;
	set_prop("ps2-mouse-id", &fptr, &plen, PROP_BIN);
	plen = 4;
	fptr = (char far *)&nbuttons;
	set_prop("mouse-nbuttons", &fptr, &plen, PROP_BIN);
	return (ret);

fail:
	/*
	 * Probe failed, no mouse.
	 *
	 * Re-disable keyboard/mouse interrupts
	 */
	(void) kbd_cmd_wr(CTLR_CMD, KBD_INTERRUPTS_OFF);
	return (ret);
}

int
dispatch(int func)
{
	/*
	 *  Driver Function Dispatcher:
	 *
	 *  This is the realmode driver equivalent of a "main" routine.  We use
	 *  the "func"tion code to determine the nature of the processing to
	 *  be undertaken ...
	 */

	char far *fptr;
	int plen;

	if (func != BEF_LEGACYPROBE)
		return (BEF_BADFUNC);	/* Not an operation we support! */
	/*
	 *  Caller wants to probe for the keyboard.  Actually, we don't
	 *  probe;  We simply assume it's installed, then look in the
	 *  BIOS to determine the device type.
	 */
	if (node_op(NODE_START) != NODE_OK)
		return (BEF_FAIL); /* can't create the node */

	/*
	 * Set our bus type.
	 */
	if (set_res("name", name, &namecnt, RES_SHARE) != RES_OK)
		return (BEF_FAIL); /* can't create the node */

	/*
	 * Set our slot number.
	 */
	if (set_res("slot", slot, &slotcnt, RES_SHARE) != RES_OK)
		return (BEF_FAIL); /* can't create the node */

	/*
	 * We've created a node, now reserve the resources for
	 * the ps/2 mouse if it is attached.
	 * Since the keyboard uses the same i/o ports we must
	 * set the RES_SHARE flag so that our node will be
	 * created.
	 */
	/*
	 * First, see if we have a ps/2 mouse
	 */
	if (!probe_ps2mouse())
		goto fail;
	if (set_res("port", ports, &portcnt, RES_SHARE) != RES_OK)
		goto fail;
	if (set_res("irq", irqs, &irqcnt, RES_SHARE) != RES_OK)
		goto fail;

	fptr = (char far *)device_type;
	plen = sizeof (device_type);
	(void) set_prop("device_type", &fptr, &plen, 0);

	fptr = (char far *)compatible;
	plen = sizeof (compatible);
	(void) set_prop("compatible", &fptr, &plen, 0);

	node_op(NODE_DONE|NODE_UNIQ);
	return (BEF_OK);

fail:
	node_op(NODE_FREE);
	return (BEF_OK);
}
