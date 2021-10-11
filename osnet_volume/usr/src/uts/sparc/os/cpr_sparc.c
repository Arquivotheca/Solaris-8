/*
 * Copyright (c) 1992-2000, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_sparc.c	1.19	99/10/19 SMI"

/*
 * This module contains functions that only available to sparc platform.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cpr.h>
#include <sys/kmem.h>
#include <sys/errno.h>

#ifdef sparc

static const struct prop_info cpr_prop_info[] = CPR_PROPINFO_INITIALIZER;
static char *cpr_next_component(char **);
static char *cpr_get_prefix(char *);
static char *cpr_build_nodename(dnode_t);

/*
 * Read the nvram properties we modify and save their current values
 * in the caller's cprinfo structure.
 */
int
cpr_get_bootinfo(struct cprinfo *ci)
{
	dnode_t node;
	int i;
	const struct prop_info *pi = cpr_prop_info;

	node = prom_optionsnode();

	if ((node == OBP_NONODE) || (node == OBP_BADNODE)) {
		cmn_err(CE_WARN, "cpr: Invalid default prom node.");

		return (-1);
	}

	/*
	 * In order to keep this routine general, the loop below is
	 * driven by the cpr_prop_info structure (see cpr.h).  If
	 * additional properties must be saved, just add a line to
	 * cpr_prop_info describing the new property.
	 */
	for (i = 0;
	    i < sizeof (cpr_prop_info) / sizeof (struct prop_info); i++) {
		int prop_len;
		char *prop_value = (char *)ci + pi[i].pinf_offset;

		if ((prop_len =
		    prom_getproplen(node, pi[i].pinf_name)) < 0 ||
		    prop_len >= pi[i].pinf_len) {
			cmn_err(CE_WARN,
			    "cpr: Invalid property or length for \"%s\".",
			    pi[i].pinf_name);

			return (-1);
		}

		if (prom_getprop(node, pi[i].pinf_name, prop_value) < 0) {
			cmn_err(CE_WARN,
			    "cpr: Cannot get \"%s\" property.",
			    pi[i].pinf_name);

			return (-1);
		}
		*(prop_value + prop_len) = '\0';
	}

	return (0);
}

/*
 * Set the the nvram properties to the values contained in the incoming
 * cprinfo structure.
 */
int
cpr_set_properties(struct cprinfo *ci)
{
	dnode_t node;
	dnode_t sp[OBP_STACKDEPTH];
	pstack_t *stk;
	int i;
	const struct prop_info *pi = cpr_prop_info;

	stk = prom_stack_init(sp, sizeof (sp));
	node = prom_findnode_byname(prom_nextnode(0), "options", stk);
	prom_stack_fini(stk);

	if ((node == OBP_NONODE) || (node == OBP_BADNODE)) {
		cmn_err(CE_WARN, "cpr: Cannot find \"options\" node.");

		return (ENOENT);
	}

	for (i = 0;
	    i < sizeof (cpr_prop_info) / sizeof (struct prop_info); i++) {
		char *prop_value = (char *)ci + pi[i].pinf_offset;
		int prop_len = strlen(prop_value);

		/*
		 * Note: When doing a prom_setprop you must include the
		 * trailing NULL in the length argument, but when calling
		 * prom_getproplen() the NULL is excluded from the count!
		 */
		if (prom_setprop(node,
		    pi[i].pinf_name, prop_value, prop_len + 1) < 0 ||
		    prom_getproplen(node, pi[i].pinf_name) < prop_len) {
			cmn_err(CE_WARN, "cpr: Can't set "
			    "property %s.\tval=%s.",
			    pi[i].pinf_name, prop_value);

			return (ENXIO);
		}
	}

	return (0);
}

void
cpr_send_notice()
{
	static char cstr[] = "\014" "\033[1P" "\033[18;21H";

	prom_printf(cstr);
	prom_printf("Saving System State. Please Wait... ");
}

void
cpr_spinning_bar()
{
	static int spinix;

	switch (spinix) {
	case 0:
		prom_printf("|\b");
		break;
	case 1:
		prom_printf("/\b");
		break;
	case 2:
		prom_printf("-\b");
		break;
	case 3:
		prom_printf("\\\b");
		break;
	}
	if ((++spinix) & 0x4) spinix = 0;
}

static dnode_t cur_node;

/*
 * Convert a full device path to its shortest unambiguous equivalent.
 * For example, a path which starts out /iommu@x,y/sbus@i,j/espdma . . .
 * might be converted to /iommu/sbus/espdma . . .  If we encounter
 * problems at any point, just output the unabbreviated path.
 */
void
cpr_abbreviate_devpath(char *in_path, char *out_path)
{
	char *position = in_path + 1;	/* Skip the leading slash. */
	char *cmpt;

	cur_node = prom_nextnode(0);
	*out_path = '\0';

	while ((cmpt = cpr_next_component(&position)) != NULL) {
		dnode_t long_match = NULL;
		dnode_t short_match = NULL;
		int short_hits = 0;
		char *name;
		char *prefix = cpr_get_prefix(cmpt);

		/* Go to next tree level by getting first child. */
		if ((cur_node = prom_childnode(cur_node)) == 0) {
			(void) strcpy(out_path, in_path);
			return;
		}

		/*
		 * Traverse the current level and remember the node (if any)
		 * where we match on the fully qualified component name.
		 * Also remember the node of the most recent prefix match
		 * and the number of such matches.
		 */
		do {
			name = cpr_build_nodename(cur_node);
			if (strcmp(name, cmpt) == 0)
				long_match = cur_node;
			if (strncmp(prefix, name, strlen(prefix)) == 0) {
				short_match = cur_node;
				short_hits++;
			}
		} while ((cur_node = prom_nextnode(cur_node)) != 0);

		/*
		 * We don't want to be too dependent on what we know
		 * about how the names are stored.  We just assume that
		 * if there is only one match on the prefix, we can
		 * use it, otherwise we need to use a fully qualified
		 * name.  In the "impossible" cases we just give up
		 * and use the complete input devpath.
		 */
		(void) strcat(out_path, "/");
		if (short_hits == 1) {
			(void) strcat(out_path, prefix);
			cur_node = short_match;
		}
		else
			if (long_match) {
				(void) strcat(out_path, cmpt);
				cur_node = long_match;
			} else {
				(void) strcpy(out_path, in_path);
				return;
			}
	}
	/* We need to copy the target and slice info manually. */
	(void) strcat(out_path, strrchr(in_path, '@'));
}

/*
 * Return a pointer to the next component of a device path or NULL if
 * the entire path has been consumed.  Note that we update the caller's
 * pointer to the current position in the full pathname buffer.
 */
static char *
cpr_next_component(char **path)
{
	static char obuf[64];
	char *slash;
	int len = strlen(*path);

	if (len == 0)
		return (NULL);

	if ((slash = strchr(*path, '/'))) {
		len = slash - *path;
		(void) strncpy(obuf, *path, len);
		obuf[len] = '\0';
		*path += len + 1;	/* Position beyond the slash. */
	} else {
		(void) strcpy(obuf, *path);
		*path += len;		/* Position at the terminal NULL. */
	}

	return (obuf);
}

/*
 * Return a pointer to the prefix (i.e., the basic unqualified node name)
 * Basically, this is the part of the fully qualified name before the @.
 */
static char *
cpr_get_prefix(char *cmpt)
{
	static char	prefix[OBP_MAXDRVNAME];
	char		*at_sign = strchr(cmpt, '@');
	int		len = at_sign ? at_sign - cmpt : strlen(cmpt);

	(void) strncpy(prefix, cmpt, len);
	prefix[len] = '\0';

	return (prefix);
}

/*
 * Build the unambiguous name for the current node, like iommu@f,e10000000.
 * The prefix is just the "name" property, and the qualifier is constructed
 * from the first two (binary) words of the "reg" property.
 */
static char *
cpr_build_nodename(dnode_t node)
{
	static char	name[OBP_MAXPATHLEN];
	int		reg[512];
	char		buf[32]; /* must contain expansion of @%x,%x */
	int		prop_len = prom_getproplen(node, OBP_NAME);

	if (prop_len < 0 || prop_len >= sizeof (name) ||
	    prom_getprop(node, OBP_NAME, name) < 0)
		return ("");
	name[prop_len] = '\0';

	if ((prop_len = prom_getproplen(node, OBP_REG)) <
	    2 * sizeof (int) || prop_len >= sizeof (reg))
		return (name);

	if (prom_getprop(node, OBP_REG, (caddr_t)reg) < 0)
		return (name);

	(void) sprintf(buf, "@%x,%x", reg[0], reg[1]);
	(void) strcat(name, buf);

	return (name);
}

/*
 * Makes a printable list of prom_prop names for error messages
 * Caller must free space.
 */
char *
cpr_enumerate_promprops(char **bufp, size_t *len)
{
	const struct prop_info *pi = cpr_prop_info;
	char *buf;
	size_t sz = 2;	/* for "." */
	int i;

	for (i = 0;
	    i < sizeof (cpr_prop_info) / sizeof (struct prop_info); i++) {
		sz += strlen(pi[i].pinf_name) + 2;	/* + ", " */
	}

	buf = kmem_alloc(sz, KM_SLEEP);
	*buf = '\0';

	for (i = 0;
	    i < sizeof (cpr_prop_info) / sizeof (struct prop_info); i++) {
		if (i != 0)
			(void) strcat(buf, ", ");
		(void) strcat(buf, pi[i].pinf_name);
	}
	(void) strcat(buf, ".");
	*bufp = buf;
	*len = sz;
	return (buf);
}

#endif sparc
