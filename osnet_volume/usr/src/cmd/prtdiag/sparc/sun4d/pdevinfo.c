
#ident	"@(#)pdevinfo.c	1.27	99/04/29 SMI"

/*
 * Copyright (c) 1990 Sun Microsystems, Inc.
 */

/*
 * For machines that support the openprom, fetch and print the list
 * of devices that the kernel has fetched from the prom or conjured up.
 *
 */

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

/*
 * function declarations
 */
static void		getpropval(struct openpromio *);
static char *get_node_type(Prom_node *);
static Prom_node *dev_find_type(Prom_node *, char *);
static int node_failed(Prom_node *);

static int prom_fd;
extern char *progname;
extern char *promdev;
extern void getppdata();
extern void printppdata();

char *badarchmsg =
	"System architecture does not support this option of this command.\n";

/*
 * Define DPRINT for run-time debugging printf's...
 * #define DPRINT	1
 */

/* #define DPRINT	1 */
#ifdef	DPRINT
static	char    vdebug_flag = 1;
#define	dprintf	if (vdebug_flag) printf
static void dprint_dev_info(caddr_t, dev_info_t *);
#endif	DPRINT

extern int _doprnt(char *, va_list, FILE   *);

/*VARARGS1*/
int
_error(char *fmt, ...)
{
	int saved_errno;
	va_list ap;
	extern int errno;
	saved_errno = errno;

	if (progname)
		(void) fprintf(stderr, "%s: ", progname);

	va_start(ap);

	(void) vfprintf(stderr, fmt, ap);

	va_end(ap);

	(void) fprintf(stderr, ": ");
	errno = saved_errno;
	perror("");

	return (2);
}

int
is_openprom(void)
{
	Oppbuf	oppbuf;
	register struct openpromio *opp = &(oppbuf.opp);
	register unsigned int i;

	opp->oprom_size = MAXVALSIZE;
	if (ioctl(prom_fd, OPROMGETCONS, opp) < 0)
		exit(_error("OPROMGETCONS"));

	i = (unsigned int)((unsigned char)opp->oprom_array[0]);
	return ((i & OPROMCONS_OPENPROM) == OPROMCONS_OPENPROM);
}

/*
 * Read all properties and values from nodes.
 * Copy the properties read into the prom_node passsed in.
 */
void
dump_node(Prom_node *node)
{
	Oppbuf oppbuf;
	register struct openpromio *opp = &oppbuf.opp;
	Prop *prop = NULL;	/* tail of properties list */

	/* clear out pointers in pnode */
	node->props = NULL;

	/* get first prop by asking for null string */
	(void) memset((void *) oppbuf.buf, 0, BUFSIZE);

	opp->oprom_size = MAXPROPSIZE;
	while (opp->oprom_size != 0) {
		Prop *temp;	/* newly allocated property */

		/* allocate space for the property */
		if ((temp = (Prop *) malloc(sizeof (Prop))) == NULL) {
		    perror("malloc");
		    exit(1);
		}

		/*
		 * get property
		 */
		opp->oprom_size = MAXPROPSIZE;

		if (ioctl(prom_fd, OPROMNXTPROP, opp) < 0)
			exit(_error("OPROMNXTPROP"));

		if (opp->oprom_size == 0) {
			free(temp);
		} else {
			temp->name.opp.oprom_size = opp->oprom_size;
			(void) strcpy(temp->name.opp.oprom_array,
				opp->oprom_array);

			(void) strcpy(temp->value.opp.oprom_array,
				temp->name.opp.oprom_array);
			getpropval(&temp->value.opp);
			temp->size = temp->value.opp.oprom_size;

			/* everything worked so link the property list */
			if (node->props == NULL)
				node->props = temp;
			else if (prop != NULL)
				prop->next = temp;
			prop = temp;
			prop->next = NULL;
		}
	}
}

int
promopen(int oflag)
{
	/*CONSTCOND*/
	while (1)  {
		if ((prom_fd = open(promdev, oflag)) < 0)  {
			if (errno == EAGAIN)   {
				(void) sleep(5);
				continue;
			}
			if (errno == ENXIO)
				return (-1);
			exit(_error("cannot open %s", promdev));
		} else
			return (0);
	}
	/*NOTREACHED*/
}

void
promclose(void)
{
	if (close(prom_fd) < 0)
		exit(_error("close error on %s", promdev));
}

/*
 * Read the value of the property from the PROM deivde tree
 */
static void
getpropval(struct openpromio *opp)
{
	opp->oprom_size = MAXVALSIZE;

	if (ioctl(prom_fd, OPROMGETPROP, opp) < 0)
		exit(_error("OPROMGETPROP"));
}

int
next(int id)
{
	Oppbuf	oppbuf;
	register struct openpromio *opp = &(oppbuf.opp);
	/* LINTED */
	int *ip = (int *)(opp->oprom_array);

	(void) memset((void *) oppbuf.buf, 0, BUFSIZE);

	opp->oprom_size = MAXVALSIZE;
	*ip = id;
	if (ioctl(prom_fd, OPROMNEXT, opp) < 0)
		return (_error("OPROMNEXT"));
	/* LINTED */
	return (*(int *)opp->oprom_array);
}

int
child(int id)
{
	Oppbuf	oppbuf;
	register struct openpromio *opp = &(oppbuf.opp);
	/* LINTED */
	int *ip = (int *)(opp->oprom_array);

	(void) memset((void *) oppbuf.buf, 0, BUFSIZE);
	opp->oprom_size = MAXVALSIZE;
	*ip = id;
	if (ioctl(prom_fd, OPROMCHILD, opp) < 0)
		return (_error("OPROMCHILD"));
	/* LINTED */
	return (*(int *)opp->oprom_array);
}

/*
 * Check if the Prom node passed in contains a property called
 * "board#". All children of the XDBus and all nodes which
 * have board spoecific information will have a board# property
 */
int
has_board_num(Prom_node *node)
{
	Prop *prop = node->props;

	/*
	 * walk thru all properties in this PROM node and look for
	 * board# prop
	 */
	while (prop != NULL) {
		if (strcmp(prop->name.opp.oprom_array, "board#") == 0)
		    return (1);

		prop = prop->next;
	}

	return (0);
}	/* end of has_board_num() */

/*
 * Retrieve the value of the board number property from this Prom
 * node. It has the type of int.
 */
int
get_board_num(Prom_node *node)
{
	Prop *prop = node->props;

	/*
	 * walk thru all properties in this PROM node and look for
	 * board# prop
	 */
	while (prop != NULL) {
		if (strcmp(prop->name.opp.oprom_array, "board#") == 0)
			/* LINTED */
			return (*((int *)prop->value.opp.oprom_array));

		prop = prop->next;
	}

	return (-1);
}	/* end of get_board_num() */

/*
 * Find the requested board struct in the system device tree.
 */
Board_node *
find_board(Sys_tree *root, int board)
{
	Board_node *bnode = root->bd_list;

	while ((bnode != NULL) && (board != bnode->board_num))
		bnode = bnode->next;

	return (bnode);
}	/* end of find_board() */

/*
 * Add a board to the system list in order. Initialize all pointer
 * fields to NULL.
 */
Board_node *
insert_board(Sys_tree *root, int board)
{
	Board_node *bnode;
	Board_node *temp = root->bd_list;

	if ((bnode = (Board_node *) malloc(sizeof (Board_node))) == NULL) {
		perror("malloc");
		exit(1);
	}
	bnode->nodes = NULL;
	bnode->next = NULL;
	bnode->board_num = board;

	if (temp == NULL)
		root->bd_list = bnode;
	else if (temp->board_num > board) {
		bnode->next = temp;
		root->bd_list = bnode;
	} else {
		while ((temp->next != NULL) && (board > temp->next->board_num))
			temp = temp->next;
		bnode->next = temp->next;
		temp->next = bnode;
	}
	root->board_cnt++;

	return (bnode);
}	/* end of insert_board() */

/*
 * This function searches through the properties of the node passed in
 * and returns a pointer to the value of the name property.
 */
char *
get_node_name(Prom_node *pnode)
{
	Prop *prop;

	if (pnode == NULL) {
		return (NULL);
	}

	prop = pnode->props;
	while (prop != NULL) {
		if (strcmp("name", prop->name.opp.oprom_array) == 0)
			return (prop->value.opp.oprom_array);
		prop = prop->next;
	}
	return (NULL);
}	/* end of get_node_name() */

/*
 * This function searches through the properties of the node passed in
 * and returns a pointer to the value of the name property.
 */
static char *
get_node_type(Prom_node *pnode)
{
	Prop *prop;

	if (pnode == NULL) {
		return (NULL);
	}

	prop = pnode->props;
	while (prop != NULL) {
		if (strcmp("device_type", prop->name.opp.oprom_array) == 0)
			return (prop->value.opp.oprom_array);
		prop = prop->next;
	}
	return (NULL);
}	/* end of get_node_type() */

/*
 * Do a depth-first walk of a device tree and
 * return the first node with the name matching.
 */

Prom_node *
dev_find_node(Prom_node *root, char *name)
{
	char *node_name;
	Prom_node *node;

	if (root == NULL)
		return (NULL);

	/* search the local node */
	if ((node_name = get_node_name(root)) != NULL) {
		if (strcmp(node_name, name) == 0) {
			return (root);
		}
	}

	/* look at your children first */
	if ((node = dev_find_node(root->child, name)) != NULL)
		return (node);

	/* now look at your siblings */
	if ((node = dev_find_node(root->sibling, name)) != NULL)
		return (node);

	return (NULL);	/* not found */
}	/* end of dev_find_node() */

/*
 * Start from the current node and return the next node besides
 * the current one which has the requested name property.
 */
Prom_node *
dev_next_node(Prom_node *root, char *name)
{
	Prom_node *node;

	if (root == NULL)
		return (NULL);

	/* look at your children first */
	if ((node = dev_find_node(root->child, name)) != NULL)
		return (node);

	/* now look at your siblings */
	if ((node = dev_find_node(root->sibling, name)) != NULL)
		return (node);

	return (NULL);  /* not found */
}	/* end of dev_next_node() */

/*
 * Search for and return a node of the required type. If no node is found,
 * then return NULL.
 */
static Prom_node *
dev_find_type(Prom_node *root, char *type)
{
	char *node_type;
	Prom_node *node;

	if (root == NULL)
		return (NULL);

	/* search the local node */
	if ((node_type = get_node_type(root)) != NULL) {
		if (strcmp(node_type, type) == 0) {
			return (root);
		}
	}

	/* look at your children first */
	if ((node = dev_find_type(root->child, type)) != NULL)
		return (node);

	/* now look at your siblings */
	if ((node = dev_find_type(root->sibling, type)) != NULL)
		return (node);

	return (NULL);  /* not found */
}

/*
 * Search a device tree and return the first failed node that is found.
 * (has a 'status' property)
 */
Prom_node *
find_failed_node(Prom_node * root)
{
	Prom_node *pnode;

	if (root == NULL)
		return (NULL);

	if (node_failed(root)) {
		return (root);
	}

	/* search the child */
	if ((pnode = find_failed_node(root->child)) != NULL)
		return (pnode);

	/* search the siblings */
	if ((pnode = find_failed_node(root->sibling)) != NULL)
		return (pnode);

	return (NULL);
}	/* end of find_failed_node() */

/*
 * Start from the current node and return the next node besides
 * the current one which is failed. (has a 'status' property)
 */
Prom_node *
next_failed_node(Prom_node * root)
{
	Prom_node *pnode;
	Prom_node *parent;

	if (root == NULL)
		return (NULL);

	/* search the child */
	if ((pnode = find_failed_node(root->child)) != NULL) {
		return (pnode);
	}

	/* search the siblings */
	if ((pnode = find_failed_node(root->sibling)) != NULL) {
		return (pnode);
	}

	/* backtracking the search up through parents' siblings */
	parent = root->parent;
	while (parent != NULL) {
		if ((pnode = find_failed_node(parent->sibling)) != NULL)
			return (pnode);
		else
			parent = parent->parent;
	}

	return (NULL);
}	/* end of find_failed_node() */

/*
 * node_failed
 *
 * This function determines if the current Prom node is failed. This
 * is defined by having a status property containing the token 'fail'.
 */
static int
node_failed(Prom_node *node)
{
	void *value;

	/* search the local node */
	if ((value = get_prop_val(find_prop(node, "status"))) != NULL) {
		if ((value != NULL) && strstr((char *)value, "fail"))
			return (1);
	}
	return (0);
}

/*
 * Get a property's value. Must be void * since the property can
 * be any data type. Caller must know the *PROPER* way to use this
 * data.
 */
void *
get_prop_val(Prop *prop)
{
	if (prop == NULL)
		return (NULL);

	return ((void *)(&prop->value.opp.oprom_array));
}	/* end of get_prop_val() */

/*
 * Search a Prom node and retrieve the property with the correct
 * name.
 */
Prop *
find_prop(Prom_node *pnode, char *name)
{
	Prop *prop;

	if (pnode  == NULL) {
		return (NULL);
	}

	if (pnode->props == NULL) {
		(void) printf("Prom node has no properties\n");
		return (NULL);
	}

	prop = pnode->props;
	while ((prop != NULL) && (strcmp(prop->name.opp.oprom_array, name)))
		prop = prop->next;

	return (prop);
}	/* end of find_prop() */
