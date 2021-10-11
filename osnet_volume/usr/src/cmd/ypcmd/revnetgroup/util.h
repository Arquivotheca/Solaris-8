/*
 * Copyright 1995 Sun Microsystems Inc.
 * All rights reserved.
 */
							    

#ifndef	__UTIL_H
#define	__UTIL_H

#pragma ident	"@(#)util.h	1.4	96/04/25 SMI"        /* SMI4.1 1.5 */

#ifdef	__cplusplus
extern "C" {
#endif

#define EOS '\0'

#ifndef NULL 
#	define NULL ((char *) 0)
#endif


#define MALLOC(object_type) ((object_type *) malloc(sizeof(object_type)))

#define FREE(ptr)	free((char *) ptr) 

#define STRCPY(dst,src) \
	(dst = malloc((unsigned)strlen(src)+1), (void) strcpy(dst,src))

#define STRNCPY(dst,src,num) \
	(dst = (char *) malloc((unsigned)(num) + 1),\
	(void)strncpy(dst,src,num),(dst)[num] = EOS) 

/*
extern char *malloc();
*/
extern char *alloca();

char *getline();
void fatal();

#ifdef	__cplusplus
}
#endif

#endif	/* __UTIL_H */
