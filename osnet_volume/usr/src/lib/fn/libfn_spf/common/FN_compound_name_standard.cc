/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)FN_compound_name_standard.cc	1.12	97/10/16 SMI"

#include <string.h>
#include <xfn/FN_compound_name_standard.hh>

#include "../libxfn/common/NameList.hh"

class FN_compound_name_standard_rep {
public:
	FN_syntax_standard syntax;
	List comps;

	FN_compound_name_standard_rep(const FN_syntax_standard &);
	FN_compound_name_standard_rep(const FN_compound_name_standard_rep &);
	~FN_compound_name_standard_rep();
};

FN_compound_name_standard_rep::FN_compound_name_standard_rep(
    const FN_syntax_standard &s)
: syntax(s)
{
}

FN_compound_name_standard_rep::FN_compound_name_standard_rep(
    const FN_compound_name_standard_rep &n)
: syntax(n.syntax), comps(n.comps)
{
}

FN_compound_name_standard_rep::~FN_compound_name_standard_rep()
{
}


FN_compound_name_standard::FN_compound_name_standard(
    FN_compound_name_standard_rep *r)
: rep(r)
{
}

FN_compound_name_standard_rep *
FN_compound_name_standard::get_rep(const FN_compound_name_standard &n)
{
	return (n.rep);
}

FN_compound_name_standard::FN_compound_name_standard(
    const FN_syntax_standard &s)
{
	rep = new FN_compound_name_standard_rep(s);
}

// Return 1 if givin string 'c' is a legal component
// It is legal if it contains no meta characters
// or if it contains meta characters, there is a way
// to escape them using either quotes or escape strings
static int syntax_legal_comp(const FN_syntax_standard &s,
    const FN_string &c)
{
	// anything goes with flat names
	if (s.direction() == FN_SYNTAX_STANDARD_DIRECTION_FLAT)
		return (1);
	// can't have primary quotes if no way to escape them using
	// escape string or secondary quotes
	if (s.escape() == 0 && s.begin_quote2() == 0 && s.begin_quote() &&
	    (c.next_substring(*s.begin_quote()) != FN_STRING_INDEX_NONE ||
	    c.next_substring(*s.end_quote()) != FN_STRING_INDEX_NONE))
		return (0);
	// can't have secondary quotes if no way to escape them using
	// escape string or primary quotes
	if (s.escape() == 0 && s.begin_quote() == 0 && s.begin_quote2() &&
	    (c.next_substring(*s.begin_quote2()) != FN_STRING_INDEX_NONE ||
	    c.next_substring(*s.end_quote2()) != FN_STRING_INDEX_NONE))
		return (0);

	return (1);
}

// set 'comp' to be next component in 'str' as defined by syntax 's'
static int
syntax_extract_comp(const FN_syntax_standard &s,
    unsigned char *comp,
    const unsigned char *&str)
{
	const char *bq1 = NULL, *bq2 = NULL, *eq1 = NULL, *eq2 = NULL,
	    *esc = NULL, *sep = NULL, *target_bq, *target_eq;
	size_t bq1_len, bq2_len, eq1_len, eq2_len, esc_len, sep_len,
	    target_bq_len, target_eq_len;
	int start = 1, one;

	// set up meta characters
	if (s.begin_quote()) {
		bq1 = (const char *)(s.begin_quote()->str());
		bq1_len = s.begin_quote()->charcount();
		eq1 = (const char *)(s.end_quote()->str());
		eq1_len = s.end_quote()->charcount();
	}
	if (s.begin_quote2()) {
		bq2 = (const char *)(s.begin_quote2()->str());
		bq2_len = s.begin_quote2()->charcount();
		eq2 = (const char *)(s.end_quote2()->str());
		eq2_len = s.end_quote2()->charcount();
	}
	if (s.escape()) {
		esc = (const char *)(s.escape()->str());
		esc_len = s.escape()->charcount();
	}
	if (s.direction() != FN_SYNTAX_STANDARD_DIRECTION_FLAT &&
	    s.component_separator()) {
		sep = (const char *)(s.component_separator()->str());
		sep_len = s.component_separator()->charcount();
	}

	while (*str) {
		// handle quoted strings
		if (start &&
		    ((one = (bq1 && strncmp((char *)str, bq1, bq1_len) == 0)) ||
		    (bq2 && strncmp((char *)str, bq2, bq2_len) == 0)))	{
			if (one) {
				target_bq_len = bq1_len;
				target_eq_len = eq1_len;
				target_bq = bq1;
				target_eq = eq1;
			} else {
				target_bq_len = bq2_len;
				target_eq_len = eq2_len;
				target_bq = bq2;
				target_eq = eq2;
			}

			for (str += target_bq_len;
			    *str &&
			    strncmp((char *)str, target_eq, target_eq_len) != 0;
			    str++) {
				// skip embedded escape character
				if (esc &&
				    strncmp((char *)str, esc, esc_len) == 0)
					str += esc_len;
				*comp++ = *str;
			}
			if (*str == 0)
				return (0);
			str += target_eq_len;

			// verified that end quote occurs at separator
			// or at end of string
			if (sep) {
				if (*str &&
				    strncmp((char *)str, sep, sep_len) != 0)
					return (0);
				if (*str) {
					str += sep_len;
					*comp = 0;
					// return at this point so that
					// next time thru, a null string will be
					// returned
					return (1);
				}
			} else {
				if (*str != 0)
					return (0);
			}
			str = 0; // to indicate end of string reached
			*comp = 0;
			return (1);
		} else if (esc && strncmp((char *)str, esc, esc_len) == 0) {
			str += esc_len;
			if (*str == 0)
				return (0);
			*comp++ = *str++;
		} else if (sep && strncmp((char *)str, sep, sep_len) == 0) {
			str += sep_len;
			*comp = 0;
			return (1);
		} else {
			*comp++ = *str++;
		}
		start = 0; // reset to indicate we're now inside string
	}
	str = 0;
	*comp = 0;
	return (1);
}

// check to see whether the 'meta' string occurs next in 'input'.
// If it does, escape it using 'esc' and return 1; otherwise return 0.

static int
check_and_escape(const unsigned char *&input, unsigned char *&target,
		    const char *meta, size_t meta_len,
		    const char *esc, size_t esc_len)
{
	if (meta && esc && strncmp((char *)input, meta, meta_len) == 0) {
		strcpy((char *)target, esc);
		target += esc_len;
		strcpy((char *)target, meta);
		target += meta_len;
		input += meta_len;
		return (1);
	}
	return (0);
}


static void
syntax_stringify_comp(const FN_syntax_standard &s,
    unsigned char *&str, const unsigned char *comp)
{
	int esc_sep = 0, start = 1;
	size_t sep_len = 0, esc_len = 0, target_eq_len = 0,
	    target_bq_len = 0, bq1_len = 0, bq2_len = 0;
	const char *sep = NULL, *esc = NULL, *target_bq = NULL, *eq2 = NULL,
	    *target_eq = NULL, *bq1 = NULL, *bq2 = NULL, *eq1 = NULL;

	// set up meta characters
	if (s.direction() != FN_SYNTAX_STANDARD_DIRECTION_FLAT &&
	    s.component_separator()) {
		sep = (const char *)(s.component_separator()->str());
		sep_len = s.component_separator()->charcount();
	}
	if (s.escape()) {
		esc = (const char *)(s.escape()->str());
		esc_len = s.escape()->charcount();
	}
	if (s.begin_quote()) {
		bq1 = (const char *)(s.begin_quote()->str());
		bq1_len = s.begin_quote()->charcount();
	}
	if (s.begin_quote2()) {
		bq2 = (const char *)(s.begin_quote2()->str());
		bq2_len = s.begin_quote2()->charcount();
	}

	// determine whether there are any separators, and if so,
	// find a way to escape or quote them
	if (sep && strstr((const char *)comp, sep)) {
		if (bq1) {
			target_bq = bq1;
			target_bq_len = bq1_len;
			target_eq = (const char *)(s.end_quote()->str());
			target_eq_len = s.end_quote()->charcount();
		} else if (bq2) {
			target_bq = bq2;
			target_bq_len = bq2_len;
			target_eq = (const char *)(s.end_quote2()->str());
			target_eq_len = s.end_quote2()->charcount();
		} else if (esc)
			esc_sep = 1;
	}
	if (target_bq) {
		strcpy((char *)str, target_bq);
		str += target_bq_len;
	}
	while (*comp) {
		if (check_and_escape(comp, str, esc, esc_len, esc, esc_len)) {
			// Escape escape string
		} else if (esc_sep &&
		    check_and_escape(comp, str, sep, sep_len, esc, esc_len)) {
			// Escape separator
		} else if (start && !target_bq &&
		    (check_and_escape(comp, str, bq1, bq1_len, esc, esc_len) ||
		    check_and_escape(comp, str, bq2, bq2_len, esc, esc_len))) {
			// Initial quotes must be escaped unless inside
			// quoted string
		} else if (check_and_escape(comp, str, target_eq,
		    target_eq_len, esc, esc_len)) {
			// Must escape end-quote when inside a quoted string
		} else {
			*str++ = *comp++;
		}
		start = 0;
	}
	if (target_eq) {
		strcpy((char *)str, target_eq);
		str += target_eq_len;
	}
}

static void
syntax_extract_name(FN_compound_name_standard_rep &d, const unsigned char *str)
{
	unsigned char *buf;

	d.comps.delete_all();
	buf = new unsigned char[strlen((char *)str)+1];
	do {
		if (!syntax_extract_comp(d.syntax, buf, str)) {
			// illegal name
			d.comps.delete_all();
			break;
		}
		switch (d.syntax.direction()) {
		case FN_SYNTAX_STANDARD_DIRECTION_FLAT:
		case FN_SYNTAX_STANDARD_DIRECTION_LTR:
			d.comps.append_item(new NameListItem(buf));
			break;
		case FN_SYNTAX_STANDARD_DIRECTION_RTL:
			d.comps.prepend_item(new NameListItem(buf));
			break;
		}
	} while (str);
	delete[] buf;
}

static unsigned char *
syntax_stringify_name(FN_compound_name_standard_rep &d)
{
	const NameListItem *i;
	void *ip;
	size_t size;
	unsigned char *buf;
	unsigned char *p;

	for (size = 0, i = (const NameListItem *)(d.comps.first(ip)); i;
	    i = (const NameListItem *)(d.comps.next(ip)))
		size += 2+2*strlen((char *)(i->name.str()));
	if (size == 0)
		p = buf = new unsigned char[1];
	else {
		p = buf = new unsigned char[size];
		switch (d.syntax.direction()) {
		case FN_SYNTAX_STANDARD_DIRECTION_FLAT:
		case FN_SYNTAX_STANDARD_DIRECTION_LTR:
			i = (const NameListItem *)(d.comps.first(ip));
			break;
		case FN_SYNTAX_STANDARD_DIRECTION_RTL:
			i = (const NameListItem *)(d.comps.last(ip));
			break;
		default:
			i = 0;
		}
		while (i) {
			syntax_stringify_comp(d.syntax, p, i->name.str());
			switch (d.syntax.direction()) {
			case FN_SYNTAX_STANDARD_DIRECTION_FLAT:
			case FN_SYNTAX_STANDARD_DIRECTION_LTR:
				i = (const NameListItem *)(d.comps.next(ip));
				break;
			case FN_SYNTAX_STANDARD_DIRECTION_RTL:
				i = (const NameListItem *)(d.comps.prev(ip));
				break;
			}
			if (i) {
				strcpy((char *)p, (char *)
				    (d.syntax.component_separator()->str()));
				p += d.syntax.component_separator()->
				    charcount();
			}
		}
	}
	*p = 0;
	return (buf);
}

FN_compound_name_standard::FN_compound_name_standard(
    const FN_syntax_standard &s, const FN_string &n)
{
	unsigned int status;

	// %%% should do matching/checking between locale info in 'n'
	// %%% and locale info in 's' before turning 'n' into str
	//
	// %%% to be properly locale-aware, the routines in this
	// %%% file should actually be manipulating FN_string objects
	// %%% instead of 'char *'.

	rep = new FN_compound_name_standard_rep(s);
	syntax_extract_name(*rep, n.str(&status));
}

FN_compound_name_standard::FN_compound_name_standard(
    const FN_compound_name_standard &n)
{
	rep = new FN_compound_name_standard_rep(*get_rep(n));
}

FN_compound_name &
FN_compound_name_standard::operator=(const FN_compound_name &n)
{
	// assume 'n' is FN_compound_name_standard
	if (&n != this) {
		if (rep)
			delete rep;
		rep = new FN_compound_name_standard_rep(
		    *get_rep((const FN_compound_name_standard &)n));
	}
	return (*this);
}

FN_compound_name *
FN_compound_name_standard::dup() const
{
	FN_compound_name_standard	*ns;

	if (rep) {
		FN_compound_name_standard_rep	*r;

		r = new FN_compound_name_standard_rep(*rep);
		ns = new FN_compound_name_standard(r);
		if (ns == 0)
			delete r;
	} else
		ns = 0;
	return (ns);
}

FN_compound_name_standard::~FN_compound_name_standard()
{
	if (rep)
		delete rep;
}

FN_syntax_standard *
FN_compound_name_standard::get_syntax() const
{
	return (new FN_syntax_standard(rep->syntax));
}

FN_compound_name_standard *
FN_compound_name_standard::from_syntax_attrs(const FN_attrset &a,
    const FN_string &n,
    FN_status &s)
{
	FN_syntax_standard *stx = FN_syntax_standard::from_syntax_attrs(a, s);
	if (stx == 0)
		return (0);

	FN_compound_name_standard *ret =
	    new FN_compound_name_standard(*stx, n);
	if (ret == 0) {
		s.set_code(FN_E_INSUFFICIENT_RESOURCES);
	} else if (ret->count() == 0) {
		s.set_code(FN_E_ILLEGAL_NAME);
		delete ret;
		ret = 0;
	} else {
		s.set_success();
	}

	delete stx;
	return (ret);
}

FN_attrset*
FN_compound_name_standard::get_syntax_attrs() const
{
	return (rep->syntax.get_syntax_attrs());
}

// convert to string representation
FN_string *
FN_compound_name_standard::string() const
{
	unsigned char *buf = syntax_stringify_name(*rep);
	FN_string *ret = new FN_string(buf);
	delete[] buf;
	return (ret);
}

#if 0
FN_compound_name&
FN_compound_name_standard::operator=(const FN_string &s)
{
	unsigned int status;
	syntax_extract_name(*rep, s.str(&status));
	return (*this);
}
#endif


// syntactic comparison
FN_compound_name_standard::operator==(const FN_compound_name &n) const
{
	return (is_equal(n));
}

FN_compound_name_standard::operator!=(const FN_compound_name &n) const
{
	void *ip1, *ip2;
	const FN_string *c1, *c2;

	for (c1 = first(ip1), c2 = n.first(ip2);
	    c1 && c2;
	    c1 = next(ip1), c2 = n.next(ip2))
		if (c1->compare(*c2, (rep)->syntax.string_case()))
			return (1);
	return (c1 != c2);
}

FN_compound_name_standard::operator<(const FN_compound_name &n) const
{
	void *ip1, *ip2;
	const FN_string *c1, *c2;

	for (c1 = first(ip1), c2 = n.first(ip2);
	    c1 && c2 && c1->compare(*c2, (rep)->syntax.string_case()) == 0;
	    c1 = next(ip1), c2 = n.next(ip2));
	if (c2) {
		if (c1)
			return (c1->compare(*c2,
			    (rep)->syntax.string_case()) < 0);
		return (1);
	}
	return (0);
}

FN_compound_name_standard::operator>(const FN_compound_name &n) const
{
	void *ip1, *ip2;
	const FN_string *c1, *c2;

	for (c1 = first(ip1), c2 = n.first(ip2);
	    c1 && c2 && c1->compare(*c2, (rep)->syntax.string_case()) == 0;
	    c1 = next(ip1), c2 = n.next(ip2));
	if (c1) {
		if (c2)
			return (c1->compare(*c2, (rep)->syntax.string_case()) >
			    0);
		return (1);
	}
	return (0);
}

FN_compound_name_standard::operator<=(const FN_compound_name &n) const
{
	void *ip1, *ip2;
	const FN_string *c1, *c2;

	for (c1 = first(ip1), c2 = n.first(ip2);
	    c1 && c2 && c1->compare(*c2, (rep)->syntax.string_case()) == 0;
	    c1 = next(ip1), c2 = n.next(ip2));
	if (c2) {
		if (c1)
			return (c1->compare(*c2, (rep)->syntax.string_case()) <
			    0);
		return (1);
	}
	if (c1)
		return (0);
	return (1);
}

FN_compound_name_standard::operator>=(const FN_compound_name &n) const
{
	void *ip1, *ip2;
	const FN_string *c1, *c2;

	for (c1 = first(ip1), c2 = n.first(ip2);
	    c1 && c2 && c1->compare(*c2, (rep)->syntax.string_case()) == 0;
	    c1 = next(ip1), c2 = n.next(ip2));
	if (c1) {
		if (c2)
			return (c1->compare(*c2, (rep)->syntax.string_case()) >
			    0);
		return (1);
	}
	if (c2)
		return (0);
	return (1);
}

// get count of components in name
unsigned
FN_compound_name_standard::count() const
{
	return (rep->comps.count());
}

// test for empty name (single empty string component)
int
FN_compound_name_standard::is_empty() const
{
	void *ip;
	return (count() == 1 && first(ip)->is_empty());
}

// get first component (points iter_pos after name)
const FN_string *
FN_compound_name_standard::first(void *&iter_pos) const
{
	const NameListItem *i;

	if (i = (const NameListItem *)((rep)->comps.first(iter_pos)))
		return (&(i->name));
	else
		return (0);
}

// test for prefix (points iter_pos after prefix)
int
FN_compound_name_standard::is_prefix(const FN_compound_name &n,
    void *&iter_pos, unsigned int * /* status */) const
{
	void *ip;
	const FN_string *c1, *c2;

	for (c1 = n.first(ip), c2 = first(iter_pos);
	    c1 && c2;
	    c1 = n.next(ip), c2 = next(iter_pos))
		if (c1->compare(*c2, (rep)->syntax.string_case()))
			return (0);
	if (c2)
		prev(iter_pos);
	return (c1 == 0);
}

// test for equality
int
FN_compound_name_standard::is_equal(const FN_compound_name &n,
    unsigned int * /* status */) const
{
	void *ip1, *ip2;
	const FN_string *c1, *c2;

	for (c1 = first(ip1), c2 = n.first(ip2);
	    c1 && c2;
	    c1 = next(ip1), c2 = n.next(ip2))
		if (c1->compare(*c2, (rep)->syntax.string_case()))
			// check status
			return (0);
	return (c1 == c2);
}

// Get last component (points iter_pos before name)
const FN_string *
FN_compound_name_standard::last(void *&iter_pos) const
{
	const NameListItem *i;

	if (i = (const NameListItem *)((rep)->comps.last(iter_pos)))
		return (&(i->name));
	else
		return (0);
}

// test for suffix (points iter_pos before suffix)
int
FN_compound_name_standard::is_suffix(const FN_compound_name &n,
    void *&iter_pos, unsigned int * /* status */) const
{
	void *ip;
	const FN_string *c1, *c2;

	for (c1 = n.last(ip), c2 = last(iter_pos);
	    c1 && c2;
	    c1 = n.prev(ip), c2 = prev(iter_pos))
		if (c1->compare(*c2, (rep)->syntax.string_case()))
			return (0);
	if (c2)
		next(iter_pos);
	return (c1 == 0);
}

// get component following iter_pos (points iter_pos after component)
const FN_string *FN_compound_name_standard::next(void *&iter_pos) const
{
	const NameListItem *i;

	if (i = (const NameListItem *)((rep)->comps.next(iter_pos)))
		return (&(i->name));
	else
		return (0);
}

// get component before iter_pos (points iter_pos before component)
const FN_string *FN_compound_name_standard::prev(void *&iter_pos) const
{
	const NameListItem *i;

	if (i = (const NameListItem *)((rep)->comps.prev(iter_pos)))
		return (&(i->name));
	else
		return (0);
}

// get copy of name from first component through iter_pos
FN_compound_name* FN_compound_name_standard::prefix(const void *iter_pos) const
{
	void *ip = (void *)iter_pos;
	FN_compound_name *n;
	const FN_string *c;
	unsigned int status;

	if ((c = prev(ip)) == 0)
		return (0);
	if ((n = new FN_compound_name_standard((rep)->syntax)) == 0)
		return (0);
	do {
		n->prepend_comp(*c, &status);
	} while (c = prev(ip));
	return (n);
}

// get copy of name from iter_pos through last component
FN_compound_name* FN_compound_name_standard::suffix(const void *iter_pos) const
{
	void *ip = (void *)iter_pos;
	FN_compound_name *n;
	const FN_string *c;
	unsigned int status;

	if ((c = next(ip)) == 0)
		return (0);
	if ((n = new FN_compound_name_standard((rep)->syntax)) == 0)
		return (0);
	do {
		n->append_comp(*c, &status);
	} while (c = next(ip));
	return (n);
}

// prepend component to name
int
FN_compound_name_standard::prepend_comp(const FN_string &c,
    unsigned int *status)
{
	// syntax allows name?
	if (!syntax_legal_comp((rep)->syntax, c)) {
		*status = FN_E_SYNTAX_NOT_SUPPORTED;
		return (0);
	}

	// flat names can only have one component
	if ((rep)->syntax.direction() == FN_SYNTAX_STANDARD_DIRECTION_FLAT &&
	    count()+1 > 1) {
		*status = FN_E_ILLEGAL_NAME;
		return (0);
	}

	int r = rep->comps.prepend_item(new NameListItem(c));
	*status = r? FN_SUCCESS: FN_E_INSUFFICIENT_RESOURCES;
	return (r);
}

// append component to name
int
FN_compound_name_standard::append_comp(const FN_string &c,
    unsigned int *status)
{
	*status = FN_SUCCESS;
	// syntax allows name?
	if (!syntax_legal_comp(rep->syntax, c)) {
		*status = FN_E_SYNTAX_NOT_SUPPORTED;
		return (0);
	}

	// special case for appending null names
	// what should we return in this case?
	if (c.is_empty())
		return (1);

	// check to see if first component is NULL
	// it is NULL, then replace the component with the new one
	if (count() == 1) {
		void *iterpos;
		const NameListItem *item =
			(const NameListItem *)(rep)->comps.first(iterpos);
		if (!item) {
			*status = FN_E_UNSPECIFIED_ERROR;
			return (0);
		}
		if ((item->name).is_empty()) {
			// first component is null, replace that
			// with new component
			(rep)->comps.delete_item(iterpos);
			int r = (rep->comps.append_item(new NameListItem(c)));
			if (!r)
				*status = FN_E_UNSPECIFIED_ERROR;
			return (r);
		}
	}
	// flat names can only have one component
	if ((rep)->syntax.direction() == FN_SYNTAX_STANDARD_DIRECTION_FLAT &&
	    (count()+1 > 1)) {
		*status = FN_E_ILLEGAL_NAME;
		return (0);
	}

	int r = (rep->comps.append_item(new NameListItem(c)));
	if (!r)
		*status = FN_E_INSUFFICIENT_RESOURCES;
	return (r);
}

// insert component before iter_pos
int
FN_compound_name_standard::insert_comp(void *&iter_pos,
    const FN_string &c, unsigned int *status)
{
	*status = FN_SUCCESS;
	// syntax allows name?
	if (!syntax_legal_comp((rep)->syntax, c)) {
		*status = FN_E_SYNTAX_NOT_SUPPORTED;
		return (0);
	}

	// special case for appending null names
	// what should we return in this case?
	if (c.is_empty())
		return (1);

	// check to see if first component is NULL
	// it is NULL, then replace the component with the new one
	if (count() == 1) {
		void *pos;
		const NameListItem *item =
			(const NameListItem *)(rep)->comps.first(pos);
		if (!item) {
			*status = FN_E_ILLEGAL_NAME;
			return (0);
		}
		if ((item->name).is_empty()) {
			// first component is null, replace that
			// with new component
			(rep)->comps.delete_item(pos);
			int r = (rep)->comps.append_item(new NameListItem(c));
			if (r) {
				// set iterpos to correct postion
				item = (const NameListItem *)
				    (rep)->comps.first(iter_pos);
			}
			*status = FN_E_INSUFFICIENT_RESOURCES;
			return (r);
		}
	}

	// flat names can only have one component
	if ((rep)->syntax.direction() == FN_SYNTAX_STANDARD_DIRECTION_FLAT &&
	    (count()+1 > 1)) {
		*status = FN_E_ILLEGAL_NAME;
		return (0);
	}

	int r = (rep->comps.insert_item(iter_pos, new NameListItem(c)));
	if (!r)
		*status = FN_E_INSUFFICIENT_RESOURCES;
	return (r);
}

// delete component before iter_pos
int
FN_compound_name_standard::delete_comp(void *&iter_pos)
{
	return (rep->comps.delete_item(iter_pos));
}

// delete all components
int
FN_compound_name_standard::delete_all()
{
	return (rep->comps.delete_all());
}


// external C interface constructor for fn_compound_name_from_syntax_attrs()
extern "C"
FN_compound_name_t *
Sstandard(const FN_attrset_t *attrs,
	const FN_string_t *name,
	FN_status_t *status)
{
	const FN_attrset *ccattrs = (const FN_attrset *)attrs;
	const FN_string *ccname = (const FN_string *)name;
	FN_status *ccstat = (FN_status *)(status);
	FN_compound_name *std =
		FN_compound_name_standard::from_syntax_attrs(*ccattrs,
			*ccname, *ccstat);

	return ((FN_compound_name_t *)std);
}
