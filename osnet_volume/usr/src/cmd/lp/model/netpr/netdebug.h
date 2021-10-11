/*	Copyright (c) 1996,  by Sun Microsystems, Inc.	*/
/*	All rights reserved.				*/

#ifndef	_NETDEBUG_H
#define	_NETDEBUG_H

#pragma ident	"@(#)netdebug.h	1.3	96/04/10 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	MALLOC_ERR	"aborting netpr: malloc returns NULL"
#define	REALLOC_ERR	"aborting netpr: realloc returns NULL"

#define	ASSERT(expr, str)	\
{	\
	if (!expr) {	\
		(void) fprintf(stderr,	\
		"%s: line %d %s\n", __FILE__, __LINE__, str);	\
		panic();	\
		exit(E_RETRY);	\
		}	\
};

#ifdef	__cplusplus
}
#endif

#endif /* _NETDEBUG_H */
