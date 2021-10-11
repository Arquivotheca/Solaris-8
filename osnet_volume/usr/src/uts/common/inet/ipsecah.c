/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ipsecah.c	1.7	99/10/21 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/errno.h>
#include <sys/strlog.h>
#include <sys/dlpi.h>
#include <sys/sockio.h>
#include <sys/tihdr.h>
#include <sys/socket.h>
#include <sys/ddi.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/debug.h>
#include <sys/atomic.h>
#include <netinet/in.h>
#include <net/pfkeyv2.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <netinet/ip6.h>
#include <inet/ip.h>
#include <inet/nd.h>
#include <inet/ip_ire.h>
#include <inet/ipsecah.h>
#include <inet/ipsec_info.h>
#include <inet/sadb.h>

#define	MAX_AALGS 256	/* Maximum number of authentication algorithms. */
#define	AH_AGE_INTERVAL_DEFAULT 1000

static kmutex_t ipsecah_param_lock;	/* Protect ipsecah_param_arr[] below. */
/*
 * Table of ND variables supported by ipsecah. These are loaded into
 * ipsecah_g_nd in ipsecah_init_nd.
 * All of these are alterable, within the min/max values given, at run time.
 */
static	ipsecahparam_t	ipsecah_param_arr[] = {
	/* min	max			value	name */
	{ 0,	3,			0,	"ipsecah_debug"},
	{ 125,	32000, AH_AGE_INTERVAL_DEFAULT,	"ipsecah_age_interval"},
	{ 1,	10,			1,	"ipsecah_reap_delay"},
	{ 0,	MAX_AALGS,	4,	"ipsecah_max_proposal_combinations"},
	{ 1,	SADB_MAX_REPLAY,	64,	"ipsecah_replay_size"},
	{ 1,	300,			15,	"ipsecah_acquire_timeout"},
	{ 1,	1800,			900,	"ipsecah_larval_timeout"},
	/* Default lifetime values for ACQUIRE messages. */
	{ 0,	0xffffffffU,		0,	"ipsecah_default_soft_bytes"},
	{ 0,	0xffffffffU,		0,	"ipsecah_default_hard_bytes"},
	{ 0,	0xffffffffU,		24000,	"ipsecah_default_soft_addtime"},
	{ 0,	0xffffffffU,		28800,	"ipsecah_default_hard_addtime"},
	{ 0,	0xffffffffU,		0,	"ipsecah_default_soft_usetime"},
	{ 0,	0xffffffffU,		0,	"ipsecah_default_hard_usetime"},
};
#define	ipsecah_debug		ipsecah_param_arr[0].ipsecah_param_value
#define	ipsecah_age_interval	ipsecah_param_arr[1].ipsecah_param_value
#define	ipsecah_age_int_max	ipsecah_param_arr[1].ipsecah_param_max
#define	ipsecah_reap_delay	ipsecah_param_arr[2].ipsecah_param_value
#define	ipsecah_max_combs	ipsecah_param_arr[3].ipsecah_param_value
#define	ipsecah_replay_size	ipsecah_param_arr[4].ipsecah_param_value
#define	ipsecah_acquire_timeout	ipsecah_param_arr[5].ipsecah_param_value
#define	ipsecah_larval_timeout	ipsecah_param_arr[6].ipsecah_param_value
#define	ipsecah_default_soft_bytes   ipsecah_param_arr[7].ipsecah_param_value
#define	ipsecah_default_hard_bytes   ipsecah_param_arr[8].ipsecah_param_value
#define	ipsecah_default_soft_addtime ipsecah_param_arr[9].ipsecah_param_value
#define	ipsecah_default_hard_addtime ipsecah_param_arr[10].ipsecah_param_value
#define	ipsecah_default_soft_usetime ipsecah_param_arr[11].ipsecah_param_value
#define	ipsecah_default_hard_usetime ipsecah_param_arr[12].ipsecah_param_value

#define	ah0dbg(a)	printf a
/* NOTE:  != 0 instead of > 0 so lint doesn't complain. */
#define	ah1dbg(a)	if (ipsecah_debug != 0) printf a
#define	ah2dbg(a)	if (ipsecah_debug > 1) printf a
#define	ah3dbg(a)	if (ipsecah_debug > 2) printf a

static IDP ipsecah_g_nd;

#define	IPV4_PADDING_ALIGN	0x04	/* Mutliple of 32 bits */

static void ah_icmp_error_v4(queue_t *, mblk_t *);
static void ah_outbound_v4(queue_t *, mblk_t *, ipsa_t *);
static void ah_auth_out_done_v4(queue_t *, mblk_t *);
static void ah_inbound_v4(mblk_t *);
static void ah_auth_in_done_v4(queue_t *, mblk_t *);
static mblk_t *ah_process_ip_options_v4(mblk_t *, ipsa_t *, auth_req_t *,
    boolean_t);
static void ah_getspi(mblk_t *, keysock_in_t *);
static void ah_flush(mblk_t *, keysock_in_t *);

static int ipsecah_open(queue_t *, dev_t *, int, int, cred_t *);
static int ipsecah_close(queue_t *);
static void ipsecah_rput(queue_t *, mblk_t *);
static void ipsecah_wput(queue_t *, mblk_t *);

static boolean_t ah_register_out(uint32_t, uint32_t, uint_t);

static struct module_info info = {
	5136, "ipsecah", 0, INFPSZ, 65536, 1024
};

static struct qinit rinit = {
	(pfi_t)ipsecah_rput, NULL, ipsecah_open, ipsecah_close, NULL, &info,
	NULL
};

static struct qinit winit = {
	(pfi_t)ipsecah_wput, NULL, ipsecah_open, ipsecah_close, NULL, &info,
	NULL
};

struct streamtab ipsecahinfo = {
	&rinit, &winit, NULL, NULL
};

/*
 * Keysock instance of AH.  "There can be only one." :)
 * Use casptr() on this because I don't set it until KEYSOCK_HELLO comes down.
 * Paired up with the ah_pfkey_q is the ah_event, which will age SAs.
 */
static queue_t *ah_pfkey_q;
static timeout_id_t ah_event;

/*
 * OTOH, this one is set at open/close, and I'm D_MTQPAIR for now.
 *
 * Question:	Do I need this, given that all instance's ahs->ahs_wq point to
 *		IP?
 *
 * Answer:	Yes, because I need to know which queue is BOUND to
 *		IPPROTO_AH
 */
static queue_t *ah_ip_q;
static mblk_t *ah_ip_unbind;

static kmutex_t ah_aalg_lock;	/* Protects ah_{aalgs,num_aalgs,sortlist} */
static ahstate_t *ah_aalgs[MAX_AALGS];
static uint_t ah_num_aalgs;	/* See ah_register_out() for why I'm here. */
static int ah_aalgs_sortlist[MAX_AALGS];

/*
 * Keep outbound assocs about the same as ire_cache entries for now.
 * One danger point, multiple SAs for a single dest will clog a bucket.
 * For the future, consider two-level hashing (2nd hash on IPC?), then probe.
 */
#define	OUTBOUND_BUCKETS 256
static isaf_t ah_outbound_assoc_v4[OUTBOUND_BUCKETS];
static isaf_t ah_outbound_assoc_v6[OUTBOUND_BUCKETS];

/* Outbound hash treats v4addr like a 32-bit quantity */
#define	OUTBOUND_HASH(v4addr) (((uint32_t)(v4addr) ^ \
	(((uint32_t)v4addr) >> 8) ^ (((uint32_t)v4addr) >> 16) ^ \
	(((uint32_t)v4addr) >> 24)) & 0xff)
/* Its v6 counterpart treats v6addr like something I can take the address of. */
#define	OUTBOUND_HASH_V6(v6addr) OUTBOUND_HASH((*(uint32_t *)&(v6addr)) ^ \
	(*((uint32_t *)&(v6addr)) + 1) ^ (*((uint32_t *)&(v6addr)) + 2) ^ \
	(*((uint32_t *)&(v6addr)) + 3))

/*
 * Inbound buckets are a bit easier w.r.t. hashing.
 */
#define	INBOUND_BUCKETS 256
static isaf_t ah_inbound_assoc_v4[INBOUND_BUCKETS];
static isaf_t ah_inbound_assoc_v6[INBOUND_BUCKETS];

/* May want to just take low 8 bits, assuming a halfway decent allocation. */
#define	INBOUND_HASH(spi) OUTBOUND_HASH(spi)

/* Hmmm, we may want to hash these out as well! */
static isaf_t ah_larval_list_v4;
static isaf_t ah_larval_list_v6;

/* Acquire list. */

static iacqf_t ah_acquires_v4[OUTBOUND_BUCKETS];
static iacqf_t ah_acquires_v6[OUTBOUND_BUCKETS];

/* Global SPI value and acquire sequence number. */

static uint32_t ah_spi;
static uint32_t ah_acquire_seq;

/*
 * Stats.  This may eventually become a full-blown SNMP MIB.  But for now,
 * keep things plain.  Use uint32_ts for identical precision across 32 and 64
 * bit models.
 */

typedef uint32_t ah_counter;

static ah_counter ah_good_auth;
static ah_counter ah_bad_auth;
static ah_counter ah_replay_failures;
static ah_counter ah_replay_early_failures;
static ah_counter ah_lookup_failure;
static ah_counter ah_keysock_in;
static ah_counter ah_in_requests;
static ah_counter ah_out_requests;
static ah_counter ah_acquire_requests;
static ah_counter ah_bytes_expired;
static ah_counter ah_in_discards;
static ah_counter ah_out_discards;

/*
 * For generic zero-address comparison.  This should be as large as the
 * ipsa_*addr field, and initialized to all-zeroes.  Globals are, so there
 * is no initialization required.
 */
static uint8_t zeroes[16];

/*
 * Don't have to lock ipsec_age_interval, as only one thread will access it at
 * a time, because I control the one function that does a qtimeout() on
 * ah_pfkey_q.
 */
/* ARGSUSED */
static void
ah_ager(void *ignoreme)
{
	hrtime_t begin = gethrtime();
	int i;
	isaf_t *bucket;
	ipsa_t *assoc, *spare;
	iacqf_t *acqlist;
	ipsacq_t *acqrec, *spareacq;
	struct templist {
		ipsa_t *ipsa;
		struct templist *next;
	} *haspeerlist = NULL, *newbie;
	time_t current;

	/*
	 * Do my dirty work.  This includes aging real entries, aging
	 * larvals, and aging outstanding ACQUIREs.
	 *
	 * I hope I don't tie up resources for too long.
	 */

	/* Snapshot current time now. */
	(void) drv_getparm(TIME, &current);

	/* Age acquires. */

	for (i = 0; i < OUTBOUND_BUCKETS; i++) {
		/* First IPv4. */
		acqlist = &(ah_acquires_v4[i]);
		mutex_enter(&acqlist->iacqf_lock);
		for (acqrec = acqlist->iacqf_ipsacq; acqrec != NULL;
		    acqrec = spareacq) {
			spareacq = acqrec->ipsacq_next;
			if (current > acqrec->ipsacq_expire)
				sadb_destroy_acquire(acqrec);
		}
		mutex_exit(&acqlist->iacqf_lock);

		/* Then IPv6.  XXX IPv6. */
	}

	/* Age larvals. */
	/* IPv4 */
	mutex_enter(&ah_larval_list_v4.isaf_lock);
	for (assoc = ah_larval_list_v4.isaf_ipsa; assoc != NULL;
	    assoc = spare) {
		/* Assign spare in case I get deleted. */
		spare = assoc->ipsa_next;
		sadb_age_larval(assoc, current);
	}
	mutex_exit(&ah_larval_list_v4.isaf_lock);
	/* XXX IPv6 */

	/* Age inbound associations. */
	for (i = 0; i < INBOUND_BUCKETS; i++) {
		/* Do IPv4... */
		bucket = &(ah_inbound_assoc_v4[i]);
		mutex_enter(&bucket->isaf_lock);
		for (assoc = bucket->isaf_ipsa; assoc != NULL;
		    assoc = spare) {
			/*
			 * Assign spare in case the current assoc gets deleted.
			 */
			spare = assoc->ipsa_next;
			if (sadb_age_assoc(ah_pfkey_q, assoc, current,
			    ipsecah_reap_delay) != NULL) {
				/*
				 * sadb_age_assoc() increments the refcnt,
				 * effectively doing an IPSA_REFHOLD().
				 */
				newbie = kmem_alloc(sizeof (*newbie),
				    KM_NOSLEEP);
				if (newbie == NULL) {
					/*
					 * Don't forget to REFRELE().
					 */
					IPSA_REFRELE(assoc);
					ah1dbg(("Can't allocate "
					    "inbound haspeer.\n"));
					continue;	/* for loop... */
				}
				newbie->next = haspeerlist;
				newbie->ipsa = assoc;
				haspeerlist = newbie;
			}
		}
		mutex_exit(&bucket->isaf_lock);

		/* ...then IPv6... XXX IPv6 */
	}

	/* Mark up those pesky haspeer cases.  (Don't forget IPv6.) */
	while (haspeerlist != NULL) {
		ah3dbg(("haspeerlist after inbound 0x%p.\n",
		    (void *)haspeerlist));
		/* "spare" contains the SA that has a peer. */
		spare = haspeerlist->ipsa;
		newbie = haspeerlist;
		haspeerlist = newbie->next;
		kmem_free(newbie, sizeof (*newbie));
		bucket = &(ah_outbound_assoc_v4[
		    OUTBOUND_HASH(*((ipaddr_t *)&spare->ipsa_dstaddr))]);
		mutex_enter(&bucket->isaf_lock);
		assoc = sadb_getassocbyspi(bucket, spare->ipsa_spi,
		    spare->ipsa_srcaddr, spare->ipsa_dstaddr,
		    spare->ipsa_addrlen);
		mutex_exit(&bucket->isaf_lock);
		if (assoc != NULL) {
			mutex_enter(&assoc->ipsa_lock);
			mutex_enter(&spare->ipsa_lock);
			assoc->ipsa_state = spare->ipsa_state;
			if (assoc->ipsa_state == IPSA_STATE_DEAD)
				assoc->ipsa_hardexpiretime = 1;
			mutex_exit(&spare->ipsa_lock);
			mutex_exit(&assoc->ipsa_lock);
			IPSA_REFRELE(assoc);
		}
		IPSA_REFRELE(spare);
	}

	/* Age outbound associations. */
	for (i = 0; i < OUTBOUND_BUCKETS; i++) {
		/* Do IPv4... */
		bucket = &(ah_outbound_assoc_v4[i]);
		mutex_enter(&bucket->isaf_lock);
		for (assoc = bucket->isaf_ipsa; assoc != NULL;
		    assoc = spare) {
			/*
			 * Assign spare in case the current assoc gets deleted.
			 */
			spare = assoc->ipsa_next;
			if (sadb_age_assoc(ah_pfkey_q, assoc, current,
			    ipsecah_reap_delay) != NULL) {
				/*
				 * sadb_age_assoc() increments the refcnt,
				 * effectively doing an IPSA_REFHOLD().
				 */
				newbie = kmem_alloc(sizeof (*newbie),
				    KM_NOSLEEP);
				if (newbie == NULL) {
					/*
					 * Don't forget to REFRELE().
					 */
					IPSA_REFRELE(assoc);
					ah1dbg(("Can't allocate "
					    "outbound haspeer.\n"));
					continue;	/* for loop... */
				}
				newbie->next = haspeerlist;
				newbie->ipsa = assoc;
				haspeerlist = newbie;
			}
		}
		mutex_exit(&bucket->isaf_lock);

		/* ...then IPv6... XXX IPv6 */
	}

	/* Mark up those pesky haspeer cases.  (Don't forget IPv6.) */
	while (haspeerlist != NULL) {
		ah3dbg(("haspeerlist after outbound 0x%p.\n",
		    (void *)haspeerlist));
		spare = haspeerlist->ipsa;
		newbie = haspeerlist;
		haspeerlist = newbie->next;
		kmem_free(newbie, sizeof (*newbie));
		bucket = &(ah_inbound_assoc_v4[INBOUND_HASH(spare->ipsa_spi)]);
		mutex_enter(&bucket->isaf_lock);
		assoc = sadb_getassocbyspi(bucket, spare->ipsa_spi,
		    spare->ipsa_srcaddr, spare->ipsa_dstaddr,
		    spare->ipsa_addrlen);
		mutex_exit(&bucket->isaf_lock);
		if (assoc != NULL) {
			mutex_enter(&assoc->ipsa_lock);
			mutex_enter(&spare->ipsa_lock);
			assoc->ipsa_state = spare->ipsa_state;
			if (assoc->ipsa_state == IPSA_STATE_DEAD)
				assoc->ipsa_hardexpiretime = 1;
			mutex_exit(&spare->ipsa_lock);
			mutex_exit(&assoc->ipsa_lock);
			IPSA_REFRELE(assoc);
		}
		IPSA_REFRELE(spare);
	}

	/*
	 * See how long this took.  If it took too long, increase the
	 * aging interval.
	 */
	if ((gethrtime() - begin) > ipsecah_age_interval * 1000000) {
		ah1dbg(("ah_ager() taking longer than %u msec, doubling.\n",
		    ipsecah_age_interval));
		if (ipsecah_age_interval == ipsecah_age_int_max) {
			/* XXX Rate limit this?  Or recommend flush? */
			(void) strlog(info.mi_idnum, 0, 0, SL_ERROR | SL_WARN,
			    "Too many AH associations to age out in %d msec.\n",
			    ipsecah_age_int_max);
		} else {
			/* Double by shifting by one bit. */
			ipsecah_age_interval <<= 1;
			ipsecah_age_interval = max(ipsecah_age_interval,
			    ipsecah_age_int_max);
		}
	} else if ((gethrtime() - begin) <= ipsecah_age_interval * 500000 &&
		ipsecah_age_interval > AH_AGE_INTERVAL_DEFAULT) {
		/*
		 * If I took less than half of the interval, then I should
		 * ratchet the interval back down.  Never automatically
		 * shift below the default aging interval.
		 *
		 * NOTE:This even overrides manual setting of the age
		 *	interval using NDD.
		 */
		/* Halve by shifting one bit. */
		ipsecah_age_interval >>= 1;
		ipsecah_age_interval = max(ipsecah_age_interval,
		    AH_AGE_INTERVAL_DEFAULT);
	}

	ah_event = qtimeout(ah_pfkey_q, ah_ager, NULL,
	    ipsecah_age_interval * drv_usectohz(1000));
}

/*
 * Get an AH NDD parameter.
 */
/* ARGSUSED */
static int
ipsecah_param_get(q, mp, cp)
	queue_t	*q;
	mblk_t	*mp;
	caddr_t	cp;
{
	ipsecahparam_t	*ipsecahpa = (ipsecahparam_t *)cp;

	mutex_enter(&ipsecah_param_lock);
	(void) mi_mpprintf(mp, "%d", ipsecahpa->ipsecah_param_value);
	mutex_exit(&ipsecah_param_lock);
	return (0);
}

/*
 * This routine sets an NDD variable in a ipsecahparam_t structure.
 */
/* ARGSUSED */
static int
ipsecah_param_set(q, mp, value, cp)
	queue_t	*q;
	mblk_t	*mp;
	char	*value;
	caddr_t	cp;
{
	char	*end;
	uint_t	new_value;
	ipsecahparam_t	*ipsecahpa = (ipsecahparam_t *)cp;

	/* Convert the value from a string into a long integer. */
	new_value = (uint_t)mi_strtol(value, &end, 10);
	/*
	 * Fail the request if the new value does not lie within the
	 * required bounds.
	 */
	if (end == value ||
	    new_value < ipsecahpa->ipsecah_param_min ||
	    new_value > ipsecahpa->ipsecah_param_max)
		return (EINVAL);

	/* Set the new value */
	mutex_enter(&ipsecah_param_lock);
	ipsecahpa->ipsecah_param_value = new_value;
	mutex_exit(&ipsecah_param_lock);
	return (0);
}

/*
 * Report AH status.  Until we have a MIB, it's the best way to go.
 */
/* ARGSUSED */
static int
ipsecah_status(queue_t *q, mblk_t *mp, void *arg)
{
	(void) mi_mpprintf(mp, "AH status");
	(void) mi_mpprintf(mp, "---------");
	(void) mi_mpprintf(mp,
	    "Authentication algorithms           =\t%u", ah_num_aalgs);
	(void) mi_mpprintf(mp,
	    "Packets passing authentication      =\t%u", ah_good_auth);
	(void) mi_mpprintf(mp,
	    "Packets failing authentication      =\t%u", ah_bad_auth);
	(void) mi_mpprintf(mp,
	    "Packets failing replay checks       =\t%u", ah_replay_failures);
	(void) mi_mpprintf(mp,
	    "Packets failing early replay checks =\t%u",
	    ah_replay_early_failures);
	(void) mi_mpprintf(mp,
	    "Failed inbound SA lookups           =\t%u", ah_lookup_failure);
	(void) mi_mpprintf(mp,
	    "Inbound PF_KEY messages             =\t%d", ah_keysock_in);
	(void) mi_mpprintf(mp,
	    "Inbound AH packets                  =\t%d", ah_in_requests);
	(void) mi_mpprintf(mp,
	    "Outbound AH requests                =\t%d", ah_out_requests);
	(void) mi_mpprintf(mp,
	    "PF_KEY ACQUIRE messages             =\t%d", ah_acquire_requests);
	(void) mi_mpprintf(mp,
	    "Expired associations (# of bytes)   =\t%d", ah_bytes_expired);
	(void) mi_mpprintf(mp,
	    "Discarded inbound packets           =\t%d", ah_in_discards);
	(void) mi_mpprintf(mp,
	    "Discarded outbound packets          =\t%d", ah_out_discards);

	return (0);
}

/*
 * Initialize things for AH at module load time.
 */
boolean_t
ipsecah_ddi_init(void)
{
	int count;
	ipsecahparam_t *ahp = ipsecah_param_arr;
	time_t current;

	for (count = A_CNT(ipsecah_param_arr); count-- > 0; ahp++) {
		if (ahp->ipsecah_param_name != NULL &&
		    ahp->ipsecah_param_name[0]) {
			if (!nd_load(&ipsecah_g_nd, ahp->ipsecah_param_name,
			    ipsecah_param_get, ipsecah_param_set,
			    (caddr_t)ahp)) {
				nd_free(&ipsecah_g_nd);
				return (B_FALSE);
			}
		}
	}

	if (!nd_load(&ipsecah_g_nd, "ipsecah_status", ipsecah_status, NULL,
	    NULL)) {
		nd_free(&ipsecah_g_nd);
		return (B_FALSE);
	}

	sadb_init(ah_outbound_assoc_v4, OUTBOUND_BUCKETS);
	sadb_init(ah_inbound_assoc_v4, INBOUND_BUCKETS);
	sadb_init(ah_outbound_assoc_v6, OUTBOUND_BUCKETS);
	sadb_init(ah_inbound_assoc_v6, INBOUND_BUCKETS);
	sadb_init(&ah_larval_list_v4, 1);
	sadb_init(&ah_larval_list_v6, 1);

	sadb_init((isaf_t *)ah_acquires_v4, OUTBOUND_BUCKETS);
	sadb_init((isaf_t *)ah_acquires_v6, OUTBOUND_BUCKETS);

	/*
	 * ah_spi will be another atomic_32 sort of variable.
	 *
	 * Initialize this as randomly as possible.
	 */
	sadb_get_random_bytes(&ah_spi, sizeof (uint32_t));
	(void) drv_getparm(TIME, &current);
	ah_spi ^= (uint32_t)current;
	ah_spi ^= (uint32_t)gethrtime();

	/* Set acquire sequence number to maximum possible. */
	ah_acquire_seq = (uint32_t)-1;

	mutex_init(&ah_aalg_lock, NULL, MUTEX_DEFAULT, 0);
	mutex_init(&ipsecah_param_lock, NULL, MUTEX_DEFAULT, 0);

	return (B_TRUE);
}

/*
 * Destroy things for AH at module unload time.
 */
void
ipsecah_ddi_destroy(void)
{
	ah1dbg(("In ddi_destroy.\n"));


	sadb_destroy(ah_outbound_assoc_v4, OUTBOUND_BUCKETS);
	sadb_destroy(ah_inbound_assoc_v4, INBOUND_BUCKETS);
	sadb_destroy(ah_outbound_assoc_v6, OUTBOUND_BUCKETS);
	sadb_destroy(ah_inbound_assoc_v6, INBOUND_BUCKETS);
	sadb_destroy(&ah_larval_list_v4, 1);
	sadb_destroy(&ah_larval_list_v6, 1);

	/* For each acquire, destroy it, including the bucket mutex. */
	sadb_destroy_acqlist(ah_acquires_v4, OUTBOUND_BUCKETS, B_TRUE);
	sadb_destroy_acqlist(ah_acquires_v6, OUTBOUND_BUCKETS, B_TRUE);

	mutex_destroy(&ipsecah_param_lock);
	mutex_destroy(&ah_aalg_lock);

	nd_free(&ipsecah_g_nd);
}

/*
 * AH module open routine.
 */
/* ARGSUSED */
static int
ipsecah_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	ahstate_t *ahs;

	if (q->q_ptr != NULL)
		return (0);  /* Re-open of an already open instance. */

	if (sflag != MODOPEN)
		return (EINVAL);

	/*
	 * Allocate AH state.  I dunno if keysock or an algorithm is
	 * on top of me, but it'll be one or the other.
	 */

	/* Note the lack of zalloc, with so few fields, it makes sense. */
	ahs = kmem_alloc(sizeof (ahstate_t), KM_NOSLEEP);
	if (ahs == NULL)
		return (ENOMEM);
	ahs->ahs_rq = q;
	ahs->ahs_wq = WR(q);
	ahs->ahs_id = 0;

	/*
	 * ASSUMPTIONS (because I'm MT_OCEXCL):
	 *
	 *	* I'm being pushed on top of IP for all my opens (incl. #1).
	 *	* Only ipsecah_open() can write into the variable ah_ip_q.
	 *	* Because of this, I can check lazily for ah_ip_q.
	 *
	 *  If these assumptions are wrong, I'm in BIG trouble...
	 */

	q->q_ptr = ahs;
	WR(q)->q_ptr = ahs;

	qprocson(q);

	if (ah_ip_q == NULL) {
		struct T_unbind_req *tur;

		ah_ip_q = WR(q);
		/* Allocate an unbind... */
		ah_ip_unbind = allocb(sizeof (struct T_unbind_req), BPRI_HI);

		/*
		 * Send down T_BIND_REQ to bind IPPROTO_AH.
		 * Handle the ACK here in AH.
		 */
		if (ah_ip_unbind == NULL ||
		    !sadb_t_bind_req(ah_ip_q, IPPROTO_AH)) {
			kmem_free(q->q_ptr, sizeof (ahstate_t));
			return (ENOMEM);
		}

		tur = (struct T_unbind_req *)ah_ip_unbind->b_rptr;
		tur->PRIM_type = T_UNBIND_REQ;
	}

	/*
	 * For now, there's not much I can do.  I'll be getting a message
	 * passed down to me from keysock (in my wput), and a T_BIND_ACK
	 * up from IP (in my rput).
	 */

	return (0);
}

/*
 * Sort algorithm lists.  I may wish to have an administrator
 * configure this list.  Hold on to some NDD variables...
 *
 * XXX For now, sort on minimum key size (GAG!).  While minimum key size is
 * not the ideal metric, it's the only quantifiable measure available in the
 * AUTH PI.  We need a better metric for sorting algorithms by preference.
 */
static void
ah_insert_sortlist(ahstate_t *ahs, ahstate_t **fanout, int *sortlist,
    uint_t count)
{
	int holder = ahs->ahs_id, swap;
	uint_t i;

	for (i = 0; i < count - 1; i++) {
		/*
		 * If you want to give precedence to newly added algs,
		 * add the = in the > comparison.
		 */
		if (holder != ahs->ahs_id ||
		    ahs->ahs_minbits > fanout[sortlist[i]]->ahs_minbits) {
				/* Swap sortlist[i] and holder. */
				swap = sortlist[i];
				sortlist[i] = holder;
				holder = swap;
		} /* Else just continue. */
	}

	/* Store holder in last slot. */
	sortlist[i] = holder;
}

/*
 * Remove an algorithm from a sorted algorithm list.
 * This should be considerably easier, even with complex sorting.
 */
static void
ah_remove_sortlist(int algid, int *sortlist, int newcount)
{
	int i;
	boolean_t copyback = B_FALSE;

	for (i = 0; i <= newcount; i++) {
		if (copyback)
			sortlist[i-1] = sortlist[i];
		else if (sortlist[i] == algid)
			copyback = B_TRUE;
	}
}

/*
 * AH module close routine.
 */
static int
ipsecah_close(queue_t *q)
{
	ahstate_t *ahs = (ahstate_t *)q->q_ptr;
	int i;

	/*
	 * Clean up q_ptr, if needed.
	 */
	qprocsoff(q);

	/* Keysock queue check is safe, because of OCEXCL perimeter. */
	if (q == ah_pfkey_q) {
		ah0dbg(("ipsecah_close:  Ummm... keysock is closing AH.\n"));
		ah_pfkey_q = NULL;
		/* Detach qtimeouts. */
		(void) quntimeout(q, ah_event);
	} else {
		/* Make sure about other stuff. */
		if (ahs->ahs_id != 0) {
			mutex_enter(&ah_aalg_lock);
			ah_num_aalgs--;
			ASSERT(ah_aalgs[ahs->ahs_id] == ahs);
			ah_aalgs[ahs->ahs_id] = NULL;
			ah_remove_sortlist(ahs->ahs_id, ah_aalgs_sortlist,
			    ah_num_aalgs);
			mutex_exit(&ah_aalg_lock);
			/*
			 * Any remaining SAs with this algorithm will
			 * fail gracefully when they are used.
			 */
			(void) ah_register_out(0, 0, 0);
		}
	}

	if (WR(q) == ah_ip_q) {
		/*
		 * If the ah_ip_q is attached to this instance, find
		 * another.  The OCEXCL outer perimeter helps us here.
		 */

		ah_ip_q = NULL;

		/*
		 * First, send a T_UNBIND_REQ to IP for this instance...
		 */
		if (ah_ip_unbind == NULL) {
			putnext(WR(q), ah_ip_unbind);
			/* putnext() will consume the mblk. */
			ah_ip_unbind = NULL;
		}

		/*
		 * ...then find a replacement queue for ah_ip_q.
		 */
		if (ah_pfkey_q != NULL && ah_pfkey_q != RD(q)) {
			/*
			 * See if we can use the pfkey_q.
			 */
			ah_ip_q = WR(ah_pfkey_q);
		} else {
			/*
			 * Use one of the algorithms.
			 */
			for (i = 0; i < MAX_AALGS; i++) {
				if (ah_aalgs[i] != NULL) {
					ah_ip_q = ah_aalgs[i]->ahs_wq;
					break;  /* Out of for loop. */
				}
			}
		}

		if (ah_ip_q == NULL ||
		    !sadb_t_bind_req(ah_ip_q, IPPROTO_AH)) {
			ah1dbg(("ipsecah: Can't reassign ah_ip_q.\n"));
			ah_ip_q = NULL;
		} else {
			ah_ip_unbind = allocb(sizeof (struct T_unbind_req),
			    BPRI_HI);
			/* If it's NULL, I can't do much here. */
		}
	}

	kmem_free(ahs, sizeof (ahstate_t));

	return (0);
}

/*
 * AH module read put routine.
 */
/* ARGSUSED */
static void
ipsecah_rput(queue_t *q, mblk_t *mp)
{
	keysock_in_t *ksi;
	int *addrtype;
	ire_t *ire;
	mblk_t *ire_mp, *last_mp;
	ipsec_in_t *ii = (ipsec_in_t *)mp->b_rptr;

	switch (mp->b_datap->db_type) {
	case M_CTL:
		/*
		 * IPsec request of some variety from IP.  IPSEC_{IN,OUT}
		 * are the common cases, but even ICMP error messages from IP
		 * may rise up here.
		 *
		 * Ummmm, actually, this can also be the reflected KEYSOCK_IN
		 * message, with an IRE_DB_TYPE hung off at the end.
		 */
		switch (((ipsec_info_t *)(mp->b_rptr))->ipsec_info_type) {
		case KEYSOCK_IN:
			last_mp = mp;
			while (last_mp->b_cont != NULL &&
			    last_mp->b_cont->b_datap->db_type != IRE_DB_TYPE)
				last_mp = last_mp->b_cont;

			if (last_mp->b_cont == NULL) {
				freemsg(mp);
				break;	/* Out of switch. */
			}

			ire_mp = last_mp->b_cont;
			last_mp->b_cont = NULL;

			ksi = (keysock_in_t *)mp->b_rptr;

			if (ksi->ks_in_srctype == KS_IN_ADDR_UNKNOWN)
				addrtype = &ksi->ks_in_srctype;
			else if (ksi->ks_in_dsttype == KS_IN_ADDR_UNKNOWN)
				addrtype = &ksi->ks_in_dsttype;
			else if (ksi->ks_in_proxytype == KS_IN_ADDR_UNKNOWN)
				addrtype = &ksi->ks_in_proxytype;

			ire = (ire_t *)ire_mp->b_rptr;

			/* XXX IPv6 :   This is v4-specific. */
			if (ire->ire_type == 0) {
				sadb_pfkey_error(ah_pfkey_q, mp, EADDRNOTAVAIL,
				    ksi->ks_in_serial);
				freemsg(ire_mp);
				return;
			} else if ((ire->ire_type & IRE_BROADCAST) ||
			    CLASSD(ire->ire_addr))
				*addrtype = KS_IN_ADDR_MBCAST;
			else if (ire->ire_type & (IRE_LOCAL|IRE_LOOPBACK))
				*addrtype = KS_IN_ADDR_ME;
			else
				*addrtype = KS_IN_ADDR_NOTME;

			freemsg(ire_mp);
			if (ah_pfkey_q != NULL) {
				/*
				 * Decrement counter to make up for
				 * auto-increment in ipsecesp_wput().
				 */
				ah_keysock_in--;
				ipsecah_wput(WR(ah_pfkey_q), mp);
			} else {
				freemsg(mp);
			}
			break;
		case IPSEC_IN:
			if (ii->ipsec_in_v4) {
				if (mp->b_cont->b_datap->db_type == M_CTL) {
					/* ICMP error */
					ah_icmp_error_v4(q, mp);
				} else {
					ah_inbound_v4(mp);
				}
			} else {
				ah2dbg(("Probably IPV6 packet %p\n",
				    (void *)mp));
				freemsg(mp);
			}
			break;
		case IPSEC_OUT:
			ah_outbound_v4(q, mp, NULL);
			break;
		default:
			freemsg(mp);
			break;
		}
		break;
	case M_PROTO:
	case M_PCPROTO:
		/* TPI message of some sort. */
		switch (*((t_scalar_t *)mp->b_rptr)) {
		case T_BIND_ACK:
			/* We expect this. */
			ah3dbg(("Thank you IP from AH for T_BIND_ACK\n"));
			break;
		case T_ERROR_ACK:
			cmn_err(CE_WARN,
			    "ipsecah:  AH received T_ERROR_ACK from IP.");
			break;
		case T_OK_ACK:
			/* Probably from a (rarely sent) T_UNBIND_REQ. */
			break;
		default:
			ah0dbg(("Unknown M_{,PC}PROTO message.\n"));
		}
		freemsg(mp);
		break;
	default:
		/* For now, eat message. */
		ah3dbg(("AH got unknown mblk type %d.\n",
		    mp->b_datap->db_type));
		freemsg(mp);
	}
}

/*
 * Construct an SADB_REGISTER message with the current algorithms.
 */
static boolean_t
ah_register_out(uint32_t sequence, uint32_t pid, uint_t serial)
{
	mblk_t *mp;
	boolean_t rc = B_TRUE;
	sadb_msg_t *samsg;
	sadb_supported_t *sasupp;
	sadb_alg_t *saalg;
	keysock_out_t *ksout;
	uint_t allocsize = sizeof (*samsg);
	uint_t i, numalgs_snap;

	/* Allocate the KEYSOCK_OUT. */
	mp = allocb(sizeof (ipsec_info_t), BPRI_HI);
	if (mp == NULL) {
		ah0dbg(("ah_register_out: couldn't allocate mblk.\n"));
		return (B_FALSE);
	}

	mp->b_datap->db_type = M_CTL;
	mp->b_wptr += sizeof (keysock_out_t);
	ksout = (keysock_out_t *)mp->b_rptr;
	ksout->ks_out_type = KEYSOCK_OUT;
	ksout->ks_out_len = sizeof (keysock_out_t);
	ksout->ks_out_serial = serial;

	/*
	 * Allocate the PF_KEY message that follows KEYSOCK_OUT.
	 *
	 * PROBLEM:	I need to hold the ah_aalg_lock to allocate the
	 *		variable part (i.e. the algorithms) because the
	 *		number is protected by the lock.
	 *
	 *		This may be a bit contentious... maybe.
	 */

	mutex_enter(&ah_aalg_lock);
	/*
	 * Fill SADB_REGISTER message's algorithm descriptors.  Hold
	 * down the lock while filling it.
	 */
	if (ah_num_aalgs != 0) {
		allocsize += (ah_num_aalgs * sizeof (*saalg));
		allocsize += sizeof (*sasupp);
	}
	mp->b_cont = allocb(allocsize, BPRI_HI);
	if (mp->b_cont == NULL) {
		mutex_exit(&ah_aalg_lock);
		freemsg(mp);
		return (B_FALSE);
	}

	mp->b_cont->b_wptr += allocsize;
	if (ah_num_aalgs != 0) {

		saalg = (sadb_alg_t *)(mp->b_cont->b_rptr + sizeof (*samsg) +
		    sizeof (*sasupp));
		ASSERT(((ulong_t)saalg & 0x7) == 0);

		numalgs_snap = 0;
		for (i = 0;
		    ((i < MAX_AALGS) && (numalgs_snap < ah_num_aalgs)); i++) {
			if (ah_aalgs[i] == NULL)
				continue;

			saalg->sadb_alg_id = ah_aalgs[i]->ahs_id;
			saalg->sadb_alg_ivlen = 0;
			saalg->sadb_alg_minbits = ah_aalgs[i]->ahs_minbits;
			saalg->sadb_alg_maxbits = ah_aalgs[i]->ahs_maxbits;
			saalg->sadb_alg_reserved = 0;
			numalgs_snap++;
			saalg++;
		}
		ASSERT(numalgs_snap == ah_num_aalgs);
#ifdef DEBUG
		/*
		 * Reality check to make sure I snagged all of the
		 * algorithms.
		 */
		while (i < MAX_AALGS)
			if (ah_aalgs[i++] != NULL)
				cmn_err(CE_PANIC,
				    "ah_register_out()!  Missed #%d.\n", i);
#endif /* DEBUG */
	}

	mutex_exit(&ah_aalg_lock);

	/* Now fill the restof the SADB_REGISTER message. */

	samsg = (sadb_msg_t *)mp->b_cont->b_rptr;
	samsg->sadb_msg_version = PF_KEY_V2;
	samsg->sadb_msg_type = SADB_REGISTER;
	samsg->sadb_msg_errno = 0;
	samsg->sadb_msg_satype = SADB_SATYPE_AH;
	samsg->sadb_msg_len = SADB_8TO64(allocsize);
	samsg->sadb_msg_reserved = 0;
	/*
	 * Assume caller has sufficient sequence/pid number info.  If it's one
	 * from me over a new alg., I could give two hoots about sequence.
	 */
	samsg->sadb_msg_seq = sequence;
	samsg->sadb_msg_pid = pid;

	if (allocsize > sizeof (*samsg)) {
		sasupp = (sadb_supported_t *)(samsg + 1);
		sasupp->sadb_supported_len =
		    SADB_8TO64(allocsize - sizeof (sadb_msg_t));
		sasupp->sadb_supported_exttype = SADB_EXT_SUPPORTED_AUTH;
		sasupp->sadb_supported_reserved = 0;
	}

	if (ah_pfkey_q != NULL)
		putnext(ah_pfkey_q, mp);
	else {
		rc = B_FALSE;
		freemsg(mp);
	}

	return (rc);
}

/*
 * Place a queue instance into the algorithm fanout.
 */
static void
ah_new_authalg(queue_t *q, mblk_t *mp)
{
	ahstate_t *ahs = (ahstate_t *)q->q_ptr;
	auth_pi_hello_t *aph = (auth_pi_hello_t *)mp->b_rptr;

	ah1dbg(("In ah_new_authalg, q = 0x%p\n", (void *)q));

	ASSERT(ahs != NULL);

	ahs->ahs_id = aph->auth_hello_id;
	ahs->ahs_ivlen = 0;
	ahs->ahs_minbits = aph->auth_hello_minbits;
	ahs->ahs_maxbits = aph->auth_hello_maxbits;
	ahs->ahs_datalen = aph->auth_hello_datalen;
	ahs->ahs_keycheck = aph->auth_hello_keycheck;

	/*
	 * If auth_hello_numkeys is needed, copy it here.
	 */

	mutex_enter(&ah_aalg_lock);
	if (ah_aalgs[ahs->ahs_id] != NULL) {
		/* Hmmm, duplicate algorithm ids. */
		(void) strlog(info.mi_idnum, 0, 0,
		    SL_ERROR | SL_CONSOLE | SL_WARN,
		    "Duplicate push of auth alg. %d on AH.\n", ahs->ahs_id);
		mutex_exit(&ah_aalg_lock);
		ahs->ahs_id = 0;
		return;
	} else {
		ah_aalgs[ahs->ahs_id] = ahs;
		ah_num_aalgs++;
	}

	/*
	 * Prioritize algorithms as they are inserted.  This will mean we have
	 * to keep holding the lock.
	 */
	ah_insert_sortlist(ahs, ah_aalgs, ah_aalgs_sortlist, ah_num_aalgs);

	mutex_exit(&ah_aalg_lock);

	/*
	 * Time to send a PF_KEY SADB_REGISTER message to AH listeners
	 * everywhere.  (The function itself checks for NULL ah_pfkey_q.)
	 */
	(void) ah_register_out(0, 0, 0);

	/* Send the AUTH_PI_ACK back. */
	aph->auth_hello_type = AUTH_PI_ACK;
	aph->auth_hello_len = sizeof (ipsec_info_t);
	qreply(q, mp);
}

/*
 * Now that weak-key passed, actually ADD the security association, and
 * send back a reply ADD message.
 */
static int
ah_add_sa_finish(mblk_t *mp, sadb_msg_t *samsg, keysock_in_t *ksi)
{
	isaf_t *primary, *secondary;
	sadb_sa_t *assoc = (sadb_sa_t *)ksi->ks_in_extv[SADB_EXT_SA];
	sadb_address_t *dstext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_DST];
	struct sockaddr_in *dst;	/* XXX IPv6, include _in6 too! */
	boolean_t clone = B_FALSE;
	uint8_t *dstaddr;
	ipsa_t *larval = NULL;
	ipsacq_t *acqrec;
	iacqf_t *acq_bucket;
	mblk_t *acq_msgs = NULL;
	int rc;

	/*
	 * Locate the appropriate table(s).
	 * XXX IPv6 : there's v4 specific stuff in here for now.
	 * The v4/v6 switch needs to be made here.
	 */

	dst = (struct sockaddr_in *)(dstext + 1);
	dstaddr = (uint8_t *)(&dst->sin_addr);

	switch (ksi->ks_in_dsttype) {
	case KS_IN_ADDR_ME:
	case KS_IN_ADDR_MBCAST:
		primary = &ah_inbound_assoc_v4[
		    INBOUND_HASH(assoc->sadb_sa_spi)];
		secondary = &ah_outbound_assoc_v4[
		    OUTBOUND_HASH(*(ipaddr_t *)dstaddr)];
		/*
		 * If the source address is either one of mine, or unspecified
		 * (which is best summed up by saying "not 'not mine'"),
		 * then the association is potentially bi-directional,
		 * in that it can be used for inbound traffic and outbound
		 * traffic.  The best example of such and SA is a multicast
		 * SA (which allows me to receive the outbound traffic).
		 */
		if (ksi->ks_in_srctype != KS_IN_ADDR_NOTME)
			clone = B_TRUE;
		break;
	case KS_IN_ADDR_NOTME:
		primary = &ah_outbound_assoc_v4[
		    OUTBOUND_HASH(*(ipaddr_t *)dstaddr)];
		secondary = &ah_inbound_assoc_v4[
		    INBOUND_HASH(assoc->sadb_sa_spi)];
		break;
	default:
		return (EINVAL);
	}

	/*
	 * Find a ACQUIRE list entry if possible.  If we've added an SA that
	 * suits the needs of an ACQUIRE list entry, we can eliminate the
	 * ACQUIRE list entry and transmit the enqueued packets.  Use the
	 * high-bit of the sequence number to queue it.  Key off destination
	 * addr, and change acqrec's state.  (XXX v4/v6 specific).
	 */

	if (samsg->sadb_msg_seq & IACQF_LOWEST_SEQ) {
		acq_bucket = &ah_acquires_v4[
		    OUTBOUND_HASH(*(ipaddr_t *)dstaddr)];
		mutex_enter(&acq_bucket->iacqf_lock);
		for (acqrec = acq_bucket->iacqf_ipsacq; acqrec != NULL;
		    acqrec = acqrec->ipsacq_next) {
			mutex_enter(&acqrec->ipsacq_lock);
			/*
			 * Q:  I only check sequence.  Should I check dst?
			 * A: Yes, check dest because those are the packets
			 *    that are queued up.
			 */
			if (acqrec->ipsacq_seq == samsg->sadb_msg_seq &&
			    bcmp(dstaddr, acqrec->ipsacq_dstaddr,
				acqrec->ipsacq_addrlen) == 0)
				break;
			mutex_exit(&acqrec->ipsacq_lock);
		}
		if (acqrec != NULL) {
			/*
			 * AHA!  I found an ACQUIRE record for this SA.
			 * Grab the msg list, and free the acquire record.
			 * I already am holding the lock for this record,
			 * so all I have to do is free it.
			 */
			acq_msgs = acqrec->ipsacq_mp;
			acqrec->ipsacq_mp = NULL;
			mutex_exit(&acqrec->ipsacq_lock);
			sadb_destroy_acquire(acqrec);
		}
		mutex_exit(&acq_bucket->iacqf_lock);
	}

	/*
	 * Find PF_KEY message, and see if I'm an update.  If so, find entry
	 * in larval list (if there).  XXX v4/v6 specific.
	 */

	if (samsg->sadb_msg_type == SADB_UPDATE) {
		boolean_t error = B_FALSE;

		mutex_enter(&ah_larval_list_v4.isaf_lock);
		larval = sadb_getassocbyspi(&ah_larval_list_v4,
		    assoc->sadb_sa_spi, zeroes, dstaddr, sizeof (ipaddr_t));
		if (larval == NULL) {
			ah0dbg(("Larval update, but larval disappeared.\n"));
			error = B_TRUE;
		} /* Else sadb_common_add unlinks it for me! */
		mutex_exit(&ah_larval_list_v4.isaf_lock);
		if (error)
			return (ESRCH);
	}

	/* XXX IPv6 : The larval list is currently v4/v6 specific. */
	rc = sadb_common_add(ah_pfkey_q, mp, samsg, ksi, primary, secondary,
	    &ah_larval_list_v4, larval, clone);

	/*
	 * How much more stack will I create with all of these
	 * ah_outbound_v4() calls?
	 */

	while (acq_msgs != NULL) {
		mblk_t *mp = acq_msgs;

		acq_msgs = acq_msgs->b_next;
		mp->b_next = NULL;
		/* XXX v4/v6 specific! */
		if (rc == 0) {
			ASSERT(ah_ip_q != NULL);
			ah_outbound_v4(OTHERQ(ah_ip_q), mp, NULL);
		} else {
			ah_out_discards++;
			freemsg(mp);
		}
	}

	return (rc);
}

/*
 * Add new AH security association.  This may become a generic AH/ESP
 * routine eventually.
 */
static int
ah_add_sa(mblk_t *mp, keysock_in_t *ksi)
{
	sadb_sa_t *assoc = (sadb_sa_t *)ksi->ks_in_extv[SADB_EXT_SA];
	sadb_address_t *srcext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_SRC];
	sadb_address_t *dstext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_DST];
	sadb_address_t *proxyext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_PROXY];
	sadb_key_t *key = (sadb_key_t *)ksi->ks_in_extv[SADB_EXT_KEY_AUTH];
	struct sockaddr_in *src, *dst, *proxy;	/* XXX IPv6 : _in6 too! */
	sadb_lifetime_t *soft =
	    (sadb_lifetime_t *)ksi->ks_in_extv[SADB_EXT_LIFETIME_SOFT];
	sadb_lifetime_t *hard =
	    (sadb_lifetime_t *)ksi->ks_in_extv[SADB_EXT_LIFETIME_HARD];
	mblk_t *keycheck_mp;
	ahstate_t *aalg;
	auth_keycheck_t *akc;

	/* I need certain extensions present for an ADD message. */
	if (srcext == NULL || dstext == NULL || assoc == NULL || key == NULL) {
		ah3dbg(("ah_add_sa: Missing basic extensions.\n"));
		return (EINVAL);
	}

	/* XXX IPv6 :  This should be less v4 specific. */
	src = (struct sockaddr_in *)(srcext + 1);
	dst = (struct sockaddr_in *)(dstext + 1);
	if (proxyext != NULL)
		proxy = (struct sockaddr_in *)(proxyext + 1);
	else
		proxy = NULL;

	/* Sundry ADD-specific reality checks. */
	/* XXX STATS : Logging/stats here? */
	if ((assoc->sadb_sa_state != SADB_SASTATE_MATURE) ||
	    (assoc->sadb_sa_encrypt != SADB_EALG_NONE) ||
	    (assoc->sadb_sa_flags & ~(SADB_SAFLAGS_NOREPLAY)) ||
	    !sadb_hardsoftchk(hard, soft) ||
	    (src->sin_family != dst->sin_family) ||
	    ((proxy != NULL) && (proxy->sin_family != dst->sin_family))) {
		ah3dbg(("First-line ADD-specific reality checks failed.\n"));
		return (EINVAL);
	}

	/* Stuff I don't support, for now. */
	if (ksi->ks_in_extv[SADB_EXT_LIFETIME_CURRENT] != NULL ||
	    ksi->ks_in_extv[SADB_EXT_SENSITIVITY] != NULL)
		return (EOPNOTSUPP);

	/*
	 * XXX Policy : I'm not checking identities or sensitivity
	 * labels at this time, but if I did, I'd do them here, before I sent
	 * the weak key check up to the algorithm.
	 */

	/*
	 * Locate the algorithm, then send up a weak key check message if
	 * needed.
	 *
	 * Q: Should I mutex_enter(&ah_aalg_lock)?
	 * A: Probably not for speed.  Besides, my open/close isn't shared.
	 */
	aalg = ah_aalgs[assoc->sadb_sa_auth];
	if (aalg == NULL) {
		ah1dbg(("Couldn't find auth alg #%d.\n", assoc->sadb_sa_auth));
		return (EINVAL);
	}

	if (aalg->ahs_keycheck) {
		keycheck_mp = allocb(sizeof (ipsec_info_t), BPRI_HI);
		if (keycheck_mp == NULL)
			return (ENOMEM);
		keycheck_mp->b_datap->db_type = M_CTL;
		keycheck_mp->b_wptr += sizeof (ipsec_info_t);
		akc = (auth_keycheck_t *)keycheck_mp->b_rptr;
		keycheck_mp->b_cont = mp;
		akc->auth_keycheck_type = AUTH_PI_KEY_CHECK;
		akc->auth_keycheck_len = sizeof (*akc);
		putnext(aalg->ahs_rq, keycheck_mp);
		/*
		 * Return message will be dispatched from wput() to
		 * ah_keycheck_ret()
		 */
		return (0);
	} else {
		ASSERT(mp->b_cont != NULL);
		/* Do some reality checking of lengths, etc. */
		if (key->sadb_key_bits < aalg->ahs_minbits ||
		    key->sadb_key_bits > aalg->ahs_maxbits)
			return (EINVAL);
		return (ah_add_sa_finish(mp, (sadb_msg_t *)mp->b_cont->b_rptr,
		    ksi));
	}
}

/*
 * Update a security association.  Updates come in two varieties.  The first
 * is an update of lifetimes on a non-larval SA.  The second is an update of
 * a larval SA, which ends up looking a lot more like an add.
 */
static int
ah_update_sa(mblk_t *mp, keysock_in_t *ksi)
{
	sadb_sa_t *assoc = (sadb_sa_t *)ksi->ks_in_extv[SADB_EXT_SA];
	sadb_address_t *srcext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_SRC];
	sadb_address_t *dstext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_DST];
	sadb_address_t *proxyext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_PROXY];
	sadb_key_t *akey = (sadb_key_t *)ksi->ks_in_extv[SADB_EXT_KEY_AUTH];
	sadb_key_t *ekey = (sadb_key_t *)ksi->ks_in_extv[SADB_EXT_KEY_ENCRYPT];
	/* XXX IPv6 :  The following should have _in6 too! */
	struct sockaddr_in *src, *dst, *proxy;
	sadb_lifetime_t *soft =
	    (sadb_lifetime_t *)ksi->ks_in_extv[SADB_EXT_LIFETIME_SOFT];
	sadb_lifetime_t *hard =
	    (sadb_lifetime_t *)ksi->ks_in_extv[SADB_EXT_LIFETIME_HARD];
	isaf_t *inbound, *outbound;
	ipsa_t *outbound_target = NULL, *inbound_target = NULL;
	int error = 0;

	/* I need certain extensions present for either UPDATE message. */
	if (srcext == NULL || dstext == NULL || assoc == NULL) {
		ah1dbg(("ah_update_sa: Missing basic extensions (1).\n"));
		return (EINVAL);
	}

	/* XXX IPv6 :  This should be less v4 specific. */
	src = (struct sockaddr_in *)(srcext + 1);
	dst = (struct sockaddr_in *)(dstext + 1);
	if (proxyext != NULL)
		proxy = (struct sockaddr_in *)(proxyext + 1);
	else
		proxy = NULL;

	/* Lock down all three buckets. */
	mutex_enter(&ah_larval_list_v4.isaf_lock);
	outbound = &ah_outbound_assoc_v4[
	    OUTBOUND_HASH(*(uint32_t *)(&dst->sin_addr))];
	mutex_enter(&outbound->isaf_lock);
	inbound = &ah_inbound_assoc_v4[INBOUND_HASH(assoc->sadb_sa_spi)];
	mutex_enter(&inbound->isaf_lock);

	/* Try outbound first. */
	outbound_target = sadb_getassocbyspi(outbound, assoc->sadb_sa_spi,
	    (uint8_t *)&src->sin_addr, (uint8_t *)&dst->sin_addr,
	    sizeof (dst->sin_addr));
	inbound_target = sadb_getassocbyspi(inbound, assoc->sadb_sa_spi,
	    (uint8_t *)&src->sin_addr, (uint8_t *)&dst->sin_addr,
	    sizeof (dst->sin_addr));

	mutex_exit(&inbound->isaf_lock);
	mutex_exit(&outbound->isaf_lock);

	if (outbound_target == NULL && inbound_target == NULL) {
		/* It's neither outbound nor inbound.  Check larval... */
		outbound_target = sadb_getassocbyspi(&ah_larval_list_v4,
		    assoc->sadb_sa_spi,
		    ((src == NULL) ? (zeroes) : ((uint8_t *)&src->sin_addr)),
		    (uint8_t *)&dst->sin_addr, sizeof (dst->sin_addr));
		mutex_exit(&ah_larval_list_v4.isaf_lock);

		if (outbound_target == NULL)
			return (ESRCH);
		/*
		 * Else REFRELE the target and let ah_add_sa() deal with
		 * updating a larval SA.
		 */
		IPSA_REFRELE(outbound_target);
		return (ah_add_sa(mp, ksi));
	}

	mutex_exit(&ah_larval_list_v4.isaf_lock);

	/* Reality checks for updates of active associations. */
	/* Sundry first-pass UPDATE-specific reality checks. */
	/* XXX STATS : logging/stats here? */
	if (assoc->sadb_sa_state != SADB_SASTATE_MATURE ||
	    (assoc->sadb_sa_flags & ~(SADB_SAFLAGS_NOREPLAY)) ||
	    (ksi->ks_in_extv[SADB_EXT_LIFETIME_CURRENT] != NULL) ||
	    !sadb_hardsoftchk(hard, soft) ||
	    (src->sin_family != dst->sin_family) ||
	    ((proxy != NULL) && (proxy->sin_family != dst->sin_family)) ||
	    akey != NULL || ekey != NULL) {
		error = EINVAL;
		goto bail;
	}

	if (outbound_target != NULL) {
		if (outbound_target->ipsa_state == IPSA_STATE_DEAD) {
			error = EINVAL;
			goto bail;
		}
		sadb_update_assoc(outbound_target, hard, soft);
	}


	if (inbound_target != NULL) {
		if (inbound_target->ipsa_state == IPSA_STATE_DEAD) {
			error = EINVAL;
			goto bail;
		}
		sadb_update_assoc(inbound_target, hard, soft);
	}

	sadb_pfkey_echo(ah_pfkey_q, mp, (sadb_msg_t *)mp->b_cont->b_rptr,
	    ksi, (outbound_target == NULL) ? inbound_target : outbound_target);

bail:
	/*
	 * Because of the multi-line macro nature of IPSA_REFRELE, keep
	 * them in { }.
	 */
	if (outbound_target != NULL) {
		IPSA_REFRELE(outbound_target);
	}
	if (inbound_target != NULL) {
		IPSA_REFRELE(inbound_target);
	}

	return (error);
}

/*
 * Delete a security association.  This is REALLY likely to be code common to
 * both AH and ESP.  Find the association, then unlink it.
 */
static int
ah_del_sa(mblk_t *mp, keysock_in_t *ksi)
{
	sadb_sa_t *assoc = (sadb_sa_t *)ksi->ks_in_extv[SADB_EXT_SA];
	sadb_address_t *srcext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_SRC];
	sadb_address_t *dstext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_DST];
	struct sockaddr_in *src, *dst;	/* XXX IPv6 : _in6 too! */
	ipsa_t *outbound_target, *inbound_target;
	isaf_t *inbound, *outbound;

	if (dstext == NULL || assoc == NULL)
		return (EINVAL);

	/* XXX IPv6 :  This should be less v4 specific. */
	if (srcext != NULL)
		src = (struct sockaddr_in *)(srcext + 1);
	else
		src = NULL;
	dst = (struct sockaddr_in *)(dstext + 1);

	/*
	 * I don't care too much about this, save that the SPI is there,
	 * and that I've a dest address.
	 *
	 * XXX IPv6 :  Be careful, you'll have to check for v4/v6.
	 */

	/* Lock down all three buckets. */
	mutex_enter(&ah_larval_list_v4.isaf_lock);
	outbound = &ah_outbound_assoc_v4[
	    OUTBOUND_HASH(*(uint32_t *)(&dst->sin_addr))];
	mutex_enter(&outbound->isaf_lock);
	inbound = &ah_inbound_assoc_v4[INBOUND_HASH(assoc->sadb_sa_spi)];
	mutex_enter(&inbound->isaf_lock);

	/* Try outbound first. */
	outbound_target = sadb_getassocbyspi(outbound, assoc->sadb_sa_spi,
	    ((src == NULL) ? (zeroes) : ((uint8_t *)&src->sin_addr)),
	    (uint8_t *)&dst->sin_addr, sizeof (dst->sin_addr));

	if (outbound_target == NULL || outbound_target->ipsa_haspeer) {
		inbound_target = sadb_getassocbyspi(inbound,
		    assoc->sadb_sa_spi,
		    ((src == NULL) ? (zeroes) : ((uint8_t *)&src->sin_addr)),
		    (uint8_t *)&dst->sin_addr, sizeof (dst->sin_addr));
	} else {
		inbound_target = NULL;
	}

	if (outbound_target == NULL && inbound_target == NULL) {
		/* It's neither outbound nor inbound.  Check larval... */
		outbound_target = sadb_getassocbyspi(&ah_larval_list_v4,
		    assoc->sadb_sa_spi,
		    ((src == NULL) ? (zeroes) : ((uint8_t *)&src->sin_addr)),
		    (uint8_t *)&dst->sin_addr, sizeof (dst->sin_addr));
		if (outbound_target == NULL) {
			mutex_exit(&inbound->isaf_lock);
			mutex_exit(&outbound->isaf_lock);
			mutex_exit(&ah_larval_list_v4.isaf_lock);
			return (ESRCH);
		}
	}

	/* At this point, I have one or two SAs to be deleted. */
	if (outbound_target != NULL)
		sadb_unlinkassoc(outbound_target);

	if (inbound_target != NULL)
		sadb_unlinkassoc(inbound_target);

	mutex_exit(&inbound->isaf_lock);
	mutex_exit(&outbound->isaf_lock);
	mutex_exit(&ah_larval_list_v4.isaf_lock);

	/*
	 * Because of the multi-line macro nature of IPSA_REFRELE, keep
	 * them in { }.
	 */
	if (outbound_target != NULL) {
		IPSA_REFRELE(outbound_target);
	}
	if (inbound_target != NULL) {
		IPSA_REFRELE(inbound_target);
	}

	ASSERT(mp->b_cont != NULL);
	sadb_pfkey_echo(ah_pfkey_q, mp, (sadb_msg_t *)mp->b_cont->b_rptr, ksi,
	    NULL);
	return (0);
}

/*
 * Find an AH security association and return it in a PF_KEY message.
 * Perhaps this should be common AH/ESP code, too.
 */
static int
ah_get_sa(mblk_t *mp, keysock_in_t *ksi)
{
	sadb_sa_t *assoc = (sadb_sa_t *)ksi->ks_in_extv[SADB_EXT_SA];
	sadb_address_t *srcext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_SRC];
	sadb_address_t *dstext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_DST];
	struct sockaddr_in *src, *dst;	/* XXX IPv6 : _in6 too! */
	ipsa_t *target;
	isaf_t *inbound, *outbound;

	if (dstext == NULL || assoc == NULL)
		return (EINVAL);

	/* XXX IPv6 :  This should be less v4 specific. */
	if (srcext != NULL)
		src = (struct sockaddr_in *)(srcext + 1);
	else
		src = NULL;
	dst = (struct sockaddr_in *)(dstext + 1);

	/*
	 * I don't care too much about this, save that the SPI is there,
	 * and that I've a dest address.
	 *
	 * XXX IPv6 :  Be careful, you'll have to check for v4/v6.
	 */

	/* Lock down all three buckets. */
	mutex_enter(&ah_larval_list_v4.isaf_lock);
	outbound = &ah_outbound_assoc_v4[
	    OUTBOUND_HASH(*(uint32_t *)(&dst->sin_addr))];
	mutex_enter(&outbound->isaf_lock);
	inbound = &ah_inbound_assoc_v4[INBOUND_HASH(assoc->sadb_sa_spi)];
	mutex_enter(&inbound->isaf_lock);

	/* Try outbound first. */
	target = sadb_getassocbyspi(outbound, assoc->sadb_sa_spi,
	    ((src == NULL) ? (zeroes) : ((uint8_t *)&src->sin_addr)),
	    (uint8_t *)&dst->sin_addr, sizeof (dst->sin_addr));
	if (target == NULL)
		target = sadb_getassocbyspi(inbound, assoc->sadb_sa_spi,
		    ((src == NULL) ? (zeroes) : ((uint8_t *)&src->sin_addr)),
		    (uint8_t *)&dst->sin_addr, sizeof (dst->sin_addr));
	if (target == NULL) {
		/* It's neither outbound nor inbound.  Check larval... */
		target = sadb_getassocbyspi(&ah_larval_list_v4,
		    assoc->sadb_sa_spi,
		    ((src == NULL) ? (zeroes) : ((uint8_t *)&src->sin_addr)),
		    (uint8_t *)&dst->sin_addr, sizeof (dst->sin_addr));
	}

	mutex_exit(&inbound->isaf_lock);
	mutex_exit(&outbound->isaf_lock);
	mutex_exit(&ah_larval_list_v4.isaf_lock);
	if (target == NULL)
		return (ESRCH);

	ASSERT(mp->b_cont != NULL);
	sadb_pfkey_echo(ah_pfkey_q, mp, (sadb_msg_t *)mp->b_cont->b_rptr, ksi,
	    target);
	IPSA_REFRELE(target);
	return (0);
}

/*
 * Flush out all of the AH security associations and send the appropriate
 * return message.
 */
static void
ah_flush(mblk_t *mp, keysock_in_t *ksi)
{
	/*
	 * Flush out each bucket, one at a time.  Were it not for keysock's
	 * enforcement, there would be a subtlety where I could add on the
	 * heels of a flush.  With keysock's enforcment, however, this makes
	 * AH's job easy.
	 */

	sadb_flush(&ah_larval_list_v4, 1);
	sadb_flush(&ah_larval_list_v6, 1);
	sadb_flush(ah_outbound_assoc_v4, OUTBOUND_BUCKETS);
	sadb_flush(ah_outbound_assoc_v6, OUTBOUND_BUCKETS);
	sadb_flush(ah_inbound_assoc_v4, INBOUND_BUCKETS);
	sadb_flush(ah_inbound_assoc_v6, INBOUND_BUCKETS);

	/*
	 * And while we're at it, destroy all outstanding ACQUIRE requests,
	 * too.  Just don't nuke the bucket mutexes (thus the B_FALSE for the
	 * third argument).
	 */
	sadb_destroy_acqlist(ah_acquires_v4, OUTBOUND_BUCKETS, B_FALSE);
	sadb_destroy_acqlist(ah_acquires_v6, OUTBOUND_BUCKETS, B_FALSE);

	ASSERT(mp->b_cont != NULL);
	sadb_pfkey_echo(ah_pfkey_q, mp, (sadb_msg_t *)mp->b_cont->b_rptr, ksi,
	    NULL);
}

/*
 * Convert the entire contents of all of AH's SA tables into PF_KEY SADB_DUMP
 * messages.
 */
static void
ah_dump(mblk_t *mp, keysock_in_t *ksi)
{
	int error;
	sadb_msg_t *samsg;

	/*
	 * Dump each fanout, bailing if errno is non-zero.
	 */

	error = sadb_dump(ah_pfkey_q, mp, ksi->ks_in_serial,
	    &ah_larval_list_v4, 1, B_TRUE);
	if (error != 0)
		goto bail;

	error = sadb_dump(ah_pfkey_q, mp, ksi->ks_in_serial,
	    &ah_larval_list_v6, 1, B_TRUE);
	if (error != 0)
		goto bail;

	error = sadb_dump(ah_pfkey_q, mp, ksi->ks_in_serial,
	    ah_outbound_assoc_v4, OUTBOUND_BUCKETS, B_TRUE);
	if (error != 0)
		goto bail;

	error = sadb_dump(ah_pfkey_q, mp, ksi->ks_in_serial,
	    ah_outbound_assoc_v6, OUTBOUND_BUCKETS, B_TRUE);
	if (error != 0)
		goto bail;

	error = sadb_dump(ah_pfkey_q, mp, ksi->ks_in_serial,
	    ah_inbound_assoc_v4, INBOUND_BUCKETS, B_FALSE);
	if (error != 0)
		goto bail;

	error = sadb_dump(ah_pfkey_q, mp, ksi->ks_in_serial,
	    ah_inbound_assoc_v6, INBOUND_BUCKETS, B_FALSE);

bail:
	ASSERT(mp->b_cont != NULL);
	samsg = (sadb_msg_t *)mp->b_cont->b_rptr;
	samsg->sadb_msg_errno = (uint8_t)error;
	sadb_pfkey_echo(ah_pfkey_q, mp, (sadb_msg_t *)mp->b_cont->b_rptr, ksi,
	    NULL);
}

/*
 * AH parsing of PF_KEY messages.  Keysock did most of the really silly
 * error cases.  What I receive is a fully-formed, syntactically legal
 * PF_KEY message.  I then need to check semantics...
 *
 * This code may become common to AH and ESP.  Stay tuned.
 *
 * I also make the assumption that db_ref's are cool.  If this assumption
 * is wrong, this means that someone other than keysock or me has been
 * mucking with PF_KEY messages.
 */
static void
ah_parse_pfkey(mblk_t *mp)
{
	mblk_t *msg = mp->b_cont;
	sadb_msg_t *samsg;
	keysock_in_t *ksi;
	int errno;

	ASSERT(msg != NULL);
	samsg = (sadb_msg_t *)msg->b_rptr;
	ksi = (keysock_in_t *)mp->b_rptr;

	switch (samsg->sadb_msg_type) {
	case SADB_ADD:
		errno = ah_add_sa(mp, ksi);
		if (errno != 0) {
			sadb_pfkey_error(ah_pfkey_q, mp, errno,
			    ksi->ks_in_serial);
		}
		/* else ah_add_sa() took care of things. */
		break;
	case SADB_DELETE:
		errno = ah_del_sa(mp, ksi);
		if (errno != 0) {
			sadb_pfkey_error(ah_pfkey_q, mp, errno,
			    ksi->ks_in_serial);
		}
		/* Else ah_del_sa() took care of things. */
		break;
	case SADB_GET:
		errno = ah_get_sa(mp, ksi);
		if (errno != 0) {
			sadb_pfkey_error(ah_pfkey_q, mp, errno,
			    ksi->ks_in_serial);
		}
		/* Else ah_get_sa() took care of things. */
		break;
	case SADB_FLUSH:
		ah_flush(mp, ksi);
		/* ah_flush will take care of the return message, etc. */
		break;
	case SADB_REGISTER:
		/*
		 * Hmmm, let's do it!  Check for extensions (there should
		 * be none), extract the fields, call ah_register_out(),
		 * then either free or report an error.
		 *
		 * Keysock takes care of the PF_KEY bookkeeping for this.
		 */
		if (ah_register_out(samsg->sadb_msg_seq, samsg->sadb_msg_pid,
		    ksi->ks_in_serial)) {
			freemsg(mp);
		} else {
			/*
			 * Only way this path hits is if there is a memory
			 * failure.  It will not return B_FALSE because of
			 * lack of ah_pfkey_q if I am in wput().
			 */
			sadb_pfkey_error(ah_pfkey_q, mp, ENOMEM,
			    ksi->ks_in_serial);
		}
		break;
	case SADB_UPDATE:
		/*
		 * Find a larval, if not there, find a full one and get
		 * strict.
		 */
		errno = ah_update_sa(mp, ksi);
		if (errno != 0) {
			sadb_pfkey_error(ah_pfkey_q, mp, errno,
			    ksi->ks_in_serial);
		}
		/* else ah_update_sa() took care of things. */
		break;
	case SADB_GETSPI:
		/*
		 * Reserve a new larval entry.
		 */
		ah_getspi(mp, ksi);
		break;
	case SADB_ACQUIRE:
		/*
		 * Find larval and/or ACQUIRE record and kill it (them), I'm
		 * most likely an error.  Inbound ACQUIRE messages should only
		 * have the base header.
		 */
		sadb_in_acquire(samsg, ah_acquires_v4, ah_acquires_v6,
		    OUTBOUND_BUCKETS, ah_pfkey_q);
		freemsg(mp);
		break;
	case SADB_DUMP:
		/*
		 * Dump all entries.
		 */
		ah_dump(mp, ksi);
		/* ah_dump will take care of the return message, etc. */
		break;
	case SADB_EXPIRE:
		/* Should never reach me. */
		sadb_pfkey_error(ah_pfkey_q, mp, EOPNOTSUPP,
		    ksi->ks_in_serial);
		break;
	default:
		sadb_pfkey_error(ah_pfkey_q, mp, EINVAL, ksi->ks_in_serial);
		break;
	}
}

/*
 * See if the authentication algorithm thought the key was weak or not.
 */
static void
ah_keycheck_ret(mblk_t *mp)
{
	auth_keycheck_t *akc;
	keysock_in_t *ksi;
	mblk_t *keycheck_mp;
	int errno;
	sadb_msg_t *samsg;

	akc = (auth_keycheck_t *)mp->b_rptr;
	errno = akc->auth_keycheck_errno;
	keycheck_mp = mp;
	mp = keycheck_mp->b_cont;
	ASSERT(mp != NULL);
	ksi = (keysock_in_t *)mp->b_rptr;
	ASSERT(mp->b_cont != NULL);
	samsg = (sadb_msg_t *)mp->b_cont->b_rptr;

	freeb(keycheck_mp);

	if (errno != 0) {
		sadb_pfkey_error(ah_pfkey_q, mp, errno, ksi->ks_in_serial);
		return;
	}

#if 0 /* Possible future. */
	if (akc->auth_keycheck_len > sizeof (*akc)) {
		/*
		 * There is a "fixed" key in here, for things like parity,
		 * etc.  This is probably not a likely outcome for AUTH
		 * algorithms, but for ENCR algorithms, its much more likely.
		 *
		 * If I don't have an SADB_EXT_KEY_AUTH pointer in ksi,
		 * this means I sliced this key.  Create one.
		 */
	}
#endif /* Possible future. */

	if (samsg->sadb_msg_type == SADB_ADD ||
	    samsg->sadb_msg_type == SADB_UPDATE)
		errno = ah_add_sa_finish(mp, samsg, ksi);

	if (errno != 0) {
		sadb_pfkey_error(ah_pfkey_q, mp, errno, ksi->ks_in_serial);
	}
	/* Else whatever got called handled it. */
}

/*
 * Handle case where PF_KEY says it can't find a keysock for one of my
 * ACQUIRE messages.
 */
static void
ah_keysock_no_socket(mblk_t *mp)
{
	sadb_msg_t *samsg;
	keysock_out_err_t *kse = (keysock_out_err_t *)mp->b_rptr;

	ASSERT(mp->b_cont != NULL);
	samsg = (sadb_msg_t *)mp->b_cont->b_rptr;

	/*
	 * If keysock can't find any registered, delete the acquire record
	 * immediately, and handle errors.
	 */
	if (samsg->sadb_msg_type == SADB_ACQUIRE) {
		samsg->sadb_msg_errno = kse->ks_err_errno;
		samsg->sadb_msg_len = SADB_8TO64(sizeof (*samsg));
		/*
		 * Use the write-side of the ah_pfkey_q, in case there is
		 * no ah_ip_q.
		 */
		sadb_in_acquire(samsg, ah_acquires_v4, ah_acquires_v6,
		    OUTBOUND_BUCKETS, WR(ah_pfkey_q));
	}

	freemsg(mp);
}

/*
 * AH module write put routine.
 */
static void
ipsecah_wput(queue_t *q, mblk_t *mp)
{
	ipsec_info_t *ii;
	keysock_in_t *ksi;
	int rc;
	struct iocblk *iocp;

	ah3dbg(("In ah_wput().\n"));

	/* NOTE:  Each case must take care of freeing or passing mp. */
	switch (mp->b_datap->db_type) {
	case M_CTL:
		ASSERT((mp->b_wptr - mp->b_rptr) >= sizeof (ipsec_info_t));
		ii = (ipsec_info_t *)mp->b_rptr;

		switch (ii->ipsec_info_type) {
		case AUTH_PI_HELLO:
			/* Assign this instance an algorithm type. */
			ah_new_authalg(q, mp);
			break;
		case AUTH_PI_OUT_AUTH_ACK:
			ah_auth_out_done_v4(q, mp);
			break;
		case AUTH_PI_IN_AUTH_ACK:
			ah_auth_in_done_v4(q, mp);
			break;
		case AUTH_PI_KEY_ACK:
			ah_keycheck_ret(mp);
			break;
		case KEYSOCK_OUT_ERR:
			ah1dbg(("Got KEYSOCK_OUT_ERR message.\n"));
			ah_keysock_no_socket(mp);
			break;
		case KEYSOCK_IN:
			/*
			 * XXX IPv6 :  What about IPv6 ire_t and IRE_DB_REQ
			 * in an IPv6 world?
			 */
			ah_keysock_in++;
			ah3dbg(("Got KEYSOCK_IN message.\n"));
			ksi = (keysock_in_t *)ii;
			/*
			 * Some common reality checks.
			 */

			if ((ksi->ks_in_extv[SADB_EXT_KEY_ENCRYPT] != NULL) ||
			    (ksi->ks_in_extv[SADB_EXT_PROPOSAL] != NULL) ||
			    (ksi->ks_in_extv[SADB_EXT_SUPPORTED_AUTH]
				!= NULL) ||
			    (ksi->ks_in_extv[SADB_EXT_SUPPORTED_ENCRYPT]
				!= NULL) ||
			    (ksi->ks_in_srctype == KS_IN_ADDR_MBCAST) ||
			    (ksi->ks_in_dsttype == KS_IN_ADDR_UNSPEC) ||
			    ((ksi->ks_in_srctype == KS_IN_ADDR_NOTME) &&
				(ksi->ks_in_dsttype == KS_IN_ADDR_NOTME))) {
				/*
				 * An encryption key in AH?!?
				 * An inbound PROPOSAL that reaches AH?
				 *	(Sure this is legal for keysock, but
				 *	 it shouldn't reach here.)
				 * Source address of multi/broad cast?
				 * Source AND dest addresses not me?
				 *	(XXX What about proxy?)
				 * Dest address of unspec?
				 */
				sadb_pfkey_error(ah_pfkey_q, mp, EINVAL,
				    ksi->ks_in_serial);
				return;
			}

			/*
			 * Use 'q' instead of ah_ip_q, since it's the write
			 * side already, and it'll go down to IP.  Use
			 * ah_pfkey_q because we wouldn't get here if
			 * that weren't set, and the RD(q) has been done
			 * already.
			 */
			if (ksi->ks_in_srctype == KS_IN_ADDR_UNKNOWN) {
				rc = sadb_addrcheck(q, ah_pfkey_q, mp,
				    ksi->ks_in_extv[SADB_EXT_ADDRESS_SRC],
				    ksi->ks_in_serial);
				if (rc == KS_IN_ADDR_UNKNOWN)
					return;
				else
					ksi->ks_in_srctype = rc;
			}
			if (ksi->ks_in_dsttype == KS_IN_ADDR_UNKNOWN) {
				rc = sadb_addrcheck(q, ah_pfkey_q, mp,
				    ksi->ks_in_extv[SADB_EXT_ADDRESS_DST],
				    ksi->ks_in_serial);
				if (rc == KS_IN_ADDR_UNKNOWN)
					return;
				else
					ksi->ks_in_dsttype = rc;
			}
			/*
			 * XXX Proxy may be a different address family.
			 */
			if (ksi->ks_in_proxytype == KS_IN_ADDR_UNKNOWN) {
				rc = sadb_addrcheck(q, ah_pfkey_q, mp,
				    ksi->ks_in_extv[SADB_EXT_ADDRESS_PROXY],
				    ksi->ks_in_serial);
				if (rc == KS_IN_ADDR_UNKNOWN)
					return;
				else
					ksi->ks_in_proxytype = rc;
			}
			ah_parse_pfkey(mp);
			break;
		case KEYSOCK_HELLO:
			sadb_keysock_hello(&ah_pfkey_q, q, mp, ah_ager,
			    &ah_event, SADB_SATYPE_AH);
			break;
		default:
			ah1dbg(("Got M_CTL from above of 0x%x.\n",
			    ii->ipsec_info_type));
			freemsg(mp);
			break;
		}
		break;
	case M_IOCTL:
		iocp = (struct iocblk *)mp->b_rptr;
		switch (iocp->ioc_cmd) {
		case ND_SET:
		case ND_GET:
			if (nd_getset(q, ipsecah_g_nd, mp)) {
				qreply(q, mp);
				return;
			} else {
				iocp->ioc_error = ENOENT;
			}
			/* FALLTHRU */
		default:
			/* We really don't support any other ioctls, do we? */

			/* Return EINVAL */
			if (iocp->ioc_error != ENOENT)
				iocp->ioc_error = EINVAL;
			iocp->ioc_count = 0;
			mp->b_datap->db_type = M_IOCACK;
			qreply(q, mp);
			return;
		}
	default:
		ah3dbg(("Got default message, type %d.\n",
		    mp->b_datap->db_type));
		freemsg(mp);  /* Or putnext() to IP? */
	}
}

/*
 * Updating use times can be tricky business if the ipsa_haspeer flag is
 * set.  This function is called once in an SA's lifetime.
 *
 * Caller has to REFRELE "assoc" which is passed in.  This function has
 * to REFRELE any peer SA that is obtained.
 */
static void
ah_set_usetime(ipsa_t *assoc, boolean_t inbound)
{
	ipsa_t *inassoc, *outassoc;
	isaf_t *bucket;

	/* No peer?  No problem! */
	if (!assoc->ipsa_haspeer) {
		sadb_set_usetime(assoc);
		return;
	}

	/*
	 * Otherwise, we want to grab both the original assoc and its peer.
	 * There might be a race for this, but if it's a real race, the times
	 * will be out-of-synch by at most a second, and since our time
	 * granularity is a second, this won't be a problem.
	 *
	 * If we need tight synchronization on the peer SA, then we need to
	 * reconsider.
	 */

	if (inbound) {
		inassoc = assoc;
		/* XXX IPv6 : we need IPv6 code here. */
		bucket = &ah_outbound_assoc_v4[
		    OUTBOUND_HASH(*((ipaddr_t *)&inassoc->ipsa_dstaddr))];
		mutex_enter(&bucket->isaf_lock);
		outassoc = sadb_getassocbyspi(bucket, inassoc->ipsa_spi,
		    inassoc->ipsa_srcaddr, inassoc->ipsa_dstaddr,
		    inassoc->ipsa_addrlen);
		mutex_exit(&bucket->isaf_lock);
		if (outassoc == NULL) {
			/* Q: Do we wish to set haspeer == B_FALSE? */
			ah0dbg(("ah_set_usetime: "
			    "can't find peer for inbound.\n"));
			sadb_set_usetime(inassoc);
			return;
		}
	} else {
		outassoc = assoc;
		bucket = &ah_inbound_assoc_v4[
		    INBOUND_HASH(outassoc->ipsa_spi)];
		mutex_enter(&bucket->isaf_lock);
		inassoc = sadb_getassocbyspi(bucket, outassoc->ipsa_spi,
		    outassoc->ipsa_srcaddr, outassoc->ipsa_dstaddr,
		    outassoc->ipsa_addrlen);
		mutex_exit(&bucket->isaf_lock);
		if (inassoc == NULL) {
			/* Q: Do we wish to set haspeer == B_FALSE? */
			ah0dbg(("ah_set_usetime: "
			    "can't find peer for outbound.\n"));
			sadb_set_usetime(outassoc);
			return;
		}
	}

	/* Update usetime on both. */
	sadb_set_usetime(inassoc);
	sadb_set_usetime(outassoc);

	/*
	 * REFRELE any peer SA.
	 *
	 * Because of the multi-line macro nature of IPSA_REFRELE, keep
	 * them in { }.
	 */
	if (inbound) {
		IPSA_REFRELE(outassoc);
	} else {
		IPSA_REFRELE(inassoc);
	}
}

/*
 * Add a number of bytes to what the SA has protected so far.  Return
 * B_TRUE if the SA can still protect that many bytes.
 *
 * Caller must REFRELE the passed-in assoc.  This function must REFRELE
 * any obtained peer SA.
 */
static boolean_t
ah_age_bytes(ipsa_t *assoc, uint64_t bytes, boolean_t inbound)
{
	ipsa_t *inassoc, *outassoc;
	isaf_t *bucket;
	boolean_t inrc, outrc;

	/* No peer?  No problem! */
	if (!assoc->ipsa_haspeer) {
		return (sadb_age_bytes(ah_pfkey_q, assoc, bytes,
		    B_TRUE));
	}

	/*
	 * Otherwise, we want to grab both the original assoc and its peer.
	 * There might be a race for this, but if it's a real race, two
	 * expire messages may occur.  We limit this by only sending the
	 * expire message on one of the peers, we'll pick the inbound
	 * arbitrarily.
	 *
	 * If we need tight synchronization on the peer SA, then we need to
	 * reconsider.
	 */

	if (inbound) {
		inassoc = assoc;
		/* XXX IPv6 : We need IPv6 code here. */
		bucket = &ah_outbound_assoc_v4[
		    OUTBOUND_HASH(*((ipaddr_t *)&inassoc->ipsa_dstaddr))];
		mutex_enter(&bucket->isaf_lock);
		outassoc = sadb_getassocbyspi(bucket, inassoc->ipsa_spi,
		    inassoc->ipsa_srcaddr, inassoc->ipsa_dstaddr,
		    inassoc->ipsa_addrlen);
		mutex_exit(&bucket->isaf_lock);
		if (outassoc == NULL) {
			/* Q: Do we wish to set haspeer == B_FALSE? */
			ah0dbg(("ah_age_bytes: "
			    "can't find peer for inbound.\n"));
			return (sadb_age_bytes(ah_pfkey_q, inassoc,
			    bytes, B_TRUE));
		}
	} else {
		outassoc = assoc;
		bucket = &ah_inbound_assoc_v4[
		    INBOUND_HASH(outassoc->ipsa_spi)];
		mutex_enter(&bucket->isaf_lock);
		inassoc = sadb_getassocbyspi(bucket, outassoc->ipsa_spi,
		    outassoc->ipsa_srcaddr, outassoc->ipsa_dstaddr,
		    outassoc->ipsa_addrlen);
		mutex_exit(&bucket->isaf_lock);
		if (inassoc == NULL) {
			/* Q: Do we wish to set haspeer == B_FALSE? */
			ah0dbg(("ah_age_bytes: "
			    "can't find peer for outbound.\n"));
			return (sadb_age_bytes(ah_pfkey_q, outassoc,
			    bytes, B_TRUE));
		}
	}

	inrc = sadb_age_bytes(ah_pfkey_q, inassoc, bytes, B_TRUE);
	outrc = sadb_age_bytes(ah_pfkey_q, outassoc, bytes, B_FALSE);

	/*
	 * REFRELE any peer SA.
	 *
	 * Because of the multi-line macro nature of IPSA_REFRELE, keep
	 * them in { }.
	 */
	if (inbound) {
		IPSA_REFRELE(outassoc);
	} else {
		IPSA_REFRELE(inassoc);
	}

	return (inrc && outrc);
}

/*
 * Perform the really difficult work of inserting the proposed situation.
 */
static void
ah_insert_prop(sadb_prop_t *prop, ipsacq_t *acqrec, uint_t combs)
{
	sadb_comb_t *comb = (sadb_comb_t *)(prop + 1);
	int aalg_count;
	ipsec_out_t *io;

	io = (ipsec_out_t *)acqrec->ipsacq_mp->b_rptr;
	ASSERT(io->ipsec_out_type == IPSEC_OUT);

	prop->sadb_prop_exttype = SADB_EXT_PROPOSAL;
	prop->sadb_prop_len = SADB_8TO64(sizeof (sadb_prop_t));
	*(uint32_t *)(&prop->sadb_prop_replay) = 0;	/* Quick zero-out! */

	prop->sadb_prop_replay = ipsecah_replay_size;

	/*
	 * Based upon algorithm properties, and what-not, prioritize
	 * a proposal.  NOTE:  The IPSEC_OUT message may have something to
	 * say about this.  In fact, the ordered preference list should be
	 * taken first from the IPSEC_OUT, if it's there, THEN from the global
	 * "sorted" lists.
	 *
	 * The following heuristic will be used in the meantime:
	 *
	 *  for (auth algs in sortlist)
	 *   Add combination.  If I've hit limit, return.
	 */

	for (aalg_count = 0; aalg_count < ah_num_aalgs; aalg_count++) {
		ahstate_t *aalg;

		if (io->ipsec_out_ah_alg != 0) {
			/*
			 * XXX In the future, when ipsec_out expresses a
			 * *list* of preferred algorithms, handle that.
			 */
			aalg = ah_aalgs[io->ipsec_out_ah_alg];
			/* Hack to make this loop run once. */
			combs = 1;
		} else
			aalg = ah_aalgs[ah_aalgs_sortlist[aalg_count]];

		comb->sadb_comb_flags = 0;
		comb->sadb_comb_reserved = 0;
		comb->sadb_comb_encrypt = 0;
		comb->sadb_comb_encrypt_minbits = 0;
		comb->sadb_comb_encrypt_maxbits = 0;

		/*
		 * The following may be based on algorithm
		 * properties, but in the meantime, we just pick
		 * some good, sensible numbers.  Key mgmt. can
		 * (and perhaps should) be the place to finalize
		 * such decisions.
		 */

		/*
		 * No limits on allocations, since we really don't
		 * support that concept currently.
		 */
		comb->sadb_comb_soft_allocations = 0;
		comb->sadb_comb_hard_allocations = 0;

		comb->sadb_comb_soft_bytes = ipsecah_default_soft_bytes;
		comb->sadb_comb_hard_bytes = ipsecah_default_hard_bytes;
		comb->sadb_comb_soft_addtime = ipsecah_default_soft_addtime;
		comb->sadb_comb_hard_addtime = ipsecah_default_hard_addtime;
		comb->sadb_comb_soft_usetime = ipsecah_default_soft_usetime;
		comb->sadb_comb_hard_usetime = ipsecah_default_hard_usetime;

		comb->sadb_comb_auth = aalg->ahs_id;
		comb->sadb_comb_auth_minbits = aalg->ahs_minbits;
		comb->sadb_comb_auth_maxbits = aalg->ahs_maxbits;
		prop->sadb_prop_len += SADB_8TO64(sizeof (*comb));
		if (--combs == 0)
			return;
		comb++;
	}
}

/*
 * Prepare and actually send the SADB_ACQUIRE message to PF_KEY.
 */
static void
ah_send_acquire(ipsacq_t *acqrec)
{
	mblk_t *pfkeymp, *msgmp;
	keysock_out_t *kso;
	uint_t allocsize, combs;
	sadb_msg_t *samsg;
	sadb_prop_t *prop;

	ah_acquire_requests++;

	ASSERT(MUTEX_HELD(&acqrec->ipsacq_lock));

	pfkeymp = allocb(sizeof (ipsec_info_t), BPRI_HI);
	if (pfkeymp == NULL) {
		ah1dbg(("ah_send_acquire: 1st allocb() failed.\n"));
		/* Just bail. */
		goto done;
	}

	pfkeymp->b_datap->db_type = M_CTL;
	pfkeymp->b_wptr += sizeof (ipsec_info_t);
	kso = (keysock_out_t *)pfkeymp->b_rptr;
	kso->ks_out_type = KEYSOCK_OUT;
	kso->ks_out_len = sizeof (*kso);
	kso->ks_out_serial = 0;

	/*
	 * First, allocate a basic ACQUIRE message.  Beyond that,
	 * you need to extract certificate info from
	 */
	allocsize = sizeof (sadb_msg_t) + sizeof (sadb_address_t) +
	    sizeof (sadb_address_t) + sizeof (sadb_prop_t);

	switch (acqrec->ipsacq_addrlen) {
	case sizeof (ipaddr_t):
		allocsize += 2 * sizeof (struct sockaddr_in);
		break;
	/* Put in IPv6 case here later. */
	}

	if (ipsecah_max_combs == 0)
		combs = ah_num_aalgs;
	else
		combs = ipsecah_max_combs;

	allocsize += combs * sizeof (sadb_comb_t);

	/*
	 * XXX If there are:
	 *	certificate IDs
	 *	proxy address
	 *	<Others>
	 * add additional allocation size.
	 */

	msgmp = allocb(allocsize, BPRI_HI);
	if (msgmp == NULL) {
		ah0dbg(("ah_send_acquire: 2nd allocb() failed.\n"));
		/* Just bail. */
		freemsg(pfkeymp);
		pfkeymp = NULL;
		goto done;
	}

	samsg = (sadb_msg_t *)msgmp->b_rptr;
	pfkeymp->b_cont = msgmp;

	/* Set up ACQUIRE. */
	samsg->sadb_msg_satype = SADB_SATYPE_AH;
	sadb_setup_acquire(samsg, acqrec);

	/* XXX Insert proxy address information here. */

	/* XXX Insert identity information here. */

	/* XXXMLS Insert sensitivity information here. */

	/* Insert proposal here. */

	prop = (sadb_prop_t *)(((uint64_t *)samsg) + samsg->sadb_msg_len);
	ah_insert_prop(prop, acqrec, combs);
	samsg->sadb_msg_len += prop->sadb_prop_len;
	msgmp->b_wptr += SADB_64TO8(samsg->sadb_msg_len);

done:
	/*
	 * Must mutex_exit() before sending PF_KEY message up, in
	 * order to avoid recursive mutex_enter() if there are no registered
	 * listeners.
	 *
	 * Once I've sent the message, I'm cool anyway.
	 */
	mutex_exit(&acqrec->ipsacq_lock);
	if (pfkeymp != NULL) {
		if (ah_pfkey_q != NULL)
			putnext(ah_pfkey_q, pfkeymp);
		else
			freemsg(pfkeymp);
	}
}

/*
 * An outbound packet needs an SA.  Set up an ACQUIRE record, then send an
 * ACQUIRE message.
 */
static void
ah_acquire(mblk_t *mp, ipha_t *ipha)
{
	mblk_t *iomp, *datamp;
	uint32_t seq;
	ipsec_out_t *io;
	ipsacq_t *acqrec;
	iacqf_t *bucket;

	iomp = mp;
	io = (ipsec_out_t *)iomp->b_rptr;
	datamp = iomp->b_cont;
	ASSERT(io->ipsec_out_type == IPSEC_OUT);
	ASSERT(datamp != NULL);
	ASSERT(datamp->b_rptr == (uchar_t *)ipha);

	/*
	 * Set up an ACQUIRE record.  (XXX IPv4 specific, need IPv6 code!).
	 * We will eventually want to pull the PROXY source address from
	 * either the inner IP header, or from a future extension to the
	 * IPSEC_OUT message.
	 *
	 * Actually, we'll also want to check for duplicates.
	 */

	seq = atomic_add_32_nv(&ah_acquire_seq, -1);
	/*
	 * Make sure the ACQUIRE sequence number doesn't slip below the
	 * lowest point allowed in the kernel.  (In other words, make sure
	 * the high bit on the sequence number is set.)
	 */
	seq |= IACQF_LOWEST_SEQ;

	if (seq == (uint32_t)-1) {
		/* We have rolled over.  Note it. */
		(void) strlog(info.mi_idnum, 0, 0, SL_ERROR | SL_NOTE,
		    "AH ACQUIRE sequence number has wrapped.\n");
	}

	/* XXX IPv4-specific code. */
	bucket = &ah_acquires_v4[OUTBOUND_HASH(ipha->ipha_dst)];
	/* Need v6 code?!? */
	acqrec = sadb_new_acquire(bucket, seq, mp, SADB_SATYPE_AH,
	    ipsecah_acquire_timeout, sizeof (ipaddr_t));

	if (acqrec == NULL) {
		/* Error condition.  Send a failure back to IP. */
		io->ipsec_out_ah_req |= IPSEC_REQ_FAILED;
		ASSERT(ah_ip_q != NULL);
		putnext(ah_ip_q, mp);
		return;
	}

	/*
	 * NOTE:	At this point, "mp" is in the acquire record pointed
	 *		to by acqreq.  We don't need to freemsg, or anything
	 *		else like that.
	 */

	if (acqrec->ipsacq_seq != seq || acqrec->ipsacq_numpackets > 1) {
		/* I have an acquire outstanding already! */
		mutex_exit(&acqrec->ipsacq_lock);
		return;
	}

	/*
	 * Send an ACQUIRE message based on this new record.
	 * I send one ACQUIRE per new acquire.  If the new acquire
	 * has exactly one mblk, then I send.  The sadb_new_acquire()
	 * returns the new record with its lock held.
	 */
	ah_send_acquire(acqrec);
}

/*
 * Handle the SADB_GETSPI message.  Create a larval SA.
 *
 * XXX IPv4 specific.  Fix for IPv6.
 */
static void
ah_getspi(mblk_t *mp, keysock_in_t *ksi)
{
	ipsa_t *newbie, *target;
	isaf_t *outbound, *inbound;
	int rc;
	sadb_sa_t *assoc;
	keysock_out_t *kso;
	uint32_t increment;

	/*
	 * Randomly increment the SPI value, now that we've used it.
	 *
	 * NOTE:  On little-endian HW, I'm increasing the "high" bits,
	 *	  but this is a random increment, so it's really no big deal.
	 */
	sadb_get_random_bytes(&increment, sizeof (uint32_t));
	newbie = sadb_getspi(ksi, atomic_add_32_nv(&ah_spi, increment));

	if (newbie == NULL) {
		sadb_pfkey_error(ah_pfkey_q, mp, ENOMEM, ksi->ks_in_serial);
		return;
	} else if (newbie == (ipsa_t *)-1) {
		sadb_pfkey_error(ah_pfkey_q, mp, EINVAL, ksi->ks_in_serial);
		return;
	}

	/*
	 * XXX - We may randomly collide.  We really should recover from this.
	 *	 Unfortunately, that could require spending way-too-much-time
	 *	 in here.  For now, let the user retry.
	 */

	mutex_enter(&ah_larval_list_v4.isaf_lock);
	outbound = &ah_outbound_assoc_v4[
	    OUTBOUND_HASH(*(uint32_t *)(newbie->ipsa_dstaddr))];
	mutex_enter(&outbound->isaf_lock);
	inbound = &ah_inbound_assoc_v4[INBOUND_HASH(newbie->ipsa_spi)];
	mutex_enter(&inbound->isaf_lock);

	/*
	 * Check for collisions (i.e. did sadb_getspi() return with something
	 * that already exists?).
	 *
	 * Try outbound first.  Even though SADB_GETSPI is traditionally
	 * for inbound SAs, you never know what a user might do.
	 */
	target = sadb_getassocbyspi(outbound, newbie->ipsa_spi,
	    newbie->ipsa_srcaddr, newbie->ipsa_dstaddr, newbie->ipsa_addrlen);
	if (target == NULL) {
		target = sadb_getassocbyspi(inbound, newbie->ipsa_spi,
		    newbie->ipsa_srcaddr, newbie->ipsa_dstaddr,
		    newbie->ipsa_addrlen);
	}

	/*
	 * We have the larval lock, and I don't have collisions elsewhere!
	 * (Nor will I because I'm still holding inbound/outbound locks.)
	 */

	if (target != NULL) {
		rc = EEXIST;
		IPSA_REFRELE(target);
	} else {
		/*
		 * sadb_insertassoc() also checks for collisions, so
		 * if there's a colliding larval entry, rc will be set
		 * to EEXIST.
		 */
		rc = sadb_insertassoc(newbie, &ah_larval_list_v4);
		(void) drv_getparm(TIME, &newbie->ipsa_hardexpiretime);
		newbie->ipsa_hardexpiretime += ipsecah_larval_timeout;
	}

	/* Can exit other mutexes.  Hold larval until we're done with newbie. */
	mutex_exit(&inbound->isaf_lock);
	mutex_exit(&outbound->isaf_lock);

	if (rc != 0) {
		mutex_exit(&ah_larval_list_v4.isaf_lock);
		IPSA_REFRELE(newbie);
		sadb_pfkey_error(ah_pfkey_q, mp, rc, ksi->ks_in_serial);
		return;
	}

	/*
	 * Construct successful return message.  We have one thing going
	 * for us in PF_KEY v2.  That's the fact that
	 *	sizeof (sadb_spirange_t) == sizeof(sadb_sa_t)
	 */
	assoc = (sadb_sa_t *)ksi->ks_in_extv[SADB_EXT_SPIRANGE];
	assoc->sadb_sa_exttype = SADB_EXT_SA;
	assoc->sadb_sa_spi = newbie->ipsa_spi;
	*((uint64_t *)(&assoc->sadb_sa_replay)) = 0;
	mutex_exit(&ah_larval_list_v4.isaf_lock);

	/* Convert KEYSOCK_IN to KEYSOCK_OUT. */
	kso = (keysock_out_t *)ksi;
	kso->ks_out_len = sizeof (*kso);
	kso->ks_out_serial = ksi->ks_in_serial;
	kso->ks_out_type = KEYSOCK_OUT;

	/*
	 * Can safely putnext() to ah_pfkey_q, because this is a turnaround
	 * from the ah_pfkey_q.
	 */
	putnext(ah_pfkey_q, mp);
}

/*
 * IP sends up the ICMP errors for validation and the removal of
 * the AH header.
 */
static void
ah_icmp_error_v4(queue_t *q, mblk_t *ipsec_mp)
{
	mblk_t *mp;
	mblk_t *mp1;
	icmph_t *icmph;
	int iph_hdr_length;
	int hdr_length;
	isaf_t *hptr;
	ipsa_t *assoc;
	int ah_length;
	ipha_t *ipha;
	ipha_t *oipha;
	ah_t *ah;
	uint32_t length;
	int alloc_size;
	uint8_t nexthdr;

	mp = ipsec_mp->b_cont;
	ASSERT(mp->b_datap->db_type == M_CTL);

	/*
	 * Change the type to M_DATA till we finish pullups.
	 */
	mp->b_datap->db_type = M_DATA;

	oipha = ipha = (ipha_t *)mp->b_rptr;
	iph_hdr_length = IPH_HDR_LENGTH(ipha);
	icmph = (icmph_t *)&mp->b_rptr[iph_hdr_length];

	ipha = (ipha_t *)&icmph[1];
	hdr_length = IPH_HDR_LENGTH(ipha);

	/*
	 * See if we have enough to locate the SPI
	 */
	if ((uchar_t *)ipha + hdr_length + 8 > mp->b_wptr) {
		if (!pullupmsg(mp, (uchar_t *)ipha + hdr_length + 8 -
			    mp->b_rptr)) {
			ipsec_rl_strlog(info.mi_idnum, 0, 0,
			    SL_WARN | SL_ERROR,
			    "ICMP error : Small AH header\n");
			ah_in_discards++;
			freemsg(ipsec_mp);
			return;
		}
		icmph = (icmph_t *)&mp->b_rptr[iph_hdr_length];
		ipha = (ipha_t *)&icmph[1];
	}

	ah = (ah_t *)((uint8_t *)ipha + hdr_length);
	nexthdr = ah->ah_nexthdr;

	hptr = &ah_outbound_assoc_v4[OUTBOUND_HASH(ipha->ipha_dst)];
	mutex_enter(&hptr->isaf_lock);
	assoc = sadb_getassocbyspi(hptr, ah->ah_spi, (uint8_t *)&ipha->ipha_src,
	    (uint8_t *)&ipha->ipha_dst, IP_ADDR_LEN);
	mutex_exit(&hptr->isaf_lock);

	if (assoc == NULL) {
		ah_lookup_failure++;
		ah_in_discards++;
		ipsec_rl_strlog(info.mi_idnum, 0, 0, SL_CONSOLE | SL_WARN |
		    SL_ERROR, "Bad ICMP message - No association for the "
		    "attached AH header whose spi is 0x%x, sender is 0x%x\n",
		    ntohl(ah->ah_spi), ntohl(oipha->ipha_src));
		freemsg(ipsec_mp);
		return;
	}

	IPSA_REFRELE(assoc);
	/*
	 * There seems to be a valid association. If there
	 * is enough of AH header remove it, otherwise remove
	 * as much as possible and send it back. One could check
	 * whether it has complete AH header plus 8 bytes but it
	 * does not make sense if an icmp error is returned for
	 * ICMP messages e.g ICMP time exceeded, that are being
	 * sent up. Let the caller figure out.
	 *
	 * NOTE: ah_length is the number of 32 bit words minus 2.
	 */
	ah_length = (ah->ah_length << 2) + 8;

	if ((uchar_t *)ipha + hdr_length + ah_length > mp->b_wptr) {
		if (mp->b_cont == NULL) {
			/*
			 * There is nothing to pullup. Just remove as
			 * much as possible. This is a common case for
			 * IPV4.
			 */
			ah_length = (mp->b_wptr - ((uchar_t *)ipha +
			    hdr_length));
			goto done;
		}
		/* Pullup the full ah header */
		if (!pullupmsg(mp, (uchar_t *)ah + ah_length - mp->b_rptr)) {
			/*
			 * pullupmsg could have failed if there was not
			 * enough to pullup or memory allocation failed.
			 * We tried hard, give up now.
			 */
			ah_in_discards++;
			freemsg(ipsec_mp);
			return;
		}
		icmph = (icmph_t *)&mp->b_rptr[iph_hdr_length];
		ipha = (ipha_t *)&icmph[1];
	}
done:
	/*
	 * Remove the AH header and change the protocol.
	 * Don't update the spi fields in the ipsec_in
	 * message as we are called just to validate the
	 * message attached to the ICMP message.
	 *
	 * If we never pulled up since all of the message
	 * is in one single mblk, we can't remove the AH header
	 * by just setting the b_wptr to the beginning of the
	 * AH header. We need to allocate a mblk that can hold
	 * up until the inner IP header and copy them.
	 */
	alloc_size = iph_hdr_length + sizeof (icmph_t) + hdr_length;

	if ((mp1 = allocb(alloc_size, BPRI_LO)) == NULL) {
		ah_in_discards++;
		freemsg(ipsec_mp);
		return;
	}
	/* ICMP errors are M_CTL messages */
	mp1->b_datap->db_type = M_CTL;
	ipsec_mp->b_cont = mp1;
	bcopy(mp->b_rptr, mp1->b_rptr, alloc_size);
	mp1->b_wptr += alloc_size;

	/*
	 * Skip whatever we have copied and as much of AH header
	 * possible. If we still have something left in the original
	 * message, tag on.
	 */
	mp->b_rptr = (uchar_t *)ipha + hdr_length + ah_length;

	if (mp->b_rptr != mp->b_wptr) {
		mp1->b_cont = mp;
	} else {
		if (mp->b_cont != NULL)
			mp1->b_cont = mp->b_cont;
		freeb(mp);
	}

	ipha = (ipha_t *)(mp1->b_rptr + iph_hdr_length + sizeof (icmph_t));
	ipha->ipha_protocol = nexthdr;
	length = ntohs(ipha->ipha_length);
	length -= ah_length;
	ipha->ipha_length = htons((uint16_t)length);
	ipha->ipha_hdr_checksum = 0;
	ipha->ipha_hdr_checksum = (uint16_t)ip_csum_hdr(ipha);

	qreply(q, ipsec_mp);
}

/*
 * This function constructs a pseudo header by looking at the IP header
 * and options if any. This is called for both outbound and inbound,
 * before fanning out to the Algorithm module.
 */
static mblk_t *
ah_process_ip_options_v4(mblk_t *mp, ipsa_t *assoc, auth_req_t *ar,
    boolean_t outbound)
{
	ipha_t	*ipha;
	ipha_t	*oipha;
	mblk_t 	*phdr_mp;
	int	 size;
	uint32_t option_length;
	uchar_t	*oi_opt;
	uchar_t	*ni_opt;
	uint32_t optval;
	uint32_t optlen;
	ipaddr_t dst;
	ah_t 	*ah;
	ah_t	*oah;
	uint_t 	ah_data_sz;
	uint32_t v_hlen_tos_len;
	int ip_hdr_length;
	uint_t	ah_align_sz;
	int	i;
	uint32_t off;

#ifdef	_BIG_ENDIAN
#define	V_HLEN	(v_hlen_tos_len >> 24)
#else
#define	V_HLEN	(v_hlen_tos_len & 0xFF)
#endif

	oipha = (ipha_t *)mp->b_rptr;
	v_hlen_tos_len = ((uint32_t *)oipha)[0];

	/*
	 * Allocate space for the authentication data also. It is
	 * useful both during the ICV calculation where we need to
	 * feed in zeroes and while sending the datagram back to IP
	 * where we will be using the same space.
	 *
	 * We need to allocate space for padding bytes if it is not
	 * a multiple of IPV4_PADDING_ALIGN.
	 */

	ah_data_sz = ah_aalgs[assoc->ipsa_auth_alg]->ahs_datalen;
	ah_align_sz = (ah_data_sz + IPV4_PADDING_ALIGN - 1) &
	    -IPV4_PADDING_ALIGN;

	ASSERT(ah_align_sz >= ah_data_sz);

	size = IP_SIMPLE_HDR_LENGTH + sizeof (ah_t) + ah_align_sz;

	if (V_HLEN != IP_SIMPLE_HDR_VERSION) {
		option_length = oipha->ipha_version_and_hdr_length -
		    (uint8_t)((IP_VERSION << 4) +
		    IP_SIMPLE_HDR_LENGTH_IN_WORDS);
		option_length <<= 2;
		size += option_length;
	}

	if ((phdr_mp = allocb(size, BPRI_HI)) == NULL) {
		return (NULL);
	}

	/*
	 * Form the basic IP header first.
	 */
	ipha = (ipha_t *)phdr_mp->b_rptr;
	ipha->ipha_version_and_hdr_length = oipha->ipha_version_and_hdr_length;
	ipha->ipha_type_of_service = 0;

	if (outbound) {
		/*
		 * Include the size of AH and authentication data.
		 * This is how our recipient would compute the
		 * authentication data. Look at what we do in the
		 * inbound case below.
		 */
		ipha->ipha_length = ntohs(htons(oipha->ipha_length) +
		    sizeof (ah_t) + ah_align_sz);
	} else {
		ipha->ipha_length = oipha->ipha_length;
	}

	ipha->ipha_ident = oipha->ipha_ident;
	ipha->ipha_fragment_offset_and_flags = 0;
	ipha->ipha_ttl = 0;
	ipha->ipha_protocol = IPPROTO_AH;
	ipha->ipha_hdr_checksum = 0;
	ipha->ipha_src = oipha->ipha_src;
	ipha->ipha_dst = dst = oipha->ipha_dst;

	/*
	 * If there is no option to process return now.
	 */
	ip_hdr_length = IP_SIMPLE_HDR_LENGTH;

	if (V_HLEN == IP_SIMPLE_HDR_VERSION) {
		/* Form the AH header */
		goto ah_hdr;
	}

	ip_hdr_length += option_length;

	/*
	 * We have options. In the outbound case for source route,
	 * ULP has already moved the first hop, which is now in
	 * ipha_dst. We need the final destination for the calculation
	 * of authentication data. And also make sure that mutable
	 * and experimental fields are zeroed out in the IP options.
	 * We assume that there are no problems with the options
	 * as IP should have already checked this.
	 */

	oi_opt = (uchar_t *)&oipha[1];
	ni_opt = (uchar_t *)&ipha[1];

	while (option_length != 0) {
		switch (optval = oi_opt[IPOPT_POS_VAL]) {
		case IPOPT_EOL:
			ni_opt[IPOPT_POS_VAL] = IPOPT_EOL;	/* Immutable */
			goto over;
		case IPOPT_NOP:
			optlen = 1;
			break;
		default:
			optlen = oi_opt[IPOPT_POS_LEN];
			break;
		}
		if (optlen == 0 || optlen > option_length) {
			ah1dbg(("AH : bad IPv4 option\
			    len %d, %d\n", optlen, option_length));
			freeb(phdr_mp);
			return (NULL);
		}
		switch (optval) {
		case IPOPT_NOP:
		case IPOPT_EXTSEC:
		case IPOPT_COMSEC:
		case IPOPT_RALERT:
		case IPOPT_SDMDD:
		case IPOPT_SEC:
			/*
			 * Immutable. Copy them over. Assume optlen gives
			 * the total length.
			 */
			bcopy(oi_opt, ni_opt, optlen);
			break;
		case IPOPT_SSRR:
		case IPOPT_LSRR:
			/*
			 * Get the final destination and then zero them
			 * out.
			 */
			off = oi_opt[IPOPT_POS_OFF];
			/*
			 * If one of the conditions is true, it means
			 * end of options and dst already has the right
			 * value. So, just fall through.
			 */
			if (!(optlen < IP_ADDR_LEN ||
			    off > optlen - 3)) {
				off = optlen - IP_ADDR_LEN;
				bcopy(&oi_opt[off], &dst, IP_ADDR_LEN);
			}
			/* FALLTHRU */
		case IPOPT_RR:
		case IPOPT_IT:
		case IPOPT_SID:
		default :
			/*
			 * optlen should include from the beginning of an
			 * option.
			 * NOTE : Stream Identifier Option (SID): RFC 791
			 * shows the bit pattern of optlen as 2 and documents
			 * the length as 4. We assume it to be 2 here.
			 */
			bzero(ni_opt, optlen);
			break;
		}
		option_length -= optlen;
		oi_opt += optlen;
		ni_opt += optlen;
	}

over:
	/*
	 * Don't change ipha_dst for an inbound datagram as it points
	 * to the right value. Only for the outbound with LSRR/SSRR,
	 * because of ip_massage_options called by the ULP, ipha_dst
	 * points to the first hop and we need to use the final
	 * destination for computing the ICV.
	 */

	if (outbound)
		ipha->ipha_dst = dst;
ah_hdr:
	ah = (ah_t *)((char *)ipha + ip_hdr_length);

	/*
	 * Padding :
	 *
	 * 1) Authentication data may have to be padded
	 * before ICV calcualation if ICV is not a multiple
	 * of 32 bits. This padding is arbitrary and transmitted
	 * with the packet at the end of the authentication data.
	 * Payload length should include the padding bytes.
	 *
	 * 2) Explicit padding of the whole datagram may be
	 * required by the alogorithm which need not be
	 * transmitted. It is assumed that this will be taken
	 * care by the algoritm module.
	 */
	bzero((char *)ah + sizeof (ah_t), ah_data_sz);
	if (outbound) {
		ah->ah_nexthdr = oipha->ipha_protocol;

		/*
		 * 3 Fixed portion of ah and x words of the Algorithm
		 * digest - 2 gives the length i.e (3 + x) - 2, which
		 * is x + 1.
		 * For MD5-HMAC, MD5-SHA1, it is (3 + 3) - 2 = 4.
		 */
		ah->ah_length = (ah_align_sz >> 2) + 1;
		ah->ah_reserved = 0;
		ah->ah_spi = assoc->ipsa_spi;

		ah->ah_replay = htonl(atomic_add_32_nv
		    (&assoc->ipsa_replay, 1));
		if (ah->ah_replay == 0 && assoc->ipsa_replay_wsize != 0) {
			/*
			 * XXX We have replay counter wrapping.  We probably
			 * want to nuke this SA (and its peer).  XXX IPv6 also
			 * print address, but in an AF-independent way.
			 */
			ipsec_rl_strlog(info.mi_idnum, 0, 0,
			    SL_ERROR | SL_CONSOLE | SL_WARN,
			    "Outbound AH SA (0x%x) has wrapped sequence.\n",
			    htonl(ah->ah_spi));

			sadb_replay_delete(assoc);
			freeb(phdr_mp);
			/*
			 * Returning NULL will tell the caller to
			 * IPSA_REFELE(), free the memory, etc.
			 */
			return (NULL);
		}

		/*
		 * Algorithm module will skip ip_hdr_length worth
		 * of data i.e the complete ip header which is now
		 * in the pseudo header.
		 */
		ar->auth_req_startoffset = ip_hdr_length;
		if (ah_data_sz != ah_align_sz) {
			uchar_t *pad = ((uchar_t *)ah + sizeof (ah_t) +
			    ah_data_sz);

			for (i = 0; i < (ah_align_sz - ah_data_sz); i++) {
				pad[i] = (uchar_t)i;	/* Fill the padding */
			}
		}
	} else {
		oah = (ah_t *)((char *)oipha + ip_hdr_length);
		ah->ah_nexthdr = oah->ah_nexthdr;
		ah->ah_length = oah->ah_length;
		ah->ah_reserved = 0;
		ASSERT(oah->ah_spi == assoc->ipsa_spi);
		ah->ah_spi = oah->ah_spi;
		ah->ah_replay = oah->ah_replay;
		/*
		 * Algorithm module will skip the ip header and the
		 * AH header of the packet which is now contained in
		 * the pseudo header.
		 */
		ar->auth_req_startoffset = ip_hdr_length + sizeof (ah_t) +
		    ah_align_sz;
		if (ah_data_sz != ah_align_sz) {
			uchar_t *opad = ((uchar_t *)oah + sizeof (ah_t) +
			    ah_data_sz);
			uchar_t *pad = ((uchar_t *)ah + sizeof (ah_t) +
			    ah_data_sz);

			for (i = 0; i < (ah_align_sz - ah_data_sz); i++) {
				pad[i] = opad[i];	/* Copy the padding */
			}
		}
	}

	phdr_mp->b_wptr = ((uchar_t *) ah + sizeof (ah_t) + ah_align_sz);

	ASSERT(phdr_mp->b_wptr <= phdr_mp->b_datap->db_lim);
	return (phdr_mp);
}

/*
 * Authenticate the outbound datagram. This function is called
 * whenever IP sends an outbound datagram that needs authentication.
 */
static void
ah_outbound_v4(queue_t *q, mblk_t *ipsec_out, ipsa_t *assoc)
{
	ipha_t *ipha;
	mblk_t *mp;
	mblk_t *phdr_mp;
	mblk_t *areq;
	auth_req_t *ar;
	ipsec_out_t *oi;
	isaf_t *hptr;
	ipaddr_t dst;
	uint_t ah_dsize;

	/*
	 * Construct the chain of mblks
	 *
	 * AUTH_REQ->IPSEC_OUT->PSEUDO_HDR->DATA
	 *
	 * one by one.
	 */

	ah_out_requests++;

	ASSERT(ipsec_out->b_datap->db_type == M_CTL);

	ASSERT((ipsec_out->b_wptr - ipsec_out->b_rptr)
	    >= sizeof (ipsec_info_t));

	mp = ipsec_out->b_cont;
	oi = (ipsec_out_t *)ipsec_out->b_rptr;

	ASSERT(mp->b_datap->db_type == M_DATA);

	if (oi->ipsec_out_ah_alg != 0 &&
	    ah_aalgs[oi->ipsec_out_ah_alg] == NULL) {
		ah0dbg(("ah_outbound_v4: Bad authentication algorithm %d.\n",
		    oi->ipsec_out_ah_alg));
		ah_out_discards++;
		freemsg(ipsec_out);
		return;
	}

	ipha = (ipha_t *)mp->b_rptr;
	dst = ip_get_dst(ipha);
	/*
	 * Getting the outbound association will be considerably painful.
	 * sadb_getassocbyipc() will require more parameters as policy
	 * implementations mature.
	 */
	if (assoc == NULL || assoc->ipsa_state == IPSA_STATE_DEAD) {
		hptr = &ah_outbound_assoc_v4[OUTBOUND_HASH(dst)];

		/*
		 * If the association is dead, we still do a lookup
		 * to find some other appropriate SA.
		 */
		if (assoc != NULL) {
			IPSA_REFRELE(assoc);
		}
		mutex_enter(&hptr->isaf_lock);
		assoc = sadb_getassocbyipc(hptr, oi, (uint8_t *)&ipha->ipha_src,
		    (uint8_t *)&dst, IP_ADDR_LEN, IPPROTO_AH);
		mutex_exit(&hptr->isaf_lock);

		if (assoc == NULL || assoc->ipsa_state == IPSA_STATE_DEAD) {
			ah3dbg(("ah_outbound_v4:  "
			    "Would send PF_KEY ACQUIRE message here!\n"));
			if (assoc != NULL) {
				IPSA_REFRELE(assoc);
			}
			ah_acquire(ipsec_out, ipha);
			return;
		}
	}

	if (ah_aalgs[assoc->ipsa_auth_alg] == NULL) {
		ah1dbg(("ah_outbound_v4: Algorithm unloaded behind my back!"));
		sadb_replay_delete(assoc);
		IPSA_REFRELE(assoc);
		ah_out_discards++;
		freemsg(ipsec_out);
		return;
	}

	if (assoc->ipsa_usetime == 0)
		ah_set_usetime(assoc, B_FALSE);

	/* Taken from ah_process_ip_options_v4 */
	ah_dsize = ah_aalgs[assoc->ipsa_auth_alg]->ahs_datalen;
	ah_dsize = (ah_dsize + IPV4_PADDING_ALIGN - 1) & -IPV4_PADDING_ALIGN;

	if (!ah_age_bytes(assoc,
	    ntohs(ipha->ipha_length) + sizeof (ah_t) + ah_dsize, B_FALSE)) {
		/*
		 * Rig things as if sadb_getassocbyipc() failed.  Jump to
		 * above.
		 */
		ipsec_rl_strlog(info.mi_idnum, 0, 0, SL_ERROR | SL_WARN,
		    "AH association 0x%x, dst %d.%d.%d.%d had bytes expire.\n",
		    ntohl(assoc->ipsa_spi), assoc->ipsa_dstaddr[0],
		    assoc->ipsa_dstaddr[1], assoc->ipsa_dstaddr[2],
		    assoc->ipsa_dstaddr[3]);
		IPSA_REFRELE(assoc);
		ah_acquire(ipsec_out, ipha);
		return;
	}

	if ((areq = allocb(sizeof (ipsec_info_t), BPRI_HI)) == NULL) {
		IPSA_REFRELE(assoc);
		oi->ipsec_out_ah_req |= IPSEC_REQ_FAILED;
		qreply(q, ipsec_out);
		return;
	}

	ar = (auth_req_t *)areq->b_rptr;

	phdr_mp = ah_process_ip_options_v4(mp, assoc, ar, B_TRUE);

	if (phdr_mp == NULL) {
		freemsg(areq);
		IPSA_REFRELE(assoc);
		oi->ipsec_out_ah_req |= IPSEC_REQ_FAILED;
		qreply(q, ipsec_out);
		return;
	}

	/*
	 * auth_req_startoffset is filled in by ah_process_ip_options_v4.
	 * This tells the algorithm module where to start authenticating
	 * the datagram from.
	 */

	areq->b_datap->db_type = M_CTL;
	areq->b_wptr += sizeof (ipsec_info_t);
	ar->auth_req_type = AUTH_PI_OUT_AUTH_REQ;
	ar->auth_req_len = sizeof (auth_req_t);
	ar->auth_req_assoc = assoc;

	areq->b_cont = ipsec_out;
	ipsec_out->b_cont = phdr_mp;
	phdr_mp->b_cont = mp;

	/*
	 * This goes up to the algorithm module. It comes back
	 * to ipsecah_wput -> ah_auth_out_done_v4().
	 */
	(void) putnext(ah_aalgs[assoc->ipsa_auth_alg]->ahs_rq, areq);
}

/*
 * Authenticate the Inbound IPv4 datagram. This is called whenever
 * IP sends up the datagram on seeing the AH header.
 */
static void
ah_inbound_v4(mblk_t *mp)
{
	mblk_t *ipsec_in;
	ipha_t *ipha;
	mblk_t *phdr_mp;
	mblk_t *areq;
	auth_req_t *ar;
	ipsa_t 	*assoc;
	uint32_t ip_hdr_length;
	ah_t *ah;
	isaf_t *hptr;
	int ah_length;

	ah_in_requests++;

	ASSERT(mp->b_datap->db_type == M_CTL);

	ipsec_in = mp;
	mp = mp->b_cont;

	ASSERT(mp->b_datap->db_type == M_DATA);

	ipha = (ipha_t *)mp->b_rptr;
	ASSERT(ipha->ipha_protocol == IPPROTO_AH);

	ip_hdr_length = ipha->ipha_version_and_hdr_length -
	    (uint8_t)((IP_VERSION << 4));
	ip_hdr_length <<= 2;

	/*
	 * We assume that the IP header is pulled up until
	 * the options. We need to see whether we have the
	 * AH header in the same mblk or not.
	 */
	if ((uchar_t *)ipha + ip_hdr_length + sizeof (ah_t) > mp->b_wptr) {
		if (!pullupmsg(mp, (uchar_t *)ipha + ip_hdr_length +
		    sizeof (ah_t) - mp->b_rptr)) {
			ipsec_rl_strlog(info.mi_idnum, 0, 0,
			    SL_WARN | SL_ERROR,
			    "ah_inbound_v4 : Small AH header\n");
			ah_in_discards++;
			freemsg(ipsec_in);
			return;
		}
		ipha = (ipha_t *)mp->b_rptr;
	}
	ah = (ah_t *)((uint8_t *)ipha + ip_hdr_length);

	/*
	 * Construct the chain of mblks
	 *
	 * AUTH_REQ->IPSEC_INFO->PSEUDO_HDR->DATA
	 *
	 * one by one.
	 */

	hptr = &ah_inbound_assoc_v4[INBOUND_HASH(ah->ah_spi)];
	mutex_enter(&hptr->isaf_lock);
	assoc = sadb_getassocbyspi(hptr, ah->ah_spi, (uint8_t *)&ipha->ipha_src,
	    (uint8_t *)&ipha->ipha_dst, IP_ADDR_LEN);
	mutex_exit(&hptr->isaf_lock);

	if (assoc == NULL || assoc->ipsa_state == IPSA_STATE_DEAD) {
		ah_lookup_failure++;
		ipsec_rl_strlog(info.mi_idnum, 0, 0,
		    SL_ERROR | SL_CONSOLE | SL_WARN,
		    "ah_inbound_v4 : No association found, spi=0x%x ,"
		    "dst addr %x.\n", ntohl(ah->ah_spi),
		    ntohl(ipha->ipha_dst));
		if (assoc != NULL) {
			IPSA_REFRELE(assoc);
		}
		ah_in_discards++;
		freemsg(ipsec_in);
		return;
	}

	if (assoc->ipsa_usetime == 0)
		ah_set_usetime(assoc, B_TRUE);

	/*
	 * We may wish to check replay in-range-only here as an optimization.
	 * Include the reality check of ipsa->ipsa_replay >
	 * ipsa->ipsa_replay_wsize for times when it's the first N packets,
	 * where N == ipsa->ipsa_replay_wsize.
	 *
	 * Another check that may come here later is the "collision" check.
	 * If legitimate packets flow quickly enough, this won't be a problem,
	 * but collisions may cause authentication algorithm crunching to
	 * take place when it doesn't need to.
	 */
	if (!sadb_replay_peek(assoc, ah->ah_replay)) {
		ah_replay_early_failures++;
		ah_in_discards++;
		freemsg(ipsec_in);
		IPSA_REFRELE(assoc);
		return;
	}

	/*
	 * Check to see if the algorithm was unloaded behind my back,
	 * and if not, allocate an AUTH PI message.
	 */
	if (ah_aalgs[assoc->ipsa_auth_alg] == NULL ||
	    (areq = allocb(sizeof (ipsec_info_t), BPRI_HI)) == NULL) {
		ah_in_discards++;
		freemsg(ipsec_in);
		IPSA_REFRELE(assoc);
		return;
	}

	/*
	 * We need to pullup until the ICV before we call
	 * ah_process_ip_options_v4.
	 */
	ah_length = (ah->ah_length << 2) + 8;
	/*
	 * NOTE : If we want to use any field of IP/AH header, you need
	 * to re-assign following the pullup.
	 */
	if (((uchar_t *)ah + ah_length) > mp->b_wptr) {
		if (!pullupmsg(mp, (uchar_t *)ah + ah_length - mp->b_rptr)) {
			ipsec_rl_strlog(info.mi_idnum, 0, 0,
			    SL_WARN | SL_ERROR,
			    "ah_inbound_v4 : Small AH header\n");
			ah_in_discards++;
			freemsg(ipsec_in);
			return;
		}
	}

	ar = (auth_req_t *)areq->b_rptr;

	phdr_mp = ah_process_ip_options_v4(mp, assoc, ar, B_FALSE);

	if (phdr_mp == NULL) {
		ah_in_discards++;
		freemsg(ipsec_in);
		freemsg(areq);
		IPSA_REFRELE(assoc);
		return;
	}


	/*
	 * auth_req_startoffset is filled in by ah_process_ip_options_v4.
	 * This tells the algorithm module where to start authenticating
	 * the datagram from.
	 */

	areq->b_datap->db_type = M_CTL;
	areq->b_wptr += sizeof (ipsec_info_t);
	ar->auth_req_type = AUTH_PI_IN_AUTH_REQ;
	ar->auth_req_len = sizeof (auth_req_t);
	ar->auth_req_assoc = assoc;

	areq->b_cont = ipsec_in;
	ipsec_in->b_cont = phdr_mp;
	phdr_mp->b_cont = mp;

	/*
	 * This goes up to the algorithm module. It comes back
	 * to ipsecah_wput -> ah_auth_in_done_v4().
	 */
	(void) putnext(ah_aalgs[assoc->ipsa_auth_alg]->ahs_rq, areq);
}

/*
 * auth_ack_mp is the mp which has the digest that needs to
 * be verified against the digest that is present in the
 * incoming datagram.
 */
static void
ah_auth_in_done_v4(queue_t *q, mblk_t *auth_ack_mp)
{
	auth_ack_t *ack;
	ipha_t *ipha;
	uint32_t ip_hdr_length;
	mblk_t *phdr_mp;
	mblk_t *mp;
	int len;
	mblk_t *ipsec_in;
	ah_t *ah;
	uint8_t *in_icv;
	ipha_t *nipha;
	uint32_t length;
	ipsa_t *assoc;
	ipsec_in_t *ii;

	ack = (auth_ack_t *)auth_ack_mp->b_rptr;
	len = ack->auth_ack_datalen;
	assoc = ack->auth_ack_assoc;

	ipsec_in = auth_ack_mp->b_cont;
	phdr_mp = ipsec_in->b_cont;
	mp = phdr_mp->b_cont;

	ipha = (ipha_t *)mp->b_rptr;
	ip_hdr_length = ipha->ipha_version_and_hdr_length -
	    (uint8_t)((IP_VERSION << 4));
	ip_hdr_length <<= 2;
	ah = (ah_t *)(mp->b_rptr + ip_hdr_length);
	in_icv = (uint8_t *)ah + sizeof (ah_t);

	if (bcmp(ack->auth_ack_data, in_icv, ack->auth_ack_datalen)) {
		/* Log the event. As of now we print out an event */
		ah_bad_auth++;
		ah_in_discards++;
		ipsec_rl_strlog(info.mi_idnum, 0, 0,
		    SL_ERROR | SL_CONSOLE | SL_WARN,
		    "AH Authentication failed spi %x, dst_addr %x",
		    ntohl(assoc->ipsa_spi),
		    *((uint32_t *)assoc->ipsa_dstaddr));
		freemsg(auth_ack_mp);
		IPSA_REFRELE(assoc);
		return;
	} else {
		ah3dbg(("AH succeeded, checking replay\n"));
		ah_good_auth++;

		if (!sadb_replay_check(assoc, ah->ah_replay)) {
			/*
			 * Log the event. As of now we print out an event.
			 * Do not print the replay failure number, or else
			 * syslog cannot collate the error messages.  Printing
			 * the replay number that failed opens a denial-of-
			 * service attack.
			 */
			ah_replay_failures++;
			ah_in_discards++;
			ipsec_rl_strlog(info.mi_idnum, 0, 0,
			    SL_ERROR | SL_CONSOLE | SL_WARN,
			    "Replay failed for AH spi %x, dst_addr %x",
			    ntohl(assoc->ipsa_spi),
			    ntohl(*((uint32_t *)assoc->ipsa_dstaddr)));
			freemsg(auth_ack_mp);
			IPSA_REFRELE(assoc);
			return;
		}

		/*
		 * We need to remove the AH header from the original
		 * datagram. Easy way to do this is to use phdr_mp
		 * to hold the IP header and the orginal mp to hold
		 * the rest of it. So, we copy the IP header on to
		 * phdr_mp, and set the b_rptr in mp past Ah header.
		 */
		bcopy(mp->b_rptr, phdr_mp->b_rptr, ip_hdr_length);
		phdr_mp->b_wptr = phdr_mp->b_rptr + ip_hdr_length;
		nipha = (ipha_t *)phdr_mp->b_rptr;
		/*
		 * Assign the right protocol, adjust the length as we
		 * are removing the AH header and adjust the checksum to
		 * account for the protocol and length.
		 */
		nipha->ipha_protocol = ah->ah_nexthdr;
		length = ntohs(nipha->ipha_length);
		if (!ah_age_bytes(assoc, length, B_TRUE)) {
			/* The ipsa has hit hard expiration, LOG and AUDIT. */
			/* XXX v4-specicific, fix for IPv6 */
			ipsec_rl_strlog(info.mi_idnum, 0, 0,
			    SL_ERROR | SL_WARN,
			    "AH Association 0x%x, dst %d.%d.%d.%d had bytes "
			    "expire.\n", ntohl(assoc->ipsa_spi),
			    assoc->ipsa_dstaddr[0], assoc->ipsa_dstaddr[1],
			    assoc->ipsa_dstaddr[2], assoc->ipsa_dstaddr[3]);
			ah_bytes_expired++;
			ah_in_discards++;
			freemsg(auth_ack_mp);
			IPSA_REFRELE(assoc);
			return;
		}
		len = (len + IPV4_PADDING_ALIGN - 1) & -IPV4_PADDING_ALIGN;
		length -= (sizeof (ah_t) + len);

		nipha->ipha_length = htons((uint16_t)length);
		nipha->ipha_hdr_checksum = 0;
		nipha->ipha_hdr_checksum = (uint16_t)ip_csum_hdr(nipha);
		/*
		 * Skip IP,AH and the authentication data in the
		 * original datagram.
		 */
		mp->b_rptr += (ip_hdr_length + sizeof (ah_t) + len);
		/*
		 * Used by IP to locate the association later while
		 * doing the policy checks.
		 */
		ii = (ipsec_in_t *)ipsec_in->b_rptr;
		ii->ipsec_in_ah_spi = ah->ah_spi;
		IPSA_REFRELE(assoc);
	}
	freeb(auth_ack_mp);
	putnext(q, ipsec_in);
}

/*
 * auth_ack_mp has the digest which has to be set appropriately
 * in the outbound datagram.
 */
static void
ah_auth_out_done_v4(queue_t *q, mblk_t *auth_ack_mp)
{
	auth_ack_t *ack;
	mblk_t *ipsec_out;
	mblk_t *phdr_mp;
	mblk_t *mp;
	int len;
	int align_len;
	uint32_t ip_hdr_length;
	uchar_t *ptr;
	ipha_t *ipha;
	ipha_t *nipha;
	ipsa_t *assoc;
	uint32_t length;

	ack = (auth_ack_t *)auth_ack_mp->b_rptr;
	len = ack->auth_ack_datalen;
	align_len = (len + IPV4_PADDING_ALIGN - 1) & -IPV4_PADDING_ALIGN;
	assoc = ack->auth_ack_assoc;

	ipsec_out = auth_ack_mp->b_cont;
	phdr_mp = ipsec_out->b_cont;
	mp = phdr_mp->b_cont;

	/*
	 * Used by IP to locate the association later while
	 * doing the policy checks.
	 */
	((ipsec_out_t *)(ipsec_out->b_rptr))->ipsec_out_ah_spi =
	    assoc->ipsa_spi;

	IPSA_REFRELE(ack->auth_ack_assoc);

	ipha = (ipha_t *)mp->b_rptr;
	ip_hdr_length = ipha->ipha_version_and_hdr_length -
	    (uint8_t)((IP_VERSION << 4));
	ip_hdr_length <<= 2;
	/*
	 * phdr_mp must have the right amount of space for the
	 * combined IP and AH header. Copy the IP header and
	 * the ack_data onto AH. Note that the AH header was
	 * already formed before the ICV calculation and hence
	 * you don't have to copy it here.
	 */
	bcopy(mp->b_rptr, phdr_mp->b_rptr, ip_hdr_length);

	ptr = phdr_mp->b_rptr + ip_hdr_length + sizeof (ah_t);
	bcopy(ack->auth_ack_data, ptr, len);

	/*
	 * Compute the new header checksum as we are assigning
	 * IPPROTO_AH and adjusting the length here.
	 */
	nipha = (ipha_t *)phdr_mp->b_rptr;

	nipha->ipha_protocol = IPPROTO_AH;
	length = ntohs(nipha->ipha_length);
	length += (sizeof (ah_t) + align_len);
	nipha->ipha_length = htons((uint16_t)length);
	nipha->ipha_hdr_checksum = 0;
	nipha->ipha_hdr_checksum = (uint16_t)ip_csum_hdr(nipha);

	/* Skip the original IP header */
	mp->b_rptr += ip_hdr_length;
	if (mp->b_rptr == mp->b_wptr) {
		phdr_mp->b_cont = mp->b_cont;
		freeb(mp);
	}

	freeb(auth_ack_mp);
	putnext(q, ipsec_out);
}


/*
 * Wrapper to get the right association. Used by IP during
 * policy checks.
 */
ipsa_t *
getahassoc(mblk_t *ipsec_mp, uint8_t *src_addr, uint8_t *dst_addr,
    int length)
{
	ipsec_info_t *inf;
	ipsa_t *target;

	ASSERT(ipsec_mp->b_datap->db_type == M_CTL);

	inf = (ipsec_info_t *)ipsec_mp->b_rptr;

	if (inf->ipsec_info_type == IPSEC_IN) {
		isaf_t *inbound;
		ipsec_in_t *ii = (ipsec_in_t *)inf;

		inbound = &ah_inbound_assoc_v4[
		    INBOUND_HASH(ii->ipsec_in_ah_spi)];
		mutex_enter(&inbound->isaf_lock);
		target = sadb_getassocbyspi(inbound, ii->ipsec_in_ah_spi,
		    src_addr, dst_addr, length);
		mutex_exit(&inbound->isaf_lock);

	} else if (inf->ipsec_info_type == IPSEC_OUT) {
		isaf_t *outbound;
		ipsec_out_t *oi = (ipsec_out_t *)inf;

		outbound = &ah_outbound_assoc_v4[
		    OUTBOUND_HASH(*(uint32_t *)(dst_addr))];
		mutex_enter(&outbound->isaf_lock);
		target = sadb_getassocbyspi(outbound, oi->ipsec_out_ah_spi,
		    src_addr, dst_addr, length);
		mutex_exit(&outbound->isaf_lock);

	} else {
		/* Wrong message type */
		ah0dbg(("getahassoc: Wrong message type\n"));
		return (NULL);
	}

	/*
	 * We could be returning a association which is dead.
	 * But, it was'nt dead while we used it to authenticate.
	 * It means that it has not yet been reaped and soon
	 * go away. The caller should do the IPSA_REFRELE when
	 * they are done using the association.
	 */
	return (target);
}
