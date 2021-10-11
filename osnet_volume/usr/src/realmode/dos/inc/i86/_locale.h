/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)_locale.h	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		_locale.h
 *
 *   Description:	contains constants related to locale implementation
 *			values; includes size of ctype character table.
 *
 */
/*      Copyright (c) 1988 AT&T */
/*        All rights reserved.  */

/*      THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T     */
/*      The copyright notice above does not evidence any        */
/*      actual or intended publication of such source code.     */

#define LC_NAMELEN      255             /* maximum part name length (inc. \0) */
#define SZ_CTYPE        (257 + 257)     /* is* and to{upp,low}er tables */
#define SZ_CODESET      7               /* bytes for codeset information */
#define SZ_NUMERIC      2               /* bytes for numeric editing */
#define SZ_TOTAL        (SZ_CTYPE + SZ_CODESET)
#define NM_UNITS        0               /* index of decimal point character */
#define NM_THOUS        1               /* index of thousand's sep. character */
extern unsigned char _ctype[SZ_TOTAL];
extern unsigned char _numeric[SZ_NUMERIC];

