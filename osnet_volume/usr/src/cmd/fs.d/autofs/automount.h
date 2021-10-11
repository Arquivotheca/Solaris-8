/*
 *	automount.h
 *
 *	Copyright (c) 1988-1996,1999 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ifndef	_AUTOMOUNT_H
#define	_AUTOMOUNT_H

#pragma ident	"@(#)automount.h	1.55	99/09/27 SMI"

#include <fslib.h>		/* needed for mntlist_t declaration */
#include <thread.h>
#include <sys/mntent.h>		/*    "    "  MNTTYPE_* declarations */
#include <synch.h>		/* needed for mutex_t declaration */
#include <sys/types.h>
#include <rpc/rpc.h>
#include <rpcsvc/autofs_prot.h>
#include <netinet/in.h>		/* needed for sockaddr_in declaration */

#ifdef MALLOC_DEBUG
#include <debug_alloc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _REENTRANT
#define	fork1			vfork
#define	rpc_control(a, b)	1
#endif

#define	MXHOSTNAMELEN	64
#define	MAXNETNAMELEN   255
#define	MAXFILENAMELEN  255
#define	LINESZ		4096
#define	MAXADDRLEN	128		/* max autofs address length */
#define	MAXOPTSLEN	1024

#define	AUTOFS_MOUNT_TIMEOUT	600	/* default min time mount will */
					/* remain mounted (in seconds) */
#define	AUTOFS_RPC_TIMEOUT	60	/* secs autofs will wait for */
					/* automountd's reply before */
					/* retransmitting */
/* stack ops */
#define	ERASE		0
#define	PUSH		1
#define	POP		2
#define	INIT		3
#define	STACKSIZ	30

#define	FNPREFIX	"-xfn"
#define	FNPREFIXLEN	4

#define	DIST_SELF	1
#define	DIST_MYSUB	2
#define	DIST_MYNET	3
#define	DIST_OTHER	4

#define	MAXIFS		32
#define	BROWSE		"browse"
#define	NOBROWSE	"nobrowse"

/*
 * Retry operation related definitions.
 */
#define	RET_OK		0
#define	RET_RETRY	32
#define	RET_ERR		33
#define	INITDELAY	5
#define	DELAY_BACKOFF	2
#define	MAXDELAY	120
#define	DELAY(delay) { \
	(void) sleep(delay); \
	delay *= DELAY_BACKOFF; \
	if (delay > MAXDELAY) \
		delay = MAXDELAY; \
}

struct mapline {
	char linebuf[LINESZ];
	char lineqbuf[LINESZ];
};

/*
 * Structure describing a host/filesystem/dir tuple in a NIS map entry
 */
struct mapfs {
	struct mapfs *mfs_next;	/* next in entry */
	int 	mfs_ignore;	/* ignore this entry */
	char	*mfs_host;	/* host name */
	char	*mfs_dir;	/* dir to mount */
	int	mfs_penalty;	/* mount penalty for this host */
	int	mfs_distance;	/* distance hint */
	struct nfs_args *mfs_args;	/* nfs_args */
	struct netconfig *mfs_nconf;
	rpcvers_t	mfs_version;	/* NFS version */

#define	MFS_ALLOC_DIR		0x1	/* mfs_dir now points to different */
					/* buffer */

#define	MFS_URL			0x2	/* is NFS url listed in this tuple. */
#define	MFS_FH_VIA_WEBNFS	0x4	/* got file handle during ping phase */

	uint_t	mfs_flags;
	uint_t	mfs_port;	/* port# in NFS url */
};

/*
 * NIS entry - lookup of name in DIR gets us this
 */
struct mapent {
	char	*map_fstype;	/* file system type e.g. "nfs" */
	char	*map_mounter;	/* base fs e.g. "cachefs" */
	char	*map_root;	/* path to mount root */
	char	*map_mntpnt;	/* path from mount root */
	char	*map_mntopts;	/* mount options */
	char    *map_fsw;	/* mount fs information */
	char    *map_fswq;	/* quoted mountfs information */
	int	map_mntlevel;	/* mapentry hierarchy level */
	bool_t	map_modified;	/* flags modified mapentries */
	bool_t	map_faked;	/* flags faked mapentries */
	int	map_err;	/* flags any bad entries in the map */
	struct mapfs *map_fs;	/* list of replicas for nfs */
	struct mapent *map_next;
};


/*
 * FNS implementation of getmapent() may return either a mapent list or
 * a symbolic link.
 */
typedef enum {FN_NONE, FN_MAPENTS, FN_SYMLINK} fn_res_type;
typedef struct {
	fn_res_type		type;
	union {
		struct mapent	*mapents;
		char		*symlink;
	} m_or_l;
} getmapent_fn_res;

/*
 * Descriptor for each directory served by the automounter
 */
struct autodir {
	char	*dir_name;		/* mount point */
	char	*dir_map;		/* name of map for dir */
	char	*dir_opts;		/* default mount options */
	int 	dir_direct;		/* direct mountpoint ? */
	int 	dir_remount;		/* a remount */
	struct autodir *dir_next;	/* next entry */
	struct autodir *dir_prev;	/* prev entry */
};

/*
 * This structure is used to build an array of
 * hostnames with associated penalties to be
 * passed to the nfs_cast procedure
 */
struct host_names {
	char *host;
	int  penalty;
};

/*
 * structure used to build list of contents for a map on
 * a readdir request
 */
struct dir_entry {
	char		*name;		/* name of entry */
	ino_t		nodeid;
	off_t		offset;
	struct dir_entry *next;
	struct dir_entry *left;		/* left element in binary tree */
	struct dir_entry *right;	/* right element in binary tree */
};

/*
 * offset index table
 */
struct off_tbl {
	off_t			offset;
	struct dir_entry	*first;
	struct off_tbl		*next;
};

/*
 * directory cache for 'map'
 */
struct rddir_cache {
	char			*map;
	struct off_tbl		*offtp;
	ulong_t			bucket_size;
	time_t			ttl;
	struct dir_entry	*entp;
	mutex_t			lock;		/* protects 'in_use' field */
	int			in_use;		/* # threads referencing it */
	rwlock_t		rwlock;		/* protects 'full' and 'next' */
	int			full;		/* full == 1 when cache full */
	struct rddir_cache	*next;
};

#define	RDDIR_CACHE_TIME	300		/* in seconds */

/*
 * structure used to maintain address list for localhost
 */

struct myaddrs {
	struct sockaddr_in sin;
	struct myaddrs *myaddrs_next;
};

extern time_t timenow;	/* set at start of processing of each RPC call */
extern char self[];
extern int verbose;
extern int trace;
extern int automountd_nobrowse;
extern int sys_nerr;
extern char *sys_errlist[];
extern struct autodir *dir_head;
extern struct autodir *dir_tail;
extern struct mntlist *current_mounts;
extern struct autofs_args;
extern struct mounta;			/* defined in sys/vfs.h */
extern struct myaddrs *myaddrs_head;

extern rwlock_t	cache_lock;
extern rwlock_t portmap_cache_lock;
extern rwlock_t rddir_cache_lock;

extern mutex_t cleanup_lock;
extern cond_t cleanup_start_cv;
extern cond_t cleanup_done_cv;

/*
 * mnttab handling routines
 */
extern void free_mapent(struct mapent *);
extern struct mntlist *getmntlist(void);
extern dev_t get_devid(struct extmnttab *);

/*
 * utilities
 */
extern struct mapent *parse_entry(char *, char *, char *, struct mapline *,
				char *, uint_t, bool_t);
extern int macro_expand(char *, char *, char *, int);
extern void unquote(char *, char *);
extern void trim(char *);
extern char *get_line(FILE *, char *, char *, int);
extern int getword(char *, char *, char **, char **, char, int);
extern int get_retry(char *);
extern int str_opt(struct mnttab *, char *, char **);
extern void dirinit(char *, char *, char *, int, char **, char ***);
extern void pr_msg();
extern void trace_prt(int, char *, ...);
extern void free_autofs_args(struct autofs_args *);
extern void free_mounta(struct mounta *);

extern int nopt(struct mnttab *, char *, int *);
extern int set_versrange(rpcvers_t, rpcvers_t *, rpcvers_t *);
extern enum clnt_stat pingnfs(char *, int, rpcvers_t *, rpcvers_t,
	ushort_t, bool_t, char *);

extern int self_check(char *);
extern int do_mount1(char *, char *, char *, char *, char *, uint_t,
	action_list **, struct authunix_parms *);
extern int do_lookup1(char *, char *, char *, char *, char *, uint_t,
	enum autofs_action *, struct linka *, struct authunix_parms *);
extern int do_unmount1(umntrequest *);
extern int do_readdir(struct autofs_rddirargs *, struct autofs_rddirres *,
	struct authunix_parms *);
extern int nfsunmount(struct mnttab *);
extern int loopbackmount(char *, char *, char *, int);
extern int mount_nfs(struct mapent *, char *, char *, int);
extern int mount_autofs(struct mapent *, char *, action_list *,
	char *root, char *subdir, char *key);
extern int mount_generic(char *, char *, char *, char *, int);
extern enum clnt_stat nfs_cast(struct mapfs *, struct mapfs **, int);
extern int svc_create_local_service(void (*) (), ulong_t, ulong_t,
	char *, char *);

/*
 * readdir handling routines
 */
extern char *auto_rddir_malloc(unsigned);
extern char *auto_rddir_strdup(const char *);
extern struct dir_entry *btree_lookup(struct dir_entry *, char *);
extern void btree_enter(struct dir_entry **, struct dir_entry *);
extern int add_dir_entry(char *, struct dir_entry **, struct dir_entry **);
extern void cache_cleanup(void);
extern int rddir_cache_lookup(char *, struct rddir_cache **);
extern struct dir_entry *rddir_entry_lookup(char *, struct dir_entry *);

/*
 * generic interface to specific name service functions
 */
extern void ns_setup(char **, char ***);
extern int getmapent(char *, char *, struct mapline *, char **, char ***,
			bool_t *);
extern int getmapkeys(char *, struct dir_entry **, int *, int *, char **,
			char ***, uid_t);
extern int loadmaster_map(char *, char *, char **, char ***);
extern int loaddirect_map(char *, char *, char *, char **, char ***);

/*
 * these name service specific functions should not be
 * accessed directly, use the generic functions.
 */
extern void init_files(char **, char ***);
extern int getmapent_files(char *, char *, struct mapline *, char **, char ***,
				bool_t *);
extern int loadmaster_files(char *, char *, char **, char ***);
extern int loaddirect_files(char *, char *, char *, char **, char ***);
extern int getmapkeys_files(char *, struct dir_entry **, int *, int *,
	char **, char ***);
extern int stack_op(int, char *, char **, char ***);

extern void init_nisplus(char **, char ***);
extern int getmapent_nisplus(char *, char *, struct mapline *, char **,
				char ***, bool_t *);
extern int loadmaster_nisplus(char *, char *, char **, char ***);
extern int loaddirect_nisplus(char *, char *, char *, char **, char ***);
extern int getmapkeys_nisplus(char *, struct dir_entry **, int *, int *,
	char **, char ***);

extern void init_nis(char **, char ***);
extern int getmapent_nis(char *, char *, struct mapline *, char **, char ***,
				bool_t *);
extern int loadmaster_nis(char *, char *, char **, char ***);
extern int loaddirect_nis(char *, char *, char *, char **, char ***);
extern int getmapkeys_nis(char *, struct dir_entry **, int *, int *,
	char **, char ***);

extern void init_ldap(char **, char ***);
extern int getmapent_ldap(char *, char *, struct mapline *, char **, char ***,
				bool_t *);
extern int loadmaster_ldap(char *, char *, char **, char ***);
extern int loaddirect_ldap(char *, char *, char *, char **, char ***);
extern int getmapkeys_ldap(char *, struct dir_entry **, int *, int *,
	char **, char ***);


/*
 * end of name service specific functions
 */

/*
 * not defined in any header file
 */
extern bool_t xdr_uid_t(XDR *, uid_t *);
extern int __rpc_control(int, char *);
extern int __clnt_bindresvport(CLIENT *);
extern int getnetmaskbynet(const struct in_addr, struct in_addr *);

/*
 * Hidden rpc functions
 */
extern int __nis_reset_state();
extern int __rpc_negotiate_uid(int);
extern int __rpc_get_local_uid(SVCXPRT *, uid_t *);

#ifdef __cplusplus
}
#endif

#endif	/* _AUTOMOUNT_H */
