#ident	"@(#)pf.c	1.11	96/10/11 SMI"	/* from snoop (solaris 2.2) */

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc. All rights reserved.
 */

/*
 * Generic Packet filter routines. Taken from solaris 2.6 snoop.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/byteorder.h>
#include <sys/syslog.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/udp.h>
#include <netinet/dhcp.h>
#include <netdb.h>
#include "pf.h"


struct packetfilt dhcppf;
static u_short *pfp;

static void
pf_init(void)
{
	(void) memset((char *)&dhcppf, 0, sizeof (dhcppf));
	pfp = &dhcppf.Pf_Filter[0];
	dhcppf.Pf_FilterLen = 0;
	dhcppf.Pf_Priority = 8;	/* not important */
}
static void
pf_finish(void)
{
	dhcppf.Pf_FilterLen = (u_char) ((u_short *) pfp -
	    &dhcppf.Pf_Filter[0]);
}
static void
pf_emit(u_short x)
{
	*pfp++ = x;
}

/*
 * Emit packet filter code to check a field in the packet for a
 * particular value. Need different code for each field size. Since the
 * pf can only compare 16 bit quantities we have to use masking to compare
 * byte values. Long word (32 bit) quantities have to be done as two 16
 * bit comparisons.
 */
static void
pf_compare_value(u_int offset, u_int len, u_int val)
{

	switch (len) {
	case 1:
		pf_emit(ENF_PUSHWORD + offset / 2);
#if	defined(_BIG_ENDIAN)
		if (offset % 2) {
#else
		if ((offset % 2) == 0) {
#endif	/* _BIG_ENDIAN */
#ifdef ENF_PUSH00FF
			pf_emit(ENF_PUSH00FF | ENF_AND);
#else
			pf_emit(ENF_PUSHLIT | ENF_AND);
			pf_emit(0x00FF);
#endif
			pf_emit(ENF_PUSHLIT | ENF_EQ);
			pf_emit(val);
		} else {
#ifdef ENF_PUSHFF00
			pf_emit(ENF_PUSHFF00 | ENF_AND);
#else
			pf_emit(ENF_PUSHLIT | ENF_AND);
			pf_emit(0xFF00);
#endif
			pf_emit(ENF_PUSHLIT | ENF_EQ);
			pf_emit(val << 8);
		}
		break;

	case 2:
		pf_emit(ENF_PUSHWORD + offset / 2);
		pf_emit(ENF_PUSHLIT | ENF_EQ);
		pf_emit(htons((u_short)val));
		break;

	case 4:
		pf_emit(ENF_PUSHWORD + offset / 2);
		pf_emit(ENF_PUSHLIT | ENF_EQ);
#if	defined(_BIG_ENDIAN)
		pf_emit(val >> 16);
#else
		pf_emit(val & 0xffff);
#endif	/* _BIG_ENDIAN */
		pf_emit(ENF_PUSHWORD + (offset / 2) + 1);
		pf_emit(ENF_PUSHLIT | ENF_EQ);
#if	defined(_BIG_ENDIAN)
		pf_emit(val & 0xffff);
#else
		pf_emit(val >> 16);
#endif	/* _BIG_ENDIAN */
		pf_emit(ENF_AND);
		break;
	}
}

/*
 * Packet Filter definition for BOOTP/UDP/IP packets over ethernet. IP
 * packet must be UDP protocol. Destination port must be IPPORT_BOOTPS.
 */
void
initialize_pf(void)
{
	register int offset;
	struct ip ip;

	offset = (u_int)((u_char *)&(ip.ip_off) - (u_char *)&ip);

	pf_init();

	/* Fragment offset 0? */
	pf_emit(ENF_PUSHWORD + offset / 2);
	pf_emit(ENF_PUSHLIT | ENF_AND);
	pf_emit(htons((u_short)0x1fff));
	pf_emit(ENF_PUSHLIT | ENF_EQ);
	pf_emit(0);

	/* BOOTP destination port? */
	offset = sizeof (struct ip) + sizeof (u_short);
	pf_compare_value(offset, 2, IPPORT_BOOTPS);
	pf_emit(ENF_AND);

	/* UDP packet? */
	offset = (u_int)((u_char *)&(ip.ip_p) - (u_char *)&ip);
	pf_compare_value(offset, 1, IPPROTO_UDP);
	pf_emit(ENF_AND);

	pf_finish();
}
