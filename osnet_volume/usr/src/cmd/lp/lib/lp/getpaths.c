/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)getpaths.c	1.5	93/10/18 SMI"	/* SVr4.0 1.15	*/
/* LINTLIBRARY */

#include "stdlib.h"

#include "lp.h"

char	Lp_Spooldir[]		= SPOOLDIR;
char	Lp_Admins[]		= SPOOLDIR "/admins";
char	Lp_FIFO[]		= SPOOLDIR "/fifos/FIFO";
char	Lp_Private_FIFOs[]	= SPOOLDIR "/fifos/private";
char	Lp_Public_FIFOs[]	= SPOOLDIR "/fifos/public";
char	Lp_Requests[]		= SPOOLDIR "/requests";
char	Lp_Schedlock[]		= SPOOLDIR "/SCHEDLOCK";
char	Lp_System[]		= SPOOLDIR "/system";
char	Lp_Temp[]		= SPOOLDIR "/temp";
char	Lp_Tmp[]		= SPOOLDIR "/tmp";
char	Lp_NetTmp[]		= SPOOLDIR "/tmp/.net";

char	Lp_Bin[]		= LPDIR "/bin";
char	Lp_Model[]		= LPDIR "/model";
char	Lp_Slow_Filter[]	= LPDIR "/bin/slow.filter";

char	Lp_A_Logs[]		= LOGDIR;
char	Lp_Logs[]		= LOGDIR;
char	Lp_ReqLog[]		= LOGDIR "/requests";

char	Lp_A[]			= ETCDIR;
char	Lp_NetData[]		= ETCDIR "/Systems";
char	Lp_Users[]		= ETCDIR "/users";
char	Lp_A_Classes[]		= ETCDIR "/classes";
char	Lp_A_Forms[]		= ETCDIR "/forms";
char	Lp_A_Interfaces[]	= ETCDIR "/interfaces";
char	Lp_A_Printers[]		= ETCDIR "/printers";
char	Lp_A_PrintWheels[]	= ETCDIR "/pwheels";
char	Lp_A_Systems[]		= ETCDIR "/systems";
char	Lp_A_Filters[]		= ETCDIR "/filter.table";
char	Lp_Default[]		= ETCDIR "/default";
char	Lp_A_Faults[]		= ETCDIR "/alerts";

int	Lp_NTBase		= sizeof(Lp_NetTmp);

/*
**	Sorry about these nonfunctional functions.  The data is
**	static now.  These exist for historical reasons.
*/

#undef	getpaths
#undef	getadminpaths

void		getpaths ( void ) { return; }
void		getadminpaths ( char * admin) { return; }

/**
 ** getprinterfile() - BUILD NAME OF PRINTER FILE
 **/

char *
getprinterfile(char *name, char *component)
{
    char	*path;

    if (!name)
	return (0);

    path = makepath(Lp_A_Printers, name, component, NULL);

    return (path);
}

/**
 ** getsystemfile() - BUILD NAME OF SYSTEM FILE
 **/

char *
getsystemfile(char *name, char *component)
{
    char	*path;

    if (!name)
	return (0);

    path = makepath(Lp_A_Systems, name, component, NULL);

    return (path);
}

/**
 ** getclassfile() - BUILD NAME OF CLASS FILE
 **/

char *
getclassfile(char *name)
{
    char	*path;

    if (!name)
	return (0);

    path = makepath(Lp_A_Classes, name, NULL);

    return (path);
}

/**
 ** getfilterfile() - BUILD NAME OF FILTER TABLE FILE
 **/

char *
getfilterfile(char *table)
{
    char	*path;

    if (!table)
	table = FILTERTABLE;

    path = makepath(ETCDIR, table, NULL);

    return (path);
}

/**
 ** getformfile() - BUILD NAME OF PRINTER FILE
 **/

char *
getformfile(char *name, char *component)
{
    char	*path;

    if (!name)
	return (0);

    path = makepath(Lp_A_Forms, name, component, NULL);

    return (path);
}
