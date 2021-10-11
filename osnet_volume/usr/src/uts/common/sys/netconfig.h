/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYS_NETCONFIG_H
#define	_SYS_NETCONFIG_H

#pragma ident	"@(#)netconfig.h	1.20	99/04/27 SMI"	/* SVr4.0 1.4 */

#ifdef	__cplusplus
extern "C" {
#endif

#define	NETCONFIG "/etc/netconfig"
#define	NETPATH   "NETPATH"

struct  netconfig {
	char		*nc_netid;	/* network identifier		*/
	unsigned int	nc_semantics;	/* defined below		*/
	unsigned int	nc_flag;	/* defined below		*/
	char		*nc_protofmly;	/* protocol family name		*/
	char		*nc_proto;	/* protocol name		*/
	char		*nc_device;	/* device name for network id	*/
	unsigned int	nc_nlookups;	/* # of entries in nc_lookups	*/
	char		**nc_lookups;	/* list of lookup directories	*/
	unsigned int	nc_unused[8];	/* borrowed for lockd etc.	*/
};

typedef struct {
	struct netconfig **nc_head;
	struct netconfig **nc_curr;
} NCONF_HANDLE;

/*
 *	Values of nc_semantics
 */

#define	NC_TPI_CLTS	1
#define	NC_TPI_COTS	2
#define	NC_TPI_COTS_ORD	3
#define	NC_TPI_RAW	4

/*
 *	Values of nc_flag
 */

#define	NC_NOFLAG	00
#define	NC_VISIBLE	01
#define	NC_BROADCAST	02

/*
 *	Values of nc_protofmly
 */

#define	NC_NOPROTOFMLY	"-"
#define	NC_LOOPBACK	"loopback"
#define	NC_INET		"inet"
#define	NC_INET6	"inet6"
#define	NC_IMPLINK	"implink"
#define	NC_PUP		"pup"
#define	NC_CHAOS	"chaos"
#define	NC_NS		"ns"
#define	NC_NBS		"nbs"
#define	NC_ECMA		"ecma"
#define	NC_DATAKIT	"datakit"
#define	NC_CCITT	"ccitt"
#define	NC_SNA		"sna"
#define	NC_DECNET	"decnet"
#define	NC_DLI		"dli"
#define	NC_LAT		"lat"
#define	NC_HYLINK	"hylink"
#define	NC_APPLETALK	"appletalk"
#define	NC_NIT		"nit"
#define	NC_IEEE802	"ieee802"
#define	NC_OSI		"osi"
#define	NC_X25		"x25"
#define	NC_OSINET	"osinet"
#define	NC_GOSIP	"gosip"

/*
 *	Values for nc_proto
 */

#define	NC_NOPROTO	"-"
#define	NC_TCP		"tcp"
#define	NC_UDP		"udp"
#define	NC_ICMP		"icmp"

#if defined(__STDC__)

extern void		*setnetconfig(void);
extern int		endnetconfig(void *);
extern struct netconfig	*getnetconfig(void *);
extern struct netconfig	*getnetconfigent(char *);
extern void		freenetconfigent(struct netconfig *);
extern void		*setnetpath(void);
extern int		endnetpath(void *);
extern struct netconfig *getnetpath(void *);
extern void		nc_perror(const char *);
extern char		*nc_sperror(void);

#else	/* __STDC__ */

extern void		*setnetconfig();
extern int		endnetconfig();
extern struct netconfig	*getnetconfig();
extern struct netconfig	*getnetconfigent();
extern void		freenetconfigent();
extern void		*setnetpath();
extern int		endnetpath();
extern struct netconfig *getnetpath();
extern void		nc_perror();
extern char		*nc_sperror();

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_NETCONFIG_H */
