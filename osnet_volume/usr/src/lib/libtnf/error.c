/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma	ident	"@(#)error.c	1.11	95/01/26 SMI"

#include "libtnf.h"
#include <libintl.h>

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

/*
 *
 */

static tnf_error_handler_t *_err_handler = &tnf_default_error_handler;
static void *_err_arg = 0;

/*
 *
 */

void
tnf_set_error_handler(tnf_error_handler_t *handler, void *arg)
{
	/* XXX Not MT-safe */
	_err_arg = arg;
	_err_handler = handler;
}

/*
 *
 */

void
_tnf_error(TNF *tnf, tnf_errcode_t err)
{
	(*_err_handler)(_err_arg, tnf, err);
}

/*
 *
 */

char *
tnf_error_message(tnf_errcode_t err)
{
	if (err == TNF_ERR_NONE)
		return (dgettext(TEXT_DOMAIN, "no error"));
	else if (err <= TNF_ERRNO_MAX)
		return (strerror(err));
	else {
		switch (err) {
		case TNF_ERR_NOTTNF:
			return (dgettext(TEXT_DOMAIN, "not a TNF file"));
		case TNF_ERR_BADDATUM:
			return (dgettext(TEXT_DOMAIN,
				"operation on bad or NULL data handle"));
		case TNF_ERR_TYPEMISMATCH:
			return (dgettext(TEXT_DOMAIN, "type mismatch"));
		case TNF_ERR_BADINDEX:
			return (dgettext(TEXT_DOMAIN, "index out of bounds"));
		case TNF_ERR_BADSLOT:
			return (dgettext(TEXT_DOMAIN, "no such slot"));
		case TNF_ERR_BADREFTYPE:
			return (dgettext(TEXT_DOMAIN, "bad reference type"));
		case TNF_ERR_ALLOCFAIL:
			return (dgettext(TEXT_DOMAIN,
				"memory allocation failure"));
		case TNF_ERR_BADTNF:
			return (dgettext(TEXT_DOMAIN, "bad TNF file"));
		case TNF_ERR_INTERNAL:
			return (dgettext(TEXT_DOMAIN, "internal error"));
		default:
			return (dgettext(TEXT_DOMAIN, "unknown error code"));
		}
	}
}

/*
 *
 */

void
/* ARGSUSED */
tnf_default_error_handler(void *arg, TNF *tnf, tnf_errcode_t err)
{
	fprintf(stderr, dgettext(TEXT_DOMAIN, "error: libtnf: %d: %s\n"),
		err, tnf_error_message(err));
	abort();
}
