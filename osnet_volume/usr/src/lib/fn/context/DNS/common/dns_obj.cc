/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)dns_obj.cc	1.5 97/10/21 SMI"


#include <string.h>

#include "dns_ops.hh"


static char *
strnew(const char *str)
{
	char	*s;

	if (str) {
		size_t	len;

		len = strlen(str) + 1;
		s = new char[len];
		if (s)
			memcpy(s, str, len);
	} else
		s = 0;
	return (s);
}


long	dns_el::nnodes = 0;


dns_el::dns_el()
{
	++nnodes;
}

dns_el::~dns_el()
{
	--nnodes;
}


dns_vec::dns_vec()
{
	p_count = 0;
	p_maxcount = 0;
	p_vector = 0;
}

dns_vec::~dns_vec()
{
	int	i;

	if (p_vector) {
		for (i = 0; i < p_count; ++i)
			delete p_vector[i];
		delete[] p_vector;
	}
}

#define	MIN_VEC_GROW	(4)
#define	MAX_VEC_GROW	(128)

int
dns_vec::append(dns_el *e)
{
	dns_el	**p;

	if (p_count >= p_maxcount) {
		int	grow;

		grow = p_count >> 2;
		if (grow < MIN_VEC_GROW)
			grow = MIN_VEC_GROW;
		else if (grow > MAX_VEC_GROW)
			grow = MAX_VEC_GROW;
		p = new dns_el *[p_count + grow];
		if (p == 0)
			return (-1);
		memcpy(p, p_vector, p_count * sizeof (*p));
		delete[] p_vector;
		p_vector = p;
		p_maxcount = p_count + grow;
	}
	p_vector[p_count] = e;
	++p_count;
	return (0);
}

void
dns_vec::get_vec(int &ac, dns_el **&av) const
{
	ac = p_count;
	av = p_vector;
}


dns_rr_vec::dns_rr_vec()
{
	p_keycount = 0;
	p_keyvec = 0;
}

dns_rr_vec::~dns_rr_vec()
{
	delete[] p_keyvec;
}

#define	MIN_KEY_GROW	(4)
#define	MAX_KEY_GROW	(2048)

/*
 * start growing in 50% increments until increment size exceeds MAX_KEY_GROW.
 * above MAX_KEY_GROW, grow in 25% increments.
 */

int
dns_rr_vec::add(dns_rr *rr)
{
	if (dns_vec::append(rr) < 0)
		return (-1);

	if (p_count > p_keycount) {
		int	grow;

		grow = p_count >> 1;
		if (grow < MIN_KEY_GROW)
			grow = MIN_KEY_GROW;
		else if (grow > MAX_KEY_GROW)
			grow = MAX_KEY_GROW + ((grow - MAX_KEY_GROW) >> 1);

		if (resize(p_count + grow) < 0)
			return (-1);
	} else {
		unsigned long	hv;

		hv = rr->hash_rname() % p_keycount;
		rr->next = p_keyvec[hv];
		p_keyvec[hv] = rr;
	}

	return (0);
}

dns_rr *
dns_rr_vec::lookup(const char *name)
{
	dns_rr		*rr;
	unsigned long	hv;

	if (p_keycount == 0)
		return (0);
	hv = dns_rr::hash_rname(name) % p_keycount;
	for (rr = p_keyvec[hv]; rr; rr = rr->next) {
		if (strcasecmp(name, rr->p_rname) == 0)
			return (rr);
	}
	return (0);
}

/*
 * this resize is fairly expensive, so we try not to do it too
 * often.
 */

int
dns_rr_vec::resize(int s)
{
	dns_rr		**rrp, *rr;
	unsigned long	hv;
	int		i;

	rrp = new dns_rr *[s];
	if (rrp == 0)
		return (-1);
	memset(rrp, 0, s * sizeof (*rrp));
	delete[] p_keyvec;
	p_keyvec = rrp;
	p_keycount = s;

	for (i = 0; i < p_count; ++i) {
		rr = (dns_rr *)p_vector[i];
		hv = rr->hash_rname() % p_keycount;
		rr->next = p_keyvec[hv];
		p_keyvec[hv] = rr;
	}

	return (0);
}

dns_in::dns_in(const struct in_addr &a)
{
	ina = a;
}

dns_in::~dns_in()
{
}

const struct in_addr *
dns_in::in() const
{
	return (&ina);
}

int
dns_in::eq(const dns_in *el) const
{
	if (memcmp(&ina, &el->ina, sizeof (ina)) == 0)
		return (1);
	else
		return (0);
}


in_vec::in_vec()
{
}

in_vec::~in_vec()
{
}

int
in_vec::add(dns_in *el)
{
	int	i;

	for (i = 0; i < p_count; ++i) {
		if (el->eq((dns_in *)p_vector[i])) {
			delete el;
			return (0);
		}
	}

	return (dns_vec::append(el));
}


dns_str::dns_str(const char *s)
{
	p_str = strnew(s);
}

dns_str::~dns_str()
{
	delete[] p_str;
}

const char *
dns_str::str() const
{
	return (p_str);
}

int
dns_str::eq(const dns_str *el) const
{
	return (strcasecmp(p_str, el->p_str) == 0);
}


ns_vec::ns_vec()
{
}

ns_vec::~ns_vec()
{
}

int
ns_vec::add(dns_str *el)
{
	int	i;

	for (i = 0; i < p_count; ++i) {
		if (el->eq((dns_str *)p_vector[i])) {
			delete el;
			return (0);
		}
	}

	return (dns_vec::append(el));
}


dns_rr::dns_rr(const char *name)
{
	p_rname = strnew(name);
	canon_name = 0;
	next = 0;
}

dns_rr::~dns_rr()
{
	delete[] p_rname;
	delete[] canon_name;
}

const char *
dns_rr::rname() const
{
	return (p_rname);
}

int
dns_rr::set_cname(const char *n)
{
	delete[] canon_name;
	if (n) {
		size_t	len;

		len = strlen(n) + 1;
		canon_name = new char[len];
		if (canon_name == 0)
			return (-1);
		memcpy(canon_name, n, len);
	} else
		canon_name = 0;
	return (0);
}

const char *
dns_rr::get_cname() const
{
	return (canon_name);
}

unsigned long
dns_rr::hash_rname()
{
	return (hash_rname(p_rname));
}

#define	HASHSHIFT	(2)
#define	HASHMASK	(0x5f)			// case insensitive char
#define HASHBITS	(0x3fffffff)

unsigned long
dns_rr::hash_rname(const char *name)
{
	const unsigned char	*cp;
	unsigned long		hv;

	hv = 0;
	if ((cp = (const unsigned char *)name)) {
		while (*cp != '\0') {
			hv = (hv << HASHSHIFT) ^ hv;
			hv += *cp++ & HASHMASK;
			hv &= HASHBITS;
		}
	}
	return (hv);
}
