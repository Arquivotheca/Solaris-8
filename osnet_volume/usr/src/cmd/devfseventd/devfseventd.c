/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)devfseventd.c	1.2	98/12/01 SMI"

/*
 *	Device configuration event Daemon
 *	Gets event messages via door upcall from kernel
 *	and distributes them to registered consumers
 *	via a second door_call entry point.
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <time.h>
#include <strings.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/instance.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <thread.h>
#include <synch.h>
#include <procfs.h>
#include <door.h>
#include <locale.h>
#include <sys/modctl.h>
#include <sys/devfs_log_event.h>


#define	MAX_CONSUMERS		4
#define	RESERVED_PID		-1
#define	CONSUMER_LISTSIZE	(MAX_CONSUMERS + 1)
#define	DEFAULT_CONSUMER	(MAX_CONSUMERS)

typedef struct eventd_msg {
	char			*msg;
	int			refcnt;
	char			status[CONSUMER_LISTSIZE];
	struct eventd_msg	*next;
} eventd_msg_t;


/* values for event msg status */
#define	EVENT_MSG_IGNORE	0
#define	EVENT_MSG_PENDING	1

/*
 * XXX need to add per-process registered criteria
 */
typedef struct eventd_mbox {
	int		pid;
	char		procname[PRFNSZ+1];
	sema_t		sema_msg;
	eventd_msg_t	*next_msg;
} eventd_mbox_t;


static eventd_mbox_t consumers[CONSUMER_LISTSIZE];
static int nconsumers = 0;
static int need_qcleanup = 0;
static eventd_msg_t *msg_qhead = NULL;
static int debug_level = 0;
static int door_upcall_retval;

#define	DEBUG_LEVEL_FORK	9	/* will run in background at all */
					/* levels less than DEBUG_LEVEL_FORK */

#define	MAX_SERVER_THREADS	0

/* function prototypes */
static void dispatch_message(void);
static void dispatch(char *);
static void door_upcall(void *, char *, size_t, door_desc_t *, uint_t);
static void door_services(void *, char *, size_t, door_desc_t *, uint_t);
static int enqueue_msg(char *mptr);
static void create_pool(void);
static void my_create(door_info_t *door_info);
static void qstats(void);
static void usage(void);
static void catch_sig(void);
static void clean_msg_queue(void);
static void unload_queued_events(int);
extern int modctl(int, ...);
static void devfseventd_print(int level, char *message, ...);

static int upcall_door;
static int service_door;

#ifdef READ_CONFIG_FILE
static void config_read(void);
#endif /* READ_CONFIG_FILE */

/*
 * uploaded event(s) from the kernel via door_upcall() will be copied
 * to this buffer before they are queued.
 */
static char eventbuf[LOGEVENT_BUFSIZE];

/*
 * sema_eventbuf - semaphore guards against the global buffer eventbuf[]
 * being written to before the previous uploaded event message contained
 * in eventbuf has been queued on to the message queue.
 *
 * sema_dispatch - this semaphore synchronizes between the producer kernel
 * thread log_event_deliver() and the consumer dispatch_message() userland
 * thread.
 *
 * server_mutex - this mutex protects both the event message queue and the
 * consumer[] data structures from being manipulated simultaneously.
 *
 * consumers[id].sema_msg - this semaphore keeps a count of event messages
 * queued for a consumer.  Once all event messages have been consumed the
 * next request blocks on this semaphore waiting for events to be uploaded
 * from the kernel.
 */
static sema_t sema_eventbuf, sema_dispatch;
static mutex_t server_mutex;
static thread_t msgid;

static int logflag = 0;
static char *prog;

static int hold_daemon_lock;
#define	DAEMON_LOCK_FILE "/dev/.devfseventd_daemon.lock"
static const char *daemon_lock_file = DAEMON_LOCK_FILE;
static int daemon_lock_fd;
static pid_t enter_daemon_lock(void);
static void exit_daemon_lock(void);

static void
usage() {
	(void) fprintf(stderr, "usage: devfseventd [-d <debug_level>]\n");
	(void) fprintf(stderr, "higher debug levels get progressively more");
	(void) fprintf(stderr, "detailed debug information.\n\n");
	(void) fprintf(stderr, "devfseventd will run in background if run");
	(void) fprintf(stderr, "with a debug_level less than %d.\n",
			DEBUG_LEVEL_FORK);
	exit(2);
}


/* common exit function which ensures releasing locks */
static void
devfseventd_exit(int status)
{
	devfseventd_print(1, "exit status = %d\n", status);

	if (hold_daemon_lock) {
		exit_daemon_lock();
	}

	exit(status);
}


/*
 * When SIGHUP is received, re-read config file and dump current state.
 */
void
catch_sig(void)
{

#ifdef READ_CONFIG_FILE
	config_read();
#endif /* READ_CONFIG_FILE */

	qstats();
	devfseventd_print(2, "debug_level = %d\n", debug_level);
	devfseventd_print(2,
		"sema_eventbuf.count = %d\n", sema_eventbuf.count);
	devfseventd_print(2, "sema_dispatch.count = %d\n",
		sema_dispatch.count);
}

void
main(int argc, char **argv)
{
	int c;
	int i;
	int dfd;
	struct sigaction	act;
	extern char *optarg;
	struct stat buf;
	sigset_t mask;

	(void) sigfillset(&mask);
	thr_sigsetmask(SIG_BLOCK, &mask, NULL);

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	if (getuid() != 0) {
		(void) fprintf(stderr, "Must be root to run devfseventd\n");
		devfseventd_exit(1);
	}

	if (argc > 3) {
		usage();
	}

	if ((prog = strrchr(argv[0], '/')) == NULL) {
		prog = argv[0];
	} else {
		prog++;
	}

	if ((c = getopt(argc, argv, "d:")) != EOF) {
		switch (c) {
		case 'd':
			debug_level = atoi(optarg);
			break;
		case '?':
		default:
			usage();
		}
	}

	/* set up our signal handlers */
	act.sa_handler = catch_sig;
	(void) sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	(void) sigaction(SIGHUP, &act, NULL);

	/* demonize ourselves */
	if (debug_level < DEBUG_LEVEL_FORK) {
		pid_t pid;

		if (fork()) {
			devfseventd_exit(0);
		}

		/* only one daemon can run at a time */
		if ((pid = enter_daemon_lock()) != getpid()) {
			devfseventd_print(0,
			    "event daemon pid %d already running\n", pid);
			exit(3);
		}

		(void) chdir("/");

		(void) setsid();
		if (debug_level <= 1) {
			for (i = 0; i < 3; i++) {
				(void) close(i);
			}
			(void) open("/dev/null", 0);
			(void) dup2(0, 1);
			(void) dup2(0, 2);
			logflag = 1;
		}
	}

	openlog("devfseventd", LOG_PID, LOG_DAEMON);

	devfseventd_print(8,
	    "devfseventd started, debug level = %d\n", debug_level);
	devfseventd_print(8,
	    "LOGEVENT_DOOR_UPCALL = %s\n", LOGEVENT_DOOR_UPCALL);

	/* set door file name */
	if (modctl(MODEVENTS,
	    (uintptr_t)MODEVENTS_SET_DOOR_UPCALL_FILENAME,
	    (uintptr_t)LOGEVENT_DOOR_UPCALL, NULL, NULL, 0) < 0) {
		devfseventd_print(0,
		    "MODEVENTS_SET_DOOR_UPCALL_FILENAME - %s\n",
		    strerror(errno));
		devfseventd_exit(-1);
	}

	/*
	 * initialize local data structures.
	 */
	for (i = 0; i < MAX_CONSUMERS; i++) {
		consumers[i].pid = 0;
		consumers[i].next_msg = NULL;
	}

	/* initialize semaphores */
	(void) sema_init(&sema_eventbuf, 1, USYNC_THREAD, NULL);
	(void) sema_init(&sema_dispatch, 0, USYNC_THREAD, NULL);

	devfseventd_print(8, "start the message thread running\n");

	if (thr_create(NULL, NULL, (void *(*)(void *))dispatch_message,
				(void *)NULL, 0, &msgid) < 0) {
		devfseventd_print(0, "could not create message thread - %s\n",
				strerror(errno));
		devfseventd_exit(1);
	}


#ifdef READ_CONFIG_FILE
	/*
	 * read configuration file
	 */
	config_read();
#endif /* READ_CONFIG_FILE */

	devfseventd_print(8,
		"Create a pool of server threads now\n");
	create_pool();

	devfseventd_print(8,
		"Create a door for kernel upcalls\n");

	if ((upcall_door = door_create(door_upcall, 0, 0)) < 0) {
		devfseventd_print(0,
		    "upcall door_create - %s\n", strerror(errno));
		devfseventd_exit(-1);
	}

	devfseventd_print(8, "Give it a name %s\n", LOGEVENT_DOOR_UPCALL);

	if ((dfd = open(LOGEVENT_DOOR_UPCALL, O_EXCL | O_CREAT,
	    S_IREAD|S_IWRITE)) < 0) {
		if (errno != EEXIST) {
			devfseventd_print(0, "open(%s) - %s\n",
				LOGEVENT_DOOR_UPCALL, strerror(errno));
			devfseventd_exit(-1);
		}
	} else {
		(void) close(dfd);
	}

	devfseventd_print(8, "Rebind ourselves\n");

	if (fdetach(LOGEVENT_DOOR_UPCALL) != 0) {
		devfseventd_print(2, "fdetach(%s) - %s\n",
				LOGEVENT_DOOR_UPCALL, strerror(errno));
	}
	if (fattach(upcall_door, LOGEVENT_DOOR_UPCALL) != 0) {
		/* XXX check for EAGAIN? */
		devfseventd_print(0, "fattach(%s) - %s\n",
			LOGEVENT_DOOR_UPCALL, strerror(errno));
		devfseventd_exit(-1);
	}


	devfseventd_print(8, "Create a door for message services\n");

	if ((service_door = door_create(door_services, 0, 0)) < 0) {
		devfseventd_print(0, "services door_create - %s\n",
						strerror(errno));
		devfseventd_exit(-1);
	}

	devfseventd_print(8, "Give it a name %s\n", LOGEVENT_DOOR_SERVICES);

	if ((dfd = open(LOGEVENT_DOOR_SERVICES, O_EXCL | O_CREAT,
	    S_IREAD|S_IWRITE)) < 0) {
		if (errno != EEXIST) {
			devfseventd_print(0, "open(%s) - %s\n",
				LOGEVENT_DOOR_SERVICES, strerror(errno));
			devfseventd_exit(-1);
		}
	} else {
		(void) close(dfd);
	}

	devfseventd_print(8, "Rebind ourselves\n");

	if (fdetach(LOGEVENT_DOOR_SERVICES) != 0) {
		devfseventd_print(2, "fdetach(%s) - %s\n",
				LOGEVENT_DOOR_SERVICES, strerror(errno));
	}

	if (fattach(service_door, LOGEVENT_DOOR_SERVICES) != 0) {
		/* XXX check for EAGAIN? */
		devfseventd_print(0, "fattach(%s) - %s\n",
				LOGEVENT_DOOR_SERVICES, strerror(errno));
		devfseventd_exit(-1);
	}

	/* get and parse kernel messages */
	setbuf(stdout, (char *)NULL);

	/* get what the kernel has waiting for us */
	if (modctl(MODEVENTS, (uintptr_t)MODEVENTS_FLUSH) < 0) {
		devfseventd_print(0,
			"modctl - MODEVENTS - %s\n", strerror(errno));
	}

	devfseventd_print(8, "Pausing\n");

	for (;;) {
		(void) pause();
	}
	/* NOTREACHED */
}


/*
 * thread to dispatch event(s) uploaded from the kernel
 */
static void
dispatch_message(void)
{
	for (;;) {
		devfseventd_print(2, "dispatch_message;\n");
		while (sema_wait(&sema_dispatch) != 0) {
			devfseventd_print(1,
			    "dispatch_message: sema_wait failed\n");
			sleep(1);
		}

		devfseventd_print(2, "dispatch_message; got sema\n");
		dispatch(eventbuf);
		(void) sema_post(&sema_eventbuf);
		devfseventd_print(2, "dispatch_message; upcall = %x\n",
			door_upcall_retval);
		if (door_upcall_retval == EAGAIN) {
			/* trigger the kernel thread - upload pending events */
			devfseventd_print(2, "dispatch_message; retrigger\n");
			if (modctl(MODEVENTS, MODEVENTS_FLUSH) < 0) {
				syslog(LOG_ERR, "modctl - MODEVENTS - %s\n",
					strerror(errno));
			}
		}
	}
	/* NOTREACHED */
}

/*
 * main service routine associated with the LOG_EVENT_DOOR_SERVICES door
 */
/* ARGSUSED */
static void
door_services(void *cookie, char *args, size_t alen, door_desc_t *ddp,
    uint_t ndid)
{
	eventd_service_t *req = (eventd_service_t *)args;
	int i, id, rval;
	eventd_msg_t *msg_ptr;
	int procfd;
	char pname[100];
	psinfo_t procinfo;
	int save_id = -1;

	id = req->event_id;

	devfseventd_print(2, "door_services: id=%d, service=%d\n",
		    id, req->service_code);

	(void) mutex_lock(&server_mutex);

	switch (req->service_code) {

	case EVENTD_REGISTER:
		if (!req->pid) {
			req->retcode = EINVAL;
			break;
		}

		(void) sprintf(pname, "/proc/%d/psinfo", req->pid);
		devfseventd_print(2, "%s registering\n", pname);

		if ((procfd = open(pname, O_RDONLY)) < 0 ||
		    read(procfd, (char *)&procinfo, sizeof (procinfo)) < 0) {
			devfseventd_print(0,
				"access %s failed\n", pname);
			req->retcode = EACCES;
			break;

		} else {
			devfseventd_print(2, "register caller %s\n",
					procinfo.pr_fname);
		}

		(void) close(procfd);

		/*
		 * First verify this consumer is not already registered under
		 * the same pid.
		 */
		for (i = 0; i < MAX_CONSUMERS; i++) {
			if (consumers[i].pid && (strcmp(consumers[i].procname,
			    procinfo.pr_fname) == 0)) {

				if ((consumers[i].pid == req->pid) ||
				    (consumers[i].pid == RESERVED_PID)) {
					if (consumers[i].pid == RESERVED_PID) {
					    consumers[i].pid = req->pid;
					    devfseventd_print(2,
						    "check-in %s\n",
						    procinfo.pr_fname);
					} else {
					    devfseventd_print(2,
						    "re-register %s\n",
						    procinfo.pr_fname);
					}

					/*
					 * XXX need to update selection
					 * criteria here.
					 */
					req->event_id = i;
					req->retcode = 0;

					goto done;
				}

				/*
				 * Registered by this name under another pid.
				 * Is that process still active?
				 */
				if ((getsid(consumers[i].pid) == -1) &&
				    (errno == ESRCH)) {
					/*
					 * Process has gone away without
					 * unregistering. Just recycle
					 * this event_id.
					 * XXX may need to update selection
					 * criteria here.
					 */
					devfseventd_print(2,
						"new-pid regstr %s %d\n",
						procinfo.pr_fname,
						consumers[i].pid);
					req->event_id = i;
					req->retcode = 0;
					consumers[i].pid = req->pid;

					goto done;
				}
			}
			if (!consumers[i].pid) {
				save_id = i;
			}
		}

		if (save_id != -1 && !consumers[save_id].pid) {
			/*
			 * This one's available.
			 */
			devfseventd_print(1, "REGISTER	pid = %d\n",
					req->pid);

			req->event_id = save_id;
			consumers[save_id].pid = req->pid;
			req->retcode = 0;

			(void) sema_init(&(consumers[save_id].sema_msg), 0,
				USYNC_THREAD, NULL);

			(void) strcpy(consumers[save_id].procname,
					procinfo.pr_fname);
			/*
			 * first consumer in
			 */
			if (nconsumers++ == 0) {
				unload_queued_events(save_id);
			}

			break;

		} else {
			/*
			 * No consumer ids available.
			 * XXX should attempt some cleanup at this point.
			 */
			req->retcode = EAGAIN;
		}

		break;

	case EVENTD_UNREGISTER:
		if ((consumers[id].pid != req->pid) ||
		    (req->pid == 0) ||
		    (consumers[id].pid == 0) ||
		    (id >= MAX_CONSUMERS)) {
			req->retcode = EINVAL;
			devfseventd_print(0, "EVENTD_UNREGISTER bad arg\n");

			break;
		}

		/*
		 * first cancel any pending messages
		 */
		msg_ptr = msg_qhead;
		while (msg_ptr) {
			msg_ptr->status[id] = EVENT_MSG_IGNORE;
			if ((--msg_ptr->refcnt) == 0) {
				need_qcleanup++;
			}
			msg_ptr = msg_ptr->next;
		}

		consumers[id].pid = 0;
		consumers[id].next_msg = NULL;

		(void) sema_destroy(&(consumers[id].sema_msg));

		req->retcode = 0;
		nconsumers--;

		break;

	case EVENTD_GETMSG:
		if ((consumers[id].pid != req->pid) ||
		    (req->pid == 0) ||
		    (consumers[id].pid == 0) ||
		    (id >= MAX_CONSUMERS)) {
			req->retcode = EINVAL;
			devfseventd_print(0, "EVENTD_GETMSG bad arg:\n\t"
			    "cons=%x req=%x id=%d\n",
			    consumers[id].pid, req->pid, id);
			break;
		}

		(void) mutex_unlock(&server_mutex);

		while ((rval = (sema_wait(&(consumers[id].sema_msg)))) != 0) {
			devfseventd_print(1,
			    "EVENTD_GETMSG: sema_wait failed, %d\n", rval);
			sleep(1);
		}

		/*
		 * We have an event message. Wait for access to the queue.
		 */
		(void) mutex_lock(&server_mutex);
		if ((msg_ptr = consumers[id].next_msg) == NULL) {
			msg_ptr = msg_qhead;
		}

		/*
		 * XXX for now, just return the next message on the queue.
		 */
		while (msg_ptr) {
			if (msg_ptr->status[id] == EVENT_MSG_PENDING) {
				msg_ptr->status[id] = EVENT_MSG_IGNORE;
				(void) strcpy(req->buf, msg_ptr->msg);
				if (--(msg_ptr->refcnt) == 0) {
					need_qcleanup++;
				}
				consumers[id].next_msg = msg_ptr->next;
				break;
			}
			msg_ptr = msg_ptr->next;
		}

		if (!msg_ptr) {
			/*
			 * XXX This should never happen!
			 * Something went wrong. We didn't find a message.
			 * Indicate caller should try again.
			 */
			devfseventd_print(0,
				"EVENTD_GETMSG - no message!\n");
			req->retcode = EAGAIN;
			qstats();
		} else {
			req->retcode = 0;
			if (strlen(req->buf) == 0) {
				devfseventd_print(0,
				    "EVENTD_GETMSG - empty message!\n");
			}
		}

		break;

	case EVENTD_LOGMSG:
		req->retcode = enqueue_msg(req->buf);
		break;

	default:
		break;
	}


done:
	(void) mutex_unlock(&server_mutex);

	door_return(args, sizeof (eventd_service_t), NULL, 0);
}

/*
 * move buffered events to the initial consumer
 */
static void
unload_queued_events(int cid)
{
	eventd_msg_t *msg_ptr;

	msg_ptr = msg_qhead;
	while (msg_ptr) {
		if (msg_ptr->status[DEFAULT_CONSUMER] == EVENT_MSG_PENDING) {
			msg_ptr->status[cid] = EVENT_MSG_PENDING;
			msg_ptr->status[DEFAULT_CONSUMER] = EVENT_MSG_IGNORE;
			(void) sema_post(&(consumers[cid].sema_msg));
		}
		msg_ptr = msg_ptr->next;
	}
}

/*
 * called from the kernel to upload event(s)
 */
/* ARGSUSED */
static void
door_upcall(void *cookie, char *args, size_t alen, door_desc_t *ddp,
    uint_t ndid)
{
	devfseventd_print(2, "door_upcall: %p %p %d %p %d\n",
		cookie, args, alen, ddp, ndid);

	if (sema_trywait(&sema_eventbuf)) {
		/*
		 * Don't let the kernel upcall thread block.
		 */
		door_upcall_retval = EAGAIN;
	} else {
		/*
		 * Copy received message to local buffer.
		 */
		(void) strcpy(eventbuf, ((log_event_upcall_arg_t *)args)->buf);
		(void) sema_post(&sema_dispatch);
		door_upcall_retval = 0;
	}

	door_return((char *)&door_upcall_retval, sizeof (int), NULL, 0);
}

/*
 * called from the door subsystem to create threads to handle door_call()'s
 */
/* ARGSUSED */
static void
my_thread(void *arg)
{
	sigset_t mask;

	(void) sigfillset(&mask);
	thr_sigsetmask(SIG_BLOCK, &mask, NULL);

	door_return(NULL, 0, NULL, 0);
	/* NOTREACHED */
}

/*
 * XXX server threads need to have THR_BOUND, otherwise the
 * event delivery seems to get confused
 */
/* ARGSUSED */
static void
my_create(door_info_t *door_info)
{
	devfseventd_print(1, "my_create\n");

	if (thr_create(NULL, NULL,
	    (void *(*)(void *))my_thread, NULL,
	    THR_BOUND | THR_DETACHED, NULL)) {
		devfseventd_print(0, "thread_create -  %s\n", strerror(errno));
	}
}

/*
 * Create an initial pool of server threads and make sure their stacks
 * get created
 */
static void
create_pool(void)
{
	door_server_create(my_create);
}

/*
 * queue event(s) to the registered consumer
 */
static int
enqueue_msg(char *mptr)
{
	eventd_msg_t *new_msg, *cur = msg_qhead;
	int i;
	int posted = 0;

	if ((new_msg = calloc(1, sizeof (eventd_msg_t))) == NULL) {
		return (ENOMEM);
	}

	if (cur == NULL) {
		/*
		 * empty queue
		 */
		msg_qhead = cur = new_msg;
	} else {
		while (cur->next) {
			cur = cur->next;
		}

		/*
		 * end of queue
		 */
		cur->next = new_msg;
	}

	if ((new_msg->msg = calloc(1, strlen(mptr) + 1)) == NULL) {
		cur->next = NULL;
		free(new_msg);
		return (ENOMEM);
	}

	(void) strcpy(new_msg->msg, mptr);

	/*
	 * XXX for now we'll simply increment all refcnts based on
	 * registered consumers.
	 */
	for (i = 0; i < MAX_CONSUMERS; i++) {
		if (consumers[i].pid) {
			posted++;
			new_msg->refcnt++;
			new_msg->status[i] = EVENT_MSG_PENDING;
			(void) sema_post(&(consumers[i].sema_msg));
		}
	}

	/*
	 * if no consumers have registered, add the event to
	 * the default consumer for buffering.
	 */
	if (!posted) {
		new_msg->refcnt++;
		new_msg->status[DEFAULT_CONSUMER] = EVENT_MSG_PENDING;
	}

	return (0);
}

/*
 * XXX for now we just check refcnt to determine whether to delete.
 *	Assumes the server_mutex is held.
 */
static void
clean_msg_queue(void)
{
	eventd_msg_t *cur = msg_qhead, *tmp;

	if (!nconsumers || !need_qcleanup) {
		/*
		 * No consumers yet or nothing to do....
		 */
		return;
	}

	need_qcleanup = 0;

	if (cur == NULL) {
		/*
		 * Nothing to do
		 */
		return;
	}

	/*
	 * First prune stuff off the front of the queue
	 */
	while (!cur->refcnt) {
		msg_qhead = cur->next;
		free(cur->msg);
		free(cur);
		if ((cur = msg_qhead) == NULL) {
			/*
			 * We've emptied the queue
			 */
			return;
		}
	}

	/*
	 * msg_qhead now points to an unconsumed message.
	 * check on down the list.
	 */
	while (cur->next) {
		if (!cur->next->refcnt) {
			tmp = cur->next;
			cur->next = cur->next->next;
			free(tmp->msg);
			free(tmp);
		}
		if ((cur = cur->next) == NULL) {
			/*
			 * we've reached the end of the queue. We're done.
			 */
			return;
		}
	}
}

/*
 * print queue stats for debugging
 */
static void
qstats(void)
{
	eventd_msg_t *msg_ptr = msg_qhead;
	int i = 0;

	while (msg_ptr) {
		msg_ptr = msg_ptr->next;
		i++;
	}
	devfseventd_print(2,
		"%d messages still on the queue, nconsumers = %d\n",
			i, nconsumers);
	devfseventd_print(2, "need_qcleanup = %d\n", need_qcleanup);
}


/*
 * Dispatch kernel messages.
 */
static void
dispatch(char *dp)
{
	char *sp;
	char *nextmsg;

	devfseventd_print(3, "dispatch: %s (%x)\n", dp, dp);

	(void) mutex_lock(&server_mutex);
	clean_msg_queue();

	if (!msg_qhead) {
		devfseventd_print(2, "msg_queue empty!\n");
	} else {
		qstats();
	}

	/* the kernel may return multiple messages; process each in turn */
	sp = strtok_r(dp, MESSAGE_SEPARATOR, &nextmsg);

	devfseventd_print(3, "dispatch: queueing message\n");

	while (sp != NULL) {
		if (enqueue_msg(sp) != 0) {
			if (enqueue_msg(sp) != 0) {
				devfseventd_print(0, "alloc error\n");
				devfseventd_exit(-1);
			}
		}

		/* set up to process next kernel string */
		sp = strtok_r((char *)NULL, MESSAGE_SEPARATOR, &nextmsg);
		if (sp != (char *)NULL) {
			devfseventd_print(2, "multiple message\n");
		}
	}

	(void) mutex_unlock(&server_mutex);
	devfseventd_print(3, "dispatch: exit\n");
}


/*
 *
 * Use an advisory lock to ensure that only one daemon process is active
 * in the system at any point in time.	If the lock is held by another
 * process, do not block but return the pid owner of the lock to the
 * caller immediately.	The lock is cleared if the holding daemon process
 * exits for any reason even if the lock file remains, so the daemon can
 * be restarted if necessary.  The lock file is daemon_lock_file.
 */
static pid_t
enter_daemon_lock(void)
{
	struct flock	lock;

	devfseventd_print(1, "enter_daemon_lock: lock file = %s\n",
		daemon_lock_file);

	daemon_lock_fd = open(daemon_lock_file, O_CREAT|O_RDWR, 0644);
	if (daemon_lock_fd < 0) {
		devfseventd_print(0, "open(%s) - %s\n",
			daemon_lock_file, strerror(errno));
		devfseventd_exit(1);
	}

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	if (fcntl(daemon_lock_fd, F_SETLK, &lock) == -1) {

		if (errno == EAGAIN || errno == EDEADLK) {

			if (fcntl(daemon_lock_fd, F_GETLK, &lock) == -1) {
				devfseventd_print(0, "lock(%s) - %s",
					daemon_lock_file, strerror(errno));
				exit(2);
			}

			return (lock.l_pid);
		}
	}
	hold_daemon_lock = 1;

	return (getpid());
}

/*
 * Drop the advisory daemon lock, close lock file
 */
static void
exit_daemon_lock(void)
{
	struct flock lock;

	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	if (fcntl(daemon_lock_fd, F_SETLK, &lock) == -1) {
		devfseventd_print(0, "unlock(%s) - %s",
			daemon_lock_file, strerror(errno));
	}

	if (close(daemon_lock_fd) == -1) {
		devfseventd_print(0, "close(%s) failed - %s\n",
			daemon_lock_file, strerror(errno));
		exit(-1);
	}
}


/*
 * print error messages to the terminal or to syslog
 */
static void
devfseventd_print(int level, char *message, ...)
{
	va_list ap;
	static int newline = 1;

	if (level > debug_level) {
		return;
	}

	va_start(ap, message);
	if (level == 0) {
		if (logflag) {
			(void) vsyslog(LOG_ERR, message, ap);
		} else {
			(void) vfprintf(stderr, message, ap);
		}

	} else {
		if (logflag) {
			(void) syslog(LOG_DEBUG, "%s[%ld]: ",
				    prog, getpid());
			(void) vsyslog(LOG_DEBUG, message, ap);
		} else {
			if (newline) {
				(void) fprintf(stdout, "%s[%ld]: ",
					prog, getpid());
				(void) vfprintf(stdout, message, ap);
			} else {
				(void) vfprintf(stdout, message, ap);
			}
		}
	}
	if (message[strlen(message)-1] == '\n') {
		newline = 1;
	} else {
		newline = 0;
	}
	va_end(ap);
}


#ifdef READ_CONFIG_FILE

static char *default_config = "/etc/devfseventd.conf";
#define	EVENTD_CONFIG_DEBUG	"debug_level"

/*
 * Called during devfseventd startup based on entries in the
 * "default_config" file.
 * Assumes no doors are open yet so no locking of data structures is required.
 */
static void
devfseventd_register(char *consumer)
{
	int i;

	for (i = 0; i < MAX_CONSUMERS; i++) {
		if (consumers[i].pid) {
			if (strcmp(consumer, consumers[i].procname) == 0) {
				/*
				 * for now only one reservation per customer.
				 */
				return;
			}
			continue;
		}
		nconsumers++;
		(void) strcpy(consumers[i].procname, consumer);
		consumers[i].pid = RESERVED_PID;
		return;
	}
	devfseventd_print(0, "could not register %s\n", consumer);
}

/*
 * read the config file
 */
static void
config_read(void)
{
	FILE		*cfp;
	char		buf[LOGEVENT_BUFSIZE];
	char		*consumer, *nl, *db;
	int		new_debug_level;

	devfseventd_print(4, "reading config file %s\n", default_config);

	if ((cfp = fopen(default_config, "r")) == NULL) {
		devfseventd_print(0,
		    "can't open config file %s\n", default_config);
		return;
	}

	while (fgets(buf, LOGEVENT_BUFSIZE, cfp) != NULL) {
		/* skip comment lines (starting with #) and blanks */
		if (buf[0] == '#' || buf[0] == '\n') {
			continue;
		}
		/*
		 * currently should only be one process name per
		 * line in default_config -- extract the basename.
		 */
		if ((consumer = strrchr(buf, '/')) == NULL) {
			consumer = buf;
		} else {
			consumer++;
		}
		/*
		 * eliminate newline character
		 */
		if (nl = strrchr(consumer, '\n')) {
			*nl = '\0';
		}
		/*
		 * Check for special directives
		 */
		if (strncmp(EVENTD_CONFIG_DEBUG, consumer,
			strlen(EVENTD_CONFIG_DEBUG)) == 0) {
			if ((!(db = strrchr(consumer, ' '))) &&
			    (!(db = strrchr(consumer, '\t')))) {
				devfseventd_print(0,
					"%s malformed debug directive\n",
					default_config);
				continue;
			}
			db++;
			if (((new_debug_level = atoi(db)) >= 0) &&
				(new_debug_level <= DEBUG_LEVEL_FORK)) {
				debug_level = new_debug_level;
			} else {
				devfseventd_print(0,
					"%s malformed debug directive\n",
					default_config);
			}
			continue;
		}
		devfseventd_print(2, "registering %s\n", consumer);
		devfseventd_register(consumer);
	}
	(void) fclose(cfp);
}
#endif /* READ_CONFIG_FILE */
