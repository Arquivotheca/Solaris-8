/*
 * @(#)audit_io.c 2.13 92/02/23 SMI; SunOS CMW
 * @(#)audit_io.c 4.2.1.2 91/05/08 SMI; BSM Module
 *
 * Routines for writing audit records.
 *
 */

/*
 * Copyright (c) 1993-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)audit_io.c	1.45	98/12/18 SMI"

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/statvfs.h>	/* for statfs */
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/reboot.h>
#include <sys/kmem.h>		/* for KM_SLEEP */
#include <sys/resource.h>	/* for RLIM_INFINITY */
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>

#include <c2/audit.h>
#include <c2/audit_kernel.h>
#include <c2/audit_record.h>
#include <c2/audit_kevents.h>

/*
 * Open an audit record.
 * Find a free descriptor and pass it back.
 * For the time being, panic if the descriptor count
 * exceeds a hard maximum, we can change if this
 * becomes a problem.
 */

#define	MAX_AU_DESCRIPTORS	64

extern int au_auditstate;
extern int audit_policy;

extern kmutex_t au_open_lock;
extern kmutex_t au_stat_lock;
extern kmutex_t checktime_lock;
extern kmutex_t au_membuf_lock;
extern kcondvar_t checktime_cv;
extern kmutex_t cktime_lock;
extern int au_dont_stop;
extern id_t au_tscid;

static int au_nomb(void);
/*
 * The actual audit record queue
 */
struct audit_queue au_queue;

time_t	au_checktime;

/*
 * The audit queue staging buffer (dynamically allocated).
 */
char *au_buffer;

/*
 * Audit descriptors are indexes into au_d and au_e.
 * au_d is the list of audit tokens for this record.
 * au_e is the saved audit event for this record.
 */

#ifdef REMOVE
/*
 * event -> class translation
 */
#include "audit_etoc.h"
#define	ETOC(X) \
((int)au_etoc[((X) > sizeof (au_etoc) / sizeof (unsigned int)) ? 0 : (X)])
#else
#define	ETOC(X) 0xff
#endif


#ifdef NOTDEF
void
au_uopen()
{
	void au_until_mem_free();

	/*
	 * This will return when it's OKay for this process to have
	 * au_membufs, based on general system au_membuf availablity.
	 */
	au_until_mem_free();
	u_ad = au_open();
}
#endif /* NOTDEF */

/*
 * Write to an audit descriptor.
 * Add the au_membuf to the descriptor chain and free the chain passed in.
 */
void
au_uwrite(m)
	token_t *m;
{
	au_write(&(u_ad), m);
}

void
au_write(caddr_t *d, token_t *m)
{
	if (d == NULL) {
		au_toss_token(m);
		return;
	}
	if (m == (token_t *)0) {
		printf("au_write: null token\n");
		return;
	}

	if (*d == NULL)
		*d = (caddr_t)m;
	else
		(void) au_append_rec((au_buff_t *)*d, (au_buff_t *)m);
}

/*
 * Close an audit descriptor.
 * Use the second parameter to indicate if it should be written or not.
 */
void
au_close(caddr_t *d, int right, short e_type, short e_mod)
{
	token_t *dchain;	/* au_membuf chain which is the tokens */
	token_t *record;	/* au_membuf chain which is the record */
	int	byte_count;

	if (d == NULL)
		panic("au_close: bad record pointer");

	if ((dchain = (token_t *)*d) == (token_t *)NULL)
		return;

	*d = NULL;

	/*
	 * If not to be written toss the record
	 */
	if ((!right || (au_auditstate != AUC_AUDITING)) &&
		(e_type != AUE_SYSTEMBOOT)) {
		au_toss_token(dchain);
		return;
	}

	/*
	 * Count up the bytes used in the record.
	 */
	byte_count = au_token_size(dchain);
#ifdef NOTDEF
	if (byte_count == 0) {
		/*
		 * Null records should be unusual, at best.
		 * For the time being, they are forbidden.
		 */
		printf("au_close(%p, %s, %d): size = %d\n",
		    d, right ? "save" : "toss",
		    e_type, byte_count);
	}
#endif

	byte_count += sizeof (char) + sizeof (int32_t) +
		sizeof (char) + 2 * sizeof (short) + sizeof (hrestime);

	if (audit_policy & AUDIT_TRAIL)
		byte_count += 7;
	/*
	 * Build the header, tack on the data, then the trailer.
	 */
	record = au_to_header(byte_count, e_type, e_mod);
	(void) au_append_rec((au_buff_t *)record, (au_buff_t *)dchain);
	if (audit_policy & AUDIT_TRAIL) {
		(void) au_append_rec((au_buff_t *)record,
			(au_buff_t *)au_to_trailer(byte_count));
	}

	AS_INC(as_enqueue, 1);
	AS_INC(as_totalsize, byte_count);

	au_enqueue(record);

		/* kick reader awake if its asleep */
	mutex_enter(&au_queue.lock);
	if (au_queue.rd_block && au_queue.cnt > au_queue.lowater)
		cv_broadcast(&au_queue.read_cv);
	mutex_exit(&au_queue.lock);
}

void
au_enqueue(m)
	au_buff_t *m;
{
	mutex_enter(&au_queue.lock);
	if (au_queue.head)
		au_queue.tail->next_rec = m;
	else
		au_queue.head = m;
	au_queue.tail = m;
	au_queue.cnt++;
	mutex_exit(&au_queue.lock);
}

au_buff_t *
au_dequeue()
{
	au_buff_t *m;

	mutex_enter(&au_queue.lock);
	if (au_queue.head) {
		m = au_queue.head;
		au_queue.head = m->next_rec;
		au_queue.cnt--;
	}
	mutex_exit(&au_queue.lock);
	return (m);
}

/*
 * anything queued on audit queue.
 */
au_isqueued()
{
	if (au_queue.head)
		return (1);
	else
		return (0);
}

/*
 * audit_sync_block()
 * if we've reached the high water mark, we look at the policy to see
 * if we sleep or that we should drop the audit record.
 */
audit_sync_block()
{
	mutex_enter(&au_queue.lock);	/* turn off interrupts */

	/*
	 * see if we've reached high water mark
	 */
	while (au_queue.cnt >= au_queue.hiwater) {

		/* yep, we have. Check what policy to use */
		if (audit_policy & AUDIT_CNT) {
			if (au_dont_stop || (curthread->t_cid != au_tscid)) {
				/* just count # of dropped audit records */
				AS_INC(as_dropped, 1);
				mutex_exit(&au_queue.lock);
				return (1);
			}
		}

		/* kick reader awake if its asleep */
		if (au_queue.rd_block && au_queue.cnt > au_queue.lowater)
			cv_broadcast(&au_queue.read_cv);

		/* keep count of # times blocked */
		AS_INC(as_wblocked, 1);

		/* sleep now, until woken by reader */
		au_queue.wt_block++;
		cv_wait(&au_queue.write_cv, &au_queue.lock);
		au_queue.wt_block--;
	}
	mutex_exit(&au_queue.lock);
	return (0);
}

/*
 * audit_async_block()
 * if we've reached the high water mark, we look at the policy to see
 * if we reboot that we should drop the audit record.
 */
audit_async_block()

{	/* AUDIT_ASYNC_BLOCK */
	mutex_enter(&au_queue.lock);	/* turn of interrupts */

	/*
	 * see if we've reached high water mark
	 */
	while (au_queue.cnt >= au_queue.hiwater) {

		/* yep, we have. Check what policy to use */
		if ((audit_policy & AUDIT_AHLT) == 0) {
			/* just count # of dropped audit records */
			AS_INC(as_dropped, 1);
			mutex_exit(&au_queue.lock);
			return (1);
		}
		/*
		 * There can be a lot of data in the audit queue. We
		 * will first sync the file systems then attempt to
		 * shutdown the kernel so that a memory dump is
		 * performed.
		 */
		sync();
		sync();

		/*
		 * now shut down. What a cruel world it has been
		 */
		panic("non-attributable halt. should dump core");

		/* mdboot(A_SHUTDOWN, RB_DUMP); */

	}
	mutex_exit(&au_queue.lock);
	return (0);

}	/* AUDIT_ASYNC_BLOCK */

/*
 * Put the calling process to sleep if there are too few free au_membufs.
 * Return only when there are sufficient.
 */

void
au_until_mem_free()

{	/* AU_UNTIL_MEM_FREE */

	register unsigned int state;	/* "total" state */
	struct p_audit_data *pad = (struct p_audit_data *)P2A(curproc);

	mutex_enter(&checktime_lock);
	/*
	 * No au_membufs available for auditing.
	 */
	while (au_nomb()) {
		/*
		 * Two ways out. First, the process should
		 * never sleep. Second, it's not auditing
		 * anything anyway.
		 */
		state = pad->pad_mask.as_success | pad->pad_mask.as_failure;
		if (state == 0)
			break;
		cv_wait(&checktime_cv, &checktime_lock);
	}
	mutex_exit(&checktime_lock);

}	/* AU_UNTIL_MEM_FREE */

#define	AU_INTERVAL 120







/*
 * Actually write audit information to the disk.
 * This version will be slow.
 * The performance police will want buffering done.
 */
int
au_doio(vp, limit)

	struct vnode *vp;
	int limit;

{	/* AU_DOIO */

	off_t		off;	/* space used in buffer */
	size_t		used;	/* space used in au_membuf */
	token_t		*cAR;	/* current AR being processed */
	token_t		*cMB;	/* current au_membuf being processed */
	token_t		*sp;	/* last AR processed */
	char		*bp;	/* start of free space in staging buffer */
	unsigned char	*cp;	/* ptr to data to be moved */
	/*
	 * size (data left in au_membuf - space in buffer)
	 */
	ssize_t		sz;
	ssize_t		len;	/* len of data to move, size of AR */
	int		error;	/* error return */
	ssize_t		left;	/* data not xfered by write to disk */
	statvfs64_t	sb;	/* buffer for statfs */
	size_t		curr_sz = 0;	/* amount of data written during now */

ADDTRACE("[%x] au_doio(%p,%x)", vp, limit, 0, 0, 0, 0);

	/*
	 * Check to insure enough free space on audit device.
	 */
ADDTRACE("[%x], vnode %p vp->v_vfsp \n", vp, vp->v_vfsp, 0, 0, 0, 0);

	bzero(&sb, sizeof (statvfs64_t));
	(void) VFS_STATVFS(vp->v_vfsp, &sb);
	/*
	 * Large Files: We do not convert any of this part of kernel
	 * to be large file aware. Original behaviour should be
	 * maintained. This function is called from audit_svc and
	 * it already checks for negative values of limit.
	 */

	if (sb.f_blocks && (fsblkcnt64_t)limit > sb.f_bavail)
		return (ENOSPC);

	if (audit_file_stat.af_filesz &&
		(audit_file_stat.af_currsz >= audit_file_stat.af_filesz))
		return (EFBIG);

	/*
	 * has the write buffer changed length due to a auditctl(2)
	 */
	if (au_queue.bufsz != au_queue.buflen) {

		AS_DEC(as_memused, au_queue.buflen);
		kmem_free(au_buffer, au_queue.buflen);

		/* only if write buffer is non-zero */
		if (au_queue.bufsz) {
			AS_INC(as_memused, au_queue.bufsz);

			/* bad, should not sleep here. Testing only */
			au_buffer = kmem_alloc(au_queue.bufsz,
			    KM_SLEEP);
		}
		au_queue.buflen = au_queue.bufsz;

ADDTRACE("[%x] new buffer size: %lx", au_queue.buflen, 0, 0, 0, 0, 0);
	}

	if (!au_queue.head) {
ADDTRACE("[%x] no data to process, exit", 0, 0, 0, 0, 0, 0);
	goto nodata;
	}

	/*
	 * If audit buffer zero length, then just write out audit data
	 * one au_membuf at a time.
	*/
	if (au_queue.buflen) {

ADDTRACE("[%x] buffered writes %lx", au_queue.buflen, 0, 0, 0, 0, 0);

		sp   = (token_t *)0; /* no AR copied */
		off  = 0;	/* no space used in buffer */
		used = 0;	/* no data processed in au_membuf */
		cAR  = au_queue.head;	/* start at head of queue */
		cMB  = cAR;	/* start with first au_membuf of record */
		bp = &au_buffer[0];	/* start at beginning of buffer */

		while (cMB) {

ADDTRACE("[%x] xfer strt - AR cnt: %lx cAR: %p"
	"cMB: %p used: %lx bp: %p off: %lx",
	au_queue.cnt, cAR, cMB, used, bp, off);

			if (au_queue.head == 0) {
debug_enter("HELP ---- au_queue.head is null and we are still here");
			}

			cp  = memtod(cMB, unsigned char *);
			/* data left in au_membuf */
			sz  = (ssize_t)cMB->len - used;
			/* len to move */
			len = (ssize_t)MIN(sz, au_queue.buflen - off);

ADDTRACE("[%x] bcopy - cp: %p used: %lx bp: %p off: %lx ln: %lx",
	cp, used, bp, off, len, 0);

			/*
			 * move the data
			 */
			bcopy(cp + used, bp + off, len);
			used += len; /* update used au_membuf */
			off  += len; /* update offset into buffer */

ADDTRACE("[%x] new offsets - off: %lx cAR: %p sp: %p", off, cAR, sp, 0, 0, 0);

			if (used >= (ssize_t)cMB->len) {
				/* advance to next au_membuf */
				used = 0;
				cMB  = cMB->next_buf;
			}
			if (cMB == (au_buff_t *)0) {
				/* advance to next AR */
				sp   = cAR;
				cAR  = cAR->next_rec;
				cMB  = cAR;
			}
ADDTRACE("[%x] finished? cAR: %p cMB: %p used: %lx off: %lx",
	cAR, cMB, used, off, 0, 0);

			/*
			 * If we've reached end of buffer, or have run out of
			 * audit records on the queue, then its time to flush
			 * the holding buffer to the audit trail.
			 */
			if (au_queue.buflen == off || cAR == (au_buff_t *)0) {
				left = 0;

ADDTRACE("[%x] vn_rdwr write - off %lx", off, left, 0, 0, 0, 0);
			/*
			 * Largefiles: We purposely pass a value of
			 * MAXOFF_T as we do not want any of the
			 * auditing files to exceed 2GB. May be we will
			 * support this in future.
			 */
				error = vn_rdwr(UIO_WRITE, vp, au_buffer,
	off, 0LL, UIO_SYSSPACE, FAPPEND, (rlim64_t)MAXOFF_T, CRED(),
							&left);

ADDTRACE("[%x] error: %x left: %lx", error, left, 0, 0, 0, 0);

				if (error != 0) { /* error on write */
ADDTRACE("[%x] error: %x", error, 0, 0, 0, 0, 0);
					if (error == EDQUOT)
						error = ENOSPC;
					return (error);
				}

			/*
			 * end of file system
			 */
			if (left) {

				sz = off - left; /* how much written */

ADDTRACE("[%x] no FS space - sz: %lx = off: %lx left: %lx",
	sz, off, left, 0, 0, 0);

				/* update space counters */
				audit_file_stat.af_currsz += sz;

				/* which AR are done */
				while (sz) {
				    cAR = au_queue.head;
				    cp  = memtod(cAR, unsigned char *);
				    len = (ssize_t)((cp[1]<<24 | cp[2]<<16 |
						cp[3]<<8 | cp[4]) &
						0xffffffffU);

ADDTRACE("[%x] releasing AR - cAR: %p &data_cAR: %p AR_len: %lx (sz: %lx)",
	cAR, cp, len, sz, 0, 0);
				    if (len > sz)
					break;
				    sz -= len;
				    au_free_rec(au_dequeue());
				    AS_INC(as_written, 1);
				}
				/*
				 * wake up writer if we've dropped below
				 * low water mark
				 */
				mutex_enter(&au_queue.lock);
				if (au_queue.cnt <= au_queue.lowater) {
					if (au_queue.wt_block)
						cv_broadcast(
							&au_queue.write_cv);
				}

				mutex_exit(&au_queue.lock);
				if (!au_nomb()) {
					cv_broadcast(&checktime_cv);
				}
				return (ENOSPC);
			} else {	/* still space in file system */
			/*
			 * NOTE: the above else statement should
			 *	really be indented by another tap.
			 *	But no room to move over.
			 */

ADDTRACE("[%x] flush buffer - AQ.head: %p AQ.len: %lx sp: %p",
	au_queue.head, au_queue.cnt, sp, 0, 0, 0);

				if (sp) {	/* if we've written an AR */
					/*
					 * free records up to last one copied.
					 */
					while (au_queue.head &&
						(au_queue.head != sp)) {
						au_free_rec(au_dequeue());
						AS_INC(as_written, 1);
					}
					/*
					 * Now free last one copied.
					 */
					if (au_queue.head &&
					    (au_queue.head == sp)) {
						au_free_rec(au_dequeue());
						AS_INC(as_written, 1);
					}
				}

ADDTRACE("[%x] bufs freed - AQ.head: %p aq.cnt: %lx",
	au_queue.head, au_queue.cnt, 0, 0, 0, 0);
				/*
				 * wake up writer if we've dropped below
				 * low water mark
				 */
				mutex_enter(&au_queue.lock);
				if (au_queue.cnt <= au_queue.lowater) {
					if (au_queue.wt_block)
						cv_broadcast(
							&au_queue.write_cv);
				}
				mutex_exit(&au_queue.lock);
				/* Update sizes */
				curr_sz += off;
				audit_file_stat.af_currsz += (u_int)off;

				/* reset au_buffer pointers */
				sp = (token_t *)0;
				off  = 0;
				bp   = &au_buffer[0];

				/* check exit conditions */

				if (sb.f_blocks) {
					u_long blks_used;
					blks_used = (curr_sz / sb.f_bsize);
					if ((fsblkcnt64_t)limit >
					(sb.f_bavail - (fsblkcnt64_t)blks_used))
						return (ENOSPC);
				}
				if (audit_file_stat.af_filesz &&
					(audit_file_stat.af_currsz
					>= audit_file_stat.af_filesz))
					return (EFBIG);
			}
				/*
				 * Note: everything associated with the
				 *	brace should be shifted over
				 *	by another tap. That is why
				 *	the following closing braces
				 *	line up.
				 */
			}

ADDTRACE("[%x] go to next AR - AQ.head: %p cAR: %p cMB: %p bp: %p",
	au_queue.head, cAR, cMB, bp, 0, 0);

		}	/* for (1; cMB; 1) */
	} else {	/* if (au_queue.buflen) */
		/*
		 * pull off an audit record and write it
		 */
		while (au_queue.head) {
			/*
			 * Always write entire records
			 * assume atomic writes of pointers
			 */
			cMB = au_queue.head;

			/*
			 * copy data into holding buffer before writting to disk
			 * for now we'll write data out au_membuf at a time
			 */
			for (; cMB != (token_t *)0; cMB = cMB->next_buf) {
				if (cMB->len <= 0)
					continue;
				/*
				 * Verify that this data should in fact
				 * be written.
				 */
				if (au_auditstate == AUC_AUDITING) {

					/*
					 * Write an au_membuf's worth
					 */
					/*
					 * Largefiles: See comments above
					 * for vn_rdwr.
					 */
					error = vn_rdwr(UIO_WRITE, vp,
	memtod(cMB, caddr_t), cMB->len, 0LL, UIO_SYSSPACE,
	FAPPEND, (rlim64_t)MAXOFF_T, CRED(), &left);

ADDTRACE("[%x] error: %x left: %lx", error, left, 0, 0, 0, 0);

					if (error != 0) {
						if (error == EDQUOT)
							error = ENOSPC;
						return (error);
					}
				}
				/*
				 * Treat a residual count as a disk full.
				 */
				if (left != 0)
					return (ENOSPC);
			}

			/*
			 * remove audit record from queue and go to next one
			 */
			au_free_rec(au_dequeue());

			/*
			 * keep count of audit records written
			 * to audit trail
			 */
			AS_INC(as_written, 1);

			/*
			 * wake up writer if we've dropped below
			 * low water mark
			 */
			mutex_enter(&au_queue.lock);
			if (au_queue.cnt <= au_queue.lowater) {
				if (au_queue.wt_block)
					cv_broadcast(&au_queue.write_cv);
			}

			mutex_enter(&au_queue.lock);
			if (!au_nomb()) {
				cv_broadcast(&checktime_cv);
			}

		}	/* while (au_queue.head) */

	}	/* else part associated with if (au_queue.buflen) */

nodata:

	mutex_enter(&checktime_lock);
	if (((time_t)hrestime.tv_sec - au_checktime) <= AU_INTERVAL) {
		mutex_exit(&checktime_lock);
		return (0);
	}
	mutex_exit(&checktime_lock);

	/*
	 * Check to insure enough free space on audit device.
	 */
ADDTRACE("[%x], vnode %p vp->v_vfsp \n", vp, vp->v_vfsp, 0, 0, 0, 0);

	mutex_enter(&checktime_lock);
	au_checktime = (time_t)hrestime.tv_sec;
	mutex_exit(&checktime_lock);
	return (0);

}	/* AU_DOIO */








#ifdef C2_DEBUG
#define	SZST 1024
char save_trace[SZST*128];
int cnt_save_trace;
char *cur_trace_ptr = save_trace;


void
addtrace(char *f, ...)
{	/* ADDTRACE */

	va_list adx;
	char	buffer[256];
	int	i;
	int	l;
	int	s;

	va_start(adx, f);

	/*
	 * zero out buffer (safety first)
	 */
	for (i = 0; i < 256; i++)
		buffer[i] = 0;

	/*
	 * make the entry
	 */
	if (c2_debug)
		vprintf(f, adx);
	else
		vsprintf(buffer, f, adx);

	/*
	 * see if we overflow end of save area;
	 * adjust things if we do
	 */
	l = strlen(buffer) + 1;

	mutex_enter(&cktime_lock);
	if (cur_trace_ptr+l >= &save_trace[SZST*128])
		cur_trace_ptr = save_trace;

	/*
	 * save the data
	 */
	bcopy(buffer, cur_trace_ptr, (u_int)l);

	/*
	 * advance the pointer & index count
	 */
	cur_trace_ptr += l;
	cnt_save_trace++;
	mutex_exit(&cktime_lock);

}	/* ADDTRACE */

#endif	/* C2_DEBUG */

static int
au_nomb()
{
	int flag = 0;
	return (flag);
}
