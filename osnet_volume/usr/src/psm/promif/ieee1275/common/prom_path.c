/*
 * Copyright (c) 1991-1994,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_path.c	1.17	98/07/10 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

char *
prom_path_gettoken(register char *from, register char *to)
{
	while (*from) {
		switch (*from) {
		case '/':
		case '@':
		case ':':
		case ',':
			*to = '\0';
			return (from);
		default:
			*to++ = *from++;
		}
	}
	*to = '\0';
	return (from);
}

/*
 * Given an OBP pathname, do the best we can to fully expand
 * the OBP pathname, in place in the callers buffer.
 *
 * If we have to complete the addrspec of any component, we can
 * only handle devices that have a maximum of NREGSPECS "reg" specs.
 * We cannot allocate memory inside this function.
 *
 * XXX: Assumes a single threaded model, as static buffers are used
 *	for temporary storage.  This is not to be used an an external
 *	interface.  The external interface should have temporary
 *	buffers passed in, or they should be allocated on the stack,
 *	(which may not be desirable in the kernel).
 */

static char buffer[OBP_MAXPATHLEN];

void
prom_pathname(char *pathname)
{
	char *from = buffer;
	char *to = pathname;
	char *p;
	cell_t ci[7];
#ifdef PROM_32BIT_ADDRS
	char *opathname = NULL;
#endif

	if ((to == (char *)0) || (*to == (char)0))
		return;

	promif_preprom();

#ifdef PROM_32BIT_ADDRS
	if ((uintptr_t)pathname > (uint32_t)-1) {
		opathname = pathname;
		pathname = promplat_alloc(OBP_MAXPATHLEN);
		if (pathname == NULL) {
			promif_postprom();
			return;
		}
		(void) prom_strcpy(pathname, opathname);
		to = pathname;
	}
#endif

	(void) prom_strcpy(from, to);
	*to = (char)0;

	ci[0] = p1275_ptr2cell("canon");	/* Service name */
	ci[1] = (cell_t)3;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_ptr2cell(from);		/* Arg1: token */
	ci[4] = p1275_ptr2cell(to);		/* Arg2: buffer address */
	ci[5] = p1275_uint2cell(OBP_MAXPATHLEN); /* Arg3: buffer length */

	(void) p1275_cif_handler(&ci);

#ifdef PROM_32BIT_ADDRS
	if (opathname != NULL) {
		(void) prom_strcpy(opathname, pathname);
		promplat_free(pathname, OBP_MAXPATHLEN);
		to = pathname = opathname;
	}
#endif
	promif_postprom();

	/*
	 * workaround for bugid 1218110, the prom strips the
	 * options from the input string ... save options at
	 * at the end of the string if the prom didn't.
	 * NB: The workaround only preserves options in the last
	 * component of the string.
	 */

	/*
	 * If there are any options in the last component of the
	 * output, the prom has copied them; No workaround required.
	 */
	if ((p = prom_strrchr(to, '/')) == 0)
		return;
	if ((p = prom_strchr(p, ':')) != 0)
		return;

	/*
	 * If there are no options in the input ... there's
	 * nothing to preserve; return.
	 */
	if ((p = prom_strrchr(from, '/')) == 0)
		p = from;
	if ((p = prom_strchr(p, ':')) == 0)
		return;

	/*
	 * Concatenate the options we found to the end of the output string.
	 */
	(void) prom_strcat(to, p);
}

/*
 * Strip any options strings from an OBP pathname.
 * Output buffer (to) expected to be as large as input buffer (from).
 */
void
prom_strip_options(char *from, char *to)
{
	while (*from != (char)0)  {
		if (*from == ':')  {
			while ((*from != (char)0) && (*from != '/'))
				++from;
		} else
			*to++ = *from++;
	}
	*to = (char)0;
}
