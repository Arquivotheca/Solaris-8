/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vuidps2.c	1.15	99/04/19 SMI"

/*
 * 			2/3 Button PS/2 Mouse Protocol
 *
 * This module dynamically determines the number of buttons on the mouse.
 */

#include <sys/param.h>
#include <sys/stream.h>
#include <sys/vuid_event.h>
#include <sys/vuidmice.h>
#include <sys/mouse.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

/*
 * BUT(1)		LEFT   BUTTON
 * BUT(2)		MIDDLE BUTTON (if present)
 * BUT(3)		RIGHT  BUTTON
 */

#define	PS2_BUTTONMASK		7		/* mask byte zero with this */

#define	PS2_BUTTON_L		(unchar)0x01	/* Left button pressed */
#define	PS2_BUTTON_R		(unchar)0x02	/* Right button pressed */
#define	PS2_BUTTON_M		(unchar)0x04	/* Middle button pressed */
#define	PS2_DATA_XSIGN		(unchar)0x10	/* X data sign bit */
#define	PS2_DATA_YSIGN		(unchar)0x20	/* Y data sign bit */

#define	PS2_START		0		/* Beginning of packet	*/
#define	PS2_BUTTON		1		/* Got button status	*/
#define	PS2_MAYBE_REATTACH	2		/* Got button status	*/
#define	PS2_DELTA_X		3		/* Got delta X		*/
#define	PS2_WAIT_RESET_ACK	4
#define	PS2_WAIT_RESET_AA	5
#define	PS2_WAIT_RESET_00	6
#define	PS2_WAIT_SETRES0_ACK1	7
#define	PS2_WAIT_SETRES0_ACK2	8	/* -+ These must be consecutive */
#define	PS2_WAIT_SCALE1_1_ACK	9	/*  | */
#define	PS2_WAIT_SCALE1_2_ACK	10	/*  | */
#define	PS2_WAIT_SCALE1_3_ACK	11	/* -+ */
#define	PS2_WAIT_STATREQ_ACK	12
#define	PS2_WAIT_STATUS_1	13
#define	PS2_WAIT_STATUS_BUTTONS	14
#define	PS2_WAIT_STATUS_REV	15
#define	PS2_WAIT_STATUS_3	16
#define	PS2_WAIT_SETRES3_ACK1	17
#define	PS2_WAIT_SETRES3_ACK2	18
#define	PS2_WAIT_STREAM_ACK	19
#define	PS2_WAIT_ON_ACK		20

#define	MSE_AA		0xaa
#define	MSE_00		0x00

extern void VUID_PUTNEXT(queue_t *const, unchar, unchar, unchar, int);

static void
sendButtonEvent(queue_t *const qp)
{
	static bmap[3] = {1, 3, 2};
	unsigned b;

	/* for each button, see if it has changed */
	for (b = 0; b < STATEP->nbuttons; b++) {
		unchar	mask = 0x1 << b;

		if ((STATEP->buttons&mask) != (STATEP->oldbuttons&mask))
			VUID_PUTNEXT(qp, BUT(bmap[b]), FE_PAIR_NONE, 0,
				(STATEP->buttons & mask ? 1 : 0));
	}
}

#define	BUGWARNING	void	/* These return values *should* be checked */

static boolean_t
put1(queue_t *const qp, int c)
{
	mblk_t *bp;

	if (bp = allocb(1, BPRI_MED)) {
		*bp->b_wptr++ = (char)c;
		putnext(qp, bp);
		return (B_TRUE);
	} else
		return (B_FALSE);
}

void
VUID_OPEN(queue_t *const qp)
{
	STATEP->format = VUID_FIRM_EVENT;
	(BUGWARNING)put1(WR(qp), MSERESET);
	STATEP->state = PS2_WAIT_RESET_ACK;
	while (STATEP->state != PS2_START) {
		if (qwait_sig(qp) == 0)
			break;
	}
}

void
VUID_QUEUE(queue_t *const qp, mblk_t *mp)
{
	int code;
	unsigned long now;
	unsigned long elapsed;
	unsigned long mouse_timeout;

	mouse_timeout = drv_usectohz(250000);
	now = ddi_get_lbolt();
	elapsed = now - STATEP->last_event_lbolt;
	STATEP->last_event_lbolt = now;

	while (mp->b_rptr < mp->b_wptr) {
		code = *mp->b_rptr++;

		switch (STATEP->state) {

		/*
		 * Start state. We stay here if the start code is not
		 * received thus forcing us back into sync. When we get a
		 * start code the button mask comes with it forcing us to
		 * to the next state.
		 */
restart:
		case PS2_START:

			if (!STATEP->inited) {
			    STATEP->sync_byte = code & 0x8;
			    STATEP->inited = 1;
			}
		/*
		 * the PS/2 mouse data format doesn't have any sort of sync
		 * data to make sure we are in sync with the packet stream,
		 * but the Technical Reference manual states that bits 2 & 3
		 * of the first byte are reserved.  Logitech uses bit 2 for
		 * the middle button.  We HOPE that noone uses bit 3 though,
		 * and decide we're out of sync if bit 3 is not set here.
		 */

			if ((code ^ STATEP->sync_byte) & 0x08) {
				/* bit 3 not set */
				STATEP->state = PS2_START;
				break;			/* toss the code */
			}

			/* get the button values */
			STATEP->buttons = code & PS2_BUTTONMASK;
			if (STATEP->buttons != STATEP->oldbuttons) {
				sendButtonEvent(qp);
				STATEP->oldbuttons = STATEP->buttons;
			}

			/* bit 5 indicates Y value is negative (the sign bit) */
			if (code & PS2_DATA_YSIGN)
				STATEP->deltay = -1 & ~0xff;
			else
				STATEP->deltay = 0;

			/* bit 4 is X sign bit */
			if (code & PS2_DATA_XSIGN)
				STATEP->deltax = -1 & ~0xff;
			else
				STATEP->deltax = 0;

			if (code == MSE_AA)
				STATEP->state = PS2_MAYBE_REATTACH;
			else
				STATEP->state = PS2_BUTTON;

			break;

		case PS2_MAYBE_REATTACH:
			if (code == MSE_00) {
				(BUGWARNING)put1(WR(qp), MSERESET);
				STATEP->state = PS2_WAIT_RESET_ACK;
				break;
			}
			/* FALLTHROUGH */

		case PS2_BUTTON:
			/*
			 * Now for the 7 bits of delta x.  "Or" in
			 * the sign bit and continue.  This is ac-
			 * tually a signed 9 bit number, but I just
			 * truncate it to a signed char in order to
			 * avoid changing and retesting all of the
			 * mouse-related modules for this patch.
			 */

			if (elapsed > mouse_timeout) goto restart;
			STATEP->deltax |= code & 0xff;
			STATEP->state = PS2_DELTA_X;
			break;

		case PS2_DELTA_X:
		/* The last part is delta Y, and the packet is complete */
			if (elapsed > mouse_timeout) goto restart;
			STATEP->deltay |= code & 0xff;

			STATEP->state = PS2_START;
			/*
			 * If we can peek at the next mouse  character,  and
			 * its not the start of the next packet,  don't  use
			 * this packet.
			 */

			if ((mp->b_wptr - mp->b_rptr) > 0 &&
			    ((mp->b_rptr[0] ^ STATEP->sync_byte) & 0x08)) {
			    /* bit 3 not set */
			    break;
			}

			/*
			 * send the info to the next level --
			 * need to send multiple events if we have both
			 * a delta *AND* button event(s)
			 */

			/* motion has occurred ... */
			if (STATEP->deltax)
				VUID_PUTNEXT(qp, LOC_X_DELTA,
					FE_PAIR_ABSOLUTE, LOC_X_ABSOLUTE,
					STATEP->deltax);

			if (STATEP->deltay)
				VUID_PUTNEXT(qp, LOC_Y_DELTA,
					FE_PAIR_ABSOLUTE, LOC_Y_ABSOLUTE,
					STATEP->deltay);

			STATEP->deltax = STATEP->deltay = 0;
			break;

		case PS2_WAIT_RESET_ACK:
			if (code != MSE_ACK)
				break;
			STATEP->state = PS2_WAIT_RESET_AA;
			break;

		case PS2_WAIT_RESET_AA:
			if (code != MSE_AA)
				break;
			STATEP->state = PS2_WAIT_RESET_00;
			break;

		case PS2_WAIT_RESET_00:
			if (code != MSE_00)
				break;
			(BUGWARNING)put1(WR(qp), MSESETRES);
			STATEP->state = PS2_WAIT_SETRES0_ACK1;
			break;

		case PS2_WAIT_SETRES0_ACK1:
			if (code != MSE_ACK)
				break;
			(BUGWARNING)put1(WR(qp), 0);
			STATEP->state = PS2_WAIT_SETRES0_ACK2;
			break;

		case PS2_WAIT_SETRES0_ACK2:
		case PS2_WAIT_SCALE1_1_ACK:
		case PS2_WAIT_SCALE1_2_ACK:
			if (code != MSE_ACK)
				break;
			(BUGWARNING)put1(WR(qp), MSESCALE1);
			STATEP->state++;
			break;

		case PS2_WAIT_SCALE1_3_ACK:
			if (code != MSE_ACK)
				break;
			(BUGWARNING)put1(WR(qp), MSESTATREQ);
			STATEP->state = PS2_WAIT_STATREQ_ACK;
			break;

		case PS2_WAIT_STATREQ_ACK:
			if (code != MSE_ACK)
				break;
			STATEP->state = PS2_WAIT_STATUS_1;
			break;

		case PS2_WAIT_STATUS_1:
			STATEP->state = PS2_WAIT_STATUS_BUTTONS;
			break;

		case PS2_WAIT_STATUS_BUTTONS:
			if (code != 0) {
				STATEP->nbuttons = (unchar)code;
				STATEP->state = (unchar)PS2_WAIT_STATUS_REV;
			} else {
#if	defined(VUID3PS2)
				/*
				 * It seems that there are some 3-button mice
				 * that don't play the Logitech autodetect
				 * game.  One is a Mouse Systems mouse OEM'ed
				 * by Intergraph.
				 *
				 * Until we find out how to autodetect these
				 * mice, we'll assume that if we're being
				 * compiled as vuid3ps2 and the mouse doesn't
				 * play the autodetect game, it's a 3-button
				 * mouse.  This effectively disables
				 * autodetect for mice using vuid3ps2, but
				 * since vuid3ps2 is used only on x86 where
				 * we currently assume manual configuration,
				 * this shouldn't be a problem.  At some point
				 * in the future when we *do* start using
				 * autodetect on x86, we should probably define
				 * VUIDPS2 instead of VUID3PS2.  Even then,
				 * we could leave this code so that *some*
				 * mice could use autodetect and others not.
				 */
				STATEP->nbuttons = 3;
#else
				STATEP->nbuttons = 2;
#endif
				STATEP->state = PS2_WAIT_STATUS_3;
			}
			break;

		case PS2_WAIT_STATUS_REV:
			/* FALLTHROUGH */
		case PS2_WAIT_STATUS_3:
			(BUGWARNING)put1(WR(qp), MSESETRES);
			STATEP->state = PS2_WAIT_SETRES3_ACK1;
			break;

		case PS2_WAIT_SETRES3_ACK1:
			if (code != MSE_ACK)
				break;
			(BUGWARNING)put1(WR(qp), 3);
			STATEP->state = PS2_WAIT_SETRES3_ACK2;
			break;

		case PS2_WAIT_SETRES3_ACK2:
			if (code != MSE_ACK)
				break;
			(BUGWARNING)put1(WR(qp), MSESTREAM);
			STATEP->state = PS2_WAIT_STREAM_ACK;
			break;

		case PS2_WAIT_STREAM_ACK:
			if (code != MSE_ACK)
				break;
			(BUGWARNING)put1(WR(qp), MSEON);
			STATEP->state = PS2_WAIT_ON_ACK;
			break;

		case PS2_WAIT_ON_ACK:
			if (code != MSE_ACK)
				break;
			STATEP->state = PS2_START;
			break;
		}
	}
	freemsg(mp);
}
