/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_par.c	1.1	99/05/21 SMI"


/* parser engine */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>
#include <sys/acpi_prv.h>

#include "acpi_exc.h"
#include "acpi_stk.h"
#include "acpi_bst.h"
#include "acpi_node.h"
#include "acpi_par.h"
#include "acpi_lex.h"


#define	LEX(LCTX, CONS) (*(gp->lex_p))(pstp->bp, LCTX, CONS)

#ifdef DEBUG
void
parse_stack_dump(acpi_stk_t *psp, int off)
{
	int i, elem, flags;
	parse_entry_t *pp;

	for (i = 0; i <= psp->index + off; i++) {
		pp = (parse_entry_t *)(psp->base) + i;
		elem = pp->elem;
		flags = pp->flags;
		exc_debug(ACPI_DPARSE,
		    "%sparse stack %d, elem %s, flags 0x %x",
		    (i == psp->index) ? "*" : "",
		    i, debug_elem_strings[elem], flags);
	}
}

void
value_stack_dump(acpi_stk_t *vsp, int off)
{
	int i;
	value_entry_t ve;

	for (i = 0; i <= vsp->index + off; i++) {
		ve = *((value_entry_t *)(vsp->base) + i);
		exc_debug(ACPI_DPARSE,
		    "%svalue stack %d, elem %s, data 0x %x",
		    (i == vsp->index) ? "*" : "",
		    i, debug_elem_strings[ve.elem], ve.data);
	}
}
#endif

static void
bkpt_hook(void)
{
	int i;

	i = 0;			/* just a convenient place to breakpoint */
	i = i + 1;
}

int bkpt_flag;
int bkpt_offset;

int
parse(gram_t *gp, parse_state_t *pstp)
{
	void *rp;		/* generic rule ptr */
	list_rule_t *lrp;
	alt_entry_t *aep;
	prod_entry_t *pep;
	reduce_action_p rap;
	lex_action_p lap;

	parse_entry_t *pp;
	value_entry_t *vp;
	node_t *np, *np0;
	int p_flags, p_elem, r_flags, look, la_elem, la_set, offset, len, i;
	int old_offset = 0;

	for (;;) {
		p_flags = PARSE_PTR->flags;
		p_elem = PARSE_PTR->elem;

		{
			char *action, *element;
			int problem = 0;

			if (p_flags & P_PARSE)
				action = "parsing";
			else if (p_flags & P_REDUCE)
				action = "reducing";
			else if (p_flags & P_LCHOICE)
				action = "list choice for";
			else if (p_flags & P_LADD)
				action = "list append for";
			else if (p_flags & P_ACCEPT)
				action = "accept with";
			else {
				action = "unknown action on";
				problem = 1;
			}

			if (p_elem > gp->max_elem || p_elem < 0) {
				element = "unknown element";
				problem = 1;
			} else
				element = debug_elem_strings[p_elem];

			exc_debug(ACPI_DPARSE, "%3d  %s %s  (at 0x%x)",
			    pstp->psp->index + 1,
			    action,
			    element,
			    bst_index(pstp->bp));

			if (problem)
				exc_debug(ACPI_DPARSE,
				    "parse flags 0x%x, element %d",
				    p_flags, p_elem);

/* very verbose! */
/*		parse_stack_dump(pstp->psp, 0); */
/*		value_stack_dump(pstp->vsp, 0); */
		}

		/* out of range parse stack element */
		if (p_elem > gp->max_elem || p_elem < 0)
			return (exc_code(ACPI_EINTERNAL));

		/* stored in value data for all parsing except list */
		offset = bst_index(pstp->bp);
		if (bkpt_flag) {
			if (offset == bkpt_offset ||
			    (old_offset < bkpt_offset && offset > bkpt_offset))
				bkpt_hook();
			old_offset = offset;
		}

		/*
		 * parse
		 */
		if (p_flags & P_PARSE) {

			/* ordinary terminal */
			if (p_elem > gp->max_parse) {
				/* parse token */
				if (LEX(CTX_PRI, 1) != p_elem)
					return (exc_code(ACPI_EPARSE));
				PARSE_POP;
				/* push value */
				if ((p_flags & P_NOSHIFT) == 0) {
					VALUE_PUSH(vp);
					vp->elem = p_elem;
					vp->data = (void *)offset;
				}
				continue;
			}

			rp = gp->parse_table_p[p_elem]; /* find rule */
			r_flags = *(int *)rp; /* first field is flags */

			/* parse terminal with lex_action */
			if (r_flags & R_LEX) {
				/* parse token with lex action */
				lap = ((lex_rule_t *)rp)->l_act;
				if (lap && (i = (*lap)(pstp)) == ACPI_EXC)
					return (ACPI_EXC);
				/* set to reduce, if special action */
				rap = ((lex_rule_t *)rp)->r_act;
				if (rap)
					PARSE_PTR->flags = P_REDUCE;
				else
					PARSE_POP;
				/* push value, if not already done */
				if (!(lap && i == VALRET) &&
				    (p_flags & P_NOSHIFT) == 0) {
					VALUE_PUSH(vp);
					vp->elem = p_elem;
					vp->data = (void *)offset;
				}
				continue;
			}

			/* production rule */
			if (r_flags & R_PROD) {
				/* set to reduce */
				PARSE_PTR->flags = P_REDUCE;
				/* push production on to parse stack */
				i = (r_flags & R_LENGTH) - 1;
				pep = ((prod_rule_t *)rp)->prods + i;
				for (; i >= 0; i--, pep--) {
					PARSE_PUSH(pp);
					pp->elem = pep->elem;
					pp->flags = P_PARSE;
				}
				/* push LHS onto value stack */
				VALUE_PUSH(vp);
				vp->elem = p_elem;
				vp->data = NULL;
				continue;
			}

			/* alternative rule */
			if (r_flags & R_ALT) {
				aep = ((alt_rule_t *)rp)->alts;
				len = r_flags & R_LENGTH;
				la_set = 0;
				/* set to reduce */
				PARSE_PTR->flags = P_REDUCE;
				/* push matching production onto parse stack */
				for (i = 0; i < len; i++, aep++)
					/* match anything entry */
					if (aep->look == ALT_DEFAULT) {
						PARSE_PUSH(pp);
						pp->flags = P_PARSE;
						pp->elem = aep->elem;
						break;
					} else if (aep->look == ALT_TABLE) {
						/* use special table context */
						if ((la_elem =
						    LEX(aep->elem|CTX_LOOK, 0))
						    != ACPI_EXC) {
							PARSE_PUSH(pp);
							pp->elem = la_elem;
							pp->flags = P_PARSE;
							break;
						}
					} else { /* get lookahead or EXC */
						if (la_set == 0)
						    look = LEX(CTX_PRI, 0);
						la_set = 1;
						/* check lookahead match */
						if (look != ACPI_EXC &&
						    aep->look == look) {
							PARSE_PUSH(pp);
							pp->elem = aep->elem;
							pp->flags = P_PARSE;
							break;
						}
					}
				if (i >= len) /* nothing matched? */
					return (exc_code(ACPI_EPARSE));
				/* push LHS onto value stack */
				VALUE_PUSH(vp);
				vp->elem = p_elem;
				vp->data = NULL;
				continue;
			}

			/* list rule */
			if (r_flags & R_LIST) {
				/* switch top to choice */
				PARSE_PTR->flags = P_LCHOICE;
				/* push list value */
				VALUE_PUSH(vp);
				vp->elem = p_elem;
				vp->data = NULL; /* data NOT used for offset */
				continue;
			}
				/* unknown rule type */
			return (exc_code(ACPI_EINTERNAL));
		} /* parse */

		/*
		 * reduce/accept
		 */
		if (p_flags & (P_REDUCE|P_ACCEPT)) {

			if (p_elem > gp->max_parse) /* shouldn't happen */
				return (exc_code(ACPI_EINTERNAL));

			rp = gp->parse_table_p[p_elem]; /* find rule */
			r_flags = *(int *)rp; /* get flags */
			rap = ((list_rule_t *)rp)->act;	/* save action */

			if (p_flags & P_REDUCE)
				PARSE_POP; /* pop parse (reduced item) */

			if (r_flags & (R_LIST|R_ALT)) {
				/* list/alt rule - adjust stack */
				VALUE_POP;
			} else if (r_flags & R_PROD) {
				/* production rule - adjust stack */
				if (stack_popn(pstp->vsp, r_flags & R_LENGTH))
					return (ACPI_EXC);
			}

			/* accept action */
			if (p_flags & P_ACCEPT)
				return (rap ? (*rap)(r_flags, pstp) : ACPI_OK);

			/* reduce action */
			if (rap && (*rap)(r_flags, pstp) == ACPI_EXC)
				return (ACPI_EXC);
			continue;
		} /* reduce */

		/*
		 * determine if there is a next element for a list
		 */
		if (p_flags & P_LCHOICE) {
			/* find relevant rule */
			lrp = (list_rule_t *)(gp->parse_table_p)[p_elem];

			/* lookahead for EOF */
			if (LEX(CTX_PRI, 0) == gp->eof) {
				/* set list end */
				PARSE_PTR->flags = P_REDUCE;
				/* push parse eof */
				PARSE_PUSH(pp);
				pp->elem = gp->eof;
				pp->flags = P_PARSE;
				continue;
			}

			/* set reduce list element */
			PARSE_PTR->flags = P_LADD;
			/* push parse element */
			PARSE_PUSH(pp);
			pp->elem = lrp->elem;
			pp->flags = P_PARSE;
			continue;
		}

		/*
		 * append list element
		 */
		if (p_flags & P_LADD) {	/* no custom rule is possible here */
			/* set list choice */
			PARSE_PTR->flags = P_LCHOICE;
			/* wrap top value in node */
			vp = VALUE_PTR;
			if ((np = node_new(vp->elem)) == NULL)
				return (ACPI_EXC);
			np->data = vp->data;
			/* add it to list chain */
			VALUE_POP;
			np0 = VALUE_PTR->data;
			if (np0) {
				if (node_add_sibling(np0, np))
					return (ACPI_EXC);
			} else
				VALUE_PTR->data = np;
			continue;
		}

		return (exc_code(ACPI_EINTERNAL)); /* unknown parse flags */
	} /* for */
}


/* eof */
