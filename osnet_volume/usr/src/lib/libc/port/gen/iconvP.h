/*	Copyright (c) 1993 SMI  */
/*	All Rights Reserved   */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)iconvP.h	1.2	96/12/04 SMI"

struct _iconv_fields {
	void *_icv_handle;
	size_t (*_icv_iconv)(iconv_t, const char **, size_t *, char **,
		size_t *);
	void (*_icv_close)(iconv_t);
	void *_icv_state;
};

typedef struct _iconv_fields *iconv_p;

struct _iconv_info {
	iconv_p _from;		/* conversion codeset for source code to UTF2 */
	iconv_p _to;		/* conversion codeset for UTF2 to target code */
	size_t  bytesleft;    	/* used for premature/incomplete conversion */
};
