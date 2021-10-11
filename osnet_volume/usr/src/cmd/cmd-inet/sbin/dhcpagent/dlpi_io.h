/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	DLPI_IO_H
#define	DLPI_IO_H

#pragma ident	"@(#)dlpi_io.h	1.1	99/04/09 SMI"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/dlpi.h>

/*
 * dlpi_io.[ch] contain the interface the agent uses to interact with
 * DLPI.  it makes use of dlprims.c (and should be its only consumer).
 * see dlpi_io.c for documentation on how to use the exported
 * functions.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * buffer size to be used in control part of DLPI messages, in bytes
 */
#define	DLPI_BUF_MAX	256

/*
 * timeout to be used on DLPI-related operations, in seconds
 */
#define	DLPI_TIMEOUT	5

/*
 * flags for dlpi_recv_link()
 */
#define	DLPI_RECV_SHORT	0x01	/* short reads are expected */

typedef ushort_t *filter_func_t(ushort_t *, void *);

filter_func_t	dhcp_filter, blackhole_filter;
uchar_t		*build_broadcast_dest(dl_info_ack_t *, uchar_t *);
void		set_packet_filter(int, filter_func_t *, void *, const char *);
int		dlpi_open(const char *, dl_info_ack_t *, size_t, t_uscalar_t);
int		dlpi_close(int);
ssize_t		dlpi_recvfrom(int, void *, size_t, struct sockaddr_in *);
ssize_t		dlpi_recv_link(int, void *, size_t, uint32_t);
ssize_t		dlpi_send_link(int, void *, size_t, uchar_t *, size_t);
ssize_t		dlpi_sendto(int, void *, size_t, struct sockaddr_in *,
		    uchar_t *, size_t);

#ifdef	__cplusplus
}
#endif

#endif	/* DLPI_IO_H */
