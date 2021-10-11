/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#ident	"@(#)misc.h	1.2	96/04/22 SMI"

#if	!defined(_MISC_H)
#define	_MISC_H

extern int snd_msg(int, ...);
extern int rcv_msg(int, ...);
extern int id_no(const char *);
extern char *user_name(const char *);
extern char *user_host(const char *);

#endif /* _MISC_H */
