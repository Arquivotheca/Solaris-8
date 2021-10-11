/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)xdr_mblk.c	1.27	97/12/17 SMI"

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *	(c) 1986, 1987, 1988, 1989  Sun Microsystems, Inc.
 *	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */

/*
 * xdr_mblk.c, XDR implementation on kernel streams mblks.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/stream.h>
#include <sys/cmn_err.h>
#include <sys/strsubr.h>
#include <sys/debug.h>

#include <rpc/types.h>
#include <rpc/xdr.h>

static bool_t	xdrmblk_getint32(XDR *, int32_t *);
static bool_t	xdrmblk_putint32(XDR *, int32_t *);
static bool_t	xdrmblk_getbytes(XDR *, caddr_t, int);
static bool_t	xdrmblk_putbytes(XDR *, caddr_t, int);
static u_int	xdrmblk_getpos(XDR *);
static bool_t	xdrmblk_setpos(XDR *, u_int);
static rpc_inline_t *xdrmblk_inline(XDR *, int);
static void	xdrmblk_destroy(XDR *);
static bool_t	xdrmblk_control(XDR *, int, void *);

static mblk_t *xdrmblk_alloc(int);

/*
 * If the size of mblk is not appreciably larger then what we
 * asked, then resize the mblk to exactly len bytes. The reason for
 * this: suppose len is 1460 bytes, which is the tidu
 * (from TCP over ethernet), and the arguments to the RPC require
 * 2800 bytes. Ideally we want the protocol to render two
 * ~1400 byte segments over the wire. However if allocb() gives us a 2k
 * mblk, and we allocate a second mblk for the remainder, the protocol
 * module may generate 3 segments over the wire:
 * 1460 bytes for the first, 588 (2048 - 1600) for the second, and
 * 752 for the third. If we "waste" 588 bytes in the first mblk,
 * the XDR encoding will generate two ~1400 byte mblks, and the
 * protocol module is more likely to produce properly sized segments.
 */
#define	XDRMBLK_SETHANDY(mp, xdrp, blen, adjust)	{	\
	int mpsize = bpsize(mp);				\
	ASSERT((mpsize) >= (blen));				\
	ASSERT((mp)->b_rptr == (mp)->b_datap->db_base);		\
	if ((mpsize >> 1) <= (blen)) {				\
		(mp)->b_rptr += (mpsize - (blen));		\
		(mp)->b_wptr = (mp)->b_rptr;			\
	}							\
	(xdrp)->x_handy = (int)((mp)->b_datap->db_lim -		\
	    (mp)->b_rptr - (adjust));				\
}


/*
 * Xdr on mblks operations vector.
 */
struct	xdr_ops xdrmblk_ops = {
	xdrmblk_getbytes,
	xdrmblk_putbytes,
	xdrmblk_getpos,
	xdrmblk_setpos,
	xdrmblk_inline,
	xdrmblk_destroy,
	xdrmblk_control,
	xdrmblk_getint32,
	xdrmblk_putint32
};

/*
 * Initialize xdr stream.
 */
void
xdrmblk_init(XDR *xdrs, mblk_t *m, enum xdr_op op, int sz)
{
	xdrs->x_op = op;
	xdrs->x_ops = &xdrmblk_ops;
	xdrs->x_base = (caddr_t)m;
	xdrs->x_public = NULL;
	xdrs->x_private = (caddr_t)sz;

	if (op == XDR_DECODE)
		xdrs->x_handy = (int)(m->b_wptr - m->b_rptr);
	else
		xdrs->x_handy = (int)(m->b_datap->db_lim - m->b_datap->db_base);
}

/* ARGSUSED */
static void
xdrmblk_destroy(XDR *xdrs)
{
}

static bool_t
xdrmblk_getint32(XDR *xdrs, int32_t *int32p)
{
	mblk_t *m;

	/* LINTED pointer alignment */
	m = (mblk_t *)xdrs->x_base;
	if (m == NULL)
		return (FALSE);
	while ((xdrs->x_handy -= (int)sizeof (int32_t)) < 0) {
		if (xdrs->x_handy != -sizeof (int32_t))
			return (FALSE);
		m = m->b_cont;
		xdrs->x_base = (caddr_t)m;
		if (m == NULL) {
			xdrs->x_handy = 0;
			return (FALSE);
		}
		xdrs->x_handy = (int)(m->b_wptr - m->b_rptr);
	}
	/* LINTED pointer alignment */
	*int32p = ntohl(*((int32_t *)(m->b_rptr)));
	m->b_rptr += sizeof (int32_t);
	return (TRUE);
}

static bool_t
xdrmblk_putint32(XDR *xdrs, int32_t *int32p)
{
	mblk_t *m;

	/* LINTED pointer alignment */
	m = (mblk_t *)xdrs->x_base;
	if (m == NULL)
		return (FALSE);
	if ((xdrs->x_handy -= (int)sizeof (int32_t)) < 0) {
		if (m->b_cont == NULL) {
			int buf_len;

			buf_len = (int)xdrs->x_private;
			m->b_cont = xdrmblk_alloc(buf_len);
			m = m->b_cont;
			xdrs->x_base = (caddr_t)m;
			if (m == NULL) {
				xdrs->x_handy = 0;
				return (FALSE);
			}
			XDRMBLK_SETHANDY(m, xdrs, buf_len, sizeof (int32_t));
		} else {
			m = m->b_cont;
			xdrs->x_base = (caddr_t)m;
			xdrs->x_handy = (int)(m->b_datap->db_lim - m->b_rptr -
			    sizeof (int32_t));
		}
		ASSERT(m->b_rptr == m->b_wptr);
		ASSERT(m->b_rptr >= m->b_datap->db_base);
		ASSERT(m->b_rptr < m->b_datap->db_lim);
	}
	/* LINTED pointer alignment */
	*(int32_t *)m->b_wptr = htonl(*int32p);
	m->b_wptr += sizeof (int32_t);
	ASSERT(m->b_wptr <= m->b_datap->db_lim);
	return (TRUE);
}

static bool_t
xdrmblk_getbytes(XDR *xdrs, caddr_t addr, int len)
{
	mblk_t *m;

	/* LINTED pointer alignment */
	m = (mblk_t *)xdrs->x_base;
	if (m == NULL)
		return (FALSE);
	while ((xdrs->x_handy -= len) < 0) {
		if ((xdrs->x_handy += len) > 0) {
			bcopy(m->b_rptr, addr, xdrs->x_handy);
			m->b_rptr += xdrs->x_handy;
			addr += xdrs->x_handy;
			len -= xdrs->x_handy;
		}
		m = m->b_cont;
		xdrs->x_base = (caddr_t)m;
		if (m == NULL) {
			xdrs->x_handy = 0;
			return (FALSE);
		}
		xdrs->x_handy = (int)(m->b_wptr - m->b_rptr);
	}
	bcopy(m->b_rptr, addr, len);
	m->b_rptr += len;
	return (TRUE);
}

/*
 * Sort of like getbytes except that instead of getting
 * bytes we return the mblk chain which contains all the rest
 * of the bytes.
 */
bool_t
xdrmblk_getmblk(XDR *xdrs, mblk_t **mm, u_int *lenp)
{
	mblk_t *m;
	int len;
	int32_t llen;

	if (!xdrmblk_getint32(xdrs, &llen))
		return (FALSE);

	*lenp = llen;
	/* LINTED pointer alignment */
	m = (mblk_t *)xdrs->x_base;
	*mm = m;
	/*
	 * Consistency check.
	 */
	len = 0;
	while (m) {
		len += (int)(m->b_wptr - m->b_rptr);
		m = m->b_cont;
	}
	if (len < llen) {
		printf("xdrmblk_getmblk failed\n");
		return (FALSE);
	}
	xdrs->x_base = NULL;
	xdrs->x_handy = 0;
	return (TRUE);
}

static bool_t
xdrmblk_putbytes(XDR *xdrs, caddr_t addr, int len)
{
	mblk_t *m;
	u_int i;

	/* LINTED pointer alignment */
	m = (mblk_t *)xdrs->x_base;
	if (m == NULL)
		return (FALSE);
	/*
	 * Performance tweak: converted explicit bcopy()
	 * call to simple in-line. This function is called
	 * to process things like readdir reply filenames
	 * which are small strings--typically 12 bytes or less.
	 * Overhead of calling bcopy() is obnoxious for such
	 * small copies.
	 * We pick 16 as a compromise threshold for most architectures.
	 */
	while ((xdrs->x_handy -= len) < 0) {
		if ((xdrs->x_handy += len) > 0) {
			if (xdrs->x_handy < 16) {
				for (i = 0; i < (u_int)xdrs->x_handy; i++)
					*m->b_wptr++ = *addr++;
			} else {
				bcopy(addr, m->b_wptr, xdrs->x_handy);
				m->b_wptr += xdrs->x_handy;
				addr += xdrs->x_handy;
			}
			len -= xdrs->x_handy;
		}
		if (m->b_cont == NULL) {
			int buf_len = (int)xdrs->x_private;

			m->b_cont = xdrmblk_alloc(buf_len);
			m = m->b_cont;
			xdrs->x_base = (caddr_t)m;
			if (m == NULL) {
				xdrs->x_handy = 0;
				return (FALSE);
			}
			XDRMBLK_SETHANDY(m, xdrs, buf_len, 0);
		} else {
			m = m->b_cont;
			xdrs->x_base = (caddr_t)m;
			if (m == NULL) {
				xdrs->x_handy = 0;
				return (FALSE);
			}
			xdrs->x_handy = (int)(m->b_datap->db_lim - m->b_rptr);
		}
		ASSERT(m->b_rptr == m->b_wptr);
		ASSERT(m->b_rptr >= m->b_datap->db_base);
		ASSERT(m->b_rptr < m->b_datap->db_lim);
	}
	if (len < 16) {
		for (i = 0; i < len; i++)
			*m->b_wptr++ = *addr++;
	} else {
		bcopy(addr, m->b_wptr, len);
		m->b_wptr += len;
	}
	ASSERT(m->b_wptr <= m->b_datap->db_lim);
	return (TRUE);
}

/*
 * We avoid a copy by merely adding this mblk to the list.  The caller is
 * responsible for allocating and filling in the mblk. If len is
 * not a multiple of BYTES_PER_XDR_UNIT, the caller has the option
 * of making the data a BYTES_PER_XDR_UNIT multiple (b_wptr - b_rptr is
 * a BYTES_PER_XDR_UNIT multiple), but in this case the caller has to ensure
 * that the filler bytes are initialized to zero. Note: Doesn't to work for
 * chained mblks.
 */
bool_t
xdrmblk_putmblk(XDR *xdrs, mblk_t *m, u_int len)
{
	int32_t llen = (int32_t)len;

	if (((m->b_wptr - m->b_rptr) % BYTES_PER_XDR_UNIT) != 0)
		return (FALSE);
	if (!xdrmblk_putint32(xdrs, &llen))
		return (FALSE);
	/* LINTED pointer alignment */
	((mblk_t *)xdrs->x_base)->b_cont = m;
	xdrs->x_base = (caddr_t)m;
	xdrs->x_handy = 0;
	return (TRUE);
}

static u_int
xdrmblk_getpos(XDR *xdrs)
{
	u_int tmp;
	mblk_t *m;

	/* LINTED pointer alignment */
	m = (mblk_t *)xdrs->x_base;

	if (xdrs->x_op == XDR_DECODE)
		tmp = (u_int)(m->b_rptr - m->b_datap->db_base);
	else
		tmp = (u_int)(m->b_wptr - m->b_datap->db_base);
	return (tmp);

}

static bool_t
xdrmblk_setpos(XDR *xdrs, u_int pos)
{
	mblk_t *m;
	unsigned char *newaddr;

	/* LINTED pointer alignment */
	m = (mblk_t *)xdrs->x_base;
	if (m == NULL)
		return (FALSE);

	/* calculate the new address from the base */
	newaddr = m->b_datap->db_base + pos;

	if (xdrs->x_op == XDR_DECODE) {
		if (newaddr > m->b_wptr)
			return (FALSE);
		m->b_rptr = newaddr;
		xdrs->x_handy = (int)(m->b_wptr - newaddr);
	} else {
		if (newaddr > m->b_datap->db_lim)
			return (FALSE);
		m->b_wptr = newaddr;
		xdrs->x_handy = (int)(m->b_datap->db_lim - newaddr);
	}

	return (TRUE);
}

#ifdef DEBUG
static int xdrmblk_inline_hits = 0;
static int xdrmblk_inline_misses = 0;
static int do_xdrmblk_inline = 1;
#endif

static rpc_inline_t *
xdrmblk_inline(XDR *xdrs, int len)
{
	rpc_inline_t *buf;
	mblk_t *m;

	/*
	 * Can't inline XDR_FREE calls, doesn't make sense.
	 */
	if (xdrs->x_op == XDR_FREE)
		return (NULL);

	/*
	 * Can't inline if there isn't enough room, don't have an
	 * mblk pointer, or if there is more than one reference to
	 * the data block associated with this mblk.  This last
	 * check is used because the caller may want to modified
	 * the data in the inlined portion and someone else is
	 * holding a reference to the data who may not want it
	 * to be modified.
	 */
	if (xdrs->x_handy < len ||
	    /* LINTED pointer alignment */
	    (m = (mblk_t *)xdrs->x_base) == NULL ||
	    m->b_datap->db_ref != 1) {
#ifdef DEBUG
		xdrmblk_inline_misses++;
#endif
		return (NULL);
	}

#ifdef DEBUG
	if (!do_xdrmblk_inline) {
		xdrmblk_inline_misses++;
		return (NULL);
	}
#endif

	xdrs->x_handy -= len;
	if (xdrs->x_op == XDR_DECODE) {
		/* LINTED pointer alignment */
		buf = (rpc_inline_t *)m->b_rptr;
		m->b_rptr += len;
	} else {
		/* LINTED pointer alignment */
		buf = (rpc_inline_t *)m->b_wptr;
		m->b_wptr += len;
	}
#ifdef DEBUG
	xdrmblk_inline_hits++;
#endif
	return (buf);
}

static bool_t
xdrmblk_control(XDR *xdrs, int request, void *info)
{
	mblk_t *m;
	int32_t *int32p;
	int len;

	switch (request) {
	case XDR_PEEK:
		/*
		 * Return the next 4 byte unit in the XDR stream.
		 */
		if (xdrs->x_handy < sizeof (int32_t))
			return (FALSE);

		/* LINTED pointer alignment */
		m = (mblk_t *)xdrs->x_base;
		if (m == NULL)
			return (FALSE);
		int32p = (int32_t *)info;
		/* LINTED pointer alignment */
		*int32p = ntohl(*((int32_t *)(m->b_rptr)));
		return (TRUE);

	case XDR_SKIPBYTES:
		/* LINTED pointer alignment */
		m = (mblk_t *)xdrs->x_base;
		if (m == NULL)
			return (FALSE);
		int32p = (int32_t *)info;
		len = RNDUP((int)(*int32p));
		while ((xdrs->x_handy -= len) < 0) {
			if ((xdrs->x_handy += len) > 0) {
				m->b_rptr += xdrs->x_handy;
				len -= xdrs->x_handy;
			}
			m = m->b_cont;
			xdrs->x_base = (caddr_t)m;
			if (m == NULL) {
				xdrs->x_handy = 0;
				return (FALSE);
			}
			xdrs->x_handy = (int)(m->b_wptr - m->b_rptr);
		}
		m->b_rptr += len;
		return (TRUE);

	default:
		return (FALSE);
	}
}

static mblk_t *
xdrmblk_alloc(int sz)
{
	mblk_t *mp;

	if (sz == 0)
		return (NULL);

	while ((mp = allocb(sz, BPRI_LO)) == NULL) {
		if (strwaitbuf(sz, BPRI_LO))
			break;
	}
	return (mp);
}
