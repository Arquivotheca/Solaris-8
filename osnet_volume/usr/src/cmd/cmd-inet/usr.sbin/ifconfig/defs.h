/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#pragma ident   "@(#)defs.h 1.2     99/03/26 SMI"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <libdevinfo.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/sockio.h>
#include <stropts.h>

#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <net/if.h>
#include <net/pfkeyv2.h>
#include <netinet/if_ether.h>

#include <netinet/dhcp.h>
#include <dhcpagent_util.h>
#include <dhcpagent_ipc.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <assert.h>

/*
 * for those who will get too depressed reading 1186672, the summary
 * is that the prototypes for ether_ntoa() and ether_aton() (among
 * others) are not provided in any header files, despite the fact that
 * we've been shipping manpages since pre-5.0.  however, it gets
 * better: the implementation of these functions in libsocket have
 * different prototypes than the ones in ethers(3N) (hopefully someone
 * got a good laugh out of all this).  looking at 4.4 BSD, it becomes
 * clear that the manpages are correct, so we tolerate the lint
 * warnings for now...
 */
extern char *ether_ntoa(struct ether_addr *);
extern struct ether_addr *ether_aton(char *);
