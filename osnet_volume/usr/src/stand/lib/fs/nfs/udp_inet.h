/*
 * Copyright (c) 1990-1999, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * UDP implementation-specific definitions
 */

#ifndef _UDP_INET_H
#define	_UDP_INET_H

#pragma ident	"@(#)udp_inet.h	1.1	99/02/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int		udp_header_len(void);
extern in_port_t	udp_ports(uint16_t *, enum Ports);
extern int		udp_input(int);
extern int		udp_output(int, struct inetgram *);
extern void		udp_socket_init(struct inetboot_socket *);

#ifdef	__cplusplus
}
#endif

#endif /* _UDP_INET_H */
