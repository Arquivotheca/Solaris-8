/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_op1.c	1.1	99/05/21 SMI"


/* reduce functions for type 1 operators */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>

#include "acpi_exc.h"
#include "acpi_bst.h"
#include "acpi_node.h"
#include "acpi_stk.h"
#include "acpi_par.h"

#include "acpi_elem.h"
#include "acpi_act.h"
#include "acpi_name.h"
#include "acpi_ns.h"
#include "acpi_val.h"
#include "acpi_thr.h"
#include "acpi_io.h"
#include "acpi_tab.h"
#include "acpi_inf.h"

#ifdef ACPI_BOOT
extern void wait100ms(void);
#endif

/*
 * reduce actions
 *
 * LHS is VALUE_PTR (or VALUE_PTR_N(0))
 * RHS[i] is VALUE_PTR_N(i), so first RHS is VALUE_PTR_N(1), etc.
 */

/*ARGSUSED*/
int
break_reduce(int rflags, parse_state_t *pstp)
{
	return (skip_lex(pstp));
}

/*ARGSUSED*/
int
breakpoint_reduce(int rflags, parse_state_t *pstp)
{
	return (ACPI_OK);
}

/*ARGSUSED*/
int
fatal_reduce(int rflags, parse_state_t *pstp)
{
	acpi_val_t *type = ACPI_VALP_N(2);
	acpi_val_t *code = ACPI_VALP_N(3);
	acpi_val_t *arg;

	if (acpi_load(VALUE_PTR_N(4), pstp, &arg, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (type->type != ACPI_INTEGER || code->type != ACPI_INTEGER ||
	    arg->type != ACPI_INTEGER)
		return (exc_code(ACPI_ETYPE));
	fatal_info.type = type->acpi_ival;
	fatal_info.code = code->acpi_ival;
	fatal_info.arg = arg->acpi_ival;
	value_free(type);
	value_free(code);
	value_free(arg);
	return (exc_code(ACPI_EFATAL));
}

int
return_reduce(int rflags, parse_state_t *pstp)
{
	acpi_val_t *retval;
	exe_desc_t *edp = (exe_desc_t *)(pstp->key);

	if ((pstp->pctx & PCTX_EXECUTE) == 0) /* only allowed in a method */
		return (ACPI_EXC);
	/* get return value */
	if (acpi_load(VALUE_PTR_N(2), pstp, &retval, 0) != ACPI_OK)
		return (ACPI_EXC);
	edp->ret->data = retval;
	edp->ret->elem = value_elem(retval);
	edp->flags |= ACPI_IRET; /* signal return value */
	/* finish off method */
	return (method_body_reduce(rflags, pstp));
}

/*ARGSUSED*/
int
sleep_reduce(int rflags, parse_state_t *pstp)
{
	acpi_val_t *msec;

	if (acpi_load(VALUE_PTR_N(2), pstp, &msec, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (msec->type != ACPI_INTEGER)
		return (exc_code(ACPI_ETYPE));
	acpi_delay_sig(msec->acpi_ival);
	value_free(msec);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
stall_reduce(int rflags, parse_state_t *pstp)
{
	acpi_val_t *usec;

	if (acpi_load(VALUE_PTR_N(2), pstp, &usec, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (usec->type != ACPI_INTEGER)
		return (exc_code(ACPI_ETYPE));
	if (usec->acpi_ival > 100) /* should not be higher than 100 */
		return (exc_code(ACPI_ERANGE));
	/* these routines do not really have the correct granularity */
	if (usec->acpi_ival > 0) {
#ifdef ACPI_BOOT
		wait100ms();
#endif
#ifdef ACPI_KERNEL
		delay((clock_t)1);
#endif
	}
	value_free(usec);
	return (ACPI_OK);
}

/*
 * if the while predicate was true, the rule will be unaltered (N_TERM_LIST)
 * if false, an N_SKIP will be on the stack
 */
/*ARGSUSED*/
int
while_reduce(int rflags, parse_state_t *pstp)
{
	parse_entry_t *pp;
	value_entry_t *op = VALUE_PTR_N(1);
	value_entry_t *clause = VALUE_PTR_N(5);

	if (clause->elem == N_SKIP) /* false */
		return (ACPI_OK); /* let the parser pop it all off */
	/* true, so put the N_WHILE rule back on the parse stack */
	PARSE_PUSH(pp);
	pp->elem = N_WHILE;
	pp->flags = P_PARSE;
	/* reset byte stream back to begining of while */
	if (bst_seek(pstp->bp, (int)op->data, 0) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	return (ACPI_OK);
}

/*ARGSUSED*/
int
load_reduce(int rflags, parse_state_t *pstp)
{
	ns_elem_t *nsp;
	acpi_val_t *memory, *handle;
	acpi_field_t *fieldp;
	acpi_region_t *regionp;
	acpi_header_t *tablep;
	ddb_desc_t *key;
	unsigned int offset, length, lset;

	if (ns_lookup(NSP_ROOT, NSP_CUR, NAME_VALP_N(2), KEY_IF_EXE, 0,
	    &nsp, NULL) != ACPI_OK)
		return (exc_code(ACPI_EUNDEF));
	if (acpi_load(VALUE_PTR_N(3), pstp, &handle, 0) != ACPI_OK)
		return (ACPI_EXC);

	/* sort through memory to find offset and length */
	memory = nsp->valp;
	offset = length = lset = 0;
	if (memory->type == ACPI_FIELD) { /* a field? */
		fieldp = memory->acpi_valp;
		if ((fieldp->flags & ACPI_FIELD_TYPE_MASK) !=
		    ACPI_REGULAR_TYPE)
			return (exc_code(ACPI_ETYPE));
		offset = fieldp->offset >> 3; /* non-byte sizes possible? */
		length = fieldp->length >> 3; /* non-byte sizes possible? */
		lset = 1;
		memory = fieldp->src.field.region;
	}
	if (memory->type != ACPI_REGION) /* region? */
		return (exc_code(ACPI_ETYPE));
	regionp = memory->acpi_valp;
	if (regionp->space != ACPI_MEMORY)
		return (exc_code(ACPI_ETYPE));
	if (lset == 0)
		length = regionp->length;
	else if (length + offset > regionp->length)
		return (exc_code(ACPI_EEOF));
	if (length < sizeof (acpi_header_t))
		return (exc_code(ACPI_EEOF));
	offset += regionp->mapping;
	tablep = (acpi_header_t *)offset;

	/* is table okay? */
	/* XXX maybe check for identical table of higher rev */
	if (table_check(tablep,
	    tablep->signature.iseg == ACPI_SSDT ? ACPI_SSDT : ACPI_PSDT, 0)
	    != ACPI_OK)
		return (ACPI_EXC);
	if ((key = table_add(tablep, handle)) == NULL)
		return (ACPI_EXC);

	if (parse_driver(pstp->thread, key,
	    (char *)(tablep) + sizeof (acpi_header_t),
	    length - sizeof (acpi_header_t),
	    NSP_CUR, N_START, 0, NULL, NULL, 256) == ACPI_EXC)
		return (ACPI_EXC);

	/* do not free handle, since ddb_desc has ref */
	return (ACPI_OK);
}

/*ARGSUSED*/
int
notify_reduce(int rflags, parse_state_t *pstp)
{
	acpi_val_t *notify, *value;
	acpi_cb_t *cbp;

	if (acpi_load(VALUE_PTR_N(2), pstp, &notify, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (acpi_load(VALUE_PTR_N(3), pstp, &value, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (notify->type != ACPI_DEVICE && notify->type != ACPI_THERMAL_ZONE)
		return (exc_code(ACPI_ETYPE));
	if (value->type != ACPI_INTEGER)
		return (exc_code(ACPI_ETYPE));

	for (cbp = notify->acpi_valp; cbp; cbp = cbp->next)
		if (cbp->fn != NULL)
		    if ((*cbp->fn)(cbp->obj, cbp->cookie, value->acpi_ival) ==
			ACPI_EXC)
			    break;

	value_free(notify);
	value_free(value);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
release_reduce(int rflags, parse_state_t *pstp)
{
	acpi_val_t *sync, *ptr, **trail;
	acpi_mutex_t *mutexp;
	acpi_thread_t *threadp;
	int i;

	if (acpi_load(VALUE_PTR_N(2), pstp, &sync, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (sync->type != ACPI_MUTEX)
		return (exc_code(ACPI_ETYPE));
	mutexp = sync->acpi_valp;
	threadp = pstp->thread;

	if (mutexp->owner != threadp)
		return (exc_code(ACPI_ESYNC));

	/* remove mutex from list */
	for (ptr = threadp->mutex_list, trail = &threadp->mutex_list; ptr; ) {
		if (ptr->type != ACPI_MUTEX)
			return (exc_code(ACPI_EINTERNAL));
		mutexp = ptr->acpi_valp;
		if (ptr == sync) {
			*trail = mutexp->next; /* splice out */
			mutexp->owner = NULL;
			mutexp->next = NULL;
			break;
		}
		ptr = mutexp->next;
		trail = &mutexp->next;
	}

	/* find highest remaining sync level */
	for (ptr = threadp->mutex_list, i = 0; ptr; ) {
		mutexp = ptr->acpi_valp;
		if (mutexp->sync > i)
			i = mutexp->sync;
		ptr = mutexp->next;
	}
	threadp->sync = (unsigned char)i;

	value_free(sync);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
reset_reduce(int rflags, parse_state_t *pstp)
{
	acpi_val_t *event;

	if (acpi_load(VALUE_PTR_N(2), pstp, &event, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (event->type != ACPI_EVENT)
		return (exc_code(ACPI_ETYPE));
	event->acpi_ival = 0;
	value_free(event);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
signal_reduce(int rflags, parse_state_t *pstp)
{
	acpi_val_t *event;

	if (acpi_load(VALUE_PTR_N(2), pstp, &event, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (event->type != ACPI_EVENT)
		return (exc_code(ACPI_ETYPE));
	event->acpi_ival += 1;
	value_free(event);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
unload_reduce(int rflags, parse_state_t *pstp)
{
	acpi_val_t *handle;
	ddb_desc_t *key;
	void *skey;

	/* temporarily use zero key to allow matches anywhere */
	skey = pstp->key;
	pstp->key = 0;
	if (acpi_load(VALUE_PTR_N(2), pstp, &handle, _NO_EVAL) != ACPI_OK) {
		pstp->key = skey;
		return (ACPI_EXC);
	}
	pstp->key = skey;
	if ((key = table_remove(handle)) == NULL)
		return (ACPI_EXC);
	ns_undefine_block(NSP_ROOT, key);
	value_free(handle);
	return (ACPI_OK);
}


/* eof */
