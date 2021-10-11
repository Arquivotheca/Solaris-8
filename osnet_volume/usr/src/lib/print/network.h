/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _NETWORK_H
#define _NETWORK_H

#pragma ident	"@(#)network.h	1.4	98/07/22 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	NETWORK_ERROR_UNKNOWN		-1
#define	NETWORK_ERROR_HOST		-2
#define	NETWORK_ERROR_SERVICE		-3
#define	NETWORK_ERROR_PORT		-4
#define	NETWORK_ERROR_SEND_RESPONSE	-5
#define	NETWORK_ERROR_SEND_FAILED	-6

#define	ACK(fd)	net_write(fd, "", 1);
#define	NACK(fd) net_write(fd, "\1", 1);

extern int	net_open(char *host, int timeout);
extern int	net_close(int nd);
extern int	net_read(int nd, char *buf, int len);
extern int	net_write(int nd, char *buf, int len);
extern int	net_printf(int nd, char *fmt, ...);
extern char	*net_gets(char *buf, int size, int nd);
extern int	net_send_message(int nd, char *fmt, ...);
extern int	net_response(int nd);
extern int	net_send_file(int nd, char *name, char *data, int data_len,
				int type);

#ifdef __cplusplus
}
#endif

#endif /* _NETWORK_H */
