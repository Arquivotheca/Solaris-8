/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_string_rep.cc	1.5	96/03/31 SMI"

#include <stdlib.h>	/* multibyte (wcstombs) */
#include <synch.h>	/* mutex */
#include <xfn/FN_status.hh>

#include "FN_string_rep.hh"


int	FN_string_rep::nnodes = 0;


FN_string_rep::FN_string_rep(unsigned long code_set, unsigned long lang_terr)
{
	mutex_t r = DEFAULTMUTEX;

	++nnodes;

	p_code_set = code_set;
	p_lang_terr = lang_terr;
	p_native = 0;
	p_native_bytes = 0;
	ref = r;
	refcnt = 1;
	// sts = FN_SUCCESS;
}

FN_string_rep::~FN_string_rep()
{
	zap_native();
	--nnodes;
}

const unsigned char *
FN_string_rep::as_str(size_t *native_bytes)
{
	if (p_native) {
		if (native_bytes)
			*native_bytes = p_native_bytes;
		return (p_native);
	}

	/*
	 * This is a no-op for regular chars.
	 */

	if (string_char::narrow(this)) {
		const unsigned char	*cp;

		cp = (const unsigned char *)contents();
		if (native_bytes)
			*native_bytes = charcount();
		return (cp);
	}

	/*
	 * Must reconstruct native representation, probably wiped out
	 * as a result of strcat, et. al..
	 */

	string_wchar	*wp;
	unsigned char	*native;
	size_t		max_bytes, mb_bytes;

	if ((wp = string_wchar::narrow(this)) == 0)
		return (0);

	// At most 4 bytes per character in EUC, plus '\0' terminator.
	max_bytes = wp->charcount() * 4 + 1;
	native = new unsigned char[max_bytes];
	if (native == 0)
		return (0);
	if ((mb_bytes = wcstombs((char *)native,
			(const wchar_t *)wp->contents(),
			wp->charcount())) == (size_t)-1) {
		delete[] native;
		return (0);
	}
	native[mb_bytes] = '\0';
	p_native = native;
	p_native_bytes = mb_bytes;
	if (native_bytes)
		*native_bytes = p_native_bytes;
	return (p_native);
}

void
FN_string_rep::zap_native()
{
	delete[] p_native;
	p_native = 0;
	p_native_bytes = 0;
}

unsigned long
FN_string_rep::code_set() const
{
	return (p_code_set);
}

unsigned long
FN_string_rep::lang_terr() const
{
	return (p_lang_terr);
}

FN_string_rep *
FN_string_rep::share()
{
	mutex_lock(&ref);
	refcnt++;
	mutex_unlock(&ref);
	return (this);
}

int
FN_string_rep::release()
{
	mutex_lock(&ref);
	int r = --refcnt;
	mutex_unlock(&ref);
	return (r);
}
