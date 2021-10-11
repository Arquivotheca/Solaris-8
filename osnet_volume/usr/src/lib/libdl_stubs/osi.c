/*
 * Stub _dl*() routines: hardcoded for OSI.
 *
 *	Each stub routine just returns an acceptable value.
 *
 *	The only difference between using these routines and the
 *	real routines is that the second argument to _dlsym() should
 *	be an actual point to an osi routine (e.g. osi_netdir_getbyname())
 *	instead of a string (e.g. "_netdir_getbyname").
 */
#include <stdio.h>

/* osi routines */
extern struct nd_addrlist 	*osi_netdir_getbyname();
extern struct nd_hostservlist 	*osi_netdir_getbyaddr();
extern char			*osi_taddr2uaddr();
extern struct netbuf		*osi_uaddr2taddr();
extern int			osi_netdir_options();

struct nd_addrlist 	
*osi_netdir_getbyname()
{
}

struct nd_hostservlist 	
*osi_netdir_getbyaddr()
{
}

char			
*osi_taddr2uaddr()
{
}

struct netbuf		
*osi_uaddr2taddr()
{
}
int			
osi_netdir_options()
{
}
