/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)mcreate.c	1.4	96/10/03 SMI"	/* SVr4.0 1.7	*/

# include	<unistd.h>
# include	<string.h>
# include	<stropts.h>
# include	<errno.h>
# include	<stdlib.h>

# include	"lp.h"
# include	"msgs.h"

#if	defined(__STDC__)
MESG * mcreate ( char * path )
#else
MESG * mcreate (path)
    char	*path;
#endif
{
    int			fds[2];
    MESG		*md;

    if (pipe(fds) != 0)
	return(NULL);

#if	!defined(NOCONNLD)
    if (ioctl(fds[1], I_PUSH, "connld") != 0)
	return(NULL);
#endif

    if (fattach(fds[1], path) != 0)
        return(NULL);

    if ((md = (MESG *)Malloc(MDSIZE)) == NULL)
	return(NULL);
    
    memset(md, 0, sizeof (MESG));
    md->admin = 1;
    md->file = Strdup(path);
    md->gid = getgid();
    md->readfd = fds[0];
    md->state = MDS_IDLE;
    md->type = MD_MASTER;
    md->uid = getuid();
#if 1
    md->writefd = fds[1];
#else
    md->writefd = fds[0];
    close(fds[1]);
#endif
    
    return(md);
}
