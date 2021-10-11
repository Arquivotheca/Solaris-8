/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ipsecesp.c	1.9	99/12/06 SMI"

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
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/debug.h>
#include <sys/atomic.h>
#include <netinet/in.h>
#include <net/pfkeyv2.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <netinet/ip6.h>
#include <inet/ip.h>
#include <inet/ip_ire.h>
#include <inet/ipsecesp.h>
#include <inet/ipsec_info.h>
#include <inet/sadb.h>

#define	MAX_AALGS 256	/* Maximum number of authentication algorithms. */
#define	ESP_AGE_INTERVAL_DEFAULT 1000

static kmutex_t ipsecesp_param_lock; /* Protects ipsecesp_param_arr[] below. */
/*
 * Table of ND variables supported by ipsecesp. These are loaded into
 * ipsecesp_g_nd in ipsecesp_init_nd.
 * All of these are alterable, within the min/max values given, at run time.
 */
static	ipsecespparam_t	ipsecesp_param_arr[] = {
	/* min	max			value	name */
	{ 0,	3,			0,	"ipsecesp_debug"},
	{ 125,	32000, ESP_AGE_INTERVAL_DEFAULT, "ipsecesp_age_interval"},
	{ 1,	10,			1,	"ipsecesp_reap_delay"},
	{ 0,	MAX_AALGS,	4,	"ipsecesp_max_proposal_combinations"},
	{ 1,	SADB_MAX_REPLAY,	64,	"ipsecesp_replay_size"},
	{ 1,	300,			15,	"ipsecesp_acquire_timeout"},
	{ 1,	1800,			900,	"ipsecesp_larval_timeout"},
	/* Default lifetime values for ACQUIRE messages. */
	{ 0,	0xffffffffU,	0,	"ipsecesp_default_soft_bytes"},
	{ 0,	0xffffffffU,	0,	"ipsecesp_default_hard_bytes"},
	{ 0,	0xffffffffU,	24000,	"ipsecesp_default_soft_addtime"},
	{ 0,	0xffffffffU,	28800,	"ipsecesp_default_hard_addtime"},
	{ 0,	0xffffffffU,	0,	"ipsecesp_default_soft_usetime"},
	{ 0,	0xffffffffU,	0,	"ipsecesp_default_hard_usetime"},
};
#define	ipsecesp_debug		ipsecesp_param_arr[0].ipsecesp_param_value
#define	ipsecesp_age_interval	ipsecesp_param_arr[1].ipsecesp_param_value
#define	ipsecesp_age_int_max	ipsecesp_param_arr[1].ipsecesp_param_max
#define	ipsecesp_reap_delay	ipsecesp_param_arr[2].ipsecesp_param_value
#define	ipsecesp_max_combs	ipsecesp_param_arr[3].ipsecesp_param_value
#define	ipsecesp_replay_size	ipsecesp_param_arr[4].ipsecesp_param_value
#define	ipsecesp_acquire_timeout ipsecesp_param_arr[5].ipsecesp_param_value
#define	ipsecesp_larval_timeout ipsecesp_param_arr[6].ipsecesp_param_value
#define	ipsecesp_default_soft_bytes \
	ipsecesp_param_arr[7].ipsecesp_param_value
#define	ipsecesp_default_hard_bytes \
	ipsecesp_param_arr[8].ipsecesp_param_value
#define	ipsecesp_default_soft_addtime \
	ipsecesp_param_arr[9].ipsecesp_param_value
#define	ipsecesp_default_hard_addtime \
	ipsecesp_param_arr[10].ipsecesp_param_value
#define	ipsecesp_default_soft_usetime \
	ipsecesp_param_arr[11].ipsecesp_param_value
#define	ipsecesp_default_hard_usetime \
	ipsecesp_param_arr[12].ipsecesp_param_value

#define	esp0dbg(a)	printf a
/* NOTE:  != 0 instead of > 0 so lint doesn't complain. */
#define	esp1dbg(a)	if (ipsecesp_debug != 0) printf a
#define	esp2dbg(a)	if (ipsecesp_debug > 1) printf a
#define	esp3dbg(a)	if (ipsecesp_debug > 2) printf a

static IDP ipsecesp_g_nd;

static int ipsecesp_open(queue_t *, dev_t *, int, int, cred_t *);
static int ipsecesp_close(queue_t *);
static void ipsecesp_rput(queue_t *, mblk_t *);
static void ipsecesp_wput(queue_t *, mblk_t *);

static boolean_t esp_register_out(uint32_t, uint32_t, uint_t);
static void esp_inbound_done_v4(queue_t *q, mblk_t *);

static struct module_info info = {
	5137, "ipsecesp", 0, INFPSZ, 65536, 1024
};

static struct qinit rinit = {
	(pfi_t)ipsecesp_rput, NULL, ipsecesp_open, ipsecesp_close, NULL, &info,
	NULL
};

static struct qinit winit = {
	(pfi_t)ipsecesp_wput, NULL, ipsecesp_open, ipsecesp_close, NULL, &info,
	NULL
};

struct streamtab ipsecespinfo = {
	&rinit, &winit, NULL, NULL
};

/*
 * Keysock instance of ESP.  "There can be only one." :)
 * Use casptr() on this because I don't set it until KEYSOCK_HELLO comes down.
 * Paired up with the esp_pfkey_q is the esp_event, which will age SAs.
 */
static queue_t *esp_pfkey_q;
static timeout_id_t esp_event;

/*
 * OTOH, this one is set at open/close, and I'm D_MTQPAIR for now.
 *
 * Question:	Do I need this, given that all instance's esps->esps_wq point
 *		to IP?
 *
 * Answer:	Yes, because I need to know which queue is BOUND to
 *		IPPROTO_ESP
 */
static queue_t *esp_ip_q;
static mblk_t *esp_ip_unbind;

static kmutex_t esp_aalg_lock;	/* Protects esp_{aalgs,num_aalgs,sortlist} */
static espstate_t *esp_aalgs[MAX_AALGS];
static int esp_aalgs_sortlist[MAX_AALGS];
static uint_t esp_num_aalgs;	/* See esp_register_out() for why I'm here. */



/*
 * Keep outbound assocs about the same as ire_cache entries for now.
 * One danger point, multiple SAs for a single dest will clog a bucket.
 * For the future, consider two-level hashing (2nd hash on IPC?), then probe.
 */
#define	OUTBOUND_BUCKETS 256
static isaf_t esp_outbound_assoc_v4[OUTBOUND_BUCKETS];
static isaf_t esp_outbound_assoc_v6[OUTBOUND_BUCKETS];

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
static isaf_t esp_inbound_assoc_v4[INBOUND_BUCKETS];
static isaf_t esp_inbound_assoc_v6[INBOUND_BUCKETS];

/* May want to just take low 8 bits, assuming a halfway decent allocation. */
#define	INBOUND_HASH(spi) OUTBOUND_HASH(spi)

/* Hmmmm, we may want to hash these out as well! */
static isaf_t esp_larval_list_v4;
static isaf_t esp_larval_list_v6;

/* Acquire list. */

static iacqf_t esp_acquires_v4[OUTBOUND_BUCKETS];
static iacqf_t esp_acquires_v6[OUTBOUND_BUCKETS];

/* Global SPI value and acquire sequence number. */

static uint32_t esp_spi;
static uint32_t esp_acquire_seq;

/*
 * Stats.  This may eventually become a full-blown SNMP MIB.  But for now,
 * keep things plain.  Use uint32_ts for identical precision across 32 and 64
 * bit models.
 */

typedef uint32_t esp_counter;

static esp_counter esp_good_auth;
static esp_counter esp_bad_auth;
static esp_counter esp_replay_failures;
static esp_counter esp_replay_early_failures;
static esp_counter esp_lookup_failure;
static esp_counter esp_keysock_in;
static esp_counter esp_in_requests;
static esp_counter esp_out_requests;
static esp_counter esp_acquire_requests;
static esp_counter esp_bytes_expired;
static esp_counter esp_in_discards;
static esp_counter esp_out_discards;

/*
 * For generic zero-address comparison.  This should be as large as the
 * ipsa_*addr field, and initialized to all-zeroes.  Globals are, so there
 * is no initialization required.
 */
static uint8_t zeroes[16];

#ifdef DEBUG
/*
 * Debug routine, useful to see pre-encryption data.
 */
static char *
dump_msg(mblk_t *mp)
{
	while (mp != NULL) {
		unsigned char *ptr;

		printf("mblk address 0x%p, length %ld, db_ref %d\n",
		    (void *) mp, (long)(mp->b_wptr - mp->b_rptr),
		    mp->b_datap->db_ref);
		printf("type %d, base 0x%p, lim 0x%p", mp->b_datap->db_type,
		    (void *)mp->b_datap->db_base, (void *)mp->b_datap->db_lim);
		ptr = mp->b_rptr;
		while (ptr < mp->b_wptr) {
			uint_t diff;

			diff = (ptr - mp->b_rptr);
			if (!(diff & 0x1f))
				printf("\nbytes: ");
			if (!(diff & 0x3))
				printf(" ");
			printf("%02x", *ptr);
			ptr++;
		}
		printf("\n");

		mp = mp->b_cont;
	}

	return ("\n");
}

#else /* DEBUG */
static char *
dump_msg(mblk_t *mp)
{
	printf("Find value of mp %p.\n", mp);
	return ("\n");
}
#endif /* DEBUG */

/*
 * Don't have to lock age_interval, as only one thread will access it at
 * a time, because I control the one function that does with timeout().
 */
/* ARGSUSED */
static void
esp_ager(void *ignoreme)
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
		acqlist = &(esp_acquires_v4[i]);
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
	mutex_enter(&esp_larval_list_v4.isaf_lock);
	for (assoc = esp_larval_list_v4.isaf_ipsa; assoc != NULL;
	    assoc = spare) {
		/* Assign spare in case I get deleted. */
		spare = assoc->ipsa_next;
		sadb_age_larval(assoc, current);
	}
	mutex_exit(&esp_larval_list_v4.isaf_lock);
	/* XXX IPv6 */

	/* Age inbound associations. */
	for (i = 0; i < INBOUND_BUCKETS; i++) {
		/* Do IPv4... */
		bucket = &(esp_inbound_assoc_v4[i]);
		mutex_enter(&bucket->isaf_lock);
		for (assoc = bucket->isaf_ipsa; assoc != NULL;
		    assoc = spare) {
			/*
			 * Assign spare in case the current assoc gets deleted.
			 */
			spare = assoc->ipsa_next;
			if (sadb_age_assoc(esp_pfkey_q, assoc, current,
			    ipsecesp_reap_delay) != NULL) {
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
					esp1dbg(("Can't allocate "
					    "inbound haspeer.\n"));
					continue;	/* for loop... */
				}
				newbie->next = haspeerlist;
				newbie->ipsa = assoc;
				haspeerlist = newbie;
			}
		}
		mutex_exit(&bucket->isaf_lock);

		/* Then IPv6.  XXX IPv6. */
	}

	/* Mark up those pesky haspeer cases.  (Don't forget IPv6.) */
	while (haspeerlist != NULL) {
		esp3dbg(("haspeerlist after inbound 0x%p.\n",
		    (void *)haspeerlist));
		spare = haspeerlist->ipsa;
		newbie = haspeerlist;
		haspeerlist = newbie->next;
		kmem_free(newbie, sizeof (*newbie));
		bucket = &(esp_outbound_assoc_v4[
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
		bucket = &(esp_outbound_assoc_v4[i]);
		mutex_enter(&bucket->isaf_lock);
		for (assoc = bucket->isaf_ipsa; assoc != NULL;
		    assoc = spare) {
			/*
			 * Assign spare in case the current assoc gets deleted.
			 */
			spare = assoc->ipsa_next;
			if (sadb_age_assoc(esp_pfkey_q, assoc, current,
			    ipsecesp_reap_delay) != NULL) {
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
					esp1dbg(("Can't allocate "
					    "outbound haspeer.\n"));
					continue;	/* for loop... */
				}
				newbie->next = haspeerlist;
				newbie->ipsa = assoc;
				haspeerlist = newbie;
			}
		}
		mutex_exit(&bucket->isaf_lock);

		/* Then IPv6.  XXX IPv6. */
	}

	/* Clean up those pesky haspeer cases.  (Don't forget IPv6.) */
	while (haspeerlist != NULL) {
		esp3dbg(("haspeerlist after outbound 0x%p.\n",
		    (void *)haspeerlist));
		spare = haspeerlist->ipsa;
		newbie = haspeerlist;
		haspeerlist = newbie->next;
		kmem_free(newbie, sizeof (*newbie));
		bucket = &(esp_inbound_assoc_v4[INBOUND_HASH(spare->ipsa_spi)]);
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
	if ((gethrtime() - begin) > ipsecesp_age_interval * 1000000) {
		esp1dbg(("esp_ager() taking longer than %u msec, doubling.\n",
		    ipsecesp_age_interval));
		if (ipsecesp_age_interval > ipsecesp_age_int_max) {
			/* XXX Rate limit this?  Or recommend flush? */
			(void) strlog(info.mi_idnum, 0, 0, SL_ERROR | SL_WARN,
			    "Too many ESP associations to age out in "
			    "%d msec.\n", ipsecesp_age_int_max);
		} else {
			/* Double by shifting by one bit. */
			ipsecesp_age_interval <<= 1;
			ipsecesp_age_interval = max(ipsecesp_age_interval,
			    ipsecesp_age_int_max);
		}
	} else if ((gethrtime() - begin) <= ipsecesp_age_interval * 500000 &&
		ipsecesp_age_interval > ESP_AGE_INTERVAL_DEFAULT) {
		/*
		 * If I took less than half of the interval, then I should
		 * ratchet the interval back down.  Never automatically
		 * shift below the default aging interval.
		 *
		 * NOTE:This even overrides manual setting of the age
		 *	interval using NDD.
		 */
		/* Halve by shifting one bit. */
		ipsecesp_age_interval >>= 1;
		ipsecesp_age_interval = max(ipsecesp_age_interval,
		    ESP_AGE_INTERVAL_DEFAULT);
	}

	esp_event = qtimeout(esp_pfkey_q, esp_ager, NULL,
	    ipsecesp_age_interval * drv_usectohz(1000));
}

/*
 * Get an ESP NDD parameter.
 */
/* ARGSUSED */
static int
ipsecesp_param_get(q, mp, cp)
	queue_t	*q;
	mblk_t	*mp;
	caddr_t	cp;
{
	ipsecespparam_t	*ipsecesppa = (ipsecespparam_t *)cp;

	mutex_enter(&ipsecesp_param_lock);
	(void) mi_mpprintf(mp, "%u", ipsecesppa->ipsecesp_param_value);
	mutex_exit(&ipsecesp_param_lock);
	return (0);
}

/*
 * This routine sets an NDD variable in a ipsecespparam_t structure.
 */
/* ARGSUSED */
static int
ipsecesp_param_set(q, mp, value, cp)
	queue_t	*q;
	mblk_t	*mp;
	char	*value;
	caddr_t	cp;
{
	char	*end;
	uint_t	new_value;
	ipsecespparam_t	*ipsecesppa = (ipsecespparam_t *)cp;

	/* Convert the value from a string into a long integer. */
	new_value = (uint_t)mi_strtol(value, &end, 10);
	/*
	 * Fail the request if the new value does not lie within the
	 * required bounds.
	 */
	if (end == value ||
	    new_value < ipsecesppa->ipsecesp_param_min ||
	    new_value > ipsecesppa->ipsecesp_param_max)
		return (EINVAL);

	/* Set the new value */
	mutex_enter(&ipsecesp_param_lock);
	ipsecesppa->ipsecesp_param_value = new_value;
	mutex_exit(&ipsecesp_param_lock);
	return (0);
}

/*
 * Report ESP status.  Until we have a MIB, it's the best way to go.
 */
/* ARGSUSED */
static int
ipsecesp_status(queue_t *q, mblk_t *mp, void *arg)
{
	(void) mi_mpprintf(mp, "ESP status");
	(void) mi_mpprintf(mp, "----------");
	(void) mi_mpprintf(mp,
	    "Authentication algorithms           =\t%u", esp_num_aalgs);
	(void) mi_mpprintf(mp,
	    "Packets passing authentication      =\t%u", esp_good_auth);
	(void) mi_mpprintf(mp,
	    "Packets failing authentication      =\t%u", esp_bad_auth);
	(void) mi_mpprintf(mp,
	    "Packets failing replay checks       =\t%u", esp_replay_failures);
	(void) mi_mpprintf(mp,
	    "Packets failing early replay checks =\t%u",
	    esp_replay_early_failures);
	(void) mi_mpprintf(mp,
	    "Failed inbound SA lookups           =\t%u", esp_lookup_failure);
	(void) mi_mpprintf(mp,
	    "Inbound PF_KEY messages             =\t%d", esp_keysock_in);
	(void) mi_mpprintf(mp,
	    "Inbound ESP packets                 =\t%d", esp_in_requests);
	(void) mi_mpprintf(mp,
	    "Outbound ESP requests               =\t%d", esp_out_requests);
	(void) mi_mpprintf(mp,
	    "PF_KEY ACQUIRE messages             =\t%d", esp_acquire_requests);
	(void) mi_mpprintf(mp,
	    "Expired associations (# of bytes)   =\t%d", esp_bytes_expired);
	(void) mi_mpprintf(mp,
	    "Discarded inbound packets           =\t%d", esp_in_discards);
	(void) mi_mpprintf(mp,
	    "Discarded outbound packets          =\t%d", esp_out_discards);

	return (0);
}

/*
 * Initialize things for ESP at module load time.
 */
boolean_t
ipsecesp_ddi_init(void)
{
	int count;
	ipsecespparam_t *espp = ipsecesp_param_arr;
	time_t current;

	for (count = A_CNT(ipsecesp_param_arr); count-- > 0; espp++) {
		if (espp->ipsecesp_param_name != NULL &&
		    espp->ipsecesp_param_name[0]) {
			if (!nd_load(&ipsecesp_g_nd, espp->ipsecesp_param_name,
			    ipsecesp_param_get, ipsecesp_param_set,
			    (caddr_t)espp)) {
				nd_free(&ipsecesp_g_nd);
				return (B_FALSE);
			}
		}
	}


	if (!nd_load(&ipsecesp_g_nd, "ipsecesp_status", ipsecesp_status, NULL,
	    NULL)) {
		nd_free(&ipsecesp_g_nd);
		return (B_FALSE);
	}

	sadb_init(esp_outbound_assoc_v4, OUTBOUND_BUCKETS);
	sadb_init(esp_inbound_assoc_v4, INBOUND_BUCKETS);
	sadb_init(esp_outbound_assoc_v6, OUTBOUND_BUCKETS);
	sadb_init(esp_inbound_assoc_v6, INBOUND_BUCKETS);
	sadb_init(&esp_larval_list_v4, 1);
	sadb_init(&esp_larval_list_v6, 1);

	sadb_init((isaf_t *)esp_acquires_v4, OUTBOUND_BUCKETS);
	sadb_init((isaf_t *)esp_acquires_v6, OUTBOUND_BUCKETS);

	mutex_init(&esp_aalg_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&ipsecesp_param_lock, NULL, MUTEX_DEFAULT, 0);

	/*
	 * esp_spi will be another atomic_32 sort of variable.
	 *
	 * Initialize this as randomly as possible.
	 */
	sadb_get_random_bytes(&esp_spi, sizeof (uint32_t));
	(void) drv_getparm(TIME, &current);
	esp_spi ^= (uint32_t)current;
	esp_spi ^= (uint32_t)gethrtime();

	/* Set acquire sequence number to maximum possible. */
	esp_acquire_seq = (uint32_t)-1;

	return (B_TRUE);
}

/*
 * Destroy things for ESP at module unload time.
 */
void
ipsecesp_ddi_destroy(void)
{
	esp1dbg(("In ddi_destroy.\n"));


	sadb_destroy(esp_outbound_assoc_v4, OUTBOUND_BUCKETS);
	sadb_destroy(esp_inbound_assoc_v4, INBOUND_BUCKETS);
	sadb_destroy(esp_outbound_assoc_v6, OUTBOUND_BUCKETS);
	sadb_destroy(esp_inbound_assoc_v6, INBOUND_BUCKETS);
	sadb_destroy(&esp_larval_list_v4, 1);
	sadb_destroy(&esp_larval_list_v6, 1);

	/* For each acquire, destroy it, including the bucket mutex. */
	sadb_destroy_acqlist(esp_acquires_v4, OUTBOUND_BUCKETS, B_TRUE);
	sadb_destroy_acqlist(esp_acquires_v6, OUTBOUND_BUCKETS, B_TRUE);

	mutex_destroy(&ipsecesp_param_lock);
	mutex_destroy(&esp_aalg_lock);

	nd_free(&ipsecesp_g_nd);
}

/*
 * ESP module open routine.
 */
/* ARGSUSED */
static int
ipsecesp_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	espstate_t *esps;

	if (q->q_ptr != NULL)
		return (0);  /* Re-open of an already open instance. */

	if (sflag != MODOPEN)
		return (EINVAL);

	/*
	 * Allocate ESP state.  I dunno if keysock or an algorithm is
	 * on top of me, but it'll be one or the other.
	 */

	/* Note the lack of zalloc, with so few fields, it makes sense. */
	esps = kmem_alloc(sizeof (espstate_t), KM_NOSLEEP);
	if (esps == NULL)
		return (ENOMEM);
	esps->esps_rq = q;
	esps->esps_wq = WR(q);
	esps->esps_id = 0;

	/*
	 * ASSUMPTIONS (because I'm MT_OCEXCL):
	 *
	 *	* I'm being pushed on top of IP for all my opens (incl. #1).
	 *	* Only ipsecesp_open() can write into the variable esp_ip_q.
	 *	* Because of this, I can check lazily for esp_ip_q.
	 *
	 *  If these assumptions are wrong, I'm in BIG trouble...
	 */

	q->q_ptr = esps;
	WR(q)->q_ptr = esps;

	qprocson(q);

	if (esp_ip_q == NULL) {
		struct T_unbind_req *tur;

		esp_ip_q = WR(q);
		/* Allocate an unbind... */
		esp_ip_unbind = allocb(sizeof (struct T_unbind_req), BPRI_HI);

		/*
		 * Send down T_BIND_REQ to bind IPPROTO_ESP.
		 * Handle the ACK here in ESP.
		 */
		if (esp_ip_unbind == NULL ||
		    !sadb_t_bind_req(esp_ip_q, IPPROTO_ESP)) {
			kmem_free(q->q_ptr, sizeof (espstate_t));
			return (ENOMEM);
		}

		tur = (struct T_unbind_req *)esp_ip_unbind->b_rptr;
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
 * Sort algorithm lists.
 *
 * XXX For now, sort on minimum key size (GAG!).  While minimum key size is
 * not the ideal metric, it's the only quantifiable measure available in the
 * AUTH PI.  We need a better metric for sorting algorithms by preference.
 */
static void
esp_insert_sortlist(espstate_t *esps, espstate_t **fanout, int *sortlist,
    uint_t count)
{
	int holder = esps->esps_id, swap;
	uint_t i;

	for (i = 0; i < count - 1; i++) {
		/*
		 * If you want to give precedence to newly added algs,
		 * add the = in the > comparison.
		 */
		if (holder != esps->esps_id ||
		    esps->esps_minbits > fanout[sortlist[i]]->esps_minbits) {
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
esp_remove_sortlist(int algid, int *sortlist, int newcount)
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
 * ESP module close routine.
 */
static int
ipsecesp_close(queue_t *q)
{
	espstate_t *esps = (espstate_t *)q->q_ptr;
	int i;

	/*
	 * Clean up q_ptr, if needed.
	 */
	qprocsoff(q);

	/* Keysock queue check is safe, because of OCEXCL perimeter. */
	if (q == esp_pfkey_q) {
		esp0dbg(("ipsecesp_close:  Ummm... keysock is closing ESP.\n"));
		esp_pfkey_q = NULL;
		/* Detach qtimeouts. */
		(void) quntimeout(q, esp_event);
	} else {
		/* Make sure about other stuff. */
		if (esps->esps_id != 0) {
				mutex_enter(&esp_aalg_lock);
				ASSERT(esp_aalgs[esps->esps_id] == esps);
				esp_num_aalgs--;
				esp_aalgs[esps->esps_id] = NULL;
				esp_remove_sortlist(esps->esps_id,
				    esp_aalgs_sortlist, esp_num_aalgs);
				mutex_exit(&esp_aalg_lock);
				/*
				 * Any remaining SAs with this algorithm
				 * will fail gracefully when they are used.
				 */
			/* I've unloaded, please notify. */
			(void) esp_register_out(0, 0, 0);
		}
	}

	if (WR(q) == esp_ip_q) {
		/*
		 * If the esp_ip_q is attached to this instance, find
		 * another.  The OCEXCL outer perimeter helps us here.
		 */
		esp_ip_q = NULL;

		/*
		 * First, send a T_UNBIND_REQ to IP for this instance...
		 */
		if (esp_ip_unbind == NULL) {
			putnext(WR(q), esp_ip_unbind);
			/* putnext() will consume the mblk. */
			esp_ip_unbind = NULL;
		}

		/*
		 * ...then find a replacement queue for esp_ip_q.
		 */
		if (esp_pfkey_q != NULL && esp_pfkey_q != RD(q)) {
			/*
			 * See if we can use the pfkey_q.
			 */
			esp_ip_q = WR(esp_pfkey_q);
		} else {
			/*
			 * Use one of the algorithms.
			 */
			for (i = 0; i < MAX_AALGS; i++) {
				if (esp_aalgs[i] != NULL) {
					esp_ip_q = esp_aalgs[i]->esps_wq;
					break;  /* Out of for loop. */
				}
			}
		}
		if (esp_ip_q == NULL ||
		    !sadb_t_bind_req(esp_ip_q, IPPROTO_ESP)) {
			esp1dbg(("ipsecesp: Can't reassign esp_ip_q.\n"));
			esp_ip_q = NULL;
		} else {
			esp_ip_unbind = allocb(sizeof (struct T_unbind_req),
			    BPRI_HI);
			/* If it's NULL, I can't do much here. */
		}
	}

	kmem_free(esps, sizeof (espstate_t));

	return (0);
}

/*
 * Add a number of bytes to what the SA has protected so far.  Return
 * B_TRUE if the SA can still protect that many bytes.
 *
 * Caller must REFRELE the passed-in assoc.  This function must REFRELE
 * any obtained peer SA.
 */
static boolean_t
esp_age_bytes(ipsa_t *assoc, uint64_t bytes, boolean_t inbound)
{
	ipsa_t *inassoc, *outassoc;
	isaf_t *bucket;
	boolean_t inrc, outrc;

	/* No peer?  No problem! */
	if (!assoc->ipsa_haspeer) {
		return (sadb_age_bytes(esp_pfkey_q, assoc, bytes,
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
		/* XXX IPv6 : we need IPv6 code here. */
		bucket = &esp_outbound_assoc_v4[
		    OUTBOUND_HASH(*((ipaddr_t *)&inassoc->ipsa_dstaddr))];
		mutex_enter(&bucket->isaf_lock);
		outassoc = sadb_getassocbyspi(bucket, inassoc->ipsa_spi,
		    inassoc->ipsa_srcaddr, inassoc->ipsa_dstaddr,
		    inassoc->ipsa_addrlen);
		mutex_exit(&bucket->isaf_lock);
		if (outassoc == NULL) {
			/* Q: Do we wish to set haspeer == B_FALSE? */
			esp0dbg(("esp_age_bytes: "
			    "can't find peer for inbound.\n"));
			return (sadb_age_bytes(esp_pfkey_q, inassoc,
			    bytes, B_TRUE));
		}
	} else {
		outassoc = assoc;
		bucket = &esp_inbound_assoc_v4[
		    INBOUND_HASH(outassoc->ipsa_spi)];
		mutex_enter(&bucket->isaf_lock);
		inassoc = sadb_getassocbyspi(bucket, outassoc->ipsa_spi,
		    outassoc->ipsa_srcaddr, outassoc->ipsa_dstaddr,
		    outassoc->ipsa_addrlen);
		mutex_exit(&bucket->isaf_lock);
		if (inassoc == NULL) {
			/* Q: Do we wish to set haspeer == B_FALSE? */
			esp0dbg(("esp_age_bytes: "
			    "can't find peer for outbound.\n"));
			return (sadb_age_bytes(esp_pfkey_q, outassoc,
			    bytes, B_TRUE));
		}
	}

	inrc = sadb_age_bytes(esp_pfkey_q, inassoc, bytes, B_TRUE);
	outrc = sadb_age_bytes(esp_pfkey_q, outassoc, bytes, B_FALSE);

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
 * Done with authentication algorithm.  Verify the ICV.
 */
static void
esp_authin_done_v4(queue_t *q, mblk_t *req_mp)
{
	auth_req_t *ar;
	auth_ack_t *aa;
	ipsa_t *assoc;
	esph_t *esph;
	uint_t espstart;
	mblk_t *ipsec_in_mp, *pseudo_hdr_mp, *data_mp;

	ASSERT(req_mp->b_datap->db_type == M_CTL);
	ar = (auth_req_t *)req_mp->b_rptr;
	ipsec_in_mp = req_mp->b_cont;

	if (ar->auth_req_type == AUTH_PI_IN_AUTH_ACK) {
		/*
		 * Check authentication residue and replay.  Strip out
		 * AUTH PI's pseudo-header.
		 */
		aa = (auth_ack_t *)ar;
		espstart = aa->auth_ack_preahlen;
		assoc = aa->auth_ack_assoc;
		pseudo_hdr_mp = ipsec_in_mp->b_cont;
		data_mp = pseudo_hdr_mp->b_cont;
		esph = (esph_t *)(data_mp->b_rptr + espstart);
		if (bcmp(pseudo_hdr_mp->b_rptr, aa->auth_ack_data,
		    aa->auth_ack_datalen)) {
			/* This is a loggable error!  AUDIT! */
			esp_bad_auth++;
			esp_in_discards++;
			ipsec_rl_strlog(info.mi_idnum, 0, 0,
			    SL_ERROR | SL_CONSOLE | SL_WARN,
			    "ESP Authentication failed for spi 0x%x, dst %x.\n",
			    htonl(assoc->ipsa_spi),
			    *((uint32_t *)assoc->ipsa_dstaddr));
			IPSA_REFRELE(assoc);
			freemsg(req_mp);
			return;
		}

		/*
		 * We passed authentication!  Check replay window here!
		 * For right now, assume keysock will set the replay window
		 * size to zero for SAs that have an unspecified sender.
		 * This may change...
		 */
		esp_good_auth++;

		if (!sadb_replay_check(assoc, esph->esph_replay)) {
			/*
			 * Log the event. As of now we print out an event.
			 * Do not print the replay failure number, or else
			 * syslog cannot collate the error messages.  Printing
			 * the replay number that failed opens a denial-of-
			 * service attack.
			 */
			esp_replay_failures++;
			esp_in_discards++;
			ipsec_rl_strlog(info.mi_idnum, 0, 0,
			    SL_ERROR | SL_CONSOLE | SL_WARN,
			    "Replay failed for ESP spi 0x%x, dst %x.\n",
			    htonl(assoc->ipsa_spi),
			    *((uint32_t *)assoc->ipsa_dstaddr));
			IPSA_REFRELE(assoc);
			freemsg(req_mp);
			return;
		}

		ipsec_in_mp->b_cont = data_mp;
		freeb(pseudo_hdr_mp);
	} else {
		/* Auth-less ESP. */
		ASSERT(ar->auth_req_type == AUTH_PI_IN_AUTH_REQ);
		assoc = ar->auth_req_assoc;
		espstart = ar->auth_req_startoffset;
		data_mp = ipsec_in_mp->b_cont;
		esph = (esph_t *)(data_mp->b_rptr + espstart);
	}

		/*
		 * Update association byte-count lifetimes.
		 */
		if (!esp_age_bytes(assoc,
		    msgdsize(data_mp) - espstart - sizeof (esph_t), B_TRUE)) {
			/* The ipsa has hit hard expiration, LOG and AUDIT. */
			/* XXX v4-specific, fix for IPv6 */
			ipsec_rl_strlog(info.mi_idnum, 0, 0,
			    SL_ERROR | SL_WARN,
			    "ESP association 0x%x, dst %d.%d.%d.%d had bytes "
			    "expire.\n", ntohl(assoc->ipsa_spi),
			    assoc->ipsa_dstaddr[0], assoc->ipsa_dstaddr[1],
			    assoc->ipsa_dstaddr[2], assoc->ipsa_dstaddr[3]);
			esp_bytes_expired++;
			esp_in_discards++;
			IPSA_REFRELE(assoc);
			freemsg(req_mp);	/* For inbound, this is good. */
			return;
		}

		esph->esph_spi = 0;
		esp_inbound_done_v4(q, req_mp);
}

/*
 * Strip ESP and rock & roll.
 */
static void
esp_inbound_done_v4(queue_t *q, mblk_t *ack_mp)
{
	mblk_t *ipsec_in_mp, *data_mp, *scratch;
	ipha_t *ipha;
	uint8_t nexthdr, padlen;
	uint32_t ivlen = 0;
	auth_ack_t *aa;
	ipsa_t *assoc;
	ipsec_in_t *ii;
	uint_t divpoint;


	ipsec_in_mp = ack_mp->b_cont;
	data_mp = ipsec_in_mp->b_cont;


	aa = (auth_ack_t *)ack_mp->b_rptr;
	assoc = aa->auth_ack_assoc;

	ii = (ipsec_in_t *)ipsec_in_mp->b_rptr;
	ii->ipsec_in_esp_spi = assoc->ipsa_spi;
	IPSA_REFRELE(assoc);

	/*
	 * Strip ESP data and fix IP header.
	 *
	 * XXX In case the beginning of esp_inbound_v4() changes to not do a
	 * pullup, this part of the code can remain unchanged.
	 */
	ASSERT((data_mp->b_wptr - data_mp->b_rptr) >= sizeof (ipha_t));
	ipha = (ipha_t *)data_mp->b_rptr;
	ASSERT((data_mp->b_wptr - data_mp->b_rptr) >= sizeof (esph_t) +
	    IPH_HDR_LENGTH(ipha));
	divpoint = IPH_HDR_LENGTH(ipha);
	scratch = data_mp;
	while (scratch->b_cont != NULL)
		scratch = scratch->b_cont;
	ASSERT((scratch->b_wptr - scratch->b_rptr) >= 2);

	/*
	 * "Next header" and padding length are the last two bytes in the
	 * ESP-protected datagram, thus the explicit - 1 and - 2.
	 */
	nexthdr = *(scratch->b_wptr - 1);
	padlen = *(scratch->b_wptr - 2);

	/* Fix part of the IP header. */
	ipha->ipha_protocol = nexthdr;


	/*
	 * Reality check the padlen.  The explicit - 2 is for the padding
	 * length and the next-header bytes.
	 */
	if (padlen >= ntohs(ipha->ipha_length) - sizeof (ipha_t) - 2 -
	    sizeof (esph_t) - ivlen) {
		esp_in_discards++;
		ipsec_rl_strlog(info.mi_idnum, 0, 0, SL_ERROR | SL_WARN,
		    "Possibly corrupt ESP packet.");
		esp1dbg(("padlen (%d) is greater than:\n", padlen));
		esp1dbg(("pkt len(%d) - ip hdr - esp hdr - ivlen(%d) = %d.\n",
		    ntohs(ipha->ipha_length), ivlen,
		    (int)(ntohs(ipha->ipha_length) - sizeof (ipha_t) - 2 -
		    sizeof (esph_t) - ivlen)));
		freemsg(ack_mp);
		return;
	}

	/*
	 * Fix the rest of the header.  The explicit - 2 is for the padding
	 * length and the next-header bytes.
	 */
	ipha->ipha_length = htons(ntohs(ipha->ipha_length) - padlen - 2 -
	    sizeof (esph_t) - ivlen);
	ipha->ipha_hdr_checksum = 0;
	ipha->ipha_hdr_checksum = (uint16_t)ip_csum_hdr(ipha);

	/*
	 * XXX The ESP spec says check the padding.
	 * Do that here if it really needs to get done.
	 */

	/* Trim off the padding. */
	ASSERT(data_mp->b_cont == NULL);
	data_mp->b_wptr -= (padlen + 2);

	/*
	 * Remove the ESP header.
	 *
	 * The above assertions about data_mp's size will make this work.
	 *
	 * XXX  Question:  If I send up and get back a contiguous mblk,
	 * would it be quicker to bcopy over, or keep doing the dupb stuff?
	 * I go with copying for now.
	 */

	if (IS_P2ALIGNED(data_mp->b_rptr, sizeof (uint32_t)) &&
	    IS_P2ALIGNED(ivlen, sizeof (uint32_t))) {
		uint32_t *src, *dst;

		src = (uint32_t *)(data_mp->b_rptr + divpoint);
		dst = (uint32_t *)(data_mp->b_rptr + divpoint +
		    sizeof (esph_t) + ivlen);

		ASSERT(IS_P2ALIGNED(dst, sizeof (uint32_t)) &&
		    IS_P2ALIGNED(src, sizeof (uint32_t)));

		do {
			src--;
			dst--;
			*dst = *src;
		} while (src != (uint32_t *)data_mp->b_rptr);

		data_mp->b_rptr = (uchar_t *)dst;
	} else {
		uint8_t *src, *dst;

		src = data_mp->b_rptr + divpoint;
		dst = data_mp->b_rptr + divpoint + sizeof (esph_t) + ivlen;

		do {
			src--;
			dst--;
			*dst = *src;
		} while (src != data_mp->b_rptr);

		data_mp->b_rptr = dst;
	}

	esp2dbg(("data_mp after inbound ESP adjustment:\n"));
	esp2dbg((dump_msg(data_mp)));

	freeb(ack_mp);
	/*
	 * Deliver to IP.  Use write side of current queue, because
	 * esp_ip_q may be NULL, and ESP should ALWAYS be auto-pushed on
	 * top of IP.
	 */
	putnext(WR(q), ipsec_in_mp);
}

/*
 * Updating use times can be tricky business if the ipsa_haspeer flag is
 * set.  This function is called once in an SA's lifetime.
 *
 * Caller has to REFRELE "assoc" which is passed in.  This function has
 * to REFRELE any peer SA that is obtained.
 */
static void
esp_set_usetime(ipsa_t *assoc, boolean_t inbound)
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
		/* XXX IPv6 : We need IPv6 code here. */
		bucket = &esp_outbound_assoc_v4[
		    OUTBOUND_HASH(*((ipaddr_t *)&inassoc->ipsa_dstaddr))];
		mutex_enter(&bucket->isaf_lock);
		outassoc = sadb_getassocbyspi(bucket, inassoc->ipsa_spi,
		    inassoc->ipsa_srcaddr, inassoc->ipsa_dstaddr,
		    inassoc->ipsa_addrlen);
		mutex_exit(&bucket->isaf_lock);
		if (outassoc == NULL) {
			/* Q: Do we wish to set haspeer == B_FALSE? */
			esp0dbg(("esp_set_usetime: "
			    "can't find peer for inbound.\n"));
			sadb_set_usetime(inassoc);
			return;
		}
	} else {
		outassoc = assoc;
		bucket = &esp_inbound_assoc_v4[
		    INBOUND_HASH(outassoc->ipsa_spi)];
		mutex_enter(&bucket->isaf_lock);
		inassoc = sadb_getassocbyspi(bucket, outassoc->ipsa_spi,
		    outassoc->ipsa_srcaddr, outassoc->ipsa_dstaddr,
		    outassoc->ipsa_addrlen);
		mutex_exit(&bucket->isaf_lock);
		if (inassoc == NULL) {
			/* Q: Do we wish to set haspeer == B_FALSE? */
			esp0dbg(("esp_set_usetime: "
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
 * Handle ESP inbound data for IPv4.
 */
static void
esp_inbound_v4(mblk_t *ipsec_in_mp)
{
	mblk_t *data_mp, *req_mp, *pseudo_hdr_mp, *placeholder;
	ipsec_in_t *ii;
	auth_req_t *ar;		/* Do auth first for inbound. */
	ipha_t *ipha;
	esph_t *esph;
	ipsa_t *ipsa;
	isaf_t *bucket;
	uint16_t authlen;
	uint_t preamble;

	esp_in_requests++;

	ASSERT(ipsec_in_mp->b_datap->db_type == M_CTL);

	/* We have IPSEC_IN already! */
	ii = (ipsec_in_t *)ipsec_in_mp->b_rptr;
	data_mp = ipsec_in_mp->b_cont;

	/* XXX IP should've done this for me already. */
	ASSERT(data_mp->b_datap->db_ref == 1);

	ipsec_in_mp->b_cont = NULL;  /* Because of possible AUTH_PI request. */
	ASSERT(ii->ipsec_in_type == IPSEC_IN);

	ipha = (ipha_t *)data_mp->b_rptr;

	/*
	 * Put all data into one mblk if it's not there already.
	 * XXX This is probably bad long-term.  Figure out better ways of doing
	 * this.  Much of the inbound path depends on all of the data being
	 * in one mblk.
	 */
	if ((data_mp->b_wptr - data_mp->b_rptr) < ntohs(ipha->ipha_length) ||
	    data_mp->b_datap->db_ref > 1) {
		placeholder = msgpullup(data_mp, -1);
		freemsg(data_mp);
		data_mp = placeholder;
		if (data_mp == NULL) {
			/* XXX STATS log dropping of message. */
			freeb(ipsec_in_mp);
			return;
		}
	}

	ipha = (ipha_t *)data_mp->b_rptr;
	preamble = IPH_HDR_LENGTH(ipha);
	esph = (esph_t *)(data_mp->b_rptr + preamble);

	bucket = &esp_inbound_assoc_v4[INBOUND_HASH(esph->esph_spi)];
	mutex_enter(&bucket->isaf_lock);
	ipsa = sadb_getassocbyspi(bucket, esph->esph_spi,
	    (uint8_t *)&ipha->ipha_src, (uint8_t *)&ipha->ipha_dst,
	    IP_ADDR_LEN);
	mutex_exit(&bucket->isaf_lock);

	if (ipsa == NULL || ipsa->ipsa_state == IPSA_STATE_DEAD) {
		/*  This is a loggable error!  AUDIT ME! */
		esp_lookup_failure++;
		esp_in_discards++;
		ipsec_rl_strlog(info.mi_idnum, 0, 0,
		    SL_ERROR | SL_CONSOLE | SL_WARN,
		    "ESP:  No association found for spi 0x%x, dst %x\n",
		    htonl(esph->esph_spi), htonl((uint32_t)ipha->ipha_dst));
		if (ipsa != NULL) {
			IPSA_REFRELE(ipsa);
		}
		freeb(ipsec_in_mp);
		freemsg(data_mp);
		return;
	}

	if (ipsa->ipsa_usetime == 0)
		esp_set_usetime(ipsa, B_TRUE);

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
	if (!sadb_replay_peek(ipsa, esph->esph_replay)) {
		esp_replay_early_failures++;
		esp_in_discards++;
		freeb(ipsec_in_mp);
		freemsg(data_mp);
		IPSA_REFRELE(ipsa);
		return;
	}

	if ((req_mp = allocb(sizeof (ipsec_info_t), BPRI_HI)) == NULL) {
		esp1dbg(("esp_inbound_v4:  Can't allocate AUTH_PI_REQ.\n"));
		esp_in_discards++;
		freemsg(ipsec_in_mp);
		freemsg(data_mp);
		IPSA_REFRELE(ipsa);
		return;
	}

	ar = (auth_req_t *)req_mp->b_rptr;
	req_mp->b_datap->db_type = M_CTL;
	req_mp->b_wptr += sizeof (ipsec_info_t);
	ar->auth_req_type = AUTH_PI_IN_AUTH_REQ;
	ar->auth_req_len = sizeof (auth_req_t);
	ar->auth_req_assoc = ipsa;
	ar->auth_req_startoffset = ((uint8_t *)esph - (uint8_t *)ipha);
	/* Need to reflect ESP start point. */
	ar->auth_req_preahlen = ar->auth_req_startoffset;


	if (ipsa->ipsa_auth_alg == IPSA_AALG_NONE) {
		/*
		 * Q:	Should I check the IPSEC_IN to see if AH was
		 *	performed on this?  Given the dangers of auth-less
		 *	ESP, this may be wise.
		 */
		if (ii->ipsec_in_ah_spi == 0) {
			/* A:  Handle it here! */

			esp3dbg(("WARNING: ESP packet with no auth and no"
			    " AH earlier.\n"));
		}

		req_mp->b_cont = ipsec_in_mp;
		ipsec_in_mp->b_cont = data_mp;
		/*
		 * Can use esp_ip_q, because I wouldn't be here (handling an
		 * inbound ESP packet) if there was no esp_ip_q.
		 */
		ASSERT(esp_ip_q != NULL);
		esp_authin_done_v4(esp_ip_q, req_mp);
		return;
	}

	if (esp_aalgs[ipsa->ipsa_auth_alg] == NULL) {
		/* Alg. must've gone while I was out of this module. */
		esp0dbg(("esp_inbound_v4:  Missing auth alg %d.\n",
		    ipsa->ipsa_auth_alg));
		esp_in_discards++;
		freemsg(req_mp);
		freemsg(ipsec_in_mp);
		freemsg(data_mp);
		IPSA_REFRELE(ipsa);
		return;
	}

	authlen = esp_aalgs[ipsa->ipsa_auth_alg]->esps_datalen;

	/*
	 * Problem:  We need to save the authentication data at the
	 * end of the ESP header, but not have the AUTH PI use it
	 * in its authentication.
	 *
	 * Solution: Save it in a pseudo-header mblk.  dupb() is
	 * your friend, but dupb() can be psycho.  We assume that data_mp
	 * is okay to do this to, because we just msgpullup()'ed above.
	 */

	/* First, adjust the IP header's length down. */
	ipha->ipha_length = htons(ntohs(ipha->ipha_length) - authlen);

	/* then do the dupb() */
	pseudo_hdr_mp = dupb(data_mp);
	if (pseudo_hdr_mp == NULL) {
		esp_in_discards++;
		esp1dbg(("esp_inbound_v4:  pseudo_hdr_mp dupb failed.\n"));
		freemsg(ipsec_in_mp);
		freemsg(req_mp);
		freemsg(data_mp);
		IPSA_REFRELE(ipsa);
		return;
	}

	/*
	 * We want a zero-length pseudo-hdr, but rptr pointing
	 * to the auth data.
	 */
	pseudo_hdr_mp->b_rptr = pseudo_hdr_mp->b_wptr - authlen;
	pseudo_hdr_mp->b_wptr = pseudo_hdr_mp->b_rptr;

	/* now trim the "front" data_mp. */
	data_mp->b_wptr -= authlen;

	req_mp->b_cont = ipsec_in_mp;
	ipsec_in_mp->b_cont = pseudo_hdr_mp;
	pseudo_hdr_mp->b_cont = data_mp;
	/*
	 * putnext() to the authentication algorithm.  Handle the return
	 * in esp_authin_done_v4().
	 */
	putnext(esp_aalgs[ipsa->ipsa_auth_alg]->esps_rq, req_mp);
}

/*
 * Handle the AUTH_ACK, and pass the packet back to IP.
 */
static void
esp_authout_done_v4(queue_t *q, mblk_t *mp)
{
	mblk_t *pseudo_hdr_mp, *iomp, *lastmp;
	auth_ack_t *aa;
	ipsa_t *assoc;

	aa = (auth_ack_t *)mp->b_rptr;
	assoc = aa->auth_ack_assoc;

	iomp = mp->b_cont;
	pseudo_hdr_mp = iomp->b_cont;
	lastmp = pseudo_hdr_mp->b_cont;
	freeb(pseudo_hdr_mp);
	iomp->b_cont = lastmp;
	lastmp = iomp;	/* For following while loop. */
	do {
		lastmp = lastmp->b_cont;
	} while (lastmp->b_cont != NULL);

	if (lastmp->b_wptr + ((uintptr_t)aa->auth_ack_datalen) >
	    lastmp->b_datap->db_lim) {
		lastmp->b_cont = allocb(aa->auth_ack_datalen, BPRI_HI);
		if (lastmp->b_cont == NULL) {
			esp_out_discards++;
			IPSA_REFRELE(assoc);
			freeb(mp);
			freemsg(iomp);
			return;
		}
		lastmp = lastmp->b_cont;
	}

	bcopy(aa->auth_ack_data, lastmp->b_wptr, aa->auth_ack_datalen);
	lastmp->b_wptr += aa->auth_ack_datalen;

	/*
	 * Let's rock-n-roll!
	 */

	IPSA_REFRELE(assoc);
	freeb(mp);
	/*
	 * Deliver to IP.  Use write side of current queue, because
	 * esp_ip_q may be NULL, and ESP should ALWAYS be auto-pushed on
	 * top of IP.
	 */
	putnext(WR(q), iomp);
}

/*
 * Set up the outbound datagram for an authentication algorithm.
 */
static void
esp_outbound_auth_v4(queue_t *q, mblk_t *mp, ipsa_t *assoc, uint_t preesplen)
{
	mblk_t *pseudo_hdr_mp, *ipsec_out_mp, *data_mp;
	espstate_t *aalg;
	auth_req_t *ar;


	ipsec_out_mp = mp->b_cont;
	data_mp = ipsec_out_mp->b_cont;

	if (assoc->ipsa_auth_alg == SADB_AALG_NONE) {
		/* We're ready to go! */
		freeb(mp);

		IPSA_REFRELE(assoc);

		/*
		 * Deliver to IP.  Use write side of current queue, because
		 * esp_ip_q may be NULL, and ESP should ALWAYS be auto-pushed
		 * on top of IP.
		 */
		putnext(WR(q), ipsec_out_mp);
		return;
	}

	if ((aalg = esp_aalgs[assoc->ipsa_auth_alg]) == NULL) {
		esp_out_discards++;
		esp1dbg(("esp_outbound_auth_v4:  Missing auth alg.\n"));
		IPSA_REFRELE(assoc);
		freemsg(mp);
		return;
	}

	pseudo_hdr_mp = dupb(mp);
	if (pseudo_hdr_mp == NULL) {
		esp_out_discards++;
		esp0dbg(("esp_outbound_auth_v4: can't get pseudo_hdr_mp"));
		IPSA_REFRELE(assoc);
		freemsg(mp);
		return;
	}

	pseudo_hdr_mp->b_wptr = pseudo_hdr_mp->b_rptr;
	ipsec_out_mp->b_cont = pseudo_hdr_mp;
	pseudo_hdr_mp->b_cont = data_mp;

	ar = (auth_req_t *)mp->b_rptr;

	ar->auth_req_type = AUTH_PI_OUT_AUTH_REQ;
	ar->auth_req_len = sizeof (*ar);
	ar->auth_req_preahlen = preesplen;
	ar->auth_req_startoffset = preesplen;
	ar->auth_req_assoc = assoc;

	/*
	 * putnext() to the authentication algorithm.  Handle the return
	 * in esp_authout_done_v4().
	 */
	putnext(aalg->esps_rq, mp);
}

/*
 * Perform the really difficult work of inserting the proposed situation.
 */
static void
esp_insert_prop(sadb_prop_t *prop, ipsacq_t *acqrec, uint_t combs)
{
	sadb_comb_t *comb = (sadb_comb_t *)(prop + 1);
	int aalg_count;
	ipsec_out_t *io;

	io = (ipsec_out_t *)acqrec->ipsacq_mp->b_rptr;
	ASSERT(io->ipsec_out_type == IPSEC_OUT);

	prop->sadb_prop_exttype = SADB_EXT_PROPOSAL;
	prop->sadb_prop_len = SADB_8TO64(sizeof (sadb_prop_t));
	*(uint32_t *)(&prop->sadb_prop_replay) = 0;	/* Quick zero-out! */

	prop->sadb_prop_replay = ipsecesp_replay_size;

	/*
	 * Based upon algorithm properties, and what-not, prioritize
	 * a proposal.  If the IPSEC_OUT message has an algorithm specified,
	 * use it first and foremost.
	 *
	 * The following heuristic will be used in the meantime:
	 *
	 *  for (auth algs in sortlist, or specified alg)
	 *   Add combination.  If I've hit limit, return.
	 */

	for (aalg_count = 0; aalg_count < esp_num_aalgs; aalg_count++) {
		espstate_t *aalg;

		if (io->ipsec_out_esp_alg != 0) {
			/*
			 * XXX In the future, when ipsec_out expresses a
			 * *list* of preferred algorithms, handle that.
			 */
			aalg = esp_aalgs[io->ipsec_out_esp_ah_alg];
			/* Hack to make this loop run once. */
			combs = 1;
		} else {
			aalg = esp_aalgs[esp_aalgs_sortlist[aalg_count]];
		}

		comb->sadb_comb_flags = 0;
		comb->sadb_comb_reserved = 0;
		comb->sadb_comb_encrypt = SADB_EALG_NULL;
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

		comb->sadb_comb_soft_bytes = ipsecesp_default_soft_bytes;
		comb->sadb_comb_hard_bytes = ipsecesp_default_hard_bytes;
		comb->sadb_comb_soft_addtime = ipsecesp_default_soft_addtime;
		comb->sadb_comb_hard_addtime = ipsecesp_default_hard_addtime;
		comb->sadb_comb_soft_usetime = ipsecesp_default_soft_usetime;
		comb->sadb_comb_hard_usetime = ipsecesp_default_hard_usetime;

		comb->sadb_comb_auth = aalg->esps_id;
		comb->sadb_comb_auth_minbits = aalg->esps_minbits;
		comb->sadb_comb_auth_maxbits = aalg->esps_maxbits;
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
esp_send_acquire(ipsacq_t *acqrec)
{
	mblk_t *pfkeymp, *msgmp;
	keysock_out_t *kso;
	uint_t allocsize, combs;
	sadb_msg_t *samsg;
	sadb_prop_t *prop;

	esp_acquire_requests++;

	ASSERT(MUTEX_HELD(&acqrec->ipsacq_lock));

	pfkeymp = allocb(sizeof (ipsec_info_t), BPRI_HI);
	if (pfkeymp == NULL) {
		esp0dbg(("esp_send_acquire: 1st allocb() failed.\n"));
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

	if (ipsecesp_max_combs == 0)
		combs = esp_num_aalgs;
	else
		combs = ipsecesp_max_combs;

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
		esp0dbg(("esp_send_acquire: 2nd allocb() failed.\n"));
		/* Just bail. */
		freemsg(pfkeymp);
		pfkeymp = NULL;
		goto done;
	}

	samsg = (sadb_msg_t *)msgmp->b_rptr;
	pfkeymp->b_cont = msgmp;

	/* Set up ACQUIRE. */
	samsg->sadb_msg_satype = SADB_SATYPE_ESP;
	sadb_setup_acquire(samsg, acqrec);

	/* XXX Insert proxy address information here. */

	/* XXX Insert identity information here. */

	/* XXXMLS Insert sensitivity information here. */

	/* Insert proposal here. */

	prop = (sadb_prop_t *)(((uint64_t *)samsg) + samsg->sadb_msg_len);
	esp_insert_prop(prop, acqrec, combs);
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
		if (esp_pfkey_q != NULL)
			putnext(esp_pfkey_q, pfkeymp);
		else
			freemsg(pfkeymp);
	}
}

/*
 * An outbound packet needs an SA.  Set up an ACQUIRE record, then send an
 * ACQUIRE message.
 */
static void
esp_acquire(mblk_t *mp, ipha_t *ipha)
{
	mblk_t *ipsec_out_mp, *data_mp;
	uint32_t seq;
	ipsec_out_t *io;
	ipsacq_t *acqrec;
	iacqf_t *bucket;

	ipsec_out_mp = mp;
	io = (ipsec_out_t *)ipsec_out_mp->b_rptr;
	data_mp = ipsec_out_mp->b_cont;
	ASSERT(io->ipsec_out_type == IPSEC_OUT);
	ASSERT(data_mp != NULL);
	ASSERT(data_mp->b_rptr == (uchar_t *)ipha);

	/*
	 * Set up an ACQUIRE record.  (XXX IPv4 specific, need IPv6 code!).
	 * Will eventually want to pull the PROXY source address from
	 * either the inner IP header, or from a future extension to the
	 * IPSEC_OUT message.
	 *
	 * Actually, we'll also want to check for duplicates.
	 */

	seq = atomic_add_32_nv(&esp_acquire_seq, -1);
	/*
	 * Make sure the ACQUIRE sequence number doesn't slip below the
	 * lowest point allowed in the kernel.  (In other words, make sure
	 * the high bit on the sequence number is set.)
	 */
	seq |= IACQF_LOWEST_SEQ;

	if (seq == (uint32_t)-1) {
		/* We have rolled over.  Note it. */
		(void) strlog(info.mi_idnum, 0, 0, SL_ERROR | SL_NOTE,
		    "ESP ACQUIRE sequence number has wrapped.\n");
	}

	/* XXX IPv4-specific code. */
	bucket = &esp_acquires_v4[OUTBOUND_HASH(ipha->ipha_dst)];
	/* Need v6 code?!? */
	acqrec = sadb_new_acquire(bucket, seq, mp, SADB_SATYPE_ESP,
	    ipsecesp_acquire_timeout, sizeof (ipaddr_t));

	if (acqrec == NULL) {
		/* Error condition.  Send a failure back to IP. */
		io->ipsec_out_esp_req |= IPSEC_REQ_FAILED;
		ASSERT(esp_ip_q != NULL);
		putnext(esp_ip_q, mp);
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
	esp_send_acquire(acqrec);
}

/*
 * Handle the SADB_GETSPI message.  Create a larval SA.
 *
 * XXX IPv4 specific.  Fix for IPv6.
 */
static void
esp_getspi(mblk_t *mp, keysock_in_t *ksi)
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
	newbie = sadb_getspi(ksi, atomic_add_32_nv(&esp_spi, increment));

	if (newbie == NULL) {
		sadb_pfkey_error(esp_pfkey_q, mp, ENOMEM, ksi->ks_in_serial);
		return;
	} else if (newbie == (ipsa_t *)-1) {
		sadb_pfkey_error(esp_pfkey_q, mp, EINVAL, ksi->ks_in_serial);
		return;
	}

	/*
	 * XXX - We may randomly collide.  We really should recover from this.
	 *	 Unfortunately, that could require spending way-too-much-time
	 *	 in here.  For now, let the user retry.
	 */

	mutex_enter(&esp_larval_list_v4.isaf_lock);
	outbound = &esp_outbound_assoc_v4[
	    OUTBOUND_HASH(*(uint32_t *)(newbie->ipsa_dstaddr))];
	mutex_enter(&outbound->isaf_lock);
	inbound = &esp_inbound_assoc_v4[INBOUND_HASH(newbie->ipsa_spi)];
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
		rc = sadb_insertassoc(newbie, &esp_larval_list_v4);
		(void) drv_getparm(TIME, &newbie->ipsa_hardexpiretime);
		newbie->ipsa_hardexpiretime += ipsecesp_larval_timeout;
	}

	/* Can exit other mutexes.  Hold larval until we're done with newbie. */
	mutex_exit(&inbound->isaf_lock);
	mutex_exit(&outbound->isaf_lock);

	if (rc != 0) {
		mutex_exit(&esp_larval_list_v4.isaf_lock);
		IPSA_REFRELE(newbie);
		sadb_pfkey_error(esp_pfkey_q, mp, rc, ksi->ks_in_serial);
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
	mutex_exit(&esp_larval_list_v4.isaf_lock);

	/* Convert KEYSOCK_IN to KEYSOCK_OUT. */
	kso = (keysock_out_t *)ksi;
	kso->ks_out_len = sizeof (*kso);
	kso->ks_out_serial = ksi->ks_in_serial;
	kso->ks_out_type = KEYSOCK_OUT;

	/*
	 * Can safely putnext() to esp_pfkey_q, because this is a turnaround
	 * from the esp_pfkey_q.
	 */
	putnext(esp_pfkey_q, mp);
}

/*
 * Insert the ESP header into a packet.  Duplicate an mblk, and insert a newly
 * allocated mblk with the ESP header in between the two.
 */
static boolean_t
esp_insert_esp(mblk_t *mp, mblk_t *esp_mp, uint_t divpoint)
{
	mblk_t *split_mp = mp;
	uint_t wheretodiv = divpoint;

	while ((split_mp->b_wptr - split_mp->b_rptr) < wheretodiv) {
		wheretodiv -= (split_mp->b_wptr - split_mp->b_rptr);
		split_mp = split_mp->b_cont;
		ASSERT(split_mp != NULL);
	}

	if (split_mp->b_wptr - split_mp->b_rptr != wheretodiv) {
		mblk_t *scratch;

		/* "scratch" is the 2nd half, split_mp is the first. */
		scratch = dupb(split_mp);
		if (scratch == NULL) {
			esp1dbg(("esp_insert_esp: can't allocate scratch.\n"));
			return (B_FALSE);
		}
		/* NOTE:  dupb() doesn't set b_cont appropriately. */
		scratch->b_cont = split_mp->b_cont;
		scratch->b_rptr += wheretodiv;
		split_mp->b_wptr = split_mp->b_rptr + wheretodiv;
		split_mp->b_cont = scratch;
	}
	/*
	 * At this point, split_mp is exactly "wheretodiv" bytes long, and
	 * holds the end of the pre-ESP part of the datagram.
	 */
	esp_mp->b_cont = split_mp->b_cont;
	split_mp->b_cont = esp_mp;

	return (B_TRUE);
}

/*
 * Handle outbound IPsec processing for IPv4.
 */
static void
esp_outbound_v4(mblk_t *mp, ipsa_t *assoc)
{
	mblk_t *ipsec_out_mp, *req_mp, *data_mp, *espmp, *tailmp;
	espstate_t *aalg;
	ipsec_out_t *io;
	ipha_t *ipha;
	esph_t *esph;
	ipaddr_t dst;
	uintptr_t divpoint, datalen, adj, padlen, i, alloclen;
	uintptr_t esplen = sizeof (esph_t);
	uint8_t protocol;
	isaf_t *bucket;

	esp_out_requests++;

	ASSERT(esp_ip_q != NULL);

	ipsec_out_mp = mp;
	data_mp = ipsec_out_mp->b_cont;

	/*
	 * <sigh> We have to copy the message here, because TCP (for example)
	 * keeps a dupb() of the message lying around for retransmission.
	 * Since ESP changes the whole of the datagram, we have to create our
	 * own copy lest we clobber TCP's data.  Since we have to copy anyway,
	 * we might as well make use of msgpullup() and get the mblk into one
	 * contiguous piece!
	 */
	ipsec_out_mp->b_cont = msgpullup(data_mp, -1);
	freemsg(data_mp);
	if (ipsec_out_mp->b_cont == NULL) {
		esp0dbg(("esp_outbound_v4: msgpullup() failed, "
		    "dropping packet.\n"));
		freeb(ipsec_out_mp);
		return;
	} else {
		data_mp = ipsec_out_mp->b_cont;
	}

	io = (ipsec_out_t *)ipsec_out_mp->b_rptr;
	ipha = (ipha_t *)data_mp->b_rptr;

	/*
	 * Reality check....
	 */


	if (io->ipsec_out_esp_ah_alg != 0 &&
	    esp_aalgs[io->ipsec_out_esp_ah_alg] == NULL) {
		esp_out_discards++;
		esp0dbg(("esp_outbound_v4: "
		    "Bad authentication algorithm request %d.\n",
		    io->ipsec_out_esp_ah_alg));
		freemsg(mp);
		return;
	}

	dst = ip_get_dst(ipha);
	/*
	 * NOTE:Getting the outbound association will be considerably
	 *	painful.  sadb_getassocbyipc() will require more parameters as
	 *	policy implementations mature.
	 */
	if (assoc == NULL || assoc->ipsa_state == IPSA_STATE_DEAD) {
		bucket = &esp_outbound_assoc_v4[OUTBOUND_HASH(ipha->ipha_dst)];

		if (assoc != NULL) {
			IPSA_REFRELE(assoc);
		}
		mutex_enter(&bucket->isaf_lock);
		assoc = sadb_getassocbyipc(bucket, io,
		    (uint8_t *)&ipha->ipha_src, (uint8_t *)&dst, IP_ADDR_LEN,
		    IPPROTO_ESP);
		mutex_exit(&bucket->isaf_lock);

		if (assoc == NULL || assoc->ipsa_state == IPSA_STATE_DEAD) {
			esp3dbg(("esp_outbound_v4:  "
			    "Would send PF_KEY ACQUIRE message here!\n"));
			if (assoc != NULL) {
				IPSA_REFRELE(assoc);
			}
			esp_acquire(mp, ipha);
			return;
		}

	}

	if (assoc->ipsa_usetime == 0)
		esp_set_usetime(assoc, B_FALSE);

	/* Update ipsec_out_esp_spi NOW, before we continue. */
	io->ipsec_out_esp_spi = assoc->ipsa_spi;

	aalg = esp_aalgs[assoc->ipsa_auth_alg];
	if (aalg == NULL) {
		esp_out_discards++;
		esp1dbg((
		    "esp_outbound_v4: Algorithm unloaded behind my back.\n"));
		/* May wish to unlink association, too. */
		sadb_replay_delete(assoc);
		IPSA_REFRELE(assoc);
		freemsg(mp);
		return;
	}

	divpoint = IPH_HDR_LENGTH(ipha);
	datalen = ntohs(ipha->ipha_length) - divpoint;

	/*
	 * Determine the padding length.   Pad to 4-bytes.
	 *
	 * Include the two additional bytes (hence the - 2) for the padding
	 * length and the next header.  Take this into account when
	 * calculating the actual length of the padding.
	 */

		padlen = ((unsigned)(sizeof (uint32_t) - datalen - 2)) %
		    sizeof (uint32_t);

	/*
	 * Update association byte-count lifetimes.  Don't forget to take
	 * into account the padding length and next-header (hence the + 2).
	 */

	if (!esp_age_bytes(assoc, datalen +
	    padlen + 2, B_FALSE)) {
		/*
		 * Rig things as if sadb_getassocbyipc() failed.
		 */
		ipsec_rl_strlog(info.mi_idnum, 0, 0, SL_ERROR | SL_WARN,
		    "ESP Association 0x%x, dst %d.%d.%d.%d had bytes expire.\n",
		    ntohl(assoc->ipsa_spi), assoc->ipsa_dstaddr[0],
		    assoc->ipsa_dstaddr[1], assoc->ipsa_dstaddr[2],
		    assoc->ipsa_dstaddr[3]);
		IPSA_REFRELE(assoc);
		esp_acquire(mp, ipha);
		return;
	}

	espmp = allocb(esplen, BPRI_HI);
	if (espmp == NULL) {
		esp_out_discards++;
		esp1dbg(("esp_outbound_v4: can't allocate espmp.\n"));
		IPSA_REFRELE(assoc);
		freemsg(mp);
		return;
	}
	espmp->b_wptr += esplen;
	esph = (esph_t *)espmp->b_rptr;
	esph->esph_spi = assoc->ipsa_spi;

	mutex_enter(&assoc->ipsa_lock);
	esph->esph_replay = ++(assoc->ipsa_replay);
	esph->esph_replay = htonl(esph->esph_replay);
	if (esph->esph_replay == 0 && assoc->ipsa_replay_wsize != 0) {
		/*
		 * XXX We have replay counter wrapping.
		 * We probably want to nuke this SA (and its peer).
		 * XXX IPv6 also print address, but in an AF-independent way.
		 */
		ipsec_rl_strlog(info.mi_idnum, 0, 0,
		    SL_ERROR | SL_CONSOLE | SL_WARN,
		    "Outbound ESP SA (0x%x) has wrapped sequence.\n",
		    htonl(esph->esph_spi));

		esp_out_discards++;
		sadb_replay_delete(assoc);
		IPSA_REFRELE(assoc);
		freemsg(mp);
		return;
	}
	mutex_exit(&assoc->ipsa_lock);


	/* Fix the IP header. */
	alloclen = padlen + 2 + ((aalg != NULL) ? aalg->esps_datalen : 0);
	adj = alloclen + (espmp->b_wptr - espmp->b_rptr);
	ipha->ipha_length = htons(ntohs(ipha->ipha_length) + adj);
	protocol = ipha->ipha_protocol;
	ipha->ipha_protocol = IPPROTO_ESP;
	ipha->ipha_hdr_checksum = 0;
	ipha->ipha_hdr_checksum = (uint16_t)ip_csum_hdr(ipha);

	/* I've got the two ESP mblks, now insert them. */

	esp1dbg(("data_mp before outbound ESP adjustment:\n"));
	esp1dbg((dump_msg(data_mp)));

	if (!esp_insert_esp(data_mp, espmp, divpoint)) {
		esp_out_discards++;
		IPSA_REFRELE(assoc);
		freeb(espmp);
		freemsg(mp);
		return;
	}

	/* Append padding (and leave room for ICV). */
	for (tailmp = data_mp; tailmp->b_cont != NULL; tailmp = tailmp->b_cont)
		;
	if (tailmp->b_wptr + alloclen > tailmp->b_datap->db_lim) {
		tailmp->b_cont = allocb(alloclen, BPRI_HI);
		if (tailmp->b_cont == NULL) {
			esp_out_discards++;
			esp0dbg(("esp_outbound_v4:  Can't allocate tailmp.\n"));
			IPSA_REFRELE(assoc);
			freemsg(mp);
			return;
		}
		tailmp = tailmp->b_cont;
	}

	/*
	 * If there's padding, N bytes of padding must be of the form 0x1,
	 * 0x2, 0x3... 0xN.
	 */
	for (i = 0; i < padlen; ) {
		i++;
		*tailmp->b_wptr++ = i;
	}
	*tailmp->b_wptr++ = i;
	*tailmp->b_wptr++ = protocol;

	req_mp = allocb(sizeof (ipsec_info_t), BPRI_HI);
	if (req_mp == NULL) {
		esp_out_discards++;
		esp0dbg(("esp_outbound_v4:  Can't allocate req_mp.\n"));
		IPSA_REFRELE(assoc);
		freemsg(mp);
		return;
	}
	req_mp->b_wptr += sizeof (ipsec_info_t);
	req_mp->b_cont = mp;
	req_mp->b_datap->db_type = M_CTL;

		ASSERT(aalg != NULL);
		/*
		 * Pass esp_ip_q for the queue, because if we hit this
		 * function (i.e. handle an outbound ESP packet), we KNOW that
		 * IP has a successful T_BIND_REQ, hence esp_ip_q is non-NULL.
		 */
		ASSERT(esp_ip_q != NULL);
		esp_outbound_auth_v4(esp_ip_q, req_mp, assoc, divpoint);
}

/*
 * IP calls this to validate the ICMP errors that
 * we got from the network.
 */
/* ARGSUSED */
static void
esp_icmp_error_v4(queue_t *q, mblk_t *ipsec_mp)
{
	mblk_t *mp;
	icmph_t *icmph;
	int iph_hdr_length, hdr_length;
	isaf_t *hptr;
	ipsa_t *assoc;
	ipha_t *ipha, *oipha;
	esph_t *esph;

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
			    "ICMP error : Small ESP header\n");
			esp_in_discards++;
			freemsg(ipsec_mp);
			return;
		}
		icmph = (icmph_t *)&mp->b_rptr[iph_hdr_length];
		ipha = (ipha_t *)&icmph[1];
	}

	esph = (esph_t *)((uint8_t *)ipha + hdr_length);

	hptr = &esp_outbound_assoc_v4[OUTBOUND_HASH(ipha->ipha_dst)];
	mutex_enter(&hptr->isaf_lock);
	assoc = sadb_getassocbyspi(hptr, esph->esph_spi,
	    (uint8_t *)&ipha->ipha_src, (uint8_t *)&ipha->ipha_dst,
	    IP_ADDR_LEN);
	mutex_exit(&hptr->isaf_lock);

	if (assoc == NULL) {
		esp_lookup_failure++;
		esp_in_discards++;
		ipsec_rl_strlog(info.mi_idnum, 0, 0, SL_WARN | SL_ERROR,
		    "Bad ICMP message - No association for the "
		    "attached ESP header whose spi is 0x%x, sender is 0x%x\n",
		    ntohl(esph->esph_spi), ntohl(oipha->ipha_src));
		freemsg(ipsec_mp);
		return;
	}

	/*
	 * There seems to be a valid association.
	 *
	 * Unlike AH, stripping out ESP is quite difficult.  I'll give it a try
	 * if the packet contains all of the data...
	 */

	if (ntohs(ipha->ipha_length) > msgdsize(mp) - iph_hdr_length -
	    sizeof (icmph_t)) {
		/*
		 * XXX if we ever use a stateful cipher, such as a stream or
		 * a one-time pad, then we can't do anything here, either.
		 */
		esp_in_discards++;
		freemsg(ipsec_mp);
	} else {
		/*
		 * Partial decryptions are useless, because the "next header"
		 * is at the end of the decrypted ESP packet.  Without the
		 * whole packet, this is useless.
		 */
		esp_in_discards++;
		freemsg(ipsec_mp);
	}

	IPSA_REFRELE(assoc);
}

/*
 * ESP module read put routine.
 */
/* ARGSUSED */
static void
ipsecesp_rput(queue_t *q, mblk_t *mp)
{
	keysock_in_t *ksi;
	int *addrtype;
	ire_t *ire;
	mblk_t *ire_mp, *last_mp;

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

			/* XXX IPv6 :  This is v4-specific. */
			if (ire->ire_type == 0) {
				sadb_pfkey_error(esp_pfkey_q, mp,
				    EADDRNOTAVAIL, ksi->ks_in_serial);
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
			if (esp_pfkey_q != NULL) {
				/*
				 * Decrement counter to make up for
				 * auto-increment in ipsecesp_wput().
				 */
				esp_keysock_in--;
				ipsecesp_wput(WR(esp_pfkey_q), mp);
			} else {
				freemsg(mp);
			}
			break;
		case IPSEC_IN:
			esp2dbg(("IPSEC_IN message.\n"));
			/* Check for v4 or v6. */
			if (((ipsec_in_t *)mp->b_rptr)->ipsec_in_v4) {
				if (mp->b_cont->b_datap->db_type == M_CTL) {
					esp_icmp_error_v4(q, mp);
				} else {
					esp_inbound_v4(mp);
				}
			} else {
				/* esp_inbound_v6(mp); */
				esp2dbg(("Probably IPv6 packet.\n"));
				freemsg(mp);
			}
			break;
		case IPSEC_OUT:
			esp2dbg(("IPSEC_OUT message.\n"));
			ASSERT(mp->b_cont != NULL);
			esp_outbound_v4(mp, NULL);
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
			esp3dbg(("Thank you IP from ESP for T_BIND_ACK\n"));
			break;
		case T_ERROR_ACK:
			cmn_err(CE_WARN,
			    "ipsecesp:  ESP received T_ERROR_ACK from IP.");
			/*
			 * Make esp_ip_q NULL, and in the future, perhaps try
			 * again.
			 */
			esp_ip_q = NULL;
			break;
		case T_OK_ACK:
			/* Probably from a (rarely sent) T_UNBIND_REQ. */
			break;
		default:
			esp0dbg(("Unknown M_{,PC}PROTO message.\n"));
		}
		freemsg(mp);
		break;
	default:
		/* For now, eat message. */
		esp2dbg(("ESP got unknown mblk type %d.\n",
		    mp->b_datap->db_type));
		freemsg(mp);
	}
}

/*
 * Construct an SADB_REGISTER message with the current algorithms.
 */
static boolean_t
esp_register_out(uint32_t sequence, uint32_t pid, uint_t serial)
{
	mblk_t *pfkey_msg_mp, *keysock_out_mp;
	sadb_msg_t *samsg;
	sadb_supported_t *sasupp_auth = NULL;
	sadb_alg_t *saalg;
	keysock_out_t *ksout;
	uint_t allocsize = sizeof (*samsg);
	uint_t i, numalgs_snap;
	int current_aalgs;

	/* Allocate the KEYSOCK_OUT. */
	keysock_out_mp = allocb(sizeof (ipsec_info_t), BPRI_HI);
	if (keysock_out_mp == NULL) {
		esp0dbg(("esp_register_out: couldn't allocate mblk.\n"));
		return (B_FALSE);
	}

	keysock_out_mp->b_datap->db_type = M_CTL;
	keysock_out_mp->b_wptr += sizeof (keysock_out_t);
	ksout = (keysock_out_t *)keysock_out_mp->b_rptr;
	ksout->ks_out_type = KEYSOCK_OUT;
	ksout->ks_out_len = sizeof (keysock_out_t);
	ksout->ks_out_serial = serial;

	/*
	 * Allocate the PF_KEY message that follows KEYSOCK_OUT.
	 *
	 * PROBLEM:	I need to hold the esp_aalg_lock to allocate the
	 *		variable part (i.e. the algorithms) because the
	 *		number is protected by the lock.
	 *
	 *		This may be a bit contentious w.r.t. the lock... maybe.
	 */

	mutex_enter(&esp_aalg_lock);

	/*
	 * Fill SADB_REGISTER message's algorithm descriptors.  Hold
	 * down the lock while filling it.
	 */
	if (esp_num_aalgs != 0) {
		allocsize += (esp_num_aalgs * sizeof (*saalg));
		allocsize += sizeof (*sasupp_auth);
	}
	keysock_out_mp->b_cont = allocb(allocsize, BPRI_HI);
	if (keysock_out_mp->b_cont == NULL) {
		mutex_exit(&esp_aalg_lock);
		freemsg(keysock_out_mp);
		return (B_FALSE);
	}

	pfkey_msg_mp = keysock_out_mp->b_cont;
	pfkey_msg_mp->b_wptr += allocsize;
	if (esp_num_aalgs != 0) {
		sasupp_auth = (sadb_supported_t *)
		    (pfkey_msg_mp->b_rptr + sizeof (*samsg));
		saalg = (sadb_alg_t *)(sasupp_auth + 1);

		ASSERT(((ulong_t)saalg & 0x7) == 0);

		numalgs_snap = 0;
		for (i = 0;
		    ((i < MAX_AALGS) && (numalgs_snap < esp_num_aalgs)); i++) {
			if (esp_aalgs[i] == NULL)
				continue;

			saalg->sadb_alg_id = esp_aalgs[i]->esps_id;
			saalg->sadb_alg_ivlen = 0;
			saalg->sadb_alg_minbits = esp_aalgs[i]->esps_minbits;
			saalg->sadb_alg_maxbits = esp_aalgs[i]->esps_maxbits;
			saalg->sadb_alg_reserved = 0;
			numalgs_snap++;
			saalg++;
		}
		ASSERT(numalgs_snap == esp_num_aalgs);

#ifdef DEBUG
		/*
		 * Reality check to make sure I snagged all of the
		 * algorithms.
		 */
		while (i < MAX_AALGS) {
			if (esp_aalgs[i++] != NULL)
				cmn_err(CE_PANIC,
				    "esp_register_out()!  Missed aalg #%d.\n",
				    i);
		}
#endif /* DEBUG */
	} else {
		saalg = (sadb_alg_t *)(pfkey_msg_mp->b_rptr + sizeof (*samsg));
	}


	current_aalgs = esp_num_aalgs;
	mutex_exit(&esp_aalg_lock);

	/* Now fill the rest of the SADB_REGISTER message. */

	samsg = (sadb_msg_t *)pfkey_msg_mp->b_rptr;
	samsg->sadb_msg_version = PF_KEY_V2;
	samsg->sadb_msg_type = SADB_REGISTER;
	samsg->sadb_msg_errno = 0;
	samsg->sadb_msg_satype = SADB_SATYPE_ESP;
	samsg->sadb_msg_len = SADB_8TO64(allocsize);
	samsg->sadb_msg_reserved = 0;
	/*
	 * Assume caller has sufficient sequence/pid number info.  If it's one
	 * from me over a new alg., I could give two hoots about sequence.
	 */
	samsg->sadb_msg_seq = sequence;
	samsg->sadb_msg_pid = pid;

	if (sasupp_auth != NULL) {
		sasupp_auth->sadb_supported_len =
		    SADB_8TO64(sizeof (*sasupp_auth) +
			sizeof (*saalg) * current_aalgs);
		sasupp_auth->sadb_supported_exttype = SADB_EXT_SUPPORTED_AUTH;
		sasupp_auth->sadb_supported_reserved = 0;
	}


	if (esp_pfkey_q != NULL)
		putnext(esp_pfkey_q, keysock_out_mp);
	else {
		freemsg(keysock_out_mp);
		return (B_FALSE);
	}

	return (B_TRUE);
}

/*
 * Place a queue instance into the algorithm fanout.
 */
static void
esp_new_alg(queue_t *q, mblk_t *mp)
{
	espstate_t *esps = (espstate_t *)q->q_ptr;
	auth_pi_hello_t *aph = (auth_pi_hello_t *)mp->b_rptr;
	kmutex_t *alg_lock;
	espstate_t **alg_fanout;
	uint_t *alg_count;
	int *alg_sortlist;

	ASSERT(esps != NULL);

	esps->esps_id = aph->auth_hello_id;
		esps->esps_ivlen = 0;
	esps->esps_minbits = aph->auth_hello_minbits;
	esps->esps_maxbits = aph->auth_hello_maxbits;
		esps->esps_datalen = aph->auth_hello_datalen;
	esps->esps_keycheck = aph->auth_hello_keycheck;

	/*
	 * If auth_hello_numkeys is needed, copy it here.
	 */

		alg_count = &esp_num_aalgs;
		alg_lock = &esp_aalg_lock;
		alg_fanout = esp_aalgs;
		alg_sortlist = esp_aalgs_sortlist;
	mutex_enter(alg_lock);
	if (alg_fanout[esps->esps_id] != NULL) {
		/* Hmmm, duplicate algorithm ids. */
		(void) strlog(info.mi_idnum, 0, 0,
		    SL_ERROR | SL_CONSOLE | SL_WARN,
		    "Duplicate push of algorithm %d on ESP.\n", esps->esps_id);
		mutex_exit(alg_lock);
		esps->esps_id = 0;
		return;
	} else {
		alg_fanout[esps->esps_id] = esps;
		(*alg_count)++;
	}

	/*
	 * Prioritize algorithms as they are inserted.  This will mean we have
	 * to keep holding the lock.
	 */
	esp_insert_sortlist(esps, alg_fanout, alg_sortlist, *alg_count);

	mutex_exit(alg_lock);

	/*
	 * Time to send a PF_KEY SADB_REGISTER message to ESP listeners
	 * everywhere.  (The function itself checks for NULL esp_pfkey_q.)
	 */
	(void) esp_register_out(0, 0, 0);

		aph->auth_hello_type = AUTH_PI_ACK;
	aph->auth_hello_len = sizeof (ipsec_info_t);
	qreply(q, mp);
}

/*
 * Now that weak-key passed, actually ADD the security association, and
 * send back a reply ADD message.
 */
static int
esp_add_sa_finish(mblk_t *mp, sadb_msg_t *samsg, keysock_in_t *ksi)
{
	isaf_t *primary, *secondary;
	sadb_sa_t *assoc = (sadb_sa_t *)ksi->ks_in_extv[SADB_EXT_SA];
	sadb_address_t *dstext =
	    (sadb_address_t *)ksi->ks_in_extv[SADB_EXT_ADDRESS_DST];
	struct sockaddr_in *dst;	/* XXX IPv6 : _in6 too! */
	boolean_t clone = B_FALSE;
	uint8_t *dstaddr;
	ipsa_t *larval = NULL;
	ipsacq_t *acqrec;
	iacqf_t *acq_bucket;
	mblk_t *acq_msgs = NULL;
	int rc;

	if (assoc->sadb_sa_encrypt != SADB_EALG_NULL)
		return (EINVAL);

	/*
	 * Locate the appropriate table(s).
	 * XXX IPv6 : There's v4 specific stuff in here for now.
	 * The v4/v6 switch needs to be made here.
	 */

	dst = (struct sockaddr_in *)(dstext + 1);
	dstaddr = (uint8_t *)(&dst->sin_addr);

	switch (ksi->ks_in_dsttype) {
	case KS_IN_ADDR_ME:
	case KS_IN_ADDR_MBCAST:
		primary = &esp_inbound_assoc_v4[
		    INBOUND_HASH(assoc->sadb_sa_spi)];
		secondary = &esp_outbound_assoc_v4[
		    OUTBOUND_HASH(*(ipaddr_t *)dstaddr)];
		/*
		 * If the source address is either one of mine, or unspecified
		 * (which is best summed up by saying "not 'not mine'"),
		 * then the association is potentially bi-directional,
		 * in that it can be used for inbound traffic and outbound
		 * traffic.  The best example of such an SA is a multicast
		 * SA (which allows me to receive the outbound traffic).
		 */
		if (ksi->ks_in_srctype != KS_IN_ADDR_NOTME)
			clone = B_TRUE;
		break;
	case KS_IN_ADDR_NOTME:
		primary = &esp_outbound_assoc_v4[
		    OUTBOUND_HASH(*(ipaddr_t *)dstaddr)];
		secondary = &esp_inbound_assoc_v4[
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
		acq_bucket = &esp_acquires_v4[
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

		mutex_enter(&esp_larval_list_v4.isaf_lock);
		larval = sadb_getassocbyspi(&esp_larval_list_v4,
		    assoc->sadb_sa_spi, zeroes, dstaddr, sizeof (ipaddr_t));
		if (larval == NULL) {
			esp0dbg(("Larval update, but larval disappeared.\n"));
			error = B_TRUE;
		} /* Else sadb_common_add unlinks it for me! */
		mutex_exit(&esp_larval_list_v4.isaf_lock);
		if (error)
			return (ESRCH);
	}

	/* XXX IPv6 :  The larval list is is v4/v6 specific. */
	rc = sadb_common_add(esp_pfkey_q, mp, samsg, ksi, primary, secondary,
	    &esp_larval_list_v4, larval, clone);

	/*
	 * How much more stack will I create with all of these
	 * esp_outbound_v4() calls?
	 */

	while (acq_msgs != NULL) {
		mblk_t *mp = acq_msgs;

		acq_msgs = acq_msgs->b_next;
		mp->b_next = NULL;
		if (rc == 0) {
			esp_outbound_v4(mp, NULL);  /* XXX v4/v6 specific! */
		} else {
			esp_out_discards++;
			freemsg(mp);
		}
	}

	return (rc);
}

/*
 * Add new ESP security association.  This may become a generic AH/ESP
 * routine eventually.
 */
static int
esp_add_sa(mblk_t *mp, keysock_in_t *ksi)
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
	struct sockaddr_in *src, *dst, *proxy;
	sadb_lifetime_t *soft =
	    (sadb_lifetime_t *)ksi->ks_in_extv[SADB_EXT_LIFETIME_SOFT];
	sadb_lifetime_t *hard =
	    (sadb_lifetime_t *)ksi->ks_in_extv[SADB_EXT_LIFETIME_HARD];
	mblk_t *keycheck_mp;
	espstate_t *aalg;
	auth_keycheck_t *akc;

	/* I need certain extensions present for an ADD message. */
	if (srcext == NULL || dstext == NULL || assoc == NULL ||
	    (ekey == NULL && assoc->sadb_sa_encrypt != SADB_EALG_NULL)) {
		esp1dbg(("esp_add_sa: Missing basic extensions.\n"));
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
	/* XXX STATS :  Logging/stats here? */
	if ((assoc->sadb_sa_state != SADB_SASTATE_MATURE) ||
	    (assoc->sadb_sa_encrypt == SADB_EALG_NONE) ||
	    (assoc->sadb_sa_encrypt == SADB_EALG_NULL &&
		assoc->sadb_sa_auth == SADB_AALG_NONE) ||
	    (assoc->sadb_sa_flags & ~(SADB_SAFLAGS_NOREPLAY)) ||
	    !sadb_hardsoftchk(hard, soft) ||
	    (src->sin_family != dst->sin_family) ||
	    ((proxy != NULL) && (proxy->sin_family != dst->sin_family))) {
		return (EINVAL);
	}

	/* Stuff I don't support, for now. */
	if (ksi->ks_in_extv[SADB_EXT_LIFETIME_CURRENT] != NULL ||
	    ksi->ks_in_extv[SADB_EXT_SENSITIVITY] != NULL) {
		return (EOPNOTSUPP);
	}

	/*
	 * XXX Policy :  I'm not checking identities or sensitivity
	 * labels at this time, but if I did, I'd do them here, before I sent
	 * the weak key check up to the algorithm.
	 */

	/*
	 * First locate the auth algorithm.
	 *
	 * Q: Should I mutex_enter(&esp_aalg_lock)?
	 * A: Probably not for speed.  Besides, my open/close isn't shared.
	 */
	if (akey != NULL) {
		aalg = esp_aalgs[assoc->sadb_sa_auth];
		if (aalg == NULL)
			return (EINVAL);
	} else {
		aalg = NULL;
	}

	if (aalg != NULL)
		if (aalg->esps_keycheck) {
			keycheck_mp = allocb(sizeof (ipsec_info_t), BPRI_HI);
			if (keycheck_mp == NULL)
				return (ENOMEM);
			keycheck_mp->b_datap->db_type = M_CTL;
			keycheck_mp->b_wptr += sizeof (ipsec_info_t);
			akc = (auth_keycheck_t *)keycheck_mp->b_rptr;
			keycheck_mp->b_cont = mp;
			akc->auth_keycheck_type = AUTH_PI_KEY_CHECK;
			akc->auth_keycheck_len = sizeof (*akc);
			putnext(aalg->esps_rq, keycheck_mp);
			/*
			 * Return message will be dispatched from wput() to
			 * esp_auth_keycheck_ret()
			 */
			return (0);
		} else if (akey->sadb_key_bits < aalg->esps_minbits ||
		    akey->sadb_key_bits > aalg->esps_maxbits)
			return (EINVAL);

	return (esp_add_sa_finish(mp, (sadb_msg_t *)mp->b_cont->b_rptr, ksi));
}

/*
 * Update a security association.  Updates come in two varieties.  The first
 * is an update of lifetimes on a non-larval SA.  The second is an update of
 * a larval SA, which ends up looking a lot more like an add.
 */
static int
esp_update_sa(mblk_t *mp, keysock_in_t *ksi)
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
	/* XXX IPv6 :  The following should be _in6 too! */
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
		esp1dbg(("esp_update_sa: Missing basic extensions.\n"));
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
	mutex_enter(&esp_larval_list_v4.isaf_lock);
	outbound = &esp_outbound_assoc_v4[
	    OUTBOUND_HASH(*(uint32_t *)(&dst->sin_addr))];
	mutex_enter(&outbound->isaf_lock);
	inbound = &esp_inbound_assoc_v4[INBOUND_HASH(assoc->sadb_sa_spi)];
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
		outbound_target = sadb_getassocbyspi(&esp_larval_list_v4,
		    assoc->sadb_sa_spi,
		    ((src == NULL) ? (zeroes) : ((uint8_t *)&src->sin_addr)),
		    (uint8_t *)&dst->sin_addr, sizeof (dst->sin_addr));
		if (outbound_target == NULL) {
			mutex_exit(&esp_larval_list_v4.isaf_lock);
			return (ESRCH);
		}
		/* Else REFRELE the target and let esp_add_sa() deal with it. */
		IPSA_REFRELE(outbound_target);
		mutex_exit(&esp_larval_list_v4.isaf_lock);
		return (esp_add_sa(mp, ksi));
	}

	mutex_exit(&esp_larval_list_v4.isaf_lock);

	/* Reality checks for updates of active associations. */
	/* Sundry first-pass UPDATE-specific reality checks. */
	/* XXX STATS :  Logging/stats here? */
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

	sadb_pfkey_echo(esp_pfkey_q, mp, (sadb_msg_t *)mp->b_cont->b_rptr,
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
esp_del_sa(mblk_t *mp, keysock_in_t *ksi)
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
	 * XXX IPv6 : Be careful, you'll have to check for v4/v6.
	 */

	/* Lock down all three buckets. */
	mutex_enter(&esp_larval_list_v4.isaf_lock);
	outbound = &esp_outbound_assoc_v4[
	    OUTBOUND_HASH(*(uint32_t *)(&dst->sin_addr))];
	mutex_enter(&outbound->isaf_lock);
	inbound = &esp_inbound_assoc_v4[INBOUND_HASH(assoc->sadb_sa_spi)];
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
		outbound_target = sadb_getassocbyspi(&esp_larval_list_v4,
		    assoc->sadb_sa_spi,
		    ((src == NULL) ? (zeroes) : ((uint8_t *)&src->sin_addr)),
		    (uint8_t *)&dst->sin_addr, sizeof (dst->sin_addr));
		if (outbound_target == NULL) {
			mutex_exit(&inbound->isaf_lock);
			mutex_exit(&outbound->isaf_lock);
			mutex_exit(&esp_larval_list_v4.isaf_lock);
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
	mutex_exit(&esp_larval_list_v4.isaf_lock);

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
	sadb_pfkey_echo(esp_pfkey_q, mp, (sadb_msg_t *)mp->b_cont->b_rptr,
	    ksi, NULL);
	return (0);
}

/*
 * Find an ESP security association and return it in a PF_KEY message.
 * Perhaps this should be common AH/ESP code, too.
 */
static int
esp_get_sa(mblk_t *mp, keysock_in_t *ksi)
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
	 * XXX IPv6 : Be careful, you'll have to check for v4/v6.
	 */

	/* Lock down all three buckets. */
	mutex_enter(&esp_larval_list_v4.isaf_lock);
	outbound = &esp_outbound_assoc_v4[
	    OUTBOUND_HASH(*(uint32_t *)(&dst->sin_addr))];
	mutex_enter(&outbound->isaf_lock);
	inbound = &esp_inbound_assoc_v4[INBOUND_HASH(assoc->sadb_sa_spi)];
	mutex_enter(&inbound->isaf_lock);

	/* Try outbound first. */
	target = sadb_getassocbyspi(outbound, assoc->sadb_sa_spi,
	    ((src == NULL) ? (zeroes) : ((uint8_t *)&src->sin_addr)),
	    (uint8_t *)&dst->sin_addr, sizeof (dst->sin_addr));
	if (target == NULL) {
		target = sadb_getassocbyspi(inbound, assoc->sadb_sa_spi,
		    ((src == NULL) ? (zeroes) : ((uint8_t *)&src->sin_addr)),
		    (uint8_t *)&dst->sin_addr, sizeof (dst->sin_addr));
	}
	if (target == NULL) {
		/* It's neither outbound nor inbound.  Check larval... */
		target = sadb_getassocbyspi(&esp_larval_list_v4,
		    assoc->sadb_sa_spi,
		    ((src == NULL) ? (zeroes) : ((uint8_t *)&src->sin_addr)),
		    (uint8_t *)&dst->sin_addr, sizeof (dst->sin_addr));
	}

	mutex_exit(&inbound->isaf_lock);
	mutex_exit(&outbound->isaf_lock);
	mutex_exit(&esp_larval_list_v4.isaf_lock);
	if (target == NULL)
		return (ESRCH);

	ASSERT(mp->b_cont != NULL);
	sadb_pfkey_echo(esp_pfkey_q, mp, (sadb_msg_t *)mp->b_cont->b_rptr,
	    ksi, target);
	IPSA_REFRELE(target);
	return (0);
}

/*
 * Flush out all of the ESP security associations and send the appropriate
 * return message.
 */
static void
esp_flush(mblk_t *mp, keysock_in_t *ksi)
{
	/*
	 * Flush out each bucket, one at a time.  Were it not for keysock's
	 * enforcement, there would be a subtlety where I could add on the
	 * heels of a flush.  With keysock's enforcement, however, this
	 * makes ESP's job easy.
	 */

	sadb_flush(&esp_larval_list_v4, 1);
	sadb_flush(&esp_larval_list_v6, 1);
	sadb_flush(esp_outbound_assoc_v4, OUTBOUND_BUCKETS);
	sadb_flush(esp_outbound_assoc_v6, OUTBOUND_BUCKETS);
	sadb_flush(esp_inbound_assoc_v4, INBOUND_BUCKETS);
	sadb_flush(esp_inbound_assoc_v6, INBOUND_BUCKETS);

	/*
	 * And while we're at it, destroy all outstanding ACQUIRE requests,
	 * too.  Just don't nuke the bucket mutexes (thus the B_FALSE for the
	 * third argument).
	 */
	sadb_destroy_acqlist(esp_acquires_v4, OUTBOUND_BUCKETS, B_FALSE);
	sadb_destroy_acqlist(esp_acquires_v6, OUTBOUND_BUCKETS, B_FALSE);

	ASSERT(mp->b_cont != NULL);
	sadb_pfkey_echo(esp_pfkey_q, mp, (sadb_msg_t *)mp->b_cont->b_rptr, ksi,
	    NULL);
}

/*
 * Convert the entire contents of all of ESP's SA tables into PF_KEY SADB_DUMP
 * messages.
 */
static void
esp_dump(mblk_t *mp, keysock_in_t *ksi)
{
	int error;
	sadb_msg_t *samsg;

	/*
	 * Dump each fanout, bailing if errno is non-zero.
	 */

	error = sadb_dump(esp_pfkey_q, mp, ksi->ks_in_serial,
	    &esp_larval_list_v4, 1, B_TRUE);
	if (error != 0)
		goto bail;

	error = sadb_dump(esp_pfkey_q, mp, ksi->ks_in_serial,
	    &esp_larval_list_v6, 1, B_TRUE);
	if (error != 0)
		goto bail;

	error = sadb_dump(esp_pfkey_q, mp, ksi->ks_in_serial,
	    esp_outbound_assoc_v4, OUTBOUND_BUCKETS, B_TRUE);
	if (error != 0)
		goto bail;

	error = sadb_dump(esp_pfkey_q, mp, ksi->ks_in_serial,
	    esp_outbound_assoc_v6, OUTBOUND_BUCKETS, B_TRUE);
	if (error != 0)
		goto bail;

	error = sadb_dump(esp_pfkey_q, mp, ksi->ks_in_serial,
	    esp_inbound_assoc_v4, INBOUND_BUCKETS, B_FALSE);
	if (error != 0)
		goto bail;

	error = sadb_dump(esp_pfkey_q, mp, ksi->ks_in_serial,
	    esp_inbound_assoc_v6, INBOUND_BUCKETS, B_FALSE);

bail:
	ASSERT(mp->b_cont != NULL);
	samsg = (sadb_msg_t *)mp->b_cont->b_rptr;
	samsg->sadb_msg_errno = (uint8_t)error;
	sadb_pfkey_echo(esp_pfkey_q, mp, (sadb_msg_t *)mp->b_cont->b_rptr, ksi,
	    NULL);
}

/*
 * ESP parsing of PF_KEY messages.  Keysock did most of the really silly
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
esp_parse_pfkey(mblk_t *mp)
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
		errno = esp_add_sa(mp, ksi);
		if (errno != 0) {
			sadb_pfkey_error(esp_pfkey_q, mp, errno,
			    ksi->ks_in_serial);
		}
		/* else esp_add_sa() took care of things. */
		break;
	case SADB_DELETE:
		errno = esp_del_sa(mp, ksi);
		if (errno != 0) {
			sadb_pfkey_error(esp_pfkey_q, mp, errno,
			    ksi->ks_in_serial);
		}
		/* Else esp_del_sa() took care of things. */
		break;
	case SADB_GET:
		errno = esp_get_sa(mp, ksi);
		if (errno != 0) {
			sadb_pfkey_error(esp_pfkey_q, mp, errno,
			    ksi->ks_in_serial);
		}
		/* Else esp_get_sa() took care of things. */
		break;
	case SADB_FLUSH:
		esp_flush(mp, ksi);
		/* esp_flush will take care of the return message, etc. */
		break;
	case SADB_REGISTER:
		/*
		 * Hmmm, let's do it!  Check for extensions (there should
		 * be none), extract the fields, call esp_register_out(),
		 * then either free or report an error.
		 *
		 * Keysock takes care of the PF_KEY bookkeeping for this.
		 */
		if (esp_register_out(samsg->sadb_msg_seq, samsg->sadb_msg_pid,
		    ksi->ks_in_serial)) {
			freemsg(mp);
		} else {
			/*
			 * Only way this path hits is if there is a memory
			 * failure.  It will not return B_FALSE because of
			 * lack of esp_pfkey_q if I am in wput().
			 */
			sadb_pfkey_error(esp_pfkey_q, mp, ENOMEM,
			    ksi->ks_in_serial);
		}
		break;
	case SADB_UPDATE:
		/*
		 * Find a larval, if not there, find a full one and get
		 * strict.
		 */
		errno = esp_update_sa(mp, ksi);
		if (errno != 0)
			sadb_pfkey_error(esp_pfkey_q, mp, errno,
			    ksi->ks_in_serial);
		/* else esp_update_sa() took care of things. */
		break;
	case SADB_GETSPI:
		/*
		 * Reserve a new larval entry.
		 */
		esp_getspi(mp, ksi);
		break;
	case SADB_ACQUIRE:
		/*
		 * Find larval and/or ACQUIRE record and kill it (them), I'm
		 * most likely an error.  Inbound ACQUIRE messages should only
		 * have the base header.
		 */
		sadb_in_acquire(samsg, esp_acquires_v4, esp_acquires_v6,
		    OUTBOUND_BUCKETS, esp_pfkey_q);
		freemsg(mp);
		break;
	case SADB_DUMP:
		/*
		 * Dump all entries.
		 */
		esp_dump(mp, ksi);
		/* esp_dump will take care of the return message, etc. */
		break;
	case SADB_EXPIRE:
		/* Should never reach me. */
		sadb_pfkey_error(esp_pfkey_q, mp, EOPNOTSUPP,
		    ksi->ks_in_serial);
		break;
	default:
		sadb_pfkey_error(esp_pfkey_q, mp, EINVAL, ksi->ks_in_serial);
		break;
	}
}


/*
 * See if the authentication algorithm thought the key was weak or not.
 */
static void
esp_auth_keycheck_ret(mblk_t *mp)
{
	auth_keycheck_t *akc;
	keysock_in_t *ksi;
	mblk_t *keycheck_mp;
	int errno;

	akc = (auth_keycheck_t *)mp->b_rptr;
	errno = akc->auth_keycheck_errno;
	keycheck_mp = mp;
	mp = keycheck_mp->b_cont;
	ASSERT(mp != NULL);
	keycheck_mp->b_cont = NULL;
	ksi = (keysock_in_t *)mp->b_rptr;
	ASSERT(mp->b_cont != NULL);

	freeb(keycheck_mp);


	if (errno != 0) {
		sadb_pfkey_error(esp_pfkey_q, mp, errno, ksi->ks_in_serial);
		return;
	}
}

/*
 * Handle case where PF_KEY says it can't find a keysock for one of my
 * ACQUIRE messages.
 */
static void
esp_keysock_no_socket(mblk_t *mp)
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
		 * Use the write-side of the esp_pfkey_q, in case there is
		 * no esp_ip_q.
		 */
		sadb_in_acquire(samsg, esp_acquires_v4, esp_acquires_v6,
		    OUTBOUND_BUCKETS, WR(esp_pfkey_q));
	}

	freemsg(mp);
}

/*
 * ESP module write put routine.
 */
static void
ipsecesp_wput(queue_t *q, mblk_t *mp)
{
	ipsec_info_t *ii;
	keysock_in_t *ksi;
	int rc;
	struct iocblk *iocp;

	esp3dbg(("In esp_wput().\n"));

	/* NOTE: Each case must take care of freeing or passing mp. */
	switch (mp->b_datap->db_type) {
	case M_CTL:
		ASSERT((mp->b_wptr - mp->b_rptr) >= sizeof (ipsec_info_t));
		ii = (ipsec_info_t *)mp->b_rptr;

		switch (ii->ipsec_info_type) {
		case AUTH_PI_HELLO:
			/* Assign this instance an algorithm type. */
			esp_new_alg(q, mp);
			break;
		case AUTH_PI_OUT_AUTH_ACK:
			esp_authout_done_v4(q, mp);
			break;
		case AUTH_PI_IN_AUTH_ACK:
			esp_authin_done_v4(q, mp);
			break;
		case AUTH_PI_KEY_ACK:
			esp_auth_keycheck_ret(mp);
			break;
		case KEYSOCK_OUT_ERR:
			esp1dbg(("Got KEYSOCK_OUT_ERR message.\n"));
			esp_keysock_no_socket(mp);
			break;
		case KEYSOCK_IN:
			/*
			 * XXX IPv6 :  What about IPv6 ire_t and IRE_DB_REQ
			 * in an IPv6 world?
			 */
			esp_keysock_in++;
			esp3dbg(("Got KEYSOCK_IN message.\n"));
			ksi = (keysock_in_t *)ii;
			/*
			 * Some common reality checks.
			 */

			if ((ksi->ks_in_extv[SADB_EXT_PROPOSAL] != NULL) ||
			    (ksi->ks_in_extv[SADB_EXT_SUPPORTED_AUTH]
				!= NULL) ||
			    (ksi->ks_in_extv[SADB_EXT_SUPPORTED_ENCRYPT]
				!= NULL) ||
			    (ksi->ks_in_srctype == KS_IN_ADDR_MBCAST) ||
			    (ksi->ks_in_dsttype == KS_IN_ADDR_UNSPEC) ||
			    ((ksi->ks_in_srctype == KS_IN_ADDR_NOTME) &&
				(ksi->ks_in_dsttype == KS_IN_ADDR_NOTME))) {
				/*
				 * An inbound PROPOSAL that reaches ESP?
				 *	(Sure this is legal for keysock, but
				 *	 it shouldn't reach here.)
				 * Source address of multi/broad cast?
				 * Source AND dest addresses not me?
				 *	(XXX What about proxy?)
				 * Dest address of unspec?
				 */
				sadb_pfkey_error(esp_pfkey_q, mp, EINVAL,
				    ksi->ks_in_serial);
				return;
			}

			/*
			 * Use 'q' instead of esp_ip_q, since it's the write
			 * side already, and it'll go down to IP.  Use
			 * esp_pfkey_q because we wouldn't get here if
			 * that weren't set, and the RD(q) has been done
			 * already.
			 */
			if (ksi->ks_in_srctype == KS_IN_ADDR_UNKNOWN) {
				rc = sadb_addrcheck(q, esp_pfkey_q, mp,
				    ksi->ks_in_extv[SADB_EXT_ADDRESS_SRC],
				    ksi->ks_in_serial);
				if (rc == KS_IN_ADDR_UNKNOWN)
					return;
				else
					ksi->ks_in_srctype = rc;
			}
			if (ksi->ks_in_dsttype == KS_IN_ADDR_UNKNOWN) {
				rc = sadb_addrcheck(q, esp_pfkey_q, mp,
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
				rc = sadb_addrcheck(q, esp_pfkey_q, mp,
				    ksi->ks_in_extv[SADB_EXT_ADDRESS_PROXY],
				    ksi->ks_in_serial);
				if (rc == KS_IN_ADDR_UNKNOWN)
					return;
				else
					ksi->ks_in_proxytype = rc;
			}
			esp_parse_pfkey(mp);
			break;
		case KEYSOCK_HELLO:
			sadb_keysock_hello(&esp_pfkey_q, q, mp, esp_ager,
			    &esp_event, SADB_SATYPE_ESP);
			break;
		default:
			esp2dbg(("Got M_CTL from above of 0x%x.\n",
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
			if (nd_getset(q, ipsecesp_g_nd, mp)) {
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
		esp3dbg(("Got default message, type %d.\n",
		    mp->b_datap->db_type));
		freemsg(mp);  /* Or putnext() to IP? */
	}
}

/*
 * Wrapper to get an association from an SPI and addresses.  Used by IP
 * for inbound datagram policy enforcement.
 */
ipsa_t *
getespassoc(mblk_t *ipsec_mp, uint8_t *src_addr, uint8_t *dst_addr,
    int length)
{
	ipsec_info_t *inf;
	ipsa_t *target;

	ASSERT(ipsec_mp->b_datap->db_type == M_CTL);

	inf = (ipsec_info_t *)ipsec_mp->b_rptr;
	ASSERT(inf->ipsec_info_type == IPSEC_IN ||
	    inf->ipsec_info_type == IPSEC_OUT);

	if (inf->ipsec_info_type == IPSEC_IN) {
		isaf_t *inbound;
		ipsec_in_t *ii = (ipsec_in_t *)inf;

		inbound = &esp_inbound_assoc_v4[INBOUND_HASH
		    (ii->ipsec_in_esp_spi)];
		mutex_enter(&inbound->isaf_lock);
		target = sadb_getassocbyspi(inbound, ii->ipsec_in_esp_spi,
		    src_addr, dst_addr, length);
		mutex_exit(&inbound->isaf_lock);

	} else if (inf->ipsec_info_type == IPSEC_OUT) {
		isaf_t *outbound;
		ipsec_out_t *oi = (ipsec_out_t *)inf;

		outbound = &esp_outbound_assoc_v4[
		    OUTBOUND_HASH(*(uint32_t *)(dst_addr))];
		mutex_enter(&outbound->isaf_lock);
		target = sadb_getassocbyspi(outbound, oi->ipsec_out_esp_spi,
		    src_addr, dst_addr, length);
		mutex_exit(&outbound->isaf_lock);

	}

	/*
	 * We could be returning an association which is dead.
	 * It means that it has not yet been reaped and soon
	 * go away. As this has been held, we are fine.
	 */

	return (target);
}

