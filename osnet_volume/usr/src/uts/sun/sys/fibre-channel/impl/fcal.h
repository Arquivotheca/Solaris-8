/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FIBRE_CHANNEL_IMPL_FCAL_H
#define	_SYS_FIBRE_CHANNEL_IMPL_FCAL_H

#pragma ident	"@(#)fcal.h	1.2	99/09/28 SMI"
#include <sys/note.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Loop Initilization Identifier values
 */
#define	LID_LISM	0x1101
#define	LID_LIFA	0x1102
#define	LID_LIPA	0x1103
#define	LID_LIHA	0x1104
#define	LID_LISA	0x1105
#define	LID_LIRP	0x1106
#define	LID_LILP	0x1107

/*
 * lilp_magic definitions
 */
#define	MAGIC_LISM	0x01
#define	MAGIC_LIFA	0x02
#define	MAGIC_LIPA	0x03
#define	MAGIC_LIHA	0x04
#define	MAGIC_LISA	0x05
#define	MAGIC_LIRP	0x06
#define	MAGIC_LILP	0x07

/*
 * PLDA timers (in seconds)
 */
#define	PLDA_R_A_TOV	2
#define	PLDA_RR_TOV	2

/*
 * Note that my_alpa field is of 16 bit size. The lowest significant
 * byte contains the real ALPA.  The highest significant bits are
 * used to indicate if the LBIT was set during Loop Initialization.
 *
 * If the NL_Ports on the loop participate in the LIRP and LILP dance
 * as part of Loop Initialization then the presence of an F_Port can
 * be detected by checking for the presence of AL_PA '0x00' in the AL_PA
 * list (That does not however guarantee if there is a violating NL_Port
 * trying to grab AL_PA value of '0x00').
 *
 * Some FCAs may be capable of notifying if the L_BIT was set in the
 * AL_PA bit map. The host should then perform an IMPLICIT LOGO and
 * execute a PLOGI before sending any other command.
 */
#define	LILP_LBIT_SET		0x100	/* Login Required */

typedef struct fc_lilpmap {
	uint16_t	lilp_magic;
	uint16_t	lilp_myalpa;
	uchar_t		lilp_length;
	uchar_t		lilp_alpalist[127];
} fc_lilpmap_t;

#if	!defined(lint)
_NOTE(SCHEME_PROTECTS_DATA("unique per request", fc_lilpmap))
#endif	/* lint */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FIBRE_CHANNEL_IMPL_FCAL_H */
