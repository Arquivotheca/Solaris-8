/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)at_keyprocess.c	1.20	99/05/04 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/kbd.h>
#include <sys/kbtrans.h>
#include <sys/sunddi.h>
#include <sys/consdev.h>
#include "kb8042.h"

/*
 * A note on the use of prom_printf here:  Most of these routines can be
 * called from "polled mode", where we're servicing I/O requests from kadb.
 * Normal system services are not available from polled mode; cmn_err will
 * not work.  prom_printf is the only safe output mechanism.
 */
#if	defined(KD_DEBUG)
extern int kb8042_debug;
#define	DEBUG_KD(f)	if (kb8042_debug) prom_printf f; else
#else
#define	DEBUG_KD(f)	/* nothing */
#endif

#define	KEYBAD		0xff		/* should generate an error */
#define	KEYIGN		0xfe		/* ignore this sequence */

#define	KEY(code)	(code)
#define	INVALID		KEYBAD
#define	IGNORE		KEYIGN

/*
 * These are the states of our parsing machine:
 */
#define	STATE_IDLE	0x10000000	/* Awaiting the start of a sequence */
#define	STATE_E0	0x100000e0	/* Rec'd an E0 */
#define	STATE_E1	0x100000e1	/* Rec'd an E1 (Pause key only) */
#define	STATE_E1_14	0x1000e114	/* Rec'd an E1 14 (Pause key only) */

#define	NELEM(a)	(sizeof (a) / sizeof (a)[0])

#ifdef __ia64
/*
 * XXX Merced Fix Me
 * Remove when simulator correctly generates AT scan codes.
 * Parse table for the base keyboard state. It provides
 * XT scan code to function key mappings for the softsdv keyboard.
 */
static const unsigned char	keytab_base[] = {
/* scan		state		keycap */
/* 00 */	INVALID,
/* 01 */	KEY(110),	/* Esc */
/* 02 */	KEY(2),		/* 1 */
/* 03 */	KEY(3),		/* 2 */
/* 04 */	KEY(4),		/* 3 */
/* 05 */	KEY(5),		/* 4 */
/* 06 */	KEY(6),		/* 5 */
/* 07 */	KEY(7),		/* 6 */
/* 08 */	KEY(8),		/* 7 */
/* 09 */	KEY(9),		/* 8 */
/* 0a */	KEY(10),	/* 9 */
/* 0b */	KEY(11),	/* 0 */
/* 0c */	KEY(12),	/* - */
/* 0d */	KEY(13),	/* = */
/* 0e */	KEY(15),	/* backspace */
/* 0f */	KEY(16),	/* tab */
/* 10 */	KEY(17),	/* Q */
/* 11 */	KEY(18),	/* W */
/* 12 */	KEY(19),	/* E */
/* 13 */	KEY(20),	/* R */
/* 14 */	KEY(21),	/* T */
/* 15 */	KEY(22),	/* Y */
/* 16 */	KEY(23),	/* U */
/* 17 */	KEY(24),	/* I */
/* 18 */	KEY(25),	/* O */
/* 19 */	KEY(26),	/* P */
/* 1a */	KEY(27),	/* [ */
/* 1b */	KEY(28),	/* ] */
/* 1c */	KEY(43),	/* Enter (main) */
/* 1d */	KEY(58),	/* L Ctrl */
/* 1e */	KEY(31),	/* A */
/* 1f */	KEY(32),	/* S */
/* 20 */	KEY(33),	/* D */
/* 21 */	KEY(34),	/* F */
/* 22 */	KEY(35),	/* G */
/* 23 */	KEY(36),	/* H */
/* 24 */	KEY(37),	/* J */
/* 25 */	KEY(38),	/* K */
/* 26 */	KEY(39),	/* L */
/* 27 */	KEY(40),	/* ; */
/* 28 */	KEY(41),	/* ' */
/* 29 */	KEY(1),		/* ` */
/* 2a */	KEY(44),	/* L Shift */
/* 2b */	KEY(29),	/* \ */
/* 2c */	KEY(46),	/* Z */
/* 2d */	KEY(47),	/* X */
/* 2e */	KEY(48),	/* C */
/* 2f */	KEY(49),	/* V */
/* 30 */	KEY(50),	/* B */
/* 31 */	KEY(51),	/* N */
/* 32 */	KEY(52),	/* M */
/* 33 */	KEY(53),	/* , */
/* 34 */	KEY(54),	/* . */
/* 35 */	KEY(55),	/* / */
/* 36 */	KEY(57),	/* R Shift */
/* 37 */	IGNORE,		/* ignore */
/* 38 */	KEY(60),	/* L + R Alt */
/* 39 */	KEY(61),	/* Space */
/* 3a */	INVALID,
/* 3b */	INVALID,
/* 3c */	INVALID,
/* 3d */	INVALID,
/* 3e */	INVALID,
/* 3f */	INVALID,
/* 40 */	INVALID,
/* 41 */	INVALID,
/* 42 */	INVALID,
/* 43 */	INVALID,
/* 44 */	INVALID,
/* 45 */	INVALID,
/* 46 */	INVALID,
/* 47 */	INVALID,
/* 48 */	INVALID,
/* 49 */	INVALID,
/* 4a */	INVALID,
/* 4b */	INVALID,
/* 4c */	INVALID,
/* 4d */	INVALID,
/* 4e */	INVALID,
/* 4f */	INVALID,
/* 50 */	INVALID,
/* 51 */	INVALID,
/* 52 */	INVALID,
/* 53 */	KEY(76),	/* Delete */
/* 54 */	INVALID,
/* 55 */	INVALID,
/* 56 */	INVALID,
/* 57 */	INVALID,
/* 58 */	INVALID,
/* 59 */	INVALID,
/* 5a */	INVALID,
/* 5b */	INVALID,
/* 5c */	INVALID,
/* 5d */	INVALID,
/* 5e */	INVALID,
/* 5f */	INVALID,
/* 60 */	INVALID,
/* 61 */	INVALID,
/* 62 */	INVALID,
/* 63 */	INVALID,
/* 64 */	INVALID,
/* 65 */	INVALID,
/* 66 */	INVALID,
/* 67 */	KEY(58),	/* R Ctrl - may map to 1d */
/* 68 */	KEY(76),	/* Delete - may map to 5e */
/* 69 */	INVALID,
/* 6a */	INVALID,
/* 6b */	INVALID,
/* 6c */	INVALID,
/* 6d */	INVALID,
/* 6e */	INVALID,
/* 6f */	INVALID,
/* 70 */	IGNORE,
/* 71 */	INVALID,
/* 72 */	INVALID,
/* 73 */	INVALID,
/* 74 */	IGNORE,		/* ignore */
/* 75 */	IGNORE,		/* ignore */
/* 76 */	INVALID,
/* 77 */	INVALID,
/* 78 */	INVALID,
/* 79 */	INVALID,
/* 7a */	INVALID,
/* 7b */	INVALID,
/* 7c */	INVALID,
/* 7d */	INVALID,
/* 7e */	INVALID,
/* 7f */	INVALID,
};

#else /* __ia64 */
/*
 *	Parse table for the base keyboard state.  The index is the start of
 *	a new sequence.
 *
 * Questionable or unusual cases:
 * 02		Old kd code says F7. Manual says no.
 * 7f		Old kd code says this is an 84-key SysReq.  Manual says no.
 * 87		Old kd code says 1 (num).  Manual says no.
 * 8c		Old kd code says / (num).  Manual says no.
 * aa		POST OK.  Handled by code.
 * e0		Extend prefix.  Handled by code. (switches to E0 table)
 * e1		Extend prefix.  Handled by code.  (Pause key only)
 * f0		Break prefix.  Handled by code.
 * f1		Korean Hangul/Hanja key.  Handled by code.
 * f2		Korean Hangul key.  Handled by code.
 * ff		Keyboard internal buffer overrun.  Handled by code.
 *
 * Other values past the end of the table are treated as INVALID.
 */

static const unsigned char	keytab_base[] = {
/* scan		state		keycap */
/* 00 */	INVALID,
/* 01 */	KEY(120),	/* F9 */
/* 02 */	INVALID,	/* F7?  Old code says so but manual doesn't */
/* 03 */	KEY(116),	/* F5 */
/* 04 */	KEY(114),	/* F3 */
/* 05 */	KEY(112),	/* F1 */
/* 06 */	KEY(113),	/* F2 */
/* 07 */	KEY(123),	/* F12 */
/* 08 */	INVALID,
/* 09 */	KEY(121),	/* F10 */
/* 0a */	KEY(119),	/* F8 */
/* 0b */	KEY(117),	/* F6 */
/* 0c */	KEY(115),	/* F4 */
/* 0d */	KEY(16),	/* tab */
/* 0e */	KEY(1),		/* ` */
/* 0f */	INVALID,
/* 10 */	INVALID,
/* 11 */	KEY(60),	/* L Alt */
/* 12 */	KEY(44),	/* L Shift */
/* 13 */	KEY(133),	/* Japanese 106-key */
/* 14 */	KEY(58),	/* L Ctrl */
/* 15 */	KEY(17),	/* Q */
/* 16 */	KEY(2),		/* 1 */
/* 17 */	INVALID,
/* 18 */	INVALID,
/* 19 */	INVALID,
/* 1a */	KEY(46),	/* Z */
/* 1b */	KEY(32),	/* S */
/* 1c */	KEY(31),	/* A */
/* 1d */	KEY(18),	/* W */
/* 1e */	KEY(3),		/* 2 */
/* 1f */	INVALID,
/* 20 */	INVALID,
/* 21 */	KEY(48),	/* C */
/* 22 */	KEY(47),	/* X */
/* 23 */	KEY(33),	/* D */
/* 24 */	KEY(19),	/* E */
/* 25 */	KEY(5),		/* 4 */
/* 26 */	KEY(4),		/* 3 */
/* 27 */	INVALID,
/* 28 */	INVALID,
/* 29 */	KEY(61),	/* Space */
/* 2a */	KEY(49),	/* V */
/* 2b */	KEY(34),	/* F */
/* 2c */	KEY(21),	/* T */
/* 2d */	KEY(20),	/* R */
/* 2e */	KEY(6),		/* 5 */
/* 2f */	INVALID,
/* 30 */	INVALID,
/* 31 */	KEY(51),	/* N */
/* 32 */	KEY(50),	/* B */
/* 33 */	KEY(36),	/* H */
/* 34 */	KEY(35),	/* G */
/* 35 */	KEY(22),	/* Y */
/* 36 */	KEY(7),		/* 6 */
/* 37 */	INVALID,
/* 38 */	INVALID,
/* 39 */	INVALID,
/* 3a */	KEY(52),	/* M */
/* 3b */	KEY(37),	/* J */
/* 3c */	KEY(23),	/* U */
/* 3d */	KEY(8),		/* 7 */
/* 3e */	KEY(9),		/* 8 */
/* 3f */	INVALID,
/* 40 */	INVALID,
/* 41 */	KEY(53),	/* , */
/* 42 */	KEY(38),	/* K */
/* 43 */	KEY(24),	/* I */
/* 44 */	KEY(25),	/* O */
/* 45 */	KEY(11),	/* 0 */
/* 46 */	KEY(10),	/* 9 */
/* 47 */	INVALID,
/* 48 */	INVALID,
/* 49 */	KEY(54),	/* . */
/* 4a */	KEY(55),	/* / */
/* 4b */	KEY(39),	/* L */
/* 4c */	KEY(40),	/* ; */
/* 4d */	KEY(26),	/* P */
/* 4e */	KEY(12),	/* - */
/* 4f */	INVALID,
/* 50 */	INVALID,
/* 51 */	KEY(56),	/* Japanese 106-key */
/* 52 */	KEY(41),	/* ' */
/* 53 */	INVALID,
/* 54 */	KEY(27),	/* [ */
/* 55 */	KEY(13),	/* = */
/* 56 */	INVALID,
/* 57 */	INVALID,
/* 58 */	KEY(30),	/* CapsLock */
/* 59 */	KEY(57),	/* R Shift */
/* 5a */	KEY(43),	/* Enter (main) */
/* 5b */	KEY(28),	/* ] */
/* 5c */	INVALID,
/* 5d */	KEY(29),	/* \, key 42 for 102-key */
/* 5e */	INVALID,
/* 5f */	INVALID,
/* 60 */	INVALID,
/* 61 */	KEY(45),	/* 102-key only, typically </> */
/* 62 */	INVALID,
/* 63 */	INVALID,
/* 64 */	KEY(132),	/* Japanese 106-key */
/* 65 */	INVALID,
/* 66 */	KEY(15),	/* backspace */
/* 67 */	KEY(131),	/* Japanese 106-key */
/* 68 */	INVALID,
/* 69 */	KEY(93),	/* 1 (num) */
/* 6a */	KEY(14),	/* Japanese 106-key */
/* 6b */	KEY(92),	/* 4 (num) */
/* 6c */	KEY(91),	/* 7 (num) */
/* 6d */	INVALID,
/* 6e */	INVALID,
/* 6f */	INVALID,
/* 70 */	KEY(99),	/* 0 (num) */
/* 71 */	KEY(104),	/* . (num) */
/* 72 */	KEY(98),	/* 2 (num) */
/* 73 */	KEY(97),	/* 5 (num) */
/* 74 */	KEY(102),	/* 6 (num) */
/* 75 */	KEY(96),	/* 8 (num) */
/* 76 */	KEY(110),	/* Esc */
/* 77 */	KEY(90),	/* NumLock */
/* 78 */	KEY(122),	/* F11 */
/* 79 */	KEY(106),	/* + (num) */
/* 7a */	KEY(103),	/* 3 (num) */
/* 7b */	KEY(105),	/* - (num) */
/* 7c */	KEY(100),	/* * (num) */
/* 7d */	KEY(101),	/* 9 (num) */
/* 7e */	KEY(125),	/* Scroll Lock */
/* 7f */	INVALID,	/* 84-key SysReq?  Manual says no. */
/* 80 */	INVALID,
/* 81 */	INVALID,
/* 82 */	INVALID,
/* 83 */	KEY(118),	/* F7 */
/* 84 */	KEY(124),	/* PrintScreen (w/ Alt = SysRq) */
};
#endif /* __ia64 */

/*
 * Parse table after receiving an E0 prefix code.
 *
 * Generally speaking, keys that were added on the 101-key keyboard are
 * represented as an E0 followed by the code for an 84-key key.  Software
 * ignorant of the 101-key keyboard ignores the E0 and so is handled
 * compatibly.  Many of these variants involve "fake" shift presses
 * and releases for compatibility; these are also prefixed with E0.
 * We ignore these fake shifts.
 */
static const unsigned char	keytab_e0[] = {
/* 00 */	INVALID,
/* 01 */	INVALID,
/* 02 */	INVALID,
/* 03 */	INVALID,
/* 04 */	INVALID,
/* 05 */	INVALID,
/* 06 */	INVALID,
/* 07 */	INVALID,
/* 08 */	INVALID,
/* 09 */	INVALID,
/* 0a */	INVALID,
/* 0b */	INVALID,
/* 0c */	INVALID,
/* 0d */	INVALID,
/* 0e */	INVALID,
/* 0f */	INVALID,
/* 10 */	INVALID,
/* 11 */	KEY(62),	/* R Alt */
/* 12 */	IGNORE,		/* Fake L Shift */
/* 13 */	INVALID,
/* 14 */	KEY(64),	/* R Ctrl */
/* 15 */	INVALID,
/* 16 */	INVALID,
/* 17 */	INVALID,
/* 18 */	INVALID,
/* 19 */	INVALID,
/* 1a */	INVALID,
/* 1b */	INVALID,
/* 1c */	INVALID,
/* 1d */	INVALID,
/* 1e */	INVALID,
/* 1f */	KEY(59),	/* L Window (104-key) */
/* 20 */	INVALID,
/* 21 */	INVALID,
/* 22 */	INVALID,
/* 23 */	INVALID,
/* 24 */	INVALID,
/* 25 */	INVALID,
/* 26 */	INVALID,
/* 27 */	KEY(63),	/* R Window (104-key) */
/* 28 */	INVALID,
/* 29 */	INVALID,
/* 2a */	INVALID,
/* 2b */	INVALID,
/* 2c */	INVALID,
/* 2d */	INVALID,
/* 2e */	INVALID,
/* 2f */	KEY(65),	/* Menu (104-key) */
/* 30 */	INVALID,
/* 31 */	INVALID,
/* 32 */	INVALID,
/* 33 */	INVALID,
/* 34 */	INVALID,
/* 35 */	INVALID,
/* 36 */	INVALID,
/* 37 */	INVALID,
/* 38 */	INVALID,
/* 39 */	INVALID,
/* 3a */	INVALID,
/* 3b */	INVALID,
/* 3c */	INVALID,
/* 3d */	INVALID,
/* 3e */	INVALID,
/* 3f */	INVALID,
/* 40 */	INVALID,
/* 41 */	INVALID,
/* 42 */	INVALID,
/* 43 */	INVALID,
/* 44 */	INVALID,
/* 45 */	INVALID,
/* 46 */	INVALID,
/* 47 */	INVALID,
/* 48 */	INVALID,
/* 49 */	INVALID,
/* 4a */	KEY(95),	/* / (num) */
/* 4b */	INVALID,
/* 4c */	INVALID,
/* 4d */	INVALID,
/* 4e */	INVALID,
/* 4f */	INVALID,
/* 50 */	INVALID,
/* 51 */	INVALID,
/* 52 */	INVALID,
/* 53 */	INVALID,
/* 54 */	INVALID,
/* 55 */	INVALID,
/* 56 */	INVALID,
/* 57 */	INVALID,
/* 58 */	INVALID,
/* 59 */	IGNORE,		/* Fake R Shift */
/* 5a */	KEY(108),	/* Enter (num) */
/* 5b */	INVALID,
/* 5c */	INVALID,
/* 5d */	INVALID,
/* 5e */	INVALID,
/* 5f */	INVALID,
/* 60 */	INVALID,
/* 61 */	INVALID,
/* 62 */	INVALID,
/* 63 */	INVALID,
/* 64 */	INVALID,
/* 65 */	INVALID,
/* 66 */	INVALID,
/* 67 */	INVALID,
/* 68 */	INVALID,
/* 69 */	KEY(81),	/* End (arrow) */
/* 6a */	INVALID,
/* 6b */	KEY(79),	/* Left (arrow) */
/* 6c */	KEY(80),	/* Home (arrow) */
/* 6d */	INVALID,
/* 6e */	INVALID,
/* 6f */	INVALID,
/* 70 */	KEY(75),	/* Insert (arrow) */
/* 71 */	KEY(76),	/* Delete (arrow) */
/* 72 */	KEY(84),	/* Down (arrow) */
/* 73 */	INVALID,
/* 74 */	KEY(89),	/* Right (arrow) */
/* 75 */	KEY(83),	/* Up (arrow) */
/* 76 */	INVALID,
/* 77 */	INVALID,
/* 78 */	INVALID,
/* 79 */	INVALID,
/* 7a */	KEY(86),	/* PgDn (arrow) */
/* 7b */	INVALID,
/* 7c */	KEY(124),	/* PrintScreen (no Alt) */
/* 7d */	KEY(85),	/* PgUp (arrow) */
/* 7e */	KEY(126),	/* Pause (w/Ctrl = Break) */
};

/*
 * Initialize the translation state machine.
 */
void
atKeyboardConvertScan_init(struct kb8042 *kb8042)
{
	kb8042->parse_scan.break_prefix_received = KEY_PRESSED;
	kb8042->parse_scan.state = STATE_IDLE;
}

/*
 *	atKeyboardConvertScan(kb8042, scan, keynum, up,
 *		synthetic_release_needed)
 *
 *	State machine that takes scan codes from the keyboard and resolves
 *	them to key numbers using the above tables.  Returns B_TRUE if this
 *	scan code completes a scan code sequence, in which case "keynum"
 *	and "state" will be filled in correctly.
 *
 *	synthetic_release_needed is a total hack to handle the
 *	additional two keys on a Korean keyboard.  They report press
 *	only, so we synthesize releases.
 */
boolean_t
atKeyboardConvertScan(
    struct kb8042	*kb8042,
    unsigned char	scan,
    int			*keynum,
    enum keystate	*state,
    boolean_t		*synthetic_release_needed)
{
	int	entry;

	DEBUG_KD(("atKeyboardConvertScan AT 0x%x", scan));

	*synthetic_release_needed = B_FALSE;

#ifdef __ia64
	/*
	 * XXX Merced Fix Me
	 */

	/* Check if key got released */
	if (scan & 0x80) {
		kb8042->parse_scan.break_prefix_received = KEY_RELEASED;
		DEBUG_KD(("-> break prefix\n"));
	}
	scan &= 0x7F;

	/*
	 * Don't worry about special cases because they all need the
	 * high bit set.
	 */
#endif /* __ia64 */

	/*
	 * First, handle special cases.
	 *
	 * ACK has already been handled by our caller.
	 */
	switch (scan) {
	case KAT_BREAK:
		kb8042->parse_scan.break_prefix_received = KEY_RELEASED;
		DEBUG_KD(("-> break prefix\n"));
		return (B_FALSE);	/* not a final keycode */
	case KB_ERROR:
		/*
		 * Perhaps we should reset state here,
		 * since we no longer know what's going on.
		 */
		DEBUG_KD(("-> overrun\n"));
		return (B_FALSE);
	case KB_POST_OK:
	case KB_POST_FAIL:
		/*
		 * Perhaps we should reset the LEDs now.
		 * If so, this check should probably be in the main line.
		 * Perhaps we should tell the higher layers that the
		 * keyboard has been reset.
		 */
		/*
		 * Reset to idle
		 */
		kb8042->parse_scan.state = STATE_IDLE;
		kb8042->parse_scan.break_prefix_received = KEY_PRESSED;
		DEBUG_KD(("-> POST %s\n", scan == KB_POST_OK ? "OK" : "FAIL"));
		return (B_FALSE);
	}

	switch (kb8042->parse_scan.state) {
	case STATE_IDLE:
		switch (scan) {
		case KAT_EXTEND:
			kb8042->parse_scan.state = STATE_E0;
			DEBUG_KD(("-> state E0\n"));
			return (B_FALSE);

		case KAT_EXTEND2:
			kb8042->parse_scan.state = STATE_E1;
			DEBUG_KD(("-> state E1\n"));
			return (B_FALSE);

		/*
		 * We could do the next two in the table, but it would
		 * require nearly doubling the size of the table.
		 *
		 * Also, for some stupid reason these two report presses
		 * only.  We tell the upper layer to synthesize a release.
		 */
		case KAT_HANGUL_HANJA:
			entry = KEY(150);
			*synthetic_release_needed = B_TRUE;
			break;

		case KAT_HANGUL:
			entry = KEY(151);
			*synthetic_release_needed = B_TRUE;
			break;

		default:
			/*
			 * Regular scan code
			 */
			if (scan < NELEM(keytab_base))
				entry = keytab_base[scan];
			else
				entry = INVALID;
			break;
		}
		break;

	case STATE_E0:		/* Mostly 101-key additions */
		if (scan < NELEM(keytab_e0))
			entry = keytab_e0[scan];
		else
			entry = INVALID;
		break;

	case STATE_E1:		/* Pause key only */
		switch (scan) {
		case 0x14:
			kb8042->parse_scan.state = STATE_E1_14;
			DEBUG_KD(("-> state E1 14\n"));
			return (B_FALSE);
		default:
			entry = INVALID;
			break;
		}
		break;

	case STATE_E1_14:	/* Pause key only */
		switch (scan) {
		case 0x77:
			entry = KEY(126);	/* Pause */
			break;
		default:
			entry = INVALID;
			break;
		}
		break;
	}

	/*
	 * We'll fill in the results here, even though they may
	 * not be valid, so that we can reset state before the
	 * switch.  We don't make any promises about *keynum and
	 * *state when we return B_FALSE.
	 */
	*keynum = entry;
	*state   = kb8042->parse_scan.break_prefix_received;

	/*
	 * Reset to idle
	 */
	kb8042->parse_scan.state = STATE_IDLE;
	kb8042->parse_scan.break_prefix_received = KEY_PRESSED;

	switch (entry) {
	case KEYIGN:				/* not a key, nor an error */
		DEBUG_KD(("-> hole -> ignored\n"));
		return (B_FALSE);		/* also not a final keycode */

	case KEYBAD:		/* not part of a legit sequence? */
		DEBUG_KD(("-> bad -> ignored\n"));
		return (B_FALSE);	/* and return not a final keycode */

	default:
		/*
		 * If we're here, it's a valid keycode.  We've already
		 * filled in the return values; return success.
		 */

		DEBUG_KD(("-> %s keypos %d\n",
			*state == KEY_RELEASED ? "released" : "pressed",
			*keynum));

		return (B_TRUE);		/* resolved to a key */
	}
}
