/*
 * Copyright (c) 1999, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma	ident	"@(#)ghd_scsi.c	1.3	99/04/09 SMI"

#include <sys/types.h>
#include <sys/byteorder.h>


/*
 * functions to convert between host format and scsi format
 */
void
scsi_htos_3byte(unchar *ap, ulong nav)
{
	*(ushort *)ap = (ushort)(((nav & 0xff0000) >> 16) | (nav & 0xff00));
	ap[2] = (unchar)nav;
}

void
scsi_htos_long(unchar *ap, ulong niv)
{
	*(ulong *)ap = htonl(niv);
}

void
scsi_htos_short(unchar *ap, ushort nsv)
{
	*(ushort *)ap = htons(nsv);
}

ulong
scsi_stoh_3byte(unchar *ap)
{
	register ulong av = *(ulong *)ap;

	return (((av & 0xff) << 16) | (av & 0xff00) | ((av & 0xff0000) >> 16));
}

ulong
scsi_stoh_long(ulong ai)
{
	return (ntohl(ai));
}

ushort
scsi_stoh_short(ushort as)
{
	return (ntohs(as));
}
