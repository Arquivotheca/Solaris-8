/*
 *	nis_log_common.c
 *
 *	Copyright (c) 1988-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)nis_log_common.c	1.31	99/10/08 SMI"


/*
 *	nis_log_common.c
 *
 * This module contains logging functions that are common to the service and
 * the log diagnostic utilities.
 */

#include <syslog.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <rpc/types.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/nis.h>
#include <limits.h>
#include <string.h>
#include "nis_svc.h"
#include "nis_proc.h"
#include "log.h"

/* string guard, it is always safe to print nilptr(char_pointer) */
#define	nilptr(s)	((s) ? (s) : "(nil)")

#define	getpagesize()   sysconf(_SC_PAGESIZE)

static int	pagesize = 0;
int	in_checkpoint	= FALSE;
int	in_update	= FALSE;
int	need_checkpoint = 0;
extern	NIS_HASH_TABLE  old_stamp_list;
extern void add_updatetime();
nis_name invalid_directory = NULL;
static log_upd *last_upd_p;	/* tmp ptr used during delta updates */
static u_long upd_cnt;		/* tmp cntr used during delta updates */

/*
 * variable used to assure that nisping warnings both do not flood the log
 * and also do get sent out periodically (moved to nis_log_common.c).
 */
long	next_warntime = 0l;

#define	CHKPT_WARN_INTERVAL 3600	/* 1 hour interval in seconds */

extern int verbose;
int	__nis_logfd 	= -1;
log_hdr	*__nis_log 	= NULL;
u_long	__nis_filesize	= FILE_BLK_SZ;
u_long	__nis_logsize;
u_long	__maxloglen = MAXLOGLEN;
u_long	__loghiwater = HIWATER;

pid_t master_pid = 0;    /* Master PROCESS id */
u_long   cur_xid;
nis_name cur_princp;

/* data and routines for the updatetime cache begin here */
/*
 * updatetime_cache is a cache copy of the update timestamp for all
 * the directories served by this server.  It is kept in sync with the
 * log file by adding time stamps in end_transaction(), and rebuilding
 * the cache when checkpoint_log has finished.  Using the cache is
 * vital in readonly children, as the log file may be inconsistant.  It
 * is an optimization only otherwise.
 *
 * NOTE: this cache is not MT safe (nis_hash.c is probably the place to
 * fix that), but it is ok for use by both the parent process and the
 * readonly children since unlike the log file it is not shared but copied
 * (by fork).
 */

NIS_HASH_TABLE	*updatetime_cache = NULL;

void init_updatetime(void); /* forward */

/*
 * Purges the update timestamp cache
 * Frees everything, and sets updatetime_cache to NULL.
 */
static void
purge_updatetime(void)
{
	if (updatetime_cache != NULL) {
		/* clean up the timestamp cache */
		stamp_item	*si;
		while (si = (stamp_item *)nis_pop_item(updatetime_cache)) {
			if (si->item.name)
				free(si->item.name);
			free(si);
		}
		free(updatetime_cache);
		updatetime_cache = NULL;
	}
}

static void
add_updatetime(nis_name name, ulong utime)
{
	stamp_item	*si;

	if (updatetime_cache == NULL) {
		init_updatetime();
		if (updatetime_cache == NULL)
			return;
	}
	si = (stamp_item *)(nis_find_item(name, updatetime_cache));
	if (si) {
		/* already cached - update the timestamp */
		si->utime = utime;
		return;
	}
	si = (stamp_item *)(XCALLOC(1, sizeof (stamp_item)));
	if (! si) {
		syslog(LOG_CRIT, "add_updatetime(): out of memory.");
		purge_updatetime();
		return;
	}
	si->item.name = (nis_name) XSTRDUP(name);
	if (! si->item.name) {
		syslog(LOG_CRIT, "add_updatetime(): out of memory.");
		purge_updatetime();
		return;
	}
	si->utime = utime;
	nis_insert_item((NIS_HASH_ITEM *)(si), updatetime_cache);
}

/*
 * Routine to generate the cache copy of the update timestamp.
 * This routine would be static, and we would rely on initializing the
 * updatetime_cache when we tried to add_updatetime() to an empty cache,
 * except that it can take some time to traverse the log, and the client
 * could time out.
 *
 * We traverse the log from old to new, so that the newer updates will
 * overwrite the older ones.
 *
 * Recursive calls would be possible if init_updatetime called
 * add_updatetime, which ran out of memory, and freed the cache.  Then
 * the next call to add_updatetime from within init_updatetime would
 * recursively call init_updatetime.  If we run out of room building
 * the cache, we just free everything and don't build it.
 */
static bool_t		updating_cache = FALSE;
void
init_updatetime(void)
{
	log_upd		*upd;

	if (updating_cache)
		return;	/* won't do recursive updates */

	if (updatetime_cache != NULL)
		purge_updatetime();
	updatetime_cache = (NIS_HASH_TABLE *)
			calloc(1, sizeof (NIS_HASH_TABLE));
	if (! updatetime_cache) {
		syslog(LOG_CRIT, "add_updatetime(): out of memory.");
		return;
	}

	upd = __nis_log->lh_head;

	updating_cache = TRUE;
	while (upd) {
		add_updatetime(upd->lu_dirname, upd->lu_time);
		upd = upd->lu_next;
	}
	updating_cache = FALSE;
}

/* data and routines for the updatetime cache end here */

void
sync_header()
{
	if (! pagesize)	/* msync misfeature */
		pagesize = getpagesize();
	if (msync((caddr_t) __nis_log, pagesize, MS_SYNC)) {
		perror("msync:");
		syslog(LOG_CRIT, "Unable to msync LOG HEADER.");
		abort();
	}
}

void
sync_update(upd)
	log_upd *upd;
{
	u_long	start, end, len;
	u_long	size;

	if (! pagesize)	/* msync misfeature */
		pagesize = getpagesize();
	/* round start to the nearest page */
	start = ((u_long) upd) & (~(pagesize-1));
	/* round end to the nearest page */
	end   = (((u_long) upd) + XID_SIZE(upd) + (pagesize-1)) &
					(~(pagesize-1));
	size  = end - start;
	if (msync((caddr_t)start, size, MS_SYNC)) {
		perror("msync:");
		syslog(LOG_CRIT, "Unable to msync LOG UPDATE.");
		abort();
	}

}

/*
 * call msync() on the entire transaction log -- used for fast delta
 * updates.
 */
void sync_log() {
	u_long	start, end, len;
	u_long	size;

	if (! pagesize)	/* msync misfeature */
	    pagesize = getpagesize();
	/* start at __nis_log */
	start = (u_long) __nis_log;
	/* round end to the nearest page */
	end   = (((u_long) __nis_log->lh_tail) + XID_SIZE(__nis_log->lh_tail)
			+ (pagesize-1)) & (~(pagesize-1));
	size  = end - start;
	if (msync((caddr_t)start, size, MS_SYNC)) {
	    perror("msync:");
	    syslog(LOG_CRIT, "Unable to msync transaction log.");
	    abort();
	}
}


/*
 * This function starts a logged transaction. When the function returns
 * the log is in the "update" state. If the function returns 0 then
 * the transaction couldn't be started (NIS_TRYAGAIN) is returned to
 * the client. Otherwise it returns the XID that should be used in
 * calls to add update.
 */
int
begin_transaction(princp)
	nis_name	princp;
{
	/* XXX for threads we set up a writer lock */

	/*
	 * Synchronization point. At this point we try to get
	 * exclusive access to the log. If this fails we return
	 * 0 so that the client can try again later.
	 */
	if (in_checkpoint)
		return (0);

	if (__nis_log->lh_state != LOG_STABLE)
		return (0);

	in_update = TRUE; /* XXX thread spin lock  */
	cur_xid = __nis_log->lh_xid + 1;
	cur_princp = princp;
	__nis_log->lh_state = LOG_UPDATE;
	sync_header();
	return (cur_xid);
}

/*
 * end_transaction
 *
 * This function does the actual commit, by setting the log header
 * to be stable, and to have the latest XID present. Further it updates
 * the time of the last update in the transaction to be 'ttime'. This
 * is the time that is sent to the replicates so that they will know
 * when they've picked up all of the updates up to this point.
 */
int
end_transaction(xid)
	int	xid;
{
	/* Check to see if add_update was called at all */
	if (__nis_log->lh_tail->lu_xid != __nis_log->lh_xid) {
		__nis_log->lh_xid++;
		if (__nis_log->lh_xid != xid) {
			syslog(LOG_CRIT, "end_transaction: Corrupted log!");
			abort();
		}
	}
	sync_update(__nis_log->lh_tail);

	__nis_log->lh_state = LOG_STABLE;
	sync_header();

	/* update the updatetime cache */
	add_updatetime(__nis_log->lh_tail->lu_dirname,
		__nis_log->lh_tail->lu_time);
	return (0);
}

/*
 * Freeze the log tail and the log counter during fast updates.
 */
void begin_update() {
	last_upd_p = __nis_log->lh_tail;
	sync_header();
	upd_cnt = __nis_log->lh_num;
}

/*
 * Unfreeze the log tail and the log counter
 */
void end_update() {
	__nis_log->lh_tail = last_upd_p;
	last_upd_p = NULL;
	__nis_log->lh_num = upd_cnt;
	sync_header();
}

/*
 * add_update()
 *
 * This function adds an update from the current transaction into
 * the transaction log. It gets a bit tricky because it "allocates"
 * all of it's memory from the file. Basically the transactions are
 * laid down as follows :
 *
 *		:  prev  update  :
 *		+----------------+
 *		| log_upd header |
 *		+----------------+
 *		|   XDR Encoded  |
 *		:   object from  :
 *		|   log_entry    |
 *		+----------------+
 *		| Directory name |
 *		+----------------+
 *		:  next  update  :
 *
 *	The macro XID_SIZE calculates the offset to the start of the next
 *	update.
 * 	returns 0 if the msync fails, otherwise it returns the log time.
 *
 * add_update is now a wrapper for the same code, but without
 * msync's.  replica_update calls add_update_nosync directly (the log tail and
 * the log counter are then frozen); all others calls are to add_update, when
 * the tail and the counter are not frozen.
 */

u_long
add_update(log_entry *le) {
	log_upd *upd;
	u_long res;
	__nis_logsize = (__nis_log->lh_tail) ? LOG_SIZE(__nis_log) :
	    sizeof (log_hdr);
	last_upd_p = __nis_log->lh_tail;
	upd_cnt = __nis_log->lh_num;
	res = add_update_nosync(le, (void **)&upd);
	sync_update(upd);
	__nis_log->lh_tail = last_upd_p;
	__nis_log->lh_num = upd_cnt;
	sync_header();
	return (res);
}

u_long
add_update_nosync(le, updp)
	log_entry	*le;
	void		**updp;
{
	XDR	xdrs;
	log_upd	*upd;
	u_long	upd_size, total_size;
	int	ret;

	if (in_checkpoint)
		abort(); /* Can't happen, because begin_trans would fail */

	if (CHILDPROC) {
		syslog(LOG_INFO,
		"add_update: attempt add transaction from readonly child.");
		return (0);
	}

	__nis_logsize = (__nis_log->lh_tail) ? __nis_logsize :
							sizeof (log_hdr);

	/* need both lines to find update size */
	if (le->le_princp == 0)
		le->le_princp = cur_princp;
	upd_size = xdr_sizeof((xdrproc_t) xdr_log_entry, le);

	/* see diagram above this function */
	total_size = __nis_logsize +  NISRNDUP(sizeof (log_upd) + upd_size +
					strlen(le->le_object.zo_domain) + 1);
	if (total_size >= __nis_filesize) {
		__nis_filesize = ((total_size / FILE_BLK_SZ) + 1) * FILE_BLK_SZ;
		ret = (int)lseek(__nis_logfd, __nis_filesize, SEEK_SET);
		if (ret == -1) {
			syslog(LOG_ERR,
				"Cannot grow transaction log, error %s",
							strerror(errno));
			return (0);
		}
		ret = (int)write(__nis_logfd, "+", 1);
		if (ret != 1) {
			syslog(LOG_ERR,
		"Cannot write one character to transaction log, error %s",
							strerror(errno));
			return (0);
		}
	}

	__nis_logsize = total_size;
	if (last_upd_p)
		upd = (log_upd *)
		(((u_long)last_upd_p) + XID_SIZE(last_upd_p));
	else {
		upd = (log_upd *)((u_long)(__nis_log)+
					NISRNDUP(sizeof (log_hdr)));
	}

	if ((u_long)(upd) > ((u_long)(__nis_log) + __loghiwater)) {
		struct timeval curtime;
		if ((gettimeofday(&curtime, 0) != -1) &&
				(curtime.tv_sec > next_warntime)) {
			next_warntime = curtime.tv_sec + CHKPT_WARN_INTERVAL;
			syslog(LOG_CRIT,
	"NIS+ server needs to be checkpointed. Use \"nisping -C domainname\"");
		}
	}

	upd->lu_magic = LOG_UPD_MAGIC;
	upd->lu_prev = last_upd_p;
	upd->lu_next = NULL;
	upd->lu_xid = cur_xid;
	upd->lu_time = le->le_time;
	upd->lu_size = upd_size;
	xdrmem_create(&xdrs, (char *) upd->lu_data, upd->lu_size, XDR_ENCODE);
	if (!xdr_log_entry(&xdrs, le)) {
		syslog(LOG_ERR, "add_update: xdr_log_entry failed");
		return (0);
	}
	upd->lu_dirname = (char *)(NISRNDUP((((u_long) upd) + sizeof (log_upd)
							    + upd->lu_size)));
	strcpy(upd->lu_dirname, le->le_object.zo_domain);
	if (updp)
	    *updp = upd;
	if (last_upd_p)
	    last_upd_p->lu_next = upd;
	else
		__nis_log->lh_head = upd;
	last_upd_p = upd;
	upd_cnt++;
	need_checkpoint = 1;
	return (1);
}

/*
 * make_stamp() - moved from nis_subr_proc.c to this file since
 * nisbackup also needs this.
 *
 * This function adds a "null" entry into the log that indicates either
 * the directory is stable or gone. When a directory is deleted, a tombstone
 * (entry with a timestamp of 0) is written to the log. This prevents prior
 * activity on the log from being confused with current activity. When a
 * directory is resynchronized with a full dump, timestamp information is
 * lost so this is used to mark the directory as being stable up to that point.
 */
void
make_stamp(name, stime)
	nis_name	name;
	u_long		stime;
{
	log_entry	le;
	u_long		xid;

	memset((char *)&le, 0, sizeof (le));
	le.le_princp = nis_local_principal();
	le.le_time = stime;
	le.le_type = UPD_STAMP;
	le.le_name = name;
	__type_of(&(le.le_object)) = NIS_NO_OBJ;
	le.le_object.zo_name = "";
	le.le_object.zo_owner = "";
	le.le_object.zo_group = "";
	le.le_object.zo_domain = name;
	if (xid = begin_transaction(le.le_princp)) {
		if (add_update(&le))
			end_transaction(xid);
		else
			fprintf(stderr, "make_stamp: could not add_update\n");
	} else {
		fprintf(stderr,
			"make_stamp: zero xid from begin_transaction\n");
	}
}

/*
 * nis_cptime()
 *
 * This function will ask the indicated replicate for the last
 * update it has seen to the given directory.
 */
u_long
nis_cptime(replica, name)
	nis_server	*replica;
	nis_name	name;
{
	return (nis_cptime_msg(replica, name, TRUE));
}

/*
 * nis_cptime_msg()
 *
 * The guts of nis_cptime(). If "domsg" is FALSE, messages are suppressed,
 * unless verbose mode is enabled.
 */
u_long
nis_cptime_msg(replica, name, domsg)
	nis_server	*replica;
	nis_name	name;
	bool_t		domsg;
{
	CLIENT		*clnt;
	enum clnt_stat 	status;
	struct timeval	tv;
	u_long		res;

	clnt = nis_make_rpchandle(replica, 0, NIS_PROG, NIS_VERSION,
					ZMH_DG|ZMH_AUTH, 1024, 512);
	/* If we can't contact it, return the safe answer */
	if (! clnt) {
		if (verbose)
		    syslog(LOG_INFO, "nis_cptime: could not contact %s",
			replica->name);
		return (0);
	}

	tv.tv_sec = 6;	/* retry time out */
	tv.tv_usec = 0;
	clnt_control(clnt, CLSET_RETRY_TIMEOUT, (void *)&tv);
	tv.tv_sec = 10;
	status = clnt_call(clnt, NIS_CPTIME, xdr_nis_name, (char *) &name,
					    xdr_u_long, (char *) &res, tv);
	if (status != RPC_SUCCESS && (domsg || verbose)) {
		syslog(LOG_WARNING,
			"nis_cptime: RPC error srv='%s', dir='%s', err='%s'",
				    replica->name, name, clnt_sperrno(status));
		res = 0;
	}
	clnt_destroy(clnt);
	if (verbose)
		syslog(LOG_INFO, "nis_cptime: returning %d for %s from %s",
			res, name, replica->name);
	return (res);
}

/*
 * __make_name()
 *
 * This function prints out a nice name for a search entry.
 */
char *
__make_name(le)
	log_entry	*le;
{
	static char	namestr[2048];
	int		i;

	if (le->le_attrs.le_attrs_len)
		strcpy(namestr, "[ ");
	else
		namestr[0] = '\0';

	for (i = 0; i < le->le_attrs.le_attrs_len; i++) {
		strcat(namestr, le->le_attrs.le_attrs_val[i].zattr_ndx);
		strcat(namestr, " = ");
		if (le->le_attrs.le_attrs_val[i].zattr_val.zattr_val_len)
			strcat(namestr,
			le->le_attrs.le_attrs_val[i].zattr_val.zattr_val_val);
		else
			strcat(namestr, "(nil)");
		strcat(namestr, ", ");
	}

	if (le->le_attrs.le_attrs_len) {
		namestr[strlen(namestr) - 2] = '\0';
		strcat(namestr, " ],");
	}
	strcat(namestr, le->le_name);
	return (namestr);
}

/*
 * __log_resync()
 *
 * This function will relocate the log to its current position in
 * memory. Errors returned :
 *	 0	success
 *	-1 	Illegal Update
 *	-2 	Missing Data
 * 	-3 	Not enough updates
 *
 * Private flag (p):
 *	FNISD	called from nisd
 *	FNISLOG	called from nislog
 *	FCHKPT	called from checkpoint_log
 */
int
__log_resync(log, p)
	log_hdr	*log;
	int	p;
{
	log_upd		*prev, *cur;
	int		i, ret;
	u_long		addr_p;

	/*
	 * Resync the hard way. In this section of code we
	 * reconstruct the log pointers by calculating where
	 * the pieces would be placed. Our calcuation is
	 * verified by the presence of the appropriate MAGIC
	 * number at the address we've calculated. If any
	 * magic number isn't present, we note that the log
	 * is corrupt and exit the service. The user will
	 * have to either patch the log, or resync the slaves
	 * by hand. (Forced resync)
	 *
	 */

	if (verbose)
		syslog(LOG_INFO, "Resynchronizing transaction log.");

	addr_p = NISRNDUP((u_long)log + sizeof (log_hdr));
	prev = NULL;

	/*
	 * if there are any transactions, this is the first one.
	 */
	if (log->lh_num)
		log->lh_head = (log_upd *)(addr_p);
	if (verbose) {
		syslog(LOG_INFO, "Log has %d transactions in it.", log->lh_num);
		syslog(LOG_INFO, "Last valid transaction is %d.", log->lh_xid);
	}

	for (i = 0; i < log->lh_num; i++) {
		cur = (log_upd *)(addr_p);
		if (cur->lu_magic != LOG_UPD_MAGIC) {
			syslog(LOG_ERR,
			"__log_resync: Transaction #%d bad magic number", i);
			if (p != FNISLOG) {
				syslog(LOG_ERR,
"__log_resync: log truncated, resync to propagate possibly lost changes");
			} else {
				printf(
		"__log_resync: Transaction #%d has a bad magic number\n", i);
			}
			break; /* major corruption */
		} else if (NISRNDUP((u_long) cur +
				sizeof (log_upd) + cur->lu_size) >=
				((u_long)log + __nis_filesize) ||
				NISRNDUP((u_long)cur +
				sizeof (log_upd) + cur->lu_size) <=
				(u_long)cur) {
/*
 * Verify that lu_size seems reasonable by checking that the address to be used
 * for cur->lu_dirname falls inside the mapped section from the transaction log.
 * Note: Test for "<=", since cur+sizeof(log_upd)+cur->lu_size could overflow a
 * u_long.
 */
			syslog(LOG_ERR,
				"__log_resync: Transaction #%d: bad size %d",
				i, cur->lu_size);
			if (p != FNISLOG) {
				syslog(LOG_ERR,
"__log_resync: log truncated, resync to propagate possibly lost changes");
			} else {
				printf(
		"__log_resync: Transaction #%d bad size %d\n", i, cur->lu_size);
			}
			break; /* also major corruption */
		}

		/* Fix up the link lists */
		log->lh_tail = cur; /* Track the current update */
		if (prev)
			prev->lu_next = cur;
		cur->lu_prev = prev;
		cur->lu_next = NULL;
		cur->lu_dirname = (char *)(NISRNDUP((((u_long) cur) +
							sizeof (log_upd) +
							cur->lu_size)));
		prev = cur;

		/* move to next update in the list */
		if (verbose) {
			syslog(LOG_INFO, "Resync'd transaction #%d.", i+1);
			syslog(LOG_INFO, "Directory was '%s'.",
						nilptr(cur->lu_dirname));
		}
		addr_p += XID_SIZE(cur);
	}
	if (i < log->lh_num) {
		if (verbose)
			syslog(LOG_INFO, "%d valid transactions.", i);
		if (prev) {
			if (prev->lu_xid > log->lh_xid) {
				syslog(LOG_INFO,
		"__log_resync: Incomplete last update transaction, removing.");
				while (log->lh_tail->lu_xid > log->lh_xid) {
					log->lh_tail = log->lh_tail->lu_prev;
					i--;
					if (! log->lh_tail)
						break;
				}
			} else
				log->lh_tail = prev;
			log->lh_num = i;
		} else {
			log->lh_tail = NULL;
			log->lh_num = 0;
		}
	}
	__nis_logsize = (log->lh_tail) ? LOG_SIZE(log) : sizeof (log_hdr);
	if (verbose)
		syslog(LOG_INFO, "Log size is %d bytes", __nis_logsize);

	if (p == FNISLOG) {
		/* called from nislog, don't ftruncate() or msync() */
		return (0);
} else if (p == FCHKPT) {
		/* called from checkpoint, truncate transaction log file */
		__nis_filesize = (((__nis_logsize / FILE_BLK_SZ) + 1) *
							FILE_BLK_SZ);
		ret = ftruncate(__nis_logfd, __nis_filesize);
		if (ret == -1) {
			syslog(LOG_ERR,
			"__log_resync: Cannot truncate transaction log file");
			return (-1);
		}
		ret = (int)lseek(__nis_logfd, 0L, SEEK_CUR);
		if (ret == -1) {
			syslog(LOG_ERR,
	"__log_resync: cannot seek to begining of transaction log file");
			return (-1);
		}
		ret = (int)lseek(__nis_logfd, __nis_filesize, SEEK_SET);
		if (ret == -1) {
			syslog(LOG_ERR,
		"__log_resync: cannot increase transaction log file size");
			return (-1);
		}
		ret = (int) write(__nis_logfd, "+", 1);
		if (ret != 1) {
			syslog(LOG_ERR,
		"__log_resync: cannot write one byte to transaction log file");
			return (-1);
		}
	}

	log->lh_addr = log;
	ret = msync((caddr_t)log, __nis_logsize, MS_SYNC);
						/* Could take a while */
	if (ret == -1) {
		syslog(LOG_ERR, "msync() error in __log_resync()");
		abort();
	}
	return (0);
}

/*
 * map_log()
 *
 * This function maps in the logfile into the address space of thenis
 * server process. After the log is successfully mapped it is "relocated"
 * so that the pointers in the file are valid for the log's position in
 * memory. Once relocated, the log is ready for use. This function returns
 * 0 if the log file is OK and !0 if it is corrupted.
 *
 * 	Error numbers :
 *		-4	File not found.
 *		-5	Cannot MMAP file
 *		-6	Corrupt file, bad magic number in header.
 *		-7	Unknonwn (illegal) state value
 */

int
map_log(logname, p)
	char	*logname;
	int	p;	/* Private flag:		*/
			/*	FNISD=called from nisd	*/
			/* 	FNISLOG=from nislog	*/
{
	int		error, fd, ret;
	log_upd		*update;
	log_hdr		tmp_hdr;
	log_entry	*entry_1, *entry_2;
	struct stat	st;
	long		log_size;

	sprintf(logname, "%s", LOG_FILE);
	if (stat(logname, &st) == -1) {
		if (p == FNISLOG) {	/* called from nislog */
			return (-4);
		}

		__nis_logfd = open(logname, O_RDWR+O_CREAT, 0600);
		if (__nis_logfd == -1) {
			syslog(LOG_ERR, "Unable to open logfile '%s'", logname);
			return (-4);
		}

		/* Make this file as it doesn't exist */
		ret = (int)lseek(__nis_logfd, 0L, SEEK_CUR);
		if (ret == -1) {
			syslog(LOG_ERR,
		"map_log: cannot seek to begining of transaction log file");
			return (-1);
		}
		ret = (int)lseek(__nis_logfd, __nis_filesize, SEEK_SET);
		if (ret == -1) {
			syslog(LOG_ERR,
			"map_log: cannot increase transaction log file size");
			return (-1);
		}
		/* writing a / all the time seemed silly */
		ret = (int) write(__nis_logfd, "+", 1);
		if (ret != 1) {
			syslog(LOG_ERR,
		"map_log: cannot write one byte to transaction log file");
			return (-1);
		}
		fstat(__nis_logfd, &st);
	} else {	/* transaction log file exists */
		__maxloglen = (st.st_size > __maxloglen) ? st.st_size :
								__maxloglen;
		__nis_logfd = open(logname, O_RDWR, 0600);
		if (__nis_logfd == -1) {
			syslog(LOG_ERR, "Unable to open logfile '%s'", logname);
			return (-4);
		}
	}
	__nis_filesize = st.st_size;


	if (p == FNISLOG) {
		/* called from nislog */
		__nis_log = (log_hdr *)mmap(0, __nis_filesize,
			PROT_READ+PROT_WRITE, MAP_PRIVATE, __nis_logfd, 0);
	} else {
		/* called from nisd */
		__nis_log = (log_hdr *)mmap(0, __maxloglen,
			PROT_READ+PROT_WRITE, MAP_SHARED, __nis_logfd, 0);
	}

	if ((int)(__nis_log) == -1) {
		syslog(LOG_ERR,
		"Unable to map logfile of length %ld into address space.",
								__maxloglen);
		close(__nis_logfd);
		return (-5);
	}

	if (__nis_log->lh_magic != LOG_HDR_MAGIC) {
		/* If it's NULL then we just created the file */
		if (__nis_log->lh_magic == 0) {
			(void) memset(__nis_log, 0, sizeof (log_hdr));
			__nis_log->lh_state = LOG_STABLE;
			__nis_log->lh_magic = LOG_HDR_MAGIC;
			__nis_log->lh_addr  = __nis_log;
			if (p == FNISD)
				msync((caddr_t)__nis_log, sizeof (log_hdr),
								MS_SYNC);
		} else {
			syslog(LOG_ERR, "Illegal log file, remove and restart");
			return (-6);
		}

	}
	switch (__nis_log->lh_state) {
		case LOG_STABLE :
			if (verbose)
				syslog(LOG_INFO, "Log state is STABLE.");
			error = __log_resync(__nis_log, p);
			if (error)
				return (error); /* resync failed */
			break;
		case LOG_RESYNC :
			if (verbose)
				syslog(LOG_INFO, "Log state is RESYNC.");
			error = __log_resync(__nis_log, p);
			if ((error) || (p == FNISLOG))
				return (error);
			__nis_log->lh_state = LOG_STABLE;
			msync((caddr_t)__nis_log, sizeof (log_hdr), MS_SYNC);
			break;
		case LOG_UPDATE :
			if (verbose)
				syslog(LOG_INFO, "Log state is IN UPDATE.");
			error = __log_resync(__nis_log, p);
			if ((error) || (p == FNISLOG))
				return (error);

			update = __nis_log->lh_tail;
			/*
			 * If the last update was a dir invalidator,
			 * a full resync will happen and restore
			 * consistancy.
			 */
			if (update->lu_time == ULONG_MAX) {
				invalid_directory = strdup(update->lu_dirname);
			}
			/*
			 * Check to see if the update was written to the
			 * log.
			 */
			if (update->lu_xid == __nis_log->lh_xid) {
				/* never made it, so we're done. */
				__nis_log->lh_state = LOG_STABLE;
				msync((caddr_t)__nis_log,
						sizeof (log_hdr), MS_SYNC);
				return (0);
			}
			if (update->lu_time == ULONG_MAX)
				return (0);
			/* Back out the change */
			return (abort_transaction(0));
			break;
		case LOG_CHECKPOINT :
			if (verbose)
				syslog(LOG_INFO, "Log state is IN CHECKPOINT");
			if (p == FNISLOG) {
				error = __log_resync(__nis_log, p);
				return (error);
			}
			strcpy(logname, nis_data(BACKUP_LOG));
			fd = open(logname, O_RDONLY);
			if (fd == -1) {
				syslog(LOG_ERR, "map_log: missing backup.");
				return (-1);
			}
			error = read(fd, &log_size, sizeof (long));
			if (error == -1) {
				syslog(LOG_ERR,
					"map_log: unable to read backup");
				close(fd);
				return (-1);
			}
			error = read(fd, __nis_log, log_size);
			close(fd);
			if ((error == -1) || (error != log_size)) {
				__nis_log->lh_state = LOG_CHECKPOINT;
				syslog(LOG_ERR,
				"map_log: Backup log truncated, fatal error.");
				return (-1);
			}
			/* Manually resync the log */
			error = __log_resync(__nis_log, p);
			if (! error) {
				__nis_log->lh_state = LOG_STABLE;
				msync((caddr_t)__nis_log, sizeof (log_hdr),
								MS_SYNC);
				unlink(logname);
				break;
			}
			syslog(LOG_ERR,
			"map_log: Unable to resync after checkpoint restore.");
			return (error);
			break;
		default :
			syslog(LOG_ERR,
				"Illegal log state, aborting.");
			return (-7);
	}
	/* At this point everything should be cool. */
	return (0);
}


/*
 *  This code is very similar to nis_name_of() in libnsl.
 *  When a server lives in its own domain, its database files are named
 *  relative to the parent domain (that is, where it's credentials are
 *  stored.  This version of nis_name_of() will take that into account.
 *
 *  This is probably not the best file to put this routine into, but
 *  it is shared by rpc.nisd, nisbackup, and nisrestore.  All of these
 *  programs need a common routine to determine the name of a database file.
 */

/*
 * relative_name()
 * This internal function will remove from the NIS name, the domain
 * name of the current server, this will leave the unique part in
 * the name this becomes the "internal" version of the name. If this
 * function returns NULL then the name we were given to resolve is
 * bad somehow.
 *
 * A dynamically-allocated string is returned.
 */

nis_name
relative_name(s)
	char	*s;	/* string with the name in it. */
{
	char			*d;
	char			*buf;
	int			dl, sl;
	name_pos		p;

	if (s == NULL)
		return (NULL);

	d = __nis_rpc_domain();
	if (d == NULL)
		return (NULL);
	dl = strlen(d); 	/* _always dot terminated_   */

	buf = strdup(s);
	if (buf == NULL)
		return (NULL);
	strcpy(buf, s);		/* Make a private copy of 's'   */
	sl = strlen(buf);

	if (dl == 1) {			/* We're the '.' directory   */
		buf[sl-1] = '\0';	/* Lose the 'dot'	  */
		return (buf);
	}

	p = nis_dir_cmp(buf, d);

	/* 's' is above 'd' in the tree */
	if ((p == HIGHER_NAME) || (p == NOT_SEQUENTIAL) || (p == SAME_NAME))
		return (NULL);

	/* Insert a NUL where the domain name starts in the string */
	buf[(sl - dl) - 1] = '\0';

	/* Don't return a zero length name */
	if (buf[0] == '\0') {
		free((void *)buf);
		return (NULL);
	}

	return (buf);
}
