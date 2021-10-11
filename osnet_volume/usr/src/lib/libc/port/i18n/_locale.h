/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)_locale.h	1.15	96/07/01 SMI"	/* SVr4.0 1.3	*/

/* maximum part name length (inc. \0) */
#define	LC_NAMELEN	255

#define	LC_ANS		(255 * 6)

/* is* and to{upp,low}er tables */
#define	SZ_CTYPE	(257 + 257)

/* bytes for codeset information */
#define	SZ_CODESET	7

/* bytes for numeric editing */
#define	SZ_NUMERIC	2

#define	SZ_TOTAL	(SZ_CTYPE + SZ_CODESET)

/* index of decimal point character */
#define	NM_UNITS	0

/* index of thousand's sep. character */
#define	NM_THOUS	1

extern unsigned char _ctype[SZ_TOTAL];
extern unsigned char _numeric[SZ_NUMERIC];
