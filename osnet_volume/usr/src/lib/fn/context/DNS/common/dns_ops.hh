/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNS_DNS_OPS_HH
#define	_FNS_DNS_OPS_HH

#pragma ident	"@(#)dns_ops.hh	1.7	96/03/31 SMI"

/*
 * dns_ops.hh is a DNS interface module
 */

#include <synch.h>

#include "dns_obj.hh"

/*
 * most of this stuff is MT-unsafe since the res_*() stuff is unsafe.
 */

class dns_client {
	static mutex_t	class_mutex;
	static int	dns_trace;
	static int	res_init_called;

	static int do_res_init(void);

	int decode_zone(
		int dns_fd,
		const char *dom,
		dns_rr_vec &,
		char **soa);
	static int connect_inet(dns_vec &);
	static void disconnect(int dns_fd);
		// disconnect from DNS server
	static int unpack(
		unsigned char *m,
		unsigned char *m_bound,
		dns_rr &rr,
		int aux);
	static int get_servers(const char *dom, dns_rr &, const char **qual);
	static int parse_rrdata(int type, const char *pkt,
		const char *pkt_bound, const char *p, dns_rr &rr);
	static int query(
		const char *host,
		int qt,
		unsigned int authoritative,
		unsigned char *mbuf,
		int msiz,
		const char **qual);
public:
	dns_client();
	~dns_client();
	dns_client(const dns_client &);		// disable default
	operator=(const dns_client &);		// disable default

	static int trace_level(int);
	static const char *get_def_dom(void);
		// gets default domain name
		// caller does not free
		// returns 0 on error
		// value valid only until next method invocation
	static const char *get_root_dom(void);

	int list_names(const char *dom, dns_rr_vec &);
		// get list of host-names and domain-names in <dom>
		// returns 0 for success, -1 for error
	int lookup_name(const char *name, dns_rr &, unsigned int auth);
};

#endif /* _FNS_DNS_OPS_HH */
