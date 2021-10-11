/*
 * Copyright (c) 1993 - 1995 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)dns_ops.cc	1.14	97/10/21 SMI"

/*
 * this started out as a DNS listing capability only.
 * it is currently growing to implement a complete DNS client
 * side interface.
 */

#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include <sys/socket.h>
#include <netinet/in.h>
#undef NOERROR
#include <arpa/inet.h>
#include <arpa/nameser.h>
/* #undef NOERROR */
#include <resolv.h>

#define	DNS_RES_NOERROR	NOERROR

#include "dns_ops.hh"


#define	TRACE	(0)			// call tracing
#define	MAXRPLY	(4096)			// max reply packet size from DNS call
#define	MAXTXT	(2048)			// max TXT packet size from DNS call


/*
 * the function prototypes are removed because they're defined in resolv.h
 * 
 * warning:  mostly MT unsafe at this point.
 */

static unsigned short parse_ushort(const char **pp);
static unsigned long parse_ulong(const char **pp);
static int txt_expand(unsigned char *cp, int dlen, unsigned char *txt,
    int txt_len);
static int mwrite(int fd, const void *buf, int buf_len);
static int mread(int fd, void *buf, int buf_len);


mutex_t	dns_client::class_mutex = DEFAULTMUTEX;
int	dns_client::dns_trace = TRACE;
int	dns_client::res_init_called = 0;



dns_client::dns_client()
{
}


dns_client::~dns_client()
{
}


/*
 * query a server for given DNS domain for its data.
 *	- contact appropriate server for dom.
 *	- adds list of hosts in that dom to h.
 *	- adds list of (sub)domains in that dom to d.
 *	- returns 0 on success, -1 on error.
 *	- h and d may contain partial info on error.
 *	- asking for an empty domoain is currently not an error.
 */

int
dns_client::list_names(const char *dom, dns_rr_vec &dom_vec)
{
	union {
		HEADER	h;
		char	buf[MAXRPLY];
	} qbuf;
	int		qlen;
	u_short		net_s;
	int		dns_fd;
	char		*soa;
	dns_rr		server_rr(dom);
	int		rval;
	char		qual_dom[MAXDNAME];
	char		suffix_buf[MAXDNAME];
	const char	*suffix;
	const char	*qual;

	mutex_lock(&class_mutex);

	if (do_res_init() < 0) {
		mutex_unlock(&class_mutex);
		return (-1);
	}

	if (get_servers(dom, server_rr, &qual) < 0) {
		if (dns_trace)
			fprintf(stderr,
	"dns_client::list_names: get_dom_servers_inet(\"%s\") failed\n",
			    dom);
		mutex_unlock(&class_mutex);
		return (-1);
	}
	if (qual) {
		/*
		 * name was resolved with a qualifier.
		 * if we did a list on "eng", it would probably have been
		 * qualified with "sun.com".  so, the actual domain name
		 * is "eng.sun.com".
		 */

		size_t	qual_len;

		qual_len = strlen(qual);
		if (qual_len > 0 && qual[qual_len - 1] == '.')
			sprintf(qual_dom, "%s.%.*s", dom, (int) qual_len - 1, qual);
		else
			sprintf(qual_dom, "%s.%s", dom, qual);
		dom = qual_dom;
		suffix = dom;
	} else {
		size_t	dom_len;

		/*
		 * the actual names stored in DNS tables does not have
		 * a trailing '.', so make sure we don't compare for one.
		 */
		dom_len = strlen(dom);
		if (dom_len > 0 && dom[dom_len - 1] == '.') {
			memcpy(suffix_buf, dom, dom_len - 1);
			suffix_buf[dom_len - 1] = '\0';
			suffix = suffix_buf;
		} else
			suffix = dom;
	}

	// format query msg

	qlen = res_mkquery(QUERY, dom, C_IN, T_AXFR,
		(const u_char *)0, 0, (const u_char *)0,
		(u_char *)&qbuf, sizeof (qbuf));
	if (qlen < 0) {
		if (dns_trace)
			fprintf(stderr,
			    "dns_client::list_names: res_mkquery() failed\n");
		mutex_unlock(&class_mutex);
		return (-1);
	}

	// connect to name server and send query pkt

	if ((dns_fd = connect_inet(server_rr.in__vec)) < 0) {
		if (dns_trace)
			fprintf(stderr,
			    "dns_client::list_names: connect_inet failed\n");
		mutex_unlock(&class_mutex);
		return (-1);
	}

	net_s = htons(qlen);
	if (mwrite(dns_fd, &net_s, sizeof (net_s)) != sizeof (net_s)) {
		if (dns_trace)
			fprintf(stderr, "mwrite size failed\n");
		disconnect(dns_fd);
		mutex_unlock(&class_mutex);
		return (-1);
	}
	if (mwrite(dns_fd, &qbuf, qlen) != qlen) {
		if (dns_trace)
			fprintf(stderr, "mwrite msg failed\n");
		disconnect(dns_fd);
		mutex_unlock(&class_mutex);
		return (-1);
	}

	// process rply pkt stream

	soa = 0;
	while ((rval = decode_zone(dns_fd, suffix, dom_vec, &soa)) > 0)
		;
	if (rval < 0) {
		if (soa)
			free(soa);
		disconnect(dns_fd);
		mutex_unlock(&class_mutex);
		return (-1);
	}
	if (soa) {
		free(soa);
		disconnect(dns_fd);
		mutex_unlock(&class_mutex);
		return (0);
	}
	// we should have seen an SOA record
	if (dns_trace)
		fprintf(stderr,
		    "dns_client::list_names: no SOA in response?\n");
	disconnect(dns_fd);
	mutex_unlock(&class_mutex);
	return (-1);
}

/*
 * make sure libresolv is initialized
 */

int
dns_client::do_res_init()
{
	if (!res_init_called) {
		// UDP much faster for small exchanges
		// _res.options |= RES_USEVC;	// not MT-safe
		if (res_init() < 0) {
			if (dns_trace)
				fprintf(stderr, "res_init() failed\n");
			return (-1);
		}
		res_init_called = 1;
	}
	return (0);
}


/*
 * given an open fd to a DNS that has been sent a T_AXFR query,
 * process one reply pkt.
 *
 *	- *soa is state information for the host/domain.
 *	- each call processes one response msg.  this should be called until
 *	  the return value is 0 (for EOF).
 *	- return 1 on success, 0 on EOF, -1 on error.
 *	- may leave partial info in d and h on error.
 *	- dom arg should be domain suffix of this domain,
 *	  but without trailing '.'.
 */

int
dns_client::decode_zone(
	int dns_fd,
	const char *dom,
	dns_rr_vec &dom_vec,
	char **soa)
{
	union {
		    char	abuf[MAXRPLY];
		    HEADER	hdr;
	} pkt;
	char		*abuf_bound;
	u_short		net_s;
	int		s;
	char		rrname[MAXDNAME];
	int		nlen;
	const char	*p;
	u_short		type;
	u_short		_class, rdata_len;
	u_long		ttl;
	int		cc;
	size_t		dom_len = strlen(dom);

	if (dns_trace) {
		if (*soa)
			fprintf(stderr, "(soa=<%s>)\n", *soa);
		else
			fprintf(stderr, "(no SOA yet)\n");
	}

	// get size short

	if (mread(dns_fd, &net_s, sizeof (net_s)) != sizeof (net_s)) {
		if (dns_trace)
			fprintf(stderr, "mread size failed\n");
		return (-1);
	}

	s = ntohs(net_s);

	if (dns_trace)
		fprintf(stderr, "resp pkt len %d\n", s);

	if (s == 0)
		return (0);

	if (s > sizeof (pkt)) {
		if (dns_trace)
			fprintf(stderr, "response pkt too big\n");
		return (-1);
	}

	if ((cc = mread(dns_fd, &pkt, s)) != s) {
		if (dns_trace)
			fprintf(stderr,
			    "mread data part failed (%d != expected %d)\n",
			    cc, s);
		goto error;
	}

	abuf_bound = pkt.abuf + s;

	p = pkt.abuf + sizeof (pkt.hdr);

	if (p > abuf_bound)
		goto error;

	if (dns_trace) {
		switch (pkt.hdr.rcode) {
		case DNS_RES_NOERROR:
			fprintf(stderr, "hdr.rcode NOERROR\n");
			break;
		case FORMERR:
			fprintf(stderr, "hdr.rcode FORMERR\n");
			break;
		case NXDOMAIN:
			fprintf(stderr, "hdr.rcode NXDOMAIN\n");
			break;
		case NOTIMP:
			fprintf(stderr, "hdr.rcode NOTIMP\n");
			break;
		default:
			fprintf(stderr, "hdr.rcode %d\n", pkt.hdr.rcode);
		}
		fprintf(stderr, "hdr.qdcount %d\n", ntohs(pkt.hdr.qdcount));
		fprintf(stderr, "hdr.ancount %d\n", ntohs(pkt.hdr.ancount));
		fprintf(stderr, "hdr.nscount %d\n", ntohs(pkt.hdr.nscount));
		fprintf(stderr, "hdr.arcount %d\n", ntohs(pkt.hdr.arcount));
	}

	if (pkt.hdr.rcode != DNS_RES_NOERROR)
		goto error;

	if (ntohs(pkt.hdr.qdcount) > 0) {
		nlen = dn_skipname((const u_char *)p,
		    (const u_char *)abuf_bound);
		if (nlen < 0)
			goto error;
		p += nlen + QFIXEDSZ;
	}

	rrname[0] = '\0';
	if ((nlen = dn_expand((const u_char *)&pkt, (const u_char *)abuf_bound,
			(const u_char *)p, (char *)rrname, sizeof (rrname)))
			< 0)
		goto error;
	p += nlen;

	if (dns_trace)
		fprintf(stderr, "rrname: <%s>\n",
			rrname[0] != '\0'? rrname: "(root)");

	type = parse_ushort(&p);
	if (type == T_SOA) {
		char	buf[MAXDNAME];
		int	blen;

		if (dns_trace)
			fprintf(stderr, "\ttype\tT_SOA\n");
		if (*soa == 0) {
			*soa = strdup(rrname);
			if (*soa == 0)
				goto error;
		} else if (strcasecmp(*soa, rrname) == 0) {
			// got same name -- encapsulates list
			return (0);
		} else {
			if (dns_trace)
				fprintf(stderr,
				    "unexpected SOA <%s>\n", rrname);
			goto error;
		}
		blen = dn_expand((u_char *)&pkt, (const u_char *)abuf_bound,
			(const u_char *)p, (char *)buf, sizeof (buf));
		if (blen <= 0) {
			if (dns_trace)
				fprintf(stderr, "SOA name??\n");
			goto error;
		}
		if (dns_trace)
			fprintf(stderr, "\tSOA name: <%s>[%d]\n",
				(blen > 0 && buf[0] != '\0')?
					buf: "(none)", blen);
		p += blen;

		return (1);
	}
	_class = parse_ushort(&p);
	ttl = parse_ulong(&p);
	rdata_len = parse_ushort(&p);

	size_t	rr_len;
	size_t	i;

	rr_len = strlen(rrname);
	i = rr_len - dom_len;
	if (i > 0 && rrname[i - 1] == '.' &&
	    strcasecmp(&rrname[i], dom) == 0) {
		// convert "foo.sun.com" to "foo"
		rrname[i - 1] = '\0';
	}

	dns_rr *rr;
	if ((rr = (dns_rr *)dom_vec.lookup(rrname)) == 0) {
		rr = new dns_rr(rrname);
		if (rr == 0)
			goto error;
		if (dom_vec.add(rr) < 0)
			goto error;
	}

	if (parse_rrdata(type, (const char *)&pkt,
	    (const char *)abuf_bound, p, *rr) < 0)
		goto error;

	if (dns_trace) {
		switch (_class) {
		case C_IN:
			fprintf(stderr, "\tclass\tC_IN\n");
			break;
		default:
			fprintf(stderr, "\tclass\t%d\n", _class);
		}
		fprintf(stderr, "\tttl\t%ld\n", ttl);
		fprintf(stderr, "\trdata_len\t%d\n", rdata_len);
		fprintf(stderr, "\n");
	}

	if (*soa == 0)
		goto error;		// first pkt should have been SOA

	return (1);

error:
	return (-1);
}


/* *** */


int
dns_client::trace_level(int level)
{
	int	olevel;

	mutex_lock(&class_mutex);
	olevel = dns_trace;
	dns_trace = level;
	mutex_unlock(&class_mutex);
	return (olevel);
}


const char *
dns_client::get_def_dom()
{
	mutex_lock(&class_mutex);
	if (do_res_init() < 0) {
		mutex_unlock(&class_mutex);
		return (0);
	}
	mutex_unlock(&class_mutex);
	return (_res.defdname);
}


/*
 * if the domain name in /etc/resolv.conf is "eng.sun.com.", then
 * the search list is ("eng.sun.com.", "sun.com.", "com.")
 *
 * if the domain name in /etc/resolv.conf is "eng.sun.com", then
 * the search list is ("eng.sun.com", "sun.com")
 *
 * if there is no domain name in /etc/resolv.conf, then
 * the search list is ("eng.sun.com", "sun.com")
 */

const char *
dns_client::get_root_dom()
{
	char	**ext;

	mutex_lock(&class_mutex);
	if (do_res_init() < 0) {
		mutex_unlock(&class_mutex);
		return (".");
	}
	mutex_unlock(&class_mutex);

	int	d = -1;

	ext = _res.dnsrch;
	if (ext[0]) {
		for (d = 0; ext[d + 1]; ++d)
			;
	}
	for (; d >= 0; --d) {
		dns_rr	rr(ext[d]);

		if (get_servers(ext[d], rr, 0) == 0)
			return (ext[d]);
	}
	return (".");
}

/*
 * return name of name servers for given domain.
 */

int
dns_client::get_servers(const char *dom, dns_rr &rr, const char **qualp)
{
	union {
		HEADER	h;
		char	buf[MAXRPLY];
	} abuf;
	int		alen;

	if (do_res_init() < 0)
		return (-1);

	alen = query(dom, T_NS, 0,
	    (unsigned char *)&abuf, sizeof (abuf), qualp);
	if (alen < 0) {
		if (dns_trace)
			fprintf(stderr,
			    "get_server_names: res_query() failed\n");
		return (-1);
	}

	if (unpack((u_char *)&abuf, (u_char *)&abuf + alen, rr, 1) < 0)
		return (-1);

	return (0);
}


/*
 * convert hostname to in_addr(s)
 * returns 0 on success, or -1 on error.
 * {a,t}_b content is undefined on error.
 *
 * this mimics the behavior of libresolv.
 */

int
dns_client::lookup_name(const char *host, dns_rr &rr,
	unsigned int authoritative)
{
	union {
		HEADER	h;
		char	buf[MAXRPLY];
	} abuf;
	int		alen;

	mutex_lock(&class_mutex);

	if (do_res_init() < 0)
		goto error;

	if ((alen = query(host, T_ANY, authoritative,
	    (unsigned char *)&abuf, sizeof (abuf), 0)) < 0)
		goto error;

	if (unpack((u_char *)&abuf, (u_char *)&abuf + alen, rr, 0) < 0)
		goto error;

	mutex_unlock(&class_mutex);
	return (0);
error:
	mutex_unlock(&class_mutex);
	return (-1);
}

/*
 * implement DNS search rules.
 *
 *	- if name has a trailing '.', assume it is fully qualified.  no
 *	  searching is done.
 */

int
dns_client::query(
	const char *host,
	int qt,
	unsigned int /* authoritative */, /* currently not supported */
	unsigned char *mbuf,
	int msiz,
	const char **qualp)
{
	char		qualified[MAXDNAME];
	size_t		hlen;
	int		alen;
	char		**ext;

	hlen = strlen(host);
	if (hlen >= 1 && host[hlen - 1] == '.') {
		memcpy(qualified, host, hlen);
		qualified[hlen - 1] = '\0';
		host = qualified;
	} else {
		// DNS domain search up the tree

		for (ext = _res.dnsrch; *ext; ++ext) {
			memcpy(qualified, host, hlen);
			qualified[hlen] = '.';
			strcpy(&qualified[hlen + 1], *ext);
			if (dns_trace)
				fprintf(stderr, "QUAL <%s>\n", qualified);
			alen = res_query(qualified, C_IN, qt, mbuf, msiz);
			if (alen > 0) {
				if (qualp)
					*qualp = *ext;
				return (alen);
			}
		}
	}
	if (dns_trace)
		fprintf(stderr, "UNQUAL <%s>\n", host);
	alen = res_query(host, C_IN, qt, mbuf, msiz);
	if (alen <= 0) {
		if (dns_trace)
			fprintf(stderr,
			    "dns_client::lookup: res_query() failed\n");
		return (-1);
	}
	if (qualp)
		*qualp = 0;
	return (alen);
}


/*
 * parse answers in query result pkt, extracting addr, NS, TXT records.
 *
 * m must be struct HEADER aligned
 *
 * returns 0 on success,  -1 on error.
 *
 * the names are strdup'd, but the addresses are copied.
 * a_b, n_b, t_b contents are undefined on error.
 * values are added to [abt]_b, so any previous entries will
 * be preserved.  it is not an error for [abt]_b to be returned empty.
 */

int
dns_client::unpack(u_char *m, u_char *m_bound, dns_rr &rr, int aux)
{
	HEADER		*hp;
	int		qdcount;
	int		ancount;
	int		nscount;
	int		arcount;
	int		count;
	unsigned char	*data;
	unsigned char	name[MAXDNAME];
	unsigned char	txt[MAXTXT];
	int		dlen;
	int		type;
	long		_class;
	unsigned long	ttl;
	dns_vec		dv;

	hp = (HEADER *)m;
	data = (unsigned char *)&hp[1];

	qdcount = ntohs(hp->qdcount);
	ancount = ntohs(hp->ancount);
	nscount = ntohs(hp->nscount);
	arcount = ntohs(hp->arcount);

	if (dns_trace) {
		fprintf(stderr, "qdcount %d\n", qdcount);
		fprintf(stderr, "ancount %d\n", ancount);
		fprintf(stderr, "nscount %d\n", nscount);
		fprintf(stderr, "arcount %d\n", arcount);
	}

	while (--qdcount >= 0) {
		dlen = dn_skipname(data, m_bound);
		if (dlen < 0)
			return (-1);
		if (dns_trace)
			fprintf(stderr, "skipping %d + %d\n", dlen, QFIXEDSZ);
		data += dlen + QFIXEDSZ;
	}

	count = ancount;
	if (aux)
		count += arcount;
	while (--count >= 0 && data < m_bound) {
		// for "additional" records, this name may be different
		if ((dlen = dn_expand(m, m_bound,
				data, (char *)name, sizeof (name))) < 0) {
			if (dns_trace)
				fprintf(stderr, "dn_expand() dom failed\n");
			return (-1);
		}
		if (dns_trace)
			fprintf(stderr, "\tname <%s>[%d]\n", name, dlen);
		data += dlen;
		type = parse_ushort((const char **)&data);
		_class = parse_ushort((const char **)&data);
		ttl = parse_ulong((const char **)&data);
		dlen = parse_ushort((const char **)&data);

		if (dns_trace) {
			switch (type) {
			case T_NS:
				fprintf(stderr, "\ttype T_NS\n");
				break;
			case T_CNAME:
				fprintf(stderr, "\ttype T_CNAME\n");
				break;
			case T_A:
				fprintf(stderr, "\ttype T_A\n");
				break;
			case T_SOA:
				fprintf(stderr, "\ttype T_SOA\n");
				break;
			case T_MX:
				fprintf(stderr, "\ttype T_MX\n");
				break;
			case T_TXT:
				fprintf(stderr, "\ttype T_TXT\n");
				break;
			default:
				fprintf(stderr, "\ttype %d\n", type);
			}
			if (_class == C_IN)
				fprintf(stderr, "\tclass C_IN\n");
			else
				fprintf(stderr, "\tclass $%lx\n", _class);
			fprintf(stderr, "\tttl %ld secs\n", ttl);
			fprintf(stderr, "\tlen %d bytes\n", dlen);
		}

		dns_el		*e;

		switch (type) {
		case T_A:
			struct in_addr	ina;
			dns_in		*e1;

			memcpy(&ina, data, sizeof (ina));
			e1 = new dns_in(ina);
			if (e1 == 0)
				return (-1);
			if (rr.in__vec.add(e1) < 0)
				return (-1);
			break;
		case T_NS:
			dns_str	*e2;

			if (dn_expand(m, m_bound,
					data, (char *)name, sizeof (name)) < 0) {
				if (dns_trace)
					fprintf(stderr,
					    "dn_expand() T_NS failed\n");
				return (-1);
			}
			e2 = new dns_str((char *)name);
			if (e2 == 0)
				return (-1);
			if (rr.ns__vec.add(e2) < 0)
				return (-1);
			if (dns_trace)
				fprintf(stderr, "\tNS=<%s>\n", name);
			break;
		case T_TXT:
			if (dlen > 0) {
				if (txt_expand(data, dlen,
						txt, sizeof (txt)) < 0) {
					if (dns_trace)
						fprintf(stderr,
						"dn_expand() T_TXT failed\n");
					return (-1);
				}
				e = new dns_str((char *)txt);
				if (e == 0)
					return (-1);
				if (rr.txt_vec.append(e) < 0)
					return (-1);
			} else
				txt[0] = '\0';
			if (dns_trace)
				fprintf(stderr, "\tTXT=<%s>\n", txt);
			break;
		}
		data += dlen;
	}

	return (0);
}


/*
 * connect to the first available DNS server.  in_b is an ordered list of
 * candidate servers (struct in_addrs).
 * returns a connected socket or -1 on error.
 *
 * when connect fails (eg. due to a timeout),
 * we get a SIGPIPE on the next connect, so we have
 * to close/reopen the socket for every connect attempt.
 */

int
dns_client::connect_inet(dns_vec &in_vec)
{
	int			in_c;
	dns_el			**in_v;
	struct sockaddr_in	sin;
	int			so;
	int			ns;

	in_vec.get_vec(in_c, in_v);

	sin.sin_family = AF_INET;
	sin.sin_port = htons(NAMESERVER_PORT);
	memset(&sin.sin_zero, 0, sizeof (sin.sin_zero));

	for (ns = 0; ns < in_c; ++ns) {
		if ((so = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			if (dns_trace)
				perror("socket");
			return (-1);
		}
		sin.sin_addr = ((dns_in *)in_v[ns])->ina;
		if (connect(so, (struct sockaddr *)&sin, sizeof (sin)) == 0)
			return (so);
		close(so);
	}

	if (dns_trace)
		fprintf(stderr,
"dns_client::connect_inet: could not connect to any DNS server\n");
	return (-1);
}


void
dns_client::disconnect(int fd)
{
	close(fd);
}


/* *** */


static unsigned short
parse_ushort(const char **pp)
{
	const u_char	*p = (const u_char *)*pp;
	unsigned short	val;

	val = (p[0] << 8) | p[1];
	*pp += 2;
	return (val);
}


static unsigned long
parse_ulong(const char **pp)
{
	const u_char	*p = (const u_char *)*pp;
	unsigned long	val;

	val =  ((u_long) p[0] << 24) | ((u_long) p[1] << 16) | ((u_long) p[2] << 8) | (u_long) p[3];
	*pp += 4;
	return (val);
}


int
dns_client::parse_rrdata(
	int type,
	const char *pkt,
	const char *pkt_bound,
	const char *p,
	dns_rr &rr)
{
	switch (type) {
	case T_A:
	{
		// non authoritative address (answer)
		// basically qualifies preceding name, ...
		struct in_addr	ina;
		dns_in		*e1;

		memcpy(&ina, p, sizeof (ina));
		p += sizeof (ina);

		if (dns_trace)
			fprintf(stderr, "\ttype\tT_A <%s>\n", inet_ntoa(ina));

		e1 = new dns_in(ina);
		if (e1 == 0)
			goto error;
		if (rr.in__vec.add(e1) < 0)
			goto error;
	}
		break;
	case T_NS:
	{
		char	buf[MAXDNAME];
		int	blen;
		dns_str	*e2;

		// auth. name server for dom (name of dom)

		blen = dn_expand((u_char *)pkt, (const u_char *)pkt_bound,
			(const u_char *)p, (char *)buf, sizeof (buf));
		if (blen <= 0) {
			if (dns_trace)
				fprintf(stderr, "foul NS record??\n");
			goto error;
		}
		p += blen;

		e2 = new dns_str(buf);
		if (e2 == 0)
			goto error;
		if (rr.ns__vec.add(e2) < 0)
			goto error;

		if (dns_trace)
			fprintf(stderr, "\ttype\tT_NS=<%s>\n", buf);
	}
		break;
	case T_CNAME:
	{
		char	buf[MAXDNAME];
		int	blen;

		// connonical name (answer)
		blen = dn_expand((u_char *)pkt, (const u_char *)pkt_bound,
			(const u_char *)p, (char *)buf, sizeof (buf));
		if (blen <= 0) {
			if (dns_trace)
				fprintf(stderr, "foul CNAME record??\n");
			goto error;
		}
		p += blen;

		if (rr.set_cname(buf) < 0)
			goto error;

		if (dns_trace)
			fprintf(stderr, "\ttype\tT_CNAME=<%s>\n", buf);
	}
		break;
	default:
		if (dns_trace)
			fprintf(stderr, "\ttype\t%d\n", type);
	}
	return (0);
error:
	return (-1);
}


/*
 * "expand" text record into one contiguous string.
 * return -1 if anything goes wrong.
 */

static int
txt_expand(unsigned char *cp, int dlen, unsigned char *txt, int txt_len)
{
	unsigned char	*dbound, *tbound;
	int		i, n;

	dbound = cp + dlen;
	tbound = txt + txt_len;
	while (cp < dbound) {
		n = *cp++;
		for (i = 0; i < n; ++i) {
			if (cp >= dbound)
				return (-1);
			if (txt >= tbound)
				return (-1);
			*txt++ = *cp++;
		}
	}
	if (txt >= tbound)
		return (-1);
	*txt = '\0';
	return (0);
}


static int
mread(int fd, void *_buf, int buf_len)
{
char	*buf = (char *)_buf;
int	i;
int	cc;

	i = 0;
	while (i < buf_len) {
		cc = read(fd, &buf[i], buf_len - i);
		if (cc < 0) {
			if (errno == EINTR) {
				cc = 0;
				continue;
			}
			return (-1);
		}
		if (cc == 0)
			return (i);
		i += cc;
	}
	return (i);
}


static int
mwrite(int fd, const void *_buf, int buf_len)
{
const char	*buf = (const char *)_buf;
int		i;
int		cc;

	i = 0;
	while (i < buf_len) {
		cc = write(fd, &buf[i], buf_len - i);
		if (cc < 0) {
			if (errno == EINTR) {
				cc = 0;
				continue;
			}
			return (-1);
		}
		if (cc == 0)
			return (-1);
		i += cc;
	}
	return (i);
}
