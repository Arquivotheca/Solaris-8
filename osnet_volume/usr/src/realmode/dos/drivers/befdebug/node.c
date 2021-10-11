/*
 *  Copyright (c) 1997, by Sun Microsystems, Inc.
 *  All rights reserved.
 */

/*
 * The #ident directive is commented out because it causes an error
 * in the MS-DOS linker.
 *
#ident "@(#)node.c	1.5	97/10/30 SMI"
 */

/*
 *	Node handling routines for BEFDEBUG.  These routines manipulate the
 *	internal tables based on calls from the driver under test.
 */

#include "befdebug.h"

#define	NS_IDLE		0
#define	NS_OPEN		1

static int node_state = NS_IDLE;
static ushort node_start;
static int cur_node;
static ushort nodes_stored;

static int node_install(int);
static int node_probe(int);
static int res_install(int, char far *, DWORD far *, DWORD far *);
static int res_probe(int, char far *, DWORD far *, DWORD far *);

/* Reset the node state machine.  Called when starting a new test */
void
node_reset(void)
{
	cur_node = -1;
	node_state = NS_IDLE;
	nodes_stored = 0;
}

ushort
node_count(void)
{
	return (nodes_stored);
}

int
node(int a)
{
	int ret = BEF_FAIL;
	char *s;

	if (callback_state != CALLBACK_ALLOWED) {
		printf("%s: driver made a %s callback after initial return\n",
			prog_name, "node");
	}

	switch (a) {
	case NODE_START:
		s = "NODE_START";
		break;
	case NODE_FREE:
		s = "NODE_FREE";
		break;
	case NODE_DONE:
		s = "NODE_DONE";
		break;
	case NODE_INCOMPLETE:
		s = "NODE_INCOMPLETE";
		break;
	default:
		s = "unrecognized value";
		break;
	}
	UDprintf(("Driver called node op(%s).\n", s));

	switch (bd.function) {
	case TEST_PROBE:
		ret = node_probe(a);
		break;
	case TEST_INSTALL:
		ret = node_install(a);
		break;
	default:
		printf(("%s internal error: node: %s.\n", prog_name,
			" unexpected test function"));
		break;
	}

	UDprintf(("node op %s.\n", ret == BEF_OK ? "succeeded" : "failed"));
	return (ret);
}

int
resource(int a, char far *b, DWORD far *c, DWORD far *d)
{
	int ret = BEF_FAIL;

	if (callback_state != CALLBACK_ALLOWED) {
		printf("%s: driver made a %s callback after initial return\n",
			prog_name, "resource");
	}

	UDprintf(("Driver called resource op for \"%s\".\n", b));

	switch (bd.function) {
	case TEST_PROBE:
		ret = res_probe(a, b, c, d);
		break;
	case TEST_INSTALL:
		ret = res_install(a, b, c, d);
		break;
	default:
		printf(("%s internal error: resource: %s.\n", prog_name,
			" unexpected test function"));
		break;
	}

	UDprintf(("resource op %s.\n",
		ret == BEF_OK ? "succeeded" : "failed"));
	return (ret);
}

static int
node_probe(int a)
{

	bd.node_called = 1;
	switch (a) {
	case NODE_START:
		UDprintf(("node_op(NODE_START)\n"));
		if (node_state != NS_IDLE) {
			printf(("node_op(NODE_START) called with a node "
				"open.\n"));
			/* Proceed anyway */
		}
		if (bd.table_size < bd.table_max) {
			set_name(bd.node_tab[bd.table_size].name,
				"node_start");
			node_state = NS_OPEN;
			node_start = bd.table_size;
			bd.table_size++;
		} else {
			set_name(bd.node_tab[bd.table_size - 1].name,
				"OVERFLOW");
			printf(("%s table overflow.\n", prog_name));
		}
		return (BEF_OK);
	case NODE_FREE:
		UDprintf(("node_op(NODE_FREE)\n"));
		if (node_state != NS_OPEN) {
			printf(("node_op(NODE_FREE) called with no node "
				"open.\n"));
			return (BEF_FAIL);
		}
		node_state = NS_IDLE;
		bd.table_size = node_start;
		return (BEF_OK);
	case NODE_DONE:
	case NODE_INCOMPLETE:
		UDprintf(("node_op(NODE_%s)\n", a == NODE_DONE ?
				"DONE" : "INCOMPLETE"));
		if (bd.table_size < bd.table_max) {
			set_name(bd.node_tab[bd.table_size].name,
				"node_end");
			bd.table_size++;
			nodes_stored++;
		} else {
			set_name(bd.node_tab[bd.table_size - 1].name,
				"OVERFLOW");
			printf(("%s table overflow.\n", prog_name));
		}
		node_state = NS_IDLE;
		return (BEF_OK);
	default:
		printf(("%s: node_op called with unrecognized argument.\n",
			prog_name));
		return (BEF_FAIL);
	}
}

static int
node_install(int a)
{
	switch (a) {
	case NODE_START:
		if (node_state != NS_IDLE) {
			printf(("node_op(NODE_START) called with a node "
				"open.\n"));
			/* Implicitly close the old node if successful */
		}
		if (cur_node == -1) {
			if (dos_strcmp(bd.node_tab[0].name, "node") == 0) {
				cur_node = 0;
				node_state = NS_OPEN;
				return (BEF_OK);
			}
			return (BEF_FAIL);
		}
		if (dos_strcmp(bd.node_tab[cur_node].name, "node"))
			return (BEF_FAIL);
		for (cur_node++; cur_node < bd.table_size &&
				dos_strcmp(bd.node_tab[cur_node].name,
				"node"); cur_node++)
			continue;
		if (cur_node < bd.table_size) {
			node_state = NS_OPEN;
			return (BEF_OK);
		}
		return (BEF_FAIL);
	case NODE_FREE:
		printf(("Unexpected node_op(NODE_FREE) call during install "
			"test.\n"));
		return (BEF_FAIL);
	case NODE_INCOMPLETE:
		printf(("Unexpected node_op(NODE_INCOMPLETE) call during "
			"install test.\n"));
		return (BEF_FAIL);
	case NODE_DONE:
		if (node_state != NS_OPEN) {
			printf("node_op(NODE_DONE) called with no node "
				"open.\n");
			return (BEF_FAIL);
		}
		node_state = NS_IDLE;
		return (BEF_OK);
	}
}

static int
res_probe(int a, char far *b, DWORD far *c, DWORD far *d)
{
	ushort i;

	switch (a & 0xF) {
	case RES_SET:
		UDprintf(("set_res: \"%s\", count = %lx", b, *d));
		if (*d) {
			UDprintf((", data ="));
			for (i = 0; i < *d && i < MaxTupleSize; i++)
				UDprintf((" %lx", c[i]));
		}
		UDprintf((".\n"));

		if (bd.table_size < bd.table_max) {
			set_name(bd.node_tab[bd.table_size].name, b);
			bd.node_tab[bd.table_size].len = (ushort)d[0];
			for (i = 0; i < d[0]; i++) {
				bd.node_tab[bd.table_size].val[i] = c[i];
			}
			bd.table_size++;
		}
		else
			printf(("%s table overflow.\n", prog_name));
		break;
	case RES_REL:
		UDprintf(("rel_res: \"%s\", count = %lx", b, *d));
		if (*d) {
			UDprintf((", data ="));
			for (i = 0; i < *d && i < MaxTupleSize; i++)
				UDprintf((" %lx", c[i]));
		}
		UDprintf((".\n"));

		/* Make sure there is a device node open */
		if (node_state != NS_OPEN) {
			printf(("%s: rel_res for \"%s\": %s\n",
				prog_name, b, "no active device node"));
			return (BEF_FAIL);
		}

		/*
		 * Search for the resource to release.  Start at the end
		 * of the table and stop at the start of the current node.
		 */
		for (i = bd.table_size - 1; i > node_start; i--) {
			if (far_strcmp(bd.node_tab[i].name, b) == 0)
				break;
		}
		if (i == node_start) {
			printf(("%s: rel_res for \"%s\": %s\n",
				prog_name, b, "no matching set_res"));
			return (BEF_FAIL);
		}
		/* Remove the resource, compress any gap in the table */
		bd.table_size--;
		for (; i < bd.table_size; i++)
			bd.node_tab[i] = bd.node_tab[i + 1];
		break;
	default:
		return (BEF_FAIL);
	}
	return (BEF_OK);
}

static int
res_install(int a, char far *b, DWORD far *c, DWORD far *d)
{
	int node = cur_node;
	unsigned i;
        
	if (a != RES_GET) {
		printf("resource call during install was not get_res.\n");
		return (BEF_FAIL);
	}

	if (node < 0 || node >= bd.table_size ||
			dos_strcmp(bd.node_tab[node].name, "node")) {
		printf("Driver called get_res with no node open.\n");
		return (BEF_FAIL);
	}
	for (node++; node < bd.table_size &&
			dos_strcmp(bd.node_tab[node].name, "node");
			node++)
		if (far_strcmp(bd.node_tab[node].name, b) == 0) {
			if (*d < bd.node_tab[node].len) {
				printf(("%s: res_install: not enough %s.\n",
					prog_name, "room for resource data"));
				return (BEF_FAIL);
			}
			*d = bd.node_tab[node].len;
			for (i = 0; i < bd.node_tab[node].len; i++) {
				c[i] = bd.node_tab[node].val[i];
			}
			return (BEF_OK);
		}

	if (far_strcmp(b, "name") == 0 && *d >= 2) {
		/*
		 * Caller asked for "name" and there was none.
		 * Assume the table was built during a legacy probe
		 * so the bus type is ISA and there is no name.
		 */
		*d = 2;
		c[0] = 0;
		c[1] = RES_BUS_ISA;
		return (BEF_OK);
	}
	
	/* Spec says no match gives 0 length rather than failure */
	*d = 0;
	return (BEF_OK);
}
