/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)pdevinfo_sun4d.c 1.5	99/08/24 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <varargs.h>
#include <errno.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/openpromio.h>
#include "pdevinfo.h"
#include "display.h"
#include "display_sun4d.h"

/*
 * This file represents the splitting out of some functionality
 * of prtdiag due to the port to the sun5f platform. The functions
 * which contain sun4d specifics were moved into this module.
 * Analogous functions should reside in the sun5f source
 * directory.
 */

/*
 * Global variables
 */
char	*progname;
char	*promdev = "/dev/openprom";
int	print_flag = 1;
int	logging = 0;

static void add_node(Sys_tree *, Prom_node *);
static Prom_node *walk(Sys_tree *, Prom_node *, int, int);

int
do_prominfo(int syserrlog, char *pgname, int log_flag, int prt_flag)
{
	Sys_tree sys_tree;		/* system information */
	Prom_node *root_node;

	/* set the global flags */
	progname = pgname;
	logging = log_flag;
	print_flag = prt_flag;

	/* set the the system tree fields */
	sys_tree.sys_mem = NULL;
	sys_tree.boards = NULL;
	sys_tree.bd_list = NULL;
	sys_tree.board_cnt = 0;

	if (promopen(O_RDONLY))  {
		exit(_error("openeepr device open failed"));
	}

	if (is_openprom() == 0)  {
		(void) fprintf(stderr, badarchmsg);
		return (1);
	}

	if (next(0) == 0)
		return (1);

	root_node = walk(&sys_tree, NULL, next(0), 0);
	promclose();

	return (display(&sys_tree, root_node, syserrlog));
}

/*
 * Walk the PROM device tree and build the system tree and root tree.
 * Nodes that have a board number property are placed in the board
 * structures for easier processing later. Child nodes are placed
 * under their parents. 'bif' nodes are placed under board structs
 * even if they do not contain board# properties, because of a
 * bug in early sun4d PROMs.
 */
static Prom_node *
walk(Sys_tree *tree, Prom_node *root, int id, int level)
{
	register int curnode;
	Prom_node *pnode;
	char *name;
	int board_node = 0;

	/* allocate a node for this level */
	if ((pnode = (Prom_node *) malloc(sizeof (struct prom_node))) ==
	    NULL) {
		perror("malloc");
		exit(2);	/* program errors cause exit 2 */
	}
	/* assign parent Prom_node */
	pnode->parent = root;

	/* read properties for this node */
	dump_node(pnode);

	name = get_node_name(pnode);

	if (has_board_num(pnode)) {
		add_node(tree, pnode);
		board_node = 1;
	} else if ((name != NULL) && (strcmp(name, "memory") == 0)) {
		tree->sys_mem = pnode;
		board_node = 1;
	} else if ((name != NULL) && (strcmp(name, "bif") == 0)) {
		add_node(tree, pnode);
		board_node = 1;
	}

	if (curnode = child(id)) {
		pnode->child = walk(tree, pnode, curnode, level+1);
	} else {
		pnode->child = NULL;
	}

	if (curnode = next(id)) {
		if (board_node) {
			return (walk(tree, root, curnode, level));
		} else {
			pnode->sibling = walk(tree, root, curnode, level);
		}
	} else if (!board_node) {
		pnode->sibling = NULL;
	}

	if (board_node) {
		return (NULL);
	} else {
		return (pnode);
	}
}

/*
 * This function adds a board node to the board structure where that
 * that node's physical component lives. Note that this function is
 * specific to sun4d. It knows that the only node passed to this
 * routine without a 'board#' property will be the 'bif' node.
 */
static void
add_node(Sys_tree *root, Prom_node *pnode)
{
	int board;
	Board_node *bnode;

	/* add this node to the Board list of the appropriate board */
	if ((board = get_board_num(pnode)) == -1) {
		void *value;

		if ((value = get_prop_val(find_prop(pnode, "reg"))) == NULL) {
			(void) printf("add_node() passed non-board# node\n");
			exit(2);	/* A programming error */
		}
		board = *(int *)value;
	}

	/* find the node with the same board number */
	if ((bnode = find_board(root, board)) == NULL) {
		bnode = insert_board(root, board);
	}

	/* now attach this prom node to the board list */
	if (bnode->nodes != NULL)
		pnode->sibling = bnode->nodes;

	bnode->nodes = pnode;
}
