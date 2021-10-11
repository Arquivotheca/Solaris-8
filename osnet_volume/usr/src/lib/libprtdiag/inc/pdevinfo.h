/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PDEVINFO_H
#define	_PDEVINFO_H

#pragma ident	"@(#)pdevinfo.h	1.2	99/10/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* structures necessary to hold Openprom data */

/*
 * 128 is the size of the largest (currently) property name
 * 4096 - MAXPROPSIZE - sizeof (int) is the size of the largest
 * (currently) property value that is allowed.
 * the sizeof (u_int) is from struct openpromio
 */
#define	MAXPROPSIZE	128
#define	MAXVALSIZE	(4096 - MAXPROPSIZE - sizeof (uint_t))
#define	BUFSIZE		(MAXPROPSIZE + MAXVALSIZE + sizeof (uint_t))
typedef union {
	char buf[BUFSIZE];
	struct openpromio opp;
} Oppbuf;

typedef struct prop Prop;
struct prop {
	Prop *next;
	Oppbuf name;
	Oppbuf value;
	int size;	/* size of data in bytes */
};

typedef struct prom_node Prom_node;
struct prom_node {
	Prom_node *parent;	/* points to parent node */
	Prom_node *child;	/* points to child PROM node */
	Prom_node *sibling;	/* point to next sibling */
	Prop *props;		/* points to list of proerties */
};

/*
 * Defines for board types. Unused in sun4d prtdiag, but used in sun4u
 * prtdiag.
 */

typedef struct board_node Board_node;
struct board_node {
	int board_num;
	int board_type;
	Prom_node *nodes;
	Board_node *next;  /* link for list */
};

typedef struct system_tree Sys_tree;
struct system_tree {
	Prom_node *sys_mem;	/* System memory node */
	Prom_node *boards;	/* boards node holds bif info if present */
	Board_node *bd_list;	/* node holds list of boards */
	int board_cnt;		/* number of boards in the system */
};

int do_prominfo(int, char *, int, int);
int is_openprom(void);
void promclose(void);
int promopen(int);
extern char *badarchmsg;
int _error(char *fmt, ...);

/* Functions for building the user copy of the device tree. */
Board_node *find_board(Sys_tree *, int);
Board_node *insert_board(Sys_tree *, int);

/* functions for searching for Prom nodes */
char *get_node_name(Prom_node *);
char *get_node_type(Prom_node *);
Prom_node *dev_find_node(Prom_node *, char *);
Prom_node *dev_next_node(Prom_node *, char *);
Prom_node *dev_find_type(Prom_node *, char *);
Prom_node *dev_next_type(Prom_node *, char *);
Prom_node *sys_find_node(Sys_tree *, int, char *);
Prom_node *find_failed_node(Prom_node *);
Prom_node *next_failed_node(Prom_node *);
int node_failed(Prom_node *);
void dump_node(Prom_node *);
int next(int);
int has_board_num(Prom_node *);
int get_board_num(Prom_node *);
int child(int);

/* functions for searching for properties, extracting data from them */
void *get_prop_val(Prop *);
void getpropval(struct openpromio *);
Prop *find_prop(Prom_node *, char *);

#ifdef	__cplusplus
}
#endif

#endif	/* _PDEVINFO_H */
