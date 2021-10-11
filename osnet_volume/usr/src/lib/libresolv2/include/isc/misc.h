/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 1995, 1996 by Internet Software Consortium
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

/*
 * $Id: misc.h,v 8.1 1996/11/12 03:19:13 vixie Exp $
 */
#pragma ident   "@(#)misc.h 1.2     98/05/18 SMI"

#ifndef _ISC_MISC_H
#define _ISC_MISC_H

#define	bitncmp		__bitncmp

extern int		bitncmp(const void *l, const void *r, int n);

#endif /*_ISC_MISC_H*/
