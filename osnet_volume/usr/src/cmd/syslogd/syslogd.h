/*
 *  		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 *  	Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	_SYSLOGD_H
#define	_SYSLOGD_H

#pragma ident "@(#)syslogd.h 1.18	99/10/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

	struct utmpx dummy;	/* for sizeof ut_user, ut_line */

/*
 * Various constants & tunable values for syslogd
 */
#define	DEBUGFILE 	"/var/adm/syslog.debug"
#define	MAXLINE		1024		/* maximum line length */
#define	DEFUPRI		(LOG_USER|LOG_INFO)
#define	DEFSPRI		(LOG_KERN|LOG_CRIT)
#define	MARKCOUNT	3		/* ratio of minor to major marks */
#define	UNAMESZ		(sizeof (dummy.ut_user)) /* length of a login name */
#define	UDEVSZ		(sizeof (dummy.ut_line)) /* length of login dev name */
#define	MAXUNAMES	20		/* maximum number of user names */
#define	Q_HIGHWATER_MARK 10000		/* max outstanding msgs per file */
#define	NOPRI		0x10		/* the "no priority" priority */
#define	LOG_MARK	(LOG_NFACILITIES << 3)	/* mark "facility" */

/*
 * host_list_t structure contains a list of hostnames for a given address
 */
typedef	struct	host_list {
	int	hl_cnt;			/* number of hl_hosts entries */
	char	**hl_hosts;		/* hostnames */
} host_list_t;

/*
 * statistics structure attached to each filed for debugging
 */
typedef struct filed_stats {
	int	flag;			/* flag word */
	int	total;			/* total messages logged */
	int	dups;			/* duplicate messages */
	int 	cantfwd;		/* can't forward */
	int	errs;			/* write errors */
} filed_stats_t;


/*
 * internal representation of a log message. Includes all routing & bookkeeping
 * information for the message. created in the system & network poll routines,
 * and passed among the various processing threads as necessary
 */

typedef struct log_message {
	pthread_mutex_t msg_mutex;	/* protects this structs members */
	int refcnt;			/* message reference count */
	int source;			/* used to index interface list */
	int pri;			/* message priority */
	int flags;			/* misc flags */
	char curaddr[SYS_NMLN + 1];	/* numeric address of sender */
	time_t ts;			/* timestamp */
	host_list_t *hlp;			/* ptr to host list struct */
	char msg[MAXLINE+1];		/* the message itself */
} log_message_t;

_NOTE(MUTEX_PROTECTS_DATA(log_message_t::msg_mutex, log_message_t))
_NOTE(DATA_READABLE_WITHOUT_LOCK(log_message_t))		


/*
 * format of a saved message. For each active file we are logging
 * we save the last message and the current message, to make it
 * possible to suppress duplicates on a per file basis. Earlier
 * syslogd's used a global buffer for duplicate checking, so
 * strict per file duplicate suppression was not always possible.
 */
typedef struct saved_msg {
	int pri;
	int flags;
	time_t time;
	char host[SYS_NMLN+1];
	char msg[MAXLINE+1];
} saved_message_t;


/*
 * Flags to logmsg().
 */

#define	IGN_CONS	0x001		/* don't print on console */
#define	IGN_FILE	0x002		/* don't write to log file */
#define	SYNC_FILE	0x004		/* do fsync on file after printing */
#define	NOCOPY		0x008		/* don't suppress duplicate messages */
#define	ADDDATE		0x010		/* add a date to the message */
#define	MARK		0x020		/* this message is a mark */
#define	LOGSYNC		0x040		/* nightly log update message */
#define	NETWORK		0x100		/* message came from the net */
#define	SHUTDOWN	0x200		/* internal shutdown message */
#define	FLUSHMSG	0x400		/* internal flush message */

/*
 * This structure represents the files that will have log
 * copies printed.  There is one instance of this for each
 * file that is being logged to.
 */
struct filed {
	pthread_mutex_t filed_mutex;	/* protects this filed */
	pthread_t f_thread;		/* thread that handles this file */
	dataq_t f_queue;		/* queue of messages for this file */
	int f_queue_count;		/* count of messages on the queue */
	short	f_type;			/* entry type, see below */
	short	f_file;			/* file descriptor */
	int	f_msgflag;		/* message disposition */
	filed_stats_t f_stat;		/* statistics */
	saved_message_t f_prevmsg;	/* previous message */
	saved_message_t f_current;	/* current message */
	int	f_prevcount;		/* message repeat count */
	u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
	union {
		char	f_uname[MAXUNAMES][SYS_NMLN + 1];
		struct {
			char	f_hname[SYS_NMLN + 1];
			struct netbuf	f_addr;
		} f_forw;		/* forwarding address */
		char	f_fname[MAXPATHLEN + 1];
	} f_un;
};

_NOTE(MUTEX_PROTECTS_DATA(filed::filed_mutex, filed))
_NOTE(DATA_READABLE_WITHOUT_LOCK(filed))

/* values for f_type */
#define	F_UNUSED	0		/* unused entry */
#define	F_FILE		1		/* regular file */
#define	F_TTY		2		/* terminal */
#define	F_CONSOLE	3		/* console terminal */
#define	F_FORW		4		/* remote machine */
#define	F_USERS		5		/* list of users */
#define	F_WALL		6		/* everyone logged on */

/*
 * values for logit routine
 */
#define	CURRENT		0		/* print current message */
#define	SAVED		1		/* print saved message */
/*
 * values for f_msgflag
 */
#define	CURRENT_VALID	0x01		/* new message is good */
#define	OLD_VALID	0x02		/* old message is valid */

/*
 * code translation struct for use in processing config file
 */
struct code {
	char	*c_name;
	int	c_val;
};

/*
 * structure describing a message to be sent to the wall thread.
 * the thread id and attributes are stored in the structure
 * passed to the thread, and the thread is created detached.
 */
typedef struct wall_device {
	pthread_t thread;
	pthread_attr_t thread_attr;
	char dev[PATH_MAX + 1];
	char msg[MAXLINE+1];
	char ut_name[sizeof (dummy.ut_name)];
} walldev_t;

/*
 * function prototypes
 */
int main(int argc, char **argv);
static void usage(void);
static void untty(void);
static void formatnet(struct netbuf *nbp, log_message_t *mp);
static void formatsys(struct log_ctl *lp, char *msg, int sync);
static void *logmsg(void *ap);
static void wallmsg(struct filed *f, char *from, char *msg);
static host_list_t *cvthname(struct netbuf *nbp, struct netconfig *ncp, char *);
static void set_flush_msg(struct filed *f);
static void flushmsg(int flags);
void logerror(char *type);
static void init(void);
static void conf_init(void);
static void cfline(char *line, int lineno, struct filed *f);
static int decode(char *name, struct code *codetab);
static int ismyaddr(struct netbuf *nbp);
static void getnets(void);
static void add(struct netconfig *ncp, struct netbuf *nbp);
static int logforward(struct filed *f, char *ebuf);
static int amiloghost(void);
static int same(char *a, char *b, unsigned int n);
static void *sys_poll(void *ap);
static void *net_poll(void *ap);
static log_message_t *new_msg(void);
static void free_msg(log_message_t *lm);
static void logmymsg(int pri, char *msg, int flags);
static void *logit(void *ap);
static void freehl(host_list_t *h);
static int filed_init(struct filed *h);
static void copy_msg(struct filed *f);
static void dumpstats(int fd);
static void filter_string(char *orig, char *new);
static int openklog(char *name, int mode);
static void writemsg(int selection, struct filed *f);
static void *writetodev(void *ap);
static void shutdown_msg(void);
static void server(void *, char *, size_t, door_desc_t *, uint_t);
static char *alloc_stacks(int);
static void dealloc_stacks(int);
static int checkm4(void);
static void filed_destroy(struct filed *f);
static void open_door(void);
static void close_door(void);
#ifdef	__cplusplus
}
#endif

#endif /* _SYSLOGD_H */
