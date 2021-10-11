/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _DEVTREE_H
#define	_DEVTREE_H

#pragma ident	"@(#)devtree.h	1.13	99/06/06 SMI"

#include <sys/types.h>
#include <sys/obpdefs.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct dprop;			/* Forward referece to remove warnings	*/

typedef struct {
	/*
	 *  AVL tree pointer type:
	 */
	struct dprop *address;
	int heavy;
} ptr;

struct dprop
{
	/*
	 *  Property list node:
	 *
	 *  There is one of these structs associated with each property assigned
	 *  to a given node.  They hang in an AVL tree that's rooted in the
	 *  corresponding device tree node.
	 *
	 *  A pair of pseudo-methods define computed values:
	 *
	 *	dp_getflag:	A property node's balance flag is -1 if
	 *	dp_setflag:	the subtree rooted at that node is balanced,
	 *			0 if the left subtree is heavy, and 1 if the
	 *			right subtree is heavy.
	 *
	 *			This allows balance flags to be used to index
	 *			into the "dp_link" array.
	 */
	struct dprop *dp_parent; /* Used for tree traversal		*/

	ptr	dp_link[2];	/* AVL tree link fields			*/
#define	left    0		/* .. Points to the left subtree	*/
#define	rite    1		/* .. Points to the right subtree	*/

	short  dp_namsize;	/* Size of prop's name (inlcudes null)	*/
	int    dp_valsize;	/* Size of prop's value			*/
	int    dp_size;		/* Total size (of entire struct)	*/

	char   dp_buff[2];	/* Buffer contaiing name & value	*/
};

#define	dp_name(p)	(&(p)->dp_buff[0])
#define	dp_value(p)	(&(p)->dp_buff[(p)->dp_namsize])

#define	dp_getflag(p)	/* Return property node's balance flag	*/ \
	((p)->dp_link[rite].heavy ? 1: ((int)(p)->dp_link[left].heavy-1))

#define	dp_setflag(p, f)	/* Set property node's balance flag	*/ \
	{ \
	    (p)->dp_link[left].heavy = (p)->dp_link[rite].heavy = 0;	\
	    if ((f) >= 0) (p)->dp_link[f].heavy = 1;			\
	}

#define	dp_setflag_nohvy(p)	/* Set property node's balance flag	*/ \
	{ \
	    (p)->dp_link[left].heavy = (p)->dp_link[rite].heavy = 0;	\
	}

struct dnode
{
	/*
	 *  Device Tree Node:
	 *
	 *	The boot device tree is extremely simple.  Each node consists
	 *	of nothing more a list of associated properties and the pointers
	 *	required to navigate the tree.
	 */
	struct dnode *dn_parent; /* Ptr to parent node (null for root)	*/
	struct dnode *dn_child;	 /* Ptr to 1st child (null for leaves)	*/
	struct dnode *dn_peer;	 /* Next sibling node 			*/
	int dn_maxchildname;	 /* Length of longest child name	*/

	struct dprop dn_root;	/* Head of property list AVL tree	*/
	phandle_t dn_nodeid;	/* external phandle */
};

#define	MAX1275NAME 32		/* Max length of a prop or dev name	*/
#define	MAX1275ADDR 40		/* Maximum formatted device addr	*/

extern struct dnode devtree[];	/* Statically allocated portion of tree */

#define	root_node    	devtree[0]
#define	boot_node    	devtree[1]
#define	bootmem_node 	devtree[2]
#define	alias_node   	devtree[3]
#define	chosen_node  	devtree[4]
#define	mem_node	devtree[5]
#define	mmu_node	devtree[6]
#define	prom_node	devtree[7]
#define	option_node	devtree[8]
#define	package_node	devtree[9]
#define	delayed_node	devtree[10]
#define	itu_node	devtree[11]

#define	DEFAULT_NODES	12

/*
 * Define NODEID_TRANSLATION if we can't safely cast a phandle_t
 * to a structure pointer. If NODEID_TRANSLATION is enabled,
 * we'll keep a table of nodeid -> struct dnode translations.
 * If it's disabled, we'll just cast between the two types.
 */

#if	defined(_LP64)
#define	NODEID_TRANSLATION
#endif

#if	defined(NODEID_TRANSLATION)

/*
 * Arbitrary nodeid base .. for creating artificial 32-bit nodeids.
 */
#define	NID_BASE	0x80000000

#endif	/* defined(NODEID_TRANSLATION) */

struct pprop			/* Pseudo-property definition:		*/
{
    struct dnode *node;		/* .. Node to which the prop belongs	*/
    char  *name;		/* .. The propery's name		*/
    int  (*get)();		/* .. The special "get" function	*/
    void *getarg;		/* .. Instance flag for "get" function	*/
    int  (*put)();		/* .. The special "put" function	*/
    void *putarg;		/* .. Instance flag for "put" function  */
};

extern struct pprop pseudo_props[];
extern int pseudo_prop_count;

#if	defined(NODEID_TRANSLATION)

extern phandle_t dnode2phandle(struct dnode *);
extern struct dnode *phandle2dnode(phandle_t nodeid);

#else	/* defined(NODEID_TRANSLATION) */

#define	dnode2phandle(d)	((phandle_t)(d))
#define	phandle2dnode(p)	((struct dnode *)(p))

#endif	/* defined(NODEID_TRANSLATION) */

#ifndef	NULL
#define	NULL	0
#endif

#ifdef	__cplusplus
}
#endif

#endif /* _DEVTREE_H */
