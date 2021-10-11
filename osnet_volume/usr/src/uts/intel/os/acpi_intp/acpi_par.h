/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_PAR_H
#define	_ACPI_PAR_H

#pragma ident	"@(#)acpi_par.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/* parser definitions */


/*
 * parse stack entry
 */
typedef struct parse_entry {
	short flags;
	short elem;
} parse_entry_t;

/* flags */
#define	P_PARSE		0x0001
#define	P_REDUCE	0x0002
#define	P_LCHOICE	0x0004 /* another list element? */
#define	P_LADD		0x0008 /* append list element */
#define	P_ACCEPT	0x0010 /* done */
#define	P_NOSHIFT	0x0020 /* internal use only */

/* macros assume address of parse stack is pstp->psp */
#define	PARSE_PUSH(PP) \
if ((PP = (parse_entry_t *)stack_push(pstp->psp)) == NULL) \
	return (ACPI_EXC)
#define	PARSE_POP if (stack_pop(pstp->psp)) return (ACPI_EXC)
#define	PARSE_PTR ((parse_entry_t *)stack_top(pstp->psp))


/*
 * value stack entry
 */
typedef struct value_entry {
	void *data;		/* should be at least 32 bits */
	int elem;
} value_entry_t;

/* macros assume address of value stack is pstp->vsp */
#define	VALUE_PUSH(VP) \
if ((VP = (value_entry_t *)stack_push(pstp->vsp)) == NULL) \
	return (ACPI_EXC)
#define	VALUE_POP if (stack_pop(pstp->vsp)) return (ACPI_EXC)
#define	VALUE_PTR ((value_entry_t *)stack_top(pstp->vsp))
#define	VALUE_PTR_N(N) ((value_entry_t *)stack_top(pstp->vsp) + N)

/* specialized for certain types */
#define	VALUE_ELEM_N(N) ((int)(VALUE_PTR_N(N)->elem))
#define	VALUE_DATA_N(N) (VALUE_PTR_N(N)->data)
#define	INT_VAL_N(N) ((int)(VALUE_PTR_N(N)->data))
#define	NAME_VALP_N(N) ((struct name *)(VALUE_PTR_N(N)->data))
#define	ACPI_VALP_N(N) ((struct acpi_val *)(VALUE_PTR_N(N)->data))
#define	ACPI_INT_N(N) (ACPI_VALP_N(N)->val_u.intval)


/*
 * parse function definitions
 */

/* grammar */
typedef struct gram {
	void **parse_table_p;	/* parse table */
				/* lex function */
	int (*lex_p)(struct byst *bp, int context, int consume);
	short max_parse;	/* max index in table */
	short max_elem;		/* max element */
	short eol;		/* EOL terminal */
	short eof;		/* EOF terminal */
} gram_t;

/* parse state */
typedef struct parse_state {
	void *thread;
	void *key;		/* definition block or execution thread */
	struct byst *bp;	/* byte stream */
	void *ns;		/* name space */
	struct acpi_stk *psp;	/* parse stack */
	struct acpi_stk *vsp;	/* value stack */
	struct acpi_stk *nssp;	/* namespace stack */
	int pctx;		/* parse context */
} parse_state_t;

/* parse context */
#define	PCTX_EXECUTE	0x0001	/* in method execution */

extern int parse(gram_t *gp, parse_state_t *pstp);
#ifdef DEBUG
extern void parse_stack_dump(struct acpi_stk *psp, int off);
extern void value_stack_dump(struct acpi_stk *vsp, int off);
#endif


/*
 * grammar independent definitions for parse rules
 */

/* rules flags, sometimes combined with a length */
#define	R_LIST		0x0100
#define	R_ALT		0x0200
#define	R_PROD		0x0400
#define	R_LEX		0x0800
#define	R_LENGTH	0x00FF	/* upto 255 entries */

/*
 * reduction
 *	parse stack: pop (reduced element)
 *	value stack: LHS is top (or special value)
 *		(list or production rules have values above top)
 *	custom action
 *
 * custom action
 *	rflags: rule flags and size (0 for tokens without special lex actions)
 *	pstp: parse_state_t *
 *	return: OK or EXC
 */
typedef int (*reduce_action_p)(int rflags, struct parse_state *pstp);

/*
 * lex
 *	custom action
 *	parse stack: change from parse to reduce
 *	value stack: push element
 *
 * custom action
 *	when done, must have consumed token from byte stream
 *	return: OK, EXC or VALRET (to indicate special value pushed)
 */
typedef int (*lex_action_p)(struct parse_state *pstp);
#define	VALRET (1)


/*
 * list rule
 *
 * parse
 *	parse stack: change to choice
 *	value stack: push list value
 *
 * choice
 *	(on EOF) parse stack: change to reduce, push parse EOF
 *	(otherwise) parse stack: change to add, push parse list element
 *
 * add
 *	value stack: pop and append to list value
 *
 * reduce
 *	parse stack: pop list
 *	value stack: top is list value, top + 1 is EOF
 */
typedef struct {
	int flags;		/* R_LIST */
	reduce_action_p act;	/* EOF action */
	int elem;		/* repeating list element */
} list_rule_t;
/*
 * all rules have the same first two fields
 * any change in the rule formats needs to be reflected in gcomp.c
 */


/*
 * alternative rule
 *
 * parse
 *	parse stack: push matching alternative
 */
typedef struct {
	short look;		/* lookahead token */
	short elem;		/* matching element */
} alt_entry_t;
#define	ALT_DEFAULT (-1)	/* special lookahead value, matches any item */
#define	ALT_TABLE   (-2)	/* use lex fn with context = elem */

typedef struct {
	int flags;		/* R_ALT and length */
	reduce_action_p act;	/* EOF action */
	alt_entry_t *alts;
} alt_rule_t;


/*
 * production rule
 *
 * parse
 *	parse stack: change to reduce, push RHS
 *	value stack: push LHS
 *
 * reduce
 *	parse stack: pop LHS
 *	value stack: pop off RHS, top becomes LHS, top + i = RHS[i]
 *	(when custom action is done, top must be LHS)
 */
typedef struct {
	short elem;		/* RHS element */
} prod_entry_t;

typedef struct {
	int flags;		/* R_PROD and length */
	reduce_action_p act;
	prod_entry_t *prods;
} prod_rule_t;


/*
 * lex rule
 *
 * parse - see lex action above
 */
typedef struct {
	int flags;		/* R_LEX */
	reduce_action_p r_act;
	lex_action_p l_act;
} lex_rule_t;


extern void *parse_table[];	/* main parse table */
extern char *debug_elem_strings[];


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_PAR_H */
