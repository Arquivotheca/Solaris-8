/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_string_wchar.cc	1.4	97/10/16 SMI"

#include <string.h>
#include <xfn/FN_status.h>
#include "FN_string_rep.hh"

#define	CLASS	string_wchar
#define	TYPE	wchar_t


unsigned long	CLASS::nnodes = 0;


CLASS *
CLASS::narrow(FN_string_rep *r)
{
	if (s_typetag() == r->typetag())
		return ((CLASS *)r);
	else
		return (0);
}

const CLASS *
CLASS::narrowc(const FN_string_rep *r)
{
	if (s_typetag() == r->typetag())
		return ((const CLASS *)r);
	else
		return (0);
}

void *
CLASS::s_typetag()
{
	return (&nnodes);
}

void *
CLASS::typetag() const
{
	return (&nnodes);
}

CLASS::CLASS(const CLASS &r)
	: FN_string_rep(r.p_code_set, r.p_lang_terr)
{
	p_valid = 0;
	if (!r.p_valid)
		return;
	clength = r.clength;
	cstorlen = r.cstorlen;
	if (r.cstorlen > 0) {
		cstring = new TYPE[r.cstorlen];
		if (cstring == 0)
			return;
		memcpy(cstring, r.cstring, (r.clength + 1) * sizeof (TYPE));
	} else
		cstring = r.cstring;
	p_valid = 1;
}

CLASS::~CLASS()
{
	if (cstorlen > 0)
		delete[] cstring;
}

int
CLASS::valid(void) const
{
	return (p_valid);
}

FN_string_rep *
CLASS::clone(void) const
{
	return (new CLASS(*this));
}

FN_string_rep *
CLASS::clone(unsigned from, size_t len, size_t storlen) const
{
	return (new CLASS(cstring + from, len, storlen,
			    p_code_set, p_lang_terr));
}

const void *
CLASS::contents() const
{
	return (cstring);
}

size_t
CLASS::charcount() const
{
	return (clength);
}

#if 0
size_t
CLASS::bytecount() const
{
	return (clength * sizeof (TYPE));
}
#endif

int
CLASS::cmp(unsigned from, const FN_string_rep *s,
	unsigned int &status) const
{
	const CLASS	*sp;

	if ((sp = narrowc(s)) == 0) {
		status = FN_E_INCOMPATIBLE_CODE_SETS;
		return (-1);
	}
	status = FN_SUCCESS;
	return (wscmp(cstring + from, sp->cstring));
}

int
CLASS::casecmp(unsigned from, const FN_string_rep *s,
	unsigned int &status) const
{
	const CLASS	*sp;

	if ((sp = narrowc(s)) == 0) {
		status = FN_E_INCOMPATIBLE_CODE_SETS;
		return (-1);
	}
	status = FN_SUCCESS;
	return (wscasecmp(cstring + from, sp->cstring));
}

int
CLASS::ncmp(unsigned from, const FN_string_rep *s, size_t len,
	unsigned int &status) const
{
	const CLASS	*sp;

	if ((sp = narrowc(s)) == 0) {
		status = FN_E_INCOMPATIBLE_CODE_SETS;
		return (-1);
	}
	status = FN_SUCCESS;
	return (wsncmp(cstring + from, sp->cstring, len));
}

int
CLASS::ncasecmp(unsigned from, const FN_string_rep *s, size_t len,
	unsigned int &status) const
{
	const CLASS	*sp;

	if ((sp = narrowc(s)) == 0) {
		status = FN_E_INCOMPATIBLE_CODE_SETS;
		return (-1);
	}
	status = FN_SUCCESS;
	return (wsncasecmp(cstring + from, sp->cstring, len));
}

unsigned int
CLASS::cat(const FN_string_rep *s)
{
	const CLASS	*sp;
	size_t		len, sz;

	if ((sp = narrowc(s)) == 0)
		return (FN_E_INCOMPATIBLE_CODE_SETS);

	zap_native();

	len = clength + sp->clength;
	sz = len + 1;		// for '\0' terminator
	if (sz > cstorlen) {
	TYPE	*cp;

		cp = new TYPE[sz];
		if (cp == 0)
			return (FN_E_INSUFFICIENT_RESOURCES);
		memcpy(cp, cstring, clength * sizeof (TYPE));
		memcpy(&cp[clength], sp->cstring, (sp->clength + 1) *
								sizeof (TYPE));
		if (cstorlen > 0)
			delete[] cstring;
		cstring = cp;
		clength = len;
		cstorlen = sz;
	} else {
		memcpy(&cstring[clength], sp->cstring, (sp->clength + 1) *
								sizeof (TYPE));
		clength += sp->clength;
	}
	return (FN_SUCCESS);
}


/*
 * Create string of length len, but allocate a string of length storlen for it.
 * If storlen == 0, assume length is len.
 */

CLASS::CLASS(const TYPE *p, size_t len, size_t storlen,
	unsigned long code_set, unsigned long lang_terr)
	: FN_string_rep(code_set, lang_terr)
{
	p_valid = 0;
	if (storlen == 0)
		storlen = len;
	clength = len;
	cstorlen = storlen + 1;		// for '\0' terminator
	cstring = new TYPE[cstorlen];
	if (cstring == 0)
		return;
	memcpy(cstring, p, len * sizeof (TYPE));
	cstring[len] = '\0';
	p_valid = 1;
}
