/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)misc.h	1.6	99/07/27 SMI"       /* SVr4.0 1.4*/


/*
 * current version of _sactab
 */

# define VERSION	1

/*
 * comment delimiter
 */

# define COMMENT	'#'

/*
 * field delimiter (one version for functions that take string args, and
 * one for character args)
 */

# define DELIM	":"
# define DELIMC	':'

/*
 * key file names
 */

# define HOME		"/etc/saf"			/* SAC home dir */
# define ALTHOME	"/var/saf"			/* alternate directory for misc. files */
# define SACTAB		"/etc/saf/_sactab"		/* SAC admin file */
# define LOGFILE	"/var/saf/_log"			/* SAC log file */
# define DBGFILE	"/var/saf/debug"		/* SAC debug file */
# define SYSCONFIG	"/etc/saf/_sysconfig"		/* sys config file */
# define CMDPIPE	"/etc/saf/_cmdpipe"		/* SAC command pipe */

/*
 * version string stamp
 */

# define VSTR		"# VERSION="

/*
 * miscellaneous
 */

# define PMTYPESIZE	14	/* maximum length for a port monitor type */
# define SVCTAGSIZE	14	/* maximum length for a service tag */
# define SLOP		20	/* enough extra bytes to hold status info */
# define TRUE		1	/* definition of true */
# define FALSE		0	/* definition of false */
# define SSTATE		255	/* special state to indicate no sac */
# define SIZE		512	/* scratch buffer size */
# define BADFARGSIZE	256	/* hold bad args (to -f) buffer size */
