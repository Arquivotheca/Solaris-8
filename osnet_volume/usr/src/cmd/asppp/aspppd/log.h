#ident	"@(#)log.h	1.1	93/05/17 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#ifndef _LOG_H
#define	_LOG_H

void	fail(char *, ...);
void	log(int, char *, ...);
void	open_log(void);

#endif	/* _LOG_H */
