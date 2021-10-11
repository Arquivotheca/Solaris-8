/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnsp_nisplus_root.cc	1.4	99/10/13 SMI"

#include <xfn/xfn.hh>
#include <rpcsvc/nis.h>
#include <synch.h>
#include <string.h>
#include <ctype.h>
#include <rpcsvc/nis.h>
#include "fnsp_nisplus_root.hh"

#ifdef DEBUG
#include <stdio.h> /* for debugging */
#endif

/*
 * Code that figures out whether NIS+ name is in home or foreign
 * NIS+ hierarchy
 */

extern "C" nis_name __nis_local_root();

const FN_string *
FNSP_get_root_name()
{
	static FN_string *root_directory = 0;
	static unsigned int attempted = 0;
	static mutex_t root_directory_lock = DEFAULTMUTEX;

	// Must keep track of whether this has been attempted before.
	// Otherwise, insertion of items into the cache may cause
	// this function to return a different answer (eg. first
	// it returns 0, then after cache update, will say
	// foreign domain is the root)

	if ((root_directory == 0) && (attempted == 0)) {
		char *rootdir = __nis_local_root();
		mutex_lock(&root_directory_lock);
		attempted = 1;
#ifdef DEBUG
		if (rootdir == NULL) {
			fprintf(stderr, "could not get nis local root");
			fflush(stderr);
		}
#endif
		if (root_directory == 0 && rootdir != NULL)
			root_directory = new
			    FN_string((unsigned char *)rootdir);
		mutex_unlock(&root_directory_lock);
	}
	return (root_directory);
}

/* returns whether 'child' is in the same domain as 'parent' */

static int
__in_domain_p(const char *child, const char *parent, size_t clen, size_t plen)
{
	if (plen > clen) {
		/* cannot be parent if name is longer than child */
		return (0);
	}

	size_t start = clen - plen;

	if (strcasecmp(&child[start], parent) != 0) {
		/* tail end of child's name is not that of parent */
		return (0);
	}

	if (start == 0)
		return (1); /* parent == child */

	/*
	 * p=abc.com. c=xabc.com. should NOT be equal
	 * p=abc.com. c=x.abc.com. should be equal
	 */
	if (child[start-1] == '.')
		return (1);
	return (0);
}

/* Returns 1 if given name is in same hierarchy as current NIS+ root */
int
FNSP_home_hierarchy_p(const FN_string &name)
{
	const FN_string* root = FNSP_get_root_name();
	unsigned int sstatus;

	if (root == 0)
		return (0);

	return (__in_domain_p((const char *)name.str(&sstatus),
			    (const char *)root->str(&sstatus),
			    name.charcount(),
			    root->charcount()));
}

/* Returns 1 if given name could be potential ancester of current NIS+ root */
int
FNSP_potential_ancestor_p(const FN_string &name)
{
	const FN_string* root = FNSP_get_root_name();
	unsigned int sstatus;

	if (root == 0)
		return (0); /* config error */

	return (__in_domain_p((const char *)root->str(&sstatus),
			    (const char *)name.str(&sstatus),
			    root->charcount(),
			    name.charcount()));
}

// Return the portion of a directory name that precedes the root name
// (with no trailing dot), or a copy of the name if the directory is
// is not in the same hierarchy as the root.
FN_string *
FNSP_strip_root_name(const FN_string &name)
{
	if (!FNSP_home_hierarchy_p(name)) {
		return (new FN_string(name));
	}
	const FN_string *root = FNSP_get_root_name();
	if (root == 0) {
		return (0);
	}
	int len = name.charcount() - root->charcount();
	if (len > 0) {
		len--;	// no trailing dot
	}
	// %%% We rely on the following returning the empty string if len==0.
	// That's the only reasonable behavior, but it's not documented to
	// work that way.  Ugh!
	return (new FN_string(name, 0, len - 1));
}

// Return nonzero if "orgname" is the local NIS+ domain.

int
FNSP_local_domain_p(const FN_string &orgname)
{
	const char *domain = nis_local_directory();
	return (domain != 0 &&
		strcmp(domain, (const char *)orgname.str()) == 0);
}

// Return the minimum-component prefix of "str1" such that the omitted
// suffix is also a suffix of "str2".  Any trailing dot is stripped from
// the result if all of "str2" is a suffix of "str1".
// eg:	min_dotname_prefix("a.b.x.d.",  "c.d.")  => "a.b.x"
//	min_dotname_prefix("a.b.c.d.",  "c.d.")  => "a.b"
//	min_dotname_prefix("a.b.c.dd.", "c.d.")  => "a.b.c.dd."

static FN_string *
min_dotname_prefix(const FN_string &str1, const FN_string &str2)
{
	const char *s1 = (const char *)str1.str();
	const char *s2 = (const char *)str2.str();
	int len1 = str1.bytecount();
	int len2 = str2.bytecount();

	// Set "len1" to index of first char of largest common suffix.
	while (len1 > 0 && len2 > 0) {
		if (tolower(s1[len1 - 1]) == tolower(s2[len2 - 1])) {
			len1--;
			len2--;
		} else {
			break;
		}
	}
	if (len1 == 0) {	// str1 == str2
		return (new FN_string());
	}
	// Strip trailing dot if "str2" is a suffix of "str1".
	if (len2 == 0 && s1[len1 - 1] == '.') {
		len1--;
	}
	// Advance "len1" to the dot at the beginning of the next component.
	while (s1[len1] != '.' && s1[len1] != '\0') {
		len1++;
	}
	return (new FN_string(str1, 0, len1 - 1));
}


// Return the shortest prefix of a fully-qualified organization name that
// can be expanded to the original by NIS+ name expansion.
// eg:  In the foo.bar.sun.com domain, "myhost.bar.sun.com." => "myhost".

FN_string *
FNSP_short_orgname(const FN_string &orgname)
{
	const char *domain = nis_local_directory();
	if (domain == 0 || !FNSP_home_hierarchy_p(orgname)) {
		return (new FN_string(orgname));
	}
	return (min_dotname_prefix(orgname, (const unsigned char *)domain));
}
