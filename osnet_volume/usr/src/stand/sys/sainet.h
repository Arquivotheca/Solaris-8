#ident	"@(#)sainet.h	1.4	92/07/14 SMI" /* from SunOS 4.1 */ 

/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

/*
 * Standalone Internet Protocol State
 */
struct sainet {
	struct in_addr    sain_myaddr;	/* my host address */
	ether_addr_t      sain_myether;	/* my Ethernet address */
	struct in_addr    sain_hisaddr;	/* his host address */
	ether_addr_t      sain_hisether;/* his Ethernet address */
};
