#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)dhcp.spec	1.7	99/10/23 SMI"
#
# lib/libdhcp/spec/dhcp.spec

function	ipv4cksum
include		<sys/types.h>
include		<netinet/in_systm.h>
include		<netinet/in.h>
include		<netinet/ip.h>
include		<netinet/udp.h>
include		<v4_sum_impl.h>
declaration	uint16_t ipv4cksum(uint16_t *cp, uint16_t count)
version		SUNWprivate_1.1
end

function	udp_chksum
include		<sys/types.h>
include		<netinet/in_systm.h>
include		<netinet/in.h>
include		<netinet/ip.h>
include		<netinet/udp.h>
include		<v4_sum_impl.h>
declaration	uint16_t udp_chksum(struct udphdr *udph, \
		    const struct in_addr *src, const struct in_addr *dst, \
		    const uint8_t proto)
version		SUNWprivate_1.1
end

function	ascii_to_octet
include		<netinet/dhcp.h>
declaration	int ascii_to_octet(char *asp, int alen, uint8_t *bufp, \
			int *blen)
version		SUNWprivate_1.1
end		

function	octet_to_ascii
include		<netinet/dhcp.h>
declaration	int octet_to_ascii(u_char *nump, int nlen, char *bufp, \
			int *blen)
version		SUNWprivate_1.1
end		

function	_dhcp_options_scan
include		<netinet/dhcp.h>
declaration	int _dhcp_options_scan(PKT_LIST *pl)
version		SUNWprivate_1.1
end

function	list_dd
include		<dd_impl.h>
declaration	int list_dd( uint_t ti, int ns, char *name, char *domain, \
			int *tbl_err, Tbl *tbl, ...)
version		SUNWprivate_1.1
end		

function	make_dd
include		<dd_impl.h>
declaration	int make_dd( uint_t ti, int ns, char *name, char *domain, \
			int *tbl_err, char *user, char *group)
version		SUNWprivate_1.1
end		

function	del_dd
include		<dd_impl.h>
declaration	int del_dd( uint_t ti, int ns, char *name, char *domain, \
			int *tbl_err)
version		SUNWprivate_1.1
end		

function	stat_dd
include		<dd_impl.h>
declaration	int stat_dd( uint_t ti, int ns, char *name, char *domain, \
			int *tbl_err, Tbl_stat **tbl_st)
version		SUNWprivate_1.1
end		

function	add_dd_entry
include		<dd_impl.h>
declaration	int add_dd_entry( uint_t ti, int ns, char *name, \
			char *domain, int *tbl_err, ...)
version		SUNWprivate_1.1
end		

function	mod_dd_entry
include		<dd_impl.h>
declaration	int mod_dd_entry( uint_t ti, int ns, char *name, \
			char *domain, int *tbl_err, ...)
version		SUNWprivate_1.1
end		

function	rm_dd_entry
include		<dd_impl.h>
declaration	int rm_dd_entry( uint_t ti, int ns, char *name, \
			char *domain, int *tbl_err, ...)
version		SUNWprivate_1.1
end		

function	check_dd_access
include		<dd_impl.h>
declaration	int check_dd_access(Tbl_stat *tsp, int *errp)
version		SUNWprivate_1.1
end		

function	dd_ls
include		<dd_impl.h>
declaration	char **dd_ls(int ns, char *domain, int *tbl_err)
version		SUNWprivate_1.1
end

function	dd_ns
include		<dd_impl.h>
declaration	int dd_ns(int *tbl_err, char **pathpp)
version		SUNWprivate_1.1
end		

function	free_dd
include		<dd_impl.h>
declaration	void free_dd(Tbl *tbl)
version		SUNWprivate_1.1
end		

function	free_dd_stat
include		<dd_impl.h>
declaration	void free_dd_stat(Tbl_stat *tbl_st)
version		SUNWprivate_1.1
end		

function	build_dhcp_ipname
include		<dd_impl.h>
declaration	void build_dhcp_ipname(char *namep, struct in_addr *np, \
			struct in_addr *mp)
version		SUNWprivate_1.1
end		

function	_dd_tempfile
include		<dd_impl.h>
declaration	char *_dd_tempfile(const char *dir)
version		SUNWprivate_1.1
end

function	_dd_lock_db
include		<dd_impl.h>
declaration	int _dd_lock_db(char *db, int type, int *fdp)
version		SUNWprivate_1.1
end

function	_dd_unlock_db
include		<dd_impl.h>
declaration	int _dd_unlock_db(int *fdp)
version		SUNWprivate_1.1
end

function	read_dhcp_defaults
include		<dhcdata.h>
declaration	int read_dhcp_defaults(dhcp_defaults_t **)
version		SUNWprivate_1.1
end

function	write_dhcp_defaults
include		<dhcdata.h>
declaration	int write_dhcp_defaults(dhcp_defaults_t *, mode_t)
version		SUNWprivate_1.1
end

function	free_dhcp_defaults
include		<dhcdata.h>
declaration	void free_dhcp_defaults(dhcp_defaults_t *)
version		SUNWprivate_1.1
end

function	delete_dhcp_defaults
include		<dhcdata.h>
declaration	int delete_dhcp_defaults(void)
version		SUNWprivate_1.1
end

function	query_dhcp_defaults
include		<dhcdata.h>
declaration	int query_dhcp_defaults(dhcp_defaults_t *, const char *, \
		    char **)
version		SUNWprivate_1.1
end

function	add_dhcp_defaults
include		<dhcdata.h>
declaration	int add_dhcp_defaults(dhcp_defaults_t **, const char *, \
		    const char *)
version		SUNWprivate_1.1
end

function	replace_dhcp_defaults
include		<dhcdata.h>
declaration	int replace_dhcp_defaults(dhcp_defaults_t **, const char *, \
		    const char *)
version		SUNWprivate_1.1
end

