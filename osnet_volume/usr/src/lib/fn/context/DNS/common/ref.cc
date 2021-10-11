/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ref.cc	1.10	97/10/21 SMI"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <xfn/fns_symbol.hh>

#include "cx.hh"


static int txtcmp(const void *, const void *);
static int demangle(int count, const char **av, FN_ref &r, FN_status &s);
#ifdef DEBUG
extern "C" int a__txt_records_to_addrs(int ac, const char *av[],
    FN_ref_t *ref, FN_status_t *);
#endif


/*
 * we need to batch XFNxxx strings of the same type together.  to do
 * this, we do something similar to "sort | uniq".  first sort, then
 * batch together runs of lines with identical XFNxxx tokens.
 */

int
DNS_ctx::addrs_from_txt(int ac, const char **av, FN_ref &r, FN_status &s)
{
	if (ac == 0)
		return (1);

	qsort(av, ac, sizeof (av[0]), txtcmp);
	int count = 0;
	int first;
	for (first = 0; first < ac; first += count) {
		const char	*cp;
		size_t		len;

		count = 1;
		if (first + 1 < ac) {
			cp = av[first];
			while (*cp != '\0') {
				if (*cp == ' ' || *cp == '\t')
					break;
				++cp;
			}
			len = cp - av[first];

			while (first + count < ac) {
				if (strncmp(av[first], av[first + count], len)
				    == 0 &&
				    (av[first + count][len] == ' ' ||
				    av[first + count][len] == '\t'))
					count += 1;
				else
					break;
			}
		}
		demangle(count, &av[first], r, s);
	}
	return (1);
}

/*
 * callback for qsort
 */

static int
txtcmp(const void *p1, const void *p2)
{
	const char	*s1 = *(const char **)p1;
	const char	*s2 = *(const char **)p2;

#if 0
fprintf(stderr, "CMP %s/%s\n", s1, s2);
#endif
	for (;;) {
		switch (*s1) {
		case '\0':
		case ' ':
		case '\t':
			return (-1);
		}
		switch (*s2) {
		case '\0':
		case ' ':
		case '\t':
			return (1);
		}
		if (*s1 != *s2)
			return (*s1 - *s2);
		++s1;
		++s2;
	}
}

/*
 * turn a run of TXT records that are known to have the same XFNxxx token
 * into addresses
 */

static int
demangle(int count, const char **av, FN_ref &r, FN_status &s)
{
	char		module[256];
	char		func[256];
	char		tag[128];
	const char	*cp;
	const char	**txtv;
	int		i;
	int		taglen;
	int		(*fn)(int ac, const char *av[], FN_ref_t *ref,
			    FN_status_t *sts);
	int		rval;

	txtv = new const char *[count];
	if (txtv == 0)
		return (0);

	/*
	 * Extract the tag from first TXT record.  All TXT records have the
	 * same tag at this point.
	 */

	taglen = 0;
	for (cp = av[0]; *cp != '\0'; ++cp) {
		if (*cp == ' ' || *cp == '\t')
			break;
		if (isascii(*cp) && isupper(*cp))
			tag[taglen++] = (char) tolower(*cp);
		else
			tag[taglen++] = *cp;
		if (taglen >= sizeof (tag) - 1)
			break;
	}
	tag[taglen] = '\0';

	/*
	 * Skip the tag part and following white space in TXT records.
	 */

	for (i = 0; i < count; ++i) {
		cp = av[i] + taglen;
		while (*cp == ' ' || *cp == '\t')
			++cp;
		txtv[i] = cp;
	}

#ifdef DEBUG
	if (strcmp(tag, "a") == 0) {
		fn = a__txt_records_to_addrs;
		rval = fn(count, txtv, (FN_ref_t *)&r, (FN_status_t *)&s);
		delete[] txtv;
		return (rval);
	}
#endif

	strcpy(func, tag);
	strcat(func, "__txt_records_to_addrs");
	strcpy(module, "fn_inet_");
	strcat(module, tag);
	if (fn = (int (*)(int, const char *[], FN_ref_t *, FN_status_t *))
	    fns_link_symbol(func, module)) {
		rval = fn(count, txtv, (FN_ref_t *)&r, (FN_status_t *)&s);
		delete[] txtv;
		return (rval);
	}

	delete[] txtv;
	return (0);
}

#ifdef DEBUG

extern "C"
int
a__txt_records_to_addrs(
	int ac,
	const char *av[],
	FN_ref_t *ref,
	FN_status_t *)
{
	const char	*cp;
	int		i;
	FN_ref		&r = *(FN_ref *)ref;

	for (i = 0; i < ac; ++i) {
		cp = av[i];
		FN_ref_addr	a(FN_identifier(
		    (unsigned char *)"inet_ipaddr_string"),
		    strlen(cp), cp);
		if (!r.append_addr(a))
			return (0);
	}
	return (1);
}
#endif
