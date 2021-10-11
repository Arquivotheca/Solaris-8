/*
 * Copyright (c) 1993 - 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cx.cc	1.13	99/10/13 SMI"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include "cx.hh"

/*
 * QBUF is the max len of a qualified name.  It is used as the result of
 * the concatenation of two valid domain names, hence double the length.
 */

#define	QBUF	(MAXDNAME + MAXDNAME + 10)

class dns_nl : public FN_namelist {
public:
	dns_rr_vec	names;
	int		next_rr;
	int		ac;
	dns_el		**av;

	dns_nl();
	~dns_nl();
	dns_nl(const dns_nl &);			// defeat default
	operator=(const dns_nl &);		// defeat default

	FN_string *next(FN_status &);
};

class dns_bl : public FN_bindinglist {
public:
	dns_rr_vec	names;
	int		next_rr;
	int		ac;
	dns_el		**av;

	dns_bl();
	~dns_bl();
	dns_bl(const dns_bl &);			// defeat default
	operator=(const dns_bl &);		// defeat default

	FN_string *next(FN_ref **, FN_status &);
};

static void my_dom(const char *dom, const char *name, const char **qnamep,
    char *qbuf);
static int make_ref(FN_ref *&, const dns_rr &rr);
static int make_dom_ref(FN_ref *&, const dns_rr &rr);
static int make_nns_ref(FN_ref *&, const dns_rr &rr);
static int make_host_ref(FN_ref *&, const dns_rr &rr);
static int add_inet_addr(FN_ref *, int ac, const dns_in **);
static FN_ref *ref_from_txtid(const char *t);


static const FN_identifier
	svc_ref_type((const unsigned char *)"XFN_SERVICE"),
	host_ref_type((const unsigned char *)"inet_host"),
	dom_ref_type((const unsigned char *)"inet_domain"),
	dom_addr_type((const unsigned char *)"inet_domain"),
	ip_addr_type((const unsigned char *)"inet_ipaddr_string"),
	cname_ref_type((const unsigned char *)"DNS-cname-ref");


DNS_ctx::DNS_ctx(const FN_ref_addr &a, unsigned int auth) : FN_ctx_svc(auth)
{
	size_t		len;
	const char	*dom;

	len = a.length();
	if (len > 0)
		dom = (const char *)a.data();
	self_domain = new char[len + 1];
	if (self_domain) {
		self_domain[len] = '\0';
		if (len > 0)
			memcpy(self_domain, dom, len);
	}

	self_reference = new FN_ref(dom_ref_type);
	self_reference->append_addr(a);

	dns_cl = new dns_client;
	if (dns_cl && trace > 0)
		dns_cl->trace_level(trace - 1);
}

DNS_ctx::~DNS_ctx()
{
	delete[] self_domain;
	delete self_reference;
	delete dns_cl;
}

FN_ref *
DNS_ctx::get_ref(FN_status &s) const
{
	if (trace)
		fprintf(stderr, "DNS_ctx::get_ref() call\n");

	s.set_success();
	return (new FN_ref(*self_reference));
}

/*
 * The application did a lookup(".../foo.bar.sun.com").
 * Cases that should work:
 *	".../foo"
 *	".../foo.bar"
 *	".../foo.bar.sun.com"
 *	".../foo.bar.sun.com."
 *	".../other-host.other-domain"
 * Cases that should fail:
 *	".../foo.eng.sun."
 */

FN_ref *
DNS_ctx::c_lookup(const FN_string &name, unsigned int, FN_status_csvc &cs)
{
	FN_ref		*ref;
	char		qbuf[QBUF];
	const char	*n, *qname;

	n = (const char *)name.str();

	if (trace)
		fprintf(stderr, "DNS_ctx::c_lookup(\"%s\")\n", n);

	my_dom(self_domain, n, &qname, qbuf);

	if (trace)
		fprintf(stderr, "DNS_ctx::c_lookup: qname \"%s\"\n", qname);

	dns_rr	rr(qname);

	if (dns_cl->lookup_name(qname, rr, authoritative) < 0) {
		cs.set_error(FN_E_NAME_NOT_FOUND, *self_reference, name);
		return (0);
	}
	if (make_ref(ref, rr) < 0) {
		cs.set_error(FN_E_NAME_NOT_FOUND, *self_reference, name);
		return (0);
	}
	cs.set_success();
	return (ref);
}

FN_ref *
DNS_ctx::c_lookup_nns(const FN_string &name, unsigned int, FN_status_csvc &cs)
{
	FN_ref		*ref;
	char		qbuf[QBUF];
	const char	*n, *qname;

	n = (const char *)name.str();

	if (trace)
		fprintf(stderr, "DNS_ctx::c_lookup_nns(\"%s\")\n", n);

	my_dom(self_domain, n, &qname, qbuf);

	if (trace)
		fprintf(stderr, "DNS_ctx::c_lookup_nns: qname \"%s\"\n", qname);

	dns_rr	rr(qname);

	if (dns_cl->lookup_name(qname, rr, authoritative) < 0) {
		cs.set_error(FN_E_NAME_NOT_FOUND, *self_reference, name);
		return (0);
	}
	if (make_nns_ref(ref, rr) < 0) {
		cs.set_error(FN_E_NAME_NOT_FOUND, *self_reference, name);
		return (0);
	}
	cs.set_success();
	return (ref);
}

/*
 * produce list of names in context
 */

FN_namelist *
DNS_ctx::c_list_names(const FN_string &name, FN_status_csvc &cs)
{
	dns_nl			*nl;
	const char		*n;
	char			qbuf[QBUF];
	const char		*qname;

	n = (const char *)name.str();

	if (trace)
		fprintf(stderr, "DNS_ctx::c_list_names(\"%s\") call\n", n);

	my_dom(self_domain, n, &qname, qbuf);

	if ((nl = new dns_nl) == 0) {
		cs.set_error(FN_E_INSUFFICIENT_RESOURCES, *self_reference,
		    name);
		return (0);
	}

	if (get_names(dns_cl, qname, nl->names, 0, authoritative) < 0) {
		if (trace)
			fprintf(stderr,
"DNS_ctx::c_list_names(\"%s\") get_names failed\n", n);
		delete nl;
		cs.set_error(FN_E_NOT_A_CONTEXT, *self_reference, name);
		return (0);
	}

	cs.set_success();
	return (nl);
}

dns_nl::dns_nl()
{
	next_rr = 0;
	ac = 0;
	av = 0;
}

dns_nl::~dns_nl()
{
}

FN_string *
dns_nl::next(FN_status &s)
{
	dns_rr		*rr;
	FN_string	*n;

	if (av == 0) {
		next_rr = 0;
		names.get_vec(ac, av);
	}

	while (next_rr < ac) {
		rr = (dns_rr *)av[next_rr++];
		if (rr->get_cname())
			continue;
		n = new FN_string((const unsigned char *)rr->rname());
		s.set_success();
		return (n);
	}

	s.set_success();
	return (0);
}

FN_bindinglist *
DNS_ctx::c_list_bindings(const FN_string &name, FN_status_csvc &cs)
{
	dns_bl			*bl;
	const char		*n;
	char			qbuf[QBUF];
	const char		*qname;

	n = (const char *)name.str();

	if (trace)
		fprintf(stderr, "DNS_ctx::c_list_bindings(\"%s\") call\n", n);

	my_dom(self_domain, n, &qname, qbuf);

	if ((bl = new dns_bl) == 0) {
		cs.set_error(FN_E_INSUFFICIENT_RESOURCES, *self_reference,
		    name);
		return (0);
	}

	if (get_names(dns_cl, qname, bl->names, 1, authoritative) < 0) {
		if (trace)
			fprintf(stderr,
"DNS_ctx::c_list_bindings(\"%s\") get_names failed\n", n);
		delete bl;
		cs.set_error(FN_E_NOT_A_CONTEXT, *self_reference, name);
		return (0);
	}

	cs.set_success();
	return (bl);
}

dns_bl::dns_bl()
{
	next_rr = 0;
	ac = 0;
	av = 0;
}

dns_bl::~dns_bl()
{
}

FN_string *
dns_bl::next(FN_ref **ref, FN_status &s)
{
	dns_rr		*rr;
	FN_string	*n;

	if (av == 0) {
		next_rr = 0;
		names.get_vec(ac, av);
	}

	while (next_rr < ac) {
		rr = (dns_rr *)av[next_rr++];
		if (rr->get_cname())
			continue;
		n = new FN_string((const unsigned char *)rr->rname());
		if (make_ref(*ref, *rr) < 0) {
			s.set_code(FN_E_INSUFFICIENT_RESOURCES);
			return (0);
		}
		s.set_success();
		return (n);
	}
	s.set_success();
	return (0);
}

FN_attrset *
DNS_ctx::c_get_syntax_attrs(const FN_string &name, FN_status_csvc &cs)
{
	// Dotted right-to-left (case insensitive).

	static const FN_string	dot_sep((unsigned char *)".");
	static const FN_string	esc((unsigned char *)"\\");
	static const FN_string	quote((unsigned char *)"\"");
	static const FN_syntax_standard
	    my_syntax(FN_SYNTAX_STANDARD_DIRECTION_RTL,
	    FN_STRING_CASE_INSENSITIVE, &dot_sep, &esc, &quote);

	FN_attrset *aset;

	if (trace)
		fprintf(stderr, "DNS_ctx::c_get_syntax_attrs() call\n");

	aset = my_syntax.get_syntax_attrs();
	if (aset)
		cs.set_success();
	else
		cs.set_error(FN_E_INSUFFICIENT_RESOURCES, *self_reference,
		    name);
	return (aset);
}

FN_namelist *
DNS_ctx::c_list_names_nns(const FN_string &name, FN_status_csvc &cs)
{
	FN_ref	*ref;

	if (trace)
		fprintf(stderr, "DNS_ctx::c_list_names_nns(\"%s\") call\n",
		    (const char *)name.str());

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}

FN_bindinglist *
DNS_ctx::c_list_bindings_nns(const FN_string &name, FN_status_csvc &cs)
{
	FN_ref	*ref;

	if (trace)
		fprintf(stderr, "DNS_ctx::c_list_bindings_nns(\"%s\") call\n",
		    (const char *)name.str());

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}

FN_attrset *
DNS_ctx::c_get_syntax_attrs_nns(const FN_string &name, FN_status_csvc &cs)
{
	FN_ref	*ref;

	if (trace)
		fprintf(stderr, "DNS_ctx::c_get_syntax_attrs_nns() call\n");

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}

/* *** */


int	DNS_ctx::trace = 0;


int
DNS_ctx::trace_level(int level)
{
	int	olevel;

	olevel = trace;
	trace = level;
	return (olevel);
}

static void
my_dom(const char *domain, const char *n, const char **qnamep, char *qbuf)
{
	if (domain && domain[0] != '\0') {
		if (n && n[0] != '\0') {
			sprintf(qbuf, "%.*s.%.*s", MAXDNAME, n, MAXDNAME,
			    domain);
			*qnamep = qbuf;
		} else
			*qnamep = domain;
	} else {
		if (n && n[0] != '\0') {
			*qnamep = n;
		} else {
			const char	*cp;
			size_t		len;

			cp = dns_client::get_root_dom();
			len = strlen(cp);
			if (len > 0 && cp[len - 1] == '.')
				sprintf(qbuf, "%s", cp);
			else
				sprintf(qbuf, "%s.", cp);
			*qnamep = qbuf;
		}
	}
	if (DNS_ctx::get_trace_level())
		fprintf(stderr, "my_dom: <%s><%s> expanded to <%s>\n",
			n, domain? domain: "", *qnamep);
}

int
DNS_ctx::get_names(dns_client *dns_cl, const char *dom, dns_rr_vec &names,
    int all, unsigned int auth)
{
	if (dns_cl->list_names(dom, names) < 0)
		return (-1);
	if (all) {
		int	r;
		int	rc;
		dns_el	**rv;
		dns_rr	*rp;
		int	nc;
		dns_el	**nv;

		names.get_vec(rc, rv);
		for (r = 0; r < rc; ++r) {
			rp = (dns_rr *)rv[r];
			rp->ns__vec.get_vec(nc, nv);
			if (nc > 0)
				dns_cl->lookup_name(rp->rname(), *rp, auth);
#ifdef DEBUG
			{
				int	i, ac;
				dns_el	**av;

				rp->txt_vec.get_vec(ac, av);
				for (i = 0; i < ac; ++i)
					fprintf(stderr, "TXT=%s\n",
					    ((dns_str *)av[i])->str());
			}
#endif
		}
	}
	if (trace) {
		int	ac;
		dns_el	**av;

		names.get_vec(ac, av);
		fprintf(stderr, "get_names: got %d names\n", ac);
	}
	return (0);
}


static int
make_ref(FN_ref *&ref, const dns_rr &rr)
{
	int		ec;
	dns_el		**ev;

	if (rr.get_cname()) {
		ref = new FN_ref(cname_ref_type);
		if (ref == 0)
			return (-1);
		return (0);
	}

	rr.ns__vec.get_vec(ec, ev);
	if (ec > 0)
		return (make_dom_ref(ref, rr));
	else
		return (make_host_ref(ref, rr));
}

/*
 * Convert list of in_addr(s) to FN_ref_addr and insert into FN_ref.
 */

static int
make_host_ref(FN_ref *&ref, const dns_rr &rr)
{
	int		ec;
	dns_el		**ev;
	int		rval;

	ref = new FN_ref(host_ref_type);
	if (ref == 0)
		return (-1);

	rr.in__vec.get_vec(ec, ev);
	if ((rval = add_inet_addr(ref, ec, (const dns_in **)ev)) < 0) {
		delete ref;
		ref = 0;
		return (-1);
	}
	return (rval);
}

static int
make_dom_ref(FN_ref *&ref, const dns_rr &rr)
{
	const char	*cp;
	int		ec;
	dns_el		**ev;

	ref = new FN_ref(dom_ref_type);
	if (ref == 0)
		return (-1);

	cp = rr.rname();

	FN_ref_addr	a(dom_addr_type, strlen(cp), cp);

	if (ref->append_addr(a) == 0) {
		delete ref;
		ref = 0;
		return (-1);
	}
	rr.in__vec.get_vec(ec, ev);
	if (add_inet_addr(ref, ec, (const dns_in **)ev) < 0) {
		delete ref;
		ref = 0;
		return (-1);
	}
	return (0);
}

static int
add_inet_addr(FN_ref *ref, int ac, const dns_in **av)
{
	int		i;
	char		*cp;

	for (i = 0; i < ac; ++i) {
		cp = inet_ntoa(*av[i]->in());

		FN_ref_addr	a(ip_addr_type, strlen(cp), cp);

		if (ref->append_addr(a) == 0)
			return (-1);
	}
	return (0);
}

static int
make_nns_ref(FN_ref *&ref, const dns_rr &rr)
{
	int		ec;
	dns_el		**ev;
	dns_str		*tp;

	rr.txt_vec.get_vec(ec, ev);
	if (ec == 0)
		return (-1);

	const char	**tv;
	const char	*cp;
	int		i, t;
	int		have_xfn_txt;

	tv = new const char *[ec];
	if (tv == 0) {
		delete ref;
		return (-1);
	}

	// scan for interesting (XFN*) TXT records

	have_xfn_txt = 0;
	t = 0;
	ref = 0;

	for (i = 0; i < ec; ++i) {
		tp = (dns_str *)ev[i];
		cp = tp->str();
		if (strncmp(cp, "XFN", 3) == 0) {
			have_xfn_txt = 1;
			if (strncmp(&cp[3], "REF", 3) == 0 &&
			    (cp[6] == ' ' || cp[6] == '\t' ||
			    cp[6] == '\0')) {
				if ((ref = ref_from_txtid(&cp[6])) == 0)
					return (-1);
			} else
				tv[t++] = &cp[3];
		}
	}
	if (!have_xfn_txt) {
		// nothing for us, bail out
		delete[] tv;
		return (-1);
	}
	if (ref == 0) {
		// default ref type is XFN_SERVICE
		if ((ref = new FN_ref(svc_ref_type)) == 0) {
			delete[] tv;
			return (-1);
		}
	}
	if (t > 0) {
		FN_status	s;

		if (DNS_ctx::addrs_from_txt(t, tv, *ref, s) == 0) {
			delete[] tv;
			delete ref;
			return (-1);
		}
	}
	delete[] tv;
	return (0);
}

static FN_ref *
ref_from_txtid(const char *t)
{
	FN_identifier	id;

	for (; *t != '\0'; ++t) {
		if (*t != ' ' && *t != '\t')
			break;
	}
	if (strncmp(t, "STRING", 6) == 0) {
		id.info.format = FN_ID_STRING;
		t += 6;
	} else if (strncmp(t, "UUID", 4) == 0) {
		id.info.format = FN_ID_DCE_UUID;
		t += 4;
	} else if (strncmp(t, "OID", 3) == 0) {
		id.info.format = FN_ID_ISO_OID_STRING;
		t += 3;
	} else
		return (0);
	if (*t != ' ' && *t != '\t')
		return (0);
	for (; *t != '\0'; ++t) {
		if (*t != ' ' && *t != '\t')
			break;
	}
	id.info.length = strlen(t);
	if (id.info.contents = new char[id.info.length])
		memcpy(id.info.contents, t, id.info.length);
	return (new FN_ref(id));
}
