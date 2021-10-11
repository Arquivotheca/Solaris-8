/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)FN_search_filter.cc	1.1	96/03/31 SMI"

#include <xfn/FN_search_filter.hh>
#include <xfn/FN_attrvalue.h>
#include <xfn/FN_attribute.h>
#include <xfn/FN_identifier.h>
#include <xfn/FN_string.h>
#include <xfn/FN_status.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


class FN_search_filter_rep {
public:
	size_t num_args;
	unsigned char *expression;
	FN_search_filter_type *argtypes;
	void **args;

	FN_search_filter_rep(unsigned int &status,
			    const unsigned char *estr,
			    va_list args);
	FN_search_filter_rep(unsigned int &status,
			    const unsigned char *estr,
			    FN_search_filter_type *types,
			    void **args);
	~FN_search_filter_rep();

	FN_search_filter_rep& operator=(const FN_search_filter_rep &);
};

/* some useful utilities */
static
FN_attrvalue_t *
fn_attrvalue_copy(const FN_attrvalue_t *orig)
{
	FN_attrvalue_t *answer;
	void *origcon;

	answer = (FN_attrvalue_t *)malloc(sizeof (FN_attrvalue_t));
	origcon = malloc(orig->length);

	if (answer == 0 || origcon == 0) {
		free(answer);
		free(origcon);
		return (0);
	}
	answer->length = orig->length;
	answer->contents = origcon;

	memcpy(origcon, orig->contents, orig->length);

	return (answer);
}

static
FN_identifier_t *
fn_identifier_copy(const FN_identifier_t *orig)
{
	FN_identifier_t *answer;
	void *origcon;

	answer = (FN_identifier_t *)malloc(sizeof (FN_identifier_t));
	origcon = malloc(orig->length);

	if (answer == 0 || origcon == 0) {
		free(answer);
		free(origcon);
		return (0);
	}
	answer->format = orig->format;
	answer->length = orig->length;
	answer->contents = origcon;

	memcpy(origcon, orig->contents, orig->length);

	return (answer);
}

static void
fn_attrvalue_destroy(FN_attrvalue_t *attrval)
{
	if (attrval) {
		free(attrval->contents);
		free(attrval);
	}
}

static void
fn_identifier_destroy(FN_identifier_t *id)
{
	if (id) {
		free(id->contents);
		free(id);
	}
}

static void
arglist_destroy(size_t nargs, FN_search_filter_type *types, void **targs)
{
	size_t i;

	for (i = 0; i < nargs; i++) {
		switch (types[i]) {
		case FN_SEARCH_FILTER_ATTR:
			fn_attribute_destroy((FN_attribute_t *)targs[i]);
			break;
		case FN_SEARCH_FILTER_ATTRVALUE:
			fn_attrvalue_destroy((FN_attrvalue_t *)targs[i]);
			break;
		case FN_SEARCH_FILTER_STRING:
			fn_string_destroy((FN_string_t *)targs[i]);
			break;
		case FN_SEARCH_FILTER_IDENTIFIER:
			fn_identifier_destroy((FN_identifier_t *)targs[i]);
			break;
		}
	}
	free(types);
	free(targs);
}


/*
 * This function initializes arglst, to contain the appropriate va_list values
 * for the first MAXARGS arguments.
 * Return status code indicating status of operation.
 */

#define	MAXARGS	30	/* max. number of args for fast positional paramters */

static unsigned int
_mkarglist(const char *fmt, va_list args,
    size_t &nargs,
    FN_search_filter_type *&types,
    void **&targs)
{
	FN_search_filter_type typelst[MAXARGS], curtype;
	size_t i, curargno;
	unsigned int error_encountered = 0;
	const FN_attribute_t *attr;
	const FN_attrvalue_t *attrval;
	const FN_string_t *str;
	const FN_identifier_t *id;

	/*
	* Algorithm	1. set all argument types to zero.
	*		2. walk through fmt putting arg types in typelst[].
	*		3. walk through args using va_arg(args, typelst[n])
	*		   and set targs[] and types[]to the appropriate values.
	*/

	(void) memset((void *)typelst, 0, sizeof (typelst));
	curargno = 0;
	while (error_encountered == 0 && ((fmt = strchr(fmt, '%')) != 0)) {
		fmt++;	/* skip % */
		switch (*fmt++)	{
		case 'a':
			curtype = FN_SEARCH_FILTER_ATTR;
			break;
		case 'v':
			curtype = FN_SEARCH_FILTER_ATTRVALUE;
			break;
		case 's':
			curtype = FN_SEARCH_FILTER_STRING;
			break;
		case 'i':
			curtype = FN_SEARCH_FILTER_IDENTIFIER;
			break;
		default:
			error_encountered = 1;
			break;
		}
		if (curargno < MAXARGS) {
			typelst[curargno] = curtype;
		}
		curargno++;	/* default to next in list */
	}

	if (curargno == 0 || error_encountered == 1) {
		targs = 0;
		types = 0;
		nargs = 0;
		if (error_encountered == 1)
			return (FN_E_SEARCH_INVALID_FILTER);
		else
			return (FN_SUCCESS); /* no '%' encountered */
	}

	types = (FN_search_filter_type *)malloc(curargno *
	    (sizeof (FN_search_filter_type)));
	targs = (void **)malloc(curargno * sizeof (void *));
	nargs = curargno;

	if (types == 0 || targs == 0) {
		free(types);
		free(targs);
		targs = 0;
		types = 0;
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	for (i = 0; i < curargno; i++) {
		types[i] = typelst[i];

		switch (typelst[i]) {
		case FN_SEARCH_FILTER_ATTR:
			attr = va_arg(args, const FN_attribute_t *);
			if (attr == 0)
				targs[i] = 0;
			else
				targs[i] = (void *)fn_attribute_copy(attr);
			break;
		case FN_SEARCH_FILTER_ATTRVALUE:
			attrval = va_arg(args, const FN_attrvalue_t *);
			if (attrval == 0)
				targs[i] = 0;
			else
				targs[i] = (void *)fn_attrvalue_copy(attrval);
			break;
		case FN_SEARCH_FILTER_STRING:
			str = va_arg(args, const FN_string_t *);
			if (str == 0)
				targs[i] = 0;
			else
				targs[i] = (void *)fn_string_copy(str);
			break;
		case FN_SEARCH_FILTER_IDENTIFIER:
			id = va_arg(args, const FN_identifier_t *);
			if (id == 0)
				targs[i] = 0;
			else
				targs[i] = (void *)fn_identifier_copy(id);
			break;
		/* should not be errors -- we set types ourselves earlier */
		}

		if (targs[i] == 0) {
			/* less arguments that requested by expression */
			error_encountered = 1;
			nargs = i;
			goto cleanup;
		}
	}

	/*
	 * if we want to be very careful, could check for extraneous
	 * arguments at this point by calling va_args() again to
	 * see if there is anything left over and report left overs
	 * with FN_E_SEARCH_INVALID_FILTER.
	 */
	va_end(args);

	if (error_encountered == 0)
		return (FN_SUCCESS);
cleanup:
	arglist_destroy(nargs, types, targs);
	types = 0;
	targs = 0;
	nargs = 0;
	return (FN_E_SEARCH_INVALID_FILTER);
}



FN_search_filter_rep::FN_search_filter_rep(unsigned int &status,
    const unsigned char *estr,
    va_list ap)
{
	expression = (unsigned char *)strdup((const char *)estr);
	status = _mkarglist((const char *)estr, ap, num_args, argtypes, args);
}

FN_search_filter_rep::FN_search_filter_rep(unsigned int &status,
					    const unsigned char *estr,
					    FN_search_filter_type *types,
					    void **vargs)
{
	expression = (unsigned char *)strdup((const char *)estr);
	argtypes = types;
	args = vargs;
	status = (expression != 0);
}




FN_search_filter_rep::~FN_search_filter_rep()
{
	delete expression;

	arglist_destroy(num_args, argtypes, args);
}

FN_search_filter_rep &
FN_search_filter_rep::operator=(const FN_search_filter_rep& r)
{
	size_t i;
	void *thisarg;

	if (&r == this)
		return (*this);

	expression = (unsigned char *)strdup((const char *)r.expression);

	args = (void **)malloc(r.num_args * sizeof (void *));
	argtypes = (FN_search_filter_type *)
		malloc(r.num_args * sizeof (FN_search_filter_type));

	if (args == 0 || argtypes == 0) {
		free(args);
		free(argtypes);
		args = 0;
		argtypes = 0;
		return (*this);
	}
	for (i = 0; i < r.num_args; i++) {
		thisarg = args[i];
		argtypes[i] = r.argtypes[i];
		switch (r.argtypes[i]) {
		case FN_SEARCH_FILTER_ATTR:
			args[i] = (void *)
			fn_attribute_copy((const FN_attribute_t *)thisarg);
			break;
		case FN_SEARCH_FILTER_ATTRVALUE:
			args[i] = (void *)
			fn_attrvalue_copy((const FN_attrvalue_t *)thisarg);
			break;
		case FN_SEARCH_FILTER_STRING:
			args[i] =
			(void *)fn_string_copy((const FN_string_t *)thisarg);
			break;
		case FN_SEARCH_FILTER_IDENTIFIER:
			args[i] = (void *)
			fn_identifier_copy((const FN_identifier_t *)thisarg);
			break;
		default:
			args[i] = 0;
		}

		if (args[i] == 0) {
			/* less arguments that requested by expression */
			arglist_destroy(i-1, argtypes, args);
			argtypes = 0;
			args = 0;
			num_args = 0;
			return (*this);
		}

	}
	return (*this);
}

FN_search_filter::FN_search_filter(FN_search_filter_rep* r)
	: rep(r)
{
}

FN_search_filter_rep *
FN_search_filter::get_rep(const FN_search_filter& s)
{
	return (s.rep);
}

FN_search_filter::FN_search_filter(unsigned int &status,
	const unsigned char *estr,
	...)
{
	va_list iargs;

	va_start(iargs, estr);
	rep = new FN_search_filter_rep(status, estr, iargs);
	va_end(iargs);
}

FN_search_filter::FN_search_filter(unsigned int &status,
    const unsigned char *estr,
    va_list args)
{
	rep = new FN_search_filter_rep(status, estr, args);
}

FN_search_filter::FN_search_filter(unsigned int &status,
    const unsigned char *estr,
    FN_search_filter_type *types, void **args)
{
	rep = new FN_search_filter_rep(status, estr, types, args);
}

FN_search_filter::~FN_search_filter()
{
	delete rep;
}

// copy and assignment

FN_search_filter::FN_search_filter(const FN_search_filter& s)
{
	rep = new FN_search_filter_rep(*get_rep(s));
}

FN_search_filter&
FN_search_filter::operator=(const FN_search_filter& s)
{
	if (&s != this) {
		*rep = *get_rep(s);
	}
	return (*this);
}

const unsigned char *
FN_search_filter::filter_expression(void) const
{
	return (rep->expression);
}

const void **
FN_search_filter::filter_arguments(size_t *num_args) const
{
	if (num_args)
		*num_args = rep->num_args;

	return ((const void **)(rep->args));
}

const FN_search_filter_type *
FN_search_filter::filter_argument_types(size_t *num_args) const
{
	if (num_args)
		*num_args = rep->num_args;

	return (rep->argtypes);
}
