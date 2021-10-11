#ident	"@(#)path.h	1.4	95/02/25 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#ifndef _PATH_H
#define	_PATH_H

#include "ipd_ioctl.h"

struct uucp {
    char	*system_name;
    int		max_connections;
};

struct interface {
    enum ipd_iftype	iftype;
    u_int		ifunit;
    boolean_t		wild_card;
    boolean_t		get_ipaddr;
    struct sockaddr	sa;
};

enum values {
    off,
    on,
    vj
};
typedef enum values	value_t;

struct ipcp {
    u_long	async_map;
    value_t	compression;
};

struct lcp {
    value_t		compression;
    int			mru;
};

enum states {
    inactive,
    dialing,
    connected,
    ppp,
    ip
};
typedef enum states	state_t;

enum authentication_type {
    none,
    pap,
    chap,
    all		/* both */
};
typedef enum authentication_type auth_t;

struct pap_credential {
    char *id;
    char *password;
};
typedef struct pap_credential pap_c;

struct chap_credential {
    char *name;
    char *secret;
};
typedef struct chap_credential chap_c;

struct authentication {
    auth_t	required;
    pap_c	pap_peer;
    chap_c	chap_peer;
    auth_t	will_do;
    pap_c	pap;
    chap_c	chap;
};
typedef struct authentication authentication;

/* description of one connection */

struct path {
    struct path		*next;
    int			s;
    int			cns_id;
    int			mux;
    int			mux_id;
    struct interface	inf;
    struct uucp		uucp;
    int			timeout;
    state_t		state;
    pid_t		pid;
    boolean_t		default_route;
    struct ipcp		ipcp;
    struct lcp		lcp;
    authentication	auth;
};

extern struct path	*paths;

struct path		*get_path_by_addr(ipd_con_dis_t);
struct path		*get_path_by_fd(int);
struct path		*get_path_by_name(char *);
void			add_path(struct path *);
void			free_path(struct path *);

#endif	/* _PATH_H */
