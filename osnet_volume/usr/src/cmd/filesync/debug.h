/*
 * Copyright (c) 1996 Sun Microsystems, Inc.  All Rights Reserved
 *
 * module:
 *	debug.h
 *
 * purpose:
 *	definitions and declarations for special debugging features
 */

#ifndef	_DEBUG_H
#define	_DEBUG_H

#pragma ident	"@(#)debug.h	1.1	96/03/26 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	DBG_ERRORS	1	/* enable error simulation code	*/
#define	DBG_MAX_ERR	20	/* maximum # simulated errs	*/

/*
 * the flaglists are used by the showflags routine in order to
 * print bitmasks in a symbolic form
 */
struct flaglist {
	long fl_mask;		/* the bit in question		*/
	char *fl_name;		/* the name of that bit		*/
};

extern struct flaglist	dbgmap[], rflags[], fileflags[], diffmap[], errmap[];

char *showflags(struct flaglist *, long);	/* turn bit to a name	*/
int dbg_set_error(char *arg);			/* simulate error	*/
int dbg_chk_error(const char *name, char code);	/* check for simul err	*/

void dbg_usage();				/* debug flag usage	*/
void err_usage();				/* error simul usage	*/

#ifdef	__cplusplus
}
#endif

#endif	/* _DEBUG_H */
