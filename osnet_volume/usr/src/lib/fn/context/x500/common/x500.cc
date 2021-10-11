/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)x500.cc	1.1	96/03/31 SMI"


#include "X500Context.hh"


/*
 * Entry point into the X500Context module (shared object)
 */


#ifdef DEBUG
static void
x500_trace(
	char	*message
)
{
	char		time_string[32];
	const time_t	time_value = time((time_t *)0);

	cftime(time_string, "%c", &time_value);
	(void) fprintf(stderr, "%s (fns x500) %s\n", time_string, message);
}
#endif


extern "C"
FN_ctx_svc_t *
ASx500(
	const FN_ref_addr_t	*caddr,
	const FN_ref_t		*cref,
	unsigned int		authoritative,
	FN_status_t		*cs)
{
	int			err = 0;
	FN_status		*s = (FN_status *)cs;
	const FN_ref		*ref = (const FN_ref *)cref;
	const FN_ref_addr	*addr = (const FN_ref_addr *)caddr;
	FN_ctx_svc		*newthing = new X500Context(*addr, *ref,
				    authoritative, err);

	if (err) {
		s->set_code(FN_E_COMMUNICATION_FAILURE);
		delete newthing;
		newthing = 0;

	} else if (! newthing)
		s->set_code(FN_E_INSUFFICIENT_RESOURCES);
	else
		s->set_success();

#ifdef DEBUG
	x500_trace(err ? "ASx500(): error" : "ASx500(): OK");
#endif

	return ((FN_ctx_svc_t *)newthing);
}


extern "C"
FN_ctx_t *
Ax500(
	const FN_ref_addr_t	*addr,
	const FN_ref_t		*ref,
	unsigned int		authoritative,
	FN_status_t		*s)
{
	FN_ctx_svc_t	*newthing = ASx500(addr, ref, authoritative, s);
	FN_ctx		*ctxobj = (FN_ctx_svc *)newthing;

#ifdef DEBUG
	x500_trace("Ax500()");
#endif

	return ((FN_ctx_t *)ctxobj);
}
