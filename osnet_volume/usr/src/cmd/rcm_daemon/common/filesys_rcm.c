/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)filesys_rcm.c	1.2	99/11/10 SMI"

/*
 * This RCM module adds support to the RCM framework for mounted filesystems
 * and files therein contained.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <synch.h>
#include <libintl.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mnttab.h>
#include <sys/param.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utssys.h>
#include <sys/wait.h>

#include "rcm_module.h"

/*
 * Definitions
 */

/* #define	RCM_FILESYS_AUTOMATION */ /* This functionality is turned off */

/* Hash table parameters */
#define	HASH_DEFAULT		16
#define	HASH_THRESHOLD		64

/* Mount options */
#define	OPT_DETACHABLE		"detachable"
#define	OPT_IGNORE		"ignore"

/* Constants */
#define	MAX_CMD			(MAXPATHLEN + 128)
#define	MNT_SPECIAL		1
#define	MNT_MOUNTP		2
#define	SUPPORTED_FLAGS		(RCM_FORCE | RCM_INCLUDE_DEPENDENT | \
				RCM_INCLUDE_SUBTREE | RCM_QUERY)

/* Messages */

/* Offline failures */
#if	defined(RCM_FILESYS_AUTOMATION)
#define	MSG_NONDETACHABLE	"cannot detach \"%1$s\" %2$s"
#else
#define	MSG_NONDETACHABLE	"mounted filesystem \"%1$s\" %2$s"
#endif
#define	MSG_BUSY		"busy filesystem \"%1$s\" %2$s"
#define	MSG_BUSYCONFLICT	"Another RCM operation is in progress " \
				"\"%1$s\" %2$s"
#define	MSG_UMOUNTFAILED	"failed to unmount \"%1$s\" %2$s"

/* Online failures */
#define	MSG_REMOUNTFAILED	"failed to remount \"%1$s\" %2$s"
#define	MSG_ONLINEDEPFAILED	"failed to online dependents of \"%1$s\" %2$s"
#define	MSG_MOUNTPNOEXIST	"bad mountpoint, failed to online \"%1$s\" %2$s"

/* Remove failures */
#define	MSG_REMOVEDEPFAILED	"failed to remove dependents of \"%1$s\" %2$s"

/* Suspend failures */
#define	MSG_SUSPENDCRITICAL	"cannot suspend critical filesystem \"%1$s\" " \
				"%2$s"
#define	MSG_SUSPENDDEPFAILED	"failed to suspend dependents of \"%1$s\" %2$s"

/* Resume failures */
#define	MSG_RESUMEDEPFAILED	"failed to resume dependents of \"%1$s\" %2$s"

/* getinfo messages */
#define	MSG_OVERLAY		"Overlay,"
#define	MSG_DETACHABLE		"Detachable,"
#define	MSG_INFO		"\"%1$s\" mounted (%2$s%3$s%4$sRefs=%5$d)"

/* Generic messages */
#define	MSG_REGFAIL		"failed to register \"%s\""
#define	MSG_UNRECOGNIZED 	"\"%s\" is not a filesystem resource"
#define	MSG_SPECIAL1		"(Special=\"%s\")"
#define	MSG_SPECIAL2		"Special=\"%s\","

/* Node in a mountpoint stack (they're stacked to show overlays) */
typedef struct mntstack {
	struct mnttab mnt;
	ino_t inode;
	dev_t device;
	int offlined;
	struct mntstack *up;
	struct mntstack *down;
} mntstack_t;

/* Node in a hash table line; each line is a sorted linked list */
typedef struct hash_line {
	mntstack_t *mnts;
	struct hash_line *next;
} hashline_t;

/* Definition of a cache, containing a mountpoint table and a special index */
typedef struct cache {
	hashline_t **table;
	hashline_t **specindex;
	time_t timestamp;
	mutex_t lock;
	uint32_t h;
	uint32_t registered;
} cache_t;

/*
 * Forward Declarations
 */

/* module interface routines */
static int mnt_register(rcm_handle_t *);
static int mnt_unregister(rcm_handle_t *);
static int mnt_getinfo(rcm_handle_t *, char *, id_t, uint_t, char **,
    rcm_info_t **);
static int mnt_suspend(rcm_handle_t *, char *, id_t, timespec_t *,
    uint_t, char **, rcm_info_t **);
static int mnt_resume(rcm_handle_t *, char *, id_t, uint_t, char **,
    rcm_info_t **);
static int mnt_offline(rcm_handle_t *, char *, id_t, uint_t, char **,
    rcm_info_t **);
static int mnt_online(rcm_handle_t *, char *, id_t, uint_t, char **,
    rcm_info_t **);
static int mnt_remove(rcm_handle_t *, char *, id_t, uint_t, char **,
    rcm_info_t **);

/* cache creators */
static cache_t *create_cache();
static hashline_t *create_hashline(mntstack_t *);

/* cache destructors */
static void free_cache(cache_t **);
static void free_hashline(hashline_t **);
static void free_mntstack(mntstack_t **);

/* cache operations */
static mntstack_t *cache_insert(cache_t *, struct mnttab *);
static void cache_remove(cache_t *, mntstack_t *);
static mntstack_t *cache_lookup(cache_t *, char *);
static mntstack_t *cache_lookup_sync(rcm_handle_t *, cache_t **, char *);
static void cache_dependent(mntstack_t *, char **, int *);
static struct mnttab *cache_walk(cache_t *, uint32_t *, hashline_t **,
    mntstack_t **);
static void cache_sync(rcm_handle_t *, cache_t **);

/* miscellaneous functions */
static uint32_t hash(uint32_t, char *);
int utssys();
static void register_mountpoint(rcm_handle_t *, struct mnttab *);
static void register_special(rcm_handle_t *, struct mnttab *);
static void unregister_mountpoint(rcm_handle_t *, struct mnttab *);
static void unregister_special(rcm_handle_t *, struct mnttab *);
static int system1(const char *);
static int is_critical(struct mnttab *);
static char *format_special(char *, char *, uint_t);
#if	defined(RCM_FILESYS_AUTOMATION)
static int is_forcible(struct mnttab *, uint_t);
#endif

/*
 * Module-Private data
 */
static struct rcm_mod_ops mnt_ops =
{
	RCM_MOD_OPS_VERSION,
	mnt_register,
	mnt_unregister,
	mnt_getinfo,
	mnt_suspend,
	mnt_resume,
	mnt_offline,
	mnt_online,
	mnt_remove
};

static cache_t *mnttab_cache = NULL;

/*
 * Module Interface Routines
 */

/*
 * rcm_mod_init()
 *
 *	Create a mnttab cache, and return the ops structure.
 */
/* ARGSUSED */
struct rcm_mod_ops *
rcm_mod_init(rcm_handle_t *hd)
{
	/* Only honor callbacks with valid arguments */
	assert(hd != NULL);

	/* If an old cache is left around, destroy it */
	if (mnttab_cache)
		free_cache(&mnttab_cache);

	/* Try making a new one */
	if ((mnttab_cache = create_cache()) == NULL) {
		rcm_log_message(RCM_ERROR,
		    gettext("module can't function, aborting."));
		return (NULL);
	}

	/* Return the ops vectors */
	return (&mnt_ops);
}

/*
 * rcm_mod_info()
 *
 *	Return a string describing this module.
 */
const char *
rcm_mod_info()
{
	return ("File system module 1.2");
}

/*
 * rcm_mod_fini()
 *
 *	Destroy the mnttab cache.
 */
int
rcm_mod_fini()
{
	if (mnttab_cache)
		free_cache(&mnttab_cache);
	return (RCM_SUCCESS);
}

/*
 * mnt_register()
 *
 *	Make sure the cache is properly sync'ed, and its registrations are in
 *	order.
 *
 *	Locking: the cache is locked throughout the execution of this routine
 *	because it reads and possibly modifies cache links continuously.
 */
int
mnt_register(rcm_handle_t *hd)
{
	uint32_t i = 0;
	struct mnttab *mnt;
	hashline_t *l = NULL;
	mntstack_t *s = NULL;

	/* Guard against bad arguments */
	assert(hd != NULL);

	/* Lock the cache */
	(void) mutex_lock(&mnttab_cache->lock);

	/* If the cache has already been registered, then just sync it. */
	if (mnttab_cache && mnttab_cache->registered) {
		cache_sync(hd, &mnttab_cache);
		if (mnttab_cache == NULL)
			return (RCM_FAILURE);
		else {
			(void) mutex_unlock(&mnttab_cache->lock);
			return (RCM_SUCCESS);
		}
	}

	/* If not, register the whole cache and mark it as registered. */
	while ((mnt = cache_walk(mnttab_cache, &i, &l, &s)) != NULL) {
		register_mountpoint(hd, mnt);
		register_special(hd, mnt);
	}
	mnttab_cache->registered = 1;

	/* Unlock the cache */
	(void) mutex_unlock(&mnttab_cache->lock);

	return (RCM_SUCCESS);
}

/*
 * mnt_unregister()
 *
 *	Manually walk through the cache, unregistering all the special files and
 *	mount points.
 *
 *	Locking: the cache is locked throughout the execution of this routine
 *	because it reads and modifies cache links continuously.
 */
int
mnt_unregister(rcm_handle_t *hd)
{
	struct mnttab *mnt;
	hashline_t *l = NULL;
	mntstack_t *s = NULL;
	uint32_t i = 0;

	/* Guard against bad arguments */
	assert(hd != NULL);

	/* Walk the cache, unregistering everything */
	(void) mutex_lock(&mnttab_cache->lock);
	cache_sync(hd, &mnttab_cache);
	if (mnttab_cache != NULL) {
		while ((mnt = cache_walk(mnttab_cache, &i, &l, &s)) != NULL) {
			unregister_mountpoint(hd, mnt);
			unregister_special(hd, mnt);
		}
		mnttab_cache->registered = 0;
		(void) mutex_unlock(&mnttab_cache->lock);
	}
	return (RCM_SUCCESS);
}

/*
 * mnt_offline()
 *
 *	Determine dependents of the resource being offlined, and offline
 *	them all.
 *
 *	Locking: the cache is locked for most of this routine, except while
 *	processing dependents.
 */
int
mnt_offline(rcm_handle_t *hd, char *rsrc, id_t id, uint_t flags,
    char **reason, rcm_info_t **dependent_reason)
{
	int len;
	int rv = RCM_SUCCESS;
	char *errfmt;
	char *special;
	char *dependent = NULL;
	mntstack_t *mnt;
#if	defined(RCM_FILESYS_AUTOMATION)
	struct stat pre_st;
	struct stat post_st;
	char cmd[MAX_CMD];
#endif

	/* Guard against bad arguments */
	assert(hd != NULL);
	assert(rsrc != NULL);
	assert(id == (id_t)0);
	assert(reason != NULL);
	assert(dependent_reason != NULL);

	/* Fail on unsupported flags */
	if (flags & (~SUPPORTED_FLAGS))
		return (ENOTSUP);

	rcm_log_message(RCM_TRACE1, "FILESYS: offline(%s)\n", rsrc);

	/* Lock the cache */
	(void) mutex_lock(&mnttab_cache->lock);

	/*
	 * Lookup this resource in the cache.  If we don't recognize the
	 * resource, it's not a failure.  The operation is just a no-op.
	 */
	if ((mnt = cache_lookup_sync(hd, &mnttab_cache, rsrc)) == NULL) {
		(void) mutex_unlock(&mnttab_cache->lock);
		rcm_log_message(RCM_ERROR, gettext(MSG_UNRECOGNIZED), rsrc);
		return (RCM_SUCCESS);
	}

	/*
	 * Decide whether or not this resource can be offlined.
	 *
	 * If the automation functionality is turned off then the offline
	 * always fails.  And, the code that implements the offline actions
	 * can be preprocessed out.
	 */
#if	!defined(RCM_FILESYS_AUTOMATION)
	rv = RCM_FAILURE;
	errfmt = gettext(MSG_NONDETACHABLE);
	goto offline_done;
#else
	if (is_critical(&(mnt->mnt)) ||
	    ((mnt->mnt.mnt_mntopts != NULL) &&
	    (hasmntopt(&mnt->mnt, OPT_DETACHABLE) == NULL))) {
		rv = RCM_FAILURE;
		errfmt = gettext(MSG_NONDETACHABLE);
		goto offline_done;
	}

	/* Initiate the offline for the resource's dependents */
	cache_dependent(mnt, &dependent, &dep_type);
	if (dependent) {

		/* For mountpoints, add the RCM_FILESYS flag */
		if (dep_type == MNT_MOUNTP)
			flags |= RCM_FILESYS;

		/* Temporarily unlock the cache then process dependents */
		(void) mutex_unlock(&mnttab_cache->lock);
		rv = rcm_request_offline(hd, dependent, flags,
		    dependent_reason);
		(void) mutex_lock(&mnttab_cache->lock);

		/*
		 * The cache could change while processing dependents, so
		 * the 'mnt' variable needs to be refreshed.  A failure to
		 * find the resource now can't be processed normally; we'll
		 * just have to log an error and return.
		 */
		if ((mnt = cache_lookup_sync(hd, &mnttab_cache, rsrc))
		    == NULL) {
			(void) mutex_unlock(&mnttab_cache->lock);
			rcm_log_message(RCM_ERROR, gettext(
			    "unexpectedly lost the '%s' resource."), rsrc);
			return (RCM_FAILURE);
		}

		/* If the dependents failed, then this resource fails too */
		if (rv != RCM_SUCCESS) {
			if (rv == RCM_CONFLICT)
				errfmt = gettext(MSG_BUSYCONFLICT);
			else
				errfmt = gettext(MSG_BUSY);
			goto offline_done;
		}
	}

	/* If it's a query or it's already offlined, then skip the unmount */
	if (flags & RCM_QUERY || mnt->offlined)
		goto offline_done;

	/*
	 * Attempt to unmount this resource.  To detect success of the umount
	 * command, stat the mountpoint before and after the umount.  If the
	 * inode/device values change, then the umount succeeded.
	 *
	 * XXX - Should umount(1M) return a non-zero exit status when it fails?
	 */
	(void) snprintf(cmd, MAX_CMD, "/usr/sbin/umount %s %s",
	    (is_forcible(&(mnt->mnt), flags)) ? "-f" : "", mnt->mnt.mnt_mountp);
	rcm_log_message(RCM_TRACE2, gettext("executing \"%s\"\n"), cmd);
	(void) stat(mnt->mnt.mnt_mountp, &pre_st);
	(void) system1(cmd);
	(void) stat(mnt->mnt.mnt_mountp, &post_st);
	if (pre_st.st_ino == post_st.st_ino &&
	    pre_st.st_dev == post_st.st_dev) {
		rv = RCM_FAILURE;
		errfmt = gettext(MSG_UMOUNTFAILED);
		goto offline_done;
	}
	mnt->inode = pre_st.st_ino;
	mnt->device = pre_st.st_dev;
	mnt->offlined = 1;
	*reason = NULL;
#endif

offline_done:
	if (dependent)
		free(dependent);
	if (rv != RCM_SUCCESS) {
		len = strlen(errfmt) + strlen(rsrc) + 1;
		special = format_special(rsrc, mnt->mnt.mnt_special, 1);
		if (special)
			len += strlen(special);
		if (*reason  = (char *)malloc(len))
			(void) snprintf(*reason, len, errfmt,
			    mnt->mnt.mnt_mountp, (special) ? special : "");
		if (special)
			free(special);
	}

	/* Unlock the cache and return */
	(void) mutex_unlock(&mnttab_cache->lock);
	return (rv);
}

/*
 * mnt_online()
 *
 *	Remount the previously offlined filesystem, and online its dependents.
 *
 *	Locking: the cache is locked for most of this routine, except while
 *	processing dependents.
 */
/*ARGSUSED*/
int
mnt_online(rcm_handle_t *hd, char *rsrc, id_t id, uint_t flag, char **reason,
    rcm_info_t **dependent_reason)
{
	int len;
	int dep_type;
	int rv = RCM_SUCCESS;
	char *errfmt;
	char *special;
	char *dependent = NULL;
	mntstack_t *mnt;
	struct stat st;
	char mnttime[20];
	char cmd[MAX_CMD];

	/* Guard against bad arguments */
	assert(hd != NULL);
	assert(rsrc != NULL);
	assert(id == (id_t)0);

	/* Fail on unsupported flags */
	if (flag & (~SUPPORTED_FLAGS))
		return (ENOTSUP);

	rcm_log_message(RCM_TRACE1, "FILESYS: online(%s)\n", rsrc);

	/* Lock the cache */
	(void) mutex_lock(&mnttab_cache->lock);

	/* Lookup this resource in the cache. */
	if ((mnt = cache_lookup_sync(hd, &mnttab_cache, rsrc)) == NULL) {
		rcm_log_message(RCM_ERROR, gettext(MSG_UNRECOGNIZED), rsrc);
		goto online_done;
	}

	/* This is a no-op if the filesystem is not offlined */
	if (!mnt->offlined)
		goto online_done;

	/*
	 * If we can't stat() the mountpoint, the mountpoint might not
	 * exist; and we shouldn't proceed.
	 */
	if (stat(mnt->mnt.mnt_mountp, &st) < 0 && errno == ENOENT) {
		errfmt = gettext(MSG_MOUNTPNOEXIST);
		rv = RCM_FAILURE;
		goto online_done;
	}

	/* Remount the filesystem with its old options */
	if (mnt->inode != st.st_ino || mnt->device != st.st_dev) {
		(void) snprintf(cmd, MAX_CMD,
		    "/usr/sbin/mount %s -F %s %s %s %s %s",
		    (mnt->down) ? "-O" : "", mnt->mnt.mnt_fstype,
		    (mnt->mnt.mnt_mntopts != NULL) ? "-o" : "",
		    (mnt->mnt.mnt_mntopts != NULL) ? mnt->mnt.mnt_mntopts : "",
		    mnt->mnt.mnt_special, mnt->mnt.mnt_mountp);
		rcm_log_message(RCM_TRACE2, gettext("executing \"%s\"\n"), cmd);
		(void) system1(cmd);
		(void) stat(mnt->mnt.mnt_mountp, &st);
		if (mnt->inode != st.st_ino || mnt->device != st.st_dev) {
			errfmt = gettext(MSG_REMOUNTFAILED);
			rv = RCM_FAILURE;
			goto online_done;
		}
		(void) snprintf(mnttime, 20, "%ld", (ulong_t)time);
		if (mnt->mnt.mnt_time)
			free(mnt->mnt.mnt_time);
		mnt->mnt.mnt_time = strdup(mnttime);
		mnt->offlined = 0;
	}

	/* Process dependents */
	cache_dependent(mnt, &dependent, &dep_type);
	if (dependent) {

		/* Add the filesystem flag when dependents are mountpoints */
		if (dep_type == MNT_MOUNTP)
			flag |= RCM_FILESYS;

		/* Temporarily unlock the cache and process the dependents */
		(void) mutex_unlock(&mnttab_cache->lock);
		rv = rcm_notify_online(hd, dependent, flag, dependent_reason);
		(void) mutex_lock(&mnttab_cache->lock);

		/*
		 * Processing the dependents could have altered the mnttab
		 * file, so refresh the 'mnt' variable.
		 */
		if ((mnt = cache_lookup_sync(hd, &mnttab_cache, rsrc))
		    == NULL) {
			(void) mutex_unlock(&mnttab_cache->lock);
			rcm_log_message(RCM_ERROR, gettext(
			    "unexpectedly lost the '%s' resource."), rsrc);
			return (RCM_FAILURE);
		}

		if (rv != RCM_SUCCESS)
			errfmt = gettext(MSG_ONLINEDEPFAILED);
	}

online_done:
	if (dependent)
		free(dependent);
	if (rv != RCM_SUCCESS) {
		len = strlen(errfmt) + strlen(rsrc) + 1;
		special = format_special(rsrc, mnt->mnt.mnt_special, 1);
		if (special)
			len += strlen(special);
		if (*reason  = (char *)malloc(len))
			(void) snprintf(*reason, len, errfmt,
			    mnt->mnt.mnt_mountp, (special) ? special : "");
		if (special)
			free(special);
	}

	/* Unlock the cache and return */
	(void) mutex_unlock(&mnttab_cache->lock);
	return (rv);
}

/*
 * mnt_getinfo()
 *
 *	Gather usage information for this resource.
 *
 *	Locking: the cache is locked while this routine looks up the
 *	resource and extracts copies of any piece of information it needs.
 *	The cache is then unlocked, and this routine performs the rest of
 *	its functions without touching any part of the cache.
 */
int
mnt_getinfo(rcm_handle_t *hd, char *rsrc, id_t id, uint_t flag,
    char **info, rcm_info_t **depend_info)
{
	int dep_type;
	int overlay = 0;
	int detachable = 0;
	int len;
	int opens;
	f_user_t fusers[1024];
	char *dependent = NULL;
	char *info_fmt;
	char *special_fmt;
	mntstack_t *mnt;
	char mountp[MAXPATHLEN];
	char special[MAXPATHLEN];

	/* Guard against bad arguments */
	assert(hd != NULL);
	assert(rsrc != NULL);
	assert(id == (id_t)0);
	assert(info != NULL);
	assert(depend_info != NULL);

	/* Fail on unsupported flags */
	if (flag & (~SUPPORTED_FLAGS))
		return (ENOTSUP);

	rcm_log_message(RCM_TRACE1, "FILESYS: getinfo(%s)\n", rsrc);

	/*
	 * Lock the cache just long enough to extract information about this
	 * resource.
	 */
	(void) mutex_lock(&mnttab_cache->lock);
	if ((mnt = cache_lookup_sync(hd, &mnttab_cache, rsrc)) == NULL ||
	    mnt->offlined) {
		(void) mutex_unlock(&mnttab_cache->lock);
		return (RCM_FAILURE);
	}
	(void) strncpy(mountp, mnt->mnt.mnt_mountp, MAXPATHLEN);
	(void) strncpy(special, mnt->mnt.mnt_special, MAXPATHLEN);
	if (mnt->down)
		++overlay;
#if	defined(RCM_FILESYS_AUTOMATION)
	if ((mnt->mnt.mnt_mntopts != NULL) &&
	    (hasmntopt(&mnt->mnt, OPT_DETACHABLE) != NULL)) {
#endif
	if ((flag & RCM_INCLUDE_DEPENDENT) || (flag & RCM_INCLUDE_SUBTREE))
		cache_dependent(mnt, &dependent, &dep_type);
	(void) mutex_unlock(&mnttab_cache->lock);

	/* Determine the reference count */
	opens = utssys(mountp, F_CONTAINED, UTS_FUSERS, fusers);
	if (opens < 0)
		opens = 0;

	/* Allocate storage for the information string */
	info_fmt = gettext(MSG_INFO);
	special_fmt = format_special(rsrc, special, 0);
	len = strlen(info_fmt) + strlen(mountp) + 1;
	if (special_fmt)
		len += strlen(special_fmt);
	if (overlay)
		len += strlen(gettext(MSG_OVERLAY));
	if (detachable)
		len += strlen(gettext(MSG_DETACHABLE));
	if ((*info = (char *)malloc(len)) == NULL) {
		rcm_log_message(RCM_ERROR,
			gettext("malloc failure during getinfo callback."));
		if (special_fmt)
			free(special_fmt);
		if (dependent)
			free(dependent);
		return (RCM_FAILURE);
	}

	/* Fill in the string */
	(void) snprintf(*info, len, info_fmt, mountp,
	    (detachable != 0) ? gettext(MSG_DETACHABLE) : "",
	    (overlay != 0) ? gettext(MSG_OVERLAY) : "",
	    (special_fmt) ? special_fmt : "", opens);

	/* Get dependent info if requested */
	if (dependent &&
	    ((flag & RCM_INCLUDE_DEPENDENT) || (flag & RCM_INCLUDE_SUBTREE))) {
		if (dep_type == MNT_MOUNTP)
			flag |= RCM_FILESYS;
		(void) rcm_get_info(hd, dependent, flag, depend_info);
	}

	if (special_fmt)
		free(special_fmt);
	if (dependent)
		free(dependent);
	return (RCM_SUCCESS);
}

/*
 * mnt_suspend()
 *
 *	Notify all dependents that the resource is being suspended.
 *	Since no real operation is involved, QUERY or not doesn't matter.
 *
 *	Locking: the cache is only used to retrieve some information about
 *	this resource, so it is only locked during that retrieval.
 */
int
mnt_suspend(rcm_handle_t *hd, char *rsrc, id_t id, timespec_t *interval,
    uint_t flag, char **reason, rcm_info_t **dependent_reason)
{
	int len;
	int dep_type;
	int rv = RCM_SUCCESS;
	int rsrc_is_critical;
	char *errfmt;
	char *dependent;
	char *special_fmt;
	mntstack_t *mnt;
	char mountp[MAXPATHLEN];
	char special[MAXPATHLEN];

	/* Guard against bad arguments */
	assert(hd != NULL);
	assert(rsrc != NULL);
	assert(id == (id_t)0);
	assert(interval != NULL);
	assert(reason != NULL);
	assert(dependent_reason != NULL);

	/* Fail on unsupported flags */
	if (flag & (~SUPPORTED_FLAGS))
		return (ENOTSUP);

	rcm_log_message(RCM_TRACE1, "FILESYS: suspend(%s)\n", rsrc);

	/*
	 * Lock the cache just long enough to extract information about this
	 * resource.
	 */
	(void) mutex_lock(&mnttab_cache->lock);
	if ((mnt = cache_lookup_sync(hd, &mnttab_cache, rsrc)) == NULL) {
		(void) mutex_unlock(&mnttab_cache->lock);
		rcm_log_message(RCM_ERROR, gettext(MSG_UNRECOGNIZED), rsrc);
		return (RCM_SUCCESS);
	}
	(void) strncpy(mountp, mnt->mnt.mnt_mountp, MAXPATHLEN);
	(void) strncpy(special, mnt->mnt.mnt_special, MAXPATHLEN);
	cache_dependent(mnt, &dependent, &dep_type);
	rsrc_is_critical = is_critical(&(mnt->mnt));
	(void) mutex_unlock(&mnttab_cache->lock);

	/*
	 * Don't allow suspension of "critical" filesystem components except
	 * in the case of a forced operation.
	 */
	if (rsrc_is_critical && ((flag & RCM_FORCE) == 0)) {
		rv = RCM_FAILURE;
		errfmt = gettext(MSG_SUSPENDCRITICAL);
		goto suspend_done;
	}

	/* If we don't care about our dependents, this is a no-op */
	if (!((flag & RCM_INCLUDE_SUBTREE) || (flag & RCM_INCLUDE_DEPENDENT)))
		goto suspend_done;

	/* Process dependents */
	if (dependent) {

		/* Add the RCM_FILESYS flag to mountpoint dependents */
		if (dep_type == MNT_MOUNTP)
			flag |= RCM_FILESYS;

		/* Initiate a suspend on the dependents */
		if ((rv = rcm_request_suspend(hd, dependent, flag, interval,
		    dependent_reason)) != RCM_SUCCESS)
			errfmt = gettext(MSG_SUSPENDDEPFAILED);
	}

suspend_done:
	if (dependent)
		free(dependent);
	if (rv != RCM_SUCCESS) {
		len = strlen(errfmt) + strlen(rsrc) + 1;
		special_fmt = format_special(rsrc, special, 1);
		if (special_fmt)
			len += strlen(special_fmt);
		if (*reason = (char *)malloc(len))
			(void) snprintf(*reason, len, errfmt, mountp,
			    (special_fmt) ? special_fmt : "");
	}
	return (rv);
}

/*
 * mnt_resume()
 *
 *	Resume all the dependents of a suspended filesystem.
 *
 *	Locking: the cache is only used to retrieve some information about
 *	this resource, so it is only locked during that retrieval.
 */
/*ARGSUSED*/
int
mnt_resume(rcm_handle_t *hd, char *rsrc, id_t id, uint_t flag, char **info,
    rcm_info_t **dependent_info)
{
	int len;
	int dep_type;
	int rv = RCM_SUCCESS;
	int rsrc_is_critical;
	char *errfmt;
	char *dependent;
	char *special_fmt;
	mntstack_t *mnt;
	char mountp[MAXPATHLEN];
	char special[MAXPATHLEN];

	/* Guard against bad arguments */
	assert(hd != NULL);
	assert(rsrc != NULL);
	assert(id == (id_t)0);

	/* Fail on unsupported flags */
	if (flag & (~SUPPORTED_FLAGS))
		return (ENOTSUP);

	rcm_log_message(RCM_TRACE1, "FILESYS: resume(%s)\n", rsrc);

	/* This is a no-op if we're not supposed to include our dependents */
	if (!((flag & RCM_INCLUDE_SUBTREE) || (flag & RCM_INCLUDE_DEPENDENT)))
		return (RCM_SUCCESS);

	/*
	 * Lock the cache just long enough to extract information about this
	 * resource.
	 */
	(void) mutex_lock(&mnttab_cache->lock);
	if ((mnt = cache_lookup_sync(hd, &mnttab_cache, rsrc)) == NULL) {
		(void) mutex_unlock(&mnttab_cache->lock);
		rcm_log_message(RCM_ERROR, gettext(MSG_UNRECOGNIZED), rsrc);
		return (RCM_SUCCESS);
	}
	(void) strncpy(mountp, mnt->mnt.mnt_mountp, MAXPATHLEN);
	(void) strncpy(special, mnt->mnt.mnt_special, MAXPATHLEN);
	cache_dependent(mnt, &dependent, &dep_type);
	rsrc_is_critical = is_critical(&(mnt->mnt));
	(void) mutex_unlock(&mnttab_cache->lock);

	/* If we suspended the resources dependents, then resume them. */
	if (dependent && !rsrc_is_critical) {
		if (dep_type == MNT_MOUNTP)
			flag |= RCM_FILESYS;
		rv = rcm_notify_resume(hd, dependent, flag, dependent_info);
		free(dependent);
	}

	/* Report success or failure */
	if (rv == RCM_SUCCESS)
		*info = NULL;
	else {
		errfmt = gettext(MSG_RESUMEDEPFAILED);
		len = strlen(errfmt) + strlen(mountp) + 1;
		special_fmt = format_special(rsrc, special, 1);
		if (special_fmt)
			len += strlen(special_fmt);
		if (*info = (char *)malloc(len))
			(void) snprintf(*info, len, errfmt, mountp,
			    (special_fmt) ? special_fmt : "");
	}
	return (rv);
}

/*
 * mnt_remove()
 *
 *	Remove the resource from the cache.
 *
 *	Locking: the cache is locked temporarily while information is gathered
 *	about the resource.  Then the cache is locked again to see if the entry
 *	still resides in the cache, and if so to remove it.
 */
/*ARGSUSED*/
int
mnt_remove(rcm_handle_t *hd, char *rsrc, id_t id, uint_t flag, char **info,
    rcm_info_t **dependent_info)
{
	int len;
	int dep_type;
	int rv = RCM_SUCCESS;
	char *errfmt;
	char *dependent;
	char *special_fmt;
	mntstack_t *mnt;
	char mountp[MAXPATHLEN];
	char special[MAXPATHLEN];

	/* Guard against bad arguments */
	assert(hd != NULL);
	assert(rsrc != NULL);
	assert(id == (id_t)0);

	/* Fail on unsupported flags */
	if (flag & (~SUPPORTED_FLAGS))
		return (ENOTSUP);

	rcm_log_message(RCM_TRACE1, "FILESYS: remove(%s)\n", rsrc);

	/*
	 * Lock the cache just long enough to extract information about this
	 * resource.
	 */
	(void) mutex_lock(&mnttab_cache->lock);
	if ((mnt = cache_lookup_sync(hd, &mnttab_cache, rsrc)) == NULL) {
		(void) mutex_unlock(&mnttab_cache->lock);
		return (RCM_SUCCESS);
	}
	cache_dependent(mnt, &dependent, &dep_type);
	(void) strcpy(mountp, mnt->mnt.mnt_mountp);
	(void) strcpy(special, mnt->mnt.mnt_special);
	(void) mutex_unlock(&mnttab_cache->lock);

	/* Remove this resource's dependents */
	if (dependent &&
	    ((flag & RCM_INCLUDE_DEPENDENT) || (flag & RCM_INCLUDE_SUBTREE))) {

		/* Add the RCM_FILESYS flag for mountpoint dependents */
		if (dep_type == MNT_MOUNTP)
			flag |= RCM_FILESYS;

		rv = rcm_notify_remove(hd, dependent, flag, dependent_info);

		/* Manually deregister interest in mountpoint dependents */
		if (dep_type == MNT_MOUNTP && rv == RCM_SUCCESS)
			(void) rcm_unregister_interest(hd, dependent,
			    RCM_FILESYS);
		free(dependent);
	}

	/* Construct an error message if there was a failure */
	if (rv != RCM_SUCCESS) {
		errfmt = gettext(MSG_REMOVEDEPFAILED);
		len = strlen(errfmt) + strlen(mountp) + 1;
		special_fmt = format_special(rsrc, special, 1);
		if (special_fmt)
			len += strlen(special_fmt);
		if (*info = (char *)malloc(len))
			(void) snprintf(*info, len, errfmt, mountp,
			    (special_fmt) ? special_fmt : "");
		if (special_fmt)
			free(special_fmt);
		return (rv);
	}

	/*
	 * Try retrieving a cache entry for this resource.  If one still exists
	 * (after processing its dependents, which could have involved its
	 * mountpoint as an alternate reference to the same cache entry), then
	 * remove it.
	 */
	(void) mutex_lock(&mnttab_cache->lock);
	if ((mnt = cache_lookup_sync(hd, &mnttab_cache, rsrc)) != NULL)
		cache_remove(mnttab_cache, mnt);
	(void) mutex_unlock(&mnttab_cache->lock);

	/* Clean up and return success */
	(void) rcm_unregister_interest(hd, rsrc, 0);
	*info = NULL;
	return (RCM_SUCCESS);
}

/*
 * Cache management routines
 */

/*
 * create_cache()
 *
 *	This routine allocates storage for a new cache.  The new cache contains
 * all of the information from the current /etc/mnttab file.
 *
 * Return Values: On success, a pointer to the newly created cache.  On failure,
 *		a NULL pointer.
 */
static cache_t *
create_cache()
{
	struct stat st;
	struct mnttab mt;
	FILE *fp;
	cache_t *cache;
	uint32_t size;
	uint32_t i;

	/* determine the size of the new cache's hash tables */
	if (stat(MNTTAB, &st) < 0) {
		rcm_log_message(RCM_ERROR,
		    gettext("can't stat mnttab file: %s"), MNTTAB);
		return (NULL);
	}
	if (st.st_size > HASH_THRESHOLD)
		size = st.st_size / HASH_THRESHOLD;
	else
		size = HASH_DEFAULT;

	/* try allocating storage for a new, empty cache */
	if ((cache = (cache_t *)malloc(sizeof (cache_t))) == NULL) {
		rcm_log_message(RCM_ERROR,
		    gettext("can't allocate memory for cache."));
		return (NULL);
	}
	(void) memset((char *)cache, 0, sizeof (*cache));
	cache->table = (hashline_t **)malloc(size * sizeof (hashline_t *));
	if (cache->table == NULL) {
		rcm_log_message(RCM_ERROR,
		    gettext("can't allocate memory for table."));
		free(cache);
		return (NULL);
	}
	cache->specindex = (hashline_t **)malloc(size * sizeof (hashline_t *));
	if (cache->specindex == NULL) {
		rcm_log_message(RCM_ERROR,
		    gettext("can't allocate memory for index."));
		free_cache(&cache);
		return (NULL);
	}

	/* initialize the new, empty cache */
	for (i = 0; i < size; i++) {
		cache->table[i] = NULL;
		cache->specindex[i] = NULL;
	}
	cache->h = size;
	cache->timestamp = st.st_mtime;

	/* open and lock the mnttab file */
	if ((fp = fopen(MNTTAB, "r")) == NULL) {
		rcm_log_message(RCM_ERROR,
		    gettext("can't open mnttab file: %s"), MNTTAB);
		free_cache(&cache);
		return (NULL);
	}

	/* Insert each entry from the mnttab file into the cache */
	while (getmntent(fp, &mt) == 0) {
		if ((mt.mnt_mntopts != NULL) &&
		    (hasmntopt(&mt, OPT_IGNORE) != NULL))
			continue;
		if (cache_insert(cache, &mt) == NULL) {
			rcm_log_message(RCM_ERROR,
			    gettext("failed to insert a mount."));
			free_cache(&cache);
			(void) fclose(fp);
			return (NULL);
		}
	}

	/* Unlock and close the mnttab file */
	(void) fclose(fp);

	/* initialize the lock mutex */
	if (mutex_init(&cache->lock, USYNC_THREAD, NULL)) {
		rcm_log_message(RCM_ERROR, gettext("can't allocate a lock."));
		free_cache(&cache);
		return (NULL);
	}

	/* Mark the cache as new */
	cache->registered = 0;

	/* finished -- return the new cache */
	return (cache);
}

/*
 * create_hashline()
 *
 *	This routine will take the address of a new stack of mountpoints and
 * encase it in a newly allocated hashline structure.
 *
 * Return Values: On success, a pointer to the new structure is returned.  On
 *		failure, a NULL pointer is returned.
 */
static hashline_t *
create_hashline(mntstack_t *newmnt)
{
	hashline_t *newhashline;

	newhashline = (hashline_t *)malloc(sizeof (*newhashline));
	if (newhashline == NULL) {
		rcm_log_message(RCM_ERROR,
		    gettext("can't allocate a table node."));
		return (NULL);
	}
	(void) memset((char *)newhashline, 0, sizeof (*newhashline));
	newhashline->mnts = newmnt;
	return (newhashline);
}

/*
 * free_cache()
 *
 *	Given a pointer to a cache structure, this routine will free all
 * of the memory allocated within the cache.
 */
static void
free_cache(cache_t **cache)
{
	uint32_t index;
	hashline_t *hashline;
	hashline_t *tmp;
	cache_t *realcache;

	/* sanity check */
	if (cache == NULL || *cache == NULL)
		return;

	/* de-reference the cache pointer */
	realcache = *cache;

	/* free the main hash table */
	for (index = 0; index < realcache->h; index++) {
		free_hashline(&realcache->table[index]);
	}
	free(realcache->table);
	realcache->table = NULL;

	/*
	 * free the special device hash table (manually, since it contains
	 * now-stale links to items in the main hash table
	 */
	for (index = 0; index < realcache->h; index++) {
		hashline = realcache->specindex[index];
		while (hashline != NULL) {
			tmp = hashline->next;
			free(hashline);
			hashline = tmp;
		}
		realcache->specindex[index] = NULL;
	}
	free(realcache->specindex);
	realcache->specindex = NULL;

	/* destroy the mutex */
	(void) mutex_destroy(&realcache->lock);

	free(realcache);
	*cache = NULL;
}

/*
 * free_hashline()
 *
 *	This routine frees all of the memory allocated within a node of a
 * hash table line.
 */
static void
free_hashline(hashline_t **hashline)
{
	hashline_t *tmp;

	if (hashline != NULL) {
		while (*hashline != NULL) {
			tmp = (*hashline)->next;
			free_mntstack(&((*hashline)->mnts));
			free(*hashline);
			*hashline = tmp;
		}
	}
}

/*
 * free_mntstack()
 *
 *	This routine frees all of the memory allocated within a stack of
 * mountpoints.
 */
static void
free_mntstack(mntstack_t **mnts)
{
	mntstack_t *tmp;

	if (mnts != NULL) {
		while (*mnts != NULL) {
			tmp = (*mnts)->down;
			if ((*mnts)->mnt.mnt_special) {
				free((*mnts)->mnt.mnt_special);
				(*mnts)->mnt.mnt_special = NULL;
			}
			if ((*mnts)->mnt.mnt_mountp) {
				free((*mnts)->mnt.mnt_mountp);
				(*mnts)->mnt.mnt_mountp = NULL;
			}
			if ((*mnts)->mnt.mnt_fstype) {
				free((*mnts)->mnt.mnt_fstype);
				(*mnts)->mnt.mnt_fstype = NULL;
			}
			if ((*mnts)->mnt.mnt_mntopts) {
				free((*mnts)->mnt.mnt_mntopts);
				(*mnts)->mnt.mnt_mntopts = NULL;
			}
			if ((*mnts)->mnt.mnt_time) {
				free((*mnts)->mnt.mnt_time);
				(*mnts)->mnt.mnt_time = NULL;
			}
			free(*mnts);
			*mnts = NULL;
			*mnts = tmp;
		}
	}
}

/*
 * cache_sync()
 *
 *	Resynchronize the mnttab cache with the mnttab file.
 *
 *	The cache must be locked by the caller prior to calling this routine.
 */
static void
cache_sync(rcm_handle_t *hd, cache_t **cachep)
{
	cache_t *new_cache;
	cache_t *old_cache = *cachep;
	mntstack_t *old_mount;
	mntstack_t *new_mount;
	mntstack_t *s = NULL;
	hashline_t *l = NULL;
	struct mnttab *mnt;
	uint32_t i = 0;
	mutex_t mutex_tmp;
	struct stat st;

	/* Stat the current mnttab file and see if the cache needs syncing */
	if (stat(MNTTAB, &st) == 0) {
		if (old_cache->timestamp >= st.st_mtime)
			return;
	}

	/* Get a new cache */
	if ((new_cache = create_cache()) == NULL) {
		rcm_log_message(RCM_DEBUG,
		    "WARNING: couldn't re-cache mnttab.");
		return;
	}

	/* For every mount in the new cache... */
	while ((mnt = cache_walk(new_cache, &i, &l, &s)) != NULL) {

		/* Look for the exact mount instance in the old cache */
		old_mount = cache_lookup(old_cache, mnt->mnt_mountp);
		while (old_mount != NULL) {
			if (strcmp(old_mount->mnt.mnt_special,
			    mnt->mnt_special) == 0)
				break;
			else
				old_mount = old_mount->up;
		}

		/*
		 * If no match was found, register the new mount instance.
		 * Otherwise,remove it from the old cache.
		 */
		if (old_mount == NULL) {
			register_mountpoint(hd, mnt);
			register_special(hd, mnt);
		} else
			cache_remove(old_cache, old_mount);
	}

	/*
	 * For every mount left in the old cache, bring it over to the new
	 * cache if it's "offlined" and awaiting either an "online" or
	 * "removal."  If it's not 'offlined', just unregister it.
	 */
	i = 0;
	l = NULL;
	s = NULL;
	while ((mnt = cache_walk(old_cache, &i, &l, &s)) != NULL) {
		if (s && s->offlined == 1) {
			new_mount = cache_insert(new_cache, mnt);
			if (new_mount) {
				new_mount->offlined = 1;
				new_mount->inode = s->inode;
				new_mount->device = s->device;
			}
		} else {
			unregister_mountpoint(hd, mnt);
			unregister_special(hd, mnt);
		}
	}

	/* Swap mutexes */
	mutex_tmp = new_cache->lock;
	new_cache->lock = old_cache->lock;
	old_cache->lock = mutex_tmp;

	/* Swap pointers */
	*cachep = new_cache;

	/* Destroy the old cache */
	free_cache(&old_cache);

	/* Mark the new cache as if it were old */
	new_cache->registered = 1;
}

/*
 * cache_insert()
 *
 *	Given a cache and a pointer to an /etc/mnttab entry, this routine
 * inserts the /etc/mnttab entry into the cache.  It gets added to the hash
 * table and, if its special device isn't yet represented, its added to the
 * special index.
 *
 *	Note that the mntopts can be NULL.
 *
 *	The cache must be locked by the caller prior to calling this routine.
 *
 * Return Values: On failure, a NULL is returned.  On success, a pointer to
 *		  the new mountstack node is returned.
 */
static mntstack_t *
cache_insert(cache_t *cache, struct mnttab *mt)
{
	uint32_t hash_index;
	hashline_t *newhashline = NULL;
	hashline_t *previous = NULL;
	hashline_t *hashline = NULL;
	mntstack_t *newmnt = NULL;
	mntstack_t *mnts = NULL;
	int comp;
	char *c;

	/* sanity checks */
	if (cache == NULL || mt == NULL || mt->mnt_special == NULL ||
	    mt->mnt_mountp == NULL || mt->mnt_fstype == NULL ||
	    mt->mnt_time == NULL)
		return (NULL);

	/*
	 * First we create a structure to hold this mnttab entry
	 */
	if ((newmnt = (mntstack_t *)malloc(sizeof (mntstack_t))) == NULL) {
		rcm_log_message(RCM_ERROR,
		    gettext("can't allocate a new mount."));
		return (NULL);
	}
	(void) memset((char *)newmnt, 0, sizeof (*newmnt));
	newmnt->mnt.mnt_special = strdup(mt->mnt_special);
	newmnt->mnt.mnt_mountp = strdup(mt->mnt_mountp);
	newmnt->mnt.mnt_fstype = strdup(mt->mnt_fstype);
	if (mt->mnt_mntopts != NULL)
		newmnt->mnt.mnt_mntopts = strdup(mt->mnt_mntopts);
	newmnt->mnt.mnt_time = strdup(mt->mnt_time);
	if (newmnt->mnt.mnt_special == NULL ||
	    newmnt->mnt.mnt_mountp == NULL ||
	    newmnt->mnt.mnt_fstype == NULL ||
	    ((mt->mnt_mntopts != NULL) && (newmnt->mnt.mnt_mntopts == NULL)) ||
	    newmnt->mnt.mnt_time == NULL) {
		rcm_log_message(RCM_ERROR,
		    gettext("can't allocate new mount data."));
		free_mntstack(&newmnt);
		return (NULL);
	}
	c = &(newmnt->mnt.mnt_mountp[strlen(newmnt->mnt.mnt_mountp) - 1]);
	if (*c == '/' && c != newmnt->mnt.mnt_mountp)
		*c = '\0';

	/*
	 * Second, make sure the entry's special device is in the special index
	 */

	/* find the special hash table line */
	hash_index = hash(cache->h, mt->mnt_special);
	if (hash_index >= cache->h) {
		rcm_log_message(RCM_ERROR,
		    gettext("can't hash special device."));
		free_mntstack(&newmnt);
		return (NULL);
	}
	hashline = cache->specindex[hash_index];

	/* if the hash table slot is empty, then this is easy */
	if (hashline == NULL) {
		hashline = (hashline_t *)malloc(sizeof (*hashline));
		if (hashline == NULL) {
			rcm_log_message(RCM_ERROR,
			    gettext("can't allocate table node."));
			free_mntstack(&newmnt);
			return (NULL);
		}
		(void) memset((char *)hashline, 0, sizeof (*hashline));
		hashline->mnts = newmnt;
		cache->specindex[hash_index] = hashline;
	}

	/* if the hash table slot isn't empty, find the immediate successor */
	else {
		previous = NULL;
		while ((comp = strcmp(hashline->mnts->mnt.mnt_special,
		    newmnt->mnt.mnt_special)) < 0 && hashline->next != NULL) {
			previous = hashline;
			hashline = hashline->next;
		}

		/* insert the entry if it's not already there */
		if (comp > 0) {
			if ((newhashline = create_hashline(newmnt)) == NULL) {
				free_mntstack(&newmnt);
				return (NULL);
			}
			newhashline->next = hashline;

			if (previous)
				previous->next = newhashline;
			else
				cache->specindex[hash_index] = newhashline;
		} else if (comp < 0) {
			if ((newhashline = create_hashline(newmnt)) == NULL) {
				free_mntstack(&newmnt);
				return (NULL);
			}
			newhashline->next = hashline->next;
			hashline->next = newhashline;
		}
	}

	/*
	 * Third we have to insert 'newmnt' into the main table
	 */

	/* find the hash table line */
	hash_index = hash(cache->h, mt->mnt_mountp);
	if (hash_index >= cache->h) {
		rcm_log_message(RCM_ERROR,
		    gettext("can't hash mountpoint."));
		free_mntstack(&newmnt);
		return (NULL);
	}
	hashline = cache->table[hash_index];

	/* if the hash table slot is empty, then this is easy */
	if (hashline == NULL) {
		hashline = (hashline_t *)malloc(sizeof (*hashline));
		if (hashline == NULL) {
			rcm_log_message(RCM_ERROR,
				gettext("can't allocate table node."));
			free_mntstack(&newmnt);
			return (NULL);
		}
		(void) memset((char *)hashline, 0, sizeof (*hashline));
		hashline->mnts = newmnt;
		cache->table[hash_index] = hashline;
		return (newmnt);
	}

	/* if the hash table slot isn't empty, find the immediate successor */
	previous = NULL;
	while ((comp = strcmp(hashline->mnts->mnt.mnt_mountp,
	    newmnt->mnt.mnt_mountp)) < 0 && hashline->next != NULL) {

		previous = hashline;
		hashline = hashline->next;
	}

	/* insert the entry */
	if (comp > 0) {

		/* insert it before the entry found */
		if ((newhashline = create_hashline(newmnt)) == NULL) {
			free_mntstack(&newmnt);
			return (NULL);
		}
		newhashline->next = hashline;
		if (previous == NULL)
			cache->table[hash_index] = newhashline;
		else
			previous->next = newhashline;

	} else if (comp < 0) {

		/* insert it after the entry found above */
		if ((newhashline = create_hashline(newmnt)) == NULL) {
			free_mntstack(&newmnt);
			return (NULL);
		}
		newhashline->next = hashline->next;
		hashline->next = newhashline;

	} else {

		/* add it to the entry found above */
		mnts = hashline->mnts;
		while (mnts->up &&
		    (strcmp(mnts->mnt.mnt_time, newmnt->mnt.mnt_time) <= 0))
			mnts = mnts->up;
		if (strcmp(mnts->mnt.mnt_time, newmnt->mnt.mnt_time) > 0) {
			hashline->mnts = newmnt;
			newmnt->up = mnts;
			newmnt->down = NULL;
			mnts->down = newmnt;
		} else {
			newmnt->down = mnts;
			newmnt->up = mnts->up;
			mnts->up = newmnt;
			if (newmnt->up)
				newmnt->up->down = newmnt;
		}
	}

	return (newmnt);
}

/*
 * cache_remove()
 *
 *	Given a cache and a node of a mountpoint stack, the mount instance is
 * removed from the cache's tables and memory for the mountpoint stack node is
 * free'ed.
 *
 *	The cache must be locked by the caller prior to calling this routine.
 */
static void
cache_remove(cache_t *cache, mntstack_t *mnt)
{
	hashline_t *hashline;
	hashline_t *previous;
	uint32_t mountp_hash;
	uint32_t spec_hash;

	/* sanity check */
	if (cache == NULL || mnt == NULL || mnt->mnt.mnt_mountp == NULL ||
	    mnt->mnt.mnt_special == NULL)
		return;

	/* If this is in the special hash table, remove it from there */
	spec_hash = hash(cache->h, mnt->mnt.mnt_special);
	if (spec_hash >= cache->h) {
		rcm_log_message(RCM_ERROR,
		    gettext("can't hash special device."));
		return;
	}
	hashline = cache->specindex[spec_hash];
	previous = NULL;
	while (hashline) {
		if (hashline->mnts &&
				hashline->mnts->mnt.mnt_special &&
				strcmp(hashline->mnts->mnt.mnt_special,
					mnt->mnt.mnt_special) == 0) {
			break;
		}
		previous = hashline;
		hashline = hashline->next;
	}
	if (hashline) {
		if (previous)
			previous->next = hashline->next;
		else
			cache->specindex[spec_hash] = hashline->next;

		/*
		 * We don't free the contents of hashline entries in the
		 * special index; just the containers.
		 */
		free(hashline);
	}

	/* Remove this from the main hash table */
	if (mnt->down) {
		mnt->down->up = mnt->up;
		if (mnt->up)
			mnt->up->down = mnt->down;
	} else {
		mountp_hash = hash(cache->h, mnt->mnt.mnt_mountp);
		if (mountp_hash >= cache->h) {
			rcm_log_message(RCM_ERROR,
			    gettext("can't hash mountpoint."));
			return;
		}
		hashline = cache->table[mountp_hash];
		previous = NULL;
		while (hashline) {
			if (hashline->mnts && hashline->mnts->mnt.mnt_mountp &&
			    strcmp(mnt->mnt.mnt_mountp,
			    hashline->mnts->mnt.mnt_mountp) == 0)
				break;
			else {
				previous = hashline;
				hashline = hashline->next;
			}
		}
		if (hashline == NULL) {
			rcm_log_message(RCM_ERROR,
			    gettext("can't find entry to remove."));
			return;
		}
		if (mnt->up) {
			hashline->mnts = mnt->up;
			hashline->mnts->down = NULL;
		} else {
			if (previous) {
				previous->next = hashline->next;
			} else {
				cache->table[mountp_hash] = hashline->next;
			}
			free(hashline);
		}
	}

	/* Free the mount stack */
	mnt->down = NULL;
	mnt->up = NULL;
	free_mntstack(&mnt);
}

/*
 * cache_lookup()
 *
 *	Attempts to find the named resource (which could be a special device
 * or a mountpoint) in the cache.  If the resource is a special device, then
 * a pointer to the mountpoint stack node which represents the mount of the
 * device will be returned.  If the resource is a mountpoint, then the
 * base of the mountpoint stack will be returned.  If the resource is not
 * found, then a NULL pointer will be returned.
 *
 *	The cache must be locked by the caller prior to calling this routine.
 */
static mntstack_t *
cache_lookup(cache_t *cache, char *rsrc)
{
	uint32_t hash_index;
	hashline_t *hashline;

	/*
	 * regardless of whether this is a special device or a mountpoint,
	 * hash() will work the same.
	 */
	hash_index = hash(cache->h, rsrc);
	if (hash_index >= cache->h) {
		rcm_log_message(RCM_ERROR, gettext("can't hash resource."));
		return (NULL);
	}

	/* first, assume its a special device and look in the special index */
	hashline = cache->specindex[hash_index];
	while (hashline) {
		if (hashline->mnts && hashline->mnts->mnt.mnt_special &&
		    strcmp(hashline->mnts->mnt.mnt_special, rsrc) == 0)
			return (hashline->mnts);
		hashline = hashline->next;
	}

	/* the above search must have failed; check the main table */
	hashline = cache->table[hash_index];
	while (hashline) {
		if (hashline->mnts && hashline->mnts->mnt.mnt_mountp &&
		    strcmp(hashline->mnts->mnt.mnt_mountp, rsrc) == 0)
			return (hashline->mnts);
		else
			hashline = hashline->next;
	}

	return (NULL);
}

/*
 * cache_lookup_sync()
 *
 *	This makes sure the cache is in sync, and then attempts to lookup the
 * resource within the cache.
 *
 *	The cache must be locked by the caller prior to calling this routine.
 */
static mntstack_t *
cache_lookup_sync(rcm_handle_t *hd, cache_t **cache, char *rsrc)
{
	mntstack_t *mnt;

	/* Make sure the cache is in sync with reality */
	cache_sync(hd, cache);

	/* Now use the main lookup routine */
	if ((mnt = cache_lookup(*cache, rsrc)) == NULL) {
		rcm_log_message(RCM_ERROR,
		    gettext("don't recognize resource \"%s\""), rsrc);
		return (NULL);
	}
	return (mnt);
}

/*
 * cache_dependent()
 *
 *	The name of the resource which is the immediate dependent of the
 * given mountpoint stack node is returned.  Storage is allocated for the
 * string returned, and must be free'ed by the caller.  It's always allocated
 * so that it is independent from the cache, thus the cache could be unlocked
 * by the caller and possibly changed in some parallel thread while the string
 * is still being used.
 *
 *	The cache must be locked by the caller prior to calling this routine.
 */
static void
cache_dependent(mntstack_t *mnt, char **dependent, int *dep_type)
{
	if (mnt == NULL || dependent == NULL || dep_type == NULL) {
		return;
	}
	if (mnt->up) {
		*dependent = strdup(mnt->up->mnt.mnt_special);
		*dep_type = MNT_SPECIAL;
		if (strcmp(mnt->up->mnt.mnt_fstype, "lofs") == 0)
			*dep_type = MNT_MOUNTP;
		return;
	} else {
		if (strcmp(mnt->mnt.mnt_mountp, "/") == 0) {
			*dependent = NULL;
			*dep_type = 0;
			return;
		} else {
			*dependent = strdup(mnt->mnt.mnt_mountp);
			*dep_type = MNT_MOUNTP;
			return;
		}
	}
}

/*
 * cache_walk()
 *
 *	Perform one step of a walk through the cache.  The i, line, and stack
 * parameters are updated to store progress of the walk for future steps.  They
 * must all be initialized for the beginning of the walk (i == 0, line == NULL,
 * and stack == NULL).  Initialize variables to these values for these
 * parameters, and then pass in the address of each of the three variables
 * along with the cache.  A NULL return value will be given to indicate when
 * there are no more cached items to be returned.
 *
 *	The cache must be locked by the caller prior to calling this routine.
 */
static struct mnttab *
cache_walk(cache_t *cache, uint32_t *i, hashline_t **line, mntstack_t **stack)
{
	uint32_t j;

	/* sanity check */
	if (cache == NULL || i == NULL || line == NULL || stack == NULL ||
	    *i >= cache->h)
		return (NULL);

	/* if initial values were given, look for the first entry */
	if (*i == 0 && *line == NULL && *stack == NULL) {
		for (j = 0; j < cache->h; j++) {
			if (cache->table[j]) {
				*i = j;
				*line = cache->table[j];
				*stack = (*line)->mnts;
				return (&((*stack)->mnt));
			}
		}
	}

	/* otherwise, look for the next entry */
	else {
		/* look for the next entry in the current stack */
		if (*stack && (*stack)->down) {
			*stack = (*stack)->down;
			return (&((*stack)->mnt));
		}

		/* next, look further in the current hash table line */
		else if (*line && (*line)->next) {
			*line = (*line)->next;
			*stack = (*line)->mnts;
			return (&((*stack)->mnt));
		}

		/* next look further down in the hash table */
		else {
			for (j = (*i) + 1; j < cache->h; j++) {
				if (cache->table[j]) {
					*i = j;
					*line = cache->table[j];
					*stack = (*line)->mnts;
					return (&((*stack)->mnt));
				}
			}
		}
	}

	/*
	 * We would have returned somewhere above if there were any more
	 * entries.  So set the sentinel values and return a NULL.
	 */
	*i = cache->h;
	*line = NULL;
	*stack = NULL;
	return (NULL);
}

/*
 * Miscellaneous Functions
 */

/*
 * hash()
 *
 *	A naive hashing function that converts a string 's' to an index
 * in a hash table of size 'h'.  It seems to spread entries around well enough.
 */
static uint32_t
hash(uint32_t h, char *s)
{
	uint32_t sum = 0;
	char *byte;

	if ((byte = s) != NULL) {
		while (*byte) {
			sum += (uint32_t)*byte;
			byte++;
		}
	}
	return (sum % h);
}

/*
 * register_mountpoint()
 *
 *	Register a mnttab entry's mountpoint, after applying the proper filter.
 */
static void
register_mountpoint(rcm_handle_t *hd, struct mnttab *mnt)
{
	char *msg_regfail;

	/* Sanity check */
	if (mnt == NULL || mnt->mnt_mountp == NULL)
		return;

	/* We register all non-root mount points */
	if (strcmp(mnt->mnt_mountp, "/") != 0 &&
	    rcm_register_interest(hd, mnt->mnt_mountp, RCM_FILESYS, NULL)
	    != RCM_SUCCESS) {
		msg_regfail = gettext(MSG_REGFAIL);
		rcm_log_message(RCM_ERROR, msg_regfail, mnt->mnt_mountp);
	}
}

/*
 * register_special()
 *
 *	Register a mnttab entry's special device, after applying the proper
 * filter.
 */
static void
register_special(rcm_handle_t *hd, struct mnttab *mnt)
{
	uint_t flag = 0;
	char *msg_regfail;

	/* Sanity check */
	if (mnt == NULL || mnt->mnt_special == NULL || mnt->mnt_fstype == NULL)
		return;

	/*
	 * We register all non-root loopback special devices and special
	 * devices in the /dev or /devices directories.
	 */
	if ((strncmp(mnt->mnt_special, "/dev", 4) == 0 ||
	    strcmp(mnt->mnt_fstype, "lofs") == 0) &&
	    strcmp(mnt->mnt_special, "/") != 0) {
		if (strcmp(mnt->mnt_fstype, "lofs") == 0)
			flag |= RCM_FILESYS;
		if (rcm_register_interest(hd, mnt->mnt_special, flag, NULL)
		    != RCM_SUCCESS) {
			msg_regfail = gettext(MSG_REGFAIL);
			rcm_log_message(RCM_ERROR, msg_regfail,
			    mnt->mnt_special);
		}
	}
}

/*
 * unregister_special()
 *
 *	Unregister a mnttab entry's special device, after applying the proper
 * filter.
 */
static void
unregister_special(rcm_handle_t *hd, struct mnttab *mnt)
{
	/* Sanity check */
	if (mnt == NULL)
		return;

	/*
	 * We register all non-root loopback special devices and special
	 * devices in the /dev or /devices directories.
	 */
	if ((strncmp(mnt->mnt_special, "/dev", 4) == 0 ||
	    strcmp(mnt->mnt_fstype, "lofs") == 0) &&
	    strcmp(mnt->mnt_special, "/") != 0) {
		(void) rcm_unregister_interest(hd, mnt->mnt_special, 0);
		rcm_log_message(RCM_TRACE2, "unregistered special \"%s\"\n",
		    mnt->mnt_special);
	}
}

/*
 * unregister_mountpoint()
 *
 *	Unregister a mnttab entry's mountpoint, after applying the proper
 * filter.
 */
static void
unregister_mountpoint(rcm_handle_t *hd, struct mnttab *mnt)
{
	/* Sanity check */
	if (mnt == NULL)
		return;

	/* We register all non-root mount points */
	if (strcmp(mnt->mnt_mountp, "/") != 0) {
		(void) rcm_unregister_interest(hd, mnt->mnt_mountp,
		    RCM_FILESYS);
		rcm_log_message(RCM_TRACE2, "unregistered mountpoint \"%s\"\n",
		    mnt->mnt_mountp);
	}
}


/*
 * system1()
 *
 *	An MT-Safe version of system() that uses fork1() instead of vfork().
 */
static int
system1(const char *s)
{
	struct sigaction sa_dfl;
	struct sigaction sa_ign;
	struct sigaction sa_sigchld;
	struct sigaction sa_sigint;
	struct sigaction sa_sigquit;
	struct stat st;
	sigset_t mask;
	sigset_t mask_old;
	pid_t pid;
	int saved_errno;
	int status;
	int w;
	static char shpath[] = { "/bin/sh" };
	static char shell[] = { "sh" };

	/* Check the requested command */
	if (s == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/* Check the ability to execute a shell from this process */
	if (stat(shpath, &st) < 0) {
		rcm_log_message(RCM_ERROR,
		    gettext("Failure executing \"%s\"; check \"%s\"."), s,
		    shpath);
		errno = ENOENT;
		return (-1);
	}

	if (((geteuid() == st.st_uid) && ((st.st_mode & S_IXUSR) == 0)) ||
	    ((getegid() == st.st_gid) && ((st.st_mode & S_IXGRP) == 0)) ||
	    ((st.st_mode & S_IXOTH) == 0)) {
		errno = EPERM;
		return (-1);
	}

	/* Initialize default/ignore actions */
	(void) memset((char *)&sa_dfl, 0, sizeof (sa_dfl));
	(void) memset((char *)&sa_ign, 0, sizeof (sa_ign));
	sa_dfl.sa_handler = SIG_DFL;
	sa_ign.sa_handler = SIG_IGN;

	/*
	 * Set the SIGCHLD handler to "default"; save the old signal handler
	 * for for later restoration.
	 */
	(void) sigemptyset(&mask);
	(void) sigaddset(&mask, SIGCHLD);
	(void) sigprocmask(SIG_BLOCK, &mask, &mask_old);
	(void) sigaction(SIGCHLD, &sa_dfl, &sa_sigchld);

	/* Fork off the child process (using fork1(), because it's MT-safe) */
	switch (pid = fork1()) {
	case -1:
		/* Error */
		saved_errno = errno;
		(void) sigaction(SIGCHLD, &sa_sigchld, NULL);
		(void) sigprocmask(SIG_SETMASK, &mask_old, NULL);
		errno = saved_errno;
		return (-1);

	case 0:
		/* Child */
		(void) sigprocmask(SIG_SETMASK, &mask_old, NULL);
		(void) execl(shpath, shell, (const char *)"-c", s,
		    (char *)0);
		_exit(-1);
		break;

	default:
		/* Parent */
		break;
	}

	/*
	 * Set the SIGINT and SIGQUIT handlers to "ignore"; save the old
	 * handlers for later restoration.
	 */
	(void) sigaction(SIGINT, &sa_ign, &sa_sigint);
	(void) sigaction(SIGQUIT, &sa_ign, &sa_sigquit);

	/* Wait for the child process to complete */
	do {
		w = waitpid(pid, &status, _WNOCHLD);
	} while (w == -1 && errno == EINTR);

	/* Restore signal settings */
	saved_errno = errno;
	(void) sigaction(SIGINT, &sa_sigint, NULL);
	(void) sigaction(SIGQUIT, &sa_sigquit, NULL);
	(void) sigaction(SIGCHLD, &sa_sigchld, NULL);
	(void) sigprocmask(SIG_SETMASK, &mask_old, NULL);
	errno = saved_errno;

	/* Return the status of the child process */
	if (w == -1)
		return (-1);
	else
		return (status);
}

/*
 * is_critical()
 *
 *	Tests a mnttab entry to determine if it's critical to the system, and
 * thus cannot be suspended.
 */
static int
is_critical(struct mnttab *mnt)
{
	/* Protect from bad arguments */
	assert(mnt != NULL);
	assert(mnt->mnt_mountp != NULL);

	if ((strcmp(mnt->mnt_mountp, "/") == 0) ||
	    (strcmp(mnt->mnt_mountp, "/usr") == 0) ||
	    (strcmp(mnt->mnt_mountp, "/usr/lib") == 0) ||
	    (strcmp(mnt->mnt_mountp, "/usr/bin") == 0) ||
	    (strcmp(mnt->mnt_mountp, "/tmp") == 0) ||
	    (strcmp(mnt->mnt_mountp, "/var") == 0) ||
	    (strcmp(mnt->mnt_mountp, "/var/run") == 0) ||
	    (strcmp(mnt->mnt_mountp, "/etc") == 0) ||
	    (strcmp(mnt->mnt_mountp, "/etc/mnttab") == 0) ||
	    (strcmp(mnt->mnt_mountp, "/sbin") == 0))
		return (1);

	return (0);
}

/*
 * is_forcible()
 *
 *	Given a mnttab entry and the current RCM flags, tests whether an
 * operation should be forced.
 */
#if	defined(RCM_FILESYS_AUTOMATION)
static int
is_forcible(struct mnttab *mnt, uint_t flags)
{
	/* Protect from bad arguments */
	assert(mnt != NULL);
	assert(mnt->mnt_fstype != NULL);

	if ((flags & RCM_FORCE) &&
	    (strcmp(mnt->mnt_fstype, "ufs") == 0 ||
	    strcmp(mnt->mnt_fstype, "nfs") == 0))
		return (1);

	return (0);
}
#endif

/*
 * format_special()
 *
 *	Returns a string representing the special device.  If the special_only
 * flag is specified, the string is wrapped in parentheses.  Otherwise, it's
 * not so that it might be included with other strings already formatted in
 * parentheses.
 */
static char *
format_special(char *rsrc, char *special, uint_t special_only)
{
	int len;
	char *s;
	char *fmt;

	if (strcmp(rsrc, special) == 0)
		return (strdup(""));

	if (special_only)
		fmt = gettext(MSG_SPECIAL1);
	else
		fmt = gettext(MSG_SPECIAL2);

	len = strlen(fmt) + strlen(special) + 1;
	if ((s = (char *)malloc(len)) != NULL)
		(void) snprintf(s, len, fmt, special);

	return (s);
}
