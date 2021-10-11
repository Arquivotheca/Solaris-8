/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All Rights Reserved
 */
#ident	"@(#)_collate.h	1.5	95/06/22 SMI"

#define	PLAIN		0
#define	SYMBOL		0x01	/* Symbol information attached */

#define	MAX_WEIGHTS	5
#define	MAX_CHAR_LEN	5
#define	MIN_WEIGHT	10	/* lightest relative weight */

#define	T_FORWARD	0x01
#define	T_BACKWARD	0x02
#define	T_POSITION	0x04

/*
 * Sorting information
 */
#define	USE_FULL_XPG4	0x00000000 /* Default, use the default full XPG4 */
#define	USE_BINARY	0x00000001 /* Use simple machine code comparison */

/*
 * types of collation_identifier
 *	collation_identifer can be:
 *	1) a character
 *		i) a character defined in charmap
 *		ii) character itself
 *		iii) an encoded character
 *	2) a collating element
 *	3) a collating symbol
 *	4) an ellipsis
 *	5) a keyword UNDEFINED
 */
#define	T_CHAR_CHARMAP	0x01
#define	T_CHAR_ID	0x02
#define	T_CHAR_ENCODED	0x04
#define	T_CHAR_COLL_ELM	0x08
#define	T_CHAR_COLL_SYM	0x10
#define	T_ELLIPSIS	0x20
#define	T_UNDEFINED	0x40
#define	T_NULL		0x00

/*
 * types of weights
 */
#define	WT_RELATIVE	0x01
#define	WT_ONE_TO_MANY	0x02
#define	WT_ELLIPSIS	0x04
#define	WT_IGNORE	0x08

/*
 * collating element
 */
typedef struct	collating_element {
	char	name[MAX_ID_LENGTH];
	encoded_val	encoded_val;
} collating_element;

/*
 * one to many information
 */
typedef struct	one_to_many {
	encoded_val	source;
	encoded_val	target;
} one_to_many;

/*
 * collating orders/weight
 */
typedef struct	weight {
	char	type;
	union {
		one_to_many	*o_t_m;
		unsigned int	weight;
		int	index;
	} u;
} weight;

typedef struct	order {
	short	type;
	encoded_val	encoded_val;
	unsigned int	r_weight;	/* relative weight */
	weight	weights[MAX_WEIGHTS];
} order;

union	args {
	encoded_val	*en;
	unsigned char	byte;
	char	*id;
};

typedef struct	header {
	char	magic;
	unsigned int flags; /* what kind of sorting ? */
	char	weight_level;
	char	weight_types[MAX_WEIGHTS];
	int	num_otm[MAX_WEIGHTS];
	int	no_of_coll_elms;
	int	no_of_orders;
} header;

/*
 *	data structure for in memory hashing
 */

typedef struct inttab {
	unsigned int key;
	unsigned int rank;
	unsigned int first_key;
	unsigned int next_key;
	unsigned int first_rank;
	unsigned int next_rank;
} inttab_t;
