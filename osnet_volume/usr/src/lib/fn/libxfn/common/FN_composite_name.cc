/*
 * Copyright (c) 1992 - 1996, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_composite_name.cc	1.9	99/05/26 SMI"

#include <xfn/FN_composite_name.hh>
#include <xfn/FN_status.hh>

#include "NameList.hh"


class FN_composite_name_rep {
public:
	List comps;

	FN_composite_name_rep();
	FN_composite_name_rep(const FN_composite_name_rep&);
	~FN_composite_name_rep();

	FN_composite_name_rep& operator=(const FN_composite_name_rep&);
};

FN_composite_name_rep::FN_composite_name_rep()
{
}

FN_composite_name_rep::FN_composite_name_rep(const FN_composite_name_rep& r)
	: comps(r.comps)
{
}

FN_composite_name_rep::~FN_composite_name_rep()
{
}

FN_composite_name_rep& FN_composite_name_rep::operator=(
	const FN_composite_name_rep& r)
{
	if (&r != this) {
		comps = r.comps;
	}
	return (*this);
}


FN_composite_name::FN_composite_name(FN_composite_name_rep* r)
	: rep(r)
{
}

FN_composite_name_rep *
FN_composite_name::get_rep(const FN_composite_name &n)
{
	return (n.rep);
}

FN_composite_name::FN_composite_name()
{
	rep = new FN_composite_name_rep();
}

FN_composite_name::~FN_composite_name()
{
	delete rep;
}

#include <string.h>

static const char FN_COMPOSITE_NAME_SEPARATOR = '/';
static const char FN_COMPOSITE_NAME_ESCAPE = '\\';
static const char FN_COMPOSITE_NAME_QUOTE1 = '\"';
static const char FN_COMPOSITE_NAME_QUOTE2 = '\'';

static inline int
is_meta_char(char c)
{
	switch (c) {
	case FN_COMPOSITE_NAME_ESCAPE:
	case FN_COMPOSITE_NAME_SEPARATOR:
	case FN_COMPOSITE_NAME_QUOTE1:
	case FN_COMPOSITE_NAME_QUOTE2:
		return (1);
	default:
		return (0);
	}
}

static inline int
is_quote_char(char c)
{
	return (c == FN_COMPOSITE_NAME_QUOTE1 || c == FN_COMPOSITE_NAME_QUOTE2);
}


static int
extract_comp(unsigned char *comp, const unsigned char *&str)
{
	int start = 1;
	char quote;

	for (; *str != '\0'; str++) {
		switch (*str) {
		case FN_COMPOSITE_NAME_ESCAPE:
			if (is_meta_char(*(str+1))) {
				// If escape preceeds meta char
				// consume it (i.e. don't copy it)
			} else {
				// If escape preceeds non-meta char,
				// escape is not consumed (i.e. copy it)
				*comp++ = *str;
			}
			*comp++ = *++str;
			break;
		case FN_COMPOSITE_NAME_QUOTE1:
		case FN_COMPOSITE_NAME_QUOTE2:
			if (start == 0) {
				// quotes that occur in non-beginning are
				// not used as quotes
				*comp++ = *str;
				break;
			}
			// Otherwise, consume string until matching quote
			quote = *str;
			for (str++; *str != '\0' && *str != quote; str++) {
				switch (*str) {
				case FN_COMPOSITE_NAME_ESCAPE:
					if (*(str+1) == 0)
						return (0);
					// escape is only significant when
					// preceeding terminating quote
					if (*(str+1) != quote)
						*comp++ = *str; // keep esc
					else {
						*comp++ = *++str; // skip esc
					}
					break;
				default:
					*comp++ = *str;
					break;
				};
			}
			if (*str == '\0')
				return (0);	// no matching quote found
			++str;  		// skip matching quote
			if (*str == '\0') {
				// matching quote found at end of string
				goto done;
			}
			if (*str == FN_COMPOSITE_NAME_SEPARATOR) {
				// matching quote found before separator
				++str;
				*comp = '\0';
				return (1);
			}
			return (0);  // matching quote found in middle
		case FN_COMPOSITE_NAME_SEPARATOR:
			str++;
			*comp = '\0';
			return (1);
		default:
			*comp++ = *str;
			break;
		}
		start = 0;
	}
done:
	str = 0;
	*comp = '\0';
	return (1);
}

static void
stringify_comp(unsigned char *&str, const unsigned char *comp)
{
	const char *quot =
	    strchr((const char *)comp, FN_COMPOSITE_NAME_SEPARATOR);
	int start = 1;

	if (quot)
		*str++ = FN_COMPOSITE_NAME_QUOTE1;
	while (*comp != '\0') {
		// when component is quoted, escape matching quote
		if (quot) {
			if (*comp == FN_COMPOSITE_NAME_QUOTE1)
				*str++ = FN_COMPOSITE_NAME_ESCAPE;
		} else {
			// When component is not quoted, add escape for
			// 1. leading quote character
			// 2. ending escape char
			// 3. an escape char preceding meta char anywhere
			if ((start && is_quote_char(*comp)) ||
			    (*comp == FN_COMPOSITE_NAME_ESCAPE &&
			    (*(comp+1) == '\0' || is_meta_char(*(comp+1)))))
				*str++ = FN_COMPOSITE_NAME_ESCAPE;
		}
		*str++ = *comp++;
		start = 0;
	}
	if (quot)
		*str++ = FN_COMPOSITE_NAME_QUOTE1;
}


static void
string_to_list(List &l, const unsigned char *str)
{
	unsigned char *buf;

	l.delete_all();
	buf = new unsigned char[strlen((const char *)str) + 1];
	do {
		if (!extract_comp(buf, str)) {
			// illegal_name
			l.delete_all();
			break;
		}
		l.append_item(new NameListItem(buf));
	} while (str);
	delete[] buf;
}

static unsigned char *
list_to_string(List &l)
{
	const NameListItem *i;
	void *ip;
	size_t size;
	unsigned char *buf, *p;
	unsigned int status;

	for (size = 0, i = (const NameListItem *)(l.first(ip)); i;
	    i = (const NameListItem *)(l.next(ip)))
		size += 3+2*strlen((const char *)(i->name.str(&status)));
	p = buf = new unsigned char[size];
	i = (const NameListItem *)(l.first(ip));
	while (i) {
		stringify_comp(p, i->name.str(&status));
		i = (const NameListItem *)(l.next(ip));
		if (i)
			*p++ = FN_COMPOSITE_NAME_SEPARATOR;
	};
	*p = '\0';
	return (buf);
}

// convert to/from string representation
FN_string *
FN_composite_name::string(unsigned int *status) const
{
	unsigned char *buf = list_to_string(rep->comps);
	FN_string *ret = new FN_string(buf);

	delete[] buf;
	if (status)
		*status = FN_SUCCESS;		// %%% check for errors
	return (ret);
}

FN_composite_name::FN_composite_name(const FN_string &s)
{
	unsigned int status;
	rep = new FN_composite_name_rep();
	string_to_list(rep->comps, s.str(&status));
}

FN_composite_name &
FN_composite_name::operator=(const FN_string &s)
{
	unsigned int status;

	string_to_list(rep->comps, s.str(&status));
	return (*this);
}

FN_composite_name::FN_composite_name(const unsigned char *s)
{
	rep = new FN_composite_name_rep();
	string_to_list(rep->comps, s);
}

// copy and assignment
FN_composite_name::FN_composite_name(const FN_composite_name &n)
{
	rep = new FN_composite_name_rep(*get_rep(n));
}

FN_composite_name &
FN_composite_name::operator=(const FN_composite_name &n)
{
	if (&n != this) {
		*rep = *get_rep(n);
	}
	return (*this);
}

// syntactic comparison
FN_composite_name::is_equal(
	const FN_composite_name &n,
	unsigned int *status) const
{
	void *ip1, *ip2;
	const FN_string *c1, *c2;

	if (status)
	    *status = FN_SUCCESS;

	for (c1 = first(ip1), c2 = n.first(ip2);
	    c1 && c2;
	    c1 = next(ip1), c2 = n.next(ip2))
		if (c1->compare(*c2, FN_STRING_CASE_SENSITIVE, status) != 0)
			return (0);

	if (c1 && c2)
	    return (c1->compare(*c2, FN_STRING_CASE_SENSITIVE, status));
	else if (c1 || c2)
	    return (0);
	else return (1);
}

FN_composite_name::operator==(const FN_composite_name &n) const
{
	return (is_equal(n));  // ignore status
}

FN_composite_name::operator!=(const FN_composite_name &n) const
{
	return (!is_equal(n)); // ignore status
}

// test for empty name (single empty string component)
int
FN_composite_name::is_empty() const
{
	void *ip;
	return (count() == 1 && first(ip)->is_empty());
}

// get count of components in name
unsigned
FN_composite_name::count() const
{
	return (rep->comps.count());
}

// get first component (points iter_pos after name)
const FN_string *
FN_composite_name::first(void *&iter_pos) const
{
	const NameListItem *i;

	if (i = (const NameListItem *)(rep->comps.first(iter_pos)))
		return (&(i->name));
	else
		return (0);
}

// test for prefix (points iter_pos after prefix)
int
FN_composite_name::is_prefix(
	const FN_composite_name &n,
	void *&iter_pos,
	unsigned int *status) const
{
	void *ip;
	const FN_string *c1, *c2;

	if (status)
	    *status = FN_SUCCESS;

	for (c1 = n.first(ip), c2 = first(iter_pos);
	    c1 && c2;
	    c1 = n.next(ip), c2 = next(iter_pos))
		if (c1->compare(*c2, FN_STRING_CASE_SENSITIVE, status) != 0)
			return (0);
	if (c2)
		prev(iter_pos);
	return (c1 == 0);
}

// get last component (points iter_pos before name)
const FN_string *
FN_composite_name::last(void *&iter_pos) const
{
	const NameListItem *i;

	if (i = (const NameListItem *)(rep->comps.last(iter_pos)))
		return (&(i->name));
	else
		return (0);
}

// test for suffix (points iter_pos before suffix)
int
FN_composite_name::is_suffix(
	const FN_composite_name &n,
	void *&iter_pos,
	unsigned int *status) const
{
	void *ip;
	const FN_string *c1, *c2;

	if (status)
	    *status = FN_SUCCESS;

	for (c1 = n.last(ip), c2 = last(iter_pos);
	    c1 && c2;
	    c1 = n.prev(ip), c2 = prev(iter_pos))
		if (c1->compare(*c2, FN_STRING_CASE_SENSITIVE, status) != 0)
			return (0);
	if (c2)
		next(iter_pos);
	return (c1 == 0);
}

// get component following iter_pos (points iter_pos after component)
const FN_string *
FN_composite_name::next(void *&iter_pos) const
{
	const NameListItem *i;

	if (i = (const NameListItem *)(rep->comps.next(iter_pos)))
		return (&(i->name));
	else
		return (0);
}

// get component before iter_pos (points iter_pos before component)
const FN_string *
FN_composite_name::prev(void *&iter_pos) const
{
	const NameListItem *i;

	if (i = (const NameListItem *)(rep->comps.prev(iter_pos)))
		return (&(i->name));
	else
		return (0);
}

// get copy of name from first component through iter_pos
FN_composite_name *
FN_composite_name::prefix(const void *iter_pos) const
{
	void *ip = (void *)iter_pos;
	FN_composite_name *n;
	const FN_string *c;

	if ((c = prev(ip)) == 0)
		return (0);
	if ((n = new FN_composite_name) == 0)
		return (0);
	do {
		n->prepend_comp(*c);
	} while (c = prev(ip));
	return (n);
}

// get copy of name from iter_pos through last component
FN_composite_name *
FN_composite_name::suffix(const void *iter_pos) const
{
	void *ip = (void *)iter_pos;
	FN_composite_name *n;
	const FN_string *c;

	if ((c = next(ip)) == 0)
		return (0);
	if ((n = new FN_composite_name) == 0)
		return (0);
	do {
		n->append_comp(*c);
	} while (c = next(ip));
	return (n);
}

// prepend component/name to name
int
FN_composite_name::prepend_comp(const FN_string &c)
{
	return (rep->comps.prepend_item(new NameListItem(c)));
}

int
FN_composite_name::prepend_name(const FN_composite_name &n)
{
	void *ip;
	const FN_string *c;

	if (&n == this)
		return (prepend_name(FN_composite_name(n)));

	for (c = n.last(ip); c; c = n.prev(ip))
		if (prepend_comp(*c) == 0)
			return (0);
	return (1);
}

// append component/name to name
int
FN_composite_name::append_comp(const FN_string &c)
{
	return (rep->comps.append_item(new NameListItem(c)));
}

int
FN_composite_name::append_name(const FN_composite_name &n)
{
	const FN_string *c;
	void *ip;

	if (&n == this)
		return (append_name(FN_composite_name(n)));

	for (c = n.first(ip); c; c = n.next(ip))
		if (append_comp(*c) == 0)
			return (0);
	return (1);
}

// insert component/name before iter_pos
int
FN_composite_name::insert_comp(void *&iter_pos, const FN_string &c)
{
	return (rep->comps.insert_item(iter_pos, new NameListItem(c)));
}

int
FN_composite_name::insert_name(void *&iter_pos, const FN_composite_name &n)
{
	void *ip;
	const FN_string *c;

	if (&n == this)
		return (insert_name(iter_pos, FN_composite_name(n)));

	for (c = n.last(ip); c; c = n.prev(ip))
		if (insert_comp(iter_pos, *c) == 0)
			return (0);
	return (1);
}

// delete component before iter_pos
int
FN_composite_name::delete_comp(void *&iter_pos)
{
	return (rep->comps.delete_item(iter_pos));
}

#if 0
// delete all components
int
FN_composite_name::delete_all()
{
	return (rep->comps.delete_all());
}
#endif
