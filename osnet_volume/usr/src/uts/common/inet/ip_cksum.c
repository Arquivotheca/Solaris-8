#pragma ident	"@(#)ip_cksum.c	1.19	99/03/21 SMI"

/*
 * Copyright (c) 1983 - 1992 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/systm.h>
#include <sys/stream.h>
#include <sys/debug.h>
#include <sys/ddi.h>
#include <sys/vtrace.h>

extern unsigned int 	ip_ocsum(ushort_t *address, int halfword_count,
    unsigned int sum);
extern unsigned int	ip_ocsum_copy(ushort_t *address, int halfword_count,
    unsigned int sum, ushort_t *dest);

/*
 * Checksum routine for Internet Protocol family headers.
 * This routine is very heavily used in the network
 * code and should be modified for each CPU to be as fast as possible.
 */

#define	mp_len(mp) ((mp)->b_wptr - (mp)->b_rptr)

/*
 * Even/Odd checks. Usually it is performed on pointers but may be
 * used on integers as well. uintptr_t is long enough to hold both
 * integer and pointer.
 */
#define	is_odd(p) (((uintptr_t)(p) & 0x1) != 0)
#define	is_even(p) (!is_odd(p))


#ifdef ZC_TEST
/*
 * Disable the TCP s/w cksum.
 * XXX - This is just a hack for testing purpose. Don't use it for
 * anything else!
 */
int noswcksum = 0;
#endif
/*
 * Note: this does not ones-complement the result since it is used
 * when computing partial checksums.
 * For nonSTRUIO_IP mblks, assumes mp->b_rptr+offset is 16 bit alligned.
 * For STRUIO_IP mblks, assumes mp->b_datap->db_struiobase is 16 bit alligned.
 *
 * Note: for STRUIO_IP special mblks some data may have been previously
 *	 checksumed, this routine will handle additional data prefixed within
 *	 an mblk or b_cont (chained) mblk(s). This routine will also handle
 *	 suffixed b_cont mblk(s) but not data suffixed within an mblk !!!
 */
unsigned int
ip_cksum(mblk_t *mp, int offset, uint_t sum)
{
	ushort_t *w;
	ssize_t	mlen;
	int pmlen;
	mblk_t *pmp;
	dblk_t *dp = mp->b_datap;
	ushort_t psum = 0;

#ifdef ZC_TEST
	if (noswcksum)
		return (0xffff);
#endif
	ASSERT(dp);
	TRACE_4(TR_FAC_IP, TR_IP_CKSUM_START,
	    "ip_cksum_start:%p size %ld off %d (%X)",
	    mp, msgdsize(mp), offset, sum);

	if (mp->b_cont == NULL) {
		/*
		 * May be fast-path, only one mblk.
		 */
		w = (ushort_t *)(mp->b_rptr + offset);
		if (dp->db_struioflag & STRUIO_IP) {
			/*
			 * Checksum any data not already done by
			 * the caller and add in any partial checksum.
			 */
			if ((uchar_t *)w > dp->db_struiobase ||
			    mp->b_wptr < dp->db_struiolim) {
				/*
				 * Mblk data pointers aren't inclusive
				 * of uio data, so disregard checksum.
				 *
				 * not using all of data in dblk make sure
				 * not use to use the precalculated checksum
				 * in this case.
				 */
				dp->db_struioflag &= ~STRUIO_IP;
				goto norm;
			}
			ASSERT(mp->b_wptr == dp->db_struiolim);
			psum = *(ushort_t *)dp->db_struioun.data;
			if ((mlen = dp->db_struiobase - (uchar_t *)w) < 0)
				mlen = 0;
			if (is_odd(mlen))
				goto slow;
			if (mlen && dp->db_struiobase != dp->db_struioptr &&
			    dp->db_struiolim != dp->db_struioptr) {
				/*
				 * There is prefix data to do and some uio
				 * data has already been checksumed and there
				 * is more uio data to do, so do the prefix
				 * data first, then do the remainder of the
				 * uio data.
				 */
				sum = ip_ocsum(w, mlen >> 1, sum);
				w = (ushort_t *)dp->db_struioptr;
				if (is_odd(w)) {
					pmp = mp;
					goto slow1;
				}
				mlen = dp->db_struiolim - dp->db_struioptr;
			} else if (dp->db_struiolim != dp->db_struioptr) {
				/*
				 * There may be uio data to do, if there is
				 * prefix data to do then add in all of the
				 * uio data (if any) to do, else just do any
				 * uio data.
				 */
				if (mlen)
					mlen += dp->db_struiolim
						- dp->db_struioptr;
				else {
					w = (ushort_t *)dp->db_struioptr;
					if (is_odd(w))
						goto slow;
					mlen = dp->db_struiolim
						- dp->db_struioptr;
				}
			} else if (mlen == 0)
#ifdef TRACE
	{
		TRACE_4(TR_FAC_IP, TR_IP_CKSUM_START,
		    "ip_cksum_start:%p size %ld off %d (%X)",
		    mp, mlen, dp->db_struioflag, sum);
		sum += psum;
		TRACE_3(TR_FAC_IP, TR_IP_CKSUM_END,
			"ip_cksum_end:(%S) type %d (%X)", "ip_cksum", 2, sum);
		return (sum);
	}
#else
				return (psum);
#endif
			if (is_odd(mlen))
				goto slow;
			sum += psum;
		} else {
			/*
			 * Checksum all data not already done by the caller.
			 */
		norm:
			mlen = mp->b_wptr - (uchar_t *)w;
			if (is_odd(mlen))
				goto slow;
		}
		ASSERT(is_even(w));
		ASSERT(is_even(mlen));
#ifdef TRACE
		TRACE_4(TR_FAC_IP, TR_IP_CKSUM_START,
		    "ip_cksum_start:%p size %ld off %d (%X)",
		    mp, mlen, dp->db_struioflag, sum);
		sum = ip_ocsum(w, mlen >> 1, sum);
		TRACE_3(TR_FAC_IP, TR_IP_CKSUM_END,
			"ip_cksum_end:(%S) type %d (%X)", "ip_cksum", 0, sum);
		return (sum);
#else
		return (ip_ocsum(w, mlen >> 1, sum));
#endif
	}
	if (dp->db_struioflag & STRUIO_IP)
		psum = *(ushort_t *)dp->db_struioun.data;
slow:
	pmp = 0;
slow1:
	mlen = 0;
	pmlen = 0;
	for (; ; ) {
		/*
		 * Each trip around loop adds in word(s) from one mbuf segment
		 * (except for when pmp == mp, then its two partial trips).
		 */
		w = (ushort_t *)(mp->b_rptr + offset);
		if (pmp) {
			/*
			 * This is the second trip around for this mblk.
			 */
			pmp = 0;
			mlen = 0;
			goto douio;
		} else if (dp->db_struioflag & STRUIO_IP) {
			/*
			 * Checksum any data not already done by the
			 * caller and add in any partial checksum.
			 */
			if ((uchar_t *)w > dp->db_struiobase ||
			    mp->b_wptr < dp->db_struiolim) {
				/*
				 * Mblk data pointers aren't inclusive
				 * of uio data, so disregard checksum.
				 *
				 * not using all of data in dblk make sure
				 * not use to use the precalculated checksum
				 * in this case.
				 */
				dp->db_struioflag &= ~STRUIO_IP;
				goto snorm;
			}
			ASSERT(mp->b_wptr == dp->db_struiolim);
			if ((mlen = dp->db_struiobase - (uchar_t *)w) < 0)
				mlen = 0;
			if (mlen && dp->db_struiobase != dp->db_struioptr) {
				/*
				 * There is prefix data too do and some
				 * uio data has already been checksumed,
				 * so do the prefix data only this trip.
				 */
				pmp = mp;
			} else {
				/*
				 * Add in any partial cksum (if any) and
				 * do the remainder of the uio data.
				 */
				int odd;
			douio:
				odd = is_odd(dp->db_struioptr -
						dp->db_struiobase);
				if (pmlen == -1) {
					/*
					 * Previous mlen was odd, so swap
					 * the partial checksum bytes.
					 */
					sum += ((psum << 8) & 0xffff)
					    | (psum >> 8);
					if (odd)
						pmlen = 0;
				} else {
					sum += psum;
					if (odd)
						pmlen = -1;
				}
				if (dp->db_struiolim != dp->db_struioptr) {
					/*
					 * If prefix data to do and then all
					 * the uio data nees to be checksumed,
					 * else just do any uio data.
					 */
					if (mlen)
						mlen += dp->db_struiolim
							- dp->db_struioptr;
					else {
						w = (ushort_t *)
						    dp->db_struioptr;
						mlen = dp->db_struiolim -
						    dp->db_struioptr;
					}
				}
			}
		} else {
			/*
			 * Checksum all of the mblk data.
			 */
		snorm:
			mlen = mp->b_wptr - (uchar_t *)w;
		}
#ifdef	TRACE
		TRACE_4(TR_FAC_IP, TR_IP_CKSUM_START,
		    "ip_cksum_start:%p size %ld off %d (%X)",
		    mp, mlen, dp->db_struioflag, sum)
#endif
		mp = mp->b_cont;
		if (mlen > 0 && pmlen == -1) {
			/*
			 * There is a byte left from the last
			 * segment; add it into the checksum.
			 * Don't have to worry about a carry-
			 * out here because we make sure that
			 * high part of (32 bit) sum is small
			 * below.
			 */
#ifdef _LITTLE_ENDIAN
			sum += *(uchar_t *)w << 8;
#else
			sum += *(uchar_t *)w;
#endif
			w = (ushort_t *)((char *)w + 1);
			mlen--;
			pmlen = 0;
		}
		if (mlen > 0) {
			if (is_even(w)) {
				sum = ip_ocsum(w, mlen>>1, sum);
				w += mlen>>1;
				/*
				 * If we had an odd number of bytes,
				 * then the last byte goes in the high
				 * part of the sum, and we take the
				 * first byte to the low part of the sum
				 * the next time around the loop.
				 */
				if (is_odd(mlen)) {
#ifdef _LITTLE_ENDIAN
					sum += *(uchar_t *)w;
#else
					sum += *(uchar_t *)w << 8;
#endif
					pmlen = -1;
				}
			} else {
				ushort_t swsum;
#ifdef _LITTLE_ENDIAN
				sum += *(uchar_t *)w;
#else
				sum += *(uchar_t *)w << 8;
#endif
				mlen--;
				w = (ushort_t *)(1 + (uintptr_t)w);

				/* Do a separate checksum and copy operation */
				swsum = ip_ocsum(w, mlen>>1, 0);
				sum += ((swsum << 8) & 0xffff) | (swsum >> 8);
				w += mlen>>1;
				/*
				 * If we had an even number of bytes,
				 * then the last byte goes in the low
				 * part of the sum.  Otherwise we had an
				 * odd number of bytes and we take the first
				 * byte to the low part of the sum the
				 * next time around the loop.
				 */
				if (is_odd(mlen)) {
#ifdef _LITTLE_ENDIAN
					sum += *(uchar_t *)w << 8;
#else
					sum += *(uchar_t *)w;
#endif
				}
				else
					pmlen = -1;
			}
		}
		/*
		 * Locate the next block with some data.
		 * If there is a word split across a boundary we
		 * will wrap to the top with mlen == -1 and
		 * then add it in shifted appropriately.
		 */
		offset = 0;
		if (! pmp) {
			for (; ; ) {
				if (mp == 0) {
					goto done;
				}
				if (mp_len(mp))
					break;
				mp = mp->b_cont;
			}
			dp = mp->b_datap;
			if (dp->db_struioflag & STRUIO_IP)
				psum = *(ushort_t *)dp->db_struioun.data;
		} else
			mp = pmp;
	}
done:
	/*
	 * Add together high and low parts of sum
	 * and carry to get cksum.
	 * Have to be careful to not drop the last
	 * carry here.
	 */
	sum = (sum & 0xFFFF) + (sum >> 16);
	sum = (sum & 0xFFFF) + (sum >> 16);
	TRACE_3(TR_FAC_IP, TR_IP_CKSUM_END,
		"ip_cksum_end:(%S) type %d (%X)", "ip_cksum", 1, sum);
	return (sum);
}
