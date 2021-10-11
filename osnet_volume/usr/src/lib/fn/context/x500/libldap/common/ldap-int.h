/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LDAP_INT_H
#define	_LDAP_INT_H

#pragma ident	"@(#)ldap-int.h	1.2	97/11/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 *  Copyright (c) 1995 Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  ldap-int.h - defines & prototypes internal to the LDAP library
 */


#ifdef LDAP_REFERRALS
#define	LDAP_REF_STR		"Referral:\n"
#define	LDAP_REF_STR_LEN	10
#define	LDAP_LDAP_REF_STR	"ldap://"
#define	LDAP_LDAP_REF_STR_LEN	7
#ifdef LDAP_DNS
#define	LDAP_DX_REF_STR		"dx://"
#define	LDAP_DX_REF_STR_LEN	5
#endif /* LDAP_DNS */
#endif /* LDAP_REFERRALS */


/*
 * in cache.c
 */
#ifdef NEEDPROTOS
void add_request_to_cache(LDAP *ld, unsigned int msgtype,
	BerElement *request);
void add_result_to_cache(LDAP *ld, LDAPMessage *result);
int check_cache(LDAP *ld, unsigned int msgtype, BerElement *request);
#else /* NEEDPROTOS */
void add_request_to_cache();
void add_result_to_cache();
int check_cache();
#endif /* NEEDPROTOS */


#ifdef KERBEROS
/*
 * in kerberos.c
 */
#ifdef NEEDPROTOS
char *get_kerberosv4_credentials(LDAP *ld, char *who, char *service,
	int *len);
#else /* NEEDPROTOS */
char *get_kerberosv4_credentials();
#endif /* NEEDPROTOS */

#endif /* KERBEROS */


/*
 * in open.c
 */
#ifdef NEEDPROTOS
int open_ldap_connection(LDAP *ld, Sockbuf *sb, char *host, int defport,
	int set_instance, int async);
#else /* NEEDPROTOS */
int open_ldap_connection();
#endif /* NEEDPROTOS */


/*
 * in os-ip.c
 */
#ifdef NEEDPROTOS
int connect_to_host(Sockbuf *sb, char *host, unsigned int address, int port,
	int async);
void close_connection(Sockbuf *sb);
#else /* NEEDPROTOS */
int connect_to_host();
void close_connection();
#endif /* NEEDPROTOS */

#ifdef KERBEROS
#ifdef NEEDPROTOS
char *host_connected_to(Sockbuf *sb);
#else /* NEEDPROTOS */
char *host_connected_to();
#endif /* NEEDPROTOS */
#endif /* KERBEROS */

#ifdef LDAP_REFERRALS
#ifdef NEEDPROTOS
int do_select(LDAP *ld, struct timeval *timeout);
void *new_select_info(void);
void free_select_info(void *sip);
void mark_select_write(LDAP *ld, Sockbuf *sb);
void mark_select_read(LDAP *ld, Sockbuf *sb);
void mark_select_clear(LDAP *ld, Sockbuf *sb);
long is_read_ready(LDAP *ld, Sockbuf *sb);
long is_write_ready(LDAP *ld, Sockbuf *sb);
#else /* NEEDPROTOS */
int do_select();
void *new_select_info();
void free_select_info();
void mark_select_write();
void mark_select_read();
void mark_select_clear();
long is_read_ready();
long is_write_ready();
#endif /* NEEDPROTOS */
#endif /* LDAP_REFERRALS */


/*
 * in request.c
 */
#ifdef NEEDPROTOS
int send_initial_request(LDAP *ld, unsigned int msgtype,
	char *dn, BerElement *ber);
BerElement *alloc_ber_with_options(LDAP *ld);
void set_ber_options(LDAP *ld, BerElement *ber);
#else /* NEEDPROTOS */
int send_initial_request();
BerElement *alloc_ber_with_options();
void set_ber_options();
#endif /* NEEDPROTOS */

#if defined(LDAP_REFERRALS) || defined(LDAP_DNS)
#ifdef NEEDPROTOS
LDAPConn *new_connection(LDAP *ld, LDAPServer **srvlistp, int use_ldsb,
	int connect, int set_instance, int bind);
LDAPRequest *find_request_by_msgid(LDAP *ld, int msgid);
void free_request(LDAP *ld, LDAPRequest *lr);
void free_connection(LDAP *ld, LDAPConn *lc, int force, int unbind);
void dump_connection(LDAP *ld, LDAPConn *lconns, int all);
void dump_requests_and_responses(LDAP *ld);
#else /* NEEDPROTOS */
LDAPConn *new_connection();
LDAPRequest *find_request_by_msgid();
void free_request();
void free_connection();
void dump_connection();
void dump_requests_and_responses();
#endif /* NEEDPROTOS */
#endif /* LDAP_REFERRALS || LDAP_DNS */

#ifdef LDAP_REFERRALS
#ifdef NEEDPROTOS
int chase_referrals(LDAP *ld, LDAPRequest *lr, char **errstrp, int *hadrefp);
int append_referral(char **referralsp, char *s);
#else /* NEEDPROTOS */
int chase_referrals();
int append_referral();
#endif /* NEEDPROTOS */
#endif /* LDAP_REFERRALS */


/*
 * in unbind.c
 */
#ifdef NEEDPROTOS
int ldap_ld_free(LDAP *ld, int close);
int send_unbind(LDAP *ld, Sockbuf *sb);
#else /* NEEDPROTOS */
int ldap_ld_free();
int send_unbind();
#endif /* NEEDPROTOS */


#ifdef LDAP_DNS
/*
 * in getdxbyname.c
 */
#ifdef NEEDPROTOS
char **getdxbyname(char *domain);
#else /* NEEDPROTOS */
char **getdxbyname();
#endif /* NEEDPROTOS */
#endif /* LDAP_DNS */


#ifdef	__cplusplus
}
#endif

#endif	/* _LDAP_INT_H */
