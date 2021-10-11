/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_string.cc	1.7	97/12/05 SMI"

#ifdef DEBUG
#include <stdio.h>
#endif

#include <stdlib.h>	/* multibyte strings */
#include <string.h>	/* str*() */
#include <stdarg.h>	/* var args */
#include <locale.h>	/* setlocale */

#include <xfn/FN_string.hh>
#include <xfn/FN_status.h>

#include "FN_string_rep.hh"

#define	FN_PCS_CODESET		(0x00010020)		/* PCS code set */
#define	FN_LATIN1_CODESET	(0x00010001)		/* Latin-1 code set */

#define	FN_DEFAULT_CODESET	FN_PCS_CODESET
#define	FN_DEFAULT_LANGTERR	(0x0000UL)

/*
 * Here, we define the FN_string class.
 *
 * If the constructor fails in some way, it may be possible for the rep ptr
 * to be null.  We take care that this does not cause a core dump.
 */


/*
 * return length and number of bytes in a multibyte string.
 */

static size_t
mbslen(const char *base, size_t *nbytes)
{
	const char	*cp = base;
	size_t		ncodes;
	int		clen;

	ncodes = 0;
	while (*cp != '\0') {
		clen = mblen(cp, 4);
		cp += clen;
		++ncodes;
	}
	if (nbytes)
		*nbytes = cp - base;
	return (ncodes);
}

/*
 * %%% hairy
 * Here is a good example of a horrible hack to find out if the current
 * locale's code set requires multiple bytes to be represented.
 *
 * If this information were not available, one could assume that all
 * strings are multibyte.  Everything should work as before, except that
 * the internal representation of strings will consume 4 times as much
 * memory and some extra multibyte <-> wchar_t conversions will have to
 * be performed.
 *
 * %%% Note!
 * The intent of the call to setlocale below is to handle the
 * condition where the FN_string constructor is called before main()
 * (and hence before setlocale() is called from the application).
 * This can happen in the case of statically defined C++ variables.
 * As documented in the man page, all well behaved applications
 * that require locale information must call setlocale() at the
 * beginning of the program - in main in the case of single
 * threaded applications and in the thread-start function in the
 * case of multi threaded applications. They cannot depend on
 * the call to setlocale that is done in this library.
 */

static int inline
use_mb()
{
	static int setlocale_called = 0;

	if (!setlocale_called) {
		setlocale(LC_ALL, "");
		setlocale_called = 1;
	}
	return (multibyte);
}

/*
 * Tear down contents of object.
 */

void
FN_string::destr()
{
	if (rep && rep->release() == 0) {
		delete rep;
		// rep = 0;
	}
}

/*
 * Build up the object.  If the rep * is invalid, return false and
 * mark object as dead (i.e. rep == 0).
 */

int
FN_string::constr(FN_string_rep *r)
{
	rep = 0;
	if (r) {
		if (r->valid()) {
			rep = r;
			return (1);
		} else
			delete r;
	}
	return (0);
}

/*
 * %%% locale support needs to be implemented here.
 */

int
FN_string::constr(FN_string_rep *r, const void *, size_t)
{
	return (constr(r));
}


FN_string::FN_string()
{
	if (use_mb())
		constr(new string_wchar(0, 0, 0,
		    FN_DEFAULT_CODESET, FN_DEFAULT_LANGTERR));
	else
		constr(new string_char(0, 0, 0,
		    FN_DEFAULT_CODESET, FN_DEFAULT_LANGTERR));
}

FN_string::~FN_string()
{
	destr();
}

FN_string::FN_string(const FN_string &s)
{
	if (s.rep)
		rep = s.rep->share();
	else
		rep = 0;
}

FN_string &
FN_string::operator=(const FN_string &s)
{
	if (&s != this) {
		destr();
		if (s.rep)
			rep = s.rep->share();
		else
			rep = 0;
	}
	return (*this);
}

FN_string::FN_string(const unsigned char *s)
{
	size_t	ncodes;
	size_t	nbytes;

	if (use_mb()) {
		wchar_t	*wp;

		ncodes = mbslen((const char *)s, &nbytes);
		wp = new wchar_t[ncodes];
		if (wp) {
			if (mbstowcs(wp, (const char *)s, ncodes) != (size_t)-1)
				constr(new string_wchar(wp, ncodes, ncodes,
				    FN_DEFAULT_CODESET, FN_DEFAULT_LANGTERR));
			delete[] wp;
		}
	} else {
		ncodes = strlen((const char *)s);
		constr(new string_char((const char *)s, ncodes, ncodes,
		    FN_DEFAULT_CODESET, FN_DEFAULT_LANGTERR));
	}
}

FN_string::FN_string(const unsigned char *s, size_t maxchars)
{
	size_t	ncodes;

	if (use_mb()) {
		wchar_t	*wp;

		wp = new wchar_t[maxchars];
		if (wp) {
			if ((ncodes = mbstowcs(wp, (const char *)s,
					maxchars)) != (size_t)-1)
				constr(new string_wchar(wp, ncodes, ncodes,
				    FN_DEFAULT_CODESET, FN_DEFAULT_LANGTERR));
			delete[] wp;
		}
	} else {
		int i;
		ncodes = 0;
		for (i = 0; i < maxchars && s[i] != '\0'; i++)
			if (s[i] != '\0')
				++ncodes;
		constr(new string_char((const char *)s, ncodes, ncodes,
		    FN_DEFAULT_CODESET, FN_DEFAULT_LANGTERR));
	}
}

const unsigned char *
FN_string::str(unsigned int *status) const
{
	const unsigned char	*cp;

	if (rep == 0) {
		if (status)
			*status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	if ((cp = rep->as_str()) == 0) {
		if (status)
			*status = FN_E_INCOMPATIBLE_CODE_SETS;
		return (0);
	}
	if (status)
		*status = FN_SUCCESS;
	return (cp);
}

FN_string::FN_string(
	unsigned long code_set,
	unsigned long lang_terr,
	size_t charcount,
	size_t bytecount,
	const void *contents,
	unsigned int *status)
{
	unsigned int	sts;
	FN_string_rep	*r;

	if (lang_terr != FN_DEFAULT_LANGTERR)
		sts = FN_E_INCOMPATIBLE_LOCALES;
	else {
		switch (code_set) {
		case FN_PCS_CODESET:
		case FN_LATIN1_CODESET:
			r = new string_char((const char *)contents,
			    charcount, bytecount, code_set, lang_terr);
			if (constr(r))
				sts = FN_SUCCESS;
			else
				sts = FN_E_INSUFFICIENT_RESOURCES;
			break;
		default:
			sts = FN_E_INCOMPATIBLE_CODE_SETS;
		}
	}
	if (status)
		*status = sts;
}

unsigned long
FN_string::code_set() const
{
	if (rep)
		return (rep->code_set());
	else
		return (0);
}

unsigned long
FN_string::lang_terr() const
{
	if (rep)
		return (rep->lang_terr());
	else
		return (0);
}

size_t
FN_string::charcount() const
{
	if (rep)
		return (rep->charcount());
	else
		return (0);
}

size_t
FN_string::bytecount() const
{
	size_t	nbytes;

	if (rep && rep->as_str(&nbytes))
		return (nbytes);
	return (0);
}

const void *
FN_string::contents() const
{
	if (rep)
		return (rep->contents());
	else
		return (0);
}

size_t
FN_string::calculate_va_size(const FN_string *s1, const FN_string *s2,
	va_list ap)
{
	// calculate total charcount
	// total_charcount is a hint as to how many chars to allocate
	// to prevent O(n^2) behavior.

	size_t total_charcount = 0;
	const FN_string *sn;

	if (s1) {
		total_charcount += s1->charcount();
		if (s2) {
			total_charcount += s2->charcount();
			while (sn = va_arg(ap, const FN_string *)) {
				total_charcount += sn->charcount();
			}
		}
	}
	return (total_charcount);
}

void
FN_string::init_va_rep(
	size_t total_charcount,
	unsigned int *status,
	const FN_string *s1,
	const FN_string *s2,
	va_list ap)
{
	unsigned int	local_status;
	const FN_string	*sn;
	FN_string_rep	*r;
	void		*tag;

	if (status)
		*status = FN_SUCCESS;

	if (s1 == 0) {
		r = new string_char(0, 0, 0, FN_DEFAULT_CODESET,
		    FN_DEFAULT_LANGTERR);
		if (!constr(r)) {
			if (status)
				*status = FN_E_INSUFFICIENT_RESOURCES;
		}
		return;
	}
	if (s2 == 0) {
		if (s1->rep) {
			rep = s1->rep->share();
		} else {
			rep = 0;
			if (status)
				*status = FN_E_INSUFFICIENT_RESOURCES;
		}
		return;
	}

	// make sure s1 and s2 are alive and of the same type.

	if (s1->rep == 0 || s2->rep == 0) {
		rep = 0;
		if (status)
			*status = FN_E_INSUFFICIENT_RESOURCES;
		return;
	}
	tag = s1->rep->typetag();
	if (s2->rep->typetag() != tag) {
		rep = 0;
		if (status)
			*status = FN_E_INCOMPATIBLE_CODE_SETS;
		return;
	}

	// copy data
	r = s1->rep->clone(0, s1->rep->charcount(), total_charcount);
	if (!constr(r)) {
		if (status)
			*status = FN_E_INSUFFICIENT_RESOURCES;
		return;
	}

	// concat s2
	local_status = rep->cat(s2->rep);
	if (local_status != FN_SUCCESS) {
		delete rep;
		rep = 0;
		if (status)
			*status = local_status;
		return;
	}

	// concat sn (and check types)
	while (sn = va_arg(ap, const FN_string *)) {
		if (sn->rep == 0) {
			delete rep;
			rep = 0;
			local_status = FN_E_INSUFFICIENT_RESOURCES;
			break;
		}
		if (sn->rep->typetag() != tag) {
			delete rep;
			rep = 0;
			local_status = FN_E_INCOMPATIBLE_CODE_SETS;
			break;
		}
		if ((local_status = rep->cat(sn->rep)) != FN_SUCCESS) {
			delete rep;
			rep = 0;
			local_status = FN_E_INSUFFICIENT_RESOURCES;
			break;
		}
	}
	if (status)
		*status = local_status;
}


FN_string::FN_string(
	size_t total_charcount,
	unsigned int *status,
	const FN_string *s1,
	const FN_string *s2,
	va_list ap)
{
	init_va_rep(total_charcount, status, s1, s2, ap);
}

FN_string::FN_string(
	unsigned int *status,
	const FN_string *s1,
	const FN_string *s2,
	...)
{
	va_list ap;
	va_start(ap, s2);
	size_t total_charcount = calculate_va_size(s1, s2, ap);
	va_end(ap);

	va_start(ap, s2);
	init_va_rep(total_charcount, status, s1, s2, ap);
	va_end(ap);
}


/*
 * Constructor returns substring between character indices first and last
 * (inclusive).
 */

FN_string::FN_string(const FN_string &orig, int first, int last)
{
	int	lasti = orig.charcount() - 1;

	if (lasti < 0) {
		rep = 0;		// rep = get_rep(orig)->clone(0, 0, 0);
		return;
	}

	// calculate start and length
	if (first < 0)
		first = 0;
	if (last > lasti)
		last = lasti;

	// what is given are indices (charcount)
	// a 0 size indicates it needs to be calculated
	// copy data
	// %%% '\0' terminator assumptions?
	constr(orig.rep->clone(first, last - first + 1, last - first + 1 + 1));
}

int
FN_string::is_empty() const
{
	return (charcount() == 0);
}

int
FN_string::compare(
	const FN_string &s,
	unsigned int string_case,
	unsigned int *status) const
{
	unsigned int	sts;
	int		cs;

	if (rep == 0 || s.rep == 0) {
		if (status)
			*status = FN_E_INSUFFICIENT_RESOURCES;
		return (-1);
	}

	if (string_case == FN_STRING_CASE_INSENSITIVE)
		cs = rep->casecmp(0, s.rep, sts);
	else
		cs = rep->cmp(0, s.rep, sts);

	if (status)
		*status = sts;
	return (cs);
}

/*
 * Compare characters specified between indices from this string and given
 * string 's'.
 */

int
FN_string::compare_substring(
	int first,
	int last,
	const FN_string &s,
	unsigned int string_case,
	unsigned int *status) const
{
	int		lasti = charcount() - 1;
	int		sub_num_chars = s.charcount();
	unsigned int	sts = FN_SUCCESS;

	if (rep == 0 || s.rep == 0) {
		if (status)
			*status = FN_E_INSUFFICIENT_RESOURCES;
		return (-1);
	}

	if (status)
		*status = sts;

	if (lasti < 0) {
		if (sub_num_chars == 0)
			return (0);
		else
			return (-1);
	}
	if (first < 0)
		first = 0;
	if (last > lasti)
		last = lasti;

	int ret;
	int num_chars = last - first + 1;

	if (num_chars > sub_num_chars) {
		if (string_case == FN_STRING_CASE_INSENSITIVE)
			ret = rep->ncasecmp(first, s.rep, sub_num_chars, sts);
		else
			ret = rep->ncmp(first, s.rep, sub_num_chars, sts);
		if (status)
			*status = sts;
		if (ret == 0)
			return (1);
		return (ret);
	}

	if (string_case == FN_STRING_CASE_INSENSITIVE)
		ret = rep->ncasecmp(first, s.rep, num_chars, sts);
	else
		ret = rep->ncmp(first, s.rep, num_chars, sts);
	if (status)
		*status = sts;
	if (num_chars < sub_num_chars) {
		if (ret == 0)
			return (-1);
		return (ret);
	}
	return (ret);
}

/*
 * Get position of where 's' occurs, starting from character position 'index',
 * in this string.
 */

int
FN_string::next_substring(
	const FN_string &s,
	int index,
	unsigned int string_case,
	unsigned int *status) const
{
	int		sub_num_chars = s.charcount();
	unsigned int	sts = FN_SUCCESS;

	if (rep == 0 || s.rep == 0) {
		if (status)
			*status = FN_E_INSUFFICIENT_RESOURCES;
		return (FN_STRING_INDEX_NONE);
	}
	if (status)
		*status = sts;

	if (sub_num_chars == 0)
		return (FN_STRING_INDEX_NONE);

	int lasti = charcount() - sub_num_chars;
	if ((lasti < 0) || (index > lasti))
		return (FN_STRING_INDEX_NONE);
	if (index < 0)
		index = 0;

	int i;

	for (i = index; i <= lasti; i++) {
		if (string_case == FN_STRING_CASE_INSENSITIVE) {
			if (rep->ncasecmp(i, s.rep, sub_num_chars, sts) == 0) {
				if (status)
					*status = sts;
				return (i);
			}
		} else {
			if (rep->ncmp(i, s.rep, sub_num_chars, sts) == 0) {
				if (status)
					*status = sts;
				return (i);
			}
		}
	}
	if (status)
		*status = sts;

	return (FN_STRING_INDEX_NONE);
}

/*
 * Get position of where 's' occurs, starting backwards from
 * character position 'index' towards the front of this string.
 */

int
FN_string::prev_substring(const FN_string &s,
	int index,
	unsigned int string_case,
	unsigned int *status) const
{
	int		sub_num_chars = s.charcount();
	unsigned int	sts = FN_SUCCESS;

	if (rep == 0 || s.rep == 0) {
		if (status)
			*status = FN_E_INSUFFICIENT_RESOURCES;
		return (-1);
	}

	if (status)
		*status = sts;

	if (sub_num_chars == 0)
		return (FN_STRING_INDEX_NONE);

	int lasti = charcount() - sub_num_chars;
	if ((lasti < 0) || (index < 0))
		return (FN_STRING_INDEX_NONE);
	if (index > lasti)
		index = lasti;

	int i;

	for (i = index; i >= 0; i--) {
		if (string_case == FN_STRING_CASE_INSENSITIVE) {
			if (rep->ncasecmp(i, s.rep, sub_num_chars, sts) == 0) {
				if (status)
					*status = sts;
				return (i);
			}
		} else {
			if (rep->ncmp(i, s.rep, sub_num_chars, sts) == 0) {
				if (status)
					*status = sts;
				return (i);
			}
		}
	}

	if (status)
		*status = sts;
	return (FN_STRING_INDEX_NONE);
}

#ifdef DEBUG
void
FN_string::report(FILE *fp)
{
//	fprintf(fp, "FN_string_rep::nnodes %d\n", FN_string_rep::nnodes);
}
#endif
