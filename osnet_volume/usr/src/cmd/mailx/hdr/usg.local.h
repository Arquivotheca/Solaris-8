/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)usg.local.h	1.7	92/07/21 SMI"
		/* from SVr4.0 1.4.2.2 */

/*
 * Declarations and constants specific to an installation.
 */

/*
 * mailx -- a modified version of a University of California at Berkeley
 *	mail program
 */

#define	LOCAL		EMPTYID		/* Dynamically determined local host */
#ifdef preSVr4
# define	MAIL	"/bin/rmail"	/* Mail delivery agent */
#else
# define	MAIL	"/usr/bin/rmail"/* Mail delivery agent */
#endif
#define SENDMAIL	"/usr/lib/sendmail"
					/* Name of classy mail deliverer */
#define	EDITOR		"ed"		/* Name of text editor */
#define	VISUAL		"vi"		/* Name of display editor */
#define	PG		(value("PAGER") ? value("PAGER") : \
			    (value("bsdcompat") ? "more" : "pg -e"))
					/* Standard output pager */
#define	MORE		PG
#define	LS		(value("LISTER") ? value("LISTER") : "ls")
					/* Name of directory listing prog*/
#ifdef preSVr4
# define	SHELL	"/bin/sh"	/* Standard shell */
#else
# define	SHELL	"/usr/bin/sh"	/* Standard shell */
#endif
#define	HELPFILE	helppath("mailx.help")
					/* Name of casual help file */
#define	THELPFILE	helppath("mailx.help.~")
					/* Name of casual tilde help */
#ifdef preSVr4
# define	MASTER	(value("bsdcompat") ? libpath("Mail.rc") : \
			    libpath("mailx.rc")
#else
# define	MASTER	(value("bsdcompat") ? "/etc/mail/Mail.rc" : \
			    "/etc/mail/mailx.rc")
#endif
#define	APPEND				/* New mail goes to end of mailbox */
#define CANLOCK				/* Locking protocol actually works */
#define	UTIME				/* System implements utime(2) */
