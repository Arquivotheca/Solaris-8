/*
 * Copyright (c) 1985-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_KBDREG_H
#define	_SYS_KBDREG_H

#pragma ident	"@(#)kbdreg.h	1.15	98/01/06 SMI"	/* SunOS4.0 1.7	*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Keyboard implementation private definitions.
 */

struct keyboardstate {
	int	k_id;
	uchar_t	k_idstate;
	uchar_t	k_state;
	uchar_t	k_rptkey;
	uint_t	k_buckybits;
	uint_t	k_shiftmask;
	struct	keyboard *k_curkeyboard;
	uint_t	k_togglemask;	/* Toggle shifts state */
};

/*
 * States of keyboard ID recognizer
 */
#define	KID_NONE		0	/* startup */
#define	KID_GOT_PREFACE		1	/* got id preface */
#define	KID_OK			2	/* locked on ID */
#define	KID_GOT_LAYOUT		3	/* got layout prefix */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_KBDREG_H */
