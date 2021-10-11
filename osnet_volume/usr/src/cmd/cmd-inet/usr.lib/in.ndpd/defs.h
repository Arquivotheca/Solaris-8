/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)defs.h	1.3	99/11/07 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <sys/stropts.h>

#include <string.h>
#include <ctype.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/if_ether.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <net/route.h>

extern int debug, no_loopback;
extern boolean_t addrconf;

extern struct in6_addr all_nodes_mcast;
extern struct in6_addr all_routers_mcast;

extern int			rtsock;
extern struct	rt_msghdr	*rt_msg;
extern struct	sockaddr_in6	*rta_gateway;
extern struct	sockaddr_dl	*rta_ifp;

/* Debug flags */
#define	D_ALL		0xffff
#define	D_DEFAULTS	0x0001		/* Default values in config file */
#define	D_CONFIG	0x0002		/* Config file */
#define	D_PHYINT	0x0004		/* phyint table */
#define	D_PREFIX	0x0008		/* prefix table */
#define	D_ROUTER	0x0010		/* router table */
#define	D_STATE		0x0020		/* RS/RA state machine */
#define	D_IFSCAN	0x0040		/* Scan of kernel interfaces */
#define	D_TIMER		0x0080		/* Timer mechanism */
#define	D_PARSE		0x0100		/* config file parser */
#define	D_PKTIN		0x0200		/* Received packet */
#define	D_PKTBAD	0x0400		/* Malformed packet */
#define	D_PKTOUT	0x0800		/* Sent packet */

#define	IF_SEPARATOR		':'
#define	IPV6_MAX_HOPS		255
#define	IPV6_MIN_MTU		(1024+256)
#define	IPV6_ABITS		128

/* Return a random number from a an range inclusive of the endpoints */
#define	GET_RANDOM(LOW, HIGH) (random() % ((HIGH) - (LOW) + 1) + (LOW))

#define	TIMER_INFINITY	0xFFFFFFFFU	/* Never time out */
#define	PREFIX_INFINITY 0XFFFFFFFFU	/* A "forever" prefix lifetime */

/*
 * Used by 2 hour rule for stateless addrconf
 */
#define	MIN_VALID_LIFETIME	(2*60*60)		/* In seconds */

/*
 * The addrconf prefixes are saved every so often in order to be able
 * to reuse valid once should the machine reboot and no router be present.
 * If the recorded lifetimes are longer than NDP_MAX_SAVE_LIFETIME they
 * are saved as NDP_MAX_SAVE_LIFETIME.
 */
#define	NDP_STATE_FILE		"/var/inet/ndpd_state."
#define	NDP_MAX_SAVE_LIFETIME	(7*24*60*60*1000)	/* 1 week in ms */
#define	NDP_STATE_FILE_SAVE_TIME (5*60*1000)		/* 5 minutes in ms */

/*
 * Control how often pi_ReachableTime gets re-randomized
 */
#define	MIN_REACH_RANDOM_INTERVAL	(60*1000)	/* 1 minute in ms */
#define	MAX_REACH_RANDOM_INTERVAL	(60*60*1000)	/* 1 hour in ms */

/*
 * Parsing constants
 */
#define	MAXLINELEN	4096
#define	MAXARGSPERLINE	128

void		timer_schedule(uint_t delay);
extern void	logerr(char *fmt, ...);
extern void	logwarn(char *fmt, ...);
extern void	logtrace(char *fmt, ...);
extern void	logdebug(char *fmt, ...);
extern void	logperror(char *str);
extern int	parse_config(char *config_file, boolean_t file_required);
extern int	parse_line(char *line, char *argvec[], int argcount,
		    int lineno);
extern int	readline(FILE *fp, char *line, int length, int *linenop);

extern int	poll_add(int fd);
extern int	poll_remove(int fd);

extern char	*fmt_lla(char *llabuf, int bufsize, uchar_t *lla, int llalen);
extern int	parse_addrprefix(char *strin, struct in6_addr *in6);
