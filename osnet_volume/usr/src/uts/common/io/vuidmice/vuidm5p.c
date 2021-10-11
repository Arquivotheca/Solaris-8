/*
 * Copyright (c) 1993 Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma	ident "@(#)vuidm5p.c	1.3	94/10/25 SMI"

/*****************************************************************************/
/* 			5-Byte Mouse Protocol                                */
/*****************************************************************************/

#include <sys/param.h>
#include <sys/stream.h>
#include <sys/vuid_event.h>
#include <sys/vuidmice.h>

#define	LOGI_NUMBUTTONS		3		/* Number of buttons	*/

#define	LOGI_BMASK		(unchar)7	/* Button mask in packet*/
#define	LOGI_NOT_BMASK		(unchar)(~LOGI_BMASK) /* Rest of the bits */
#define	LOGI_START_CODE		(unchar)(0x80)	/* Start code in char	*/

#define	LOGI_START		0		/* Beginning of packet	*/
#define	LOGI_BUTTON		1		/* Got button status	*/
#define	LOGI_DELTA_X1		2		/* First of 2 delta X	*/
#define	LOGI_DELTA_Y1		3		/* First of 2 delta Y	*/
#define	LOGI_DELTA_X2		4		/* Second of 2 delta X	*/

extern void VUID_PUTNEXT(queue_t *const, unchar, unchar, unchar, int);

void
VUID_OPEN(queue_t *const qp)
{
	/*
	 * The current kdmconfig tables imply that this module can be used
	 * for both 2- and 3- button mice, so based on that evidence we
	 * can't assume a constant.  I don't know whether it's possible
	 * to autodetect.
	 */
	STATEP->nbuttons = 0;	/* Don't know. */
}

static void
vuidm5p_sendButtonEvent(queue_t *const qp)
{
	int b;

	/* for each button, see if it has changed */
	for(b=0; b<3; b++) {
		unchar	mask = 4 >> b;

		if ((STATEP->buttons&mask) != (STATEP->oldbuttons&mask))
			VUID_PUTNEXT(qp, BUT(b+1), FE_PAIR_NONE, 0,
			    (STATEP->buttons & mask ? 1 : 0));
	}
}

void
vuidm5p(queue_t *const qp, mblk_t *mp)
{
	int r, code;
	unsigned char *bufp;

    	bufp = mp->b_rptr;
	r = mp->b_wptr - mp->b_rptr;

    	for (r--; r >= 0; r--) {
		code = *bufp++;

	switch (STATEP->state) {
	/* Start state. We stay here if the start code is not received	*/
	/* thus forcing us back into sync. When we get a start code the	*/
	/* button mask comes with it forcing us to the next state.	*/
	default:
 	resync:
	case LOGI_START:
		if ((code & LOGI_NOT_BMASK) != LOGI_START_CODE)
			break;

		STATEP->state   = LOGI_BUTTON;
		STATEP->deltax  = STATEP->deltay = 0;
		STATEP->buttons = (~code) & LOGI_BMASK;	/* or xlate[code & ] */
		break;

	/* We receive the first of 2 delta x which forces us to the	*/
	/* next state. We just add the values of each delta x together.	*/
	case LOGI_BUTTON:
		if ((code & LOGI_NOT_BMASK) == LOGI_START_CODE) {
			STATEP->state = LOGI_START;
			goto resync;
		}

		/* The cast sign extends the 8-bit value. */
		STATEP->deltax += (signed char)code;
		STATEP->state = LOGI_DELTA_X1;
		break;

	/* The first of 2 delta y. We just add the 2 delta y together.	*/
	case LOGI_DELTA_X1:
		if ((code & LOGI_NOT_BMASK) == LOGI_START_CODE) {
			STATEP->state = LOGI_START;
			goto resync;
		}

		/* The cast sign extends the 8-bit value. */
		STATEP->deltay += (signed char)code;
		STATEP->state = LOGI_DELTA_Y1;
		break;

	/* The second of 2 delta x. We just add the 2 delta x together.	*/
	case LOGI_DELTA_Y1:
		if ((code & LOGI_NOT_BMASK) == LOGI_START_CODE) {
			STATEP->state = LOGI_START;
			goto resync;
		}

		/* The cast sign extends the 8-bit value. */
		STATEP->deltax += (signed char)code;
		STATEP->state = LOGI_DELTA_X2;
		break;

	/* The second of 2 delta y. We just add the 2 delta y together.	*/
	case LOGI_DELTA_X2:
		if ((code & LOGI_NOT_BMASK) == LOGI_START_CODE) {
			STATEP->state = LOGI_START;
			goto resync;
		}

		/* The cast sign extends the 8-bit value. */
		STATEP->deltay += (signed char)code;
		STATEP->state = LOGI_START;

		/* check if motion has occurred and send event(s)... */
		if ( STATEP->deltax )
			VUID_PUTNEXT(qp, LOC_X_DELTA, FE_PAIR_ABSOLUTE,
				LOC_X_ABSOLUTE, STATEP->deltax);
	
		if ( STATEP->deltay )
			VUID_PUTNEXT(qp, LOC_Y_DELTA, FE_PAIR_ABSOLUTE,
				LOC_Y_ABSOLUTE, STATEP->deltay);

		STATEP->deltax = STATEP->deltay = 0;

		/* see if the buttons have changed */
		if ( STATEP->buttons != STATEP->oldbuttons ) {
			/* buttons have changed */
			vuidm5p_sendButtonEvent(qp);

			/* update new button state */
			STATEP->oldbuttons = STATEP->buttons;
		}

	} /* end of switch */

	} /* end of for() loop */

	freemsg(mp);
}
