#ifndef lint
static char sccsid[] = "@(#)audit_ftpd.c 1.1 93/06/14 SMI";
#endif

/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <utmpx.h>
#include <pwd.h>
#include <shadow.h>
#include <netinet/in.h>

audit_ftpd_no_anon()
{
        return (0);
}
 
audit_ftpd_sav_data(sin, port)
        struct sockaddr_in *sin;
        long port;
{
        return (0);
}
 
audit_ftpd_bad_pw(uname)
        char *uname;
{
        return (0);
}

audit_ftpd_failure(uname)
	char *uname;
{
	return (0);
}
 
audit_ftpd_success(uname)
        char *uname;
{
        return (0);
}

audit_ftpd_unknown(uname)
        char *uname;
{
        return (0);
}
 
audit_ftpd_excluded(uname)
        char *uname;
{
        return (0);
}

