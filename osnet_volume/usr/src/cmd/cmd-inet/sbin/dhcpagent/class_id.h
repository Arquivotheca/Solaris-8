/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	CLASS_ID_H
#define	CLASS_ID_H

#pragma ident	"@(#)class_id.h	1.1	99/04/09 SMI"

/*
 * class_id.[ch] provides an interface for retrieving the class id
 * from the prom.  see class_id.c for more details on how to use the
 * exported function.
 */

#ifdef	__cplusplus
extern "C" {
#endif

char		*get_class_id(void);

#ifdef	__cplusplus
}
#endif

#endif	/* CLASS_ID_H */
