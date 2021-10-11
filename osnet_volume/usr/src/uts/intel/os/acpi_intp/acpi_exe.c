/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_exe.c	1.1	99/05/21 SMI"


/* reduce functions for execution */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>

#include "acpi_exc.h"
#include "acpi_node.h"
#include "acpi_stk.h"
#include "acpi_bst.h"
#include "acpi_par.h"

#include "acpi_elem.h"
#include "acpi_act.h"
#include "acpi_name.h"
#include "acpi_ns.h"
#include "acpi_val.h"
#include "acpi_thr.h"
#include "acpi_io.h"


/*
 * special reduce actions
 *
 * LHS is VALUE_PTR (or VALUE_PTR_N(0))
 * RHS[i] is VALUE_PTR_N(i), so first RHS is VALUE_PTR_N(1), etc.
 */

/*ARGSUSED*/
int
debug_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;

	ret->elem = V_DEBUG_OBJ;
	ret->data = debug_obj_new();
	return (ACPI_OK);
}


/*
 * locals
 */

/*ARGSUSED*/
static int
local_reduce(parse_state_t *pstp, int local_num)
{
	static int local_elems[] = {
		V_LOCAL0, V_LOCAL1, V_LOCAL2, V_LOCAL3, V_LOCAL4, V_LOCAL5,
		V_LOCAL6, V_LOCAL7,
	};
	value_entry_t *ret = VALUE_PTR;

	if ((pstp->pctx & PCTX_EXECUTE) == 0)
		return (ACPI_EXC);
	ret->elem = local_elems[local_num];
	return (ACPI_OK);
}

/*ARGSUSED*/
int
local0_reduce(int rflags, parse_state_t *pstp)
{
	return (local_reduce(pstp, 0));
}

/*ARGSUSED*/
int
local1_reduce(int rflags, parse_state_t *pstp)
{
	return (local_reduce(pstp, 1));
}

/*ARGSUSED*/
int
local2_reduce(int rflags, parse_state_t *pstp)
{
	return (local_reduce(pstp, 2));
}

/*ARGSUSED*/
int
local3_reduce(int rflags, parse_state_t *pstp)
{
	return (local_reduce(pstp, 3));
}

/*ARGSUSED*/
int
local4_reduce(int rflags, parse_state_t *pstp)
{
	return (local_reduce(pstp, 4));
}

/*ARGSUSED*/
int
local5_reduce(int rflags, parse_state_t *pstp)
{
	return (local_reduce(pstp, 5));
}

/*ARGSUSED*/
int
local6_reduce(int rflags, parse_state_t *pstp)
{
	return (local_reduce(pstp, 6));
}

/*ARGSUSED*/
int
local7_reduce(int rflags, parse_state_t *pstp)
{
	return (local_reduce(pstp, 7));
}


/*
 * arg objects
 */
static int
arg_reduce(parse_state_t *pstp, int argnum)
{
	static int arg_elems[] = {
		V_ARG0, V_ARG1, V_ARG2, V_ARG3, V_ARG4, V_ARG5, V_ARG6,
	};
	value_entry_t *ret = VALUE_PTR;
	exe_desc_t *edp = pstp->key;

	if ((pstp->pctx & PCTX_EXECUTE) &&
	    (edp->flags & ACPI_ARGS_MASK) >= argnum)
		ret->elem = arg_elems[argnum];
	else
		return (ACPI_EXC);
	return (ACPI_OK);
}


/*ARGSUSED*/
int
arg0_reduce(int rflags, parse_state_t *pstp)
{
	return (arg_reduce(pstp, 0));
}

/*ARGSUSED*/
int
arg1_reduce(int rflags, parse_state_t *pstp)
{
	return (arg_reduce(pstp, 1));
}

/*ARGSUSED*/
int
arg2_reduce(int rflags, parse_state_t *pstp)
{
	return (arg_reduce(pstp, 2));
}

/*ARGSUSED*/
int
arg3_reduce(int rflags, parse_state_t *pstp)
{
	return (arg_reduce(pstp, 3));
}

/*ARGSUSED*/
int
arg4_reduce(int rflags, parse_state_t *pstp)
{
	return (arg_reduce(pstp, 4));
}

/*ARGSUSED*/
int
arg5_reduce(int rflags, parse_state_t *pstp)
{
	return (arg_reduce(pstp, 5));
}

/*ARGSUSED*/
int
arg6_reduce(int rflags, parse_state_t *pstp)
{
	return (arg_reduce(pstp, 6));
}


/*
 * method calls
 */

static int
execute_setup(parse_state_t *pstp, int args)
{
	name_t *namep = NAME_VALP_N(1);
	ns_elem_t *method_nsp, *dynamic_nsp;
	exe_desc_t *edp;
	acpi_val_t *valp;
	acpi_method_t *methodp;
	bst *bp;
	parse_entry_t *pp;
	ns_entry_t *nsp;
	int i;

	/* lookup name */
	if (ns_lookup(NSP_ROOT, NSP_CUR, namep, KEY_IF_EXE, 0, &method_nsp,
	    NULL) != ACPI_OK)
		return (exc_code(ACPI_EUNDEF));
	/* XXX  do sync check here */
	/* push exe_desc */
	if ((edp = exe_desc_push(pstp->thread)) == NULL)
		return (ACPI_EXC);
	/* dynamic name space */
	if ((dynamic_nsp = ns_dynamic_copy(method_nsp, edp)) == NULL)
		return (ACPI_EXC);
	/* new bst */
	valp = dynamic_nsp->valp;
	methodp = valp->acpi_valp;
	if ((bp = bst_open(methodp->byte_code, methodp->length)) == NULL)
		return (ACPI_EXC);
	bp->sp = pstp->bp->sp;	/* borrow narrow stack of current parse */
	/* setup after all structs allocated */
	edp->ret = VALUE_PTR;	/* where the return value will end up */
	edp->dyn = dynamic_nsp;
	edp->flags = ((acpi_method_t *)(dynamic_nsp->valp->acpi_valp))->flags;
	/* save old parse state */
	edp->saved.key = pstp->key;
	edp->saved.bp = pstp->bp;
	edp->saved.bst_stack = stack_index(pstp->bp->sp);
	edp->saved.parse_stack = stack_index(pstp->psp);
	edp->saved.value_stack = stack_index(pstp->vsp);
	edp->saved.ns_stack = stack_index(pstp->nssp);
	edp->saved.pctx = pstp->pctx;
	/* save the args */
	for (i = 0; i < args; i++) {
		/* XXX maybe should do lazy eval? */
		if (acpi_load(VALUE_PTR_N(i + 2), pstp, &edp->args[i], 0) !=
		    ACPI_OK)
			return (ACPI_EXC);
		value_hold(edp->args[i]);
	}
	/* fix parse stacks */
	PARSE_PUSH(pp);
	pp->elem = N_METHOD_BODY;
	pp->flags = P_PARSE;
	VALUE_POP;
	/* adjust context */
	pstp->key = edp;
	pstp->bp = bp;
	if (bst_push(pstp->bp, bst_length(pstp->bp)))
		return (ACPI_EXC);
	NS_PUSH(nsp);
	nsp->elem = dynamic_nsp;
	pstp->pctx |= PCTX_EXECUTE;
	/* go */
	return (ACPI_OK);
}

/*ARGSUSED*/
int
method_call0_reduce(int rflags, parse_state_t *pstp)
{
	return (execute_setup(pstp, 0));
}

/*ARGSUSED*/
int
method_call1_reduce(int rflags, parse_state_t *pstp)
{
	return (execute_setup(pstp, 1));
}

/*ARGSUSED*/
int
method_call2_reduce(int rflags, parse_state_t *pstp)
{
	return (execute_setup(pstp, 2));
}

/*ARGSUSED*/
int
method_call3_reduce(int rflags, parse_state_t *pstp)
{
	return (execute_setup(pstp, 3));
}

/*ARGSUSED*/
int
method_call4_reduce(int rflags, parse_state_t *pstp)
{
	return (execute_setup(pstp, 4));
}

/*ARGSUSED*/
int
method_call5_reduce(int rflags, parse_state_t *pstp)
{
	return (execute_setup(pstp, 5));
}

/*ARGSUSED*/
int
method_call6_reduce(int rflags, parse_state_t *pstp)
{
	return (execute_setup(pstp, 6));

}

/*ARGSUSED*/
int
method_call7_reduce(int rflags, parse_state_t *pstp)
{
	return (execute_setup(pstp, 7));
}

/*
 * we get here by finishing an inline method call or by return
 *
 * in the case of return, we still might have gotten here by eval
 * call which has its own cleanup
 */
/*ARGSUSED*/
int
method_body_reduce(int rflags, parse_state_t *pstp)
{
	exe_desc_t *edp;

	/* return value (if any) should already be set */
	edp = (exe_desc_t *)(pstp->key);

	/* if there was no return value, put in a default just in case */
	if ((edp->flags & ACPI_IRET) == 0) {
		edp->ret->data = uninit_new();
		edp->ret->elem = value_elem(edp->ret->data);
	}

	/* if we used a separate bst, free it up */
	if (pstp->bp != edp->saved.bp) {
		pstp->bp->sp = NULL;	/* disconnect bst stack */
		bst_close(pstp->bp);
	}
	/* restore parse context */
	pstp->key = edp->saved.key;
	pstp->bp = edp->saved.bp;
	if (stack_seek(pstp->bp->sp, edp->saved.bst_stack) == ACPI_EXC)
		return (ACPI_EXC);
	if (stack_seek(pstp->psp, edp->saved.parse_stack) == ACPI_EXC)
		return (ACPI_EXC);
	if (stack_seek(pstp->vsp, edp->saved.value_stack) == ACPI_EXC)
		return (ACPI_EXC);
	if (stack_seek(pstp->nssp, edp->saved.ns_stack) == ACPI_EXC)
		return (ACPI_EXC);
	pstp->pctx = edp->saved.pctx;

	if ((edp->flags & ACPI_IEVAL) == 0) { /* eval does elsewhere */
		/* XXX release sync */
		/* pop exe_desc */
		exe_desc_pop(edp->thread);
	}
	return (ACPI_OK);
}


/* eof */
