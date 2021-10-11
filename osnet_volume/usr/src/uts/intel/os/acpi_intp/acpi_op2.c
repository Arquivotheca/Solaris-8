/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_op2.c	1.1	99/05/21 SMI"


/* reduce functions for type 2 operators */

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

#include "acpi_elem.h"
#include "acpi_act.h"
#include "acpi_name.h"
#include "acpi_ns.h"
#include "acpi_val.h"
#include "acpi_thr.h"
#include "acpi_io.h"
#include "acpi_inf.h"


/*
 * reduce actions for type 2 operators
 *
 * LHS is VALUE_PTR (or VALUE_PTR_N(0))
 * RHS[i] is VALUE_PTR_N(i), so first RHS is VALUE_PTR_N(1), etc.
 */

#define	_STORE (1)

/*ARGSUSED*/
static int
two_arg_op_reduce(int opcode, int rflags, parse_state_t *pstp, int store_f)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *operand1, *operand2;
	value_entry_t *result;	/* VALUE_PTR(4) if store_f is true */
	unsigned int answer;

	if (acpi_load(VALUE_PTR_N(2), pstp, &operand1, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (acpi_load(VALUE_PTR_N(3), pstp, &operand2, 0) != ACPI_OK) {
		value_free(operand1);
		return (ACPI_EXC);
	}

	if (operand1->type != ACPI_INTEGER || operand2->type != ACPI_INTEGER) {
		value_free(operand1);
		value_free(operand2);
		return (exc_code(ACPI_ETYPE));
	}

	switch (opcode) {
	case T_ADD_OP:
		answer = operand1->acpi_ival + operand2->acpi_ival;
		break;
	case T_SUBTRACT_OP:
		answer = operand1->acpi_ival - operand2->acpi_ival;
		break;
	case T_MULTIPLY_OP:
		answer = operand1->acpi_ival * operand2->acpi_ival;
		break;
	case T_AND_OP:
		answer = operand1->acpi_ival & operand2->acpi_ival;
		break;
	case T_NAND_OP:
		answer = ~(operand1->acpi_ival & operand2->acpi_ival);
		break;
	case T_OR_OP:
		answer = operand1->acpi_ival | operand2->acpi_ival;
		break;
	case T_NOR_OP:
		answer = ~(operand1->acpi_ival | operand2->acpi_ival);
		break;
	case T_XOR_OP:
		answer = operand1->acpi_ival ^ operand2->acpi_ival;
		break;
	case T_SHIFT_LEFT_OP:
		answer = operand1->acpi_ival << operand2->acpi_ival;
		break;
	case T_SHIFT_RIGHT_OP:
		answer = operand1->acpi_ival >> operand2->acpi_ival;
		break;
	case T_LAND_OP:
		answer = (operand1->acpi_ival && operand2->acpi_ival) ?
		    ACPI_ONES : 0;
		break;
	case T_LNAND_OP:
		answer = (operand1->acpi_ival && operand2->acpi_ival) ?
		    0 : ACPI_ONES;
		break;
	case T_LOR_OP:
		answer = (operand1->acpi_ival || operand2->acpi_ival) ?
		    ACPI_ONES : 0;
		break;
	case T_LNOR_OP:
		answer = (operand1->acpi_ival || operand2->acpi_ival) ?
		    0 : ACPI_ONES;
		break;
	case T_LEQ_OP:
		answer = (operand1->acpi_ival == operand2->acpi_ival) ?
		    ACPI_ONES : 0;
		break;
	case T_LNE_OP:
		answer = (operand1->acpi_ival != operand2->acpi_ival) ?
		    ACPI_ONES : 0;
		break;
	case T_LGT_OP:
		answer = (operand1->acpi_ival > operand2->acpi_ival) ?
		    ACPI_ONES : 0;
		break;
	case T_LLE_OP:
		answer = (operand1->acpi_ival <= operand2->acpi_ival) ?
		    ACPI_ONES : 0;
		break;
	case T_LLT_OP:
		answer = (operand1->acpi_ival < operand2->acpi_ival) ?
		    ACPI_ONES : 0;
		break;
	case T_LGE_OP:
		answer = (operand1->acpi_ival >= operand2->acpi_ival) ?
		    ACPI_ONES : 0;
		break;
	}
	value_free(operand1);
	value_free(operand2);
	ret->elem = V_ACPI_VALUE;
	if ((ret->data = integer_new(answer)) == NULL)
		return (ACPI_EXC);
	result = VALUE_PTR_N(4);
	if (store_f && (acpi_store(ret->data, pstp, result) == ACPI_EXC))
		return (ACPI_EXC);
	return (ACPI_OK);
}

int
add_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_ADD_OP, rflags, pstp, _STORE));
}

int
subtract_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_SUBTRACT_OP, rflags, pstp, _STORE));
}

/*ARGSUSED*/
int
increment_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	value_entry_t *addend = VALUE_PTR_N(2);
	acpi_val_t *value;

	if (acpi_load(addend, pstp, &value, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (value->type != ACPI_INTEGER) {
		value_free(value);
		return (exc_code(ACPI_ETYPE));
	}
	ret->elem = V_ACPI_VALUE;
	ret->data = integer_new(value->acpi_ival + 1);
	value_free(value);
	if (ret->data == NULL)
		return (ACPI_EXC);
	if (acpi_store(ret->data, pstp, addend) == ACPI_EXC)
		return (ACPI_EXC);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
decrement_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	value_entry_t *addend = VALUE_PTR_N(2);
	acpi_val_t *value;

	if (acpi_load(addend, pstp, &value, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (value->type != ACPI_INTEGER) {
		value_free(value);
		return (exc_code(ACPI_ETYPE));
	}
	ret->elem = V_ACPI_VALUE;
	ret->data = integer_new(value->acpi_ival - 1);
	value_free(value);
	if (ret->data == NULL)
		return (ACPI_EXC);
	if (acpi_store(ret->data, pstp, addend) == ACPI_EXC)
		return (ACPI_EXC);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
divide_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *dividend_val, *divisor_val;
	value_entry_t *remainder = VALUE_PTR_N(4);
	value_entry_t *result = VALUE_PTR_N(5);
	acpi_val_t *value;
	int dividend, divisor;

	if (acpi_load(VALUE_PTR_N(2), pstp, &dividend_val, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (acpi_load(VALUE_PTR_N(3), pstp, &divisor_val, 0) != ACPI_OK) {
		value_free(dividend_val);
		return (ACPI_EXC);
	}
	if (dividend_val->type != ACPI_INTEGER ||
	    divisor_val->type != ACPI_INTEGER) {
		value_free(dividend_val);
		value_free(divisor_val);
		return (exc_code(ACPI_ETYPE));
	}
	dividend = dividend_val->acpi_ival;
	divisor = divisor_val->acpi_ival;
	value_free(dividend_val);
	value_free(divisor_val);
	if (divisor_val->acpi_ival == 0)
		return (exc_code(ACPI_ELIMIT));	/* Fatal error */
	ret->elem = V_ACPI_VALUE;
	if ((ret->data = integer_new(dividend / divisor)) == NULL)
		return (ACPI_EXC);
	if ((value = integer_new(dividend % divisor)) == NULL)
		return (ACPI_EXC);
	if (acpi_store(value, pstp, remainder) == ACPI_EXC) {
		value_free(value);
		return (ACPI_EXC);
	}
	value_free(value);
	if (acpi_store(ret->data, pstp, result) == ACPI_EXC)
		return (ACPI_EXC);
	return (ACPI_OK);
}

int
multiply_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_MULTIPLY_OP, rflags, pstp, _STORE));
}

int
and_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_AND_OP, rflags, pstp, _STORE));
}

int
nand_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_NAND_OP, rflags, pstp, _STORE));
}

int
or_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_OR_OP, rflags, pstp, _STORE));
}

int
nor_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_NOR_OP, rflags, pstp, _STORE));
}

int
xor_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_XOR_OP, rflags, pstp, _STORE));
}

/*ARGSUSED*/
int
not_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *source;
	value_entry_t *result = VALUE_PTR_N(3);

	if (acpi_load(VALUE_PTR_N(2), pstp, &source, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (source->type != ACPI_INTEGER) {
		value_free(source);
		return (exc_code(ACPI_ETYPE));
	}
	ret->elem = V_ACPI_VALUE;
	if ((ret->data = integer_new(~(source->acpi_ival))) == NULL) {
		value_free(source);
		return (ACPI_EXC);
	}
	if (acpi_store(ret->data, pstp, result) == ACPI_EXC)
		return (ACPI_EXC);
	value_free(source);
	return (ACPI_OK);
}

int
shift_left_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_SHIFT_LEFT_OP, rflags, pstp, _STORE));
}

int
shift_right_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_SHIFT_RIGHT_OP, rflags, pstp, _STORE));
}

/*ARGSUSED*/
int
left_bit_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *source;
	value_entry_t *result = VALUE_PTR_N(3);
	int i;
	unsigned int src, mask;

	if (acpi_load(VALUE_PTR_N(2), pstp, &source, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (source->type != ACPI_INTEGER) {
		value_free(source);
		return (exc_code(ACPI_ETYPE));
	}
	src = source->acpi_ival;
	value_free(source);
	for (i = 0, mask = (unsigned int)0x80000000; i < 32; i++, mask >>= 1)
		if (src & mask)
			break;
	ret->elem = V_ACPI_VALUE;
	if ((ret->data = integer_new((i == 32) ? 0 : (i + 1))) == NULL)
		return (ACPI_EXC);
	if (acpi_store(ret->data, pstp, result) == ACPI_EXC)
		return (ACPI_EXC);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
right_bit_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *source;
	value_entry_t *result = VALUE_PTR_N(3);
	int i;
	unsigned int src, mask;

	if (acpi_load(VALUE_PTR_N(2), pstp, &source, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (source->type != ACPI_INTEGER) {
		value_free(source);
		return (exc_code(ACPI_ETYPE));
	}
	src = source->acpi_ival;
	value_free(source);
	for (i = 0, mask = 1; i < 32; i++, mask <<= 1)
		if (src & mask)
			break;
	ret->elem = V_ACPI_VALUE;
	if ((ret->data = integer_new((i == 32) ? 0 : (32 - i))) == NULL)
		return (ACPI_EXC);
	if (acpi_store(ret->data, pstp, result) == ACPI_EXC)
		return (ACPI_EXC);
	return (ACPI_OK);
}

int
land_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_LAND_OP, rflags, pstp, 0));
}

int
lnand_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_LNAND_OP, rflags, pstp, 0));
}

int
lor_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_LOR_OP, rflags, pstp, 0));
}

int
lnor_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_LNOR_OP, rflags, pstp, 0));
}

/*ARGSUSED*/
int
lnot_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *source;

	if (acpi_load(VALUE_PTR_N(2), pstp, &source, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (source->type != ACPI_INTEGER) {
		value_free(source);
		return (exc_code(ACPI_ETYPE));
	}
	ret->elem = V_ACPI_VALUE;
	ret->data = integer_new(source->acpi_ival ? 0 : ACPI_ONES);
	value_free(source);
	if (ret->data == NULL)
		return (ACPI_EXC);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
lnz_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *source;

	if (acpi_load(VALUE_PTR_N(2), pstp, &source, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (source->type != ACPI_INTEGER) {
		value_free(source);
		return (exc_code(ACPI_ETYPE));
	}
	ret->elem = V_ACPI_VALUE;
	ret->data = integer_new(source->acpi_ival ? ACPI_ONES : 0);
	value_free(source);
	if (ret->data == NULL)
		return (ACPI_EXC);
	return (ACPI_OK);
}

int
leq_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_LEQ_OP, rflags, pstp, 0));
}

int
lne_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_LNE_OP, rflags, pstp, 0));
}

int
lgt_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_LGT_OP, rflags, pstp, 0));
}

int
lle_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_LLE_OP, rflags, pstp, 0));
}

int
llt_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_LLT_OP, rflags, pstp, 0));
}

int
lge_reduce(int rflags, parse_state_t *pstp)
{
	return (two_arg_op_reduce(T_LGE_OP, rflags, pstp, 0));
}

/*ARGSUSED*/
int
cond_ref_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *source, *ref;
	int undef = 0;

	if (acpi_load(VALUE_PTR_N(2), pstp, &source, _NO_EVAL) != ACPI_OK) {
		if (exc_no() == ACPI_EUNDEF)
			undef = 1;
		else
			return (ACPI_EXC);
	}
	ret->elem = V_ACPI_VALUE;
	ret->data = integer_new(undef ? 0: ACPI_ONES); /* can't fail */
	if ((ref = ref_new(source)) == NULL)
		return (ACPI_EXC);
	if (acpi_store(ref, pstp, VALUE_PTR_N(3)) != ACPI_OK)
		return (ACPI_EXC);
	value_free(source);
	value_free(ref);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
deref_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *ref;

	if (acpi_load(VALUE_PTR_N(2), pstp, &ref, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (ref->type != ACPI_REF)
		return (ACPI_EXC);
	ret->data = ref->acpi_valp;
	ret->elem = value_elem(ret->data);
	value_free(ref);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
ref_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *object;

	if (acpi_load(VALUE_PTR_N(2), pstp, &object, _NO_EVAL) != ACPI_OK)
		return (ACPI_EXC);
	ret->elem = V_REF;
	if ((ret->data = ref_new(object)) == NULL)
		return (ACPI_EXC);
	value_free(object);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
index_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *source, *index, *antecedent;

	if (acpi_load(VALUE_PTR_N(2), pstp, &source, _EXPECT_BUF) != ACPI_OK)
		return (ACPI_EXC);
	if (acpi_load(VALUE_PTR_N(3), pstp, &index, 0) != ACPI_OK) {
		value_free(source);
		return (ACPI_EXC);
	}
	if (source->type != ACPI_BUFFER && source->type != ACPI_PACKAGE ||
	    index->type != ACPI_INTEGER) {
		value_free(source);
		value_free(index);
		return (exc_code(ACPI_ETYPE));
	}
	if (index->acpi_ival >= source->length)	/* zero based */
		return (exc_code(ACPI_ERANGE));
	if (source->type == ACPI_BUFFER) {
		if ((antecedent = buffield_new(source, index->acpi_ival, 8)) ==
		    NULL)
			return (ACPI_EXC);
	} else if (source->type == ACPI_PACKAGE)
		antecedent = ACPI_PKG_N(source, index->acpi_ival);

	ret->elem = V_REF;
	if ((ret->data = ref_new(antecedent)) == NULL)
		return (ACPI_EXC);
	value_free(source);
	value_free(index);
	return (ACPI_OK);
}

/* match relation values */
#define	MTR 0
#define	MEQ 1
#define	MLE 2
#define	MLT 3
#define	MGE 4
#define	MGT 5

static int
match_comp(acpi_val_t *pkg_elem, int relation, int obj)
{
	int is_integer;

	is_integer = (pkg_elem->type == ACPI_INTEGER);
	switch (relation) {
	case MTR:
		return (1);
	case MEQ:
		if (is_integer == 0)
			return (0);
		return (pkg_elem->acpi_ival == obj);
	case MLE:
		if (is_integer == 0)
			return (0);
		return (pkg_elem->acpi_ival <= obj);
	case MLT:
		if (is_integer == 0)
			return (0);
		return (pkg_elem->acpi_ival < obj);
	case MGE:
		if (is_integer == 0)
			return (0);
		return (pkg_elem->acpi_ival >= obj);
	case MGT:
		if (is_integer == 0)
			return (0);
		return (pkg_elem->acpi_ival > obj);
	default:
		return (0);
	}
}

/*ARGSUSED*/
int
match_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	int rel1 = INT_VAL_N(3);
	int rel2 = INT_VAL_N(5);
	acpi_val_t *pkg, *obj1, *obj2, *start;
	int reduce_ret = ACPI_EXC;
	int i;

	if (acpi_load(VALUE_PTR_N(2), pstp, &pkg, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (acpi_load(VALUE_PTR_N(4), pstp, &obj1, 0) != ACPI_OK)
		goto fail3;
	if (acpi_load(VALUE_PTR_N(6), pstp, &obj2, 0) != ACPI_OK)
		goto fail2;
	if (acpi_load(VALUE_PTR_N(7), pstp, &start, 0) != ACPI_OK)
		goto fail1;
	if (pkg->type != ACPI_PACKAGE || obj1->type != ACPI_INTEGER ||
		obj2->type != ACPI_INTEGER || start->type != ACPI_INTEGER) {
		(void) exc_code(ACPI_ETYPE);
		goto fail;
	}
	for (i = start->acpi_ival; i < pkg->length; i++)
		if (match_comp(ACPI_PKG_N(pkg, i), rel1, obj1->acpi_ival) &&
		    match_comp(ACPI_PKG_N(pkg, i), rel2, obj2->acpi_ival))
			break;
	ret->elem = V_ACPI_VALUE;
	if (ret->data = integer_new(i < pkg->length ? i : ACPI_ONES))
	    reduce_ret = ACPI_OK;
fail:
	value_free(start);
fail1:
	value_free(obj2);
fail2:
	value_free(obj1);
fail3:
	value_free(pkg);
	return (reduce_ret);
}

/*ARGSUSED*/
int
from_bcd_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *value;
	value_entry_t *result = VALUE_PTR_N(3);
	unsigned int bcd;
	int i, num, step;

	if (acpi_load(VALUE_PTR_N(2), pstp, &value, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (value->type != ACPI_INTEGER) {
		value_free(value);
		return (exc_code(ACPI_ETYPE));
	}
	bcd = value->acpi_ival;
	value_free(value);
	for (i = num = 0, step = 1; i < 8; i++) {
		num += (bcd & 0xF) * step;
		bcd >>= 4;
		step *= 10;
	}
	ret->elem = V_ACPI_VALUE;
	if ((ret->data = integer_new(num)) == NULL)
		return (ACPI_EXC);
	if (acpi_store(ret->data, pstp, result) == ACPI_EXC)
		return (ACPI_EXC);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
to_bcd_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	value_entry_t *result = VALUE_PTR_N(3);
	acpi_val_t *value;
	unsigned int bcd;
	int i, num;

	if (acpi_load(VALUE_PTR_N(2), pstp, &value, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (value->type != ACPI_INTEGER) {
		value_free(value);
		return (exc_code(ACPI_ETYPE));
	}
	num = value->acpi_ival;
	value_free(value);
	for (i = bcd = 0; i < 8; i++) {
		bcd |= (num % 10) << (i * 4);
		num /= 10;
	}
	ret->elem = V_ACPI_VALUE;
	if ((ret->data = integer_new(bcd)) == NULL)
		return (ACPI_EXC);
	if (acpi_store(ret->data, pstp, result) == ACPI_EXC) /* return bcd */
		return (ACPI_EXC);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
acquire_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *timeout = ACPI_VALP_N(3);
	acpi_val_t *sync;
	acpi_mutex_t *mutexp;
	acpi_thread_t *threadp;

	if (acpi_load(VALUE_PTR_N(2), pstp, &sync, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (sync->type != ACPI_MUTEX || timeout->type != ACPI_INTEGER)
		return (exc_code(ACPI_ETYPE));
	mutexp = sync->acpi_valp;
	threadp = pstp->thread;

	ret->elem = V_ACPI_VALUE;
	if (mutexp->owner == threadp) {
		ret->data = integer_new(0);
	} else if (mutexp->owner == NULL) {
		if (mutexp->sync < threadp->sync)
			return (exc_code(ACPI_ESYNC));
		threadp->sync = mutexp->sync; /* raise sync level  */
		mutexp->owner = threadp; /* add to mutex list */
		mutexp->next = threadp->mutex_list;
		threadp->mutex_list = sync;
		ret->data = integer_new(0);
	} else {
	/*
	 * XXX needs work for true MT interpreter, do cv wait sig timeout
	 * in the meantime do not honor NO_TIMEOUT as we would hang
	 */
		if (timeout->acpi_ival != ACPI_NO_TIMEOUT)
			acpi_delay_sig(timeout->acpi_ival);
		ret->data = integer_new(ACPI_ONES);
	}

	value_free(sync);
	value_free(timeout);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
concat_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *source1, *source2;
	value_entry_t *result = VALUE_PTR_N(4);
	int *ibuf;
	char *buf;

	if (acpi_load(VALUE_PTR_N(2), pstp, &source1, _EXPECT_BUF) != ACPI_OK)
		return (ACPI_EXC);
	if (acpi_load(VALUE_PTR_N(3), pstp, &source2, 0) != ACPI_OK) {
		value_free(source1);
		return (ACPI_EXC);
	}
	if (source1->type != source2->type || (source1->type != ACPI_INTEGER &&
	    source1->type != ACPI_STRING && source1->type != ACPI_BUFFER)) {
		value_free(source1);
		value_free(source2);
		return (exc_code(ACPI_ETYPE));
	}
	ret->elem = V_ACPI_VALUE;
	if (source1->type == ACPI_INTEGER) {
				/* multiple of 4 */
		ibuf = kmem_alloc(sizeof (int) * 2, KM_SLEEP);
		*ibuf = source1->acpi_ival;
		*(ibuf + 1) = source2->acpi_ival;
		ret->data = buffer_new((char *)ibuf, sizeof (int) * 2);
	} else if (source1->type == ACPI_BUFFER) {
		buf = kmem_alloc(RND_UP4(source1->length + source2->length),
		    KM_SLEEP);
		bcopy(source1->acpi_valp, buf, source1->length);
		bcopy(source2->acpi_valp, buf + source1->length,
		    source2->length);
		ret->data = buffer_new(buf, source1->length + source2->length);
	} else {
		buf = kmem_alloc(strlen(source1->acpi_valp) +
		    strlen(source2->acpi_valp) + 1, KM_SLEEP);
		(void) strcpy(buf, source1->acpi_valp);
		(void) strcpy(buf + strlen(source1->acpi_valp),
		    source2->acpi_valp);
		ret->data = string_new(buf);
	}
	value_free(source1);
	value_free(source2);
	if (ret->data == NULL || (acpi_store(ret->data, pstp, result) ==
	    ACPI_EXC))
		return (ACPI_EXC);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
sizeof_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *object;

	if (acpi_load(VALUE_PTR_N(2), pstp, &object, _NO_EVAL) != ACPI_OK)
		return (ACPI_EXC);
	if (object->type != ACPI_STRING && object->type != ACPI_BUFFER &&
	    object->type != ACPI_PACKAGE) {
		value_free(object);
		return (exc_code(ACPI_ETYPE));
	}
	ret->elem = V_ACPI_VALUE;
	if ((ret->data = integer_new(object->type == ACPI_STRING ?
	    strlen(object->acpi_valp) : object->length)) == NULL)
		return (ACPI_EXC);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
store_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	value_entry_t *dest = VALUE_PTR_N(3);
	acpi_val_t *src;

	if (acpi_load(VALUE_PTR_N(2), pstp, &src, 0) != ACPI_OK)
		return (ACPI_EXC);
	ret->data = src;
	ret->elem = value_elem(src);
	if (acpi_store(src, pstp, dest) == ACPI_EXC)
		return (ACPI_EXC);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
type_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t *object;

	if (acpi_load(VALUE_PTR_N(2), pstp, &object, 0) != ACPI_OK)
		return (ACPI_EXC);
	for (; object->type == ACPI_REF; ) /* deref refs */
		object = (acpi_val_t *)(object->acpi_valp);
	ret->elem = V_ACPI_VALUE;
	ret->data = integer_new(object->type);
	value_free(object);
	if (ret->data == NULL)
		return (ACPI_EXC);
	return (ACPI_OK);
}

/*ARGSUSED*/
int
wait_reduce(int rflags, parse_state_t *pstp)
{
	value_entry_t *ret = VALUE_PTR;
	acpi_val_t  *event, *timeout;

	if (acpi_load(VALUE_PTR_N(2), pstp, &event, 0) != ACPI_OK)
		return (ACPI_EXC);

	/* XXX despite the spec says, timeout is a TERMARG not WORD_DATA */
	if (acpi_load(VALUE_PTR_N(3), pstp, &timeout, 0) != ACPI_OK)
		return (ACPI_EXC);
	if (event->type != ACPI_EVENT || timeout->type != ACPI_INTEGER)
		return (exc_code(ACPI_ETYPE));

	ret->elem = V_ACPI_VALUE;
	if (event->acpi_ival > 0) {
		event->acpi_ival -= 1;
		ret->data = integer_new(0);
	} else if (event->acpi_ival == 0) {
		/* XXX TERMARG could have huge values */
		if (timeout->acpi_ival > (unsigned int)ACPI_NO_TIMEOUT)
			timeout->acpi_ival = ACPI_NO_TIMEOUT - 1;
	/*
	 * XXX needs work for true MT interpreter, do cv wait sig timeout
	 * in the meantime do not honor NO_TIMEOUT as we would hang
	 */
		if (timeout->acpi_ival != ACPI_NO_TIMEOUT)
			acpi_delay_sig(timeout->acpi_ival);
		ret->data = integer_new(ACPI_ONES);
	}
	value_free(event);
	value_free(timeout);
	return (ACPI_OK);
}


/* eof */
