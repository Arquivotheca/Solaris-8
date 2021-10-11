/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_decl.c	1.2	99/10/14 SMI"


/* reduce functions for data and name space */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>
#include <sys/acpi_prv.h>

#ifdef ACPI_BOOT
#include <sys/salib.h>
#endif

#ifdef ACPI_USER
#include <stdlib.h>
#include <strings.h>
#endif

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
#include "acpi_inf.h"

/*
 * special reduce actions
 *
 * LHS is VALUE_PTR (or VALUE_PTR_N(0))
 * RHS[i] is VALUE_PTR_N(i), so first RHS is VALUE_PTR_N(1), etc.
 */

/*LINTLIBRARY*/
/*ARGSUSED*/
int
null_action(int rflags, parse_state_t *pstp)
{
	return (ACPI_OK);
}

/* promote up the type hierarchy */
/*ARGSUSED*/
int
rhs1_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *lhs = VALUE_PTR;
	value_entry_t *rhs1 = VALUE_PTR_N(1);

	*lhs = *rhs1;
	return (ACPI_OK);
}

/*ARGSUSED*/
int
rhs2_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *lhs = VALUE_PTR;
	value_entry_t *rhs2 = VALUE_PTR_N(2);

	*lhs = *rhs2;
	return (ACPI_OK);
}

/*ARGSUSED*/
int
list_free(int rflags, parse_state_t *pstp)
{
	node_free_list(VALUE_PTR->data);
	return (ACPI_OK);
}


/*
 * data
 */

/*ARGSUSED*/
int
zero_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;

	ret->elem = V_ACPI_VALUE;
	ret->data = integer_new(0); /* can't fail */
	return (ACPI_OK);
}


/*ARGSUSED*/
int
one_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;

	ret->elem = V_ACPI_VALUE;
	ret->data = integer_new(1); /* can't fail */
	return (ACPI_OK);
}


/*ARGSUSED*/
int
ones_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;

	ret->elem = V_ACPI_VALUE;
	ret->data = integer_new(ACPI_ONES); /* can't fail */
	return (ACPI_OK);
}

/*ARGSUSED*/
int
revision_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;

	ret->elem = V_ACPI_VALUE;
	if ((ret->data = integer_new(acpi_interpreter_revision)) == NULL)
		return (ACPI_EXC);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
buffer_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *decl_size;
	acpi_val_t *byte_list_p = ACPI_VALP_N(4);
	char *buffer;

	if (acpi_load(VALUE_PTR_N(3), pstp, &decl_size, 0) == ACPI_EXC ||
	    decl_size->type != ACPI_INTEGER)
		return (exc_code(ACPI_ETYPE));
	ret->elem = V_ACPI_VALUE;
	if (decl_size->acpi_ival <= byte_list_p->length) {
		value_free(decl_size);
		ret->data = byte_list_p;
		return (ACPI_OK);	/* no adjustment necessary */
	}
				/* need bigger */
	if ((buffer = kmem_alloc(RND_UP4(decl_size->acpi_ival), KM_SLEEP)) ==
	    NULL)
		return (exc_code(ACPI_ERES));
	bzero(buffer, decl_size->acpi_ival);
	bcopy(byte_list_p->acpi_valp, buffer, byte_list_p->length);
	if ((ret->data = buffer_new(buffer, decl_size->acpi_ival)) == NULL)
		return (ACPI_EXC);
	value_free(byte_list_p);
	value_free(decl_size);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
package_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	int decl_elem = ACPI_INT_N(3);
	node_t *np = VALUE_DATA_N(4);
	node_t *ptr;
	acpi_val_t **avpp;
	int elem, i;

	for (elem = 0, ptr = np; ptr; ptr = ptr->next) /* count elements */
		elem++;
	if (elem > decl_elem)
		decl_elem = elem;
	ret->elem = V_ACPI_VALUE;
	if ((ret->data = package_new(decl_elem)) == NULL)
		return (ACPI_EXC);
	/* set elements, we don't use pkg_setn to avoid an extra hold here */
	avpp = (acpi_val_t **)(((acpi_val_t *)(ret->data))->acpi_valp);
	for (i = 0, ptr = np; i < elem; i++, avpp++, ptr = ptr->next) {
		value_free(*avpp);
		*avpp = (acpi_val_t *)(ptr->data);
	}
	value_free(VALUE_PTR_N(3)->data);
	node_free_list(np); /* free the list */
	return (ACPI_OK);
}


/*
 * buffer fields
 */

/*ARGSUSED*/
int
create_field_reduce(int rflags, parse_state_t *pstp)
{
	int offset = INT_VAL_N(1);
	acpi_val_t *src_buf, *bit_idx, *width;
	name_t *namep = NAME_VALP_N(5);
	ns_elem_t *nsp;
	int bit_len;

	if (acpi_load(VALUE_PTR_N(2), pstp, &src_buf, _EXPECT_BUF) ==
	    ACPI_EXC ||
	    acpi_load(VALUE_PTR_N(3), pstp, &bit_idx, 0) == ACPI_EXC ||
	    acpi_load(VALUE_PTR_N(4), pstp, &width, 0) == ACPI_EXC ||
	    src_buf->type != ACPI_BUFFER ||
	    bit_idx->type != ACPI_INTEGER ||
	    width->type != ACPI_INTEGER)
		return (exc_code(ACPI_ETYPE));
	bit_len = src_buf->length * 8;
	if (bit_len < bit_idx->acpi_ival + width->acpi_ival)
		return (exc_code(ACPI_ERANGE));
	if ((nsp = ns_define(NSP_ROOT, NSP_CUR, namep, KEY_IF_EXE, pstp->key,
	    offset)) == NULL)
		return (exc_code(ACPI_EALREADY));
	if ((nsp->valp = buffield_new(src_buf, bit_idx->acpi_ival,
	    width->acpi_ival)) == NULL)
		return (ACPI_EXC);
	DYNAMIC_IF_EXE(nsp);
	value_free(src_buf);
	value_free(bit_idx);
	value_free(width);
	name_free(namep);
	return (ACPI_OK);
}

static int
fixed_field_reduce(parse_state_t *pstp, int offscale, int width)
{
	int offset = INT_VAL_N(1);
	acpi_val_t *src_buf, *bit_idx;
	name_t *namep = NAME_VALP_N(4);
	ns_elem_t *nsp;
	int bit_len;

	if (acpi_load(VALUE_PTR_N(2), pstp, &src_buf, _EXPECT_BUF) ==
	    ACPI_EXC ||
	    acpi_load(VALUE_PTR_N(3), pstp, &bit_idx, 0) == ACPI_EXC ||
	    src_buf->type != ACPI_BUFFER ||
	    bit_idx->type != ACPI_INTEGER)
		return (exc_code(ACPI_ETYPE));
	bit_len = src_buf->length * 8;
	if (bit_len < bit_idx->acpi_ival * offscale + width)
		return (exc_code(ACPI_ERANGE));
	if ((nsp = ns_define(NSP_ROOT, NSP_CUR, namep, KEY_IF_EXE, pstp->key,
	    offset)) == NULL)
		return (exc_code(ACPI_EALREADY));
	if ((nsp->valp = buffield_new(src_buf, bit_idx->acpi_ival * offscale,
	    width)) == NULL)
		return (ACPI_EXC);
	DYNAMIC_IF_EXE(nsp);
	value_free(src_buf);
	value_free(bit_idx);
	name_free(namep);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
bit_field_reduce(int rflags, parse_state_t *pstp)
{
	return (fixed_field_reduce(pstp, 1, 1));
}

/*ARGSUSED*/
int
byte_field_reduce(int rflags, parse_state_t *pstp)
{
	return (fixed_field_reduce(pstp, 8, 8));
}

/*ARGSUSED*/
int
word_field_reduce(int rflags, parse_state_t *pstp)
{
	return (fixed_field_reduce(pstp, 8, 16));
}

/*ARGSUSED*/
int
dword_field_reduce(int rflags, parse_state_t *pstp)
{
	return (fixed_field_reduce(pstp, 8, 32));
}


/*
 * fields
 *
 * access/named/reserved data adjusted by field list later
 */

/*ARGSUSED*/
int
access_field_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	int acc_type = ACPI_INT_N(2);
	int acc_attrib = ACPI_INT_N(3);

	if ((ret->data = field_new(NULL, 0, 0, 0, 0, acc_type, acc_attrib)) ==
	    NULL)
		return (ACPI_EXC);
	value_free(ACPI_VALP_N(2));
	value_free(ACPI_VALP_N(3));
	return (ACPI_OK);
}

/*ARGSUSED*/
int
named_field_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	int offset = INT_VAL_N(1);
	acpi_nameseg_t *segp = (acpi_nameseg_t *)(VALUE_PTR_N(2)->data);
	int length = INT_VAL_N(3);
	ns_elem_t *nsp;

	if (ns_lookup_here(NSP_CUR, segp, 0) != NULL)
		return (exc_code(ACPI_EALREADY));
	if ((nsp = ns_define_here(NSP_CUR, segp, pstp->key, offset)) == NULL)
		return (ACPI_EXC);
	/*
	 * use field no matter what it really is
	 */
	if ((ret->data = field_new(0, 0, 0, length, 0, 0, 0)) == NULL)
		return (ACPI_EXC);
	nsp->valp = ret->data;	/* so field list can find it */
	DYNAMIC_IF_EXE(nsp);
	nameseg_free(segp);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
res_field_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	int length = INT_VAL_N(2);

	if ((ret->data = field_new(NULL, 0, 0, length, 0, 0, 0)) == NULL)
		return (ACPI_EXC);
	return (ACPI_OK);
}

/* process field list down to named_fields */
/*ARGSUSED*/
int
field_list_reduce(int rflags, parse_state_t *pstp)
{
	node_t *np = VALUE_PTR->data;
	node_t *ptr, *next;
	acpi_field_t *afp;
	acpi_field_t current;

	bzero(&current, sizeof (acpi_field_t));
	for (ptr = np; ptr; ptr = next) {
		next = ptr->next; /* save next, so ptr can be freed */
		afp = ((acpi_val_t *)(ptr->data))->acpi_valp;
		switch (ptr->elem) {
		case N_RES_FIELD:
			current.offset += afp->length;
			if (ptr == np)
				np = np->next;
			value_free(ptr->data);
			node_unlink_subtree(ptr);
			node_free(ptr);
			break;
		case N_ACCESS_FIELD:
			current.acc_type = afp->acc_type;
			current.acc_attrib = afp->acc_attrib;
			if (ptr == np)
				np = np->next;
			value_free(ptr->data);
			node_unlink_subtree(ptr);
			node_free(ptr);
			break;
		case N_NAMED_FIELD:
			afp->offset = current.offset;
			afp->acc_type = current.acc_type;
			afp->acc_attrib = current.acc_attrib;
			current.offset += afp->length;
			break;
		}
	}
	VALUE_PTR->data = np;	/* in case head changed */
	return (ACPI_OK);
}

/*ARGSUSED*/
int
bank_field_reduce(int rflags, parse_state_t *pstp)
{
	name_t *region_namep = NAME_VALP_N(3);
	name_t *bank_namep = NAME_VALP_N(4);
	int field_flags = INT_VAL_N(6);
	node_t *np = VALUE_DATA_N(7);
	ns_elem_t *region_nsp, *bank_nsp;
	acpi_val_t *bank_value;
	acpi_field_t *afp;
	node_t *ptr;

	if (acpi_load(VALUE_PTR_N(5), pstp, &bank_value, 0) == ACPI_EXC ||
	    bank_value->type != ACPI_INTEGER)
		return (exc_code(ACPI_ETYPE));
				/* look up region */
	if (ns_lookup(NSP_ROOT, NSP_CUR, region_namep, KEY_IF_EXE, 0,
	    &region_nsp, NULL) != ACPI_OK)
		return (exc_code(ACPI_EUNDEF));
				/* look up bank */
	if (ns_lookup(NSP_ROOT, NSP_CUR, bank_namep, KEY_IF_EXE, 0, &bank_nsp,
	    NULL) != ACPI_OK)
		return (exc_code(ACPI_EUNDEF));
	for (ptr = np; ptr; ptr = ptr->next) {
		afp = ((acpi_val_t *)(ptr->data))->acpi_valp;
		/* set other acpi_field_t items */
		afp->src.bank.region = region_nsp->valp;
		value_hold(region_nsp->valp);
		afp->src.bank.bank = bank_nsp->valp;
		value_hold(bank_nsp->valp);
		afp->src.bank.value = bank_value->acpi_ival;
		afp->flags = ACPI_BANK_TYPE;
		afp->fld_flags = (unsigned char)field_flags;
	}
	name_free(region_namep);
	name_free(bank_namep);
	value_free(bank_value);
	node_free_list(np);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
field_reduce(int rflags, parse_state_t *pstp)
{
	name_t *region_namep = NAME_VALP_N(3);
	int field_flags = INT_VAL_N(4);
	node_t *np = VALUE_DATA_N(5);
	ns_elem_t *region_nsp;
	acpi_field_t *afp;
	node_t *ptr;

	if (ns_lookup(NSP_ROOT, NSP_CUR, region_namep, KEY_IF_EXE, 0,
	    &region_nsp, NULL) != ACPI_OK)
		return (exc_code(ACPI_EUNDEF));
	for (ptr = np; ptr; ptr = ptr->next) {
		afp = ((acpi_val_t *)(ptr->data))->acpi_valp;
		afp->src.field.region = region_nsp->valp;
		value_hold(region_nsp->valp);
		afp->flags = ACPI_REGULAR_TYPE;
		afp->fld_flags = (unsigned char)field_flags;
	}
	name_free(region_namep);
	node_free_list(np);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
index_field_reduce(int rflags, parse_state_t *pstp)
{
	name_t *index_namep = NAME_VALP_N(3);
	name_t *data_namep = NAME_VALP_N(4);
	int field_flags = INT_VAL_N(5);
	node_t *np = VALUE_DATA_N(6);
	ns_elem_t *index_nsp, *data_nsp;
	acpi_field_t *afp;
	node_t *ptr;

				/* look up index */
	if (ns_lookup(NSP_ROOT, NSP_CUR, index_namep, KEY_IF_EXE, 0,
	    &index_nsp, NULL) != ACPI_OK)
		return (exc_code(ACPI_EUNDEF));
				/* look up data */
	if (ns_lookup(NSP_ROOT, NSP_CUR, data_namep, KEY_IF_EXE, 0, &data_nsp,
	    NULL) != ACPI_OK)
		return (exc_code(ACPI_EUNDEF));
	for (ptr = np; ptr; ptr = ptr->next) {
		afp = ((acpi_val_t *)(ptr->data))->acpi_valp;
		/* set other acpi_field_t items */
		afp->src.index.index = index_nsp->valp;
		value_hold(index_nsp->valp);
		afp->src.index.data = data_nsp->valp;
		value_hold(data_nsp->valp);
		afp->flags = ACPI_INDEX_TYPE;
		afp->fld_flags = (unsigned char)field_flags;
	}
	name_free(index_namep);
	name_free(data_namep);
	node_free_list(np);
	return (ACPI_OK);
}


/*
 * other name space declarations
 */

/* most of work done in device_scope rule */
/*ARGSUSED*/
int
device_reduce(int rflags, parse_state_t *pstp)
{
	name_t *namep = VALUE_DATA_N(3);
	/* object_list freed by list_free rule */

	exc_debug(ACPI_DREDUCE, "pop device scope");
	name_free(namep);
	NS_POP;
	return (ACPI_OK);
}

/*ARGSUSED*/
int
event_reduce(int rflags, parse_state_t *pstp)
{
	int offset = INT_VAL_N(1);
	name_t *namep = NAME_VALP_N(2);
	ns_elem_t *nsp;

	exc_debug(ACPI_DREDUCE, "event %s", name_strbuf(namep));
	if ((nsp = ns_define(NSP_ROOT, NSP_CUR, namep, KEY_IF_EXE, pstp->key,
	    offset)) == NULL)
		return (exc_code(ACPI_EALREADY));
	if ((nsp->valp = event_new()) == NULL)
		return (ACPI_EXC);
	DYNAMIC_IF_EXE(nsp);
	name_free(namep);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
method_reduce(int rflags, parse_state_t *pstp)
{
	int offset = INT_VAL_N(1);
	name_t *namep = NAME_VALP_N(3);
	unsigned int method_flags = INT_VAL_N(4);
	acpi_val_t *byte_code = ACPI_VALP_N(5);
	ns_elem_t *nsp;

	exc_debug(ACPI_DLEX, "push method scope %s", name_strbuf(namep));
	if ((nsp = ns_define(NSP_ROOT, NSP_CUR, namep, KEY_IF_EXE, pstp->key,
	    offset)) == NULL)
		return (exc_code(ACPI_EALREADY));
	if ((nsp->valp = method_new(method_flags, byte_code->acpi_valp,
	    byte_code->length)) == NULL)
		return (ACPI_EXC);
	DYNAMIC_IF_EXE(nsp);
	name_free(namep);
	byte_code->acpi_valp = NULL; /* disconnect buffer, free primary */
	value_free(byte_code);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
mutex_reduce(int rflags, parse_state_t *pstp)
{
	int offset = INT_VAL_N(1);
	name_t *namep = NAME_VALP_N(2);
	unsigned char sync = INT_VAL_N(3);
	ns_elem_t *nsp;

	exc_debug(ACPI_DREDUCE, "mutex %s", name_strbuf(namep));
	if ((nsp = ns_define(NSP_ROOT, NSP_CUR, namep, KEY_IF_EXE, pstp->key,
	    offset)) == NULL)
		return (exc_code(ACPI_EALREADY));
	if ((nsp->valp = mutex_new(sync)) == NULL)
		return (ACPI_EXC);
	DYNAMIC_IF_EXE(nsp);
	name_free(namep);
	return (ACPI_OK);
}

/* most of work done in power_scope rule */
/*ARGSUSED*/
int
power_reduce(int rflags, parse_state_t *pstp)
{
	name_t *namep = NAME_VALP_N(3);
	acpi_val_t *resource_order = ACPI_VALP_N(5);

	exc_debug(ACPI_DREDUCE, "pop power scope");
	name_free(namep);
	value_free(resource_order);
	NS_POP;
	return (ACPI_OK);
}

/* most of work done in proc_scope rule */
/*ARGSUSED*/
int
proc_reduce(int rflags, parse_state_t *pstp)
{
	name_t *namep = NAME_VALP_N(3);
	acpi_val_t *id = ACPI_VALP_N(4);
	acpi_val_t *pbaddr = ACPI_VALP_N(5);

	exc_debug(ACPI_DREDUCE, "pop processor scope");
	name_free(namep);
	value_free(id);
	value_free(pbaddr);
	NS_POP;
	return (ACPI_OK);
}

/*ARGSUSED*/
int
region_reduce(int rflags, parse_state_t *pstp)
{
	int offset = INT_VAL_N(1);
	name_t *namep = NAME_VALP_N(2);
	unsigned char space = INT_VAL_N(3);
	acpi_val_t *region_offset, *length;
	acpi_region_t *regionp;
	ns_elem_t *nsp;

	if (acpi_load(VALUE_PTR_N(4), pstp, &region_offset, 0) == ACPI_EXC ||
	    acpi_load(VALUE_PTR_N(5), pstp, &length, 0) == ACPI_EXC ||
	    region_offset->type != ACPI_INTEGER ||
	    length->type != ACPI_INTEGER)
		return (exc_code(ACPI_ETYPE));
	exc_debug(ACPI_DREDUCE, "region %s", name_strbuf(namep));
	if ((nsp = ns_define(NSP_ROOT, NSP_CUR, namep, KEY_IF_EXE, pstp->key,
	    offset)) == NULL)
		return (exc_code(ACPI_EALREADY));
	if ((nsp->valp = region_new(space, region_offset->acpi_ival,
	    length->acpi_ival, (ns_elem_t *)(nsp->node.parent))) == NULL)
		return (ACPI_EXC);
	regionp = (acpi_region_t *)(((acpi_val_t *)(nsp->valp))->acpi_valp);
	if (region_map(regionp) != ACPI_OK)
		return (ACPI_EXC);
	DYNAMIC_IF_EXE(nsp);
	name_free(namep);
	value_free(region_offset);
	value_free(length);
	return (ACPI_OK);
}

/* most of work done in thermal_scope rule */
/*ARGSUSED*/
int
thermal_reduce(int rflags, parse_state_t *pstp)
{
	name_t *namep = NAME_VALP_N(3);

	exc_debug(ACPI_DREDUCE, "pop thermal scope");
	name_free(namep);
	NS_POP;
	return (ACPI_OK);
}


/*
 * name space modifiers
 */
/*ARGSUSED*/
int
alias_reduce(int rflags, parse_state_t *pstp)
{
	int offset = INT_VAL_N(1);
	name_t *srcp = NAME_VALP_N(2);
	name_t *dstp = NAME_VALP_N(3);
	ns_elem_t *src_nsp, *dst_nsp;

	exc_debug(ACPI_DREDUCE, "alias %s", name_strbuf(dstp));
	if (ns_lookup(NSP_ROOT, NSP_CUR, srcp, KEY_IF_EXE, 0, &src_nsp, NULL)
	    != ACPI_OK)
		return (exc_code(ACPI_EUNDEF));
	if ((dst_nsp = ns_define(NSP_ROOT, NSP_CUR, dstp, KEY_IF_EXE,
	    pstp->key, offset)) == NULL)
		return (exc_code(ACPI_EALREADY));
	dst_nsp->valp = src_nsp->valp;
	value_hold(src_nsp->valp);
	DYNAMIC_IF_EXE(dst_nsp);
	name_free(srcp);
	name_free(dstp);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
name_reduce(int rflags, parse_state_t *pstp)
{
	int offset = INT_VAL_N(1);
	name_t *namep = NAME_VALP_N(2);
	acpi_val_t *valp = ACPI_VALP_N(3);
	ns_elem_t *nsp;

	exc_debug(ACPI_DREDUCE, "name %s", name_strbuf(namep));
	if ((nsp = ns_define(NSP_ROOT, NSP_CUR, namep, KEY_IF_EXE, pstp->key,
	    offset)) == NULL)
		return (exc_code(ACPI_EALREADY));
	nsp->valp = valp;
	value_hold(nsp->valp);
	DYNAMIC_IF_EXE(nsp);
	name_free(namep);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
scope_reduce(int rflags, parse_state_t *pstp)
{
	exc_debug(ACPI_DREDUCE, "pop scope");
	NS_POP;
	return (ACPI_OK);
}


/* eof */
