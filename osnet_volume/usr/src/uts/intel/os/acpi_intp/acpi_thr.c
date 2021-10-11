/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_thr.c	1.1	99/05/21 SMI"


/* threads and parser driver */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>

#ifdef ACPI_BOOT
#include <sys/salib.h>
#endif

#ifdef ACPI_USER
#include <stdlib.h>
#include <strings.h>
#endif

#include "acpi_exc.h"
#include "acpi_bst.h"
#include "acpi_node.h"
#include "acpi_stk.h"
#include "acpi_par.h"

#include "acpi_lex.h"
#include "acpi_elem.h"
#include "acpi_act.h"
#include "acpi_name.h"
#include "acpi_ns.h"
#include "acpi_val.h"
#include "acpi_thr.h"


gram_t gram_info = {
	&parse_table[0], lex, MAX_PARSE_ENTRY, MAX_TERM, T_EMPTY, T_EOF
};

acpi_thread_t *acpi_threads;
acpi_thread_t *current_acpi_thread;

/*LINTLIBRARY*/
acpi_thread_t *
acpi_thread_new(struct ddb_desc *ddp, struct exe_desc *edp)
{
	acpi_thread_t *new;

	if ((new = kmem_alloc(sizeof (exe_desc_t), KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	new->next = NULL;
	new->ddp = ddp;
	new->edp = edp;
	new->mutex_list = NULL;
	new->sync = 0;
	return (new);
}

static exe_desc_t *
exe_desc_new(acpi_thread_t *threadp)
{
	exe_desc_t *new;
	int i;

	if ((new = kmem_alloc(sizeof (exe_desc_t), KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	bzero(new, sizeof (exe_desc_t));
	new->type = KEY_EXE;
	new->thread = threadp;
	for (i = 0; i < 8; i++)
		new->locals[i] = uninit_new();
	return (new);
}

static void
exe_desc_free(exe_desc_t *edp)
{
	int i;
	ns_elem_t *dyn, *next;

	if (edp == NULL)
		return;
	/* free args */
	for (i = 0; i < (edp->flags & ACPI_ARGS_MASK); i++)
		value_free(edp->args[i]);
	/* free locals */
	for (i = 0; i < 8; i++)
		value_free(edp->locals[i]);
	/* free dynamic stuff, dynamic method is last on the list */
	for (dyn = edp->dyn; dyn; dyn = next) {
		next = dyn->dyn;
		ns_undefine(dyn);
	}
	kmem_free(edp, sizeof (exe_desc_t));
}

exe_desc_t *
exe_desc_push(acpi_thread_t *threadp)
{
	exe_desc_t *new;

	if ((new = exe_desc_new(threadp)) == NULL)
		return (NULL);
	new->next = threadp->edp; /* push on exe_desc */
	threadp->edp = new;
	return (new);
}

void
exe_desc_pop(acpi_thread_t *threadp)
{
	exe_desc_t *edp;

	if (threadp == NULL || threadp->edp == NULL)
		return;
	edp = threadp->edp;
	threadp->edp = edp->next;
	exe_desc_free(edp);
}


/*
 * args:
 *	key: ddb or exe
 *	buf, length: for bst construction
 *	initial_ns: initial name space
 *	initial_symbol, initial_pctx: initial parse state
 *	ret_vep: for edp initialization
 *	retval: return acpi_value
 *	stack_size: size of internal stacks
 */
int
parse_driver(acpi_thread_t *threadp, void *key, char *buf, int length,
    ns_elem_t *initial_ns, int initial_symbol, int initial_pctx,
    value_entry_t **ret_vep, acpi_val_t **retval, int stack_size)
{
	parse_state_t parse_info;
	parse_state_t *pstp = &parse_info;
	parse_entry_t *pp;
	value_entry_t *vp, *ret;
	ns_entry_t *nsp;
	exe_desc_t *edp;

	/* setup input */
	if ((pstp->bp = bst_open(buf, length)) == NULL)
		return (ACPI_EXC);
	if (bst_stack(pstp->bp, stack_size)) /* add narrow stack to bp */
		return (ACPI_EXC);
	/* terminating EOF for top-level list */
	if (bst_push(pstp->bp, bst_length(pstp->bp)))
		return (ACPI_EXC);
	/* create parse stack */
	if ((pstp->psp = stack_new(sizeof (parse_entry_t), stack_size)) ==
	    NULL)
		return (ACPI_EXC);
	/* create value stack */
	if ((pstp->vsp = stack_new(sizeof (value_entry_t), stack_size)) ==
	    NULL)
		return (ACPI_EXC);
	/* create ns stack */
	if ((pstp->nssp = stack_new(sizeof (ns_entry_t), stack_size)) ==
	    NULL)
		return (ACPI_EXC);

	pstp->thread = threadp;
	pstp->key = key;
	pstp->ns = root_ns;

	/* initial parse stack */
	pp = PARSE_PTR;
	pp->elem = N_START; /* accept symbol */
	pp->flags = P_ACCEPT;

	/* initial value stack */
	vp = VALUE_PTR;
	vp->elem = N_START; /* accept symbol */
	vp->data = 0;
	VALUE_PUSH(vp);
	ret = vp;		/* where the return value_entry will be */

	/* initial name space */
	nsp = (ns_entry_t *)stack_top(pstp->nssp);
	nsp->elem = initial_ns;

	/* initial parse context */
	pstp->pctx = initial_pctx;

	if (initial_pctx & PCTX_EXECUTE) {
		edp = key;
		edp->saved.key = pstp->key;
		edp->saved.bp = pstp->bp;
		edp->saved.bst_stack = stack_index(pstp->bp->sp);
		edp->saved.parse_stack = stack_index(pstp->psp);
		edp->saved.value_stack = stack_index(pstp->vsp);
		edp->saved.ns_stack = stack_index(pstp->nssp);
		edp->saved.pctx = pstp->pctx;
	}

	/* add initial parse */
	if ((pp = (parse_entry_t *)stack_push(pstp->psp)) == NULL)
		return (exc_warn("can't push start symbol")); /* possible? */
	pp->elem = initial_symbol; /* initial symbol */
	pp->flags = P_PARSE;

	/* more value stack */
	VALUE_POP;
	if (ret_vep)
		*ret_vep = ret;

	if (parse(&gram_info, &parse_info))
		return (ACPI_EXC);

	if (retval)
		*retval = ret->data; /* get the return value out */

	/* free structs */
	bst_close(pstp->bp);
	stack_free(pstp->psp);
	stack_free(pstp->vsp);
	stack_free(pstp->nssp);

	return (ACPI_OK);
}

int
eval_driver(acpi_thread_t *threadp, ns_elem_t *method_nsp, acpi_val_t *args,
    acpi_val_t **retpp, int stack_size)
{
	exe_desc_t *edp;
	ns_elem_t *dynamic_nsp;
	acpi_method_t *methodp;
	acpi_val_t *avp;
	int i, argc;

	/* XXX do sync check here */
	/* push exe_desc */
	if ((edp = exe_desc_push(threadp)) == NULL)
		return (ACPI_EXC);
	/* dynamic name space */
	if ((dynamic_nsp = ns_dynamic_copy(method_nsp, edp)) == NULL)
		return (ACPI_EXC);
	methodp = (acpi_method_t *)(dynamic_nsp->valp->acpi_valp);

	/* setup edp */
	/* edp->ret done by parse_driver */
	edp->dyn = dynamic_nsp;
	edp->flags = ((acpi_method_t *)(dynamic_nsp->valp->acpi_valp))->flags;
	edp->flags |= ACPI_IEVAL;
	/* copy args */
	argc = edp->flags & ACPI_ARGS_MASK;
	if (args) {
		if (args->type != ACPI_PACKAGE || args->length != argc)
			return (ACPI_EXC);
	} else if (argc != 0)
		return (ACPI_EXC);
	for (i = 0; i < argc; i++) {
		avp = ACPI_PKG_N(args, i);
		value_hold(avp);
		value_free(edp->args[i]);
		edp->args[i] = avp;
	}

	if (parse_driver(threadp, edp, methodp->byte_code, methodp->length,
	    dynamic_nsp, N_METHOD_BODY, PCTX_EXECUTE, &edp->ret, retpp,
	    stack_size) == ACPI_EXC)
		return (ACPI_EXC);

	/* XXX release sync */
	/* pop exe_desc */
	exe_desc_pop(edp->thread);
	return (ACPI_OK);
}     


/* eof */
