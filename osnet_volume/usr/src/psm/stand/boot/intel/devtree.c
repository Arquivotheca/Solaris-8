/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)devtree.c	1.22	99/06/06 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/salib.h>
#include <sys/promif.h>
#include "devtree.h"

#define	LINESIZE   80		/* Should probably come from BIOS! 	*/

extern struct bootops bootops;	    /* Ptr to bootop vector		*/
extern int bkern_mount(struct bootops *bop, char *dev, char *mpt, char *type);
extern int bgetproplen(struct bootops *, char *, phandle_t);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int get_bin_prop(char *cp, char **bufp, char *cmd);

extern struct dnode devtree[];

struct dnode *active_node = &option_node;

#ifndef	ASSERT
#ifdef	DEBUG
#define	ASSERT(EX)	{ if (EX) \
			prom_panic("Assertion failed: (%s) file %s line %d\n", \
			"EX", __FILE__, __LINE__); }
#else
#define	ASSERT(EX)
#endif
#endif

#if	defined(NODEID_TRANSLATION)

/*
 * We only need nodeid translation if we can't safely cast a pointer
 * to an integer.  Without nodeid translation, we treat phandle_t's
 * accross the bootops interface as pointers.  With nodeid translation,
 * we look up the pointer in a table we allocate on the fly. Depending
 * on where boot's data segment lives, you may still be able to safely
 * cast between the types even if the sizes are different.
 *
 * Keep a matrix of phandle_t -> struct dnode * translations, allocated
 * on the fly.  Each dimension of the matrix has at most NID_ENTRIES.
 * The table can manage NID_ENTRIES * NIDENTRIES unique ids. At 256
 * entries per dimension, the table can manage 64k unique nodeids.
 *
 * Allocate integers starting at NID_BASE, an arbitrary base.
 */

static void ***phandle_to_dnode_matrix;
static uint32_t next_phandle = (uint32_t)NID_BASE;
#define	NID_ENTRIES	256
#define	NID_MASK	((NID_ENTRIES) - 1))
#define	NID_SHIFT	8

/*
 * Allocate a phandle, and associate this dnode * with it.
 */
phandle_t
dnode2phandle(struct dnode *ptr)
{
	uint32_t row, column, nodeid, n;
	void **p;

	nodeid = next_phandle++;
	n = nodeid - NID_BASE;

	if (nodeid >= (NID_BASE + (NID_ENTRIES << NID_SHIFT)))
		prom_panic("dev_dnode_2_phandle: out of nodeids\n");

	/*
	 * If this is the first time, allocate the columns ...
	 */
	if (phandle_to_dnode_matrix == NULL)
		phandle_to_dnode_matrix =
		    (void ***)bkmem_alloc((sizeof (void *)) * NID_ENTRIES);

	row = n >> NID_SHIFT;
	ASSERT(row < NID_ENTRIES);

	/*
	 * If the row doesn't exist, allocate it now ...
	 */
	if ((p = phandle_to_dnode_matrix[row]) == NULL)
		p = phandle_to_dnode_matrix[row] =
		    (void **)bkmem_alloc((sizeof (void *)) * NID_ENTRIES);

	column = n & NID_MASK;
	ASSERT(p[column] == NULL);

	p[column] = ptr;

	return ((phandle_t)nodeid);
}

struct dnode *
phandle2dnode(phandle_t nodeid)
{
	uint32_t row, column, n;
	void **p;

	n = (uint32_t)nodeid;
	n -= NID_BASE;

	ASSERT(phandle_to_dnode_matrix != NULL);

	row = n >> NID_SHIFT;
	ASSERT(row < NID_ENTRIES);

	if ((p = phandle_to_dnode_matrix[row]) == NULL)
		return (NULL);

	column = n & NID_MASK;
	return ((struct dnode *)p[column]);
}

static void
reset_phandle_to_dnode_matrix(void)
{
	void **p;
	int row, column;

	if (phandle_to_dnode_matrix == NULL)
		return;

	/*
	 * The first DEFAULT_NODES entries are statically allocated.
	 * Be careful not to reset their values.
	 */
	if (next_phandle > (NID_BASE + DEFAULT_NODES))
		next_phandle = NID_BASE + DEFAULT_NODES;

	for (row = 0; row < NID_ENTRIES; ++row) {
		if ((p = phandle_to_dnode_matrix[row]) == NULL)
			break;
		for (column = row ? 0 : DEFAULT_NODES; column < NID_ENTRIES;
		    ++column)
			p[column] = NULL;
	}
}

#else	/* defined(NODEID_TRANSLATION) */

static void
reset_phandle_to_dnode_matrix(void)
{
}

#endif	/* defined(NODEID_TRANSLATION) */

static void
format_dev_addr(struct dnode *dnp, char *buf, int flag)
{
	/*
	 *    Format a device address:
	 *
	 *    Formats the device address associated with the node at "dnp" into
	 *    the "buf"fer provided.  If the "flag" is non-zero, nodes with no
	 *    address are formatted as "@0,0".  Otherwise, we place a null
	 *    string in the output buffer.
	 */
	extern struct dprop *find_prop_node();
	struct dprop *dpp;

	if (dpp = find_prop_node(dnp, "$at", 0)) {
		/*
		 *  If this node has an "$at" property, use the corresponding
		 *  value as the node address.
		 */
		(void) sprintf(buf, "@%s", dp_value(dpp));

	} else if ((flag != 0) || find_prop_node(dnp, "reg", 0) ||
	    find_prop_node(dnp, "reg ", 0)) {
		/*
		 *  IF node has a "reg" property (or the caller claims there's
		 *  an address we can't find) use the default address, "0,0".
		 */
		(void) strcpy(buf, "@0,0");
	} else {
		/*
		 *  Node has no address component.  Return a null string.
		 */
		*buf = '\0';
	}
}

char *
build_path_name(struct dnode *dnp, char *buf, int len)
{
	/*
	 *  Construct device path:
	 *
	 *    This routine recursively builds the device path associated with
	 *    the node at "dnp", placing the result in indicated "buf"fer which
	 *    is "len" bytes long.  If "buf" is NULL, it prints the path name
	 *    rather than copying it into the buffer.
	 */
	char *bend = &buf[len-1];
	char *cp = "/";

	if (dnp != &root_node) {
		/*
		 *  Recursively call this routine until we reach the root node.
		 *  Then copy each path component into the buffer [or write it
		 *  to stdout] as we unwind the stack frames.  Each invocation
		 *  returns the the "buf"fer location immediately behind the
		 *  node name it copies over.
		 *
		 *  Statically allocating the name buffer saves stack space.
		 */
		static char nbuf[MAX1275NAME+MAX1275ADDR];
		char *nnp = nbuf;

		cp = nnp + (dnp->dn_parent == &root_node);
		buf = build_path_name(dnp->dn_parent, buf, len);
		(void) bgetprop(&bootops, "name", nnp+1, MAX1275NAME,
			dnp->dn_nodeid);
		*nnp = '/';

		format_dev_addr(dnp, strchr(nnp, '\0'), 0);
	}

	if (buf != (char *)0) {
		/*
		 *  Copy this node name into the output buffer with a lead-
		 *  ing slash and a trailing null.  Also update the "buf"
		 *  pointer along the way.
		 */

		while (*cp && (buf < bend))
			*buf++ = *cp++;
		*buf = '\0';

	} else {
		/*
		 *  Print the next path component to stdout!
		 */

		printf(cp);
	}

	return (buf);
}

static int
maxnodesize(struct dnode *parent)
{
	int		k = 0, j;
	struct dnode	*np;
	char		node_name[MAX1275NAME], node_addr[MAX1275ADDR];

	/*
	 * only run this routine once as necessary. if a node gets
	 * added to this parent then dn_maxchildname is zero'd.
	 */
	if (parent->dn_maxchildname)
		return (parent->dn_maxchildname);

	for (np = parent->dn_child; np; np = np->dn_peer) {
		format_dev_addr(np, node_addr, 1);
		j = bgetprop(&bootops, "name", node_name,
		    sizeof (node_name), np->dn_nodeid);
		if (j < 0)
			j = 0;

		j += strlen(node_addr);
		if (j > k)
			k = j;
	}

	parent->dn_maxchildname = k;
	return (parent->dn_maxchildname);
}

struct dnode *
find_node(char *path, struct dnode *anp)
{
	/*
	 *  Convert pathname to device node:
	 *
	 *    This is the equivalent of the kernel's "namei" routine.  It
	 *    traverses the indicated pathname and returns a pointer to the
	 *    device tree node identified by that path.
	 */
	int j, k, x;
	char *cp, *xp = 0;
	struct dnode *dnp;
	phandle_t alias_nodeid = alias_node.dn_nodeid;

	if (*(cp = path) != '/') {
		/*
		 *  Path name is relative, check to see if first component is
		 *  an alias!  If there are any device args, they apply to the
		 *  device named by the alias.
		 */
		while (*cp && (*cp != '/') && (*cp != ':'))
			cp++;
		j = *cp;
		*cp = '\0';

		if (((k = bgetproplen(&bootops, path, alias_nodeid)) > 0) &&
		    (xp = bkmem_alloc(x = k + strlen(cp + 1) + 1))) {
			/*
			 *  Yes, we have an alias!  Buy a buffer large enough
			 *  to contain the alias value followed by the
			 *  remaining pathname components and load the alias
			 *  value into it.  Concatenate the remainder of the
			 *  path after restoring the termination character.
			 */
			(void) bgetprop(&bootops, path, xp, k, alias_nodeid);
		}

		*cp = j;
		if (xp)
			(void) strcat(path = xp, cp);
	}

	for (dnp = ((*path == '/') ? &root_node : anp); dnp; path = cp) {
		/*
		 *  The "dnp" register points to the device node corresponding
		 *  to that portion of the path that's already been traversed.
		 */
		int l;
		char *ap;

		while (*path && (*path == '/'))
			path++;
		for (cp = path;
		    *cp && *cp != '/' && *cp != '@' && *cp != ':';
		    cp++);

		l = cp - path;
		if (l <= 0) {
			/*
			 *  We've exhausted the input.  The current node
			 *  ("dnp") is the one the caller wants!
			 */
			break;
		}

		/*
		 * set "ap" to point to the address portion of the next path
		 * component.  This may be taken from the input pathname or
		 * it may be the default address: "0,0".
		 */
		k = 0;

		ap = ((*cp == '@') ? cp : "@0,0");
		while (ap[k] && (ap[k] != '/') && (ap[k] != ':'))
			k++;

		if ((l == 2) && (*cp != '@') && (strncmp(path, "..", 2) == 0)) {
			/*
			 *  If next path component is "..", step back up
			 *  the tree.  If we're at the root, ".." is equivalent
			 *  to ".".
			 */
			if (dnp->dn_parent)
				dnp = dnp->dn_parent;

		} else if ((l > 1) || (*path != '.') || (*cp == '@')) {
			/*
			 *  If next path component is a real node name
			 *  (not "." or ".."), search the current node's
			 *  child list.
			 */
			for (dnp = dnp->dn_child; dnp; dnp = dnp->dn_peer) {
				/*
				 *  Step thru the current node's child list
				 *  looking for a node that matches the next
				 *  component of the path name.  The "l"
				 *  register holds the length of this component.
				 */
				unsigned char child_name[MAX1275NAME];
				unsigned char child_addr[MAX1275ADDR];

				(void) bgetprop(&bootops, "name",
					(char *)child_name, MAX1275NAME,
					dnp->dn_nodeid);
				format_dev_addr(dnp, (char *)child_addr, 1);

				if (!(j = strncmp(path,
				    (char *)child_name, l)) &&
				    !(j = (0 - child_name[l])) &&
				    !(j = strncmp(ap, (char *)child_addr, k)) &&
				    !(j = (0 - child_addr[k]))) {
					/*
					 *  Name and address components match.
					 *  This is the node we want, so move
					 *  on to the next component of
					 *  the path name.
					 */
					break;
				}
			}
		}

		if ((*cp == '@') || (*cp == ':')) {
			/*
			 *  Skip over the address portion (if any) of the
			 *  current path component.
			 */
			while (*cp && (*cp != '/'))
				cp++;
		}
	}

	if (xp)
		bkmem_free(xp, x);
	return (dnp);
}

/*
 *  Device tree bootops:
 *
 *     These routines are called through the bootops vector.  Most are
 *     very straightforward ...
 */

/*ARGSUSED*/
phandle_t
bpeer(struct bootops *bop, phandle_t node)
{
	struct dnode *dnp;

	/* Return handle for given node's next sibling */

	if (node == 0)
		return (root_node.dn_nodeid);

	dnp = phandle2dnode(node);
	return (dnp->dn_peer ? dnp->dn_peer->dn_nodeid : 0);
}

/*ARGSUSED*/
phandle_t
bchild(struct bootops *bop, phandle_t node)
{
	struct dnode *dnp;

	/* Return handle for given node's first child */

	dnp = phandle2dnode(node);
	return (dnp->dn_child ? dnp->dn_child->dn_nodeid : 0);
}

/*ARGSUSED*/
phandle_t
bparent(struct bootops *bop, phandle_t node)
{
	struct dnode *dnp;

	/* Return handle for given node's parent */

	dnp = phandle2dnode(node);
	return (dnp->dn_parent ? dnp->dn_parent->dn_nodeid : 0);
}

/*ARGSUSED*/
phandle_t
bmknod(struct bootops *bop, phandle_t node)
{
	/*
	 * Create a device tree node:
	 *
	 * The new node becomes the first child of the specified parent "node";
	 * we can't sort it into the peer list yet because it doesn't have a
	 * "name" property!
	 *
	 * Returns a pointer to the new node or NULL if we can't get memory.
	 * Also makes the new node the active node.
	 */

	struct dnode *np = phandle2dnode(node);
	struct dnode *dnp = (struct dnode *)bkmem_alloc(sizeof (struct dnode));

	if (dnp == NULL) {
		printf("mknod: no memory\n");
		return (NULL);
	}

	/*  We've got the memory, now link into the tree */
	bzero((caddr_t)dnp, sizeof (struct dnode));
	dnp->dn_parent = np;
	dnp->dn_nodeid = dnode2phandle(dnp);

	/*
	 * Don't touch the ordering of the nodes. Insert the new
	 * node maintaining a fifo order. Certain kernel drivers,
	 * namely the four port dnet card, need to receive the
	 * nodes in the order that bootconf creates them.
	 */
	dnp->dn_peer = NULL;

	if (np->dn_child == NULL)
		np->dn_child = dnp;
	else {
		struct dnode *lp = np->dn_child;
		while (lp->dn_peer)
			lp = lp->dn_peer;
		lp->dn_peer = dnp;
	}
	active_node = dnp;

	return (dnp->dn_nodeid);
}

/*
 *  Stubs:
 *
 *	The following bootops are not yet implemented:
 */

/*ARGSUSED*/
ihandle_t
bmyself(struct bootops *bop)
{
	/*  Return an ihandle for active node. */
	return (0);
}

/*ARGSUSED*/
int
binst2path(struct bootops *bop, ihandle_t dev, char *path, int len)
{
	/*  Convert an instance (ihandle) to a path name */
	return (0);
}

/*ARGSUSED*/
phandle_t
binst2pkg(struct bootops *bop, ihandle_t dev)
{
	/*  Convert an instance (ihandle) to a package (phandle) */
	return (0);
}

/*ARGSUSED*/
int
bpkg2path(struct bootops *bop, phandle_t node, char *path, int len)
{
	/*  Convert a package (phandle) to a path name */
	return (0);
}

/*
 *  Boot interpreter commands:
 *
 *      These boot interpreter commands may be used to navigate the device
 *	tree.  They are also used by realmode code to construct the device
 *	tree in the first place (aside from the "standard" nodes that were
 *      established by "setup_devtree").
 */

void
dev_cmd(int argc, char **argv)
{
	/*
	 *  Set the active node:
	 *
	 *  This is similar to the UNIX "cd" command, except that the (optional)
	 *  argument is a device tree path rather than a file path.  If the path
	 *  argument is omitted, "/chosen" is assumed.
	 */
	char *path = ((argc > 1) ? argv[1] : "/chosen");
	struct dnode *dnp = find_node(path, active_node);

	if (dnp) {
		/*
		 *  If "find_node" was able to locate the dnode struct,
		 *  it becomes the new active node ...
		 */
		active_node = dnp;

	} else {
		/*
		 *  Otherwise, we have a user error:
		 */

		printf("%s not found\n", argv[1]);
	}
}

/*ARGSUSED*/
void
pwd_cmd(int argc, char **argv)
{
	/*
	 *  Print active node name:
	 *
	 *  The "build_path_name" routine does the real work; all we have to do
	 *  is add the trailing newline.
	 */
	(void) build_path_name(active_node, 0, 0);
	printf("\n");
}

/*ARGSUSED*/
void
ls_cmd(int argc, char **argv)
{
	/*
	 *  Print names of all children of the active node:
	 *
	 *    This is much like the UNIX "ls" command, except that it doesn't
	 *    support fifty-jillion options.
	 */

	struct dnode *dnp;
	char node_name[MAX1275NAME+MAX1275ADDR];
	int j, k, n = maxnodesize(active_node)+2;

	j = 0;			/* "j" is no. of names on current line */
	n += 3;			/* "n" is length of each name */
	k = LINESIZE / n;	/* "k" is max names per output line */
	if (k == 0)
		k = 1;

	for (dnp = active_node->dn_child; dnp; dnp = dnp->dn_peer) {
		/*
		 *  Print the child list across the screen.  We allow no
		 *  less than four spaces between each name.
		 */
		int x = bgetprop(&bootops, "name", node_name,
		    sizeof (node_name), dnp->dn_nodeid);
		char *cp = &node_name[x-1];

		format_dev_addr(dnp, cp, 0);
		while (*cp)
			(cp += 1, x += 1);
		while (x++ < n)
			*cp++ = ' ';
		*cp = '\0';

		printf("%s", node_name);

		if ((++j >= k) || !dnp->dn_peer) {
			/*  That's it for this line! */
			printf("\n");
			j = 0;
		}
	}
}

/*
 * Free any dynamically created device nodes below this branch.
 *
 * We make use of the the fact that there are no statically
 * created nodes below any dynamic ones.
 */
static struct dnode *
free_dnodes(struct dnode *dnp)
{
	struct dnode *peer = dnp->dn_peer;
	extern void free_props();

	if (dnp->dn_child) {
		(void) free_dnodes(dnp->dn_child);
	}

	if (peer) {
		peer = free_dnodes(peer);
		dnp->dn_peer = peer;
	}

	if ((dnp < &devtree[0]) || (dnp > &devtree[DEFAULT_NODES - 1])) {
		dnp->dn_parent->dn_child = peer;
		free_props(dnp->dn_root.dp_link[left].address);
		bkmem_free((caddr_t)dnp, sizeof (struct dnode));
		return (peer);
	} else {
		if (dnp->dn_parent) { /* check for non root node */
			dnp->dn_parent->dn_child = dnp;
		}
		return (dnp);
	}
}

/*ARGSUSED*/
void
resetdtree_cmd(int argc, char **argv)
{
	/*
	 * Reset the device tree to its initial state.
	 * Delete any dynamically created device nodes, by walking
	 * the device tree and bkmem_free all dnodes except the
	 * default dnodes (devtree[0] - devtree[DEFAULT_NODES - 1]).
	 *
	 * For each dnode freed also free all associated properties.
	 */
	if (argc > 1) {
		printf("no arguments to resetdtree_cmd\n");
		return;
	}
	/*
	 * recursively delete dynamically created device nodes
	 * starting at the root
	 */
	(void) free_dnodes(&devtree[0]);
	reset_phandle_to_dnode_matrix();
}

void
mknod_cmd(int argc, char **argv)
{
/* XXX Merced fix-me need to port */
#if defined(i386) || defined(__i386)
	/*
	 *  Create a device node:
	 *
	 *    This is analogus to the UNIX "mknod" command.  It creates the
	 *    node named by its first argument and sets its "reg" property
	 *    to the value of the second argument (if the second argument is
	 *    omitted, no "reg" property is created).  When present, the second
	 *    argument must be a comma-separated integer list a-la the
	 *    "setbinprop" command.
	 *
	 *    This command has the side effect of changing the active node to
	 *    the newly created node (unless it fails, of course).
	 */
	if (argc >= 2) {
		/*
		 *  User-supplied path name is in "argv[1]".   The last part
		 *  of this string becomes the "name" property of the new node.
		 */
		struct dnode *dxp, *dnp = active_node;
		struct dnode *save = active_node;
		char *path = argv[1];
		char *regs = 0;
		int n = -1;
		char *cp;
		phandle_t xnode;

		if ((argc > 2) &&
		    ((n = get_bin_prop(argv[2], &regs, "mknod")) < 0)) {
			/*
			 *  Second argument (reg list) is ill-formed.
			 *  The "get_bin_prop" routine has already issued an
			 *  error message.
			 */
			return;
		}

		if (cp = strrchr(path, '/')) {
			/*
			 *  Non-simple path name.  Use the "find_node" routine
			 *  to locate the parent node to which we'll attach the
			 *  new node.
			 */
			*cp = '\0';	/* Remove the last slash! */

			if (cp == path) {
				/*
				 *  New node is to be a child of the root!
				 */
				dnp = &root_node;
			} else if (!(dnp = find_node(path, active_node))) {
				/*
				 *  Path was bogus.  Print an error message
				 *  and bail out.
				 */
				printf("%s not found\n", path);
				return;
			}

			path = cp+1; /* Node name is last part of path name */
		}

		if (strchr(path, '@') || strchr(path, ':')) {
			/*
			 *  These characters are not valid in a device name!
			 */
			printf("mknod: bogus path name\n");
			return;
		}

		if (xnode = bmknod(&bootops, dnp->dn_nodeid)) {
			dxp = phandle2dnode(xnode);
			/*
			 *  The "mknod" bootop created the node for us, all we
			 *  have to do now is name it.  The only way this could
			 *  fail is if we were to run out of memory.
			 */
			if (bsetprop(&bootops, "name", path,
			    strlen(path)+1, xnode) || ((n >= 0) &&
			    bsetprop(&bootops, "reg", regs, n, xnode))) {
				/*
				 *  No memory left; "bsetprop" already
				 *  generated the error message, but we have
				 *  to unlink the partially created dev node.
				 *  Also restore the active node pointer JIC
				 *  "bmknod" changed it!
				 */
				active_node = save;
				dnp->dn_child = dxp->dn_peer;
				bkmem_free((caddr_t)dxp, sizeof (struct dnode));
			}
		}

		if (n > 0) {
			/*
			 *  We have a register property buffer that needs
			 *  to be freed.
			 */
			bkmem_free(regs, n);
		}

	} else {
		/*
		 *  User forgot the pathname argument.
		 */

		printf("usage: mknod path [regs]\n");
	}
#endif
}

struct nlink			/* Path name changing element used by	*/
{				/* .. "show-devs" command.		*/
	struct nlink *next;	/* .. .. ptr to next element		*/
	char *name;		/* .. .. name of this element		*/
};

static void
show_nodes(struct dnode *dnp, struct nlink *lp, struct nlink *root)
{
	/*
	 *  Depth-first device tree search:
	 *
	 *    This recursive routine is used by the "show-devs" command to
	 *    search the device tree in a depth-first manner, looking for any
	 *    and all nodes with a "$at" property.  Such nodes are assumed to
	 *    be "real" device nodes, and the corresponding path names are
	 *    printed.
	 */
	for (dnp = dnp->dn_child; dnp; dnp = dnp->dn_peer) {
		/*
		 *  Check all children of the current node.  Those with a "$at"
		 *  property are printed before we descend recursively.
		 */
		char *buf;
		int j = bgetproplen(&bootops, "name", dnp->dn_nodeid);

		if (buf = bkmem_alloc(j+MAX1275ADDR)) {
			/*
			 *  We have a buffer into which we can place the
			 *  current node name.  Build it up and see if we
			 *  should print it now.  Then add a "/" to the end
			 *  of the name buffer and recursively descend
			 *  into the named node.
			 */
			struct nlink link, *nlp;
			lp->next = &link;
			link.name = buf;
			link.next = 0;

			(void) bgetprop(&bootops, "name", buf, j,
			    dnp->dn_nodeid);
			format_dev_addr(dnp, &buf[j-1], 0);

			if (buf[j-1] != '\0') {
				/*
				 *  This node has a "$at" property, which means
				 *  that it's a true device node and we should
				 *  print its name.
				 */
				for (nlp = root; nlp; nlp = nlp->next)
					printf("%s", nlp->name);
				printf("\n");
			}

			(void) strcat(buf, "/");
			show_nodes(dnp, &link, root);

			bkmem_free(buf, j+MAX1275ADDR);
		}
	}
}

void
show_cmd(int argc, char **argv)
{
	/*
	 *  List configured devices:
	 *
	 *    This command produces a depth-first list of all nodes below the
	 *    target that have a "$at" property [i.e, that correspond to real
	 *    devices].  The target node is either the node specified by the
	 *    first argument, or the root node when the argument is omitted.
	 */
	struct dnode *dnp = ((argc > 1) ? find_node(argv[1], &root_node) :
	    &root_node);

	if (dnp != 0) {
		/*
		 *  The "dnp" register now points to the target node.  Build
		 *  up the initial path name and use the "show_nodes" routine
		 *  above to print the names of all device nodes below the
		 *  target.
		 */
		int j;
		int plen = 0;
		char *path = "";
		struct nlink link;

		if (argc > 1) {
			/*
			 *  Caller has supplied a target node name.  It may or
			 *  may not have a trailing slash.  If so, we can use
			 *  it as-is ...
			 */
			j = strlen(argv[1]);

			if ((*((path = argv[1])+j-1) != '/') &&
			    (path = bkmem_alloc(plen = j+2))) {
				/*
				 *  ... but if not, we have to copy it into a
				 *  slightly bigger buffer and insert the
				 *  trailing slash ourselves.
				 */
				(void) strcpy(path, argv[1]);
				path[j++] = '/';
				path[j] = '\0';

			} else if (!path) {
				/*
				 *  Error message if we run out of memory here.
				 *  But we're silent about failing mallocs from
				 *  this point on!
				 */
				printf("show-devs: no memory\n");
				return;
			}
		}

		link.name = path;
		link.next = 0;

		show_nodes(dnp, &link, &link);
		if (plen) bkmem_free(path, plen);

	} else {
		/*
		 *  Target node specification was bogus!
		 */

		printf("show-devs: %s not found\n", argv[1]);
	}
}

void
mount_cmd(int argc, char **argv)
{
	/*
	 *  Mount command:
	 *
	 *    The mount bootop does the real work.
	 */
	if (argc > 2) {
		/*
		 *  Call the "mount" bootop, which will print an error
		 *  message if it finds something wrong.
		 */

		(void) bkern_mount(&bootops, argv[1], argv[2],
		    (argc > 3) ? argv[3] : 0);
	} else {
		/*
		 *  Bogus command syntax.
		 */

		printf("usage: mount device path [type]\n");
	}
}
