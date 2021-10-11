/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_path.c	1.14	94/12/10 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

char *
prom_path_gettoken(register char *from, register char *to)
{
	while (*from) {
		switch (*from) {
		case '/':
		case '@':
		case ':':
		case ',':
			*to = '\0';
			return (from);
		default:
			*to++ = *from++;
		}
	}
	*to = '\0';
	return (from);
}

/*
 * Given an OBP pathname, do the best we can to fully expand
 * the OBP pathname, in place in the callers buffer.
 */

/*
 * If we have to complete the addrspec of any component, we can
 * only handle devices that have a maximum of NREGSPECS "reg" specs.
 * We cannot allocate memory inside this function.
 */

#define	NREGSPECS	16

static char buffer[OBP_MAXPATHLEN];
static struct prom_reg regs[NREGSPECS];
static char *zero_zero = "@0,0";

void
prom_pathname(char *pathname)
{
	register char *from = buffer;
	register char *to = pathname;
	char addrspec[64];
	char devname[64];
	register dnode_t node = prom_rootnode();
	int regsize;
	dnode_t sp[OBP_STACKDEPTH];
	pstack_t *stk;

	if ((to == (char *)0) || (*to == (char)0))
		return;

	(void) prom_strcpy(from, to);
	*to = (char)0;

	if ((*from != (char)0) && (*from == '/'))  {
		(void) prom_strcat(to, "/");
		while ((*from != (char)0) && (*from == '/'))
			++from;
	}

	while (*from != (char)0)  {

		from = prom_path_gettoken(from, devname);

		/*
		 * We either got to the '@' or to the ',' part of
		 * the stock symbol e.g. SUNW,esp@0,800000.  If it's
		 * a ',' we have a tad more work to do.  (We also
		 * try to handle the obscure case of multiple ','
		 * in a name)
		 */
		while (*from == ',') {
			(void) prom_strcat(devname, ",");
			from = prom_path_gettoken(++from,
				devname + prom_strlen(devname));
		}
		(void) prom_strcat(to, devname);

		/*
		 * Eliminate the simpler cases; copy addrspec and possibly
		 * any options, if they are already present and continue
		 * on with the next component of the pathname.
		 *
		 * Also, names that are specified as foo@mumble are
		 * explicitly expanded to foo@mumble,0
		 */

		if (*from == '@')  {
			from = prom_path_gettoken(++from, addrspec);
			(void) prom_strcat(to, "@");
			(void) prom_strcat(to, addrspec);
			if (*from == ',') {
				from = prom_path_gettoken(++from, addrspec);
				(void) prom_strcat(to, ",");
				(void) prom_strcat(to, addrspec);
			} else
				(void) prom_strcat(to, ",0");
			goto options;
		}

		/*
		 * We have to default the addrspec by finding the matching
		 * node, and if it exists and it has registers, take the
		 * first matching one.  If it does not exist or does not
		 * have regs, then we default the addprspec to "0,0".
		 */


		stk = prom_stack_init(sp, sizeof (sp));
		node = prom_findnode_byname(node, devname, stk);
		prom_stack_fini(stk);

		if ((node == OBP_NONODE) || (node == OBP_BADNODE))  {
			(void) prom_strcat(to, zero_zero);
			goto options;
		}

		/*
		 * If the node has no reg property and is a leaf node,
		 * default addrspec to @0,0
		 */
		regsize = prom_getproplen(node, OBP_REG);
		if ((regsize == 0) || (regsize == -1))  {
			if (prom_childnode(node) == OBP_NONODE) /* leaf? */
				(void) prom_strcat(to, zero_zero);
			goto options;
		}

		if (regsize > sizeof (regs))  {
			prom_printf("prom_pathname: too many regs in %s\n",
			    devname);
			goto options;
		}
		(void) prom_getprop(node, OBP_REG, (caddr_t)&regs);

		(void) prom_sprintf(addrspec, "@%x,%x",
		    regs[0].hi, regs[0].lo);
		(void) prom_strcat(to, addrspec);

options:
		if (*from == ':') {
			from = prom_path_gettoken(++from, addrspec);
			(void) prom_strcat(to, ":");
			(void) prom_strcat(to, addrspec);
		}

		if ((*from != (char)0) && (*from == '/'))  {
			(void) prom_strcat(to, "/");
			while ((*from != (char)0) && (*from == '/'))
				++from;
		}
	}
}

/*
 * Strip any options strings from an OBP pathname.
 * Output buffer (to) expected to be as large as input buffer (from).
 */
void
prom_strip_options(register char *from, register char *to)
{
	while (*from != (char)0)  {
		if (*from == ':')  {
			while ((*from != (char)0) && (*from != '/'))
				++from;
		} else
			*to++ = *from++;
	}
	*to = (char)0;
}
