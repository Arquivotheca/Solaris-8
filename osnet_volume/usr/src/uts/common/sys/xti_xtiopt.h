/*	Copyright (c) 1996 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#ifndef _SYS_XTI_XTIOPTT_H
#define	_SYS_XTI_XTIOPTT_H

#pragma ident	"@(#)xti_xtiopt.h	1.5	98/04/28 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * OPTIONS ON XTI LEVEL
 *
 * Note:
 * Unfortunately, XTI specification test assertions require exposing in
 * headers options that are not implemented. They also require exposing
 * Internet and OSI related options as part of inclusion of <xti.h>
 */

/* XTI level */

#define	XTI_GENERIC	0xfffe

/*
 * XTI-level Options
 */

#define	XTI_DEBUG		0x0001 /* enable debugging */
#define	XTI_LINGER		0x0080 /* linger on close if data present */
#define	XTI_RCVBUF		0x1002 /* receive buffer size */
#define	XTI_RCVLOWAT		0x1004 /* receive low water mark */
#define	XTI_SNDBUF		0x1001 /* send buffer size */
#define	XTI_SNDLOWAT		0x1003 /* send low-water mark */


/*
 * Structure used with linger option.
 */

struct t_linger {
	t_scalar_t	l_onoff;	/* option on/off */
	t_scalar_t	l_linger;	/* linger time */
};


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_XTI_XTIOPTT_H */
