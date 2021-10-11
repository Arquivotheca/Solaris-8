/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)printer.c	1.2	99/06/04 SMI"

#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stab.h>
#include "stabspf_impl.h"

#define	STARS	"********************"
static char const *stars = STARS;

/* Maximum number of characters for print_smartdump */
#define	MAXSTRDUMP 40

static int get_root_type_of_pointer(FILE *, type_t *, type_t **,
    char **, char *, size_t, int *);


/*
 * The caller sets bitfield to the width of the datum if it is attempting
 * to print a bit field member.
 */
int
print_basic(FILE *stream, type_t *t, char *name, int bitfield, uint64_t val)
{
	int	retval = 0;
	char	*format;
	btype_t	*bt;
	char	*tname;

	assert(t->t_code & TS_BASIC);

	bt = &t->t_tinfo.ti_bt;

	if (string_offset2ptr(t->t_name, &tname) != STAB_SUCCESS)
		return (0);

	if (name != NULL)
		retval = fprintf(stream, "%s: ", name);

	if (bitfield != 0) {
		if (bt->bt_display == 'c')
			format = "(%1$s:%2$d) \'%5$c\'\t(0x%3$x)\n";
		else if (bt->bt_sign == 's')
			format = "(%s:%d) %lld\t(0x%llx)\n";
		else
			format = "(%s:%d) %llu\t(0x%llx)\n";
		retval += fprintf(stream, format,
		    tname, bitfield, val, val, (char)val);
	} else {
		if (bt->bt_display == 'c')
			format = "(%1$s) \'%4$c\'\t(0x%2$x)\n";
		else if (bt->bt_sign == 's')
			format = "(%s) %lld\t(0x%llx)\n";
		else
			format = "(%s) %llu\t(0x%llx)\n";
		retval += fprintf(stream, format, tname, val, val, (char)val);
	}

	return (retval);
}

int
print_float(FILE *stream, type_t *t, char *name, void const *val)
{
	int	retval = 0;
	ftype_t *ft;
	char	*tname;

	assert(t->t_code & TS_FLOAT);

	ft = &t->t_tinfo.ti_ft;

	if (string_offset2ptr(t->t_name, &tname) != STAB_SUCCESS)
		return (0);

	if (name != NULL)
		retval = fprintf(stream, "%s: ", name);

	switch (ft->ft_format) {
	case NF_SINGLE:
		retval += fprintf(stream, "(%s) %g\n", tname, *((float *)val));
		break;
	case NF_DOUBLE:
		retval += fprintf(stream,
		    "(%s) %g\n", tname, *((double *)val));
		break;
	case NF_LDOUBLE:
		retval += fprintf(stream, "(%s) %Lg\n",
		    tname, *((long double *)val));
		break;
	default:
		/* This default should never happen */
		retval += fprintf(stream, "(%s) Unknown float type: %d\n",
		    tname, ft->ft_format);
		break;
	}

	return (retval);
}

int
print_pointer(FILE *stream, type_t *t, int levels, char *name, void const *val)
{
	int	retval = 0;
	int	level = levels;
	char	cn[BUFSIZ];		/* Canonical name */
	size_t	cnlen = sizeof (cn);
	type_t	*rt;
	char	*rtname;

	/*
	 * If level is zero, then the caller does not know how many levels
	 * of indirection there are but does have a type_t for the pointer.
	 *
	 * Otherwise, we have a type_t for the base type and a number of
	 * levels of indirection.
	 */
	if (level == 0)
		(void) get_root_type_of_pointer(stream, t, &rt, &rtname, cn,
		    cnlen, &level);
	else {
		stroffset_t stroff;

		if (t->t_code & TS_FORWARD) {
			stroff = t->t_tinfo.ti_xt.xt_name;
		} else {
			stroff = t->t_name;
		}
		if (string_offset2ptr(stroff, &rtname) !=
		    STAB_SUCCESS) {
			(void) fprintf(stream,
			    "print_pointer: "
			    "lookup of type name failed\n");
			goto end;
		}
		if (level < sizeof (STARS))
			(void) snprintf(cn, cnlen,
			    "%s %.*s", rtname, level, stars);
		else
			/*
			 * The only reason we are here is that
			 * val has more than 20 levels of indirection!
			 */
			(void) snprintf(cn, cnlen,
			    "%s with %d *'s", rtname, level);
		rt = t;
	}

	/*
	 * If name, then we are a structure member and name
	 * is the member name.
	 */
	if (name != NULL)
		retval = fprintf(stream, "%s: ", name);

	retval += fprintf(stream, "(%s) ", cn);
	if (val == NULL)
		retval += fputs("NULL ", stream);
	else
		retval += fprintf(stream, "0x%p ", val);

	/*
	 * If we have only one level of indirection and the base
	 * type is char, then print as a string.
	 *
	 * Else if name is NULL (ie. not a structure member) AND
	 * val is not NULL, AND we have only one level of indirection
	 * AND the base type is a structure, then dive into print_struct.
	 */
	if ((level == 1) &&
	    (val != NULL) &&
	    (rt->t_code & TS_BASIC) &&
	    (rt->t_tinfo.ti_bt.bt_display == 'c')) {
		retval += print_smartdump(stream, MAXSTRDUMP, val);
	} else if ((name == NULL) &&
	    (val != NULL) &&
	    (level == 1) &&
	    ((rt->t_code & (TS_STRUCT | TS_UNION)))) {
		retval += print_struct(stream, rt, name, 0, val);
	}
	retval += fputs("\n", stream);
end:
	return (retval);
}

/*
 * At the time of this writing, a 20 dimensional array seems very unlikely.
 * Thus if you are reading this because 20 is not big enough, then my my my
 * times have changed.
 */
#define	MAXRANGES 20

int
print_array(FILE *stream, type_t *t, char *name, void const *val)
{
	int	retval = 0;
	char	*tname;
	int	dim = 0, i;
	int	dimranges[MAXRANGES];

	assert(t->t_code & TS_ARRAY);

	dimranges[dim++] = t->t_tinfo.ti_at.at_range + 1;

	for (;;) {
		if (ttable_td2ptr(t->t_tinfo.ti_at.at_td, &t) != STAB_SUCCESS)
			return (0);
		if (t->t_code & TS_ARRAY)
			dimranges[dim++] = t->t_tinfo.ti_at.at_range + 1;
		else
			break;
	}

	if (string_offset2ptr(t->t_name, &tname) != STAB_SUCCESS)
		return (0);

	if (name != NULL)
		retval = fprintf(stream, "%s: ", name);

	retval += fprintf(stream, "(%s", tname);
	for (i = 0; i < dim; i++)
		retval += fprintf(stream, "[%d]", dimranges[i]);
	retval += fprintf(stream, ") 0x%p\n", val);

	return (retval);
}

int
print_enum(FILE *stream, type_t *t, char *name, int *val)
{
	int	retval = 0;
	type_t	*mtp = t;
	etype_t	*et;
	char	*ename;
	char	*valname = NULL;
	stroffset_t stroff;

	assert(t->t_code & TS_ENUM);

	if (t->t_code & TS_FORWARD) {
		stroff = t->t_tinfo.ti_xt.xt_name;
	} else {
		stroff = t->t_name;
	}

	if (string_offset2ptr(stroff, &ename) != STAB_SUCCESS)
		goto end;

	et = &mtp->t_tinfo.ti_et;

	while (et->et_next != TD_NOTYPE) {
		if (ttable_td2ptr(et->et_next, &mtp) != STAB_SUCCESS)
			goto end;
		assert(mtp->t_code & TS_MEMBER);
		if (et->et_number == *val) {
			(void) string_offset2ptr(mtp->t_name, &valname);
			break;
		}
		et = &mtp->t_tinfo.ti_et;
	}

	if (name != NULL)
		retval = fprintf(stream, "%s: ", name);

	retval += fprintf(stream, "(%s) enum [%s] (%d)\n",
	    ename,
	    (valname == NULL) ? "UNKNOWN" : valname,
	    *val);

end:
	return (retval);
}

static int
print_indent(FILE *stream, int level)
{
	char *p = "";

	if (level)
		return (fprintf(stream, "%*s", (level * 4), p));

	return (0);
}

int
print_struct(FILE *stream, type_t *t, char *name, int level, void const *val)
{
	int		retval = 0;
	type_t		*mp = t;	/* Member Pointer */
	type_t		*mtp;		/* Member Type Pointer */
	char		*mname;		/* Member Name */
	char		*suname;	/* Struct/Union Name */
	uintptr_t	s = (uintptr_t)val;
	int		align;
	uint64_t	basic;
	float		fval;
	double		dval;
	long double	ldval;
	uint_t		bitoffset, bitsize;
	/* Short cuts */
	mtype_t		*mps;		/* Member List */
	typedesc_t	next_td;
	ftype_t		*ft;

	assert((t->t_code & (TS_STRUCT | TS_UNION)));

	/* May be anon struct / union */
	if (string_offset2ptr(t->t_name, &suname) != STAB_SUCCESS) {
		if (t->t_code & TS_STRUCT)
			suname = "struct";
		else
			suname = "union";
	}

	if (name != NULL)
		retval += fprintf(stream, "%s: ", name);

	if (check_addr(val, t->t_tinfo.ti_st.st_size) <= 0) {
		retval += fputs("(Invalid address)\n", stream);
		goto end;
	}

	if (t->t_code & TS_FORWARD) {
		retval += fprintf(stream, "(%s) { Forward Reference }\n",
		    suname);
		goto end;
	}

	retval += fprintf(stream, "(%s) {\n", suname);

	++level;

	next_td = t->t_tinfo.ti_st.st_first;

	do {
		/* Get the type_t for the next member */
		if (ttable_td2ptr(next_td, &mp) != STAB_SUCCESS)
			goto end;
		assert(mp->t_code & TS_MEMBER);

		/* Get the member name */
		if (string_offset2ptr(mp->t_name, &mname) != STAB_SUCCESS)
			goto end;

		mps = &mp->t_tinfo.ti_mt;

		/* Get the type_t for the type of the member */
		if (ttable_td2ptr(mps->mt_td, &mtp) != STAB_SUCCESS)
			goto end;

		bitoffset = mps->mt_bitoffset;
		s = ((uintptr_t)val + (bitoffset >> 3));

		retval += print_indent(stream, level);

		if (mtp->t_code & TS_BASIC) {
			align = bitoffset % mtp->t_tinfo.ti_bt.bt_nbits;
			s = (uintptr_t)val + ((bitoffset - align) >> 3);
			bitsize = mps->mt_bitsize;

			switch (mtp->t_tinfo.ti_bt.bt_nbits) {
			case 8:
				basic = *((uint8_t *)s);
				break;
			case 16:
				basic = *((uint16_t *)s);
				break;
			case 32:
				basic = *((uint32_t *)s);
				break;
			case 64:
				basic = *((uint64_t *)s);
				break;
			default:
				/* Perfectly normal */
				break;
			}

			if (align != 0) {
				/*
				 * Not pretty but functional so far
				 */
#if defined(__sparc)
				int shiftval;
				uint64_t mask;

				shiftval = mtp->t_tinfo.ti_bt.bt_nbits -
				    (bitoffset - (bitoffset - align)) -
				    bitsize;
				basic >>= shiftval;
				mask = (1 << (bitsize)) - 1;
				basic &= mask;
#elif defined(__i386)
				basic >>= align;
				basic <<= (sizeof (basic) * 8) - bitsize;
				basic >>= (sizeof (basic) * 8) - bitsize;
#else
#error	"ISA unsupported"
#endif
			}

			retval += print_basic(stream, mtp, mname,
			    align != 0 ? bitsize : 0, basic);
		} else if (mtp->t_code & TS_FLOAT) {
			ft = &mtp->t_tinfo.ti_ft;
			switch (ft->ft_format) {
			case NF_SINGLE:
				fval = *((float *)s);
				retval += print_float(stream, mtp, mname,
				    &fval);
				break;
			case NF_DOUBLE:
				dval = *((double *)s);
				retval += print_float(stream, mtp, mname,
				    &dval);
				break;
			case NF_LDOUBLE:
				ldval = *((long double *)s);
				retval += print_float(stream, mtp, mname,
				    &ldval);
				break;
			}
		} else if (mtp->t_code & TS_ARRAY) {
			retval += print_array(stream, mtp, mname, (void *)s);
		} else if (mtp->t_code & TS_ENUM) {
			retval += print_enum(stream, mtp, mname, (int *)s);
		} else if (mtp->t_code & TS_POINTER) {
			retval += print_pointer(stream, mtp, 0,
			    mname, *((void **)s));
		} else if ((mtp->t_code & (TS_STRUCT | TS_UNION))) {
			retval += print_struct(stream, mtp, mname,
			    level, (void *)s);
		}
		next_td = mps->mt_next;
	} while (next_td != TD_NOTYPE);
	retval += print_indent(stream, --level);
	retval += fputs("}\n", stream);
end:
	return (retval);
}

static int
get_root_type_of_pointer(FILE *stream, type_t *tp, type_t **rtp, char **rtname,
    char *cname, size_t cnamelen, int *levels)
{
	type_t *ntp;
	int retval = 0;
	ptype_t	*pt;

	/* First time */
	if (*levels == 0)
		*levels = 1;

	if (tp->t_code & TS_POINTER) {
		pt = &tp->t_tinfo.ti_pt;
		if (ttable_td2ptr(pt->pt_td, &ntp) != STAB_SUCCESS) {
			/* Should never happen */
			(void) fprintf(stderr, "get_root_type_of_pointer: "
			    "lookup of referred type failed\n");
			goto end;
		}

		if (ntp->t_code & TS_POINTER) {
			(*levels)++;
			retval = get_root_type_of_pointer(stream, ntp, rtp,
			    rtname, cname, cnamelen, levels);
		} else {
			stroffset_t stroff;

			if (ntp->t_code & TS_FORWARD) {
				stroff = ntp->t_tinfo.ti_xt.xt_name;
			} else {
				stroff = ntp->t_name;
			}

			if (string_offset2ptr(stroff,
			    rtname) != STAB_SUCCESS) {
				if (ntp->t_code & TS_STRUCT)
					*rtname = "struct";
				else if (ntp->t_code & TS_UNION)
					*rtname = "union";
				else
					*rtname = "enum";
			}
			if (ntp->t_code & TS_FUNC) {
				if (*levels <= sizeof (STARS))
					retval = snprintf(cname, cnamelen,
					    "%s (%.*s)()",
					    *rtname, *levels, stars);
				else
					retval = snprintf(cname, cnamelen,
					    "func returning %s with %d *'s",
					    *rtname, *levels);
			} else if (*levels <= sizeof (STARS)) {
				retval = snprintf(cname, cnamelen,
				    "%s %.*s", *rtname, *levels, stars);
			} else {
				retval = snprintf(cname, cnamelen,
				    "%s with %d *'s", *rtname, *levels);
			}
			*rtp = ntp;
			goto end;
		}
	}

end:
	return (retval);
}
