/*
 * Copyright (c) 1992-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#pragma ident   "@(#)ifconfig.h 1.3     99/03/26 SMI"

/*
 * return values for (af_getaddr)() from in_getprefixlen()
 */
#define	BAD_ADDR	-1	/* prefix is invalid */
#define	NO_PREFIX	-2	/* no prefix was found */

#define	MAX_MODS	9	/* max modules that can be pushed on intr */

typedef struct dev_att {
	char	ifname[LIFNAMSIZ];
	int	style;
	int	ppa;
	int	lun;
	int	mod_cnt;
	char	devname[LIFNAMSIZ];
	char	modlist[MAX_MODS][LIFNAMSIZ];
} dev_att_t;

extern int	debug;
extern uid_t	euid;

extern void	Perror0(char *);
extern void	Perror0_exit(char *);
extern void	Perror2(char *, char *);
extern void	Perror2_exit(char *, char *);

extern int	doifrevarp(char *, struct sockaddr_in *);
extern int	getnetmaskbyaddr(struct in_addr, struct in_addr *);

extern int	dlpi_set_address(char *, struct ether_addr *);
extern int	dlpi_get_address(char *, struct ether_addr *);
extern int	dlpi_attach(int, int, int);
extern int	dlpi_detach(int, int);

extern int	do_dad(char *, struct sockaddr_in6 *);
extern int	ifname_open(char *, dev_att_t *);
extern int	open_dev(dev_att_t *, int, boolean_t, int *, int);
