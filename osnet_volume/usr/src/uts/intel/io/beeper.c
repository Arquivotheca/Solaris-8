/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)beeper.c	1.9	99/05/04 SMI"

/*
 * Simple beeper support for PC platform, using standard timer 2 beeper.
 * Eventually this should probably be in a driver.
 */

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/beep.h>
#include <sys/ksynch.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#define	NELEM(a)	(sizeof (a) / sizeof ((a)[0]))

#define	TIMER		0x40
#define	TIMERCR		(TIMER+3)
#define	TIMER2		(TIMER+2)

#define	TONE_CTL	0x61
#define	T_CTLWORD	0xB6
#define	TONE_ON		0x03

static struct beep_state {
	enum {BEEP_OFF, BEEP_TIMED, BEEP_ON} state;
	kmutex_t mutex;
} beep_state;

static void beep_end(void *);
static void beep_on(void);
static void beep_off(void);
static void beep_frequency(int frequency);

struct beep_params {
	enum beep_type	type;
	int		frequency;	/* Hz */
	int		duration;	/* milliseconds */
};

struct beep_params beep_params[] = {
	BEEP_CONSOLE,	900,	200,
	BEEP_TYPE4,	2000,	0,
	BEEP_DEFAULT,	1000,	200,	/* Must be last */
};

/* ARGSUSED */
void
beep(enum beep_type type)
{
	struct beep_params *bp;

	/*
	 * In the fullness of time, we would use the "type" argument
	 * to determine frequency, volume, duration, waveform, sample
	 * to be played, ...
	 */

	for (bp = beep_params; bp->type != BEEP_DEFAULT; bp++) {
		if (bp->type == type)
			break;
	}

	mutex_enter(&beep_state.mutex);
	if (beep_state.state == BEEP_OFF) {
	    beep_state.state = BEEP_TIMED;
	    beep_frequency(bp->frequency);
	    beep_on();
	    (void) timeout(beep_end, &beep_state,
			drv_usectohz(bp->duration*1000));
	}
	mutex_exit(&beep_state.mutex);
}

/* ARGSUSED */
void
beep_polled(enum beep_type type)
{
	/* No-op at this time */
}

static void
beep_end(void *arg)
{
	struct beep_state *state = arg;

	/* Perhaps we should enforce a small "quiet" period between beeps. */
	mutex_enter(&state->mutex);
	state->state = BEEP_OFF;
	beep_off();
	mutex_exit(&state->mutex);
}

static void
beep_frequency(int frequency)
{
	int counter;

	counter = 1193180 / frequency;
	if (counter > 65535)
		counter = 65535;
	else if (counter < 1)
		counter = 1;

	outb(TIMERCR, T_CTLWORD);
	outb(TIMER2, counter & 0xff);
	outb(TIMER2, counter >> 8);
}

static void
beep_on(void)
{
	outb(TONE_CTL, inb(TONE_CTL) | TONE_ON);
}

static void
beep_off(void)
{
	outb(TONE_CTL, inb(TONE_CTL) & ~TONE_ON);
}

void
beeper_on(enum beep_type type)
{
	struct beep_params *bp;

	for (bp = beep_params; bp->type != BEEP_DEFAULT; bp++) {
		if (bp->type == type)
			break;
	}

	mutex_enter(&beep_state.mutex);
	if (beep_state.state == BEEP_OFF) {
		beep_state.state = BEEP_ON;
		beep_frequency(bp->frequency);
		beep_on();
	}
	mutex_exit(&beep_state.mutex);
}

void
beeper_off()
{
	mutex_enter(&beep_state.mutex);
	if (beep_state.state == BEEP_ON) {
		beep_state.state = BEEP_OFF;
		beep_off();
	}
	mutex_exit(&beep_state.mutex);
}
