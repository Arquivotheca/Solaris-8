/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#pragma ident   "@(#)nis_p.h 1.1     97/12/03 SMI"

/*
 * $Id: nis_p.h,v 1.4 1996/10/25 07:23:24 vixie Exp $
 */

/*
 * nis_p.h - private include file for the NIS functions.
 */

/*
 * Object state.
 */
struct nis_p {
	char *		domain;
};


/*
 * Methods.
 */

extern struct irs_gr *	irs_nis_gr __P((struct irs_acc *));
extern struct irs_pw *	irs_nis_pw __P((struct irs_acc *));
extern struct irs_sv *	irs_nis_sv __P((struct irs_acc *));
extern struct irs_pr *	irs_nis_pr __P((struct irs_acc *));
extern struct irs_ho *	irs_nis_ho __P((struct irs_acc *));
extern struct irs_nw *	irs_nis_nw __P((struct irs_acc *));
extern struct irs_ng *	irs_nis_ng __P((struct irs_acc *));
