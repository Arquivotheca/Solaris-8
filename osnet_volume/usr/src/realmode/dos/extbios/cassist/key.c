/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Realmode pseudo-driver for keybaords:
 *
 *    This file contains a probe-only pseudo-bef that tries to determine what
 *    sort of keyboard (if any) is attached to the system.  Right now, it
 *    assumes there's a keyboard and merely checks to see if it's the 101 or
 *    84 key variety.
 *    We also attach a few relevant properties to the keyboard node to aid
 *    later device configuration, includeing keyboard layout that was set
 *    in the advanced menu.
 */

#ident "<@(#)key.c	1.25	99/03/19 SMI>"
#include <befext.h>
#include <biosmap.h>
#include <string.h>
#include <stdio.h>
#include <dos.h>

/* Keyboard resource definitions ... */
static unsigned long ports[] = { 0x60, 1, 0, 0x64, 1, 0 };
static unsigned long portcnt = sizeof (ports)/sizeof (ports[0]);

static unsigned long irqs[]  = { 1, 0 };
static unsigned long irqcnt  = sizeof (irqs)/sizeof (irqs[0]);

#define	EISAize(l1, l2, l3, n)				\
	((unsigned long)(l1 & 0x1f) << 2) |		\
	((unsigned long)(l2 & 0x18) >> 3) |		\
	((unsigned long)(l2 & 0x07) << 13) |		\
	((unsigned long)(l3 & 0x1f) << 8) |		\
	((unsigned long)(n & 0xff) << 24) |		\
	((unsigned long)(n & 0xff00) << 8)

static unsigned long slot[] = { 0 };
static unsigned long slotcnt  = sizeof (slot)/sizeof (slot[0]);

static unsigned long name[] = { EISAize('P','N','P',0x0303),  RES_BUS_I8042 };
static unsigned long namecnt  = sizeof (name)/sizeof (name[0]);

static char compatible[] = "pnpPNP,303";
static char device_type[] = "keyboard";

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

#define	BIOS_GET_PS2_MOUSE_ID	0xc204	/* get mouse id bios command */
#define	BIOS_GET_KBD_STATUS	0x01	/* get keyboard status command */
#define	BIOS_GET_KBD_CHAR	0x00	/* get char from keyboard command */

/*
 * Function to read a byte from the keyboard, returns byte read if
 * successful, returns 0xff if it times out after 1/4 of a second.
 */
static unsigned char
kbd_read(void)
{
	int i;

	/*
	 * Wait for controller buffer to have a character, time out after
	 * 1/4 of a second.
	 */
	for (i = 0; (inp(KBD_CTL_STAT) & KBD_OBUF_FULL) == 0; i++)
		if (i == 250)
			return (0xff);
		else
			delay(1);
	delay(1);
	return (inp(KBD_DATA));
}

/*
 * Function to send a command to the keyboard, returns 0 if successful,
 * returns -1 if it times out or, does not receive an ack for the command.
 */
static int
kbd_cmd(unsigned char c)
{
	int i;
	unsigned char ack;
	extern void delay();

	/*
	 * Wait for controller buffer to be empty, time out after
	 * 1/4 of a second.
	 */
	for (i = 0; (inp(KBD_CTL_STAT) & KBD_IBUF_FULL); i++)
		if (i == 250)
			return (-1);
		else
			delay(1);
	delay(1);
	outp(KBD_CMD, c);
	/*
	 * Wait for command to be sent, time out after
	 * 1/4 of a second.
	 */
	for (i = 0; (inp(KBD_CTL_STAT) & KBD_IBUF_FULL); i++)
		if (i == 250)
			return (-1);
		else
			delay(1);
	ack = kbd_read();
	if (ack != KBD_ACK)
		return (-1);
	return (0);
}

/*
 * Driver Function Dispatcher:
 *
 * This is the realmode driver equivalent of a "main" routine.  We use
 * the "func"tion code to determine the nature of the processing to
 * be undertaken ...
 */
int
dispatch(int func)
{
	union _REGS inregs, outregs;
	unsigned long val;
	long nkeys = 101;
	char far *fptr;
	int plen, i;
	char buf[8];
	unsigned char c;
	long id;
	int idok = 0;

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
		goto fail;

	/*
	 * Set our slot number.
	 */
	if (set_res("slot", slot, &slotcnt, RES_SHARE) != RES_OK)
		goto fail;

	/*
	 * We've created a node, now reserve the resources for
	 * the keyboard.
	 * Some vendors consider the keyboard to be part of
	 * the motherboard, and reserve it's resources to
	 * slot 0.  Since we want this device in the tree, we
	 * set the RES_SHARE flag so that our node will be
	 * there regardless.
	 * XXX - This may lead to problems in the future
	 * on systems with different keyboard hardware
	 * interfaces.  e.g. USB or Access.Bus systems.  If
	 * problems occur, we will need to recognize those
	 * situations in this bef and not create this node.
	 */
	if (set_res("port", ports, &portcnt, RES_SHARE) != RES_OK)
		goto fail;

	if (set_res("irq", irqs, &irqcnt, RES_SHARE) != RES_OK)
		goto fail;

	if (!bdap->FunctionKeys) {
		/*
		 * We've got a keyboard, but it has no
		 * function keys!  Change the number of
		 * keys to reflect this fact.
		 */
		nkeys = 84;
	}
	/*
	 * See if user says we have a 104 key keyboard
	 */
	(void) get_prop("kbd-wkeys", &fptr, &plen);
	if (strcmp(fptr, "true") == 0)
		nkeys = 104;
	plen = 4;
	fptr = (char far *)&nkeys;
	(void) set_prop("keyboard-nkeys", &fptr, &plen, PROP_BIN);
	fptr = (char far *)0;
	(void) get_prop("kbd-type", &fptr, &plen);
	if (fptr == (char far *)0) {
		/*
		 * Default to US-English if can't get
		 * a kbd-type
		 */
		fptr = "US-English";
		plen = strlen(fptr) + 1;
	}
	(void) set_prop("keyboard-type", &fptr, &plen, 0);

	fptr = (char far *)device_type;
	plen = sizeof (device_type);
	(void) set_prop("device_type", &fptr, &plen, 0);

	fptr = (char far *)compatible;
	plen = sizeof (compatible);
	(void) set_prop("compatible", &fptr, &plen, 0);

	/*
	 * collect keyboard id
	 *
	 * Note:  boot.bin disables keyboard and mouse interrupts.
	 */

	if (kbd_cmd(READ_KBD_ID) >= 0) {
		idok = 1;
		id = kbd_read();
		id |= (long)kbd_read() << 8;
	}
	/*
	 * Drain any keyboard junk
	 */
	for (i = 0; i < 20; i++)
		if ((c = kbd_read()) == 0xff)
			break;

	if (idok) {
		fptr = (char far *)&id;
		plen = 4;
		(void) set_prop("keyboard-id", &fptr, &plen, PROP_BIN);
	}
	/*
	 * Drain any keystrokes we may have caused that the BIOS
	 * is holding (This will also flush any user typeahead).
	 */
	delay(100);
	for (i = 0; i < 20; i++) {
		inregs.h.ah = BIOS_GET_KBD_STATUS;
		(void) _int86(0x16, &inregs, &outregs);
		/*
		 * Int 16 returns the flags reg in bx, test
		 * for ZF bit set to indicate no chars waiting
		 */
		if (outregs.x.bx & 0x40)
			break;
		inregs.h.ah = BIOS_GET_KBD_CHAR;
		(void) _int86(0x16, &inregs, &outregs);
		delay(100);
	}
#ifdef notyet
	/*
	 * Also clear any set shift/ctl flags
	 */
	bdap->RiteCtlDown = 0;
	bdap->KeyShiftFlgs[0] = 0;
	bdap->KeyShiftFlgs[1] = 0;
#endif
	node_op(NODE_DONE|NODE_UNIQ);
	return (BEF_OK);

fail:
	node_op(NODE_FREE);
	return (BEF_OK);
}
