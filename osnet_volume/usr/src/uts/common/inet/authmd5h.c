/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)authmd5h.c	1.2	99/10/21 SMI"

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
#include <inet/ip_ire.h>
#include <inet/ipsecah.h>
#include <inet/ipsec_info.h>
#include <inet/sadb.h>
#include <sys/md5.h>

#include <sys/conf.h>
#include <sys/modctl.h>

static struct streamtab authmd5hinfo;

static struct fmodsw fsw = {
	"authmd5h",
	&authmd5hinfo,
	D_NEW|D_MP|D_MTQPAIR|D_MTPUTSHARED
};

extern struct mod_ops mod_strmodops;

static struct modlstrmod modlstrmod = {
	&mod_strmodops, "MD5-HMAC algorithm", &fsw
};

static struct modlinkage modlinkage = {
	MODREV_1, &modlstrmod, NULL
};


_init()
{
	return (mod_install(&modlinkage));
}


_fini()
{
	return (mod_remove(&modlinkage));
}


_info(modinfop)
struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

static int authmd5h_open(queue_t *, dev_t *, int, int, cred_t *);
static int authmd5h_close(queue_t *);
static void authmd5h_rput(queue_t *, mblk_t *);
static void authmd5h_wput(queue_t *, mblk_t *);

static struct module_info authmd5h_info = {
	5141, "authmd5h", 0, INFPSZ, 65536, 1024
};

static struct qinit authmd5h_rinit = {
	(pfi_t)authmd5h_rput, NULL, authmd5h_open, authmd5h_close, NULL,
	&authmd5h_info, NULL
};

static struct qinit authmd5h_winit = {
	(pfi_t)authmd5h_wput, NULL, authmd5h_open, authmd5h_close, NULL,
	&authmd5h_info, NULL
};

static struct streamtab authmd5hinfo = {
	&authmd5h_rinit, &authmd5h_winit, NULL, NULL
};

/*
 * Key length should be exactly 128 bits. We should not support
 * other key lengths.
 */
#define	MD5HMAC_MIN_BITS	128
#define	MD5HMAC_MAX_BITS	128
#define	MD5HMAC_KEY_LENGTH	16		/* MD5 key length in bytes */
#define	MD5HMAC_BLOCK_SIZE	64		/* Algorithm block size */
/*
 * We allocate 2 context structures. One contains the MD5(key, i_pad)
 * and the other contains the MD5(key, o_pad). We store this in the
 * Security Association and reuse it rather then compute it everytime.
 */
#define	MD5HMAC_AUTHMISC_LEN	(2 * sizeof (MD5_CTX))
#define	MD5_DIGEST_LENGTH	16		/* Output of MD5 hash */

/*
 * Truncated HMAC ICV length is 96 bits i.e 12 bytes.
 */
#define	MD5HMAC_DATA_LEN	12

/* ARGSUSED */
static int
authmd5h_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	mblk_t *mp;
	auth_pi_hello_t *auth_mp;

	if (q->q_ptr)
		return (0);  /* Re-open of an already open instance. */

	if (sflag != MODOPEN)
		return (EINVAL);

	/*
	 * Send down the information about us.
	 */
	if ((mp = allocb(sizeof (ipsec_info_t), BPRI_HI)) == NULL) {
		freemsg(mp);
		return (ENOMEM);
	}
	qprocson(q);

	mp->b_datap->db_type = M_CTL;
	mp->b_wptr += sizeof (ipsec_info_t);

	auth_mp = (auth_pi_hello_t *)mp->b_rptr;
	auth_mp->auth_hello_type = AUTH_PI_HELLO;
	auth_mp->auth_hello_len = sizeof (auth_pi_hello_t);
	auth_mp->auth_hello_id = SADB_AALG_MD5HMAC;
	auth_mp->auth_hello_iv = (uint8_t)0;
	auth_mp->auth_hello_minbits = MD5HMAC_MIN_BITS;
	auth_mp->auth_hello_maxbits = MD5HMAC_MAX_BITS;
	auth_mp->auth_hello_datalen = MD5HMAC_DATA_LEN;
	auth_mp->auth_hello_keycheck = B_FALSE;
	auth_mp->auth_hello_numkeys = 0;

	(void) putnext(WR(q), mp);
	return (0);
}


static int
authmd5h_close(queue_t *q)
{
	qprocsoff(q);
	return (0);
}

/*
 * This function is called once to get the padded key when a SA is
 * used for authenticating the datagram first time. We pad the keys
 * with zeroes to the right length and XOR with constants defined
 * in RFC 2104 and do a partial MD5 on the XORed key. We just do this
 * once for a given SA as this does not change.
 */
static void
md5_key(ipsa_t *assoc, uchar_t *buf)
{
	unsigned char *key;
	int keylen;
	uint32_t *ip, *op;
	uchar_t ipad[MD5HMAC_BLOCK_SIZE];
	uchar_t opad[MD5HMAC_BLOCK_SIZE];
	MD5_CTX *icontext, *ocontext;
	int i;
	int nints;

	icontext = (MD5_CTX *)buf;
	ocontext = (MD5_CTX *)(buf + sizeof (MD5_CTX));

	key = assoc->ipsa_authkey;
	keylen = assoc->ipsa_authkeylen;
	ASSERT(keylen == MD5HMAC_KEY_LENGTH);

	/*
	 * We need to append zeroes if the key length is smaller
	 * than the block size. Easy way of doing this is to bzero
	 * the buffer and copy the key into it.
	 */
	bzero(ipad, MD5HMAC_BLOCK_SIZE);
	bzero(opad, MD5HMAC_BLOCK_SIZE);
	bcopy(key, ipad, keylen);
	bcopy(key, opad, keylen);

	/*
	 * XOR key with ipad (0x36) and opad (0x5c) as defined
	 * in RFC 2104.
	 */
	ip = (uint32_t *)ipad;
	op = (uint32_t *)opad;
	nints = MD5HMAC_BLOCK_SIZE/sizeof (uint32_t);

	for (i = 0; i < nints; i++) {
		ip[i] ^= 0x36363636;
		op[i] ^= 0x5c5c5c5c;
	}
	/*
	 * Perform MD5 with ipad.
	 */
	MD5Init(icontext);		/* init context for 1st pass */
	MD5Update(icontext, ipad, MD5HMAC_BLOCK_SIZE);
	/*
	 * Perform MD5 with opad.
	 */
	MD5Init(ocontext);		/* init context for 2nd pass */
	MD5Update(ocontext, opad, MD5HMAC_BLOCK_SIZE);
}

static void
hmac_md5(mblk_t *mp)
{
	MD5_CTX *saved_contexts;
	MD5_CTX icontext;
	MD5_CTX ocontext;
	uchar_t digest[MD5_DIGEST_LENGTH];
	uchar_t tmp_buf[MD5HMAC_AUTHMISC_LEN];
	uchar_t *buf;
	int i;
	ipsa_t *assoc;
	mblk_t *auth_req_mp;
	mblk_t *ipsec_info_mp;
	mblk_t *phdr_mp;
	mblk_t *data_mp;
	mblk_t *auth_ack_mp;
	mblk_t *temp_mp;
	auth_ack_t *ack;
	auth_req_t *req;
	int phdr_length;
	unsigned int offset;

	auth_req_mp = mp;
	ipsec_info_mp = mp->b_cont;
	phdr_mp = ipsec_info_mp->b_cont;
	data_mp = phdr_mp->b_cont;

	req = (auth_req_t *)auth_req_mp->b_rptr;

	ASSERT((auth_req_mp->b_wptr - auth_req_mp->b_rptr) >=
	    sizeof (ipsec_info_t));

	assoc = req->auth_req_assoc;

	/*
	 * First get your key and then do a MD5 on the message.
	 */
	if (assoc->ipsa_authmisc == NULL) {
		buf = (uchar_t *)kmem_alloc(MD5HMAC_AUTHMISC_LEN, KM_NOSLEEP);
		if (buf != NULL) {
			/*
			 * We initialize the ipsa_authmisc here only if it
			 * is NULL. So, the caller need not hold the lock.
			 */
			md5_key(assoc, buf);
			mutex_enter(&assoc->ipsa_lock);
			if (assoc->ipsa_authmisc == NULL) {
				assoc->ipsa_authmisc = buf;
				assoc->ipsa_authmisclen = MD5HMAC_AUTHMISC_LEN;
			} else {
				/*
				 * If some other thread won the race,
				 * free our memory and honor it's victory.
				 */
				kmem_free(buf, MD5HMAC_AUTHMISC_LEN);
				buf = assoc->ipsa_authmisc;
				ASSERT(buf != NULL);
			}
			mutex_exit(&assoc->ipsa_lock);
		} else {
			buf = tmp_buf;
			md5_key(assoc, buf);
		}
	} else {
		buf = assoc->ipsa_authmisc;
	}

	saved_contexts = (MD5_CTX *)buf;
	icontext = saved_contexts[0];		/* Structure copy */
	ocontext = saved_contexts[1];		/* Structure copy */

	offset = req->auth_req_startoffset;
	phdr_length = (phdr_mp->b_wptr - phdr_mp->b_rptr);

	if (offset == 0) {
		offset = phdr_length;
	}

	/*
	 * Feed the data in two steps. First, the pseudo header
	 * and then the actual data. We may have a null pseudo
	 * header for some cases like ESP.
	 */
	if (phdr_length != 0)
		MD5Update(&icontext, phdr_mp->b_rptr, phdr_length);

	/*
	 * Get to the right offset in the message
	 */
	temp_mp = data_mp;
	if (offset != 0) {
		int len = 0;
		int save_len = 0;
		uchar_t *ptr;

		while (temp_mp != NULL) {
			len += temp_mp->b_wptr - temp_mp->b_rptr;
			if (offset < len) {
				ptr = temp_mp->b_rptr + (offset - save_len);
				MD5Update(&icontext, ptr, (len - offset));
				temp_mp = temp_mp->b_cont;
				break;
			}
			save_len = len;
			temp_mp = temp_mp->b_cont;
		}
	}

	for (; temp_mp != NULL; temp_mp = temp_mp->b_cont) {
		MD5Update(&icontext, temp_mp->b_rptr,
		    (temp_mp->b_wptr - temp_mp->b_rptr));
	}

	MD5Final(digest, &icontext);

	/*
	 * Perform MD5(K XOR OPAD, DIGEST), where DIGEST is the
	 * MD5(K XOR IPAD, DATA).
	 */
	MD5Update(&ocontext, digest, MD5_DIGEST_LENGTH);
	MD5Final(digest, &ocontext);

	auth_ack_mp = auth_req_mp;
	auth_ack_mp->b_datap->db_type = M_CTL;
	ack = (auth_ack_t *)auth_ack_mp->b_rptr;

	if (req->auth_req_type == AUTH_PI_OUT_AUTH_REQ)
		ack->auth_ack_type = AUTH_PI_OUT_AUTH_ACK;
	else
		ack->auth_ack_type = AUTH_PI_IN_AUTH_ACK;

	ack->auth_ack_len = sizeof (auth_ack_t);
	ack->auth_ack_assoc = assoc;

	for (i = 0; i < MD5HMAC_DATA_LEN; i++) {
		ack->auth_ack_data[i] = digest[i];
	}
	ack->auth_ack_datalen = MD5HMAC_DATA_LEN;
}

static void
authmd5h_rput(queue_t *q, mblk_t *mp)
{
	ipsec_info_t *ii;

	switch (mp->b_datap->db_type) {

	case M_CTL:
		ii = (ipsec_info_t *)mp->b_rptr;
		switch (ii->ipsec_info_type) {
		case AUTH_PI_OUT_AUTH_REQ:
		case AUTH_PI_IN_AUTH_REQ:
			hmac_md5(mp);
			putnext(WR(q), mp);
			break;
		default:
			freemsg(mp);
			break;
		}
		break;

	default:
		freemsg(mp);
		break;
	}
}

/*
 * Poison the STREAM with M_ERROR.
 */
static void
authmd5h_wput(queue_t *q, mblk_t *mp)
{
	freemsg(mp);
	mp = allocb(2, BPRI_HI);
	if (mp == NULL) {
		(void) strlog(authmd5h_info.mi_idnum, 0, 0,
		    SL_ERROR | SL_WARN | SL_CONSOLE,
		    "authmd5h_wput: can't alloc M_ERROR\n");
		return;
	}
	mp->b_datap->db_type = M_ERROR;
	mp->b_wptr = mp->b_rptr + 1;
	*(mp->b_rptr) = EPERM;
	qreply(q, mp);
}
