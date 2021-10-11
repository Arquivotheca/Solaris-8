/*
 * Copyright (c) 1993 Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma	ident "@(#)vuidm3p.c	1.4	95/08/02 SMI"

/*****************************************************************************/
/* 			3-Byte Mouse Protocol                                */
/*****************************************************************************/
#include <sys/param.h>
#include <sys/stream.h>
#include <sys/vuid_event.h>
#include <sys/vuidmice.h>

#define	VUID_BUT(b)		BUT((b*2)+1)

/*
VUID_BUT(0)	BUT(1)		LEFT  BUTTON
VUID_BUT(1)	BUT(3)		RIGHT BUTTON
*/

#define	MOUSE_BUTTON_L		(unchar)(0x20)	/* Left button pressed	*/
#define	MOUSE_BUTTON_R		(unchar)(0x10)	/* Right button pressed	*/

#define	MOUSE_START_CODE	(unchar)(0x40)	/* Start code in char	*/

#define	MOUSE_START		0		/* Beginning of packet	*/
#define	MOUSE_BUTTON		1		/* Got button status	*/
#define	MOUSE_DELTA_X		2		/* got delta X	        */

extern void VUID_PUTNEXT(queue_t *const, unchar, unchar, unchar, int);

void
VUID_OPEN(queue_t *const qp)
{
	STATEP->nbuttons = 2;
}

static void
vuidm3p_sendButtonEvent(queue_t *const qp)
{
	int b;

	if ((STATEP->buttons == 0x30) && (!STATEP->oldbuttons)) {
	    /*
	     * both buttons going down simultaneously means button
	     * two going down
	     */
	    vuidm3p_putnext(qp, MS_MIDDLE, FE_PAIR_NONE, 0, 1);
	    return;
	} else if ((!STATEP->buttons) && (STATEP->oldbuttons == 0x30)) {
	    /*
	     * both buttons going up simultaneously means button
	     * two going up
	     */
	    vuidm3p_putnext(qp, MS_MIDDLE, FE_PAIR_NONE, 0, 0);
	    return;
	}

	/* for each button, see if it has changed */
	for(b=0; b<2; b++) {
		unchar	mask = 0x20 >> b;

		if ((STATEP->buttons&mask) != (STATEP->oldbuttons&mask))
			VUID_PUTNEXT(qp, VUID_BUT(b), FE_PAIR_NONE, 0,
				(STATEP->buttons & mask ? 1 : 0));
	}
}

void
vuidm3p(queue_t *const qp, mblk_t *mp)
{
	int r, code;
	unsigned char *bufp;

    	bufp = mp->b_rptr;
	r = mp->b_wptr - mp->b_rptr;

    	for (r--; r >= 0; r--) {
		code = *bufp++;

		/* strip the high-order bit (mouse sends 7-bit data) */
		code &= 0x7f;

		switch (STATEP->state) {

		/*
		 * Start state. We stay here if the start code is not 
		 * received thus forcing us back into sync. When we 
		 * get a start code the	button mask comes with it
		 * forcing us to the next state.
		 */

		default:
		case MOUSE_START:
 start_code:
			STATEP->deltax = STATEP->deltay = 0;

			/* look for sync */
			if ((code & MOUSE_START_CODE) == 0)
				break;

			STATEP->buttons = code & 0x30;

			if ( STATEP->buttons != STATEP->oldbuttons ) {
				vuidm3p_sendButtonEvent (qp);
	
				/* remember state */
				STATEP->oldbuttons = STATEP->buttons;
			}
			
			/* bits 0 & 1 are bits 6 & 7 of X value */
			/* Sign extend them with the cast. */
			STATEP->deltax = (signed char)((code & 0x03) << 6);
			/* bits 2 & 3 are bits 6 & 7 of Y value */
			/* Sign extend them with the cast. */
			STATEP->deltay = (signed char)((code & 0x0c) << 4);
	
			STATEP->state = MOUSE_BUTTON;/* go to the next state */
			break;
	
		/* We receive the remaining 6 bits of delta x, forcing us to
		 * the next state. We just piece the value of delta x together.
		 */
		case MOUSE_BUTTON:
			if (code & MOUSE_START_CODE) {
				STATEP->state = MOUSE_START;
				goto start_code;	/* restart */
			}
	
			STATEP->deltax |= code & 0x3f;
			STATEP->state = MOUSE_DELTA_X;
			break;
	
		/* The last part of delta Y, and the packet *may be* complete */
		case MOUSE_DELTA_X:
			if (code & MOUSE_START_CODE) {
				STATEP->state = MOUSE_START;
				goto start_code;	/* restart */
			}
	
			STATEP->deltay |= code&0x3f;
	
			/* generate motion Event */
	
			if ( STATEP->deltax )
				VUID_PUTNEXT(qp,LOC_X_DELTA,FE_PAIR_ABSOLUTE,
					LOC_X_ABSOLUTE, STATEP->deltax);

			if ( STATEP->deltay )
				VUID_PUTNEXT(qp,LOC_Y_DELTA,FE_PAIR_ABSOLUTE,
					LOC_Y_ABSOLUTE, -STATEP->deltay);

			STATEP->deltax = STATEP->deltay = 0;
			break;
	
		} /* end of switch() loop */

	} /* end of for() loop */

	freemsg(mp);
}
