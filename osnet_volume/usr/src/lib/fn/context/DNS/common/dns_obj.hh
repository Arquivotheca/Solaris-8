/*
 * Copyright (c) 1994-1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNS_DNS_OBJ_HH
#define	_FNS_DNS_OBJ_HH

#pragma ident	"@(#)dns_obj.hh	1.6	96/03/31 SMI"

/*
 * dns_ops.hh is a DNS interface module
 */

#include <netinet/in.h>
#include <arpa/inet.h>


class dns_el {
	static long	nnodes;
public:
	dns_el();
	virtual ~dns_el();
	dns_el(const dns_el &);			// defeat default
	operator=(const dns_el &);		// defeat default
};

class dns_vec {
protected:
	int	p_count;
	int	p_maxcount;
	dns_el	**p_vector;
public:
	dns_vec();
	virtual ~dns_vec();
	dns_vec(const dns_vec &);		// defeat default
	operator=(const dns_vec &);		// defeat default

	int append(dns_el *);
	void get_vec(int &ac, dns_el **&av) const;
};

class dns_in : public dns_el {
public:
	struct in_addr	ina;

	dns_in();				// defeat default
	dns_in(const struct in_addr &);
	~dns_in();
	dns_in(const dns_in &);			// defeat default
	operator=(const dns_in &);		// defeat default

	const struct in_addr *in() const;
	int eq(const dns_in *) const;
};

class in_vec : public dns_vec {
public:
	in_vec();
	virtual ~in_vec();
	in_vec(const in_vec &);			// defeat default
	operator=(const in_vec &);		// defeat default

	int add(dns_in *);
};

class dns_str : public dns_el {
	char		*p_str;
public:
	dns_str();				// defeat default
	dns_str(const char *);			// copied internally
	~dns_str();
	dns_str(const dns_str &);		// defeat default
	operator=(const dns_str &);		// defeat default

	const char *str() const;
	int eq(const dns_str *) const;
};

class ns_vec : public dns_vec {
public:
	ns_vec();
	virtual ~ns_vec();
	ns_vec(const ns_vec &);			// defeat default
	operator=(const ns_vec &);		// defeat default

	int add(dns_str *);
};

class dns_rr : public dns_el {
	char		*p_rname;
	char		*canon_name;
protected:
	dns_rr		*next;
public:
	in_vec		in__vec;
	ns_vec		ns__vec;
	dns_vec		txt_vec;

	dns_rr(const char *rname);
	~dns_rr();
	dns_rr(const dns_rr &);			// defeat default
	operator=(const dns_rr &);		// defeat default

	const char *rname() const;
	unsigned long hash_rname();
	int set_cname(const char *);
	const char *get_cname() const;

	static unsigned long hash_rname(const char *rname);

	friend class dns_rr_vec;
};

class dns_rr_vec : public dns_vec {
	int	p_keycount;
	dns_rr	**p_keyvec;
protected:
	int resize(int nsize);
public:
	dns_rr_vec();
	~dns_rr_vec();
	dns_rr_vec(const dns_rr_vec &);		// defeat default
	operator=(const dns_rr_vec &);		// defeat default

	int add(dns_rr *);
	dns_rr *lookup(const char *);
};

#endif /* _FNS_DNS_OBJ_HH */
