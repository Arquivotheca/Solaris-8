/*
 * Copyright (c) 1990-1999, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Socket-specific definitions
 */

#ifndef _SOCKET_INET_H
#define	_SOCKET_INET_H

#pragma ident	"@(#)socket_inet.h	1.2	99/06/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Socket support definitions
 */

#define	MAXSOCKET	(10)
#define	SOCKETTYPE	(65536)
#define	MEDIA_LVL	0
#define	NETWORK_LVL	1
#define	TRANSPORT_LVL	2
#define	APP_LVL		3

#ifndef	TRUE
#define	TRUE	(1)
#define	FALSE	(0)
#endif	/* TRUE */

#ifndef	BUFSIZ
#define	BUFSIZ	(1024)
#endif	/* BUFSIZ */

enum SockType { INETBOOT_UNUSED, INETBOOT_DGRAM, INETBOOT_RAW };
enum Ports { SOURCE, DESTINATION };
#define	FD_TO_SOCKET(v)	((v) - SOCKETTYPE)


/* Network configuration protocol definitions */
#define	NCT_RARP_BOOTPARAMS	(0)
#define	NCT_BOOTP_DHCP		(1)
#define	NCT_DEFAULT		NCT_RARP_BOOTPARAMS
#define	NCT_BUFSIZE		(64)

/*
 * "target" is needed for input prior to IP address assignment. It may
 * seem redundant given the binding information contained in the socket,
 * but that's only true if we have an IP address. If we don't, and we
 * try DHCP, we'll try to udp checksum using INADDR_ANY as the destination
 * IP address, when in fact the destination IP address was the IP address
 * we were OFFERED/Assigned.
 */
struct inetgram {
	/* Common */
	struct sockaddr_in	igm_saddr;	/* source address info */
	int			igm_level;	/* Stack level (LVL) of data */
	caddr_t			igm_datap;	/* data offset */
	caddr_t			igm_bufp;	/* buffer */
	int			igm_len;	/* buffer len */
	struct inetgram		*igm_next;	/* next inetgram in list */
	union {
		struct {
			/* Input specific */
			struct in_addr	in_t;
			uint16_t	in_i;
		} _IN_un;
		struct {
			/* Output specific */
			struct in_addr	out_r;
			int		out_f;
		} _OUT_un;
	} _i_o_inet;
#define	igm_target	_i_o_inet._IN_un.in_t	/* See above comment block */
#define	igm_id		_i_o_inet._IN_un.in_i	/* IP id */
#define	igm_router	_i_o_inet._OUT_un.out_r	/* first router IP  ... */
#define	igm_oflags	_i_o_inet._OUT_un.out_f	/* flag: 0 or MSG_DONTROUTE */
};

struct inetboot_socket {
	enum SockType		type;		/* socket type */
	uint8_t			proto;		/* ip protocol */
	int			out_flags;	/* 0 or MSG_DONTROUTE */
	int			bound;		/* boolean */
	struct sockaddr_in	bind;		/* Binding info */
	struct inetgram		*inq;		/* input queue */
	uint32_t		in_timeout;	/* Input timeout (msec) */
	int			(*headerlen[APP_LVL])(void);
	int			(*input[APP_LVL])(int);
	int			(*output[APP_LVL])(int, struct inetgram *);
	in_port_t		(*ports)(uint16_t *, enum Ports);
};

extern struct inetboot_socket	sockets[MAXSOCKET];
extern int dontroute;

extern void add_grams(struct inetgram **, struct inetgram *);
extern void del_gram(struct inetgram **, struct inetgram *, int);
extern void nuke_grams(struct inetgram **);
extern struct inetgram *last_gram(struct inetgram *);
extern int socket(int, int, int);
extern int socket_close(int);
extern int bind(int, const struct sockaddr *, int);
extern int sendto(int, const char *, int, int, const struct sockaddr *, int);
extern int recvfrom(int, char *, int, int, struct sockaddr *, int *);
extern int getsockopt(int, int, int, const void *, size_t);
extern int setsockopt(int, int, int, const void *, size_t);
extern struct sockaddr_in *nfs_server_sa(void);
extern void b_sleep(uint32_t);
extern int get_netconfig_strategy(void);
extern int prom_cached_reply(int);

#ifdef	__cplusplus
}
#endif

#endif /* _SOCKET_INET_H */
