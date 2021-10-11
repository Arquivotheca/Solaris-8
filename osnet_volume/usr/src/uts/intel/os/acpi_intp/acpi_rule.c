/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_rule.c	1.1	99/05/21 SMI"


/* lex rule functions */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>
#include <sys/acpi_prv.h>

#ifdef ACPI_BOOT
#include <sys/salib.h>
#endif

#ifdef ACPI_USER
#include <stdlib.h>
#endif

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


/*
 * error codes should be set on exception
 */

/* empty token, usually just used as a stack place holder */
/*ARGSUSED*/
int
empty_lex(parse_state_t *pstp)
{
	return (ACPI_OK);
}

/* package length */
int
pkg_length_lex(parse_state_t *pstp)
{
	value_entry_t *ret;
	int byte, follow, value;

	if ((byte = bst_get(pstp->bp)) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	follow = (byte & 0xC0) >> 6;

	if (follow == 0) {
		value = byte & 0x3F;
		exc_debug(ACPI_DLEX, "package length 0x%x (%d)", value, value);
		VALUE_PUSH(ret);
		ret->elem = T_PKG_LENGTH;
		ret->data = (void *)value;
		return (VALRET);
	}

	value = byte & 0xF;
	if ((byte = bst_get(pstp->bp)) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	value |= byte << 4;

	if (follow == 1) {
		exc_debug(ACPI_DLEX, "package length 0x%x (%d)", value, value);
		VALUE_PUSH(ret);
		ret->elem = T_PKG_LENGTH;
		ret->data = (void *)value;
		return (VALRET);
	}

	if ((byte = bst_get(pstp->bp)) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	value |= byte << 12;

	if (follow == 2) {
		exc_debug(ACPI_DLEX, "package length 0x%x (%d)", value, value);
		VALUE_PUSH(ret);
		ret->elem = T_PKG_LENGTH;
		ret->data = (void *)value;
		return (VALRET);
	}

	if ((byte = bst_get(pstp->bp)) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	value |= byte << 20;

	exc_debug(ACPI_DLEX, "package length 0x%x (%d)", value, value);
	VALUE_PUSH(ret);		/* follow == 3 */
	ret->elem = T_PKG_LENGTH;
	ret->data = (void *)value;
	return (VALRET);
}

/* length delimited parse scope, causes a narrowing push */
int
pkg_narrow_lex(parse_state_t *pstp)
{
	int save;

	save = bst_index(pstp->bp);
	if (pkg_length_lex(pstp) == ACPI_EXC)
		return (ACPI_EXC);	/* code set in pkg_length_lex */
	VALUE_PTR->elem = T_PKG_NARROW;
	if (bst_push(pstp->bp, save + (int)(VALUE_PTR->data)))
		return (ACPI_EXC);	/* code set in push */
	return (VALRET);
}

/* pop parse scope */
int
eof_lex(parse_state_t *pstp)
{
	if (bst_peek(pstp->bp) != ACPI_EXC || bst_pop(pstp->bp))
		return (exc_code(ACPI_EPARSE));
	return (ACPI_OK);
}


/*
 * special values
 */
static int
special_value_lex(parse_state_t *pstp, int limit, int elem, char *msg)
{
	value_entry_t *ret;
	int value;

	if ((value = bst_get(pstp->bp)) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	if (value > limit)
		return (exc_code(ACPI_ERANGE));
	VALUE_PUSH(ret);
	ret->elem = elem;
	ret->data = (void *)value;
	exc_debug(ACPI_DLEX, "%s  0x%x (%d)", msg, value, value);
	return (VALRET);
}



/* method flags  */
int
method_flags_lex(parse_state_t *pstp)
{
	return (special_value_lex(pstp, 15, T_METHOD_FLAGS,
	    "method flags value"));
}

/* power resource system level */
int
system_level_lex(parse_state_t *pstp)
{
	return (special_value_lex(pstp, 5, T_SYSTEM_LEVEL,
	    "system level value"));
}

/* processor block length */
int
pblock_length_lex(parse_state_t *pstp)
{
	value_entry_t *ret;
	int value;

	if ((value = bst_get(pstp->bp)) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	if (value != 0 && value != 6) /* allowable pblock lengths */
		return (exc_code(ACPI_ERANGE));
	VALUE_PUSH(ret);
	ret->elem = T_PBLOCK_LENGTH;
	ret->data = (void *)value;
	exc_debug(ACPI_DLEX, "processor block length  0x%x (%d)", value,
	    value);
	return (VALRET);
}

/* field flags */
int
field_flags_lex(parse_state_t *pstp)
{
	return (special_value_lex(pstp, 127, T_FIELD_FLAGS, "field flags"));
}

/* sync level */
int
sync_level_lex(parse_state_t *pstp)
{
	return (special_value_lex(pstp, 15, T_SYNC_LEVEL, "sync level"));
}

/* region space */
int
region_space_lex(parse_state_t *pstp)
{
	value_entry_t *ret;
	int value;

	if ((value = bst_get(pstp->bp)) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	if (value > 0x04 && value < 0x80)
		return (exc_code(ACPI_ERANGE));
	VALUE_PUSH(ret);
	ret->elem = T_REGION_SPACE;
	ret->data = (void *)value;
	exc_debug(ACPI_DLEX, "region space  0x%x (%d)", value, value);
	return (VALRET);
}

/* match relation operator */
int
match_rel_lex(parse_state_t *pstp)
{
	return (special_value_lex(pstp, 5, T_MATCH_REL,
	    "match operator value"));
}


/*
 * ACPI (unsigned) numbers
 */
int
byte_data_lex(parse_state_t *pstp)
{
	value_entry_t *ret;
	int value;

	if ((value = bst_get(pstp->bp)) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	VALUE_PUSH(ret);
	ret->elem = V_ACPI_VALUE;
	if ((ret->data = integer_new(value)) == NULL)
		return (ACPI_EXC);
	exc_debug(ACPI_DLEX, "byte value 0x%x (%d)", value, value);
	return (VALRET);
}

int
word_data_lex(parse_state_t *pstp)
{
	value_entry_t *ret;
	int value, byte;

	if ((value = bst_get(pstp->bp)) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	if ((byte = bst_get(pstp->bp)) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	value |= byte << 8;
	VALUE_PUSH(ret);
	ret->elem = V_ACPI_VALUE;
	if ((ret->data = integer_new(value)) == NULL)
		return (ACPI_EXC);
	exc_debug(ACPI_DLEX, "word value 0x%x (%d)", value, value);
	return (VALRET);
}

int
dword_data_lex(parse_state_t *pstp)
{
	value_entry_t *ret;
	int value, byte;

	if ((value = bst_get(pstp->bp)) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	if ((byte = bst_get(pstp->bp)) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	value |= byte << 8;
	if ((byte = bst_get(pstp->bp)) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	value |= byte << 16;
	if ((byte = bst_get(pstp->bp)) == ACPI_EXC)
		return (exc_code(ACPI_EPARSE));
	value |= byte << 24;
	VALUE_PUSH(ret);
	ret->elem = V_ACPI_VALUE;
	if ((ret->data = integer_new(value)) == NULL)
		return (ACPI_EXC);
	exc_debug(ACPI_DLEX, "dword value  0x%x (%d)", value, value);
	return (VALRET);
}


/*
 * name_seg_lex, name_string_lex in name.c
 * but name_string_reduce below
 */

/*
 * ranges of bytes
 */

/* ACPI NULL-terminated string */
int
string_lex(parse_state_t *pstp)
{
	value_entry_t *ret;
	char *buffer;
	int size;

	size = bst_strlen(pstp->bp);
				/* extra for NULL */
	if ((buffer = kmem_alloc(size + 1, KM_SLEEP)) == NULL)
		return (exc_code(ACPI_ERES));
	if (bst_buffer(pstp->bp, buffer, size + 1))
		return (exc_code(ACPI_EPARSE));
	VALUE_PUSH(ret);
	ret->elem = V_ACPI_VALUE;
	if ((ret->data = string_new(buffer)) == NULL)
		return (ACPI_EXC);
	exc_debug(ACPI_DLEX, "string value %s", buffer);
	return (VALRET);
}

/* skip to end of narrow scope */
int
skip_lex(parse_state_t *pstp)
{
	int size;

	size = bst_length(pstp->bp) - bst_index(pstp->bp);
	return ((bst_seek(pstp->bp, size, 1) == ACPI_EXC) ?
	    ACPI_EXC : ACPI_OK);
}

/* rest of narrow scope is a byte list */
int
byte_list_lex(parse_state_t *pstp)
{
	value_entry_t *ret;
	char *buffer;
	int size;

	size = bst_length(pstp->bp) - bst_index(pstp->bp);
	if ((buffer = kmem_alloc(RND_UP4(size), KM_SLEEP)) == NULL)
		return (exc_code(ACPI_ERES));
	if (bst_buffer(pstp->bp, buffer, size))
		return (exc_code(ACPI_EPARSE));
	VALUE_PUSH(ret);
	ret->elem = V_ACPI_VALUE; /* maybe further processed by N_BUFFER */
	if ((ret->data = buffer_new(buffer, size)) == NULL)
		return (ACPI_EXC);
	return (VALRET);
}


/*
 * reduce action
 * handle special name string cases, not necessary for most uses
 */
/*ARGSUSED*/
int
name_string_reduce(int rflags, parse_state_t *pstp)
{
	static int method_calls[8] = {
		N_METHOD_CALL0, N_METHOD_CALL1, N_METHOD_CALL2, N_METHOD_CALL3,
		N_METHOD_CALL4, N_METHOD_CALL5, N_METHOD_CALL6, N_METHOD_CALL7
	};
	parse_entry_t *pp;
	value_entry_t *vp = VALUE_PTR;
	value_entry_t lhs;
	ns_elem_t *nsp;
	char *string;
	int args, i;

	if (PARSE_PTR->elem == N_TERMARG || PARSE_PTR->elem == N_TERM) {
		/* possibly a method call */
		if (ns_lookup(NSP_ROOT, NSP_CUR, vp->data, KEY_IF_EXE, 0, &nsp,
		    NULL) != ACPI_OK)
			return (exc_code(ACPI_EUNDEF));
		if (nsp->valp->type != ACPI_METHOD)
			return (ACPI_OK);	/* something else */

		args = ((acpi_method_t *)(nsp->valp->acpi_valp))->flags & 0x7;
		if (args < 0 || args > 7) /* possible? */
			return (exc_code(ACPI_ERANGE));

		PARSE_PUSH(pp);		/* fix up parse stack */
		pp->elem = method_calls[args];
		pp->flags = P_REDUCE;
		for (i = 0; i < args; i++) {
			PARSE_PUSH(pp);
			pp->elem = N_TERMARG;
			pp->flags = P_PARSE;
		}

		lhs = *vp; 	/* fix up value stack */
		vp->elem = method_calls[args];
		vp->data = NULL;
		VALUE_PUSH(vp);
		*vp = lhs;
		/* XXX maybe should cache the name lookup? */

	} else if (PARSE_PTR->elem == N_PDATUM) { /* package element */
		/* convert to string */
		if ((string = kmem_alloc(name_strlen(vp->data) + 1,
		    KM_SLEEP)) == NULL)
			return (exc_code(ACPI_ERES));
		(void) name_sprint(vp->data, string);
		name_free(vp->data);
		vp->elem = V_ACPI_VALUE;
		if ((vp->data = string_new(string)) == NULL)
			return (ACPI_EXC);
		exc_debug(ACPI_DREDUCE, " converted to string %s", string);
	}
	return (ACPI_OK);
}


/*
 * mid-rule reduce actions
 *
 * nothing above stack top
 * element parsed just before is VALUE_PTR (or VALUE_PTR_N(0))
 * previous elements are VALUE_PTR_N(-1), VALUE_PTR_N(-2), etc.
 *
 * most generally follow the same structure as device_scope action
 * generally, we rely on reduce actions to free memory
 */

/*ARGSUSED*/
int
device_scope(int rflags, parse_state_t *pstp)
{
	int offset = INT_VAL_N(-3);
	name_t *namep = NAME_VALP_N(-1);
	ns_entry_t *scopep;
	ns_elem_t *nsp;

	exc_debug(ACPI_DLEX, "push device scope %s", name_strbuf(namep));
	if ((nsp = ns_define(NSP_ROOT, NSP_CUR, namep, KEY_IF_EXE, pstp->key,
	    offset)) == NULL)
		return (exc_code(ACPI_EALREADY));
	if ((nsp->valp = device_new()) == NULL)
		return (ACPI_EXC);
	DYNAMIC_IF_EXE(nsp);	/* if executing, add to dynamic list */
	NS_PUSH(scopep);	/* push name device name scope */
	scopep->elem = nsp;
	return (ACPI_OK);
}

/*ARGSUSED*/
int
power_scope(int rflags, parse_state_t *pstp)
{
	int offset = INT_VAL_N(-5);
	name_t *namep = NAME_VALP_N(-3);
	unsigned char system_level = INT_VAL_N(-2);
	unsigned short resource_order = ACPI_INT_N(-1);
	ns_entry_t *scopep;
	ns_elem_t *nsp;

	exc_debug(ACPI_DLEX, "push power scope %s", name_strbuf(namep));
	if ((nsp = ns_define(NSP_ROOT, NSP_CUR, namep, KEY_IF_EXE, pstp->key,
	    offset)) == NULL)
		return (exc_code(ACPI_EALREADY));
	if ((nsp->valp = powerres_new(system_level, resource_order)) == NULL)
		return (ACPI_EXC);
	DYNAMIC_IF_EXE(nsp);
	NS_PUSH(scopep);
	scopep->elem = nsp;
	return (ACPI_OK);
}

/*ARGSUSED*/
int
proc_scope(int rflags, parse_state_t *pstp)
{
	int offset = INT_VAL_N(-6);
	name_t *namep = NAME_VALP_N(-4);
	unsigned char id = ACPI_INT_N(-3);
	unsigned int pbaddr = ACPI_INT_N(-2);
	unsigned char pblen = INT_VAL_N(-1);
	ns_entry_t *scopep;
	ns_elem_t *nsp;

	exc_debug(ACPI_DLEX, "push processor scope %s", name_strbuf(namep));
	if ((nsp = ns_define(NSP_ROOT, NSP_CUR, namep, KEY_IF_EXE, pstp->key,
	    offset)) == NULL)
		return (exc_code(ACPI_EALREADY));
	if ((nsp->valp = processor_new(id, pbaddr, pblen)) == NULL)
		return (ACPI_EXC);
	DYNAMIC_IF_EXE(nsp);
	NS_PUSH(scopep);
	scopep->elem = nsp;
	return (ACPI_OK);
}

/*ARGSUSED*/
int
thermal_scope(int rflags, parse_state_t *pstp)
{
	int offset = INT_VAL_N(-3);
	name_t *namep = NAME_VALP_N(-1);
	ns_entry_t *scopep;
	ns_elem_t *nsp;

	exc_debug(ACPI_DLEX, "push thermal zone scope %s", name_strbuf(namep));
	if ((nsp = ns_define(NSP_ROOT, NSP_CUR, namep, KEY_IF_EXE, pstp->key,
	    offset)) == NULL)
		return (exc_code(ACPI_EALREADY));
	if ((nsp->valp = thermal_new()) == NULL)
		return (ACPI_EXC);
	DYNAMIC_IF_EXE(nsp);
	NS_PUSH(scopep);
	scopep->elem = nsp;
	return (ACPI_OK);
}

/*ARGSUSED*/
int
scope_scope(int rflags, parse_state_t *pstp)
{
	name_t *namep = NAME_VALP_N(-1);
	ns_entry_t *scopep;
	ns_elem_t *nsp;

	exc_debug(ACPI_DLEX, "push scope %s", name_strbuf(namep));
	if (ns_lookup(NSP_ROOT, NSP_CUR, namep, KEY_IF_EXE, 0, &nsp, NULL) !=
	    ACPI_OK)
		return (exc_code(ACPI_EUNDEF));
	NS_PUSH(scopep);
	scopep->elem = nsp;
	return (ACPI_OK);
}

/*
 * if conditional
 *
 * N_IF rule assumes a true predicate
 * THEN clause is taken and ELSE clause skipped (N_ELSE_SKIP)
 *
 * this checks if the predicate is false and then alters the N_IF rule
 * to skip the THEN (substitute N_SKIP for N_TERMLIST) and take the ELSE
 * substitute N_ELSE_TAKE for N_ELSE_SKIP)
 */
/*ARGSUSED*/
int
if_cond(int rflags, parse_state_t *pstp)
{
	acpi_val_t *predicate;
	parse_entry_t *pp;

	if (acpi_load(VALUE_PTR_N(-1), pstp, &predicate, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (predicate->type != ACPI_INTEGER) {
		value_free(predicate);
		return (exc_code(ACPI_ETYPE));
	}
	if (predicate->type == ACPI_INTEGER && predicate->acpi_ival == 0) {
		PARSE_POP;	/* pop off N_TERMLIST */
		PARSE_POP;	/* pop off N_ELSE_SKIP */
		PARSE_PUSH(pp);
		pp->elem = N_ELSE_TAKE;
		pp->flags = P_PARSE;
		PARSE_PUSH(pp);
		pp->elem = N_SKIP;
		pp->flags = P_PARSE;
	}
	value_free(predicate);
	return (ACPI_OK);
}

/*
 * while conditional
 *
 * N_WHILE rule assumes a true predicate
 * using the TERM_LIST
 *
 * this checks if the predicate is false and then alters the N_WHILE rule
 * to skip the TERM_LIST (substitute N_SKIP for N_TERMLIST), the reduce
 * rules check for this substitution to determine whether to continue or not
 */
/*ARGSUSED*/
int
while_cond(int rflags, parse_state_t *pstp)
{
	acpi_val_t *predicate;
	parse_entry_t *pp;

	if (acpi_load(VALUE_PTR_N(-1), pstp, &predicate, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (predicate->type != ACPI_INTEGER) {
		value_free(predicate);
		return (exc_code(ACPI_ETYPE));
	}
	if (predicate->type == ACPI_INTEGER && predicate->acpi_ival == 0) {
		PARSE_POP;	/* pop off N_TERMLIST */
		PARSE_PUSH(pp);
		pp->elem = N_SKIP;
		pp->flags = P_PARSE;
	}
	value_free(predicate);
	return (ACPI_OK);
}


/* eof */
