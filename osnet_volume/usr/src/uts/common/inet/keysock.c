/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)keysock.c	1.7	99/10/21 SMI"


#include <sys/param.h>
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/stropts.h>
#include <sys/vnode.h>
#include <sys/strlog.h>
#include <sys/sysmacros.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/tiuser.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/kstr.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/proc.h>
#include <sys/suntpi.h>
#include <sys/atomic.h>
#include <sys/mkdev.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <net/pfkeyv2.h>

#include <inet/common.h>
#include <netinet/ip6.h>
#include <inet/ip.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/optcom.h>
#include <inet/ipsec_info.h>
#include <inet/keysock.h>

#include <sys/isa_defs.h>

/*
 * This is a transport provider for the PF_KEY key mangement socket.
 * (See RFC 2367 for details.)
 * Downstream messages are wrapped in a keysock consumer interface KEYSOCK_IN
 * messages (see ipsec_info.h), and passed to the appropriate consumer.
 * Upstream messages are generated for all open PF_KEY sockets, when
 * appropriate, as well as the sender (as long as SO_USELOOPBACK is enabled)
 * in reply to downstream messages.
 *
 * Upstream messages must be created asynchronously for the following
 * situations:
 *
 *	1.) A keysock consumer requires an SA, and there is currently none.
 *	2.) An SA expires, either hard or soft lifetime.
 *	3.) Other events a consumer deems fit.
 *
 * The MT model of this is PERMOD, with shared put procedures.  Two types of
 * messges, SADB_FLUSH and SADB_DUMP, need to lock down the perimeter to send
 * down the *multiple* messages they create.
 */

/* List of open PF_KEY sockets. */
static kmutex_t keysock_list_lock;  /* should I use a mutex? rwlock? */
static minor_t keysock_next_serial;  /* Also protected by list mutex */
static keysock_t *keysock_list;

/* Consumers table.  If an entry is NULL, keysock maintains the table. */
static kmutex_t keysock_consumers_lock;

#define	KEYSOCK_MAX_CONSUMERS 256
static keysock_consumer_t *keysock_consumers[KEYSOCK_MAX_CONSUMERS];

/* Default structure copied into T_INFO_ACK messages (from rts.c...) */
static struct T_info_ack keysock_g_t_info_ack = {
	T_INFO_ACK,
	T_INFINITE,	/* TSDU_size. Maximum size messages. */
	T_INVALID,	/* ETSDU_size. No expedited data. */
	T_INVALID,	/* CDATA_size. No connect data. */
	T_INVALID,	/* DDATA_size. No disconnect data. */
	0,		/* ADDR_size. */
	0,		/* OPT_size. No user-settable options */
	64 * 1024,	/* TIDU_size. keysock allows maximum size messages. */
	T_COTS,		/* SERV_type. keysock supports connection oriented. */
	TS_UNBND,	/* CURRENT_state. This is set from keysock_state. */
	(XPG4_1)	/* Provider flags */
};

/* Named Dispatch Parameter Management Structure */
typedef struct keysockpparam_s {
	uint_t	keysock_param_min;
	uint_t	keysock_param_max;
	uint_t	keysock_param_value;
	char	*keysock_param_name;
} keysockparam_t;

/*
 * Table of NDD variables supported by keysock. These are loaded into
 * keysock_g_nd in keysock_init_nd.
 * All of these are alterable, within the min/max values given, at run time.
 */
static	keysockparam_t	keysock_param_arr[] = {
	/* min	max	value	name */
	{ 4096, 65536,	8192,	"keysock_xmit_hiwat"},
	{ 0,	65536,	1024,	"keysock_xmit_lowat"},
	{ 4096, 65536,	8192,	"keysock_recv_hiwat"},
	{ 65536, 1024*1024*1024, 256*1024,	"keysock_max_buf"},
	{ 0,	3,	0,	"keysock_debug"},
};
#define	keysock_xmit_hiwat	keysock_param_arr[0].keysock_param_value
#define	keysock_xmit_lowat	keysock_param_arr[1].keysock_param_value
#define	keysock_recv_hiwat	keysock_param_arr[2].keysock_param_value
#define	keysock_max_buf		keysock_param_arr[3].keysock_param_value
#define	keysock_debug		keysock_param_arr[4].keysock_param_value

kmutex_t keysock_param_lock;	/* Protects the NDD variables. */

#define	ks0dbg(a)	printf a
/* NOTE:  != 0 instead of > 0 so lint doesn't complain. */
#define	ks1dbg(a)	if (keysock_debug != 0) printf a
#define	ks2dbg(a)	if (keysock_debug > 1) printf a
#define	ks3dbg(a)	if (keysock_debug > 2) printf a

static IDP keysock_g_nd;

/*
 * State for flush/dump.  This would normally be a boolean_t, but
 * cas32() works best for a known 32-bit quantity.
 */
static uint32_t keysock_flushdump;
static int keysock_flushdump_errno;

static int keysock_close(queue_t *);
static int keysock_open(queue_t *, dev_t *, int, int, cred_t *);
static void keysock_wput(queue_t *, mblk_t *);
static void keysock_rput(queue_t *, mblk_t *);
static void keysock_rsrv(queue_t *);
static void keysock_passup(mblk_t *, sadb_msg_t *, keysock_t *, uint_t,
    keysock_consumer_t *, boolean_t);

static struct module_info info = {
	5138, "keysock", 1, INFPSZ, 512, 128
};

static struct qinit rinit = {
	(pfi_t)keysock_rput, (pfi_t)keysock_rsrv, keysock_open, keysock_close,
	NULL, &info
};

static struct qinit winit = {
	(pfi_t)keysock_wput, NULL, NULL, NULL, NULL, &info,
	NULL, NULL, NULL, STRUIOT_STANDARD
};

struct streamtab keysockinfo = {
	&rinit, &winit
};

/*
 * Plumb IPsec.
 *
 * NOTE:  New "default" modules will need to be loaded here if needed before
 *	  boot time.
 */

/* Keep these in global space to keep the lint from complaining. */
static char *IPSECESP = "ipsecesp";
static char *IPSECAH = "ipsecah";
static char *IP6 = "ip6";
static char *KEYSOCK = "keysock";
static char *STRMOD = "strmod";
static char *authmods[] = {"authmd5h", "authsha1", NULL};
static char *encrmods[] = {"encrdes", "encr3des", NULL};

int
keysock_plumb_ipsec(void)
{
	vnode_t *vp;
	vnode_t *nvp;
	int err = 0;
	int fd, muxid;
	boolean_t esp_present = B_TRUE;
	char **modptr;
	major_t ESP_MAJ, AH_MAJ, IP6_MAJ;
	minor_t IP6_MIN = (minor_t)1;

	IP6_MAJ = ddi_name_to_major(IP6);
	AH_MAJ = ddi_name_to_major(IPSECAH);
	ESP_MAJ = ddi_name_to_major(IPSECESP);

	/*
	 * Load up the drivers (AH/ESP).
	 *
	 * I do this separately from the actual plumbing in case this function
	 * ever gets called from a diskless boot before the root filesystem is
	 * up.  I don't have to worry about "keysock" because, well, if I'm
	 * here, keysock must've loaded sucessfully.
	 */
	err = ddi_install_driver(IPSECAH);
	if (err != 0) {
		ks0dbg(("IPsec:  ddi_install_driver(AH) failed (err %d).\n",
		    err));
		return (err);
	}
	err = ddi_install_driver(IPSECESP);
	if (err != 0) {
		ks0dbg(("IPsec:  ddi_install_driver(ESP) failed (err %d).\n",
		    err));
		esp_present = B_FALSE;
	}

	/*
	 * Now do the same with the modules.  Don't return failure on
	 * these, but do report them.
	 */
	for (modptr = (char **)authmods; *modptr != NULL; modptr++) {
		/*
		 * Don't set 'err' here, because we should handle
		 * missing modules gracefully.
		 */
		if (modload(STRMOD, *modptr) < 0) {
			ks0dbg(("IPsec:  Algorithm modload(%s) failed "
			    "(err %d).\n", *modptr, err));
		}
	}

	if (esp_present) {
		for (modptr = (char **)encrmods; *modptr != NULL; modptr++) {
			/*
			 * Don't set 'err' here, because we should handle
			 * missing modules gracefully.
			 */
			if (modload(STRMOD, *modptr) < 0) {
				ks1dbg(("IPsec:  Algorithm modload(%s) failed "
				    "(err %d).\n", *modptr, err));
			}
		}
	}

	/*
	 * Set up the IP streams for AH and ESP, as well as tacking keysock
	 * on top of them.  Assume keysock has set the autopushes up already.
	 */

	/* Open IP. */
	if (err = kstr_open(IP6_MAJ, IP6_MIN, &nvp, NULL)) {
		ks0dbg(("IPsec:  Open of IP6 failed (err %d).\n", err));
		return (err);
	}

	/* PLINK KEYSOCK/AH */
	if (err = kstr_open(AH_MAJ, IP6_MIN, &vp, &fd)) {
		ks0dbg(("IPsec:  Open of AH failed (err %d).\n", err));
		goto bail;
	}
	if (err = kstr_push(vp, KEYSOCK)) {
		ks0dbg(("IPsec:  Push of KEYSOCK onto AH failed (err %d).\n",
		    err));
		(void) kstr_close(NULL, fd);
		goto bail;
	}
	if (err = kstr_plink(nvp, fd, &muxid)) {
		ks0dbg(("IPsec:  PLINK of KEYSOCK/AH failed (err %d).\n", err));
		(void) kstr_close(NULL, fd);
		goto bail;
	}
	(void) kstr_close(NULL, fd);

	/* PLINK KEYSOCK/ESP */
	if (esp_present) {
		if (err = kstr_open(ESP_MAJ, IP6_MIN, &vp, &fd)) {
			ks0dbg(("IPsec:  Open of ESP failed (err %d).\n", err));
			goto bail;
		}
		if (err = kstr_push(vp, KEYSOCK)) {
			ks0dbg(("IPsec:  "
			    "Push of KEYSOCK onto ESP failed (err %d).\n",
			    err));
			(void) kstr_close(NULL, fd);
			goto bail;
		}
		if (err = kstr_plink(nvp, fd, &muxid)) {
			ks0dbg(("IPsec:  "
			    "PLINK of KEYSOCK/ESP failed (err %d).\n", err));
			(void) kstr_close(NULL, fd);
			goto bail;
		}
	}
	(void) kstr_close(NULL, fd);

	/*
	 * Open-and-plink algorithm modules.
	 */

	for (modptr = (char **)authmods; *modptr != NULL; modptr++) {
		if (err = kstr_open(AH_MAJ, IP6_MIN, &vp, &fd)) {
			ks0dbg(("IPsec:  Open of AH failed (err %d).\n", err));
			goto bail;
		}
		if (err = kstr_push(vp, *modptr)) {
			/*
			 * If this fails, just continue, it's a missing
			 * module, most likely.
			 */
			ks0dbg(("IPsec:  Push of %s "
			    "onto AH failed (err %d).\n", *modptr, err));
		} else {
			if (err = kstr_plink(nvp, fd, &muxid)) {
				ks0dbg(("IPsec:  PLINK of %s/AH failed "
				    "(err %d).\n", *modptr, err));
				(void) kstr_close(NULL, fd);
				goto bail;
			}
		}
		(void) kstr_close(NULL, fd);
	}

	if (esp_present) {
		for (modptr = (char **)authmods; *modptr != NULL; modptr++) {
			if (err = kstr_open(ESP_MAJ, IP6_MIN, &vp, &fd)) {
				ks0dbg(("IPsec:  Open of ESP failed "
				    "(err %d).\n", err));
				goto bail;
			}
			/*
			 * Like the modload part, don't set 'err' for
			 * graceful handling of missing modules.
			 */
			if (kstr_push(vp, *modptr) != 0) {
				/*
				 * If this fails, just continue, it's a missing
				 * module, most likely.
				 */
				ks0dbg(("IPsec:  Push of %s onto ESP "
				    "failed (err %d).\n", *modptr, err));
			} else {
				if (err = kstr_plink(nvp, fd, &muxid)) {
					ks0dbg(("IPsec:  PLINK of %s/ESP failed"
					    " (err %d).\n", *modptr, err));
					(void) kstr_close(NULL, fd);
					goto bail;
				}
			}
			(void) kstr_close(NULL, fd);
		}

		for (modptr = (char **)encrmods; *modptr != NULL; modptr++) {
			if (err = kstr_open(ESP_MAJ, IP6_MIN, &vp, &fd)) {
				ks0dbg(("IPsec:  Open of ESP failed "
				    "(err %d).\n", err));
				goto bail;
			}
			/*
			 * Like the modload part, don't set 'err' for
			 * graceful handling of missing modules.
			 */
			if (kstr_push(vp, *modptr) != 0) {
				/*
				 * If this fails, just continue, it's a missing
				 * module, most likely.
				 */
				ks1dbg(("IPsec:  Push of %s onto ESP failed "
				    "(err %d).\n", *modptr, err));
			} else {
				if (err = kstr_plink(nvp, fd, &muxid)) {
					ks1dbg(("IPsec:  PLINK of %s/ESP failed"
					    " (err %d).\n", *modptr, err));
					(void) kstr_close(vp, fd);
					goto bail;
				}
			}
			(void) kstr_close(vp, fd);
		}
	}

bail:
	(void) kstr_close(nvp, -1);
	return (err);
}

/* ARGSUSED */
static int
keysock_param_get(q, mp, cp)
	queue_t	*q;
	mblk_t	*mp;
	caddr_t	cp;
{
	keysockparam_t	*keysockpa = (keysockparam_t *)cp;

	mutex_enter(&keysock_param_lock);
	(void) mi_mpprintf(mp, "%u", keysockpa->keysock_param_value);
	mutex_exit(&keysock_param_lock);
	return (0);
}

/* This routine sets an NDD variable in a keysockparam_t structure. */
/* ARGSUSED */
static int
keysock_param_set(q, mp, value, cp)
	queue_t	*q;
	mblk_t	*mp;
	char	*value;
	caddr_t	cp;
{
	char	*end;
	uint_t	new_value;
	keysockparam_t	*keysockpa = (keysockparam_t *)cp;

	/* Convert the value from a string into a long integer. */
	new_value = (uint_t)mi_strtol(value, &end, 10);

	mutex_enter(&keysock_param_lock);
	/*
	 * Fail the request if the new value does not lie within the
	 * required bounds.
	 */
	if (end == value ||
	    new_value < keysockpa->keysock_param_min ||
	    new_value > keysockpa->keysock_param_max) {
		mutex_exit(&keysock_param_lock);
		return (EINVAL);
	}

	/* Set the new value */
	keysockpa->keysock_param_value = new_value;
	mutex_exit(&keysock_param_lock);

	return (0);
}

/*
 * Initialize NDD variables, and other things, for keysock.
 */
boolean_t
keysock_ddi_init(void)
{
	keysockparam_t *ksp = keysock_param_arr;
	int count = A_CNT(keysock_param_arr);

	if (!keysock_g_nd) {
		for (; count-- > 0; ksp++) {
			if (ksp->keysock_param_name != NULL &&
			    ksp->keysock_param_name[0]) {
				if (!nd_load(&keysock_g_nd,
				    ksp->keysock_param_name,
				    keysock_param_get, keysock_param_set,
				    (caddr_t)ksp)) {
					nd_free(&keysock_g_nd);
					return (B_FALSE);
				}
			}
		}
	}

	keysock_max_optbuf_len = optcom_max_optbuf_len(
	    keysock_opt_obj.odb_opt_des_arr, keysock_opt_obj.odb_opt_arr_cnt);

	keysock_next_serial = 1;	/* Serial # 0 is special. */

	mutex_init(&keysock_list_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&keysock_consumers_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&keysock_param_lock, NULL, MUTEX_DEFAULT, NULL);

	return (B_TRUE);
}

/*
 * Free NDD variable space, and other destructors, for keysock.
 */
void
keysock_ddi_destroy(void)
{
	/* XXX Free instances? */
	ks0dbg(("keysock_ddi_destroy being called.\n"));

	mutex_destroy(&keysock_list_lock);
	mutex_destroy(&keysock_consumers_lock);
	mutex_destroy(&keysock_param_lock);
	nd_free(&keysock_g_nd);
}

/*
 * Close routine for keysock.
 */
static int
keysock_close(queue_t *q)
{
	keysock_t *ks;
	keysock_consumer_t *kc;
	void *ptr = q->q_ptr;
	int size;

	qprocsoff(q);

	/* Safe assumption. */
	ASSERT(ptr != NULL);

	if (WR(q)->q_next) {
		kc = (keysock_consumer_t *)ptr;
		ks0dbg(("Module close, removing a consumer (%d).\n",
		    kc->kc_sa_type));
		/*
		 * Because of PERMOD open/close exclusive perimeter, I
		 * can inspect KC_FLUSHING w/o locking down kc->kc_lock.
		 */
		if (kc->kc_flags & KC_FLUSHING) {
			/*
			 * If this decrment was the last one, send
			 * down the next pending one, if any.
			 *
			 * With a PERMOD perimeter, the mutexes ops aren't
			 * really necessary, but if we ever loosen up, we will
			 * have this bit covered already.
			 */
			keysock_flushdump--;
			if (keysock_flushdump == 0) {
				/*
				 * The flush/dump terminated by having a
				 * consumer go away.  I need to send up to the
				 * appropriate keysock all of the relevant
				 * information.  Unfortunately, I don't
				 * have that handy.
				 */
				ks0dbg(("Consumer went away while flushing or"
				    " dumping.\n"));
			}
		}
		size = sizeof (keysock_consumer_t);
		mutex_enter(&keysock_consumers_lock);
		keysock_consumers[kc->kc_sa_type] = NULL;
		mutex_exit(&keysock_consumers_lock);
		mutex_destroy(&kc->kc_lock);
	} else {
		ks3dbg(("Driver close, PF_KEY socket is going away.\n"));
		ks = (keysock_t *)ptr;
		size = sizeof (keysock_t);
		mutex_enter(&keysock_list_lock);
		*(ks->keysock_ptpn) = ks->keysock_next;
		if (ks->keysock_next != NULL)
			ks->keysock_next->keysock_ptpn = ks->keysock_ptpn;
		mutex_exit(&keysock_list_lock);
		mutex_destroy(&ks->keysock_lock);
	}

	/* Now I'm free. */
	kmem_free(ptr, size);
	return (0);
}

/*
 * Open routine for keysock.
 */
/* ARGSUSED */
static int
keysock_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	boolean_t ispriv = (drv_priv(credp) == 0);
	keysock_t *ks;
	keysock_consumer_t *kc;
	mblk_t *mp;
	ipsec_info_t *ii;
	keysock_t **ptpn = &keysock_list;
	extern kmutex_t ipsec_loader_lock;
	extern kcondvar_t ipsec_loader_sig_cv;
	extern int ipsec_loader_sig;

	ks3dbg(("Entering keysock open.\n"));

	if (!ispriv) {
		/*
		 * Perhaps dumping more user information?
		 *
		 * Also, don't worry about denial-of-service here because
		 * this case should never execute unless the /devices entry
		 * for keysock has been altered from the 0600 we ship with.
		 */
		(void) strlog(info.mi_idnum, 0, 0,
		    SL_ERROR | SL_CONSOLE | SL_WARN,
		    "Non-privileged user trying to open PF_KEY.\n");
		return (EPERM);
	}

	if (q->q_ptr)
		return (0);  /* Re-open of an already open instance. */

	if (ipsec_loader_sig == 0) {
		/*
		 * Don't worry about ipsec_failure being true here.
		 * (See ip.c).  An open of keysock should try and force
		 * the issue.  Maybe it was a transient failure.
		 */
		mutex_enter(&ipsec_loader_lock);
		if (ipsec_loader_sig == 0) {
			ipsec_loader_sig = 1;
			cv_signal(&ipsec_loader_sig_cv);
		}
		mutex_exit(&ipsec_loader_lock);
	}

	if (sflag & MODOPEN) {
		/* Initialize keysock_consumer state here. */
		kc = kmem_zalloc(sizeof (keysock_consumer_t), KM_NOSLEEP);
		if (kc == NULL)
			return (ENOMEM);
		mutex_init(&kc->kc_lock, NULL, MUTEX_DEFAULT, 0);
		kc->kc_rq = q;
		kc->kc_wq = WR(q);

		q->q_ptr = kc;
		WR(q)->q_ptr = kc;

		qprocson(q);

		/*
		 * Send down initial message to whatever I was pushed on top
		 * of asking for its consumer type.  The reply will set it.
		 */

		/* Allocate it. */
		mp = allocb(sizeof (ipsec_info_t), BPRI_HI);
		if (mp == NULL) {
			ks1dbg((
			    "keysock_open:  Cannot allocate KEYSOCK_HELLO.\n"));
			/* Do I need to set these to null? */
			q->q_ptr = NULL;
			WR(q)->q_ptr = NULL;
			mutex_destroy(&kc->kc_lock);
			kmem_free(kc, sizeof (*kc));
			return (ENOMEM);
		}

		/* If I allocated okay, putnext to what I was pushed atop. */
		mp->b_wptr += sizeof (ipsec_info_t);
		mp->b_datap->db_type = M_CTL;
		ii = (ipsec_info_t *)mp->b_rptr;
		ii->ipsec_info_type = KEYSOCK_HELLO;
		/* Length only of type/len. */
		ii->ipsec_info_len = sizeof (ii->ipsec_allu);
		ks2dbg(("Ready to putnext KEYSOCK_HELLO.\n"));
		putnext(kc->kc_wq, mp);
	} else {
		/* Initialize keysock state here. */

		ks2dbg(("Made it into PF_KEY socket open.\n"));

		ks = kmem_zalloc(sizeof (keysock_t), KM_NOSLEEP);
		if (ks == NULL)
			return (ENOMEM);
		mutex_init(&ks->keysock_lock, NULL, MUTEX_DEFAULT, 0);
		ks->keysock_rq = q;
		ks->keysock_wq = WR(q);
		ks->keysock_state = TS_UNBND;

		q->q_ptr = ks;
		WR(q)->q_ptr = ks;

		/*
		 * The receive hiwat is only looked at on the stream head
		 * queue.  Store in q_hiwat in order to return on SO_RCVBUF
		 * getsockopts.
		 */

		q->q_hiwat = keysock_recv_hiwat;

		/*
		 * The transmit hiwat/lowat is only looked at on IP's queue.
		 * Store in q_hiwat/q_lowat in order to return on
		 * SO_SNDBUF/SO_SNDLOWAT getsockopts.
		 */

		WR(q)->q_hiwat = keysock_xmit_hiwat;
		WR(q)->q_lowat = keysock_xmit_lowat;

		mutex_enter(&keysock_list_lock);
		/*
		 * New serial number.  Serial number protected by list mutex.
		 */
		ks->keysock_serial = keysock_next_serial++;
		keysock_next_serial &= MAXMIN;
		if (keysock_next_serial == 0) {
			/*
			 * Wraparound code.  For now, be naive and skip 0.
			 * A smart app would mark the lowest one allocated,
			 * and if above a certain amount, worry.
			 */
			keysock_next_serial++;
		}

		while (*ptpn != NULL &&
		    ks->keysock_serial <= keysock_list->keysock_serial) {
			/*
			 * keysock_serial always points to highest serial,
			 * and list is sorted high-low.
			 * XXX This can be a performance problem if a
			 * high-numbered keysock stays open.
			 */
			if (ks->keysock_serial == (*ptpn)->keysock_serial) {
				ks->keysock_serial =
				    ((*ptpn)->keysock_serial + 1) & MAXMIN;
				if (ks->keysock_serial == 0)
					ks->keysock_serial = 1;
				if (ks->keysock_serial == keysock_next_serial) {
					mutex_exit(&keysock_list_lock);
					return (EBUSY);
				}
			} else {
				ptpn = &((*ptpn)->keysock_next);
			}
		}

		/*
		 * Don't forget to ALSO assign the minor number
		 * to the dev_t (devp->???!!!???).  (See the end of
		 * mi_open_link() for where this code came from.)
		 */
		*devp = makedevice(getmajor(*devp), ks->keysock_serial);

		ks->keysock_next = *ptpn;
		ks->keysock_ptpn = ptpn;
		if (*ptpn != NULL)
			(*ptpn)->keysock_ptpn = &ks->keysock_next;
		*ptpn = ks;
		mutex_exit(&keysock_list_lock);
		qprocson(q);
	}

	return (0);
}

/* BELOW THIS LINE ARE ROUTINES INCLUDING AND RELATED TO keysock_wput(). */

/*
 * Copy relevant state bits.
 */
static void
keysock_copy_info(struct T_info_ack *tap, keysock_t *ks)
{
	*tap = keysock_g_t_info_ack;
	tap->CURRENT_state = ks->keysock_state;
	tap->OPT_size = keysock_max_optbuf_len;
}

/*
 * This routine responds to T_CAPABILITY_REQ messages.  It is called by
 * keysock_wput.  Much of the T_CAPABILITY_ACK information is copied from
 * keysock_g_t_info_ack.  The current state of the stream is copied from
 * keysock_state.
 */
static void
keysock_capability_req(queue_t *q, mblk_t *mp)
{
	keysock_t *ks = (keysock_t *)q->q_ptr;
	t_uscalar_t cap_bits1;
	struct T_capability_ack	*tcap;

	cap_bits1 = ((struct T_capability_req *)mp->b_rptr)->CAP_bits1;

	mp = tpi_ack_alloc(mp, sizeof (struct T_capability_ack),
		mp->b_datap->db_type, T_CAPABILITY_ACK);
	if (mp == NULL)
		return;

	tcap = (struct T_capability_ack *)mp->b_rptr;
	tcap->CAP_bits1 = 0;

	if (cap_bits1 & TC1_INFO) {
		keysock_copy_info(&tcap->INFO_ack, ks);
		tcap->CAP_bits1 |= TC1_INFO;
	}

	qreply(q, mp);
}

/*
 * This routine responds to T_INFO_REQ messages. It is called by
 * keysock_wput_other.
 * Most of the T_INFO_ACK information is copied from keysock_g_t_info_ack.
 * The current state of the stream is copied from keysock_state.
 */
static void
keysock_info_req(q, mp)
	queue_t	*q;
	mblk_t	*mp;
{
	mp = tpi_ack_alloc(mp, sizeof (struct T_info_ack), M_PCPROTO,
	    T_INFO_ACK);
	if (mp == NULL)
		return;
	keysock_copy_info((struct T_info_ack *)mp->b_rptr,
	    (keysock_t *)q->q_ptr);
	qreply(q, mp);
}

/*
 * keysock_err_ack. This routine creates a
 * T_ERROR_ACK message and passes it
 * upstream.
 */
static void
keysock_err_ack(q, mp, t_error, sys_error)
	queue_t	*q;
	mblk_t	*mp;
	int	t_error;
	int	sys_error;
{
	if ((mp = mi_tpi_err_ack_alloc(mp, t_error, sys_error)) != NULL)
		qreply(q, mp);
}

/*
 * This routine retrieves the current status of socket options.
 * It returns the size of the option retrieved.
 */
/* ARGSUSED */
int
keysock_opt_get(queue_t *q, t_uscalar_t level, t_uscalar_t name, uchar_t *ptr)
{
	int *i1 = (int *)ptr;
	keysock_t *ks = (keysock_t *)q->q_ptr;

	switch (level) {
	case SOL_SOCKET:
		mutex_enter(&ks->keysock_lock);
		switch (name) {
		case SO_TYPE:
			*i1 = SOCK_RAW;
			break;
		case SO_USELOOPBACK:
			*i1 = (int)(!((ks->keysock_flags & KEYSOCK_NOLOOP) ==
			    KEYSOCK_NOLOOP));
			break;
		/*
		 * The following two items can be manipulated,
		 * but changing them should do nothing.
		 */
		case SO_SNDBUF:
			*i1 = (int)q->q_hiwat;
			break;
		case SO_RCVBUF:
			*i1 = (int)(RD(q)->q_hiwat);
			break;
		}
		mutex_exit(&ks->keysock_lock);
		break;
	default:
		return (0);
	}
	return (sizeof (int));
}

/*
 * This routine sets socket options.
 */
/* ARGSUSED */
int
keysock_opt_set(queue_t *q, t_uscalar_t mgmt_flags, t_uscalar_t level,
    t_uscalar_t name, t_uscalar_t inlen, uchar_t *invalp, t_uscalar_t *outlenp,
    uchar_t *outvalp)
{
	int *i1 = (int *)invalp;
	keysock_t *ks = (keysock_t *)q->q_ptr;

	switch (level) {
	case SOL_SOCKET:
		mutex_enter(&ks->keysock_lock);
		switch (name) {
		case SO_USELOOPBACK:
			if (!(*i1))
				ks->keysock_flags |= KEYSOCK_NOLOOP;
			else ks->keysock_flags &= ~KEYSOCK_NOLOOP;
			break;
		case SO_SNDBUF:
			if (*i1 > keysock_max_buf)
				return (ENOBUFS);
			q->q_hiwat = *i1;
			q->q_next->q_hiwat = *i1;
			break;
		case SO_RCVBUF:
			if (*i1 > keysock_max_buf)
				return (ENOBUFS);
			RD(q)->q_hiwat = *i1;
			(void) mi_set_sth_hiwat(RD(q), *i1);
			break;
		}
		mutex_exit(&ks->keysock_lock);
		break;
	}
	return (0);
}

/*
 * Handle STREAMS messages.
 */
static void
keysock_wput_other(queue_t *q, mblk_t *mp)
{
	struct iocblk *iocp;

	switch (mp->b_datap->db_type) {
	case M_PROTO:
	case M_PCPROTO:
		if ((mp->b_wptr - mp->b_rptr) < sizeof (long)) {
			ks3dbg((
			    "keysock_wput_other: Not big enough M_PROTO\n"));
			freemsg(mp);
			return;
		}
		switch (((union T_primitives *)mp->b_rptr)->type) {
		case T_CAPABILITY_REQ:
			keysock_capability_req(q, mp);
			return;
		case T_INFO_REQ:
			keysock_info_req(q, mp);
			return;
		case T_SVR4_OPTMGMT_REQ:
			svr4_optcom_req(q, mp, B_TRUE, &keysock_opt_obj);
			return;
		case T_OPTMGMT_REQ:
			tpi_optcom_req(q, mp, B_TRUE, &keysock_opt_obj);
			return;
		case T_DATA_REQ:
		case T_EXDATA_REQ:
		case T_ORDREL_REQ:
			/* Illegal for keysock. */
			freemsg(mp);
			(void) putnextctl1(RD(q), M_ERROR, EPROTO);
			return;
		default:
			/* Not supported by keysock. */
			keysock_err_ack(q, mp, TNOTSUPPORT, 0);
			return;
		}
	case M_IOCTL:
		iocp = (struct iocblk *)mp->b_rptr;
		switch (iocp->ioc_cmd) {
		case ND_SET:
		case ND_GET:
			if (nd_getset(q, keysock_g_nd, mp)) {
				qreply(q, mp);
				return;
			} else iocp->ioc_error = ENOENT;
		default:
			/* Return EINVAL */
			if (iocp->ioc_error != ENOENT)
				iocp->ioc_error = EINVAL;
			iocp->ioc_count = 0;
			mp->b_datap->db_type = M_IOCACK;
			qreply(q, mp);
			return;
		}
	}

	/* If fell through, just black-hole the message. */
	freemsg(mp);
}

/*
 * Transmit a PF_KEY error message to the instance either pointed to
 * by ks, the instance with serial number serial, or more, depending.
 *
 * The faulty message (or a reasonable facsimile thereof) is in mp.
 * This function will free mp or recycle it for delivery, thereby causing
 * the stream head to free it.
 */
static void
keysock_error(keysock_t *ks, mblk_t *mp, int errno)
{
	sadb_msg_t *samsg = (sadb_msg_t *)mp->b_rptr;

	ASSERT(mp->b_datap->db_type == M_DATA);

	if (samsg->sadb_msg_type < SADB_GETSPI ||
	    samsg->sadb_msg_type > SADB_MAX)
		samsg->sadb_msg_type = SADB_RESERVED;

	/*
	 * Strip out extension headers.
	 */
	ASSERT(mp->b_rptr + sizeof (*samsg) <= mp->b_datap->db_lim);
	mp->b_wptr = mp->b_rptr + sizeof (*samsg);
	samsg->sadb_msg_len = SADB_8TO64(sizeof (sadb_msg_t));
	samsg->sadb_msg_errno = (uint8_t)errno;

	keysock_passup(mp, samsg, ks, 0, NULL, B_FALSE);
}

/*
 * Pass down a message to a consumer.  Wrap it in KEYSOCK_IN, and copy
 * in the extv if passed in.
 */
static void
keysock_passdown(keysock_t *ks, mblk_t *mp, uint8_t satype, sadb_ext_t *extv[],
    boolean_t flushmsg)
{
	keysock_consumer_t *kc;
	mblk_t *wrapper;
	keysock_in_t *ksi;
	int i;

	wrapper = allocb(sizeof (ipsec_info_t), BPRI_HI);
	if (wrapper == NULL) {
		ks3dbg(("keysock_passdown: allocb failed.\n"));
		if (extv[SADB_EXT_KEY_ENCRYPT] != NULL)
			bzero(extv[SADB_EXT_KEY_ENCRYPT],
			    SADB_64TO8(
				extv[SADB_EXT_KEY_ENCRYPT]->sadb_ext_len));
		if (extv[SADB_EXT_KEY_AUTH] != NULL)
			bzero(extv[SADB_EXT_KEY_AUTH],
			    SADB_64TO8(
				extv[SADB_EXT_KEY_AUTH]->sadb_ext_len));
		if (flushmsg) {
			ks0dbg((
			    "keysock: Downwards flush/dump message failed!\n"));
			/* If this is true, I hold the perimeter. */
			keysock_flushdump--;
		}
		freemsg(mp);
		return;
	}

	wrapper->b_datap->db_type = M_CTL;
	ksi = (keysock_in_t *)wrapper->b_rptr;
	ksi->ks_in_type = KEYSOCK_IN;
	ksi->ks_in_len = sizeof (keysock_in_t);
	if (extv[SADB_EXT_ADDRESS_SRC] != NULL)
		ksi->ks_in_srctype = KS_IN_ADDR_UNKNOWN;
	else ksi->ks_in_srctype = KS_IN_ADDR_NOTTHERE;
	if (extv[SADB_EXT_ADDRESS_DST] != NULL)
		ksi->ks_in_dsttype = KS_IN_ADDR_UNKNOWN;
	else ksi->ks_in_dsttype = KS_IN_ADDR_NOTTHERE;
	if (extv[SADB_EXT_ADDRESS_PROXY] != NULL)
		ksi->ks_in_proxytype = KS_IN_ADDR_UNKNOWN;
	else ksi->ks_in_proxytype = KS_IN_ADDR_NOTTHERE;
	for (i = 0; i <= SADB_EXT_MAX; i++)
		ksi->ks_in_extv[i] = extv[i];
	ksi->ks_in_serial = ks->keysock_serial;
	wrapper->b_wptr += sizeof (ipsec_info_t);
	wrapper->b_cont = mp;

	/*
	 * Find the appropriate consumer where the message is passed down.
	 */
	kc = keysock_consumers[satype];
	if (kc == NULL) {
		keysock_error(ks, mp, EINVAL);
		if (flushmsg) {
			ks0dbg((
			    "keysock: Downwards flush/dump message failed!\n"));
			/* If this is true, I hold the perimeter. */
			keysock_flushdump--;
		}
		return;
	}

	/*
	 * NOTE: There used to be code in here to spin while a flush or
	 *	 dump finished.  Keysock now assumes that consumers have enough
	 *	 MT-savviness to deal with that.
	 */

	/*
	 * Current consumers (AH and ESP) are guaranteed to return a
	 * FLUSH or DUMP message back, so when we reach here, we don't
	 * have to worry about keysock_flushdumps.
	 */

	putnext(kc->kc_wq, wrapper);
}

/*
 * High-level reality checking of extensions.
 */
static boolean_t
ext_check(sadb_ext_t *ext)
{
	int i;
	uint64_t *lp;
	sadb_ident_t *id;
	char *idstr;

	switch (ext->sadb_ext_type) {
	case SADB_EXT_ADDRESS_SRC:
	case SADB_EXT_ADDRESS_DST:
	case SADB_EXT_ADDRESS_PROXY:
		/* Check for at least enough addtl length for a sockaddr. */
		if (ext->sadb_ext_len <= SADB_8TO64(sizeof (sadb_address_t)))
			return (B_FALSE);
		break;
	case SADB_EXT_LIFETIME_HARD:
	case SADB_EXT_LIFETIME_SOFT:
	case SADB_EXT_LIFETIME_CURRENT:
		if (ext->sadb_ext_len != SADB_8TO64(sizeof (sadb_lifetime_t)))
			return (B_FALSE);
		break;
	case SADB_EXT_SPIRANGE:
		/* See if the SPI range is legit. */
		if (htonl(((sadb_spirange_t *)ext)->sadb_spirange_min) >
		    htonl(((sadb_spirange_t *)ext)->sadb_spirange_max))
			return (B_FALSE);
		break;
	case SADB_EXT_KEY_AUTH:
	case SADB_EXT_KEY_ENCRYPT:
		/* Key length check. */
		if (((sadb_key_t *)ext)->sadb_key_bits == 0)
			return (B_FALSE);
		/*
		 * Check to see if the key length (in bits) is less than the
		 * extension length (in 8-bits words).
		 */
		if ((roundup(SADB_1TO8(((sadb_key_t *)ext)->sadb_key_bits), 8) +
		    sizeof (sadb_key_t)) != SADB_64TO8(ext->sadb_ext_len)) {
			ks1dbg((
			    "ext_check:  Key bits/length inconsistent.\n"));
			ks1dbg(("%d bits, len is %d bytes.\n",
			    ((sadb_key_t *)ext)->sadb_key_bits,
			    SADB_64TO8(ext->sadb_ext_len)));
			return (B_FALSE);
		}

		/* All-zeroes key check. */
		lp = (uint64_t *)(((char *)ext) + sizeof (sadb_key_t));
		for (i = 0;
		    i < (ext->sadb_ext_len - SADB_8TO64(sizeof (sadb_key_t)));
		    i++)
			if (lp[i] != 0)
				break;	/* Out of for loop. */
		/* If finished the loop naturally, it's an all zero key. */
		if (lp[i] == 0)
			return (B_FALSE);
		break;
	case SADB_EXT_IDENTITY_SRC:
	case SADB_EXT_IDENTITY_DST:
		/*
		 * Make sure the strings in these identities are
		 * null-terminated.  RFC 2367 underspecified how to handle
		 * such a case.  I "proactively" null-terminate the string
		 * at the last byte if it's not terminated sooner.
		 */
		id = (sadb_ident_t *)ext;
		i = SADB_64TO8(id->sadb_ident_len);
		i -= sizeof (sadb_ident_t);
		idstr = (char *)(id + 1);
		while (*idstr != '\0' && i > 0) {
			i--;
			idstr++;
		}
		if (i == 0) {
			/*
			 * I.e., if the bozo user didn't NULL-terminate the
			 * string...
			 */
			idstr--;
			*idstr = '\0';
		}
		break;
	}
	return (B_TRUE);	/* For now... */
}

/* Return values for keysock_get_ext(). */
#define	KGE_OK	0
#define	KGE_DUP	1
#define	KGE_UNK	2
#define	KGE_LEN	3
#define	KGE_CHK	4

/*
 * Parse basic extension headers and return in the passed-in pointer vector.
 * Return values include:
 *
 *	KGE_OK	Everything's nice and parsed out.
 *		If there are no extensions, place NULL in extv[0].
 *	KGE_DUP	There is a duplicate extension.
 *		First instance in appropriate bin.  First duplicate in
 *		extv[0].
 *	KGE_UNK	Unknown extension type encountered.  extv[0] contains
 *		unknown header.
 *	KGE_LEN	Extension length error.
 *	KGE_CHK	High-level reality check failed on specific extension.
 *
 * My apologies for some of the pointer arithmetic in here.  I'm thinking
 * like an assembly programmer, yet trying to make the compiler happy.
 */
static int
keysock_get_ext(sadb_ext_t *extv[], sadb_msg_t *basehdr, uint_t msgsize)
{
	int i;

	/* This is faster than bzero(). */
	for (i = 1; i <= SADB_EXT_MAX; i++)
		extv[i] = NULL;

	/* Use extv[0] as the "current working pointer". */

	extv[0] = (sadb_ext_t *)(basehdr + 1);

	while (extv[0] < (sadb_ext_t *)(((uint8_t *)basehdr) + msgsize)) {
		/* Check for unknown headers. */
		if (extv[0]->sadb_ext_type == 0 ||
		    extv[0]->sadb_ext_type > SADB_EXT_MAX)
			return (KGE_UNK);

		/*
		 * Check length.  Use uint64_t because extlen is in units
		 * of 64-bit words.  If length goes beyond the msgsize,
		 * return an error.  (Zero length also qualifies here.)
		 */
		if (extv[0]->sadb_ext_len == 0 ||
		    (void *)((uint64_t *)extv[0] + extv[0]->sadb_ext_len) >
		    (void *)((uint8_t *)basehdr + msgsize))
			return (KGE_LEN);

		/* Check for redundant headers. */
		if (extv[extv[0]->sadb_ext_type] != NULL)
			return (KGE_DUP);

		/*
		 * Reality check the extension if possible at the keysock
		 * level.
		 */
		if (!ext_check(extv[0]))
			return (KGE_CHK);

		/* If I make it here, assign the appropriate bin. */
		extv[extv[0]->sadb_ext_type] = extv[0];

		/* Advance pointer (See above for uint64_t ptr reasoning.) */
		extv[0] = (sadb_ext_t *)
		    ((uint64_t *)extv[0] + extv[0]->sadb_ext_len);
	}

	/* Everything's cool. */

	/*
	 * If extv[0] == NULL, then there are no extension headers in this
	 * message.  Ensure that this is the case.
	 */
	if (extv[0] == (sadb_ext_t *)(basehdr + 1))
		extv[0] = NULL;

	return (KGE_OK);
}

/*
 * qwriter() callback to handle flushes and dumps.  This routine will hold
 * the inner perimeter.
 */
void
keysock_do_flushdump(queue_t *q, mblk_t *mp)
{
	int i, start, finish;
	mblk_t *mp1 = NULL;
	keysock_t *ks = (keysock_t *)q->q_ptr;
	sadb_ext_t *extv[SADB_EXT_MAX + 1];
	sadb_msg_t *samsg = (sadb_msg_t *)mp->b_rptr;

	/*
	 * I am guaranteed this will work.  I did the work in keysock_parse()
	 * already.
	 */
	(void) keysock_get_ext(extv, samsg, SADB_64TO8(samsg->sadb_msg_len));

	/*
	 * I hold the perimeter, therefore I don't need to use atomic ops.
	 */
	if (keysock_flushdump) {
		/* XXX Should I instead use EBUSY? */
		/* XXX Or is there a way to queue these up? */
		keysock_error(ks, mp, ENOMEM);
		return;
	}

	if (samsg->sadb_msg_satype == SADB_SATYPE_UNSPEC) {
		start = 0;
		finish = KEYSOCK_MAX_CONSUMERS - 1;
	} else {
		start = samsg->sadb_msg_satype;
		finish = samsg->sadb_msg_satype;
	}

	/*
	 * Fill up keysock_flushdump with the number of outstanding dumps
	 * and/or flushes.
	 */

	keysock_flushdump_errno = 0;

	/*
	 * Okay, I hold the perimeter.  Eventually keysock_flushdump will
	 * contain the number of consumers with outstanding flush operations.
	 *
	 * SO, here's the plan:
	 *	* For each relevant consumer (Might be one, might be all)
	 *		* Twiddle on the FLUSHING flag.
	 *		* Pass down the FLUSH/DUMP message.
	 *
	 * When I see upbound FLUSH/DUMP messages, I will decrement the
	 * keysock_flushdump.  When I decrement it to 0, I will pass the
	 * FLUSH/DUMP message back up to the PF_KEY sockets.  Because I will
	 * pass down the right SA type to the consumer (either its own, or
	 * that of UNSPEC), the right one will be reflected from each consumer,
	 * and accordingly back to the socket.
	 */

	mutex_enter(&keysock_consumers_lock);
	for (i = start; i <= finish; i++) {
		if (keysock_consumers[i] != NULL) {
			mp1 = copymsg(mp);
			if (mp1 == NULL) {
				ks0dbg(("SADB_FLUSH copymsg() failed.\n"));
				/*
				 * Error?  And what about outstanding
				 * flushes?  Oh, yeah, they get sucked up and
				 * the counter is decremented.  Consumers
				 * (see keysock_passdown()) are guaranteed
				 * to deliver back a flush request, even if
				 * it's an error.
				 */
				keysock_error(ks, mp, ENOMEM);
				return;
			}
			/*
			 * Because my entry conditions are met above, the
			 * following assertion should hold true.
			 */
			mutex_enter(&(keysock_consumers[i]->kc_lock));
			ASSERT((keysock_consumers[i]->kc_flags & KC_FLUSHING)
			    == 0);
			keysock_consumers[i]->kc_flags |= KC_FLUSHING;
			mutex_exit(&(keysock_consumers[i]->kc_lock));
			/* Always increment the number of flushes... */
			keysock_flushdump++;
			/* Guaranteed to return a message. */
			keysock_passdown(ks, mp1, i, extv, B_TRUE);
		} else if (start == finish) {
			/*
			 * In case where start == finish, and there's no
			 * consumer, should we force an error?  Yes.
			 */
			mutex_exit(&keysock_consumers_lock);
			keysock_error(ks, mp, EINVAL);
			return;
		}
	}
	mutex_exit(&keysock_consumers_lock);

	/* Free original message. */
	freemsg(mp);
}

/*
 * Handle PF_KEY messages.
 */
static void
keysock_parse(queue_t *q, mblk_t *mp)
{
	sadb_msg_t *samsg;
	sadb_ext_t *extv[SADB_EXT_MAX + 1];
	keysock_t *ks = (keysock_t *)q->q_ptr;
	uint_t msgsize;

	/* Make sure I'm a PF_KEY socket.  (i.e. nothing's below me) */
	ASSERT(WR(q)->q_next == NULL);

	samsg = (sadb_msg_t *)mp->b_rptr;
	ks2dbg(("Received possible PF_KEY message, type %d.\n",
	    samsg->sadb_msg_type));

	msgsize = SADB_64TO8(samsg->sadb_msg_len);

	if (msgdsize(mp) != msgsize) {
		/*
		 * Message len incorrect w.r.t. actual size.  Send an error
		 * (EMSGSIZE).	It may be necessary to massage things a
		 * bit.	 For example, if the sadb_msg_type is hosed,
		 * I need to set it to SADB_RESERVED to get delivery to
		 * do the right thing.	Then again, maybe just letting
		 * the error delivery do the right thing.
		 */
		ks2dbg(("mblk (%lu) and base (%d) message sizes don't jibe.\n",
		    msgdsize(mp), msgsize));
		keysock_error(ks, mp, EMSGSIZE);
		return;
	}

	if (msgsize > (uint_t)(mp->b_wptr - mp->b_rptr))
		/* Get all message into one mblk. */
		if (pullupmsg(mp, -1) == 0) {
			/*
			 * Something screwy happened.
			 */
			ks3dbg(("keysock_parse: pullupmsg() failed.\n"));
			return;
		} else samsg = (sadb_msg_t *)mp->b_rptr;

	switch (keysock_get_ext(extv, samsg, msgsize)) {
	case KGE_DUP:
		/* Handle duplicate extension. */
		ks1dbg(("Got duplicate extension of type %d.\n",
		    extv[0]->sadb_ext_type));
		keysock_error(ks, mp, EINVAL);
		return;
	case KGE_UNK:
		/* Handle unknown extension. */
		ks1dbg(("Got unknown extension of type %d.\n",
		    extv[0]->sadb_ext_type));
		keysock_error(ks, mp, EINVAL);
		return;
	case KGE_LEN:
		/* Length error. */
		ks1dbg(("Length %d on extension type %d overrun or 0.\n",
		    extv[0]->sadb_ext_len, extv[0]->sadb_ext_type));
		keysock_error(ks, mp, EINVAL);
		return;
	case KGE_CHK:
		/* Reality check failed. */
		ks1dbg(("Reality check failed on extension type %d.\n",
		    extv[0]->sadb_ext_type));
		keysock_error(ks, mp, EINVAL);
		return;
	default:
		/* Default case is no errors. */
		break;
	}

	switch (samsg->sadb_msg_type) {
	case SADB_REGISTER:
		/*
		 * Twiddle registered bit for whatever SA type.
		 * Then pass down.
		 *
		 * NOTE:	There's a semantic weirdness in that a message
		 *		OTHER than the return REGISTER message may
		 *		be passed up if I set the registered bit
		 *		BEFORE I pass it down.
		 *
		 *		SOOOO, I'll not twiddle it until the upbound
		 *		REGISTER (with a serial number in it)
		 */
		/* FALLTHRU */
	case SADB_GETSPI:
	case SADB_ADD:
	case SADB_UPDATE:
	case SADB_DELETE:
	case SADB_GET:
		/*
		 * Pass down to appropriate consumer.
		 */
		if (samsg->sadb_msg_satype != SADB_SATYPE_UNSPEC)
			keysock_passdown(ks, mp, samsg->sadb_msg_satype, extv,
			    B_FALSE);
		else keysock_error(ks, mp, EINVAL);
		return;
	case SADB_ACQUIRE:
		/*
		 * If I _receive_ an acquire, this means I should spread it
		 * out to registered sockets.  Unless there's an errno...
		 *
		 * Need ADDRESS, may have ID, SENS, and PROP, unless errno,
		 * in which case there should be NO extensions.
		 *
		 * Return to registered.
		 */
		if (samsg->sadb_msg_errno != 0) {
			keysock_passdown(ks, mp, samsg->sadb_msg_satype, extv,
			    B_FALSE);
		} else {
			if (samsg->sadb_msg_satype == 0)
				keysock_error(ks, mp, EINVAL);
			else keysock_passup(mp, samsg, NULL, 0, NULL, B_FALSE);
		}
		return;
	case SADB_EXPIRE:
		/*
		 * If someone sends this in, then send out to all senders.
		 * (Save maybe ESP or AH, I have to be careful here.)
		 *
		 * Need ADDRESS, may have ID and SENS.
		 *
		 * XXX for now this is unsupported.
		 */
		break;
	case SADB_FLUSH:
	case SADB_DUMP:	 /* not used by normal applications */
		/*
		 * Nuke all SAs, or dump out the whole SA table to sender only.
		 *
		 * No extensions at all.  Return to all listeners.
		 *
		 * Question:	Should I hold a lock here to prevent
		 *		additions/deletions while flushing?
		 * Answer:	No.  (See keysock_passdown() for details.)
		 */
		if (extv[0] != NULL) {
			/*
			 * FLUSH or DUMP messages shouldn't have extensions.
			 * Return EINVAL.
			 */
			ks2dbg(("FLUSH message with extension.\n"));
			keysock_error(ks, mp, EINVAL);
			return;
		}

		/* Passing down of DUMP/FLUSH messages are special. */
		qwriter(q, mp, keysock_do_flushdump, PERIM_INNER);
		return;
	case SADB_X_PROMISC:
		/*
		 * Promiscuous processing message.
		 * BTW, I still don't like the way cmetz proposes it.
		 */
		if (samsg->sadb_msg_satype == 0)
			ks->keysock_flags &= ~KEYSOCK_PROMISC;
		else ks->keysock_flags |= KEYSOCK_PROMISC;
		keysock_passup(mp, samsg, ks, 0, NULL, B_FALSE);
		return;
	default:
		ks2dbg(("Got unknown message type %d.\n",
		    samsg->sadb_msg_type));
		keysock_error(ks, mp, EINVAL);
		return;
	}

	/* As a placeholder... */
	ks0dbg(("keysock_parse():  Hit EOPNOTSUPP\n"));
	keysock_error(ks, mp, EOPNOTSUPP);
}

static void
keysock_poison_stream(queue_t *q, mblk_t *mp)
{
	uchar_t *rptr = mp->b_rptr;

	if ((mp->b_datap->db_ref > 1) || (mp->b_wptr - rptr < 1)) {
		/* Create a new M_ERROR. */
		freemsg(mp);
		mp = allocb(2, BPRI_HI);
		if (mp == NULL) {
			ks0dbg(("keysock_wput: can't alloc M_ERROR\n"));
			return;
		}
		rptr = mp->b_rptr;
	}
	mp->b_datap->db_type = M_ERROR;
	mp->b_wptr = rptr + 1;
	*rptr = EPERM;
	qreply(q, mp);
}

/*
 * wput routing for PF_KEY/keysock/whatever.  Unlike the routing socket,
 * I don't convert to ioctl()'s for IP.  I am the end-all driver as far
 * as PF_KEY sockets are concerned.  I do some conversion, but not as much
 * as IP/rts does.
 */
static void
keysock_wput(queue_t *q, mblk_t *mp)
{
	uchar_t *rptr = mp->b_rptr;
	mblk_t *mp1;

	ks3dbg(("In keysock_wput\n"));

	if (WR(q)->q_next) {
		keysock_consumer_t *kc = (keysock_consumer_t *)q->q_ptr;

		/*
		 * We shouldn't get writes on a consumer instance.  Poison
		 * the stream if we do.
		 */
		ks0dbg(("Huh?  wput for an consumer instance (%d)?\n",
		    kc->kc_sa_type));
		keysock_poison_stream(q, mp);
		return;
	}

	switch (mp->b_datap->db_type) {
	case M_DATA:
		/*
		 * I should probably reject these and poison the stream with
		 * M_ERROR.  User processes shouldn't write raw data to
		 * /dev/keysock.
		 */
		ks2dbg(("M_DATA\n"));
		keysock_poison_stream(q, mp);
		return;
	case M_PROTO:
	case M_PCPROTO:
		if ((mp->b_wptr - rptr) >= sizeof (struct T_data_req)) {
			if (((union T_primitives *)rptr)->type == T_DATA_REQ) {
				if ((mp1 = mp->b_cont) == NULL) {
					/* No data after T_DATA_REQ. */
					ks2dbg(("No data after DATA_REQ.\n"));
					freemsg(mp);
					return;
				}
				freeb(mp);
				mp = mp1;
				ks2dbg(("T_DATA_REQ\n"));
				break;	/* Out of switch. */
			}
		}
		/* FALLTHRU */
	default:
		ks3dbg(("In default wput case (%d %d).\n",
		    mp->b_datap->db_type, ((union T_primitives *)rptr)->type));
		keysock_wput_other(q, mp);
		return;
	}

	/* I now have a PF_KEY message in an M_DATA block, pointed to by mp. */
	keysock_parse(q, mp);
}

/* BELOW THIS LINE ARE ROUTINES INCLUDING AND RELATED TO keysock_rput(). */

/*
 * Called upon receipt of a KEYSOCK_HELLO_ACK to set up the appropriate
 * state vectors.
 */
static void
keysock_link_consumer(uint8_t satype, keysock_consumer_t *kc)
{
	keysock_t *ks;

	mutex_enter(&keysock_consumers_lock);
	mutex_enter(&kc->kc_lock);
	if (keysock_consumers[satype] != NULL) {
		ks0dbg((
		    "Hmmmm, someone closed %d before the HELLO_ACK happened.\n",
		    satype));
		/*
		 * Perhaps updating the new below-me consumer with what I have
		 * so far would work too?
		 */
		mutex_exit(&kc->kc_lock);
		mutex_exit(&keysock_consumers_lock);
	} else {
		/* Add new below-me consumer. */
		keysock_consumers[satype] = kc;

		kc->kc_flags = 0;
		kc->kc_sa_type = satype;
		mutex_exit(&kc->kc_lock);
		mutex_exit(&keysock_consumers_lock);

		/* Scan the keysock list. */
		mutex_enter(&keysock_list_lock);
		for (ks = keysock_list; ks != NULL; ks = ks->keysock_next) {
			if (KEYSOCK_ISREG(ks, satype)) {
				/*
				 * XXX Perhaps send an SADB_REGISTER down on
				 * the socket's behalf.
				 */
				ks1dbg(("Socket %u registered already for "
				    "new consumer.\n", ks->keysock_serial));
			}
		}
		mutex_exit(&keysock_list_lock);
	}
}

/*
 * Generate a KEYSOCK_OUT_ERR message for my consumer.
 */
static void
keysock_out_err(keysock_consumer_t *kc, int ks_errno, mblk_t *mp)
{
	keysock_out_err_t *kse;
	mblk_t *imp;

	imp = allocb(sizeof (ipsec_info_t), BPRI_HI);
	if (imp == NULL) {
		ks1dbg(("keysock_out_err:  Can't alloc message.\n"));
		return;
	}

	imp->b_datap->db_type = M_CTL;
	imp->b_wptr += sizeof (ipsec_info_t);

	kse = (keysock_out_err_t *)imp->b_rptr;
	imp->b_cont = mp;
	kse->ks_err_type = KEYSOCK_OUT_ERR;
	kse->ks_err_len = sizeof (*kse);
	/* Is serial necessary? */
	kse->ks_err_serial = 0;
	kse->ks_err_errno = ks_errno;

	/*
	 * XXX What else do I need to do here w.r.t. information
	 * to tell the consumer what caused this error?
	 *
	 * I believe the answer is the PF_KEY ACQUIRE (or other) message
	 * attached in mp, which is appended at the end.  I believe the
	 * db_ref won't matter here, because the PF_KEY message is only read
	 * for KEYSOCK_OUT_ERR.
	 */

	putnext(kc->kc_wq, imp);
}

/* XXX this is a hack errno. */
#define	EIPSECNOSA 255

/*
 * Route message (pointed by mp, header in samsg) toward appropriate
 * sockets.  Assume the message's creator did its job correctly.
 *
 * This should be a function that is followed by a return in its caller.
 * The compiler _should_ be able to use tail-call optimizations to make the
 * large ## of parameters not a huge deal.
 */
static void
keysock_passup(mblk_t *mp, sadb_msg_t *samsg, keysock_t *ks, minor_t serial,
    keysock_consumer_t *kc, boolean_t persistent)
{
	uint8_t satype = samsg->sadb_msg_satype;
	boolean_t toall = B_FALSE, reg = B_FALSE;
	mblk_t *mp1;
	keysock_t *oks;
	int err = EIPSECNOSA;

	/*
	 * If a keysock instance is known, make damned sure its serial matches
	 * the passed in serial.
	 */
	ASSERT((ks == NULL) || (serial == 0) || (ks->keysock_serial == serial));

	/* Convert mp, which is M_DATA, into an M_PROTO of type T_DATA_IND */
	mp1 = allocb(sizeof (struct T_data_req), BPRI_HI);
	if (mp1 == NULL) {
		err = ENOMEM;
		goto error;
	}
	mp1->b_wptr += sizeof (struct T_data_req);
	((struct T_data_ind *)mp1->b_rptr)->PRIM_type = T_DATA_IND;
	((struct T_data_ind *)mp1->b_rptr)->MORE_flag = 0;
	mp1->b_datap->db_type = M_PROTO;
	mp1->b_cont = mp;
	mp = mp1;

	/*
	 * XXX  Perhaps walking the keysock_list TWICE
	 * isn't such a wise idea, performance wise.  :-P
	 */
	if (ks == NULL && serial != 0) {
		mutex_enter(&keysock_list_lock);
		for (ks = keysock_list; ks != NULL; ks = ks->keysock_next)
			if (ks->keysock_serial == serial)
			    break;
		mutex_exit(&keysock_list_lock);
	}

	if (ks != NULL) {
		ks3dbg(("Have sending ks retrieved, serial %d.\n", serial));
	}

	/*
	 * At this point, if I have a NULL ks pointer, then I just deliver
	 * where appropos.  A NULL ks pointer is MOST LIKELY a message that
	 * ORIGINATE either with me, or with a consumer.
	 */

	switch (samsg->sadb_msg_type) {
	case SADB_FLUSH:
	case SADB_GETSPI:
	case SADB_UPDATE:
	case SADB_ADD:
	case SADB_DELETE:
	case SADB_EXPIRE:
		/*
		 * These are most likely replies.  Don't worry about
		 * KEYSOCK_OUT_ERR handling.  Deliver to all sockets.
		 */
		ks3dbg(("Delivering normal message (%d) to all sockets.\n",
		    samsg->sadb_msg_type));
		toall = B_TRUE;
		break;
	case SADB_REGISTER:
		/*
		 * Upbound REGISTER message with a serial number means:
		 * 1.) I have ks already set.
		 * 2.) I can set the registered bit now.
		 *
		 * When I address the twice-scanning problem, this logic
		 * will change.
		 */
		if (ks != NULL) {
			mutex_enter(&ks->keysock_lock);
			KEYSOCK_SETREG(ks, samsg->sadb_msg_satype);
			mutex_exit(&ks->keysock_lock);
		}
		/* FALLTHRU */
	case SADB_ACQUIRE:
		/*
		 * These are generated messages.  I need to worry about
		 * KEYSOCK_OUT_ERR.  Fortunately, the reg boolean doubles
		 * as a "worry about OUT_ERR" flag.
		 */
		ks3dbg(("Delivering REGISTER OR ACQUIRE (%d, actually).\n",
		    samsg->sadb_msg_type));
		ASSERT(samsg->sadb_msg_satype != 0);
		reg = B_TRUE;
		break;
	case SADB_X_PROMISC:
	case SADB_DUMP:
	case SADB_GET:
	default:
		/*
		 * Deliver to the sender and promiscuous only.
		 */
		ks3dbg(("Delivering sender/promisc only (%d).\n",
		    samsg->sadb_msg_type));
		break;
	}

	if (ks != NULL && (persistent || canputnext(ks->keysock_rq)) &&
	    !(ks->keysock_flags & KEYSOCK_NOLOOP)) {
		mp1 = dupmsg(mp);
		if (mp1 == NULL) {
			ks2dbg(("keysock_passup():  dupmsg() failed.\n"));
			mp1 = mp;
			mp = NULL;
		}
		ks3dbg(("Putting (first) to serial %d.\n", ks->keysock_serial));

		if (persistent && !canputnext(ks->keysock_rq)) {
			if (putq(ks->keysock_rq, mp1) == 0) {
				ks1dbg(("keysock_passup: putq failed.\n"));
			}
		} else {
			putnext(ks->keysock_rq, mp1);
		}
		err = 0;
		if (mp == NULL)
			goto error;
		oks = ks;
	}

	mutex_enter(&keysock_list_lock);
	for (ks = keysock_list; ks != NULL; ks = ks->keysock_next) {
		/*
		 * Deliver PF_KEY messages to non-sending sockets.  Do this
		 * if:
		 *
		 *	1. There's room (i.e. canputnext())
		 *	2. I haven't delivered it already (i.e. oks != ks)
		 *	3. The user hasn't turned off SO_USELOOPBACK
		 *	   (e.g. KEYSOCK_NOLOOP).
		 *	4. It's supposed to be delivered either to all
		 *	   PF_KEY sockets, or all REGISTERED PF_KEY sockets.
		 */
		if ((toall ||
		    (reg && KEYSOCK_ISREG(ks, satype)) ||
		    (ks->keysock_flags & KEYSOCK_PROMISC)) &&
		    !(ks->keysock_flags & KEYSOCK_NOLOOP) &&
		    oks != ks &&
		    canputnext(ks->keysock_rq)) {
			mp1 = dupmsg(mp);
			if (mp1 == NULL) {
				ks2dbg((
				    "keysock_passup():  dupmsg() failed.\n"));
				mp1 = mp;
				mp = NULL;
			}
			if (toall || (reg && KEYSOCK_ISREG(ks, satype))) {
				ks3dbg(("Legitimate deliver to serial %d.\n",
				    ks->keysock_serial));
				err = 0;
			}
			ks3dbg(("Putting to serial %d.\n", ks->keysock_serial));
			/*
			 * Unlike the specific keysock instance case, this
			 * will only hit for listeners, so we will only
			 * putnext() if we can.
			 */
			putnext(ks->keysock_rq, mp1);
			if (mp == NULL)
				break;	/* out of for loop. */
		}
	}
	mutex_exit(&keysock_list_lock);

error:
	if ((err != 0) && (kc != NULL)) {
		/*
		 * Generate KEYSOCK_OUT_ERR for consumer.
		 * Basically, I send this back if I have not been able to
		 * transmit (for whatever reason)
		 */
		ks1dbg(("keysock_passup():  No registered of type %d.\n",
		    satype));
		if (mp != NULL) {
			if (mp->b_datap->db_type == M_PROTO) {
				mp1 = mp;
				mp = mp->b_cont;
				freeb(mp1);
			}
			/*
			 * Do a copymsg() because people who get
			 * KEYSOCK_OUT_ERR may alter the message contents.
			 */
			mp1 = copymsg(mp);
			if (mp1 == NULL) {
				ks2dbg(("keysock_passup: copymsg() failed.\n"));
				mp1 = mp;
				mp = NULL;
			}
			keysock_out_err(kc, err, mp1);
		}
	}

	/*
	 * XXX Blank the message somehow.  This is difficult because we don't
	 * know at this point if the message has db_ref > 1, etc.
	 *
	 * Optimally, keysock messages containing actual keying material would
	 * be allocated with esballoc(), with a zeroing free function.
	 */
	if (mp != NULL)
		freemsg(mp);
}

/*
 * Keysock's read service procedure is there only for PF_KEY reply
 * messages that really need to reach the top.
 */
static void
keysock_rsrv(queue_t *q)
{
	mblk_t *mp;

	while ((mp = getq(q)) != NULL) {
		putnext(q, mp);
	}
}

/*
 * The read procedure should only be invoked by a keysock consumer, like
 * ESP, AH, etc.  I should only see KEYSOCK_OUT and KEYSOCK_HELLO_ACK
 * messages on my read queues.
 */
static void
keysock_rput(queue_t *q, mblk_t *mp)
{
	keysock_consumer_t *kc = (keysock_consumer_t *)q->q_ptr;
	ipsec_info_t *ii;
	keysock_hello_ack_t *ksa;
	minor_t serial;
	mblk_t *mp1;
	sadb_msg_t *samsg;

	/* Make sure I'm a consumer instance.  (i.e. something's below me) */
	ASSERT(WR(q)->q_next != NULL);

	if (mp->b_datap->db_type != M_CTL) {
		/*
		 * Keysock should only see keysock consumer interface
		 * messages (see ipsec_info.h) on its read procedure.
		 */
		ks0dbg(("Hmmm, a non M_CTL (%d, 0x%x) on keysock_rput.\n",
		    mp->b_datap->db_type, mp->b_datap->db_type));
		freemsg(mp);
		return;
	}

	ii = (ipsec_info_t *)mp->b_rptr;

	switch (ii->ipsec_info_type) {
	case KEYSOCK_OUT:
		/*
		 * A consumer needs to pass a response message or an ACQUIRE
		 * UP.  I assume that the consumer has done the right
		 * thing w.r.t. message creation, etc.
		 */
		serial = ((keysock_out_t *)mp->b_rptr)->ks_out_serial;
		mp1 = mp->b_cont;	/* Get M_DATA portion. */
		freeb(mp);
		samsg = (sadb_msg_t *)mp1->b_rptr;
		if (samsg->sadb_msg_type == SADB_FLUSH ||
		    (samsg->sadb_msg_type == SADB_DUMP &&
			samsg->sadb_msg_len == SADB_8TO64(sizeof (*samsg)))) {
			/*
			 * If I'm an end-of-FLUSH or an end-of-DUMP marker...
			 */
			ASSERT(keysock_flushdump != 0);  /* Am I flushing? */

			mutex_enter(&kc->kc_lock);
			kc->kc_flags &= ~KC_FLUSHING;
			mutex_exit(&kc->kc_lock);

			if (samsg->sadb_msg_errno != 0)
				keysock_flushdump_errno = samsg->sadb_msg_errno;

			/*
			 * Lower the atomic "flushing" count.  If it's
			 * the last one, send up the end-of-{FLUSH,DUMP} to
			 * the appropriate PF_KEY socket.
			 */
			if (atomic_add_32_nv(&keysock_flushdump, -1) != 0) {
				ks1dbg(("One flush/dump message back from %d,"
				    " more to go.\n", samsg->sadb_msg_satype));
				freemsg(mp1);
				return;
			}

			samsg->sadb_msg_errno =
			    (uint8_t)keysock_flushdump_errno;
			if (samsg->sadb_msg_type == SADB_DUMP) {
				samsg->sadb_msg_seq = 0;
			}
		}
		keysock_passup(mp1, samsg, NULL, serial, kc,
		    (samsg->sadb_msg_type == SADB_DUMP &&
			samsg->sadb_msg_seq == 0));
		return;
	case KEYSOCK_HELLO_ACK:
		/* Aha, now we can link in the consumer! */
		ksa = (keysock_hello_ack_t *)ii;
		keysock_link_consumer(ksa->ks_hello_satype, kc);
		freemsg(mp);
		return;
	default:
		ks0dbg(("Hmmm, an IPsec info I'm not used to, 0x%x\n",
		    ii->ipsec_info_type));
		freemsg(mp);
		return;
	}
}
