#ident	"@(#)uucp_glue.h	1.5	94/01/21 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

extern int	Cn;		/* fd returned by conn */

extern int	Debug;		/* uucp debugging level 0-7 */

void	cleanup(int);		/* called on interrupt or at completion, */
				/* passed an error code			 */

int	conn(char *);		/* connects to uucp system, passed system */
				/* name, returns fd or -1 on failure	  */

void	uucp_epilog(void);	/* call after calling conn */

void	uucp_prolog(char *);	/* call before calling conn, passed	    */
				/* service name (e.g. "uucico", "ppp", "cu" */
