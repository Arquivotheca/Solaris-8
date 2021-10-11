/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _AR_H
#define	_AR_H

#pragma ident	"@(#)ar.h	1.10	93/11/01 SMI"	/* SVr4.0 2.12	*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *		COMMON ARCHIVE FORMAT
 *
 *	ARCHIVE File Organization:
 *	_________________________________________________
 *	|__________ARCHIVE_MAGIC_STRING_________________|
 *	|__________ARCHIVE_FILE_MEMBER_1________________|
 *	|						|
 *	|	Archive File Header "ar_hdr"		|
 *	|...............................................|
 *	|	Member Contents				|
 *	|		1. External symbol directory	|
 *	|		2. Text file			|
 *	|_______________________________________________|
 *	|________ARCHIVE_FILE_MEMBER_2__________________|
 *	|		"ar_hdr"			|
 *	|...............................................|
 *	|	Member Contents (.o or text file)	|
 *	|_______________________________________________|
 *	|	.		.		.	|
 *	|	.		.		.	|
 *	|	.		.		.	|
 *	|_______________________________________________|
 *	|________ARCHIVE_FILE_MEMBER_n__________________|
 *	|		"ar_hdr"			|
 *	|...............................................|
 *	|		Member Contents			|
 *	|_______________________________________________|
 *
 */

#define	ARMAG	"!<arch>\n"
#define	SARMAG	8
#define	ARFMAG	"`\n"

struct ar_hdr		/* archive file member header - printable ascii */
{
	char	ar_name[16];	/* file member name - `/' terminated */
	char	ar_date[12];	/* file member date - decimal */
	char	ar_uid[6];	/* file member user id - decimal */
	char	ar_gid[6];	/* file member group id - decimal */
	char	ar_mode[8];	/* file member mode - octal */
	char	ar_size[10];	/* file member size - decimal */
	char	ar_fmag[2];	/* ARFMAG - string to end header */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _AR_H */
