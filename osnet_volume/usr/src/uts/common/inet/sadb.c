/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sadb.c	1.6	99/10/21 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/stream.h>
#include <sys/strlog.h>
#include <sys/kmem.h>
#include <sys/sunddi.h>
#include <sys/tihdr.h>
#include <sys/ksynch.h>
#include <sys/atomic.h>
#include <sys/socket.h>
#include <sys/sysmacros.h>
#include <netinet/in.h>
#include <net/pfkeyv2.h>
#include <inet/common.h>
#include <netinet/ip6.h>
#include <inet/ip.h>
#include <inet/ipsec_info.h>
#include <inet/sadb.h>

/*
 * For generic zero-address comparison.  This should be as large as the
 * ipsa_*addr field, and initialized to all-zeroes.  Globals are, so there
 * is no initialization required.
 *
 * XXX IPv6 - IPv6 may have such a field already in place.  Don't duplicate
 * code!
 */
static uint8_t zeroes[16];

/*
 * This macro is used to generate unique ids (along with the addresses) for
 * outbound datagrams that require unique SAs.
 */
#define	SA_FORM_UNIQUE_ID(io)				\
	((io)->ipsec_out_src_port |			\
	    ((io)->ipsec_out_dst_port  << 16) |		\
	    ((uint64_t)((io)->ipsec_out_proto) << 32))
/*
 * This source file contains Security Association Database (SADB) common
 * routines.  They are linked in with the keysock driver.  Keysock itself
 * may not have a lot of SA tables per se, but it seemed the logical place
 * to put these common routines.
 */

/*
 * Callers of this function have already created a working security
 * association, and have found the appropriate table & hash chain.  All this
 * function does is check duplicates, and insert the SA.  The caller needs to
 * hold the hash bucket lock and increment the refcnt before insertion.
 *
 * Return 0 if success, EEXIST if collision.
 */
int
sadb_insertassoc(ipsa_t *ipsa, isaf_t *bucket)
{
	ipsa_t **ptpn = NULL;
	ipsa_t *walker;
	boolean_t unspecsrc;

	ASSERT(MUTEX_HELD(&bucket->isaf_lock));

	unspecsrc = (bcmp(ipsa->ipsa_srcaddr, zeroes, ipsa->ipsa_addrlen) == 0);

	walker = bucket->isaf_ipsa;

	/*
	 * Find insertion point (pointed to with **ptpn).  Insert at the head
	 * of the list unless there's an unspecified source address, then
	 * insert it after the last SA with a specified source address.
	 *
	 * BTW, you'll have to walk the whole chain, matching on {DST, SPI}
	 * checking for collisions.
	 */

	while (walker != NULL) {
		if (walker->ipsa_spi == ipsa->ipsa_spi &&
		    bcmp(walker->ipsa_dstaddr, ipsa->ipsa_dstaddr,
			ipsa->ipsa_addrlen) == 0) {
			return (EEXIST);
		}

		if (ptpn == NULL && unspecsrc) {
			if (bcmp(walker->ipsa_srcaddr, zeroes,
			    ipsa->ipsa_addrlen) == 0)
				ptpn = walker->ipsa_ptpn;
			else if (walker->ipsa_next == NULL)
				ptpn = &walker->ipsa_next;
		}

		walker = walker->ipsa_next;
	}

	if (ptpn == NULL)
		ptpn = &bucket->isaf_ipsa;
	ipsa->ipsa_next = *ptpn;
	ipsa->ipsa_ptpn = ptpn;
	if (ipsa->ipsa_next != NULL)
		ipsa->ipsa_next->ipsa_ptpn = &ipsa->ipsa_next;
	*ptpn = ipsa;
	ipsa->ipsa_linklock = &bucket->isaf_lock;

	return (0);
}

/*
 * Free a security association.  Its reference count is 0, which means
 * I must free it.  The SA's lock must be held, too.
 */
static void
sadb_freeassoc(ipsa_t *ipsa)
{
	ASSERT(MUTEX_HELD(&ipsa->ipsa_lock));
	ASSERT(ipsa->ipsa_refcnt == 0);
	ASSERT(ipsa->ipsa_next == NULL);
	ASSERT(ipsa->ipsa_ptpn == NULL);

	if (ipsa->ipsa_commonstore != NULL) {
		/*
		 * Reality check that all of the component lengths add
		 * up to the length of common storage.
		 */
		ASSERT(ipsa->ipsa_commonstorelen ==
		    ipsa->ipsa_authkeylen + ipsa->ipsa_encrkeylen +
		    ipsa->ipsa_ivlen +
		    ((ipsa->ipsa_src_cid == NULL) ?
			0 : (strlen(ipsa->ipsa_src_cid) + 1)) +
		    ((ipsa->ipsa_dst_cid == NULL) ?
			0 : (strlen(ipsa->ipsa_dst_cid) + 1)) +
		    ((ipsa->ipsa_proxy_cid == NULL) ?
			0 : (strlen(ipsa->ipsa_proxy_cid) + 1)) +
		    ipsa->ipsa_authmisclen + ipsa->ipsa_encrmisclen +
		    ipsa->ipsa_integlen + ipsa->ipsa_senslen);

		if (ipsa->ipsa_authkey != NULL)
			bzero(ipsa->ipsa_authkey, ipsa->ipsa_authkeylen);
		if (ipsa->ipsa_encrkey != NULL)
			bzero(ipsa->ipsa_encrkey, ipsa->ipsa_encrkeylen);
		if (ipsa->ipsa_authmisc != NULL)
			bzero(ipsa->ipsa_authmisc, ipsa->ipsa_authmisclen);
		if (ipsa->ipsa_encrmisc != NULL)
			bzero(ipsa->ipsa_encrmisc, ipsa->ipsa_encrmisclen);
		kmem_free(ipsa->ipsa_commonstore, ipsa->ipsa_commonstorelen);
	} else {
		/* bzero() these fields for paranoia's sake. */
		if (ipsa->ipsa_authkey != NULL) {
			bzero(ipsa->ipsa_authkey, ipsa->ipsa_authkeylen);
			kmem_free(ipsa->ipsa_authkey, ipsa->ipsa_authkeylen);
		}
		if (ipsa->ipsa_authmisc != NULL) {
			bzero(ipsa->ipsa_authmisc, ipsa->ipsa_authmisclen);
			kmem_free(ipsa->ipsa_authmisc, ipsa->ipsa_authmisclen);
		}
		if (ipsa->ipsa_encrkey != NULL) {
			bzero(ipsa->ipsa_encrkey, ipsa->ipsa_encrkeylen);
			kmem_free(ipsa->ipsa_encrkey, ipsa->ipsa_encrkeylen);
		}
		if (ipsa->ipsa_encrmisc != NULL) {
			bzero(ipsa->ipsa_encrmisc, ipsa->ipsa_encrmisclen);
			kmem_free(ipsa->ipsa_encrmisc, ipsa->ipsa_encrmisclen);
		}

		if (ipsa->ipsa_iv != NULL)
			kmem_free(ipsa->ipsa_iv, ipsa->ipsa_ivlen);
		if (ipsa->ipsa_src_cid != NULL)
			kmem_free(ipsa->ipsa_src_cid,
			    strlen(ipsa->ipsa_src_cid) + 1);
		if (ipsa->ipsa_dst_cid != NULL)
			kmem_free(ipsa->ipsa_dst_cid,
			    strlen(ipsa->ipsa_dst_cid) + 1);
		if (ipsa->ipsa_proxy_cid != NULL)
			kmem_free(ipsa->ipsa_proxy_cid,
			    strlen(ipsa->ipsa_proxy_cid) + 1);
		if (ipsa->ipsa_integ != NULL)
			kmem_free(ipsa->ipsa_integ, ipsa->ipsa_integlen);
		if (ipsa->ipsa_sens != NULL)
			kmem_free(ipsa->ipsa_sens, ipsa->ipsa_senslen);
	}

	mutex_destroy(&ipsa->ipsa_lock);
	kmem_free(ipsa, sizeof (*ipsa));
}

/*
 * Unlink a security association from a hash bucket.  Assume the hash bucket
 * lock is held, but the association's lock is not.
 */
void
sadb_unlinkassoc(ipsa_t *ipsa)
{
	ASSERT(ipsa->ipsa_linklock != NULL);
	ASSERT(MUTEX_HELD(ipsa->ipsa_linklock));

	/* These fields are protected by the link lock. */
	*(ipsa->ipsa_ptpn) = ipsa->ipsa_next;
	if (ipsa->ipsa_next != NULL) {
		ipsa->ipsa_next->ipsa_ptpn = ipsa->ipsa_ptpn;
		ipsa->ipsa_next = NULL;
	}

	ipsa->ipsa_ptpn = NULL;

	/* This may destroy the SA. */
	IPSA_REFRELE(ipsa);
}

/*
 * Create a larval security association with the specified SPI.  All other
 * fields are zeroed.
 */
static ipsa_t *
sadb_makelarvalassoc(uint32_t spi, uint8_t *src, uint8_t *dst, int addrlen)
{
	ipsa_t *newbie;

	/*
	 * Allocate...
	 */

	newbie = (ipsa_t *)kmem_zalloc(sizeof (ipsa_t), KM_NOSLEEP);
	if (newbie == NULL) {
		/* Can't make new larval SA. */
		return (NULL);
	}

	/* Assigned requested SPI, assume caller does SPI allocation magic. */
	newbie->ipsa_spi = spi;

	/*
	 * Copy addresses...
	 */

	bcopy(src, newbie->ipsa_srcaddr, addrlen);
	bcopy(dst, newbie->ipsa_dstaddr, addrlen);

	newbie->ipsa_addrlen = addrlen;

	/*
	 * Set common initialization values, including refcnt.
	 */
	mutex_init(&newbie->ipsa_lock, NULL, MUTEX_DEFAULT, NULL);
	newbie->ipsa_state = IPSA_STATE_LARVAL;
	newbie->ipsa_refcnt = 1;
	newbie->ipsa_freefunc = sadb_freeassoc;

	/*
	 * There aren't a lot of other common initialization values, as
	 * they are copied in from the PF_KEY message.
	 */

	return (newbie);
}

/*
 * Look up a security association based on the security parameters index (SPI)
 * and address(es).  This is used for inbound packets and general SA lookups
 * (even in outbound SA tables).  The source address may be ignored.  Return
 * NULL if no association is available.  If an SA is found, return it, with
 * its refcnt incremented.  The caller must REFRELE after using the SA.
 * The hash bucket must be locked down before calling.
 *
 * Assume addrlen is in bytes... for now.
 */
ipsa_t *
sadb_getassocbyspi(isaf_t *bucket, uint32_t spi, uint8_t *src, uint8_t *dst,
    int addrlen)
{
	ipsa_t *retval;

	ASSERT(MUTEX_HELD(&bucket->isaf_lock));

	/*
	 * Walk the hash bucket, matching exactly on SPI, then destination,
	 * then source.
	 *
	 * Per-SA locking doesn't need to happen, because I'm only matching
	 * on addresses.  Addresses are only changed during insertion/deletion
	 * from the hash bucket.  Since the hash bucket lock is held, we don't
	 * need to worry about addresses changing.
	 */

	for (retval = bucket->isaf_ipsa; retval != NULL;
	    retval = retval->ipsa_next) {
		if (retval->ipsa_spi != spi)
			continue;
		if (bcmp(dst, retval->ipsa_dstaddr, addrlen) != 0)
			continue;

		/*
		 * Assume that wildcard source addresses are inserted at the
		 * end of the hash bucket.  (See sadb_insertassoc().)
		 * The following check for source addresses is a weak form
		 * of access control/source identity verification.  If an
		 * SA has a source address, I only match an all-zeroes
		 * source address, or that particular one.  If the SA has
		 * an all-zeroes source, then I match regardless.
		 *
		 * There is a weakness here in that a packet with all-zeroes
		 * for an address will match.  It's possible that IP will
		 * flat-out reject such packets before kicking it up to
		 * anything calling this function.
		 */
		if (bcmp(src, retval->ipsa_srcaddr, addrlen) == 0 ||
		    bcmp(retval->ipsa_srcaddr, zeroes, addrlen) == 0 ||
		    bcmp(src, zeroes, addrlen) == 0)
			break;
	}

	if (retval != NULL) {
		/*
		 * Just refhold the return value.  The caller will call the
		 * sadb_set_usetime() below to set the USED flag, and
		 * the times.
		 */
		IPSA_REFHOLD(retval);
	}

	return (retval);
}

/*
 * Look up a security association based on the unique ID generated by IP and
 * transport information, such as ports and upper-layer protocol, and the
 * address(es).  Used for uniqueness testing and outbound packets.  The
 * source address may be ignored.
 *
 * I expect an SA hash bucket, and that its per-bucket mutex is held.
 * The SA ptr I return will have its reference count incremented by one.
 *
 * Addrlen is in bytes... for now.
 */
ipsa_t *
sadb_getassocbyipc(isaf_t *bucket, ipsec_out_t *io, uint8_t *src,
    uint8_t *dst, int addrlen, uint8_t protocol)
{
	ipsa_t *retval, *candidate;
	boolean_t need_unique;
	uint_t encr_alg, auth_alg;
	uint64_t unique_id;
	uint32_t old_flags;

	ASSERT(MUTEX_HELD(&bucket->isaf_lock));

	if (protocol == IPPROTO_AH) {
		auth_alg = io->ipsec_out_ah_alg;
		encr_alg = 0;
		need_unique = io->ipsec_out_ah_req & IPSEC_PREF_UNIQUE;
	} else {
		ASSERT(protocol == IPPROTO_ESP);
		auth_alg = io->ipsec_out_esp_ah_alg;
		encr_alg = io->ipsec_out_esp_alg;
		need_unique = io->ipsec_out_esp_req & IPSEC_PREF_UNIQUE;
	}

	if (need_unique) {
		/* Construct the unique id */
		unique_id = SA_FORM_UNIQUE_ID(io);
	} else {
		unique_id = 0;
	}


	/*
	 * Walk the hash bucket, matching on:
	 *
	 * - unique_id
	 * - destination
	 * - source
	 * - algorithms
	 * - <MORE TBD>
	 *
	 * Make sure that wildcard sources are inserted at the end of the hash
	 * bucket.
	 *
	 * DEFINITIONS:	A _shared_ SA is one with unique_id == 0 and USED.
	 *		An _unused_ SA is one with unique_id == 0 and not USED.
	 * 		A _unique_ SA is one with unique_id != 0 and USED.
	 *		An SA with unique_id != 0 and not USED never happens.
	 */

	candidate = NULL;

	for (retval = bucket->isaf_ipsa; retval != NULL;
	    retval = retval->ipsa_next) {

		/*
		 * Q: Should I lock this SA?
		 * A: For now, yes.  I change and use too many fields in here
		 *    (e.g. unique_id) that I may be racing with other threads.
		 *    Also, the refcnt needs to be bumped up.
		 */

		mutex_enter(&retval->ipsa_lock);

		/* My apologies for the gotos instead of the continues. */
		if (bcmp(dst, retval->ipsa_dstaddr, addrlen))
			goto next_ipsa;	/* Destination mismatch. */
		if (bcmp(retval->ipsa_srcaddr, zeroes, addrlen) &&
		    bcmp(src, retval->ipsa_srcaddr, addrlen))
			goto next_ipsa;	/* Specific source and not matched. */
		if (auth_alg != 0 && auth_alg != retval->ipsa_auth_alg)
			goto next_ipsa;	/* Algorithm mismatch. */
		if (encr_alg != 0 && encr_alg != retval->ipsa_encr_alg)
			goto next_ipsa;	/* Algorithm mismatch. */

		/*
		 * At this point, we know that we have at least a match on:
		 *
		 * - dest
		 * - source (if source is specified, i.e. non-zeroes)
		 * - auth alg (if auth alg is specified, i.e. non-zero)
		 * - encrypt. alg (if encrypt. alg is specified, i.e. non-zero)
		 *
		 * (Keep in mind known-src SAs are hit before zero-src SAs,
		 * thanks to insert.)
		 * If we need a unique asssociation, optimally we have
		 * a USED and ipsa_unique_id == unique_id, otherwise NOT USED
		 * is held in reserve (stored in candidate).
		 * If we don't need a unique association, optimally we have
		 * a USED and unique_id == 0, otherwise NOT USED is held
		 * in reserve (stored in candidate).
		 *
		 * For those stored in candidate, take best-match (i.e. given
		 * a choice, candidate should have non-zero ipsa_src).
		 *
		 * There is some debate here over whether or not this is
		 * a good thing, given that used SA's get precedence
		 * over shiny new ones (that may reveal less ciphertext for
		 * a given key).
		 */

		if (retval->ipsa_flags & IPSA_F_USED) {
			/*
			 * I matched a USED SA.
			 */
			if (need_unique) {
				if (unique_id == retval->ipsa_unique_id) {
					/*
					 * I found an SA that matches
					 * my requested unique ID.
					 */
					break;
				}
			} else {
				if (retval->ipsa_unique_id == 0) {
					/*
					 * I found a SHARED SA that I can use.
					 */
					break;
				}
			}
		} else {
			/*
			 * I matched an UNUSED SA.
			 */
			if (candidate == NULL) {
				/*
				 * Take the first UNUSED SA as a "candidate".
				 * Do this in case we find a USED SA that
				 * works better.
				 */
				candidate = retval;
			} else {
				/*
				 * If candidate's source address is zero and
				 * the current match (i.e. retval) address is
				 * not zero, re-assign candidate.
				 */
				if ((bcmp(candidate->ipsa_srcaddr, zeroes,
				    addrlen) == 0) &&
				    (bcmp(retval->ipsa_srcaddr, zeroes,
				    addrlen) != 0)) {
					mutex_exit(&candidate->ipsa_lock);
					candidate = retval;
				}
			}
		}

next_ipsa:
		/* Keep mutex on "candidate" entry. */
		if (candidate != retval)
			mutex_exit(&retval->ipsa_lock);
	}

	if (retval == NULL && candidate == NULL)
		return (NULL);

	if (retval == NULL) {
		ASSERT(MUTEX_HELD(&candidate->ipsa_lock));
		retval = candidate;
	} else if (candidate != NULL) {
		mutex_exit(&candidate->ipsa_lock);
	}

	/*
	 * Already holding both hash bucket lock and retval's mutex, so I have
	 * done the moral equivalent of IPSA_REFHOLD except the following
	 * lines.  (The mutex_exit is later.)
	 */
	retval->ipsa_refcnt++;
	ASSERT(retval->ipsa_refcnt != 0);

	/*
	 * This association is no longer unused.
	 */
	old_flags = retval->ipsa_flags;
	retval->ipsa_flags |= IPSA_F_USED;
	/*
	 * Set the uniqueness only first time.
	 */
	if (need_unique && !(old_flags & IPSA_F_USED)) {
		ASSERT(retval->ipsa_unique_id == 0);
		/*
		 * From now on, only this src, dst[ports, addr],
		 * proto, should use it.
		 */
		retval->ipsa_flags |= IPSA_F_UNIQUE;
		retval->ipsa_unique_id = unique_id;

		/*
		 * Set the source address and adjust the hash
		 * buckets only if src_addr is zero.
		 */
		if (bcmp(retval->ipsa_srcaddr, zeroes, addrlen) == 0) {
			/*
			 * sadb_unlinkassoc() will decrement the refcnt.  Bump
			 * up when we have the lock so that we don't have to
			 * acquire locks when we come back from
			 * sadb_insertassoc().
			 */
			retval->ipsa_refcnt++;
			ASSERT(retval->ipsa_refcnt != 0);
			bcopy(src, retval->ipsa_srcaddr, addrlen);
			mutex_exit(&retval->ipsa_lock);
			sadb_unlinkassoc(retval);
			/*
			 * Since the bucket lock is held, we know
			 * sadb_insertassoc() will succeed.
			 */
#ifdef DEBUG
			if (sadb_insertassoc(retval, bucket) != 0)
				cmn_err(CE_PANIC,
				    "sadb_insertassoc() failed in "
				    "sadb_getassocbyipc().\n");
#else	/* non-DEBUG */
			(void) sadb_insertassoc(retval, bucket);
#endif	/* DEBUG */
			return (retval);
		}
	}
	mutex_exit(&retval->ipsa_lock);

	/* Else let caller react to a lookup failure when it gets NULL. */

	return (retval);
}

/*
 * Call me to initialize a security association fanout.
 */
void
sadb_init(isaf_t *table, uint_t numentries)
{
	int i;

	for (i = 0; i < numentries; i++) {
		mutex_init(&(table[i].isaf_lock), NULL, MUTEX_DEFAULT, NULL);
		table[i].isaf_ipsa = NULL;
	}
}

/*
 * Deliver a single SADB_DUMP message representing a single SA.  This is
 * called many times by sadb_dump().
 *
 * If the return value of this is ENOBUFS (not the same as ENOMEM), then
 * the caller should take that as a hint that dupb() on the "original answer"
 * failed, and that perhaps the caller should try again with a copyb()ed
 * "original answer".
 */
static int
sadb_dump_deliver(queue_t *pfkey_q, mblk_t *original_answer, ipsa_t *ipsa,
    sadb_msg_t *samsg)
{
	mblk_t *answer;

	answer = dupb(original_answer);
	if (answer == NULL)
		return (ENOBUFS);
	answer->b_cont = sadb_sa2msg(ipsa, samsg);
	if (answer->b_cont == NULL) {
		freeb(answer);
		return (ENOMEM);
	}

	/* Just do a putnext, and let keysock deal with flow control. */
	putnext(pfkey_q, answer);
	return (0);
}

/*
 * Perform an SADB_DUMP, spewing out every SA in an array of SA fanouts
 * to keysock.
 */
int
sadb_dump(queue_t *pfkey_q, mblk_t *mp, minor_t serial, isaf_t *fanout,
    int num_entries, boolean_t do_peers)
{
	int i, error = 0;
	mblk_t *original_answer;
	ipsa_t *walker;
	keysock_out_t *kso;
	sadb_msg_t *samsg;

	/*
	 * For each IPSA hash bucket do:
	 *	- Hold the mutex
	 *	- Walk each entry, doing an sadb_dump_deliver() on it.
	 */
	ASSERT(mp->b_cont != NULL);
	samsg = (sadb_msg_t *)mp->b_cont->b_rptr;

	original_answer = allocb(sizeof (ipsec_info_t), BPRI_HI);
	if (original_answer == NULL) {
		return (ENOMEM);
	}
	original_answer->b_datap->db_type = M_CTL;
	original_answer->b_wptr += sizeof (ipsec_info_t);
	kso = (keysock_out_t *)original_answer->b_rptr;
	kso->ks_out_type = KEYSOCK_OUT;
	kso->ks_out_len = sizeof (*kso);
	kso->ks_out_serial = serial;

	for (i = 0; i < num_entries; i++) {
		mutex_enter(&fanout[i].isaf_lock);
		for (walker = fanout[i].isaf_ipsa; walker != NULL;
		    walker = walker->ipsa_next) {
			if (!do_peers && walker->ipsa_haspeer)
				continue;
			error = sadb_dump_deliver(pfkey_q, original_answer,
			    walker, samsg);
			if (error == ENOBUFS) {
				mblk_t *new_original_answer;

				/* Ran out of dupb's.  Try a copyb. */
				new_original_answer = copyb(original_answer);
				if (new_original_answer == NULL) {
					error = ENOMEM;
				} else {
					freeb(original_answer);
					original_answer = new_original_answer;
					error = sadb_dump_deliver(pfkey_q,
					    original_answer, walker, samsg);
				}
			}
			if (error != 0)
				break;	/* out of for loop. */
		}
		mutex_exit(&fanout[i].isaf_lock);
		if (error != 0)
			break;	/* out of for loop. */
	}

	freeb(original_answer);
	return (error);
}

/*
 * Call me to de-initialize a security association fanout.
 */
static void
sadb_destroyer(isaf_t *table, uint_t numentries, boolean_t forever)
{
	int i;

	for (i = 0; i < numentries; i++) {
		mutex_enter(&table[i].isaf_lock);
		while (table[i].isaf_ipsa != NULL)
			sadb_unlinkassoc(table[i].isaf_ipsa);
		mutex_exit(&table[i].isaf_lock);
		if (forever)
			mutex_destroy(&(table[i].isaf_lock));
	}
}

void
sadb_flush(isaf_t *fanout, int num_entries)
{
	sadb_destroyer(fanout, num_entries, B_FALSE);
}

void
sadb_destroy(isaf_t *table, uint_t numentries)
{
	sadb_destroyer(table, numentries, B_TRUE);
}

/*
 * Check hard vs. soft lifetimes.
 */
boolean_t
sadb_hardsoftchk(sadb_lifetime_t *hard, sadb_lifetime_t *soft)
{
	if (hard == NULL || soft == NULL)
		return (B_TRUE);

	if (hard->sadb_lifetime_allocations != 0 &&
	    soft->sadb_lifetime_allocations != 0 &&
	    hard->sadb_lifetime_allocations < soft->sadb_lifetime_allocations)
		return (B_FALSE);

	if (hard->sadb_lifetime_bytes != 0 &&
	    soft->sadb_lifetime_bytes != 0 &&
	    hard->sadb_lifetime_bytes < soft->sadb_lifetime_bytes)
		return (B_FALSE);

	if (hard->sadb_lifetime_addtime != 0 &&
	    soft->sadb_lifetime_addtime != 0 &&
	    hard->sadb_lifetime_addtime < soft->sadb_lifetime_addtime)
		return (B_FALSE);

	if (hard->sadb_lifetime_usetime != 0 &&
	    soft->sadb_lifetime_usetime != 0 &&
	    hard->sadb_lifetime_usetime < soft->sadb_lifetime_usetime)
		return (B_FALSE);

	return (B_TRUE);
}

/*
 * Clone a security association for the purposes of inserting two
 * into inbound and outbound tables respectively.
 */
static ipsa_t *
sadb_cloneassoc(ipsa_t *ipsa)
{
	ipsa_t *newbie;
	boolean_t error = B_FALSE;

	ASSERT(!MUTEX_HELD(&ipsa->ipsa_lock));

	newbie = kmem_alloc(sizeof (ipsa_t), KM_NOSLEEP);
	if (newbie == NULL)
		return (NULL);

	/* Copy over what we can. */
	*newbie = *ipsa;

	/* bzero and initialize locks, in case *_init() allocates... */
	mutex_init(&newbie->ipsa_lock, NULL, MUTEX_DEFAULT, NULL);

	/*
	 * XXX While somewhat dain-bramaged, the most graceful way to
	 * recover from errors is to keep plowing through the allocations,
	 * and getting what I can.  It's easier to call sadb_freeassoc() on
	 * the stillborn clone when all the pointers aren't pointing to
	 * the parent's data.
	 */

	if (ipsa->ipsa_authkey != NULL) {
		newbie->ipsa_authkey = kmem_alloc(newbie->ipsa_authkeylen,
		    KM_NOSLEEP);
		if (newbie->ipsa_authkey == NULL)
			error = B_TRUE;
		else
			bcopy(ipsa->ipsa_authkey, newbie->ipsa_authkey,
			    newbie->ipsa_authkeylen);
	}

	if (ipsa->ipsa_encrkey != NULL) {
		newbie->ipsa_encrkey = kmem_alloc(newbie->ipsa_encrkeylen,
		    KM_NOSLEEP);
		if (newbie->ipsa_encrkey == NULL)
			error = B_TRUE;
		else
			bcopy(ipsa->ipsa_encrkey, newbie->ipsa_encrkey,
			    newbie->ipsa_encrkeylen);
	}

	if (ipsa->ipsa_iv != NULL) {
		newbie->ipsa_iv = kmem_alloc(newbie->ipsa_ivlen,
		    KM_NOSLEEP);
		if (newbie->ipsa_iv == NULL)
			error = B_TRUE;
		else
			bcopy(ipsa->ipsa_iv, newbie->ipsa_iv,
			    newbie->ipsa_ivlen);
	}

	if (ipsa->ipsa_authmisc != NULL) {
		newbie->ipsa_authmisc = kmem_alloc(newbie->ipsa_authmisclen,
		    KM_NOSLEEP);
		if (newbie->ipsa_authmisc == NULL)
			error = B_TRUE;
		else
			bcopy(ipsa->ipsa_authmisc, newbie->ipsa_authmisc,
			    newbie->ipsa_authmisclen);
	}

	if (ipsa->ipsa_encrmisc != NULL) {
		newbie->ipsa_encrmisc = kmem_alloc(newbie->ipsa_encrmisclen,
		    KM_NOSLEEP);
		if (newbie->ipsa_encrmisc == NULL)
			error = B_TRUE;
		else
			bcopy(ipsa->ipsa_encrmisc, newbie->ipsa_encrmisc,
			    newbie->ipsa_encrmisclen);
	}

	if (ipsa->ipsa_integ != NULL) {
		newbie->ipsa_integ = kmem_alloc(newbie->ipsa_integlen,
		    KM_NOSLEEP);
		if (newbie->ipsa_integ == NULL)
			error = B_TRUE;
		else
			bcopy(ipsa->ipsa_integ, newbie->ipsa_integ,
			    newbie->ipsa_integlen);
	}

	if (ipsa->ipsa_sens != NULL) {
		newbie->ipsa_sens = kmem_alloc(newbie->ipsa_senslen,
		    KM_NOSLEEP);
		if (newbie->ipsa_sens == NULL)
			error = B_TRUE;
		else
			bcopy(ipsa->ipsa_sens, newbie->ipsa_sens,
			    newbie->ipsa_senslen);
	}

	if (ipsa->ipsa_src_cid != NULL) {
		/*
		 * ASSUME that ipsa_*_cid is a proper C string.  See
		 * ext_check() in keysock.c for why I can make this assumption.
		 */
		newbie->ipsa_src_cid = kmem_alloc(
		    strlen(ipsa->ipsa_src_cid) + 1, KM_NOSLEEP);
		if (newbie->ipsa_src_cid == NULL)
			error = B_TRUE;
		else
			(void) strcpy(newbie->ipsa_src_cid, ipsa->ipsa_src_cid);
	}

	if (ipsa->ipsa_dst_cid != NULL) {
		/*
		 * ASSUME that ipsa_*_cid is a proper C string.  See
		 * ext_check() in keysock.c for why I can make this assumption.
		 */
		newbie->ipsa_dst_cid = kmem_alloc(
		    strlen(ipsa->ipsa_dst_cid) + 1, KM_NOSLEEP);
		if (newbie->ipsa_dst_cid == NULL)
			error = B_TRUE;
		else
			(void) strcpy(newbie->ipsa_dst_cid, ipsa->ipsa_dst_cid);
	}

#if 0 /* XXX PROXY  - Proxy identities not supported yet. */
	if (ipsa->ipsa_proxy_cid != NULL) {
		/*
		 * ASSUME that ipsa_*_cid is a proper C string.  See
		 * ext_check() in keysock.c for why I can make this assumption.
		 */
		newbie->ipsa_proxy_cid = kmem_alloc(
		    strlen(ipsa->ipsa_proxy_cid) + 1, KM_NOSLEEP);
		if (newbie->ipsa_proxy_cid == NULL)
			error = B_TRUE;
		else
			(void) strcpy(newbie->ipsa_proxy_cid,
			    ipsa->ipsa_proxy_cid);
	}
#endif /* XXX PROXY */

	if (error) {
		sadb_freeassoc(newbie);
		return (NULL);
	}

	return (newbie);
}

/*
 * Given an original message header, and an SA, construct a full PF_KEY
 * message with all of the relevant extensions.  This is mostly used for
 * SADB_GET, and SADB_DUMP.
 */
mblk_t *
sadb_sa2msg(ipsa_t *ipsa, sadb_msg_t *samsg)
{
	int alloclen, addrsize, authsize, encrsize, srcidsize, dstidsize;
	sa_family_t fam;	/* Address family for SADB_EXT_ADDRESS */
				/* sockaddrs. */
	/*
	 * The following are pointers into the PF_KEY message this PF_KEY
	 * message creates.
	 */
	sadb_msg_t *newsamsg;
	sadb_sa_t *assoc;
	sadb_lifetime_t *lt;
	sadb_address_t *addr;
	struct sockaddr_in *sin;
	sadb_key_t *key;
	sadb_ident_t *ident;
	sadb_sens_t *sens;
	sadb_ext_t *walker;	/* For when we need a generic ext. pointer. */
	mblk_t *mp;
	uint64_t *bitmap;
	/* These indicate the presence of the above extension fields. */
	boolean_t soft, hard, proxy, auth, encr, sensinteg, srcid, dstid;
#if 0 /* XXX PROXY see below... */
	boolean_t proxyid, iv;
	int proxyidsize, ivsize;
#endif /* XXX PROXY */

	/* First off, figure out the allocation length for this message. */

	/*
	 * Constant stuff.  This includes base, SA, address (src, dst)
	 * (Determine sockaddr AF_? from addrlen...), and lifetime (current).
	 */
	alloclen = sizeof (sadb_msg_t) + sizeof (sadb_sa_t) +
	    sizeof (sadb_lifetime_t);

	switch (ipsa->ipsa_addrlen) {
	case sizeof (ipaddr_t):
		addrsize = roundup(sizeof (struct sockaddr_in) +
		    sizeof (sadb_address_t), sizeof (uint64_t));
		/*
		 * Allocate TWO address extensions, for source and destination.
		 * (Thus, the * 2.)
		 */
		alloclen += addrsize * 2;
		fam = AF_INET;
		break;
	default:
		return (NULL);
	}

	/* How 'bout other lifetimes? */
	if (ipsa->ipsa_softaddlt != 0 || ipsa->ipsa_softuselt != 0 ||
	    ipsa->ipsa_softbyteslt != 0 || ipsa->ipsa_softalloc != 0) {
		alloclen += sizeof (sadb_lifetime_t);
		soft = B_TRUE;
	} else {
		soft = B_FALSE;
	}

	if (ipsa->ipsa_hardaddlt != 0 || ipsa->ipsa_harduselt != 0 ||
	    ipsa->ipsa_hardbyteslt != 0 || ipsa->ipsa_hardalloc != 0) {
		alloclen += sizeof (sadb_lifetime_t);
		hard = B_TRUE;
	} else {
		hard = B_FALSE;
	}

	/* Proxy address? */
	if (bcmp(zeroes, ipsa->ipsa_proxyaddr, ipsa->ipsa_addrlen) != 0) {
		alloclen += addrsize;
		proxy = B_TRUE;
	} else {
		proxy = B_FALSE;
	}

	/* For the following fields, assume that length != 0 ==> stuff */
	if (ipsa->ipsa_authkeylen != 0) {
		authsize = roundup(sizeof (sadb_key_t) + ipsa->ipsa_authkeylen,
		    sizeof (uint64_t));
		alloclen += authsize;
		auth = B_TRUE;
	} else {
		auth = B_FALSE;
	}

	if (ipsa->ipsa_encrkeylen != 0) {
		encrsize = roundup(sizeof (sadb_key_t) + ipsa->ipsa_encrkeylen,
		    sizeof (uint64_t));
		alloclen += encrsize;
		encr = B_TRUE;
	} else {
		encr = B_FALSE;
	}

#if 0 /* NOT NEEDED YET */
	if (ipsa->ipsa_ivlen != 0) {
		ivsize = roundup(sizeof (sadb_key_t) + ipsa->ipsa_ivlen,
		    sizeof (uint64_t));
		alloclen += ivsize;
		iv = B_TRUE;
	} else {
		iv = B_FALSE;
	}
#endif /* NOT NEEDED YET */

	/* No need for roundup on sens and integ. */
	if (ipsa->ipsa_integlen != 0 || ipsa->ipsa_senslen != 0) {
		alloclen += sizeof (sadb_key_t) + ipsa->ipsa_integlen +
		    ipsa->ipsa_senslen;
		sensinteg = B_TRUE;
	} else {
		sensinteg = B_FALSE;
	}

	/*
	 * Must use strlen() here for lengths.  Identities use NULL pointers
	 * to indicate their existence.
	 */
	if (ipsa->ipsa_src_cid != NULL) {
		srcidsize = roundup(sizeof (sadb_ident_t) +
		    strlen(ipsa->ipsa_src_cid) + 1,
		    sizeof (uint64_t));
		alloclen += srcidsize;
		srcid = B_TRUE;
	} else {
		srcid = B_FALSE;
	}

	if (ipsa->ipsa_dst_cid != NULL) {
		dstidsize = roundup(sizeof (sadb_ident_t) +
		    strlen(ipsa->ipsa_dst_cid) + 1,
		    sizeof (uint64_t));
		alloclen += dstidsize;
		dstid = B_TRUE;
	} else {
		dstid = B_FALSE;
	}

#if 0 /* XXX PROXY not yet. */
	if (ipsa->ipsa_proxy_cid != NULL) {
		proxyidsize = roundup(sizeof (sadb_ident_t) +
		    strlen(ipsa->ipsa_proxy_cid) + 1,
		    sizeof (uint64_t));
		alloclen += proxyidsize;
		proxyid = B_TRUE;
	} else {
		proxyid = B_FALSE;
	}
#endif /* XXX PROXY */

	/* Make sure the allocation length is a multiple of 8 bytes. */
	ASSERT((alloclen & 0x7) == 0);

	/* XXX Possibly make it esballoc, with a bzero-ing free_ftn. */
	mp = allocb(alloclen, BPRI_HI);
	if (mp == NULL)
		return (NULL);

	mp->b_wptr += alloclen;
	newsamsg = (sadb_msg_t *)mp->b_rptr;
	*newsamsg = *samsg;
	newsamsg->sadb_msg_len = (uint16_t)SADB_8TO64(alloclen);

	mutex_enter(&ipsa->ipsa_lock);	/* Since I'm grabbing SA fields... */

	newsamsg->sadb_msg_satype = ipsa->ipsa_type;

	assoc = (sadb_sa_t *)(newsamsg + 1);
	assoc->sadb_sa_len = SADB_8TO64(sizeof (*assoc));
	assoc->sadb_sa_exttype = SADB_EXT_SA;
	assoc->sadb_sa_spi = ipsa->ipsa_spi;
	assoc->sadb_sa_replay = ipsa->ipsa_replay_wsize;
	assoc->sadb_sa_state = ipsa->ipsa_state;
	assoc->sadb_sa_auth = ipsa->ipsa_auth_alg;
	assoc->sadb_sa_encrypt = ipsa->ipsa_encr_alg;
	assoc->sadb_sa_flags = ipsa->ipsa_flags;

	lt = (sadb_lifetime_t *)(assoc + 1);
	lt->sadb_lifetime_len = SADB_8TO64(sizeof (*lt));
	lt->sadb_lifetime_exttype = SADB_EXT_LIFETIME_CURRENT;
	lt->sadb_lifetime_allocations = ipsa->ipsa_alloc;
	lt->sadb_lifetime_bytes = ipsa->ipsa_bytes;
	lt->sadb_lifetime_addtime = ipsa->ipsa_addtime;
	lt->sadb_lifetime_usetime = ipsa->ipsa_usetime;

	if (hard) {
		lt++;
		lt->sadb_lifetime_len = SADB_8TO64(sizeof (*lt));
		lt->sadb_lifetime_exttype = SADB_EXT_LIFETIME_HARD;
		lt->sadb_lifetime_allocations = ipsa->ipsa_hardalloc;
		lt->sadb_lifetime_bytes = ipsa->ipsa_hardbyteslt;
		lt->sadb_lifetime_addtime = ipsa->ipsa_hardaddlt;
		lt->sadb_lifetime_usetime = ipsa->ipsa_harduselt;
	}

	if (soft) {
		lt++;
		lt->sadb_lifetime_len = SADB_8TO64(sizeof (*lt));
		lt->sadb_lifetime_exttype = SADB_EXT_LIFETIME_SOFT;
		lt->sadb_lifetime_allocations = ipsa->ipsa_softalloc;
		lt->sadb_lifetime_bytes = ipsa->ipsa_softbyteslt;
		lt->sadb_lifetime_addtime = ipsa->ipsa_softaddlt;
		lt->sadb_lifetime_usetime = ipsa->ipsa_softuselt;
	}

	addr = (sadb_address_t *)(lt + 1);
	addr->sadb_address_len = SADB_8TO64(addrsize);
	addr->sadb_address_proto = 0;
	addr->sadb_address_prefixlen = 0;
	addr->sadb_address_reserved = 0;
	addr->sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
	switch (fam) {
	case AF_INET:
		sin = (struct sockaddr_in *)(addr + 1);
		sin->sin_family = fam;
		bzero(sin->sin_zero, sizeof (sin->sin_zero));
		sin->sin_port = 0;
		bcopy(ipsa->ipsa_srcaddr, &sin->sin_addr, ipsa->ipsa_addrlen);
		break;
	}

	addr = (sadb_address_t *)((uint64_t *)addr + addr->sadb_address_len);
	addr->sadb_address_len = SADB_8TO64(addrsize);
	addr->sadb_address_proto = 0;
	addr->sadb_address_prefixlen = 0;
	addr->sadb_address_reserved = 0;
	addr->sadb_address_exttype = SADB_EXT_ADDRESS_DST;
	switch (fam) {
	case AF_INET:
		sin = (struct sockaddr_in *)(addr + 1);
		sin->sin_family = fam;
		bzero(sin->sin_zero, sizeof (sin->sin_zero));
		sin->sin_port = 0;
		bcopy(ipsa->ipsa_dstaddr, &sin->sin_addr, ipsa->ipsa_addrlen);
		break;
	}

	if (proxy) {
		addr = (sadb_address_t *)
		    ((uint64_t *)addr + addr->sadb_address_len);
		addr->sadb_address_len = SADB_8TO64(addrsize);
		addr->sadb_address_proto = 0;
		addr->sadb_address_prefixlen = 0;
		addr->sadb_address_reserved = 0;
		addr->sadb_address_exttype = SADB_EXT_ADDRESS_PROXY;
		switch (fam) {
		case AF_INET:
			sin = (struct sockaddr_in *)(addr + 1);
			sin->sin_family = fam;
			bzero(sin->sin_zero, sizeof (sin->sin_zero));
			sin->sin_port = 0;
			bcopy(ipsa->ipsa_proxyaddr, &sin->sin_addr,
			    ipsa->ipsa_addrlen);
			break;
		}
	}

	if (auth) {
		key = (sadb_key_t *)((uint64_t *)addr + addr->sadb_address_len);
		walker = (sadb_ext_t *)key;
		key->sadb_key_len = SADB_8TO64(authsize);
		key->sadb_key_exttype = SADB_EXT_KEY_AUTH;
		key->sadb_key_bits = ipsa->ipsa_authkeybits;
		key->sadb_key_reserved = 0;
		bcopy(ipsa->ipsa_authkey, key + 1, ipsa->ipsa_authkeylen);
	} else {
		walker = (sadb_ext_t *)addr;
	}

	if (encr) {
		key = (sadb_key_t *)((uint64_t *)walker + walker->sadb_ext_len);
		walker = (sadb_ext_t *)key;
		key->sadb_key_len = SADB_8TO64(encrsize);
		key->sadb_key_exttype = SADB_EXT_KEY_ENCRYPT;
		key->sadb_key_bits = ipsa->ipsa_encrkeybits;
		key->sadb_key_reserved = 0;
		bcopy(ipsa->ipsa_encrkey, key + 1, ipsa->ipsa_encrkeylen);
	}

	if (key == NULL)
		walker = (sadb_ext_t *)addr;
	else
		walker = (sadb_ext_t *)key;

	/* XXX Explicit iv? */

	if (srcid) {
		ident = (sadb_ident_t *)
		    ((uint64_t *)walker + walker->sadb_ext_len);
		walker = (sadb_ext_t *)ident;
		ident->sadb_ident_len = SADB_8TO64(srcidsize);
		ident->sadb_ident_exttype = SADB_EXT_IDENTITY_SRC;
		ident->sadb_ident_type = ipsa->ipsa_scid_type;
		ident->sadb_ident_id = 0;
		ident->sadb_ident_reserved = 0;
		(void) strcpy((char *)(ident + 1), ipsa->ipsa_src_cid);
	}

	if (dstid) {
		ident = (sadb_ident_t *)
		    ((uint64_t *)walker + walker->sadb_ext_len);
		walker = (sadb_ext_t *)ident;
		ident->sadb_ident_len = SADB_8TO64(dstidsize);
		ident->sadb_ident_exttype = SADB_EXT_IDENTITY_DST;
		ident->sadb_ident_type = ipsa->ipsa_dcid_type;
		ident->sadb_ident_id = 0;
		ident->sadb_ident_reserved = 0;
		(void) strcpy((char *)(ident + 1), ipsa->ipsa_dst_cid);
	}

#if 0 /* XXX PROXY not yet */
	if (proxyid) {
		ident = (sadb_ident_t *)
		    ((uint64_t *)walker + walker->sadb_ext_len);
		walker = (sadb_ext_t *)ident;
		ident->sadb_ident_len = SADB_8TO64(proxyidsize);
		ident->sadb_ident_exttype = SADB_EXT_IDENTITY_PROXY;
		ident->sadb_ident_type = ipsa->ipsa_pcid_type;
		ident->sadb_ident_id = 0;
		ident->sadb_ident_reserved = 0;
		(void) strcpy((char *)(ident + 1), ipsa->ipsa_proxy_cid);
	}
#endif /* XXX PROXY */

	if (sensinteg) {
		sens = (sadb_sens_t *)
		    ((uint64_t *)walker + walker->sadb_ext_len);
		sens->sadb_sens_len = SADB_8TO64(sizeof (sadb_sens_t *) +
		    ipsa->ipsa_senslen + ipsa->ipsa_integlen);
		sens->sadb_sens_dpd = ipsa->ipsa_dpd;
		sens->sadb_sens_sens_level = ipsa->ipsa_senslevel;
		sens->sadb_sens_integ_level = ipsa->ipsa_integlevel;
		sens->sadb_sens_sens_len = SADB_8TO64(ipsa->ipsa_senslen);
		sens->sadb_sens_integ_len = SADB_8TO64(ipsa->ipsa_integlen);
		sens->sadb_sens_reserved = 0;
		bitmap = (uint64_t *)(sens + 1);
		if (ipsa->ipsa_sens != NULL) {
			bcopy(ipsa->ipsa_sens, bitmap, ipsa->ipsa_senslen);
			bitmap += sens->sadb_sens_sens_len;
		}
		if (ipsa->ipsa_integ != NULL)
			bcopy(ipsa->ipsa_integ, bitmap, ipsa->ipsa_integlen);
	}

	/* Pardon any delays... */
	mutex_exit(&ipsa->ipsa_lock);

	return (mp);
}

/*
 * Strip out marked headers and adjust base message accordingly.  Headers are
 * marked with SADB_EXT_RESERVED (keying material usually gets marked).
 * Assume message is pulled up in one piece of contiguous memory.
 *
 * Say if we start off with:
 *
 * +------+----+-------------+-----------+-------------------+---------------+
 * | base | SA | source addr | dest addr | rsrvd. (auth key) | soft lifetime |
 * +------+----+-------------+-----------+-------------------+---------------+
 *
 * we will end up with
 *
 * +------+----+-------------+-----------+---------------+
 * | base | SA | source addr | dest addr | soft lifetime |
 * +------+----+-------------+-----------+---------------+
 */
static void
sadb_strip(sadb_msg_t *samsg)
{
	sadb_ext_t *ext;
	uint8_t *target = NULL;
	uint8_t *msgend;
	int sofar = SADB_8TO64(sizeof (*samsg));
	int copylen;

	ext = (sadb_ext_t *)(samsg + 1);
	msgend = (uint8_t *)samsg;
	msgend += SADB_64TO8(samsg->sadb_msg_len);
	while ((uint8_t *)ext < msgend) {
		if (ext->sadb_ext_type == SADB_EXT_RESERVED) {
			/*
			 * Aha!  I found a header to be erased.
			 */

			if (target != NULL) {
				/*
				 * If I had a previous header to be erased,
				 * copy over it.  I can get away with just
				 * copying backwards because the target will
				 * always be 8 bytes behind the source.
				 */
				copylen = ((uint8_t *)ext) - (target +
				    SADB_64TO8(
					((sadb_ext_t *)target)->sadb_ext_len));
				bcopy(((uint8_t *)ext - copylen), target,
				    copylen);
				target += copylen;
				((sadb_ext_t *)target)->sadb_ext_len =
				    SADB_8TO64(((uint8_t *)ext) - target +
					SADB_64TO8(ext->sadb_ext_len));
			} else {
				target = (uint8_t *)ext;
			}
		} else {
			sofar += ext->sadb_ext_len;
		}

		ext = (sadb_ext_t *)(((uint64_t *)ext) + ext->sadb_ext_len);
	}

	ASSERT((uint8_t *)ext == msgend);

	if (target != NULL) {
		copylen = ((uint8_t *)ext) - (target +
		    SADB_64TO8(((sadb_ext_t *)target)->sadb_ext_len));
		if (copylen != 0)
			bcopy(((uint8_t *)ext - copylen), target, copylen);
	}

	/* Adjust samsg. */
	samsg->sadb_msg_len = (uint16_t)sofar;

	/* Assume all of the rest is cleared by caller in sadb_pfkey_echo(). */
}

/*
 * AH needs to send an error to PF_KEY.  Assume mp points to an M_CTL
 * followed by an M_DATA with a PF_KEY message in it.  The serial of
 * the sending keysock instance is included.
 */
void
sadb_pfkey_error(queue_t *pfkey_q, mblk_t *mp, int errno, uint_t serial)
{
	mblk_t *msg = mp->b_cont;
	sadb_msg_t *samsg;
	keysock_out_t *kso;

	/*
	 * Enough functions call this to merit a NULL queue check.
	 */
	if (pfkey_q == NULL) {
		freemsg(mp);
		return;
	}

	ASSERT(msg != NULL);
	ASSERT((mp->b_wptr - mp->b_rptr) == sizeof (ipsec_info_t));
	ASSERT((msg->b_wptr - msg->b_rptr) >= sizeof (sadb_msg_t));
	samsg = (sadb_msg_t *)msg->b_rptr;
	kso = (keysock_out_t *)mp->b_rptr;

	kso->ks_out_type = KEYSOCK_OUT;
	kso->ks_out_len = sizeof (*kso);
	kso->ks_out_serial = serial;

	/*
	 * Only send the base message up in the event of an error.
	 */
	msg->b_wptr = msg->b_rptr + sizeof (*samsg);
	samsg = (sadb_msg_t *)msg->b_rptr;
	samsg->sadb_msg_len = SADB_8TO64(sizeof (*samsg));
	samsg->sadb_msg_errno = (uint8_t)errno;

	putnext(pfkey_q, mp);
}

/*
 * Send a successful return packet back to keysock via the queue in pfkey_q.
 *
 * Often, an SA is associated with the reply message, it's passed in if needed,
 * and NULL if not.  BTW, that ipsa will have its refcnt appropriately held,
 * and the caller will release said refcnt.
 */
void
sadb_pfkey_echo(queue_t *pfkey_q, mblk_t *mp, sadb_msg_t *samsg,
    keysock_in_t *ksi, ipsa_t *ipsa)
{
	keysock_out_t *kso;
	mblk_t *mp1;
	boolean_t strip = B_FALSE;

	ASSERT((mp->b_cont != NULL) &&
	    ((void *)samsg == (void *)mp->b_cont->b_rptr) &&
	    ((void *)mp->b_rptr == (void *)ksi));

	switch (samsg->sadb_msg_type) {
	case SADB_ADD:
	case SADB_UPDATE:
	case SADB_DELETE:
	case SADB_FLUSH:
	case SADB_DUMP:
		/*
		 * I have all of the message already.  I just need to strip
		 * out the keying material and echo the message back.
		 *
		 * NOTE: for SADB_DUMP, the function sadb_dump() did the
		 * work.  When DUMP reaches here, it should only be a base
		 * message.
		 */
		ASSERT(samsg->sadb_msg_type != SADB_DUMP ||
		    samsg->sadb_msg_len == SADB_8TO64(sizeof (sadb_msg_t)));

		if (ksi->ks_in_extv[SADB_EXT_KEY_AUTH] != NULL) {
			ksi->ks_in_extv[SADB_EXT_KEY_AUTH]->sadb_ext_type =
			    SADB_EXT_RESERVED;
			strip = B_TRUE;
		}
		if (ksi->ks_in_extv[SADB_EXT_KEY_ENCRYPT] != NULL) {
			ksi->ks_in_extv[SADB_EXT_KEY_ENCRYPT]->sadb_ext_type =
			    SADB_EXT_RESERVED;
			strip = B_TRUE;
		}
		if (strip) {
			uint8_t *oldend;

			sadb_strip(samsg);
			/* Assume PF_KEY message is contiguous. */
			ASSERT(mp->b_cont->b_cont == NULL);
			oldend = mp->b_cont->b_wptr;
			mp->b_cont->b_wptr = mp->b_cont->b_rptr +
			    SADB_64TO8(samsg->sadb_msg_len);
			bzero(mp->b_cont->b_wptr, oldend - mp->b_cont->b_wptr);
		}
		break;
	case SADB_GET:
		/*
		 * Do a lot of work here, because of the ipsa I just found.
		 * First abandon the PF_KEY message, then construct
		 * the new one.
		 */
		mp1 = sadb_sa2msg(ipsa, samsg);
		if (mp1 == NULL) {
			sadb_pfkey_error(pfkey_q, mp, ENOMEM,
			    ksi->ks_in_serial);
			return;
		}
		freemsg(mp->b_cont);
		mp->b_cont = mp1;
		break;
	default:
		if (mp != NULL)
			freemsg(mp);
		return;
	}

	/* ksi is now null and void. */
	kso = (keysock_out_t *)ksi;
	kso->ks_out_type = KEYSOCK_OUT;
	kso->ks_out_len = sizeof (*kso);
	kso->ks_out_serial = ksi->ks_in_serial;
	/* We're ready to send... */
	putnext(pfkey_q, mp);
}

/*
 * Set up a global pfkey_q instance for AH, ESP, or some other consumer.
 */
void
sadb_keysock_hello(queue_t **pfkey_qp, queue_t *q, mblk_t *mp,
    void (*ager)(void *), timeout_id_t *top, int satype)
{
	keysock_hello_ack_t *kha;
	queue_t *oldq;

	ASSERT(OTHERQ(q) != NULL);

	/*
	 * First, check atomically that I'm the first and only keysock
	 * instance.
	 *
	 * Use OTHERQ(q), because qreply(q, mp) == putnext(OTHERQ(q), mp),
	 * and I want this module to say putnext(*_pfkey_q, mp) for PF_KEY
	 * messages.
	 */

	oldq = casptr((void **)pfkey_qp, NULL, OTHERQ(q));
	if (oldq != NULL) {
		ASSERT(oldq != q);
		cmn_err(CE_WARN, "Danger!  Multiple keysocks on top of %s.\n",
		    (satype == SADB_SATYPE_ESP)? "ESP" : "AH or other");
		freemsg(mp);
		return;
	}

	kha = (keysock_hello_ack_t *)mp->b_rptr;
	kha->ks_hello_len = sizeof (keysock_hello_ack_t);
	kha->ks_hello_type = KEYSOCK_HELLO_ACK;
	kha->ks_hello_satype = (uint8_t)satype;

	/*
	 * If we made it past the casptr, then we have "exclusive" access
	 * to the timeout handle.  Fire it off in 4 seconds, because it
	 * just seems like a good interval.
	 */
	*top = qtimeout(*pfkey_qp, ager, NULL, drv_usectohz(4000000));

	putnext(*pfkey_qp, mp);
}

/*
 * Send IRE_DB_REQ down to IP to get properties of address.
 * If I can determine the address, return the proper type.  If an error
 * occurs, or if I have to send down an IRE_DB_REQ, return UNKNOWN, and
 * the caller will just let go of mp w/o freeing it.
 *
 * Whomever called the function will handle the return message that IP sends
 * in response to the message this function generates.
 */
int
sadb_addrcheck(queue_t *ip_q, queue_t *pfkey_q, mblk_t *mp, sadb_ext_t *ext,
    uint_t serial)
{
	sadb_address_t *addr = (sadb_address_t *)ext;
	struct sockaddr_in *sin;  /* XXX IPv6 : need _in6 eventually, too. */
	mblk_t *ire_db_req_mp;
	ire_t *ire;

	ASSERT(ext != NULL);
	ASSERT((ext->sadb_ext_type == SADB_EXT_ADDRESS_SRC) ||
	    (ext->sadb_ext_type == SADB_EXT_ADDRESS_DST) ||
	    (ext->sadb_ext_type == SADB_EXT_ADDRESS_PROXY));

	ire_db_req_mp = allocb(sizeof (ire_t), BPRI_HI);
	if (ire_db_req_mp == NULL) {
		/* cmn_err(CE_WARN, "sadb_addrcheck: allocb() failed.\n"); */
		sadb_pfkey_error(pfkey_q, mp, ENOMEM, serial);
		return (KS_IN_ADDR_UNKNOWN);
	}

	ire_db_req_mp->b_datap->db_type = IRE_DB_REQ_TYPE;
	ire_db_req_mp->b_wptr += sizeof (ire_t);
	ire = (ire_t *)ire_db_req_mp->b_rptr;
	/* XXX IPv6 : This is v4-specific code. */
	sin = (struct sockaddr_in *)(addr + 1);
	switch (sin->sin_family) {
	case AF_INET:
		ire->ire_ipversion = IPV4_VERSION;
		ire->ire_addr = sin->sin_addr.s_addr;
		if (ire->ire_addr == INADDR_ANY) {
			freemsg(ire_db_req_mp);
			return (KS_IN_ADDR_UNSPEC);
		}
		if (CLASSD(ire->ire_addr)) {
			freemsg(ire_db_req_mp);
			return (KS_IN_ADDR_MBCAST);
		}
		break;
	/* case AF_INET6: */
	default:
		freemsg(ire_db_req_mp);
		/* printf("Sockaddr family %d invalid.\n", sin->sin_family); */
		sadb_pfkey_error(pfkey_q, mp, EINVAL, serial);
		return (KS_IN_ADDR_UNKNOWN);
	}
	ire_db_req_mp->b_cont = mp;

	ASSERT(ip_q != NULL);
	putnext(ip_q, ire_db_req_mp);
	return (KS_IN_ADDR_UNKNOWN);
}

/*
 * This function is called from consumers that need to insert a fully-grown
 * security association into its tables.  This function takes into account that
 * SAs can be "inbound", "outbound", or "both".  The "primary" and "secondary"
 * hash bucket parameters are set in order of what the SA will be most of the
 * time.  (For example, an SA with an unspecified source, and a multicast
 * destination will primarily be an outbound SA.  OTOH, if that destination
 * is unicast for this node, then the SA will primarily be inbound.)
 *
 * It takes a lot of parameters because even if clone is B_FALSE, this needs
 * to check both buckets for purposes of collision.
 *
 * Return 0 upon success.  Return various errnos (ENOMEM, EEXIST, etc.) for
 * various error conditions.
 */
int
sadb_common_add(queue_t *pfkey_q, mblk_t *mp, sadb_msg_t *samsg,
    keysock_in_t *ksi, isaf_t *primary, isaf_t *secondary, isaf_t *larval,
    ipsa_t *newbie, boolean_t clone)
{
	ipsa_t *newbie_clone = NULL, *scratch;
	sadb_sa_t *assoc = (sadb_sa_t *)ksi->ks_in_extv[SADB_EXT_SA];
	sadb_address_t *srcext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_SRC];
	sadb_address_t *dstext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_DST];
	sadb_address_t *proxyext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_PROXY];
	sadb_key_t *akey = (sadb_key_t *)ksi->ks_in_extv[SADB_EXT_KEY_AUTH];
	sadb_key_t *ekey = (sadb_key_t *)ksi->ks_in_extv[SADB_EXT_KEY_ENCRYPT];
#if 0
	/* XXX MLS */
	sadb_sens_t *sens = (sadb_sens_t *);
#endif
	struct sockaddr_in *src, *dst, *proxy;	/* XXX IPv6 : _in6 too! */
	sadb_lifetime_t *soft =
	    (sadb_lifetime_t *)ksi->ks_in_extv[SADB_EXT_LIFETIME_SOFT];
	sadb_lifetime_t *hard =
	    (sadb_lifetime_t *)ksi->ks_in_extv[SADB_EXT_LIFETIME_HARD];
	int addrlen, error = 0;
	boolean_t isupdate = (newbie != NULL);

	/* XXX IPv6 : Make this IPv4/IPv6 friendly. */
	src = (struct sockaddr_in *)(srcext + 1);
	dst = (struct sockaddr_in *)(dstext + 1);
	if (proxyext != NULL)
		proxy = (struct sockaddr_in *)(proxyext + 1);
	else
		proxy = NULL;
	addrlen = IP_ADDR_LEN;
	if (!isupdate) {
		newbie = sadb_makelarvalassoc(assoc->sadb_sa_spi,
		    (uint8_t *)&src->sin_addr, (uint8_t *)&dst->sin_addr,
		    addrlen);
		if (newbie == NULL)
			return (ENOMEM);
	} /* Else this is an UPDATE, and I was passed in the new SA. */

	/* XXX IPv6 :  IPv4-specific code. */
	if (proxy != NULL)
		bcopy(&proxy->sin_addr, &newbie->ipsa_proxyaddr, addrlen);

	newbie->ipsa_type = samsg->sadb_msg_satype;
	newbie->ipsa_state = assoc->sadb_sa_state;
	ASSERT(newbie->ipsa_state == SADB_SASTATE_MATURE);
	newbie->ipsa_auth_alg = assoc->sadb_sa_auth;
	newbie->ipsa_encr_alg = assoc->sadb_sa_encrypt;
	newbie->ipsa_flags = assoc->sadb_sa_flags;
	/*
	 * If unspecified source address, force replay_wsize to 0.
	 * This is because an SA that has multiple sources of secure
	 * traffic cannot enforce a replay counter w/o synchronizing the
	 * senders.
	 */
	if (ksi->ks_in_srctype != KS_IN_ADDR_UNSPEC)
		newbie->ipsa_replay_wsize = assoc->sadb_sa_replay;
	else
		newbie->ipsa_replay_wsize = 0;

	(void) drv_getparm(TIME, &newbie->ipsa_addtime);
	/*
	 * XXX CURRENT lifetime checks MAY BE needed for an UPDATE.
	 * The spec says that one can update current lifetimes, but
	 * that seems impractical, especially in the larval-to-mature
	 * update that this function performs.
	 */
	if (soft != NULL) {
		newbie->ipsa_softaddlt = soft->sadb_lifetime_addtime;
		newbie->ipsa_softuselt = soft->sadb_lifetime_usetime;
		newbie->ipsa_softbyteslt = soft->sadb_lifetime_bytes;
		newbie->ipsa_softalloc = soft->sadb_lifetime_allocations;
		if (newbie->ipsa_softaddlt != 0)
			newbie->ipsa_softexpiretime = newbie->ipsa_addtime +
			    newbie->ipsa_softaddlt;
	}
	if (hard != NULL) {
		newbie->ipsa_hardaddlt = hard->sadb_lifetime_addtime;
		newbie->ipsa_harduselt = hard->sadb_lifetime_usetime;
		newbie->ipsa_hardbyteslt = hard->sadb_lifetime_bytes;
		newbie->ipsa_hardalloc = hard->sadb_lifetime_allocations;
		if (newbie->ipsa_hardaddlt != 0)
			newbie->ipsa_hardexpiretime = newbie->ipsa_addtime +
			    newbie->ipsa_hardaddlt;
	}

	if (akey != NULL) {
		newbie->ipsa_authkeybits = akey->sadb_key_bits;
		newbie->ipsa_authkeylen = SADB_1TO8(akey->sadb_key_bits);
		/* In case we have to round up to the next byte... */
		if ((akey->sadb_key_bits & 0x7) != 0)
			newbie->ipsa_authkeylen++;
		newbie->ipsa_authkey = kmem_alloc(newbie->ipsa_authkeylen,
		    KM_NOSLEEP);
		if (newbie->ipsa_authkey == NULL) {
			error = ENOMEM;
			goto error;
		}
		bcopy(akey + 1, newbie->ipsa_authkey, newbie->ipsa_authkeylen);
		bzero(akey + 1, newbie->ipsa_authkeylen);
	}

	if (ekey != NULL) {
		newbie->ipsa_encrkeybits = ekey->sadb_key_bits;
		newbie->ipsa_encrkeylen = SADB_1TO8(ekey->sadb_key_bits);
		/* In case we have to round up to the next byte... */
		if ((ekey->sadb_key_bits & 0x7) != 0)
			newbie->ipsa_encrkeylen++;
		newbie->ipsa_encrkey = kmem_alloc(newbie->ipsa_encrkeylen,
		    KM_NOSLEEP);
		if (newbie->ipsa_encrkey == NULL) {
			error = ENOMEM;
			goto error;
		}
		bcopy(ekey + 1, newbie->ipsa_encrkey, newbie->ipsa_encrkeylen);
		/* XXX is this safe w.r.t db_ref, etc? */
		bzero(ekey + 1, newbie->ipsa_encrkeylen);
	}

	/*
	 * Certificate ID stuff.
	 */
	if (ksi->ks_in_extv[SADB_EXT_IDENTITY_SRC] != NULL) {
		sadb_ident_t *id =
		    (sadb_ident_t *)ksi->ks_in_extv[SADB_EXT_IDENTITY_SRC];

		/*
		 * Can assume strlen() will return okay because ext_check() in
		 * keysock.c prepares the string for us.
		 */
		newbie->ipsa_src_cid = kmem_alloc(strlen((char *)(id + 1)) + 1,
		    KM_NOSLEEP);
		if (newbie->ipsa_src_cid == NULL) {
			error = ENOMEM;
			goto error;
		}
		(void) strcpy(newbie->ipsa_src_cid, (char *)(id + 1));
		newbie->ipsa_scid_type = id->sadb_ident_type;
	}

	if (ksi->ks_in_extv[SADB_EXT_IDENTITY_DST] != NULL) {
		sadb_ident_t *id =
		    (sadb_ident_t *)ksi->ks_in_extv[SADB_EXT_IDENTITY_DST];

		/*
		 * Can assume strlen() will return okay because ext_check() in
		 * keysock.c prepares the string for us.
		 */
		newbie->ipsa_dst_cid = kmem_alloc(strlen((char *)(id + 1)) + 1,
		    KM_NOSLEEP);
		if (newbie->ipsa_dst_cid == NULL) {
			error = ENOMEM;
			goto error;
		}
		(void) strcpy(newbie->ipsa_dst_cid, (char *)(id + 1));
		newbie->ipsa_dcid_type = id->sadb_ident_type;
	}

#if 0
	/* XXXMLS  SENSITIVITY handling code. */
	if (sens != NULL) {
		int i;
		uint64_t *bitmap = (uint64_t *)(sens + 1);

		newbie->ipsa_dpd = sens->sadb_sens_dpd;
		newbie->ipsa_senslevel = sens->sadb_sens_sens_level;
		newbie->ipsa_integlevel = sens->sadb_sens_integ_level;
		newbie->ipsa_senslen = SADB_64TO8(sens->sadb_sens_sens_len);
		newbie->ipsa_integlen = SADB_64TO8(sens->sadb_sens_integ_len);
		newbie->ipsa_integ = kmem_alloc(newbie->ipsa_integlen,
		    KM_NOSLEEP);
		if (newbie->ipsa_integ == NULL) {
			error = ENOMEM;
			goto error;
		}
		newbie->ipsa_sens = kmem_alloc(newbie->ipsa_senslen,
		    KM_NOSLEEP);
		if (newbie->ipsa_sens == NULL) {
			error = ENOMEM;
			goto error;
		}
		for (i = 0; i < sens->sadb_sens_sens_len; i++) {
			newbie->ipsa_sens[i] = *bitmap;
			bitmap++;
		}
		for (i = 0; i < sens->sadb_sens_integ_len; i++) {
			newbie->ipsa_integ[i] = *bitmap;
			bitmap++;
		}
	}

#endif

	if (clone) {
		newbie_clone = sadb_cloneassoc(newbie);

		if (newbie_clone == NULL) {
			error = ENOMEM;
			goto error;
		}
		newbie->ipsa_haspeer = B_TRUE;
		newbie_clone->ipsa_haspeer = B_TRUE;
	}

	/*
	 * Enter the bucket locks.  The order of entry is larval, outbound,
	 * inbound.  We map "primary" and "secondary" into outbound and inbound
	 * based on the destination address type.  If the destination address
	 * type is for a node that isn't mine (or potentially mine), the
	 * "primary" bucket is the outbound one.
	 */
	mutex_enter(&larval->isaf_lock);
	if (ksi->ks_in_dsttype == KS_IN_ADDR_NOTME) {
		/* primary == outbound */
		mutex_enter(&primary->isaf_lock);
		mutex_enter(&secondary->isaf_lock);
	} else {
		/* primary == inbound */
		mutex_enter(&secondary->isaf_lock);
		mutex_enter(&primary->isaf_lock);
	}

	/*
	 * Now that we're locked and loaded, we first have to check
	 * for duplicates.  If any exist, EEXIST should be returned.
	 */

	if (isupdate) {
		ASSERT(newbie->ipsa_linklock == NULL ||
		    newbie->ipsa_linklock == &larval->isaf_lock);

		/* If in larval list, unlink. */
		if (newbie->ipsa_linklock != NULL)
			sadb_unlinkassoc(newbie);
	} else {
		scratch = sadb_getassocbyspi(larval, newbie->ipsa_spi, zeroes,
		    newbie->ipsa_dstaddr, addrlen);
		if (scratch != NULL) {
			/* Collision in larval table. */
			IPSA_REFRELE(scratch);
			if (ksi->ks_in_dsttype == KS_IN_ADDR_NOTME) {
				/* primary == inbound */
				mutex_exit(&secondary->isaf_lock);
				mutex_exit(&primary->isaf_lock);
			} else {
				mutex_exit(&primary->isaf_lock);
				mutex_exit(&secondary->isaf_lock);
			}
			mutex_exit(&larval->isaf_lock);
			error = EEXIST;
			goto error;
		}
	}

	mutex_enter(&newbie->ipsa_lock);
	error = sadb_insertassoc(newbie, primary);
	mutex_exit(&newbie->ipsa_lock);
	/*
	 * We can exit the locks in any order.  Only entrance needs to
	 * follow any protocol.
	 */
	if (error != 0) {
		mutex_exit(&secondary->isaf_lock);
		mutex_exit(&primary->isaf_lock);
		mutex_exit(&larval->isaf_lock);
		goto error;
	}

	/*
	 * sadb_insertassoc() doesn't increment the reference count.
	 * We therefore have to increment the reference count one more
	 * time to reflect the pointers of the table that reference this
	 * SA.
	 */
	IPSA_REFHOLD(newbie);

	if (newbie_clone != NULL) {
		mutex_enter(&newbie_clone->ipsa_lock);
		error = sadb_insertassoc(newbie_clone, secondary);
		mutex_exit(&newbie_clone->ipsa_lock);
		if (error != 0) {
			/* Collision in secondary table. */
			sadb_unlinkassoc(newbie);  /* This does REFRELE. */
			/* We can exit the locks in any order. */
			mutex_exit(&secondary->isaf_lock);
			mutex_exit(&primary->isaf_lock);
			mutex_exit(&larval->isaf_lock);
			goto error;
		}
		IPSA_REFHOLD(newbie_clone);
	} else {
		ASSERT(primary != secondary);
		scratch = sadb_getassocbyspi(secondary, newbie->ipsa_spi,
		    zeroes, newbie->ipsa_dstaddr, addrlen);
		if (scratch != NULL) {
			/* Collision in secondary table. */
			sadb_unlinkassoc(newbie);  /* This does REFRELE. */
			/* We can exit the locks in any order. */
			mutex_exit(&secondary->isaf_lock);
			mutex_exit(&primary->isaf_lock);
			mutex_exit(&larval->isaf_lock);
			/* Set the error, since sadb_getassocbyspi() can't. */
			error = EEXIST;
			goto error;
		}
	}

	/* OKAY!  So let's do some reality check assertions. */

	ASSERT(!MUTEX_HELD(&newbie->ipsa_lock));
	ASSERT(newbie_clone == NULL || (!MUTEX_HELD(&newbie_clone->ipsa_lock)));

	/* We can exit the locks in any order. */
	mutex_exit(&secondary->isaf_lock);
	mutex_exit(&primary->isaf_lock);
	mutex_exit(&larval->isaf_lock);

	/* Common error point for this routine. */
error:
	if (newbie != NULL) {
		IPSA_REFRELE(newbie);
	}
	if (newbie_clone != NULL) {
		IPSA_REFRELE(newbie_clone);
	}

	if (error == 0) {
		/*
		 * Construct favorable PF_KEY return message and send to
		 * keysock.  (Q:  Do I need to pass "newbie"?  If I do,
		 * make sure to REFHOLD, call, then REFRELE.)
		 */
		sadb_pfkey_echo(pfkey_q, mp, samsg, ksi, NULL);
	}

	return (error);
}

/*
 * Set the time of first use for a security association.  Update any
 * expiration times as a result.
 */
void
sadb_set_usetime(ipsa_t *assoc)
{
	mutex_enter(&assoc->ipsa_lock);
	/*
	 * Caller does check usetime before calling me usually, and
	 * double-checking is better than a mutex_enter/exit hit.
	 */
	if (assoc->ipsa_usetime == 0) {
		/*
		 * This is redundant for outbound SA's, as sadb_getassocbyipc()
		 * sets the IPSA_F_USED flag already.  Inbound SAs, however,
		 * have no such protection.
		 */
		assoc->ipsa_flags |= IPSA_F_USED;

		(void) drv_getparm(TIME, &assoc->ipsa_usetime);

		/*
		 * After setting the use time, see if we have a use lifetime
		 * that would cause the actual SA expiration time to shorten.
		 */
		if (assoc->ipsa_softuselt != 0) {
			if (assoc->ipsa_softexpiretime == 0 ||
			    assoc->ipsa_usetime +
			    assoc->ipsa_softuselt <
			    assoc->ipsa_softexpiretime) {
				assoc->ipsa_softexpiretime =
				    assoc->ipsa_usetime +
				    assoc->ipsa_softuselt;
			}
		}
		if (assoc->ipsa_harduselt != 0) {
			if (assoc->ipsa_hardexpiretime == 0 ||
			    assoc->ipsa_usetime +
			    assoc->ipsa_harduselt <
			    assoc->ipsa_hardexpiretime) {
				assoc->ipsa_hardexpiretime =
				    assoc->ipsa_usetime +
				    assoc->ipsa_harduselt;
			}
		}
	}
	mutex_exit(&assoc->ipsa_lock);
}

/*
 * Send up a PF_KEY expire message for this association.
 */
static void
sadb_expire_assoc(queue_t *pfkey_q, ipsa_t *assoc)
{
	mblk_t *mp, *mp1;
	int alloclen, af;
	sadb_msg_t *samsg;
	sadb_lifetime_t *current, *expire;
	sadb_address_t *srcext, *dstext;
	sadb_sa_t *saext;
	keysock_out_t *kso;
	/* XXX IPv6 */
	struct sockaddr_in *src, *dst;

	ASSERT(MUTEX_HELD(&assoc->ipsa_lock));

	/* Don't bother sending if there's no queue. */
	if (pfkey_q == NULL)
		return;

	mp = allocb(sizeof (ipsec_info_t), BPRI_HI);
	if (mp == NULL) {
		/* cmn_err(CE_WARN, */
		/*	"sadb_expire_assoc: Can't allocate KEYSOCK_OUT.\n"); */
		return;
	}
	mp->b_datap->db_type = M_CTL;
	mp->b_wptr += sizeof (ipsec_info_t);
	kso = (keysock_out_t *)mp->b_rptr;
	kso->ks_out_type = KEYSOCK_OUT;
	kso->ks_out_len = sizeof (*kso);
	kso->ks_out_serial = 0;

	alloclen = sizeof (*samsg) + sizeof (*current) + sizeof (*expire) +
	    sizeof (*srcext) + sizeof (*dstext) + sizeof (*saext);

	switch (assoc->ipsa_addrlen) {
	case sizeof (ipaddr_t):
		af = AF_INET;
		alloclen += 2 * sizeof (struct sockaddr_in);
		break;

		/* XXX IPv6 */
	default:
		/* Won't happen unless there's a kernel bug. */
		freeb(mp);
		cmn_err(CE_WARN,
		    "sadb_expire_assoc: Unknown address length.\n");
		return;
	}

	mp->b_cont = allocb(alloclen, BPRI_HI);
	if (mp->b_cont == NULL) {
		freeb(mp);
		/* cmn_err(CE_WARN, */
		/*	"sadb_expire_assoc: Can't allocate message.\n"); */
		return;
	}

	mp1 = mp;
	mp = mp->b_cont;

	samsg = (sadb_msg_t *)mp->b_wptr;
	mp->b_wptr += sizeof (*samsg);
	samsg->sadb_msg_version = PF_KEY_V2;
	samsg->sadb_msg_type = SADB_EXPIRE;
	samsg->sadb_msg_errno = 0;
	samsg->sadb_msg_satype = assoc->ipsa_type;
	samsg->sadb_msg_len = SADB_8TO64(alloclen);
	samsg->sadb_msg_reserved = 0;
	samsg->sadb_msg_seq = 0;
	samsg->sadb_msg_pid = 0;

	saext = (sadb_sa_t *)mp->b_wptr;
	mp->b_wptr += sizeof (*saext);
	saext->sadb_sa_len = SADB_8TO64(sizeof (*saext));
	saext->sadb_sa_exttype = SADB_EXT_SA;
	saext->sadb_sa_spi = assoc->ipsa_spi;
	saext->sadb_sa_replay = assoc->ipsa_replay_wsize;
	saext->sadb_sa_state = assoc->ipsa_state;
	saext->sadb_sa_auth = assoc->ipsa_auth_alg;
	saext->sadb_sa_encrypt = assoc->ipsa_encr_alg;
	saext->sadb_sa_flags = assoc->ipsa_flags;

	current = (sadb_lifetime_t *)mp->b_wptr;
	mp->b_wptr += sizeof (sadb_lifetime_t);
	current->sadb_lifetime_len = SADB_8TO64(sizeof (*current));
	current->sadb_lifetime_exttype = SADB_EXT_LIFETIME_CURRENT;
	current->sadb_lifetime_allocations = assoc->ipsa_alloc;
	current->sadb_lifetime_bytes = assoc->ipsa_bytes;
	current->sadb_lifetime_addtime = assoc->ipsa_addtime;
	current->sadb_lifetime_usetime = assoc->ipsa_usetime;

	expire = (sadb_lifetime_t *)mp->b_wptr;
	mp->b_wptr += sizeof (*expire);
	expire->sadb_lifetime_len = SADB_8TO64(sizeof (*expire));

	if (assoc->ipsa_state == IPSA_STATE_DEAD) {
		expire->sadb_lifetime_exttype = SADB_EXT_LIFETIME_HARD;
		expire->sadb_lifetime_allocations = assoc->ipsa_hardalloc;
		expire->sadb_lifetime_bytes = assoc->ipsa_hardbyteslt;
		expire->sadb_lifetime_addtime = assoc->ipsa_hardaddlt;
		expire->sadb_lifetime_usetime = assoc->ipsa_harduselt;
	} else {
		ASSERT(assoc->ipsa_state == IPSA_STATE_DYING);
		expire->sadb_lifetime_exttype = SADB_EXT_LIFETIME_SOFT;
		expire->sadb_lifetime_allocations = assoc->ipsa_softalloc;
		expire->sadb_lifetime_bytes = assoc->ipsa_softbyteslt;
		expire->sadb_lifetime_addtime = assoc->ipsa_softaddlt;
		expire->sadb_lifetime_usetime = assoc->ipsa_softuselt;
	}

	srcext = (sadb_address_t *)mp->b_wptr;
	mp->b_wptr += sizeof (*srcext);
	srcext->sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
	srcext->sadb_address_proto = 0;
	srcext->sadb_address_prefixlen = 0;
	srcext->sadb_address_reserved = 0;
	switch (af) {
	case AF_INET:
		src = (struct sockaddr_in *)mp->b_wptr;
		mp->b_wptr += sizeof (*src);
		srcext->sadb_address_len =
		    SADB_8TO64(sizeof (*src) + sizeof (*srcext));
		src->sin_family = AF_INET;
		src->sin_port = 0;
		bzero(&src->sin_zero, sizeof (src->sin_zero));
		bcopy(assoc->ipsa_srcaddr, &src->sin_addr,
		    assoc->ipsa_addrlen);
		break;
	/* XXX IPv6. */
	/* No default case. */
	}

	dstext = (sadb_address_t *)mp->b_wptr;
	mp->b_wptr += sizeof (*dstext);
	dstext->sadb_address_exttype = SADB_EXT_ADDRESS_DST;
	dstext->sadb_address_proto = 0;
	dstext->sadb_address_prefixlen = 0;
	dstext->sadb_address_reserved = 0;
	switch (af) {
	case AF_INET:
		dst = (struct sockaddr_in *)mp->b_wptr;
		mp->b_wptr += sizeof (*dst);
		dstext->sadb_address_len =
		    SADB_8TO64(sizeof (*dst) + sizeof (*dstext));
		dst->sin_family = AF_INET;
		dst->sin_port = 0;
		bzero(&dst->sin_zero, sizeof (dst->sin_zero));
		bcopy(assoc->ipsa_dstaddr, &dst->sin_addr,
		    assoc->ipsa_addrlen);
		break;
	/* XXX IPv6. */
	/* No default case. */
	}

	/* Can just putnext, we're ready to go! */
	putnext(pfkey_q, mp1);
}

/*
 * "Age" the SA with the number of bytes that was used to protect traffic.
 * Send an SADB_EXPIRE message if appropriate.  Return B_TRUE if there was
 * enough "charge" left in the SA to protect the data.  Return B_FALSE
 * otherwise.  (If B_FALSE is returned, the association either was, or became
 * DEAD.)
 */
boolean_t
sadb_age_bytes(queue_t *pfkey_q, ipsa_t *assoc, uint64_t bytes,
    boolean_t sendmsg)
{
	boolean_t rc = B_TRUE;
	uint64_t newtotal;

	mutex_enter(&assoc->ipsa_lock);
	newtotal = assoc->ipsa_bytes + bytes;
	if (assoc->ipsa_hardbyteslt != 0 &&
	    newtotal >= assoc->ipsa_hardbyteslt) {
		if (assoc->ipsa_state < IPSA_STATE_DEAD) {
			/*
			 * Send EXPIRE message to PF_KEY.  May wish to pawn
			 * this off on another non-interrupt thread.  Also
			 * unlink this SA immediately.
			 */
			assoc->ipsa_state = IPSA_STATE_DEAD;
			if (sendmsg)
				sadb_expire_assoc(pfkey_q, assoc);
			/*
			 * Set non-zero expiration time so sadb_age_assoc()
			 * will work when reaping.
			 */
			assoc->ipsa_hardexpiretime = (time_t)1;
		} /* Else someone beat me to it! */
		rc = B_FALSE;
	} else if (assoc->ipsa_softbyteslt != 0 &&
	    (newtotal >= assoc->ipsa_softbyteslt)) {
		if (assoc->ipsa_state < IPSA_STATE_DYING) {
			/*
			 * Send EXPIRE message to PF_KEY.  May wish to pawn
			 * this off on another non-interrupt thread.
			 */
			assoc->ipsa_state = IPSA_STATE_DYING;
			if (sendmsg)
				sadb_expire_assoc(pfkey_q, assoc);
		} /* Else someone beat me to it! */
	}
	if (rc == B_TRUE)
		assoc->ipsa_bytes = newtotal;
	mutex_exit(&assoc->ipsa_lock);
	return (rc);
}

/*
 * See if a larval SA has reached its expiration time.
 */
void
sadb_age_larval(ipsa_t *assoc, time_t current)
{
	mutex_enter(&assoc->ipsa_lock);

	/* Much simpler aging.  May wish to send EXPIRE message, too. */
	if (assoc->ipsa_hardexpiretime <= current) {
		mutex_exit(&assoc->ipsa_lock);
		sadb_unlinkassoc(assoc);
		return;
	}
	mutex_exit(&assoc->ipsa_lock);
}

/*
 * Return "assoc" iff haspeer is true and I send an expire.  This allows
 * the consumers' aging functions to tidy up an expired SA's peer.
 */
ipsa_t *
sadb_age_assoc(queue_t *pfkey_q, ipsa_t *assoc, time_t current, int reap_delay)
{
	ipsa_t *retval = NULL;

	mutex_enter(&assoc->ipsa_lock);

	/*
	 * Check lifetimes.  Fortunately, SA setup is done
	 * such that there are only two times to look at,
	 * softexpiretime, and hardexpiretime.
	 *
	 * Check hard first.
	 */

	if (assoc->ipsa_hardexpiretime != 0 &&
	    assoc->ipsa_hardexpiretime <= current) {
		if (assoc->ipsa_state == IPSA_STATE_DEAD) {
			mutex_exit(&assoc->ipsa_lock);
			sadb_unlinkassoc(assoc);
			return (retval);
		}

		/*
		 * Send SADB_EXPIRE with hard lifetime, delay for unlinking.
		 */
		assoc->ipsa_state = IPSA_STATE_DEAD;
		if (assoc->ipsa_haspeer) {
			/*
			 * If I return assoc, I have to bump up its
			 * reference count to keep with the ipsa_t reference
			 * count semantics.
			 */
			assoc->ipsa_refcnt++;
			retval = assoc;
		}
		sadb_expire_assoc(pfkey_q, assoc);
		assoc->ipsa_hardexpiretime = current + reap_delay;
	} else if (assoc->ipsa_softexpiretime != 0 &&
	    assoc->ipsa_softexpiretime <= current &&
	    assoc->ipsa_state < IPSA_STATE_DYING) {
		/*
		 * Send EXPIRE message to PF_KEY.  May wish to pawn
		 * this off on another non-interrupt thread.
		 */
		assoc->ipsa_state = IPSA_STATE_DYING;
		if (assoc->ipsa_haspeer) {
			/*
			 * If I return assoc, I have to bump up its
			 * reference count to keep with the ipsa_t reference
			 * count semantics.
			 */
			assoc->ipsa_refcnt++;
			retval = assoc;
		}
		sadb_expire_assoc(pfkey_q, assoc);
	}

	mutex_exit(&assoc->ipsa_lock);
	return (retval);
}

/*
 * Update the lifetime values of an SA.  This is the path an SADB_UPDATE
 * message takes when updating a MATURE or DYING SA.
 */
void
sadb_update_assoc(ipsa_t *assoc, sadb_lifetime_t *hard, sadb_lifetime_t *soft)
{
	mutex_enter(&assoc->ipsa_lock);

	assoc->ipsa_state = IPSA_STATE_MATURE;

	/*
	 * XXX RFC 2367 mentions how an SADB_EXT_LIFETIME_CURRENT can be
	 * passed in during an update message.  We currently don't handle
	 * these.
	 */

	if (hard != NULL) {
		if (hard->sadb_lifetime_bytes != 0)
			assoc->ipsa_hardbyteslt = hard->sadb_lifetime_bytes;
		if (hard->sadb_lifetime_usetime != 0)
			assoc->ipsa_harduselt = hard->sadb_lifetime_usetime;
		if (hard->sadb_lifetime_addtime != 0)
			assoc->ipsa_hardaddlt = hard->sadb_lifetime_addtime;
		if (assoc->ipsa_hardaddlt != 0) {
			assoc->ipsa_hardexpiretime =
			    assoc->ipsa_addtime + assoc->ipsa_hardaddlt;
		}
		if (assoc->ipsa_harduselt != 0) {
			if (assoc->ipsa_hardexpiretime != 0) {
				assoc->ipsa_hardexpiretime =
				    min(assoc->ipsa_hardexpiretime,
					assoc->ipsa_usetime +
					assoc->ipsa_harduselt);
			} else {
				assoc->ipsa_hardexpiretime =
				    assoc->ipsa_usetime + assoc->ipsa_harduselt;
			}
		}

		if (hard->sadb_lifetime_allocations != 0)
			assoc->ipsa_hardalloc = hard->sadb_lifetime_allocations;
	}

	if (soft != NULL) {
		if (soft->sadb_lifetime_bytes != 0)
			assoc->ipsa_softbyteslt = soft->sadb_lifetime_bytes;
		if (soft->sadb_lifetime_usetime != 0)
			assoc->ipsa_softuselt = soft->sadb_lifetime_usetime;
		if (soft->sadb_lifetime_addtime != 0)
			assoc->ipsa_softaddlt = soft->sadb_lifetime_addtime;
		if (assoc->ipsa_softaddlt != 0) {
			assoc->ipsa_softexpiretime =
			    assoc->ipsa_addtime + assoc->ipsa_softaddlt;
		}
		if (assoc->ipsa_softuselt != 0) {
			if (assoc->ipsa_softexpiretime != 0) {
				assoc->ipsa_softexpiretime =
				    min(assoc->ipsa_softexpiretime,
					assoc->ipsa_usetime +
					assoc->ipsa_softuselt);
			} else {
				assoc->ipsa_softexpiretime =
				    assoc->ipsa_usetime + assoc->ipsa_softuselt;
			}
		}

		if (soft->sadb_lifetime_allocations != 0)
			assoc->ipsa_softalloc = soft->sadb_lifetime_allocations;
	}

	mutex_exit(&assoc->ipsa_lock);
}

/*
 * The following functions deal with ACQUIRE LISTS.  An ACQUIRE list is
 * a list of outstanding SADB_ACQUIRE messages.  If sadb_getassocbyipc() fails
 * for an outbound datagram, that datagram is queued up on an ACQUIRE record,
 * and an SADB_ACQUIRE message is sent up.  Presumably, a user-space key
 * management daemon will process the ACQUIRE, use a SADB_GETSPI to reserve
 * an SPI value and a larval SA, then SADB_UPDATE the larval SA, and ADD the
 * other direction's SA.
 */

/*
 * For this mblk, insert a new acquire record.  (XXX IPv4 specific for now.
 * Needs IPv6 code.)  Assume bucket contains addrs of all of the same length.
 * Return NULL if and exisiting record cannot be found, or if memory cannot be
 * allocated for a new one.
 */
ipsacq_t *
sadb_new_acquire(iacqf_t *bucket, uint32_t seq, mblk_t *mp, int satype,
    uint_t lifetime, int addrlen)
{
	ipsacq_t *newbie, *walker;
	mblk_t *iomp = mp, *datamp = mp->b_cont;
	ipsec_out_t *io = (ipsec_out_t *)iomp->b_rptr;
	/* XXX IPv4-specific, need IPv6. */
	ipha_t *ipha = (ipha_t *)datamp->b_rptr;
	uint8_t *src, *dst;
	uint64_t unique_id;

	/* Hmmm, this is IP-specific, no? */
	if ((ipha->ipha_version_and_hdr_length >> 4) == IP_VERSION) {
		src = (uint8_t *)&ipha->ipha_src;
		dst = (uint8_t *)&ipha->ipha_dst;
#if 0
	} else {
		/* XXX IPv6... */
#endif
	}

	mutex_enter(&bucket->iacqf_lock);

	/*
	 * Scan list for duplicates.  Check for UNIQUE, src/dest, algorithms.
	 *
	 * XXX May need search for duplicates based on other things too!
	 */
	for (walker = bucket->iacqf_ipsacq; walker != NULL;
	    walker = walker->ipsacq_next) {
		if (bcmp(dst, walker->ipsacq_dstaddr, walker->ipsacq_addrlen))
			continue;	/* Dst addr mismatch. */
		if (bcmp(src, walker->ipsacq_srcaddr, walker->ipsacq_addrlen))
			continue;	/* Src addr mismatch. */
		/* XXX Will check for proxy addr eventually. */

		/*
		 * Okay!  At this point, both source and dst match.
		 * Now we have to figure out uniqueness characteristics.
		 */
		if (satype == IPPROTO_ESP) {
			if (io->ipsec_out_esp_alg != walker->ipsacq_encr_alg)
				continue;
			if (io->ipsec_out_esp_ah_alg != walker->ipsacq_auth_alg)
				continue;
			if (io->ipsec_out_esp_req & IPSEC_PREF_UNIQUE) {
				unique_id = SA_FORM_UNIQUE_ID(io);
			} else {
				unique_id = 0;
			}
			if (unique_id == walker->ipsacq_unique_id) {
				break;
			}
		} else {
			/* Assume not ESP is AH... for now. */
			if (io->ipsec_out_ah_alg != walker->ipsacq_auth_alg)
				continue;
			if (io->ipsec_out_ah_req & IPSEC_PREF_UNIQUE) {
				unique_id = SA_FORM_UNIQUE_ID(io);
			} else {
				unique_id = 0;
			}
			if (unique_id == walker->ipsacq_unique_id) {
				break;
			}
		}
	}

	if (walker == NULL) {
		newbie = kmem_zalloc(sizeof (*newbie), KM_NOSLEEP);
		if (newbie == NULL)
			return (NULL);
		newbie->ipsacq_linklock = &bucket->iacqf_lock;
		newbie->ipsacq_next = bucket->iacqf_ipsacq;
		newbie->ipsacq_ptpn = &bucket->iacqf_ipsacq;
		if (newbie->ipsacq_next != NULL)
			newbie->ipsacq_next->ipsacq_ptpn = &newbie->ipsacq_next;
		bucket->iacqf_ipsacq = newbie;
		mutex_init(&newbie->ipsacq_lock, NULL, MUTEX_DEFAULT, NULL);
	} else {
		newbie = walker;
	}

	mutex_enter(&newbie->ipsacq_lock);
	mutex_exit(&bucket->iacqf_lock);

	/*
	 * This assert looks silly for now, but we may need to enter newbie's
	 * mutex during a search.
	 */
	ASSERT(MUTEX_HELD(&newbie->ipsacq_lock));

	mp->b_next = NULL;
	/* Queue up packet.  Use b_next. */
	if (newbie->ipsacq_numpackets == 0) {
		/* First one. */
		newbie->ipsacq_mp = mp;
		newbie->ipsacq_numpackets = 1;
		(void) drv_getparm(TIME, &newbie->ipsacq_expire);
		newbie->ipsacq_expire += lifetime;
		newbie->ipsacq_seq = seq;
		newbie->ipsacq_addrlen = addrlen;

		newbie->ipsacq_srcport = io->ipsec_out_src_port;
		newbie->ipsacq_dstport = io->ipsec_out_dst_port;
		newbie->ipsacq_proto = io->ipsec_out_proto;

		/* Assume (not ESP) is AH... for now. */
		if (satype == IPPROTO_ESP) {
			if (io->ipsec_out_esp_req & IPSEC_PREF_UNIQUE) {
				newbie->ipsacq_unique_id =
				    SA_FORM_UNIQUE_ID(io);
			} else {
				newbie->ipsacq_unique_id = 0;
			}
			newbie->ipsacq_encr_alg = io->ipsec_out_esp_alg;
			newbie->ipsacq_auth_alg = io->ipsec_out_esp_ah_alg;
		} else {
			if (io->ipsec_out_ah_req & IPSEC_PREF_UNIQUE) {
				newbie->ipsacq_unique_id =
				    SA_FORM_UNIQUE_ID(io);
			} else {
				newbie->ipsacq_unique_id = unique_id;
			}
			newbie->ipsacq_auth_alg = io->ipsec_out_ah_alg;
		}
	} else {
		/* Scan to the end of the list & insert. */
		mblk_t *lastone = newbie->ipsacq_mp;

		while (lastone->b_next != NULL)
			lastone = lastone->b_next;
		lastone->b_next = mp;
		if (newbie->ipsacq_numpackets++ == IPSACQ_MAXPACKETS) {
			/* XXX STATS : Log a packet dropped. */

			lastone = newbie->ipsacq_mp;
			newbie->ipsacq_mp = lastone->b_next;
			lastone->b_next = NULL;
			freemsg(lastone);
		}
	}

	/*
	 * Reset addresses.  Set them to the most recently added mblk chain,
	 * so that the address pointers in the acquire record will point
	 * at an mblk still attached to the acquire list.
	 */

	newbie->ipsacq_srcaddr = src;
	newbie->ipsacq_dstaddr = dst;

	/* Return with the newbie's mutex held. */

	return (newbie);
}

/*
 * Unlink and free an acquire record.
 */
void
sadb_destroy_acquire(ipsacq_t *acqrec)
{
	mblk_t *mp;

	ASSERT(MUTEX_HELD(acqrec->ipsacq_linklock));

	/* Unlink */
	*(acqrec->ipsacq_ptpn) = acqrec->ipsacq_next;
	if (acqrec->ipsacq_next != NULL)
		acqrec->ipsacq_next->ipsacq_ptpn = acqrec->ipsacq_ptpn;

	/*
	 * Free hanging mp's.
	 *
	 * XXX Instead of freemsg(), perhaps use IPSEC_REQ_FAILED.
	 */

	mutex_enter(&acqrec->ipsacq_lock);
	while (acqrec->ipsacq_mp != NULL) {
		mp = acqrec->ipsacq_mp;
		acqrec->ipsacq_mp = mp->b_next;
		mp->b_next = NULL;
		freemsg(mp);
	}
	mutex_exit(&acqrec->ipsacq_lock);

	/* Free */
	mutex_destroy(&acqrec->ipsacq_lock);
	kmem_free(acqrec, sizeof (*acqrec));
}

/*
 * Destroy an acquire list fanout.
 */
void
sadb_destroy_acqlist(iacqf_t *list, uint_t numentries, boolean_t forever)
{
	int i;

	for (i = 0; i < numentries; i++) {
		mutex_enter(&(list[i].iacqf_lock));
		while (list[i].iacqf_ipsacq != NULL)
			sadb_destroy_acquire(list[i].iacqf_ipsacq);
		mutex_exit(&(list[i].iacqf_lock));
		if (forever)
			mutex_destroy(&(list[i].iacqf_lock));
	}
}

/*
 * Generic setup of an ACQUIRE message.  Caller sets satype.
 */
void
sadb_setup_acquire(sadb_msg_t *samsg, ipsacq_t *acqrec)
{
	sadb_address_t *addr;
	struct sockaddr_in *sin;	/* XXX IPv6 is needed. */

	samsg->sadb_msg_version = PF_KEY_V2;
	samsg->sadb_msg_type = SADB_ACQUIRE;
	samsg->sadb_msg_errno = 0;
	samsg->sadb_msg_pid = 0;
	samsg->sadb_msg_reserved = 0;
	samsg->sadb_msg_len = SADB_8TO64(sizeof (*samsg));
	samsg->sadb_msg_seq = acqrec->ipsacq_seq;

	addr = (sadb_address_t *)(samsg + 1);
	addr->sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
	/* Quick zero-out of sadb_address_proto, sadb_address_prefix, etc. */
	*((uint32_t *)&addr->sadb_address_proto) = 0;
	addr->sadb_address_proto = acqrec->ipsacq_proto;
	addr->sadb_address_len = SADB_8TO64(sizeof (*addr));
	switch (acqrec->ipsacq_addrlen) {
		/*
		 * One word of warning, if any sockaddr type's size is not
		 * evenly divisible by 8, make sure you pad out accordingly
		 * before updating addr->sadb_address_len.
		 */
	case sizeof (ipaddr_t):
		sin = (struct sockaddr_in *)(addr + 1);
		sin->sin_family = AF_INET;
		sin->sin_port = acqrec->ipsacq_srcport;
		sin->sin_addr.s_addr = *(ipaddr_t *)acqrec->ipsacq_srcaddr;
		/* Quick zero-out of sin->sin_zero. */
		*((uint64_t *)&sin->sin_zero) = 0;
		addr->sadb_address_len += SADB_8TO64(sizeof (*sin));
		break;
	/* XXX IPv6. */
	default:
		/* This should never happen unless we have kernel bugs. */
		cmn_err(CE_WARN,
		    "sadb_setup_acquire:  corrupt ACQUIRE record.\n");
		break;
	}
	samsg->sadb_msg_len += addr->sadb_address_len;

	addr = (sadb_address_t *)(((uint64_t *)addr) + addr->sadb_address_len);
	addr->sadb_address_exttype = SADB_EXT_ADDRESS_DST;
	/* Quick zero-out of sadb_address_proto, sadb_address_prefix, etc. */
	*((uint32_t *)&addr->sadb_address_proto) = 0;
	addr->sadb_address_proto = acqrec->ipsacq_proto;
	addr->sadb_address_len = SADB_8TO64(sizeof (*addr));
	switch (acqrec->ipsacq_addrlen) {
		/*
		 * One word of warning, if any sockaddr type's size is not
		 * evenly divisible by 8, make sure you pad out accordingly
		 * before updating addr->sadb_address_len.
		 */
	case sizeof (ipaddr_t):
		sin = (struct sockaddr_in *)(addr + 1);
		sin->sin_family = AF_INET;
		sin->sin_port = acqrec->ipsacq_dstport;
		sin->sin_addr.s_addr = *(ipaddr_t *)acqrec->ipsacq_dstaddr;
		/* Quick zero-out of sin->sin_zero. */
		*((uint64_t *)&sin->sin_zero) = 0;
		addr->sadb_address_len += SADB_8TO64(sizeof (*sin));
		break;
	/* XXX IPv6. */
	default:
		/* This should never happen unless we have kernel bugs. */
		cmn_err(CE_WARN,
		    "sadb_setup_acquire:  corrupt ACQUIRE record.\n");
		break;
	}
	samsg->sadb_msg_len += addr->sadb_address_len;
}

/*
 * Given an SADB_GETSPI message, find an appropriately ranged SA and
 * allocate an SA.  If there are message improprieties, return (ipsa_t *)-1.
 * If there was a memory allocation error, return NULL.  (Assume NULL !=
 * (ipsa_t *)-1).
 *
 * master_spi is passed in host order.
 */
ipsa_t *
sadb_getspi(keysock_in_t *ksi, uint32_t master_spi)
{
	sadb_address_t *src =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_SRC],
	    *dst = (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_DST];
	sadb_spirange_t *range =
	    (sadb_spirange_t *)ksi->ks_in_extv[SADB_EXT_SPIRANGE];
	struct sockaddr_in *ssa, *dsa;
	uint8_t *srcaddr, *dstaddr;
	uint_t addrlen;
	uint32_t additive, min, max;

	if (src == NULL || dst == NULL || range == NULL)
		return ((ipsa_t *)-1);

	min = ntohl(range->sadb_spirange_min);
	max = ntohl(range->sadb_spirange_max);
	dsa = (struct sockaddr_in *)(dst + 1);
	ssa = (struct sockaddr_in *)(src + 1);

	if (dsa->sin_family != ssa->sin_family)
		return ((ipsa_t *)-1);

	switch (dsa->sin_family) {
	case AF_INET:
		srcaddr = (uint8_t *)(&ssa->sin_addr);
		dstaddr = (uint8_t *)(&dsa->sin_addr);
		addrlen = sizeof (ipaddr_t);
		break;
	default:	/* XXX IPv6 Need to fix this for IPv6. */
		return ((ipsa_t *)-1);
	}

	if (master_spi < min || master_spi > max) {
		/* Return a random value in the range. */
		sadb_get_random_bytes(&additive, sizeof (uint32_t));
		master_spi = min + (additive % (max - min + 1));
	}

	/*
	 * Since master_spi is passed in host order, we need to htonl() it
	 * for the purposes of creating a new SA.
	 */
	return (sadb_makelarvalassoc(htonl(master_spi), srcaddr, dstaddr,
	    addrlen));
}

/*
 * XXX NOTE:	The ip_q parameter will be used in the future for ACQUIRE
 *		failures.
 *
 * Locate an ACQUIRE and nuke it.  If I have an samsg that's larger than the
 * base header, just ignore it.  Otherwise, lock down the whole ACQUIRE list
 * and scan for the sequence number in question.  I may wish to accept an
 * address pair with it, for easier searching.
 *
 * Caller frees the message, so we don't have to here.
 */
/* ARGSUSED */
void
sadb_in_acquire(sadb_msg_t *samsg, iacqf_t *acqlist, iacqf_t *acqlist_v6,
    int outbound_buckets, queue_t *ip_q)
{
	int i;
	ipsacq_t *acqrec;
	iacqf_t *bucket;

	/*
	 * I only accept the base header for this!
	 * Though to be honest, requiring the dst address would help
	 * immensely.
	 *
	 * XXX  There are already cases where I can get the dst address.
	 */
	if (samsg->sadb_msg_len > SADB_8TO64(sizeof (*samsg)))
		return;

	/*
	 * Using the samsg->sadb_msg_seq, find the ACQUIRE record, delete it,
	 * (and in the future send a message to IP with the appropriate error
	 * number).
	 *
	 * Q: Do I want to reject if pid != 0?
	 */

	for (i = 0; i < outbound_buckets; i++) {
		bucket = &acqlist[i];
		mutex_enter(&bucket->iacqf_lock);
		for (acqrec = bucket->iacqf_ipsacq; acqrec != NULL;
		    acqrec = acqrec->ipsacq_next) {
			if (samsg->sadb_msg_seq == acqrec->ipsacq_seq)
				break;	/* for acqrec... loop. */
		}
		if (acqrec != NULL)
			break;	/* for i = 0... loop. */

		mutex_exit(&bucket->iacqf_lock);
		/* XXX IPv6 */
		/* And then check the corresponding v6 bucket. */
		/* buckets = acqlist_v6[i]; */
	}

	if (acqrec == NULL)
		return;

	/*
	 * What do I do with the errno and IP?  I may need mp's services a
	 * little more.  See sadb_destroy_acquire() for future directions
	 * beyond free the mblk chain on the acquire record.
	 */

	ASSERT(&bucket->iacqf_lock == acqrec->ipsacq_linklock);
	sadb_destroy_acquire(acqrec);
	/* Have to exit mutex here, because of breaking out of for loop. */
	mutex_exit(&bucket->iacqf_lock);
}

/*
 * The following function work with the replay windows of an SA.  They assume
 * the ipsa->ipsa_replay_arr is an array of uint64_t, and that the bit vector
 * represents the highest sequence number packet received, and back
 * (ipsa->ipsa_replay_wsize) packets.
 */

/*
 * Is the replay bit set?
 */
static boolean_t
ipsa_is_replay_set(ipsa_t *ipsa, uint32_t offset)
{
	uint64_t bit = (uint64_t)1 << (uint64_t)(offset & 63);

	return ((bit & ipsa->ipsa_replay_arr[offset >> 6]) ? B_TRUE : B_FALSE);
}

/*
 * Shift the bits of the replay window over.
 */
static void
ipsa_shift_replay(ipsa_t *ipsa, uint32_t shift)
{
	int i;
	int jump = ((shift - 1) >> 6) + 1;

	if (shift == 0)
		return;

	for (i = (ipsa->ipsa_replay_wsize - 1) >> 6; i >= 0; i--) {
		if (i + jump <= (ipsa->ipsa_replay_wsize - 1) >> 6) {
			ipsa->ipsa_replay_arr[i + jump] |=
			    ipsa->ipsa_replay_arr[i] >> (64 - (shift & 63));
		}
		ipsa->ipsa_replay_arr[i] <<= shift;
	}
}

/*
 * Set a bit in the bit vector.
 */
static void
ipsa_set_replay(ipsa_t *ipsa, uint32_t offset)
{
	uint64_t bit = (uint64_t)1 << (uint64_t)(offset & 63);

	ipsa->ipsa_replay_arr[offset >> 6] |= bit;
}

#define	SADB_MAX_REPLAY_VALUE 0xffffffff

/*
 * Assume caller has NOT done ntohl() already on seq.  Check to see
 * if replay sequence number "seq" has been seen already.
 */
boolean_t
sadb_replay_check(ipsa_t *ipsa, uint32_t seq)
{
	boolean_t rc;
	uint32_t diff;

	if (ipsa->ipsa_replay_wsize == 0)
		return (B_TRUE);

	/*
	 * NOTE:  I've already checked for 0 on the wire in sadb_replay_peek().
	 */

	/* Convert sequence number into host order before holding the mutex. */
	seq = ntohl(seq);

	mutex_enter(&ipsa->ipsa_lock);

	/* Initialize inbound SA's ipsa_replay field to last one received. */
	if (ipsa->ipsa_replay == 0)
		ipsa->ipsa_replay = 1;

	if (seq > ipsa->ipsa_replay) {
		/*
		 * I have received a new "highest value received".  Shift
		 * the replay window over.
		 */
		diff = seq - ipsa->ipsa_replay;
		if (diff < ipsa->ipsa_replay_wsize) {
			/* In replay window, shift bits over. */
			ipsa_shift_replay(ipsa, diff);
		} else {
			/* WAY FAR AHEAD, clear bits and start again. */
			bzero(ipsa->ipsa_replay_arr,
			    sizeof (ipsa->ipsa_replay_arr));
		}
		ipsa_set_replay(ipsa, 0);
		ipsa->ipsa_replay = seq;
		rc = B_TRUE;
		goto done;
	}
	diff = ipsa->ipsa_replay - seq;
	if (diff >= ipsa->ipsa_replay_wsize || ipsa_is_replay_set(ipsa, diff)) {
		rc = B_FALSE;
		goto done;
	}
	/* Set this packet as seen. */
	ipsa_set_replay(ipsa, diff);

	rc = B_TRUE;
done:
	mutex_exit(&ipsa->ipsa_lock);
	return (rc);
}

/*
 * "Peek" and see if we should even bother going through the effort of
 * running an authentication check on the sequence number passed in.
 * this takes into account packets that are below the replay window,
 * and collisions with already replayed packets.  Return B_TRUE if it
 * is okay to proceed, B_FALSE if this packet should be dropped immeidately.
 * Assume same byte-ordering as sadb_replay_check.
 */
boolean_t
sadb_replay_peek(ipsa_t *ipsa, uint32_t seq)
{
	boolean_t rc = B_FALSE;
	uint32_t diff;

	if (ipsa->ipsa_replay_wsize == 0)
		return (B_TRUE);

	/*
	 * 0 is 0, regardless of byte order... :)
	 *
	 * If I get 0 on the wire (and there is a replay window) then the
	 * sender most likely wrapped.  This ipsa may need to be marked or
	 * something.
	 */
	if (seq == 0)
		return (B_FALSE);

	seq = ntohl(seq);
	mutex_enter(&ipsa->ipsa_lock);
	if (seq < ipsa->ipsa_replay - ipsa->ipsa_replay_wsize &&
	    ipsa->ipsa_replay >= ipsa->ipsa_replay_wsize)
		goto done;

	/*
	 * If I've hit 0xffffffff, then quite honestly, I don't need to
	 * bother with formalities.  I'm not accepting any more packets
	 * on this SA.
	 */
	if (ipsa->ipsa_replay == SADB_MAX_REPLAY_VALUE) {
		/*
		 * Since we're already holding the lock, update the
		 * expire time ala. sadb_replay_delete() and return.
		 */
		ipsa->ipsa_hardexpiretime = (time_t)1;
		goto done;
	}

	if (seq <= ipsa->ipsa_replay) {
		/*
		 * This seq is in the replay window.  I'm not below it,
		 * because I already checked for that above!
		 */
		diff = ipsa->ipsa_replay - seq;
		if (ipsa_is_replay_set(ipsa, diff))
			goto done;
	}
	/* Else return B_TRUE, I'm going to advance the window. */

	rc = B_TRUE;
done:
	mutex_exit(&ipsa->ipsa_lock);
	return (rc);
}

/*
 * Delete a single SA.
 *
 * For now, use the quick-and-dirty trick of making the association's
 * hard-expire lifetime (time_t)1, ensuring deletion by the *_ager().
 */
void
sadb_replay_delete(ipsa_t *assoc)
{
	mutex_enter(&assoc->ipsa_lock);
	assoc->ipsa_hardexpiretime = (time_t)1;
	mutex_exit(&assoc->ipsa_lock);
}

/*
 * The following routine provide random bytes.  sadb_get_random_bytes()
 * should be extra-strong.
 */
void
sadb_get_random_bytes(void *act_dst, size_t len)
{
	extern int tcp_random(void);
	uint16_t result;
	uint8_t *dst = (uint8_t *)act_dst;

	/*
	 * XXX For now, return calls from tcp_random(), which yields 16 bits
	 * of data apiece.
	 *
	 * We will want to replace tcp_random() with something better.  For
	 * now, this is sufficient.
	 */
	while (len != 0) {
		result = (uint16_t)tcp_random();
		*dst = (uint8_t)result;
		dst++;
		if (len > 1)
			*dst = (uint8_t)(result >> 8);
		else
			break;	/* Out of while loop. */
		dst++;
		len -= 2;
	}
}

/*
 * Given a queue that presumably points to IP, send a T_BIND_REQ for _proto_
 * down.  The caller will handle the T_BIND_ACK locally.
 */
boolean_t
sadb_t_bind_req(queue_t *q, int proto)
{
	struct T_bind_req *tbr;
	mblk_t *mp;

	mp = allocb(sizeof (struct T_bind_req) + 1, BPRI_HI);
	if (mp == NULL) {
		/* cmn_err(CE_WARN, */
		/* "sadb_t_bind_req(%d): couldn't allocate mblk\n", proto); */
		return (B_FALSE);
	}
	mp->b_datap->db_type = M_PCPROTO;
	tbr = (struct T_bind_req *)mp->b_rptr;
	mp->b_wptr += sizeof (struct T_bind_req);
	tbr->PRIM_type = T_BIND_REQ;
	tbr->ADDR_length = 0;
	tbr->ADDR_offset = 0;
	tbr->CONIND_number = 0;
	*mp->b_wptr = (uint8_t)proto;
	mp->b_wptr++;

	putnext(q, mp);
	return (B_TRUE);
}

/*
 * Rate-limiting front-end to strlog() for AH and ESP.  Uses the ndd variables
 * in /dev/ip and the same rate-limiting clock so that there's a single
 * knob to turn to throttle the rate of messages.
 *
 * This function needs to be kept in synch with ipsec_log_policy_failure() in
 * ip.c.  Eventually, ipsec_log_policy_failure() should use this function.
 */
void
ipsec_rl_strlog(short mid, short sid, char level, ushort_t sl, char *fmt, ...)
{
	va_list adx;
	hrtime_t current = gethrtime();

	/* Convert interval (in msec) to hrtime (in nsec), which means * 10^6 */
	if (ipsec_policy_failure_last +
	    ((hrtime_t)ipsec_policy_log_interval * (hrtime_t)1000000) <=
	    current) {
		/*
		 * Throttle the logging such that I only log one
		 * message every 'ipsec_policy_log_interval' amount
		 * of time.
		 */
		va_start(adx, fmt);
		(void) vstrlog(mid, sid, level, sl, fmt, adx);
		va_end(adx);
		ipsec_policy_failure_last = current;
	}
}
