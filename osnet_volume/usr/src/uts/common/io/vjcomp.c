/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vjcomp.c	1.17	98/09/30 SMI"


/*
 * vjcomp.c
 *
 * Module which does VJ compression in the ppp stream.
 *
 */


#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/ddi.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/strlog.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <sys/kmem.h>

#include <netinet/ip.h>
#include <netinet/tcp.h>

#ifdef ISERE_TREE
#include <ppp/ppp_ioctl.h>
#include <ppp/vjcomp.h>
#include <ppp/ppp_sys.h>
#else
#include <sys/ppp_ioctl.h>
#include <sys/vjcomp.h>
#include <sys/ppp_sys.h>
#endif

#ifndef SL_NO_STATS
#define	INCR(counter) ++comp->counter;
#else
#define	INCR(counter)
#endif

void
vjcompress_init(vjstruct_t *comp)
{
	uint_t i;
	struct cstate *tstate = comp->tstate;
	bzero((char *)comp, sizeof (*comp));
	for (i = MAX_STATES - 1; i != 0; --i) {
		tstate[i].cs_id = (uchar_t)i;
		tstate[i].cs_next = &tstate[i - 1];
	}
	tstate[0].cs_next = &tstate[MAX_STATES - 1];
	tstate[0].cs_id = 0;
	comp->last_cs = &tstate[0];
	comp->last_recv = (uchar_t)255;
	comp->last_xmit = (uchar_t)255;
	comp->flags = SLF_TOSS;
	comp->maxslotout = MAX_STATES;
	comp->compslotout = CSLOT;
	comp->maxslotin = MAX_STATES;
	comp->compslotin = CSLOT;
}

void
vjsetinparms(vjstruct_t *comp, uint_t maxslot, uint_t cslot)
{
	comp->maxslotin = maxslot;
	comp->compslotin = cslot;
}

/*
 * Set slot information
 * Assumes maxslot < MAX_SLOTS
 */
void
vjsetoutparms(vjstruct_t *comp, uint_t maxslot, uint_t cslot)
{
	struct cstate *tstate = comp->tstate;

	comp->maxslotout = maxslot;
	comp->compslotout = cslot;
	tstate[0].cs_next = &tstate[maxslot - 1];
	if (comp->last_cs->cs_id >= maxslot) {
		comp->last_cs = &tstate[0];
	}
}

/*
 * ENCODE encodes a number that is known to be non-zero.  ENCODEZ
 * checks for zero (since zero has to be encoded in the long, 3 byte
 * form).
 */
#define	ENCODE(n)	{ \
	if ((ushort_t)(n) >= 256) { \
		*cp++ = 0; \
		cp[1] = (uchar_t)(n); \
		cp[0] = (uchar_t)((n) >> 8); \
		cp += 2; \
	} else { \
		*cp++ = (uchar_t)(n); \
	} \
}
#define	ENCODEZ(n)	{ \
	if ((ushort_t)(n) >= 256 || (ushort_t)(n) == 0) { \
		*cp++ = (uchar_t)0; \
		cp[1] = (uchar_t)(n); \
		cp[0] = (uchar_t)((n) >> 8); \
		cp += 2; \
	} else { \
		*cp++ = (uchar_t)(n); \
	} \
}

#define	DECODEL(f)	{ \
	if (*cp == 0) {\
		(f) = htonl(ntohl(f) + ((cp[1] << 8) | cp[2])); \
		cp += 3; \
	} else { \
		(f) = htonl(ntohl(f) + (uint32_t)*cp++); \
	} \
}

#define	DECODES(f)	{ \
	if (*cp == 0) {\
		(f) = htons(ntohs(f) + ((cp[1] << 8) | cp[2])); \
		cp += 3; \
	} else { \
		(f) = htons(ntohs(f) + (uint16_t)*cp++); \
	} \
}

#define	DECODEU(f)	{ \
	if (*cp == 0) {\
		(f) = htons((cp[1] << 8) | cp[2]); \
		cp += 3; \
	} else { \
		(f) = htons((uint16_t)*cp++); \
	} \
}


/*
 * vjcompress_tcp
 *
 * Compress tcp packets.  Assumes message block has been pulled up.
 */
int
vjcompress_tcp(mblk_t **ptrmp, vjstruct_t *comp)
{
	mblk_t *mp, *newmp;
	struct ip *ip;
	struct cstate *cs = comp->last_cs->cs_next;
	uint_t hlen;
	struct tcphdr *oth;
	struct tcphdr *th;
	uint_t deltaS, deltaA;
	uint_t changes = 0;
	uchar_t new_seq[16];
	uchar_t *cp = new_seq;
	uint_t mlen;

	/*
	 * Bail if this is an IP fragment or if the TCP packet isn't
	 * `compressible' (i.e., ACK isn't set or some other control bit is
	 * set).  (We assume that the caller has already made sure the
	 * packet is IP proto TCP).
	 */
	mp = *ptrmp;
	mlen = (uint_t)msgdsize(mp);
	ip = (struct ip *)mp->b_rptr;
	hlen = ip->ip_hl;
	if ((ip->ip_off & htons(0x3fff)) || mlen < 40)
		return (TYPE_IP);

	if (ip->ip_p != IPPROTO_TCP)
		return (TYPE_IP);
	th = (struct tcphdr *)&((int *)ip)[hlen];
	if ((th->th_flags & (TH_SYN|TH_FIN|TH_RST|TH_ACK)) != TH_ACK)
		return (TYPE_IP);
	/*
	 * Packet is compressible -- we're going to send either a
	 * COMPRESSED_TCP or UNCOMPRESSED_TCP packet.  Either way we need
	 * to locate (or create) the connection state.	Special case the
	 * most recently used connection since it's most likely to be used
	 * again & we don't have to do any reordering if it's used.
	 */
	INCR(sls_packets)
	if (ip->ip_src.s_addr != cs->cs_ip.ip_src.s_addr ||
	    ip->ip_dst.s_addr != cs->cs_ip.ip_dst.s_addr ||
	    *(int *)th != ((int *)&cs->cs_ip)[cs->cs_ip.ip_hl]) {
		/*
		 * Wasn't the first -- search for it.
		 *
		 * States are kept in a circularly linked list with
		 * last_cs pointing to the end of the list.  The
		 * list is kept in lru order by moving a state to the
		 * head of the list whenever it is referenced.	Since
		 * the list is short and, empirically, the connection
		 * we want is almost always near the front, we locate
		 * states via linear search.  If we don't find a state
		 * for the datagram, the oldest state is (re-)used.
		 */
		struct cstate *lcs;
		struct cstate *lastcs = comp->last_cs;

		do {
			lcs = cs; cs = cs->cs_next;
			INCR(sls_searches)
			if (ip->ip_src.s_addr == cs->cs_ip.ip_src.s_addr &&
			    ip->ip_dst.s_addr == cs->cs_ip.ip_dst.s_addr &&
			    *(int *)th ==
			    ((int *)&cs->cs_ip)[cs->cs_ip.ip_hl])
				goto found;
		} while (cs != lastcs);

		/*
		 * Didn't find it -- re-use oldest cstate.  Send an
		 * uncompressed packet that tells the other side what
		 * connection number we're using for this conversation.
		 * Note that since the state list is circular, the oldest
		 * state points to the newest and we only need to set
		 * last_cs to update the lru linkage.
		 */
		INCR(sls_misses)
		comp->last_cs = lcs;
		hlen += th->th_off;
		hlen <<= 2;
		if (hlen > mlen)
			return (TYPE_IP);
		goto uncompressed;

	found:
		/*
		 * Found it -- move to the front on the connection list.
		 */
		if (cs == lastcs)
			comp->last_cs = lcs;
		else {
			lcs->cs_next = cs->cs_next;
			cs->cs_next = lastcs->cs_next;
			lastcs->cs_next = cs;
		}
	}

	/*
	 * Make sure that only what we expect to change changed. The first
	 * line of the `if' checks the IP protocol version, header length &
	 * type of service.  The 2nd line checks the "Don't fragment" bit.
	 * The 3rd line checks the time-to-live and protocol (the protocol
	 * check is unnecessary but costless).	The 4th line checks the TCP
	 * header length.  The 5th line checks IP options, if any.  The 6th
	 * line checks TCP options, if any.  If any of these things are
	 * different between the previous & current datagram, we send the
	 * current datagram `uncompressed'.
	 */
	oth = (struct tcphdr *)&((int *)&cs->cs_ip)[hlen];
	deltaS = hlen;
	hlen += th->th_off;
	hlen <<= 2;
	if (hlen > mlen)
		return (TYPE_IP);

	if (((ushort_t *)ip)[0] != ((ushort_t *)&cs->cs_ip)[0] ||
	    ((ushort_t *)ip)[3] != ((ushort_t *)&cs->cs_ip)[3] ||
	    ((ushort_t *)ip)[4] != ((ushort_t *)&cs->cs_ip)[4] ||
	    th->th_off != oth->th_off ||
	    (deltaS > 5 &&
	    bcmp((ip + 1), (&cs->cs_ip + 1), (deltaS - 5) << 2)) ||
	    (th->th_off > 5 &&
	    bcmp((th + 1), (oth + 1), (th->th_off - 5) << 2)))
		goto uncompressed;

	/*
	 * Figure out which of the changing fields changed.  The
	 * receiver expects changes in the order: urgent, window,
	 * ack, seq (the order minimizes the number of temporaries
	 * needed in this section of code).
	 */
	if (th->th_flags & TH_URG) {
		deltaS = ntohs(th->th_urp);
		ENCODEZ(deltaS);
		changes |= NEW_U;
	} else if (th->th_urp != oth->th_urp)
/*
 * argh! URG not set but urp changed -- a sensible
 * implementation should never do this but RFC793
 * doesn't prohibit the change so we have to deal
 * with it.
 */
		goto uncompressed;

	deltaS = (ushort_t)(ntohs(th->th_win) - ntohs(oth->th_win));
	if (deltaS) {
		ENCODE(deltaS);
		changes |= NEW_W;
	}

	deltaA = ntohl(th->th_ack) - ntohl(oth->th_ack);
	if (deltaA) {
		if (deltaA > 0xffff)
			goto uncompressed;
		ENCODE(deltaA);
		changes |= NEW_A;
	}

	deltaS = ntohl(th->th_seq) - ntohl(oth->th_seq);
	if (deltaS) {
		if (deltaS > 0xffff)
			goto uncompressed;
		ENCODE(deltaS);
		changes |= NEW_S;
	}

	switch (changes) {

	case 0:
/*
 * Nothing changed. If this packet contains data and the
 * last one didn't, this is probably a data packet following
 * an ack (normal on an interactive connection) and we send
 * it compressed.  Otherwise it's probably a retransmit,
 * retransmitted ack or window probe.  Send it uncompressed
 * in case the other side missed the compressed version.
 */
		if (ip->ip_len != cs->cs_ip.ip_len &&
		    ntohs(cs->cs_ip.ip_len) == hlen)
			break;
		goto uncompressed;

	case SPECIAL_I:
	case SPECIAL_D:
		/*
		 * actual changes match one of our special case encodings --
		 * send packet uncompressed.
		 */
		goto uncompressed;

	case NEW_S|NEW_A:
		if (deltaS == deltaA &&
		    deltaS == ntohs(cs->cs_ip.ip_len) - hlen) {
			/* special case for echoed terminal traffic */
			changes = SPECIAL_I;
			cp = new_seq;
		}
		break;

	case NEW_S:
		if (deltaS == ntohs(cs->cs_ip.ip_len) - hlen) {
			/* special case for data xfer */
			changes = SPECIAL_D;
			cp = new_seq;
		}
		break;
	}

	deltaS = ntohs(ip->ip_id) - ntohs(cs->cs_ip.ip_id);
	if (deltaS != 1) {
		ENCODEZ(deltaS);
		changes |= NEW_I;
	}
	if (th->th_flags & TH_PUSH)
		changes |= TCP_PUSH_BIT;
	/*
	 * Grab the cksum before we overwrite it below.	 Then update our
	 * state with this packet's header.
	 */
	deltaA = ntohs(th->th_sum);
	bcopy(ip, &cs->cs_ip, hlen);

	/*
	 * We want to use the original packet as our compressed packet.
	 * (cp - new_seq) is the number of bytes we need for compressed
	 * sequence numbers.  In addition we need one byte for the change
	 * mask, one for the connection id and two for the tcp checksum.
	 * So, (cp - new_seq) + 4 bytes of header are needed.  hlen is how
	 * many bytes of the original packet to toss so subtract the two to
	 * get the new packet size.
	 */
	deltaS = cp - new_seq;

	newmp = allocb(EXTRA_ALLOC + MAX_COMP_HDR, BPRI_HI);
	if (newmp == NULL) {
		return (TYPE_BAD_TCP);
	}

	newmp->b_wptr += EXTRA_ALLOC;
	newmp->b_rptr += EXTRA_ALLOC;
	cp = (uchar_t *)newmp->b_wptr;
	linkb(newmp, mp);
	mp->b_rptr += hlen;

	if (comp->compslotout == 0 || comp->last_xmit != cs->cs_id) {
		comp->last_xmit = cs->cs_id;
		*cp++ = changes | NEW_C;
		*cp++ = cs->cs_id;
	} else {
		*cp++ = (uchar_t)changes;
	}
	*cp++ = (uchar_t)(deltaA >> 8);
	*cp++ = (uchar_t)deltaA;
	bcopy(new_seq, cp, deltaS);
	newmp->b_wptr = cp + deltaS;
	*ptrmp = newmp;
	INCR(sls_compressed)
	return (TYPE_COMPRESSED_TCP);

	/*
	 * Update connection state cs & send uncompressed packet ('uncompressed'
	 * means a regular ip/tcp packet but with the 'conversation id' we hope
	 * to use on future compressed packets in the protocol field).
	 */
uncompressed:
	bcopy(ip, &cs->cs_ip, hlen);
	newmp = msgpullup(mp, sizeof (struct ip));
	if (newmp == NULL) {
		return (TYPE_BAD_TCP);
	}
	freemsg(mp);
	*ptrmp = newmp;
	ip = (struct ip *)newmp->b_rptr;
	ip->ip_p = cs->cs_id;
	comp->last_xmit = cs->cs_id;
	return (TYPE_UNCOMPRESSED_TCP);
}


/*
 * vjcompress_tcp
 *
 * Compress tcp packets.  Assumes message block has been pulled up.
 */
int
vjuncompress_tcp(mblk_t **mp, uint_t type, vjstruct_t *comp)
{
	uchar_t *cp;
	uint_t hlen, changes;
	struct tcphdr *th;
	struct cstate *cs;
	struct ip *ip;
	mblk_t *dmp, *newmp;

	dmp = *mp;

	switch (type) {

	case TYPE_UNCOMPRESSED_TCP:
		ip = (struct ip *)dmp->b_rptr;
		if (ip->ip_p >= comp->maxslotin)
			goto bad;
		cs = &comp->rstate[comp->last_recv = ip->ip_p];
		comp->flags &= ~SLF_TOSS;
		ip->ip_p = IPPROTO_TCP;
		hlen = ip->ip_hl;
		hlen += ((struct tcphdr *)&((int *)ip)[hlen])->th_off;
		hlen <<= 2;
		bcopy(ip, &cs->cs_ip, hlen);
		cs->cs_ip.ip_sum = 0;
		cs->cs_hlen = (uchar_t)hlen;
		INCR(sls_uncompressedin)
		return (0);

	default:
		goto bad;

	case TYPE_COMPRESSED_TCP:
		break;
	}
	/* We've got a compressed packet. */
	INCR(sls_compressedin)
	cp = dmp->b_rptr;
	changes = *cp++;
	if (changes & NEW_C) {
/*
 * Make sure the state index is in range, then grab the state.
 * If we have a good state index, clear the 'discard' flag.
 */
		if (*cp >= comp->maxslotin) {
			goto bad;
		}
		comp->flags &= ~SLF_TOSS;
		comp->last_recv = *cp++;
	} else {
/*
 * this packet has an implicit state index.  If we've
 * had a line error since the last time we got an
 * explicit state index, we have to toss the packet.
 */
		if (comp->flags & SLF_TOSS) {
			INCR(sls_tossed)
			return (TYPE_BAD_TCP);
		}
	}
	cs = &comp->rstate[comp->last_recv];
	hlen = cs->cs_ip.ip_hl << 2;
	th = (struct tcphdr *)&((uchar_t *)&cs->cs_ip)[hlen];
	th->th_sum  = htons((((ushort_t)*cp) << 8) + (ushort_t)cp[1]);
	cp += 2;
	if (changes & TCP_PUSH_BIT)
		th->th_flags |= TH_PUSH;
	else
		th->th_flags &= ~TH_PUSH;

	switch (changes & SPECIALS_MASK) {
	case SPECIAL_I:
		{
		uint_t i = ntohs(cs->cs_ip.ip_len) - cs->cs_hlen;
		th->th_ack = htonl(ntohl(th->th_ack) + i);
		th->th_seq = htonl(ntohl(th->th_seq) + i);
		}
		break;

	case SPECIAL_D:
		th->th_seq = htonl(ntohl(th->th_seq) +
		    (ushort_t)ntohs(cs->cs_ip.ip_len)
		    - (uint32_t)cs->cs_hlen);
		break;

	default:
		if (changes & NEW_U) {
			th->th_flags |= TH_URG;
			DECODEU(th->th_urp)
		} else
			th->th_flags &= ~TH_URG;
		if (changes & NEW_W)
			DECODES(th->th_win)
		if (changes & NEW_A)
			DECODEL(th->th_ack)
		if (changes & NEW_S)
			DECODEL(th->th_seq)
		break;
	}
	if (changes & NEW_I) {
		DECODES(cs->cs_ip.ip_id)
	} else
		cs->cs_ip.ip_id = htons(ntohs(cs->cs_ip.ip_id) + 1);

	/*
	 * At this point, cp points to the first byte of data in the
	 * packet.  If we're not aligned on a 4-byte boundary, copy the
	 * data down so the ip & tcp headers will be aligned.  Then back up
	 * cp by the tcp/ip header length to make room for the reconstructed
	 * header (we assume the packet we were handed has enough space to
	 * prepend 128 bytes of header).  Adjust the length to account for
	 * the new header & fill in the IP total length.
	 */
	dmp->b_rptr = cp;
	newmp = allocb(cs->cs_hlen, BPRI_HI);
	if (newmp == NULL) {
		return (TYPE_BAD_TCP);
	}

	cp = newmp->b_wptr;
	cs->cs_ip.ip_len = htons((ushort_t)msgdsize(dmp) + cs->cs_hlen);
	bcopy(&cs->cs_ip, newmp->b_wptr, cs->cs_hlen);
	newmp->b_wptr += cs->cs_hlen;
	linkb(newmp, dmp);

	/* recompute the ip header checksum */
	{
		ushort_t *bp = (ushort_t *)cp;
		for (changes = 0; hlen != 0; hlen -= 2)
			changes += *bp++;
		changes = (changes & 0xffff) + (changes >> 16);
		changes = (changes & 0xffff) + (changes >> 16);
		((struct ip *)cp)->ip_sum = ~ changes;
	}
	*mp = msgpullup(newmp, -1);
	freemsg(newmp);
	return (0);
bad:
	comp->flags |= SLF_TOSS;
	INCR(sls_errorin)
	return (TYPE_BAD_TCP);
}
