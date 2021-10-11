/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bootprop.c	1.40	99/06/06 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/salib.h>
#include <values.h>
#include "devtree.h"

#if defined(i386) || defined(__i386)
#include <sys/machine.h>
#endif

#define	__ctype _ctype		/* Incredibly stupid hack used by	*/
#include <ctype.h>    		/* ".../stand/lib/i386/subr_i386.c"	*/

extern struct pprop pseudo_props[];
extern int pseudo_prop_count;

extern int get_memlist();
extern int get_string(), get_word();

extern void *memcpy(void *s1, void *s2, size_t n);

extern int boldgetproplen(struct bootops *bop, char *name);
extern int boldgetprop(struct bootops *bop, char *name, void *value);
extern int boldsetprop(struct bootops *bop, char *name, char *value);
extern char *boldnextprop(struct bootops *bop, char *prev);
extern int bgetproplen(struct bootops *, char *, phandle_t);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int bnextprop(struct bootops *, char *, char *, phandle_t);

static struct dprop *
prop_alloc(int size)
{

	return ((struct dprop *)bkmem_alloc(size));
}

static void
prop_free(struct dprop *dpp, int size)
{
#if defined(i386) || defined(__i386)
	/*
	 *  Use free routine appropriate to the property node's memory
	 *  location.  The size of each node is encoded within the
	 *  "dprop" struct.
	 * XXX bkmem_alloc'd memory should always call bkmem_free, remove
	 * this when we are 100% sure.
	 */
	if ((uintptr_t)dpp < USER_START) {
		printf("prop_free: dpp < USER_START!\n");
		return;
	}
#endif

	bkmem_free((char *)dpp, size);
}

/*
 *  AVL tree routines:
 *
 *    Technically, I didn't need to separate out the "rotate" routine, but
 *    the code can be shared with an AVL delete routine if there becomes a
 *    need for one.
 */

static int
compare(struct dnode *dnp, char *name, struct dprop *dpp)
{
	/*
	 *  AVL Comparison routine:
	 *
	 *  Returns a balance flag based on the result of a string
	 *  comparison of the two property names.  Note that the root
	 *  node (which has no name) compares greater than everything else.
	 */
	int x;

	if (dpp == &dnp->dn_root)
		return (0);

	x = strcmp(name, dp_name(dpp));

	if (x == 0)
		return (-1);
	if (x > 0)
		return (1);
	return (0);
}

static void
rotate(struct dprop *np, int sub, int bal)
{
	/*
	 *  Rotate a Subtree:
	 *
	 *    This routine is called after adding a property node that
	 *    unbalances the AVL tree.  It rebalances the offending
	 *    "sub"tree of the base node ("np") by performing one of two
	 *    possible rotations.  The "bal"ance flag specifies which
	 *    side of the unbalanced subtree is heavier.
	 *
	 *    A tri-state return code reflects the state of the tree after the
	 *    rotation is performed:
	 *
	 *    0  --  Single rotation, tree height unchanged
	 *    1  --  Single rotation, tree height changed
	 *    2  --  Double rotation
	 *
	 *    Note that a zero return is only possible after node deletion,
	 *    which isn't supported (but easily could be).
	 */
	int j, k, x = bal ^ 1;
	struct dprop *pivot = np->dp_link[sub].address;
	struct dprop *px, *pp, *pr = pivot->dp_link[bal].address;

	j = dp_getflag(pr);
	if (x == j) {
		/* BEGIN CSTYLED */
		/*
		 *  Double rotation;  Graphically, the transformation looks
		 *  like this ...
		 *
		 *  *   [] <-pivot                         pp-> []
		 *  *  /  \                                    /  \
		 *  *  |   [] <-pr                   pivot-> []   [] <-pr
		 *  *  |  /  \              =>              /  \ /  \
		 *  *     |   [] <-pp                       |  | |  |
		 *  *     |  /  \                           |  New  |
		 *  *        |  |
		 *  *         New
		 *
		 *  The "New" node was inserted into one of "pp"s original
		 *  subtrees; the "k" register tells us which one.
		 */
		/* END CSTYLED */
		pp = pr->dp_link[x].address;
		k = dp_getflag(pp);

		px = pp->dp_link[bal].address;
		pr->dp_link[x].address = px;	/* heavy set below */
		if (px)
			px->dp_parent = pr;

		pp->dp_link[bal].address = pr;	/* heavy set below */
		pr->dp_parent = pp;

		px = pp->dp_link[x].address;
		pivot->dp_link[bal].address = px; /* heavy set below */
		if (px)
			px->dp_parent = pivot;

		pp->dp_link[x].address = pivot;	/* heavy set below */
		pivot->dp_parent = pp;

		dp_setflag_nohvy(pp);

		if (k == bal) {
			/* Tree is now balanced below "pr" */
			dp_setflag(pivot, x);
			dp_setflag_nohvy(pr);
		} else {
			/*
			 * Tree may still be unbalanced, but by no more
			 * than 1 node.
			 */
			k = ((k >= 0) ? bal : -1);
			dp_setflag_nohvy(pivot);
			dp_setflag(pr, k);
		}

		k = 2;		/* Let caller know what we just did! */

	} else {
		/* BEGIN CSTYLED */
		/*
		 *  Single rotation; The basic transformation looks like
		 *  this ...
		 *
		 *  *   [] <- pivot                       pr -> [] <- pp
		 *  *  /  \                                    /  \
		 *  *  |   [] <- pr                  pivot-> []   |
		 *  *  |  /  \              =>              /  \  |
		 *  *     |  |                              |  |  New
		 *  *     |  |                              |  |
		 *  *       New
		 *
		 *  Normally, subtrees will grow (or shrink) as a result of this
		 *  transformation, but delete processing can leave subtree
		 *  depth unchanged after a single rotation.  "k" register will
		 *  be zero in this case.
		 */
		/* END CSTYLED */
		px = pr->dp_link[x].address;
		pivot->dp_link[bal].address = px; /* heavy set below */
		if (px)
			px->dp_parent = pivot;

		pr->dp_link[x].address = pivot;	/* heavy set below */
		pivot->dp_parent = pp = pr;

		k = (j >= 0);
		if (k != 0) {
			/* We just rebalanced the tree.  Fix balance flags */
			dp_setflag_nohvy(pivot);
			dp_setflag_nohvy(pr);
		} else {
			/* Tree remains slightly unbalanced (delete's only!) */
			dp_setflag(pivot, bal);
			dp_setflag(pr, bal ^ 1);
		}
	}

	/*
	 *  Set new rotation point in parent node.  "x" register preserves
	 *  the balance flag while we dink with other parts of the link.
	 */

	x = np->dp_link[sub].heavy;
	np->dp_link[sub].address = pp;
	np->dp_link[sub].heavy = x;
	pp->dp_parent = np;
}

struct dprop *
find_prop_node(struct dnode *dnp, char *name, struct dprop *np)
{
	/*
	 *  Find a property node:
	 *
	 *    This routine searches the property list associated with the device
	 *    node at "dp" looking for a property with the given "name" and re-
	 *    turns a pointer to it if found.  If the target property does not
	 *    exist (and "np" is non-null) we add the new node at "np" (which we
	 *    assume has the given name) and return null.
	 */
	int j, k;
	struct dprop *pp, *pq, *ps;
	struct dprop *pr = &dnp->dn_root;

	pq = pr->dp_link[left].address;
	ps = pq;
	while (pq) {
		/*
		 *  Walk the tree looking for the node in question.  When
		 *  (and if) we get to a leaf, the "pr" register will point
		 *  to the root of the subtree requiring a rebalance (if
		 *  there is one).
		 */
		pp = pq;

		j = compare(dnp, name, pp);
		if (j < 0) {
			/*
			 *  This is the node we're looking for.
			 *  Return its address.
			 */
			return (pp);
		}

		pq = pp->dp_link[j].address;
		if (pq != 0 && dp_getflag(pq) >= 0) {
			/*
			 *  If next node ("pq") isn't balanced, its parent
			 *  ("pp") becomes the rotation point for any
			 *  rebalancing operation we may undertake later.
			 */
			pr = pp;
			ps = pq;
		}
	}

	if (np != 0) {
		/*
		 *  Caller wants to add a node.  Make sure its "dp_link" fields
		 *  are clear before going any further.
		 */

		np->dp_link[rite].address = 0;
		np->dp_link[left].address = 0;
		dp_setflag_nohvy(np);

		if (ps != 0) {
			/*
			 *  Tree is non-empty.  The "pp" register points to
			 *  the new node's parent and the "j" register
			 *  specifies which subtree we're to hang it off of.
			 */
			pp->dp_link[j].address = np;
			pp->dp_link[j].heavy = 0;
			np->dp_parent = pp;

			k = compare(dnp, name, ps);
			pp = ps->dp_link[k].address;

			while ((j = compare(dnp, name, pp)) >= 0) {
				/*
				 *  Adjust all balance flags between our base
				 *  ancestor ("ps" register) and the new node
				 *  to reflect the fact that the tree may
				 *  have grown a bit taller.
				 */
				dp_setflag(pp, j);
				pp = pp->dp_link[j].address;
			}

			j = dp_getflag(ps);
			if (j == k) {
				/*
				 * Tree has become unbalanced;
				 * rotate to fix things.
				 */
				rotate(pr, compare(dnp, name, pr), j);
			} else {
				/* Tree has grown, but remains balanced */
				if (j >= 0)
					k = -1;
				dp_setflag(ps, k);
			}

		} else {
			/*
			 *  Tree is empty.  Place the new node on the left side
			 *  of the dummy root node ("pr" register) and mark
			 *  this side heavy.
			 */
			pr->dp_link[left].address = np;
			pr->dp_link[left].heavy = 1;
			np->dp_parent = pr;
		}
	}

	return ((struct dprop *)0);
}

static struct dprop *
descend(struct dprop *dpp)
{
	/*
	 *  Descend left-hand subtree:
	 *
	 *    This routine is used to locate the node to the extreme left of
	 *    the given node.  It is used by the "bnextprop" routine to walk
	 *    the AVL tree.
	 */

	struct dprop *dxp;

	for (dxp = dpp; dxp; dxp = dxp->dp_link[left].address)
		dpp = dxp;

	return (dpp);
}

/*
 * Recursively free all props hung off this property
 */
void
free_props(struct dprop *dpp)
{
	if (dpp->dp_link[left].address) {
		free_props(dpp->dp_link[left].address);
	}

	if (dpp->dp_link[rite].address) {
		free_props(dpp->dp_link[rite].address);
	}

	prop_free(dpp, dpp->dp_size);
}


static struct pprop *
find_special(struct dnode *dnp, char *name)
{
	/*
	 * Search special properties list:
	 *
	 * This routine returns a non-zero value (i.e, a pointer to an
	 * appropriate "pseudo_props" table entry) if the specified
	 * property of the given node is in some way special.
	 */
	struct pprop *sbot = pseudo_props;
	struct pprop *stop = &pseudo_props[pseudo_prop_count];

	while (stop > sbot) {
		/*
		 *  Perform a binary search of the special properties table.
		 *  Comparisons consist of two parts:  Node addresses (the
		 *  fast compare) and property names (the slow compares).
		 *  Obviously, we skip the latter when the former yields
		 *  not-equal!
		 */
		int rc;
		struct pprop *sp = sbot + ((stop - sbot) >> 1);

		if (dnp == sp->node) {
			rc = strcmp(name, sp->name);
			if (rc == 0) {
				/*
				 *  This is the node we want; return
				 *  its address to the caller.
				 */
				return (sp);
			}
		} else {
			/*
			 *  We can skip the string comparison when nodes are
			 *  non-equal,  but we do have to reset the "rc"
			 *  register to tell us which way to move the search.
			 */
			rc = ((dnp > sp->node) ? 1 : -1);
		}

		if (rc > 0)
			sbot = sp+1;
		else
			stop = sp;
	}

	return ((struct pprop *)0);
}

/*ARGSUSED*/
int
put_hex_int(struct dnode *dnp, char *buf, int len, unsigned int *valp)
{
	static char *hexchars = "0123456789ABCDEF";
	char *ptr;
	unsigned int value;
	int i;

	value = 0;
	len -= 3;		/* chars other than "0x" and NULL */
	if (len <= 0 || *buf++ != '0' || *buf++ != 'x') {
		*valp = value;
		return (BOOT_SUCCESS);
	}
	if (len > 8)
		len = 8;
	for (i = 0; i < len && *buf; i++, buf++) {
		if ((ptr = strchr(hexchars, toupper(*buf))) == 0)
			break;
		value = (value << 4) | ((ptr - hexchars) & 0xF);
	}
	*valp = value;
	return (BOOT_SUCCESS);
}

/*ARGSUSED*/
int
get_word(struct dnode *dnp, int *buf, int len, int *value)
{
	/* Return given word as pseudo-property value */

	if (!buf || (len > sizeof (int)))
		len = sizeof (int);
	if (buf)
		(void) memcpy(buf, value, len);

	return (len);
}

/*ARGSUSED*/
int
get_string(struct dnode *dnp, char *buf, int len, char **value)
{
	/* Return given string as pseudo-property value */
	char *vp = ((*value) ? *value : "");
	int lx = strlen(vp)+1;

	if (!buf || (len > lx))
		len = lx;
	if (buf)
		(void) memcpy(buf, vp, len);

	return (len);
}

/*ARGSUSED*/
int
get_memlist(struct dnode *dnp, char *buf, int len, void *head)
{
	/*
	 *  The "get" routine for memory lists.  These are linked lists that
	 *  we copy into the indicated "buf" (except when "buf" is null, in
	 *  which case we simply return the amount of storage required to hold
	 *  the memlist array).
	 */
	int size = 0;
	struct memlist *mlp;

	for (mlp = *((struct memlist **)head); mlp; mlp = mlp->next) {
		/*
		 *  Step thru the memlist calculating size and (if asked),
		 *  copying its contents to the output "buf"fer.
		 */
		if (buf) {
			/*
			 *  We're delivering the real value, copy the next
			 *  address/length pair into the output buffer.
			 *  There's a bit of weirdness here to deal with buffer
			 *  lengths that are not multiples of the word size
			 *  (I don't know why I bother!).
			 */
			int x, n = 2;
			uintptr_t word = mlp->address;

			while (n-- && (len > 0)) {
				/*
				 *  There's at least one byte remaining in
				 *  the output buffer.  Copy as much of the
				 *  next memlist word as will fit and
				 *  update the length registers accordingly.
				 */
				x = ((len -= sizeof (uint64_t)) >= 0) ?
				    sizeof (mlp->address) :
				    (-len & (sizeof (uint64_t)-1));
				(void) memcpy(buf, (caddr_t)&word, x);
				buf += sizeof (uint64_t);
				word = mlp->size;
				size += x;
			}

			if (len <= 0) {
				/*
				 *  We ran out of output buffer.
				 *  Time to bail out!
				 */
				break;
			}
		} else {
			/*
			 *  Caller is just asking for the memlist size.  This
			 *  will work out to 16 bytes per memlist entry.
			 */
			size += (2 * sizeof (uint64_t));
		}
	}

	return (size);
}

static int
check_name(char *name, int size)
{
	/*
	 *  Validate a 1275 name:
	 *
	 *  P1275 has some rather strict rules about what constitutes a name.
	 *  This routine ensures the given "name" follows those rules.
	 *  It returns "BOOT_FAILURE" if it does not.
	 */
	int c;
	char *cp = name;

	if (size > MAX1275NAME) {
		/* Name is too long, bail out now! */

		printf("setprop: name too long\n");
		return (BOOT_FAILURE);
	}

	while (*cp) {
		c = *cp++;
		switch (c) {
		/*
		 *  Only certain characters are legal in a name, so we check
		 *  here to make sure that no disallowd characters are being
		 *  used.  Per 1275 Spec, section 3.2.2.1.1, "The property
		 *  name is a human-readable text string consisting of one
		 *  to thirty-one printable characters. Property names shall
		 *  not contain uppercase characters or the characters
		 *  '/', '\', ':', '[', ']', and '@'. Note '#' is legal, but
		 *  also marks a comment, so must be escaped ie \#.
		 *
		 *  Unfortunately, some properties already exist that use
		 *  upper case eg SUNW-ata-1f0-d1-chs, so we do not exclude
		 *  upper case.
		 */
		case '/':
		case '\\':
		case ':':
		case '[':
		case ']':
		case '@':
			goto bad;
		default:
			if (!isascii(c) || !isprint(c)) {
				goto bad;
			}
		}
	}
	return (size);
bad:
	/* Name contains an invalid character */
	printf("setprop: invalid name\n");
	return (BOOT_FAILURE);
}

/*
 *  These routines implement the boot getprop interface.  These new & improved
 *  versions follow the semantics of the corresponding 1275 forth words.  The
 *  older (slightly different) semantics are still supported by passing in
 *  zeros for the new arguments.
 */

int
boldgetproplen(struct bootops *bop, char *name)
{
	return (bgetproplen(bop, name, 0));
}

/*ARGSUSED*/
int
bgetproplen(struct bootops *bop, char *name, phandle_t node)
{
	/*
	 *  Return the length of the "name"d property's value.  If the "node"
	 *  argument is null (or omitted), we try both the 1275 "/options"
	 *  and "/chosen" nodes.
	 */
	struct pprop *sp;
	struct dprop *dpp;
	struct dnode *dnp;

	struct dnode *nodelist[2];
	int nnodes, i;

	nnodes = 1;
	if (node != NULL) {
		nodelist[0] = phandle2dnode(node);
	} else {
		nodelist[0] = &chosen_node;
		nodelist[1] = &option_node;
		nnodes++;
	}

	for (i = 0; i < nnodes; i++) {
		dnp = nodelist[i];

		sp = find_special(dnp, name);
		if (sp != 0) {
			/*
			 * A special node; use the "get" method to
			 * fetch the length
			 */
			return ((sp->get)(dnp, 0, 0, sp->getarg));
		}

		dpp = find_prop_node(dnp, name, 0);
		if (dpp != 0) {
			/*
			 * We found the property in question,
			 * return the value size
			 */
			return (dpp->dp_valsize);
		}
	}

	/* Property not found, return error code */
	return (BOOT_FAILURE);
}

int
boldgetprop(struct bootops *bop, char *name, void *value)
{
	/* old version only returns BOOT_SUCCESS or BOOT_FAILURE */
	return ((bgetprop(bop, name, value, 0, 0) == BOOT_FAILURE)
	    ? BOOT_FAILURE : BOOT_SUCCESS);
}

/*ARGSUSED*/
int
bgetprop(struct bootops *bop, char *name, caddr_t buf, int size,
	    phandle_t node)
{
	/*
	 *  Return the "name"d property's value in the specified "buf"ffer,
	 *  but don't copy more than "size" bytes.  If "size" is zero (or
	 *  omitted), the buffer is assumed to be big enough to hold the
	 *  value (i.e, caller used "getproplen" to obtain the value size).
	 *  If the "node" argument is null (or omitted), work from either
	 *  the 1275 "/options" node or the "/chosen" node.
	 */
	struct pprop *sp;
	struct dprop *dpp;
	int len = MAXINT;
	struct dnode *dnp;
	struct dnode *nodelist[2];
	int nnodes, i;

	if (size != 0)
		len = size;

	nnodes = 1;
	if (node != NULL) {
		nodelist[0] = phandle2dnode(node);
	} else {
		nodelist[0] = &chosen_node;
		nodelist[1] = &option_node;
		nnodes++;
	}

	for (i = 0; i < nnodes; i++) {
		dnp = nodelist[i];
		sp = find_special(dnp, name);
		if (sp != 0) {
			/* A special property, use the special "get" method */

			len = (sp->get)(dnp, buf, len, sp->getarg);
			return (len);

		}

		dpp = find_prop_node(dnp, name, 0);
		if (dpp != 0) {
			/* Found the property in question; return its value */

			if (len > dpp->dp_valsize)
				len = dpp->dp_valsize;
			(void) memcpy(buf, (caddr_t)dp_value(dpp), len);
			return (len);
		}
	}

	/* Property not found, return error code */
	return (BOOT_FAILURE);
}

static struct dprop *
get_next_dprop(struct dnode *dnp, char *prev)
{
	int climbing = 0;
	struct dprop *dpp, *dxp = 0;

	/*
	 *  Caller has provided a non-null "prev"ious name pointer.
	 *  Unfortunately, this may be the name of a pseudo property.
	 *  If so, our first attempt to locate it will fail ...
	 */
	dpp = find_prop_node(dnp, prev, 0);
	if (dpp == 0) {
		/*
		 *  ... and we'll have to try again with a space
		 *  concatenated onto the tail end!
		 */
		char prev_name[MAX1275NAME+1];

		(void) sprintf(prev_name, "%s ", prev);
		dpp = find_prop_node(dnp, prev_name, 0);
	}

	if (dpp == 0) {
		/*
		 *  The "prev" property was not found, this is
		 *  a programming error!
		 */
		printf("nextprop: property not found\n");
		return (0);
	}

	/*
	 *  Caller is working from an established property.
	 *  Use the "find_prop_node" routine to locate the
	 *  corresponding "dprop" structure, and move forward
	 *  from there.
	 */
	for (;;) {
		/*
		 *  When (and if) this loop exits, "dxp" will
		 *  point to the next property node!
		 */
		if (!climbing && (dxp = dpp->dp_link[rite].address)) {
			/*
			 * If the current node has a right-hand subtree,
			 * we haven't processed it yet.  Descend thru its
			 * left subtrees until we get to a leaf.
			 * This becomes the next property node.
			 */
			dxp = descend(dxp);
			break;
		}

		dxp = dpp->dp_parent;
		if (dxp == &dnp->dn_root) {
			/*
			 *  If parent is root of the tree,
			 *  we're done!  Return zero length
			 *  (or null name ptr) to indicate
			 *  end of list.
			 */
			return (0);
		}

		climbing = compare(dnp, dp_name(dpp), dxp);
		if (climbing == 0) {
			/*
			 *  If we stepped up from the left
			 *  subtree, the parent becomes the
			 *  next property node!
			 */
			break;
		}

		/*
		 *  We stepped up from the right subtree, set the current node
		 *  pointer to the parent node and try again.
		 */
		dpp = dxp;
	}

	return (dxp);
}

char *
boldnextprop(struct bootops *bop, char *prev)
{
	/*
	 * Return the name of the property following the "prev"ious property
	 * (NULL if "prev" is the last property in the list).  Does NOT return
	 * the name of any properties starting with a dollar sign!
	 */
	struct dprop *dxp = 0;
	struct dnode *dnp = &option_node;

	if (prev && *prev) {
		dxp = get_next_dprop(dnp, prev);
	} else {
		dxp = descend(dnp->dn_root.dp_link[left].address);
	}

	if (dxp == 0) {
		/*
		 *  Property list is empty!  Return null to indicate the end of
		 *  the list.
		 */
		return ((char *)0);
	}

	if (*dp_name(dxp) == '$') {
		/*
		 *  Properties with names that begin with a dollar sign are
		 *  said to be "invisible" (i.e, the kernel doesn't see them),
		 *  so we refuse to deliver them.  Since dollar signs sort
		 *  ahead of all legal 1275 name characters, we can skip this
		 *  one by recursively calling ourselves!
		 */
		return (boldnextprop(bop, dp_name(dxp)));
	}

	/*
	 *  Old version just returns a pointer to the name.  Since old
	 *  kernels don't know about pseudo-properties, there's no need
	 *  to check for them here.
	 */
	return (dp_name(dxp));
}

int
bnextprop(struct bootops *bop, char *prev, char *buf, phandle_t node)
{
	/*
	 * Return the name of the property following the "prev"ious property
	 * (NULL if "prev" is the last property in the list).  Does NOT return
	 * the name of any properties starting with a dollar sign!
	 */
	struct dprop *dxp = 0;
	struct dnode *dnp = node ? phandle2dnode(node) : &option_node;
	int len;


	if (prev && *prev) {
		dxp = get_next_dprop(dnp, prev);
	} else {
		dxp = descend(dnp->dn_root.dp_link[left].address);
	}

	if (dxp == 0) {
		/*
		 * Property list is empty!  Return null to indicate
		 * the end of the list.
		 */
		return (0);
	}

	if (*dp_name(dxp) == '$') {
		/*
		 * Properties with names that begin with a dollar sign are
		 * said to be "invisible" (i.e, the kernel doesn't see them),
		 * so we refuse to deliver them.  Since dollar signs sort
		 * ahead of all legal 1275 name characters, we can skip this
		 * one by recursively calling ourselves!
		 */
		return (bnextprop(bop, dp_name(dxp), buf, node));
	}

	if (buf == (char *)0) {
		/*
		 * This is a programming error an should never occur!
		 */
		printf("bnextprop: NULL buf\n");
		return (0);
	}

	/*
	 * Enhanced version copies property name into the indicated
	 * buffer and returns the total name length (including the
	 * null).  We have to be careful of pseudo-property names
	 * that end in a space, however!
	 */
	len = dxp->dp_namsize;
	(void) memcpy(buf, dp_name(dxp), len);

	if (buf[len - 2] == ' ') {
		buf[len - 2] = '\0';
		len--;
	}
	return (len);
}

int
boldsetprop(struct bootops *bop, char *name, char *value)
{
	return (bsetprop(bop, name, value, 0, 0));
}

/*ARGSUSED*/
int
bsetprop(struct bootops *bop, char *name, caddr_t value, int size,
	    phandle_t node)
{
	/*
	 *  Set a property value.  If the property doesn't exist already, we'll
	 *  create it.  Otherwise, we just change its value (which might mean
	 *  re-allocating the storage we've assigned to it.
	 *
	 *  Values are copied into dynamically allocated storage.  This allows
	 *  calling routines to pass stack-local buffers as values.
	 */
	struct pprop *sp;
	struct dprop *dpp = (struct dprop *)0;
	struct dnode *pp;
	struct dnode *dnp = node ? phandle2dnode(node) : &option_node;
	static int recurse = 0;  /* Prevent infinite loops due to recursion */
	int len = size;
	int x;
	int nam_size;
	int tot_size;

	if (size == 0 && node == 0) {
		len = strlen((char *)value) + 1;
	}

	nam_size = check_name(name, strlen(name)+1);
	tot_size = (int)((sizeof (struct dprop) - sizeof (dpp->dp_buff)) +
							nam_size + len);

	if (nam_size <= 0) {
		/*
		 *  Property name is bogus, bail out now!
		 */

		return (BOOT_FAILURE);
	}

	/* begin SPECIAL CASES section */

	/*
	 * bootpath should be mirrored on both /options and /chosen.
	 * boot-path should be mirrored on both /options and /chosen.
	 * whoami should be mirrored on both /options and /chosen.
	 *
	 * boot-args and bootargs should be the same and in sync on both
	 * /options and /chosen.
	 */
	if (!recurse && (dnp == &option_node || dnp == &chosen_node)) {
		struct dnode *mnode;
		char *mname;

		/*
		 *  Mirror values on the node we weren't called with
		 */
		mnode = ((dnp == &option_node) ? &chosen_node : &option_node);
		if (strcmp(name, "bootpath") == 0 ||
		    strcmp(name, "boot-path") == 0 ||
		    strcmp(name, "bootargs") == 0 ||
		    strcmp(name, "boot-args") == 0 ||
		    strcmp(name, "whoami") == 0) {
			recurse = 1;
			(void) bsetprop(bop, name, value, len,
			    mnode->dn_nodeid);
			recurse = 0;
		}

		/*
		 * bootargs and boot-args should be kept the same on both
		 * /chosen and /options
		 *
		 * We've already taken care of mirroring this guy on the
		 * mirror node, now we have to make the copy on both nodes.
		 * E.G., if we are setting "boot-args", we've already made
		 * sure above that the same value is set under both /options
		 * and /chosen.  What we'll do below is ensure that "bootargs"
		 * is set on both those nodes as well.
		 */
		mname = (char *)0;
		if (strcmp(name, "boot-args") == 0) {
			mname = "bootargs";
		} else if (strcmp(name, "bootargs") == 0) {
			mname = "boot-args";
		}

		if (mname) {
			recurse = 1;
			(void) bsetprop(bop, mname, value, len, dnp->dn_nodeid);
			(void) bsetprop(bop, mname, value, len,
			    mnode->dn_nodeid);
			recurse = 0;
		}
	}

	/* end SPECIAL CASES section */

	x = strcmp(name, "name");
	if (x == 0 && (check_name((char *)value, size) < 0)) {
		/*
		 *  Caller is changing this node's "name" property, so we have
		 *  to perform some special checking.  The value of a "name"
		 *  property must, itself, be a valid name.
		 */
		return (BOOT_FAILURE);
	}

	if ((pp = dnp->dn_parent) != 0) {
		/*
		 *  If we're setting a property of a non-root node, we have to
		 *  make sure we're not altering the device tree sort sequence
		 *  by doing so.  Note that "x" register will be zero if we're
		 *  trying to change this node's name.
		 */
		if ((x == 0) || (strcmp(name, "$at") == 0)) {
			/*
			 *  The two properties that influence the sort
			 *  sequence are "name" and "$at" (device address).
			 *  If we're changing either of these, we have to make
			 *  sure that the new node specification doesn't
			 *  conflict with one that already exists.
			 */
			if (x == 0) {
				/*
				 *  If we're not changing the node address,
				 *  figure out what the current node address
				 *  happens to be.  This will be the
				 *  current value of the "$at" property if
				 *  there is one, "0,0" otherwise.
				 */
				dpp = find_prop_node(dnp, "$at", 0);
			} else if (len > MAX1275ADDR) {
				/*
				 *  If we are changing the node address, make
				 *  sure it will fit in our internal buffers!
				 */
				printf("setprop: $at value too long\n");
				return (BOOT_FAILURE);
			} else {
				/*
				 *  If we're not changing the name field, find
				 *  the current value of the name property.
				 *  This is null if we haven't set a name yet.
				 */
				dpp = find_prop_node(dnp, "name", 0);
			}

			if (value && (!x || dpp)) {
				/*
				 *  Changing the node specification
				 *  via its "name" or "$at" property
				 *  will force us to re-sort this
				 *  branch of the tree the next time
				 *  we proceess it.  Clearing the
				 *  parent node's max child name
				 *  length will remind us to do so.
				 */
				pp->dn_maxchildname = 0;
			} else {
				/*
				 *  Nodes must have a "name" property before
				 *  they can acquire a "$at" property (and
				 *  both the name and the address must be
				 *  non-empty).
				 */
				printf("setprop: name must set name "
				    "before $at \n");
				return (BOOT_FAILURE);
			}
		}
	}

	if (sp = find_special(dnp, name)) {
		/*
		 *  A special property.  If it has a "put" method, use that to
		 *  set the new value.  Otherwise, generate an error.
		 */
		if (sp->put) {
			/* Property has a "put" method, use it */
			return ((sp->put)(dnp, value, len, sp->putarg));
		}

		/* Caller is trying to modify a read-only property */
		printf("setprop: read-only property\n");
		return (BOOT_FAILURE);
	} else if (!(dpp = find_prop_node(dnp, name, 0))) {
		/*
		 *  Property node doesn't exist, create a new one.  This means
		 *  allocating enough memory to hold the new "dprop" node,
		 *  setting the appropriate fields, then re-calling
		 *  "find_prop_node" to add the new node to the AVL tree.
		 */
		dpp = prop_alloc(tot_size);
		if (dpp == 0) {
			/*
			 *  Can't get memory for the new property node.
			 */
			printf("setprop: no memory\n");
			return (BOOT_FAILURE);
		}

		dpp->dp_size = tot_size;
		dpp->dp_namsize = nam_size;
		(void) strcpy(dp_name(dpp), name);

		(void) find_prop_node(dnp, name, dpp);

	} else if (dpp->dp_size < tot_size) {
		/*
		 *  Property node exists, but it's not big enough to hold the
		 *  value we're trying to store there.  We have to do what
		 *  amounts to a "kmem_realloc" to obtain a bigger buffer,
		 *  after which we have to patch the "dp_link" in our parent
		 *  and "dp_parent" links in both our descendents.
		 */
		struct dprop *dxp;
		int j = compare(dnp, dp_name(dpp), dpp->dp_parent);

		dxp = prop_alloc(tot_size);
		if (dxp == 0) {
			/*
			 *  Can't get enough memory for the new property
			 *  value.  Deliver appropriate error.
			 */
			printf("setprop: no memory\n");
			return (BOOT_FAILURE);
		}

		/*
		 *  Make a copy of the node and then free the old buffer
		 */
		(void) memcpy(dxp, dpp, dpp->dp_size);
		prop_free(dpp, dpp->dp_size);

		dpp = dxp;
		dpp->dp_size = tot_size;

		/*
		 *  Fix parent node's "dp_link" field.  The "j"
		 *  register tells which one it is.
		 */
		dxp = dpp->dp_parent;
		dxp->dp_link[j].address = dpp;
		dxp->dp_link[j].heavy = 1;

		if ((dxp = dpp->dp_link[left].address) != 0)
			dxp->dp_parent = dpp;
		if ((dxp = dpp->dp_link[rite].address) != 0)
			dxp->dp_parent = dpp;
	}

	(void) memcpy(dp_value(dpp), value, dpp->dp_valsize = len);
	/*
	 * If we just set the node-address property, set the same value
	 * into a "unit-address" property which will be visible to the
	 * upper layers.
	 */
	if (strcmp(name, "$at") == 0) {
		recurse = 1;
		(void) bsetprop(bop, "unit-address", value, len, node);
		recurse = 0;
	}
	return (BOOT_SUCCESS);
}
