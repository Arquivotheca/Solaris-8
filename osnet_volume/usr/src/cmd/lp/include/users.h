/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)users.h	1.4	93/11/18 SMI"	/* SVr4.0 1.6	*/

#if	!defined(_LP_USERS_H)
#define	_LP_USERS_H

#include "stdio.h"

typedef struct
{
    short	priority_limit;
}
USER;

int		putuser ( char * , USER * );
int		deluser ( char * );
int		getdfltpri ( void );
void		trashusers ( void );

USER *		getuser ( char *);

#define LEVEL_DFLT 20
#define LIMIT_DFLT 0

#define TRUE  1
#define FALSE 0

#define PRI_MAX 39
#define	PRI_MIN	 0

#define LPU_MODE 0644

struct user_priority
{
    short	deflt;		/* priority to use when not specified */
    short	deflt_limit;	/* priority limit for users not
				   otherwise specified */
    char	**users[PRI_MAX - PRI_MIN + 1];
};

#endif
