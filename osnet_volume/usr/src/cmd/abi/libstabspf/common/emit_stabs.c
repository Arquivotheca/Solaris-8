/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)emit_stabs.c	1.1	99/05/14 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stab.h>
#include <assert.h>
#include "stabspf_impl.h"

/*
 * General information.
 *
 * emit_() functions are use to emit a complete stab from the begining.
 *	Function returns a stabsret_t value.
 *
 * put_() functions are snprintf() like in that they will place string
 *	information into a supplied buffer but not go past the given size.
 *	Function returns the number of characters that would have been written
 *	to the buffer if it were large enough.
 *
 * MAX_STAB_STR_LEN from stab.h is used as a buffer size for each stab string.
 *
 * Simple stabs will always fit in a singfle MAX_STAB_STR_LEN buffer.
 *
 * Complex stabs (enum, struct, or union) will emit continuation stabs
 * 	brings the stab string beyond MAX_STAB_STR_LEN.  Continuation occurs
 *	on member boundary.  No single member will be longer than
 *	MAX_STAB_STR_LEN.
 *
 * Types that have there stabs emitted as follows:
 *
 *	- All types that have a name associated with them.
 *	- Any type that a named type depends on.
 *	- If type A depends on type B then type B must be emitted first.
 */

/* Indicates whether the stab to be emitted will be continued or not. */
typedef enum continued_stab {
	NOT_CONTINUED = 0,
	CONTINUED = 1
} conts_t;


static size_t put_subtype(char *, size_t, typedesc_t, type_t *, tcode_t);

/*
 * print_stab() - emits a stab string to the appropriate destination.
 *	Destinations are either two elf sections or stdout.
 */
static stabsret_t
print_stab(const char *s, conts_t conts)
{
	if (conts == CONTINUED) {
		(void) fprintf(stdout,
		    "\t.stabs\t\"%s\\\\\",0x%x,0x0,0x0,0x0\n", s, N_LSYM);
	} else {
		(void) fprintf(stdout,
		    "\t.stabs\t\"%s\",0x%x,0x0,0x0,0x0\n", s, N_LSYM);
	}
	return (STAB_SUCCESS);
}

/*
 * emit_copy() - Emit the stab for a type object that was created by a
 *	type_copy() operation in stab.c.
 */
static stabsret_t
emit_copy(typedesc_t td, type_t *type)
{
	char stabline[MAX_STAB_STR_LEN];
	char *s = stabline;
	size_t r = sizeof (stabline);	/* remainder */
	char *name;
	size_t sz;
	stabsret_t ret;

	if ((ret = string_offset2ptr(type->t_name, &name)) != STAB_SUCCESS) {
		return (ret);
	}
	sz = snprintf(s, r, "%s:t(0,%u)", name, td);

	if (r > sz) {
		s += sz;
		r -= sz;
	} else {
		return (STAB_FAIL);
	}

	sz = put_subtype(s, r, td, type, TS_EMITTED);
	if (sz > r) {
		return (STAB_FAIL);
	}

	return (print_stab(stabline, NOT_CONTINUED));
}

/*
 * emit_bf() - emit basic or float type.
 */
static stabsret_t
emit_bf(typedesc_t td, type_t *type)
{
	char stabline[MAX_STAB_STR_LEN];
	char *s = stabline;
	size_t r = sizeof (stabline);	/* remainder */
	stabsret_t ret;
	char *name;
	size_t sz;

	if ((ret = string_offset2ptr(type->t_name, &name)) != STAB_SUCCESS) {
		return (ret);
	}

	if (type->t_code & TS_BASIC) {	/* Basic type. */
		btype_t *bt = &type->t_tinfo.ti_bt;

		/*
		 * Display is optional.  The parser guarantees that if
		 * display was not defined then it is '\0'.
		 */
		if (bt->bt_display == '\0') {
			sz = snprintf(s, r, "%s:t(0,%u)=b%c%u;%u;%u",
			    name, td, bt->bt_sign,
			    bt->bt_width, bt->bt_offset, bt->bt_nbits);
		} else {
			sz = snprintf(s, r, "%s:t(0,%u)=b%c%c%u;%u;%u",
			    name, td, bt->bt_sign, bt->bt_display,
			    bt->bt_width, bt->bt_offset, bt->bt_nbits);
		}
		/* Claim this types to be emitted. */
		type->t_code |= TS_EMITTED;


	} else if (type->t_code & TS_FLOAT) {	/* Float type. */
		ftype_t *ft = &type->t_tinfo.ti_ft;

		sz = snprintf(s, r, "%s:t(0,%u)=R%u;%u",
		    name, td, ft->ft_format, ft->ft_nbytes);

		/* Claim this types to be emitted. */
		type->t_code |= TS_EMITTED;

	} else {
		return (STAB_FAIL);
	}

	if (sz > r) {
		return (STAB_FAIL);
	}

	return (print_stab(stabline, NOT_CONTINUED));
}

/*
 * emit_enum() - emit an enum stab.
 *	May split to a new stab on a member boundary.
 */
static stabsret_t
emit_enum(typedesc_t td, type_t *type)
{
	char stabline[MAX_STAB_STR_LEN];
	char *s = stabline;
	size_t r = sizeof (stabline);	/* remainder */
	stabsret_t ret;
	char *name;
	size_t sz;
	etype_t *et;
	type_t *ntype;

	if (type->t_name == SO_NOSTRING) {
		/* It is anonymous. */
		sz = snprintf(s, r, ":T(0,%u)=e", td);
	} else {
		ret = string_offset2ptr(type->t_name, &name);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
		/* Strip the word enum. */
		name += sizeof ("enum");
		sz = snprintf(s, r, "%s:T(0,%u)=e", name, td);
	}

	if (r > sz) {
		s += sz;
		r -= sz;
	} else {
		return (STAB_FAIL);
	}

	type->t_code |= TS_EMITTED;

	et = &type->t_tinfo.ti_et;

	while (et->et_next != TD_NOTYPE) {
		ret = ttable_td2ptr(et->et_next, &ntype);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
		ret = string_offset2ptr(ntype->t_name, &name);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}

		sz = snprintf(s, r, "%s:%d,",
		    name, ntype->t_tinfo.ti_et.et_number);

		/*
		 * If a continuation must happen then it should happen
		 * at a sensible boundary.
		 */
		if (sz + 2 > r) {
			/* Truncate */
			*s = '\0';
			ret = print_stab(stabline, CONTINUED);
			if (ret != STAB_SUCCESS) {
				return (ret);
			}
			/* Do it again. */
			s = stabline;
			r = sizeof (stabline);
		} else {
			et = &ntype->t_tinfo.ti_et;
			s += sz;
			r -= sz;
			ntype->t_code |= TS_EMITTED;
		}
	}

	/* Terminate the stab. */
	*s++ = ';';
	*s = '\0';

	return (print_stab(stabline, NOT_CONTINUED));
}

/*
 * put_member() - used by emit_su() to stuff the stab of struct or union
 *	members.
 *
 * NOTE:
 *	Function can process member types with or without emitting the stab
 *	strings.  This behaviour is controlled by the <emit> arg.
 */
static size_t
put_member(char *sl, size_t slsz, char *s, size_t r, size_t sum,
    typedesc_t td, tcode_t emit)
{
	type_t *type;
	mtype_t *mt;
	char *name;
	char *snip;
	size_t sz;
	stabsret_t ret;

	while (td != TD_NOTYPE) {
		/* Resolve members here. */
		if ((ret = ttable_td2ptr(td, &type)) != STAB_SUCCESS) {
			(void) fprintf(stderr, "put_member: XXX"
			    "Failed getting type of %u ret = %d\n",
			    td, ret);
		}
		ret = string_offset2ptr(type->t_name, &name);
		if (ret != STAB_SUCCESS) {
			(void) fprintf(stderr, "put_member: XXX"
			    "Failed getting name of %u ret = %d\n",
			    td, ret);
		}
		mt = &type->t_tinfo.ti_mt;

		snip = s;

		/* Place member name. */
		sz = snprintf(s, r, "%s:(0,%u)",
		    name, mt->mt_td);
		if (r > sz) {
			s += sz;
			r -= sz;
		}
		sum += sz;

		type->t_code |= emit;

		if ((ret = ttable_td2ptr(mt->mt_td, &type)) != STAB_SUCCESS) {
			(void) fprintf(stderr, "put_member: XXX"
			    "Failed getting type of %u ret = %d\n",
			    td, ret);
		}

		if ((type->t_code & TS_EMITTED) == 0) {
			sz = put_subtype(s, r, mt->mt_td, type, emit);
			if (r > sz) {
				s += sz;
				r -= sz;
			}
			sum += sz;
		}

		sz = snprintf(s, r, ",%u,%u;",
		    mt->mt_bitoffset, mt->mt_bitsize);

		sum += sz;

		if (sum + 2 >= slsz &&
		    emit == TS_EMITTED) {
			/* Truncate */
			*snip = '\0';
			if ((ret = print_stab(sl, CONTINUED)) != STAB_SUCCESS) {
				return (ret);
			}
			/* Do it again */
			s = sl;
			r = slsz;
			sum = 0;
		} else {
			td = mt->mt_next;
			s += sz;
			r -= sz;
			type->t_code |= emit;
		}
	}

	return (sum);
}

/*
 * emit_su() - emit stab for a struct or union.
 */
static stabsret_t
emit_su(typedesc_t td, type_t *type)
{
	char stabline[MAX_STAB_STR_LEN];
	char *s = stabline;
	size_t r = sizeof (stabline);	/* remainder */
	size_t sum;
	stabsret_t ret;
	char *name;
	size_t sz;
	stype_t *st;
	char su;
	size_t su_off;

	if (type->t_code & TS_STRUCT) {
		su = 's';
		su_off = sizeof ("struct");
	} else if (type->t_code & TS_UNION) {
		su = 'u';
		su_off = sizeof ("union");
	} else {
		return (STAB_FAIL);
	}

	st = &type->t_tinfo.ti_st;

	if (type->t_name == SO_NOSTRING) {
		/* It is anonymous. */
		sz = snprintf(s, r, ":T(0,%u)=%c%u",
		    td, su, st->st_size);
	} else {
		ret = string_offset2ptr(type->t_name, &name);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
		/* stript the word enum */
		name += su_off;
		sz = snprintf(s, r, "%s:T(0,%u)=%c%u",
		    name, td, su, st->st_size);
	}

	if (r > sz) {
		s += sz;
		r -= sz;
	} else {
		return (STAB_FAIL);
	}
	sum = sz;

	type->t_code |= TS_EMITTED;

	/*
	 * Process all members before emitting stabs for them.
	 * This is necessary so that stabs that this struct or union
	 * depends on are emited before any part of this stab is.
	 *
	 * We must go through every member first in case there is
	 * a continuation.
	 */
	(void) put_member(stabline, sizeof (stabline), s, r, sum,
	    st->st_first, TS_UNDEF);

	/* Do it for real this time. */
	sum = put_member(stabline, sizeof (stabline), s, r, sum,
	    st->st_first, TS_EMITTED);

	s = stabline + sum;

	/* Terminate the stab. */
	*s++ = ';';
	*s = '\0';

	return (print_stab(stabline, NOT_CONTINUED));
}

/*
 * put_forward() - places a forward reference into the supplied string.
 */
static size_t
put_forward(char *s, size_t r, typedesc_t td, type_t *type, tcode_t emit)
{
	xtype_t *xt;
	char *xname;
	char esu;
	stabsret_t ret;
	size_t esu_offset;

	xt = &type->t_tinfo.ti_xt;


	if (type->t_code & TS_ENUM) {
		esu = 'e';
		esu_offset = sizeof ("enum");
	} else if (type->t_code & TS_UNION) {
		esu = 'u';
		esu_offset = sizeof ("union");

	} else if (type->t_code & TS_STRUCT) {
		esu = 's';
		esu_offset = sizeof ("struct");
	} else {
		return (STAB_FAIL);
	}
	if ((ret = string_offset2ptr(xt->xt_name, &xname)) != STAB_SUCCESS) {
		(void) fprintf(stderr, "put_forward: XXX"
		    "Failed getting name of %u ret = %d\n",
		    td, ret);
	}
	xname += esu_offset;

	type->t_code |= emit;

	return (snprintf(s, r, "=x%c%s:", esu, xname));
}

/*
 * emit_forward() - emit a stab for a forward reference.
 */
static stabsret_t
emit_forward(typedesc_t td, type_t *type)
{
	xtype_t *xt;
	type_t *ntype;
	char *name;
	stabsret_t ret;
	char stabline[MAX_STAB_STR_LEN];
	char *s = stabline;
	size_t r = sizeof (stabline);	/* remainder */
	size_t sz;

	if ((ret = string_offset2ptr(type->t_name, &name)) != STAB_SUCCESS) {
		return (ret);
	}

	sz = snprintf(s, r, "%s:t(0,%u)", name, td);
	if (r > sz) {
		s += sz;
		r -= sz;
	} else {
		return (STAB_FAIL);
	}

	xt = &type->t_tinfo.ti_xt;

	if (xt->xt_td != TD_NOTYPE) {
		if ((ret = ttable_td2ptr(xt->xt_td, &ntype)) != STAB_SUCCESS) {
			return (ret);
		}

		if ((ntype->t_code & TS_EMITTED) == 0) {
			if (ntype->t_code & TS_ENUM) {
				ret = emit_enum(xt->xt_td, ntype);
			} else if (ntype->t_code & (TS_STRUCT | TS_UNION)) {
				ret = emit_su(xt->xt_td, ntype);
			} else {
				return (STAB_FAIL);
			}
		}
	}
	sz = put_forward(s, r, td, type, TS_EMITTED);

	ret = print_stab(stabline, NOT_CONTINUED);
	type->t_code |= TS_EMITTED;
	return (ret);
}

/*
 * put_pointer() - place a pointer stab in the given buffer.
 */
static size_t
put_pointer(char *s, size_t r, type_t *type, tcode_t emit)
{
	size_t sz;
	size_t sum;
	stabsret_t ret;
	typedesc_t ntd;
	type_t *ntype;

	ntd = type->t_tinfo.ti_pt.pt_td;

	sz = snprintf(s, r, "=*(0,%u)", ntd);
	sum = sz;

	if ((ret = ttable_td2ptr(ntd, &ntype)) != STAB_SUCCESS) {
		(void) fprintf(stderr, "put_forward: XXX"
		    "Failed getting name of %u ret = %d\n",
		    ntd, ret);
	}

	if ((ntype->t_code & TS_EMITTED) == 0) {
		if (r > sz) {
			s += sz;
			r -= sz;
		}
		sz = put_subtype(s, r, ntd, ntype, emit);
		sum += sz;
	}
	return (sum);
}

/*
 * put_copy() - place the stab from a type_copy() operation into the
 *	given buffer.
 */
static size_t
put_copy(char *s, size_t r, type_t *type, tcode_t emit)
{
	type_t *ntype;
	size_t sz;
	size_t sum = 0;
	stabsret_t ret;

	if (type->t_code & TS_TYPEDEF) {
		sz = snprintf(s, r, "=(0,%u)", type->t_alias);
	} else {
		char c;

		if (type->t_code & TS_CONST) {
			c = 'k';
		} else if (type->t_code & TS_VOLATILE) {
			c = 'B';
		} else if (type->t_code & TS_RESTRICTED) {
			c = 'K';
		} else if (type->t_code & TS_FUNC) {
			c = 'f';
		} else {
			return (STAB_FAIL);
		}
		sz = snprintf(s, r, "=%c(0,%u)", c, type->t_alias);
	}

	if (r > sz) {
		s += sz;
		r -= sz;
	}
	sum += sz;

	type->t_code |= emit;

	if ((ret = ttable_td2ptr(type->t_alias, &ntype)) != STAB_SUCCESS) {
		return (ret);
	}

	if ((ntype->t_code & TS_EMITTED) == 0) {
		sum += put_subtype(s, r, type->t_alias, ntype, emit);
	}

	return (sum);
}

/*
 * put_array() - place an array stab into the given buffer.
 */
static size_t
put_array(char *s, size_t r, type_t *type, tcode_t emit)
{
	type_t *ntype;
	size_t sum;
	size_t sz;
	stabsret_t ret;
	atype_t *at;

	at = &type->t_tinfo.ti_at;

	/* C arrays allways have a range type of int and a min of 0. */
	sz = snprintf(s, r, "=ar(0,3);0;%u;(0,%u)", at->at_range, at->at_td);
	if (r > sz) {
		s += sz;
		r -= sz;
	}
	sum = sz;

	if ((ret = ttable_td2ptr(at->at_td, &ntype)) != STAB_SUCCESS) {
		(void) fprintf(stderr, "XXX put of array failed %d\n", ret);
	}

	if ((ntype->t_code & TS_EMITTED) == 0) {
		sz = put_subtype(s, r, at->at_td, ntype, emit);
		if (r > sz) {
			s += sz;
			r -= sz;
		}
		sum += sz;
	}

	return (sum);
}

/*
 * emit_array() - emit an array stab.
 */
static stabsret_t
emit_array(typedesc_t td, type_t *type)
{
	char stabline[MAX_STAB_STR_LEN];
	char *s = stabline;
	size_t r = sizeof (stabline);	/* remainder */
	stabsret_t ret;
	char *name;
	size_t sz;

	if ((ret = string_offset2ptr(type->t_name, &name)) != STAB_SUCCESS) {
		return (ret);
	}

	type->t_code |= TS_EMITTED;

	sz = snprintf(s, r, "%s:t(0,%u)", name, td);

	if (r > sz) {
		s += sz;
		r -= sz;
	} else {
		return (STAB_FAIL);
	}
	sz = put_array(s, r, type, TS_EMITTED);

	return (print_stab(stabline, NOT_CONTINUED));
}

/*
 * put_subtype() - Given a type place the stab for it in a given buffer.
 *	This function makes decisions of how to emit the portion of the
 *	stab after the leading type has been placed in the buffer.
 */
static size_t
put_subtype(char *s, size_t r, typedesc_t td, type_t *type, tcode_t emit)
{
	size_t sz = 0;
	stabsret_t ret;

	if (type->t_code & TS_COPY) {
		sz = put_copy(s, r, type, emit);
	} else if (type->t_code & TS_POINTER) {
		sz = put_pointer(s, r, type, emit);
	} else if (type->t_code & TS_FORWARD) {
		if (type->t_code & TS_EMITTED ||
		    type->t_name == SO_NOSTRING) {
			sz = put_forward(s, r, td, type, emit);
		} else {
			/*
			 * if a name exists then
			 * emit whole forward reference
			 */
			ret = emit_forward(td, type);
			if (ret != STAB_SUCCESS) {
				(void) fprintf(stderr, "XXX emit of forward "
				    "ref failed for %u ret = %d\n", td, ret);
			}
		}
	} else if (type->t_code & TS_ENUM) {
		if ((ret = emit_enum(td, type)) != STAB_SUCCESS) {
			(void) fprintf(stderr, "XXX emit of enum failed %d\n",
			    ret);
		}
	} else if (type->t_code & (TS_STRUCT | TS_UNION)) {
		if ((ret = emit_su(td, type)) != STAB_SUCCESS) {
			(void) fprintf(stderr, "XXX put of su failed %d\n",
			    ret);
		}
	} else if (type->t_code & TS_ARRAY) {
		sz = put_array(s, r, type, emit);
	}
	return (sz);
}

/*
 * emit_stab() - every named type that has yet to be emitted starts here.
 */
static stabsret_t
emit_stab(typedesc_t td, type_t *type)
{
	stabsret_t ret;

	/* Make sure it is a defined type. */
	if (type->t_code == TS_UNDEF) {
		return (STAB_FAIL);
	}

	if (type->t_code & TS_FORWARD) {
		/*
		 * Most forward references should have been resolved.
		 * Only emit the ones that another type depends on.
		 */
		return (STAB_SUCCESS);
	} else if (type->t_code & (TS_COPY | TS_POINTER)) {
		/*
		 * Type objects that are pointers or were
		 *created by a copy must be handled next.
		 */
		ret = emit_copy(td, type);
	} else if (type->t_code & TS_ARRAY) {
		ret = emit_array(td, type);
	} else if (type->t_code & TS_MEMBER) {
		return (STAB_SUCCESS);
	} else if (type->t_code & TS_ENUM) {
		ret = emit_enum(td, type);
	} else if (type->t_code & (TS_STRUCT | TS_UNION)) {
		ret = emit_su(td, type);
	} else {
		/* Anything else is a basic or float type. */
		ret = emit_bf(td, type);
	}

	return (ret);
}

/*
 * dump_stabs() - cycle through the type table looking for named type.
 * 	If one is found and it has yet to be emitted then call
 *	emit_stab().
 */
void
dump_stabs(void)
{
	typedesc_t td = 1;
	type_t *type;

	while (ttable_td2ptr(td, &type) == STAB_SUCCESS) {
		if (type->t_name != SO_NOSTRING &&	/* Named. */
		    (type->t_code & TS_EMITTED) == 0) { /* Not emitted. */
			(void) emit_stab(td, type);
		}
		++td;
	}
}
