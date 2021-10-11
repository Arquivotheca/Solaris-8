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

#pragma ident   "@(#)irs_data.h 1.3     98/05/18 SMI"

/*
 * $Id: irs_data.h,v 1.6 1996/11/07 10:03:06 vixie Exp $
 */

#ifndef	_IRS_DATA_H
#define	_IRS_DATA_H


#define net_data		__net_data
#define	net_data_init		__net_data_init

struct net_data {
	struct irs_acc *	irs;

	struct irs_gr *		gr;
	struct irs_pw *		pw;
	struct irs_sv *		sv;
	struct irs_pr *		pr;
	struct irs_ho *		ho;
	struct irs_nw *		nw;
	struct irs_ng *		ng;

	struct group *		gr_last;
	struct passwd *		pw_last;
	struct servent *	sv_last;
	struct protoent *	pr_last;
	struct netent *		nw_last;
	struct hostent *	ho_last;

	unsigned int		gr_stayopen :1;
	unsigned int		pw_stayopen :1;
	unsigned int		sv_stayopen :1;
	unsigned int		pr_stayopen :1;
	unsigned int		ho_stayopen :1;
	unsigned int		nw_stayopen :1;

	void *			nw_data;
	void *			ho_data;

	char			fill[512 - 68];	/* 68 = sizeof(above) */
};

extern struct net_data		net_data;
extern int			net_data_init(void);

#endif	/* _IR_DATA_H */
