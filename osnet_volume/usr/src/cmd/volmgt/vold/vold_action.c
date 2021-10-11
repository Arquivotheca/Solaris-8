/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vold_action.c	1.27	98/01/05 SMI"

#include	<stdlib.h>
#include	<string.h>
#include	<pwd.h>
#include	<sys/types.h>
#include	<unistd.h>
#include	<sys/stat.h>
#include	<sys/mnttab.h>
#include	<sys/resource.h>
#include	<regex.h>
#include	"vold.h"


/*
 * Volume Daemon action support.
 */

typedef struct action {
	struct action	*a_next;	/* linked list pointer */
	char		*a_re;		/* original regex */
	regex_t		a_recmp;	/* complied regex */
	actprog_t	*a_prog;	/* program and args to execute */
} action_t;

static action_t	*insert = NULL;
static action_t	*eject = NULL;
static action_t	*notify = NULL;
static action_t	*aerror = NULL;


struct q 	reapq;

char *actnames[] = {
	{ "" },
	{ "insert" },
	{ "eject" },
	{ "notify" },
	{ "error" },
};



/* length of buffer used to create environment variables */
#define	VOLD_ENVBUFLEN	512

/* length of buffer for printing numbers */
#define	VOLD_NUMBUFLEN	512



/*
 * Execute the specified actions for this volume.
 */
int
action(u_int act, vol_t *v)
{
	static void	action_reaper(u_int, vol_t *, pid_t, char *);
	static void	action_exec(u_int, actprog_t *, vol_t *);
	struct vnwrap	*vw;
	action_t	*a, **list;
	char		*path;
	int		nacts;
	int		re_ret;



#ifdef	DEBUG
	debug(10, "action: entering for vol %s (act %s)\n",
	    v->v_obj.o_name, actnames[act]);
#endif

	switch (act) {
	case ACT_NOTIFY:
		list = &notify;
		break;
	case ACT_INSERT:
		list = &insert;
		break;
	case ACT_EJECT:
		list = &eject;
		break;
	case ACT_ERROR:
		list = &aerror;
		break;
	default:
		warning(gettext("action: unknown action %d\n"), act);
		return (0);
	}

	vw = node_findnode(v->v_obj.o_id, FN_ANY, FN_ANY, FN_ANY);
	if (vw == NULL) {
		debug(1, "action: couldn't find any vn's for %s\n",
			v->v_obj.o_name);

		if (act == ACT_EJECT) {
			action_reaper(act, v, 0, "internal error");
		}
		return (0);
	}

	nacts = 0;
	debug(1, "action: %s on volume %s\n", actnames[act],
	    v->v_obj.o_name);
	for (; vw; vw = vw->vw_next) {
		path = path_make(vw->vw_node);
		for (a = *list; a; a = a->a_next) {
			debug(6, "action: regexec(%s, %s)\n", a->a_re, path);
			if ((re_ret = regexec(&a->a_recmp, path, 0, NULL,
			    0)) == REG_OK) {
				a->a_prog->ap_matched = path;
				action_exec(act, a->a_prog, v);
				nacts++;
			} else if (re_ret != REG_NOMATCH) {
				debug(1,
			"action: can't compre RE \"%s\" to \"%s\" (ret %d)\n",
				    a->a_re, path, re_ret);
			}
		}
		free(path);
	}
	node_findnode_free(vw);
	return (nacts);
}


/*
 * Add a new action
 */
bool_t
action_new(u_int act, char *re, struct actprog *ap)
{
	action_t	*a, **list;
	int		regcomp_ret;


	switch (act) {
	case ACT_NOTIFY:
		list = &notify;
		break;
	case ACT_INSERT:
		list = &insert;
		break;
	case ACT_EJECT:
		list = &eject;
		break;
	case ACT_ERROR:
		list = &aerror;
		break;
	default:
		warning(gettext("action_new: unknown action %d\n"), act);
		return (FALSE);
	}

	a = (action_t *)calloc(1, sizeof (action_t));

	/* compile the regular expression */
	if ((regcomp_ret = regcomp(&a->a_recmp, re, REG_NOSUB)) != REG_OK) {
		debug(1, "action_new: can't compile RE \"%s\" (ret = %d)\n",
		    re, regcomp_ret);
		free(a);
		return (FALSE);
	}

	/* stick it on our list */
	if (*list == NULL) {
		*list = a;
	} else {
		a->a_next = *list;
		*list = a;
	}
	a->a_re = strdup(re);
	a->a_prog = ap;
	return (TRUE);
}


/*
 * Remove all actions.
 */
void
action_flush()
{
	action_t	*a;
	action_t	*a_next = NULL;
	actprog_t	*ap;
	int		i;


	/* flush 'insert' events */
	for (a = insert; a != NULL; a = a_next) {
		a_next = a->a_next;
		regfree(&a->a_recmp);
		free(a->a_re);
		ap = a->a_prog;
		free(a);
		free(ap->ap_prog);
		if (ap->ap_args != NULL) {
			for (i = 0; ap->ap_args[i]; i++) {
				free(ap->ap_args[i]);
			}
			free(ap->ap_args);
		}
	}
	insert = NULL;

	/* flush 'notify' events */
	for (a = notify; a != NULL; a = a_next) {
		a_next = a->a_next;
		regfree(&a->a_recmp);
		free(a->a_re);
		ap = a->a_prog;
		free(a);
		free(ap->ap_prog);
		if (ap->ap_args != NULL) {
			for (i = 0; ap->ap_args[i]; i++) {
				free(ap->ap_args[i]);
			}
			free(ap->ap_args);
		}
	}
	notify = NULL;

	/* flush 'eject' events */
	for (a = eject; a != NULL; a = a_next) {
		a_next = a->a_next;
		regfree(&a->a_recmp);
		free(a->a_re);
		ap = a->a_prog;
		free(a);
		free(ap->ap_prog);
		if (ap->ap_args != NULL) {
			for (i = 0; ap->ap_args[i]; i++) {
				free(ap->ap_args[i]);
			}
			free(ap->ap_args);
		}
	}
	eject = NULL;

	/* flush 'action error' events */
	for (a = aerror; a != NULL; a = a_next) {
		a_next = a->a_next;
		regfree(&a->a_recmp);
		free(a->a_re);
		ap = a->a_prog;
		free(a);
		free(ap->ap_prog);
		if (ap->ap_args != NULL) {
			for (i = 0; ap->ap_args[i]; i++) {
				free(ap->ap_args[i]);
			}
			free(ap->ap_args);
		}
	}
	aerror = NULL;
}


static void
action_reaper(u_int act, vol_t *v, pid_t pid, char *hint)
{
	struct reap *r;

	r = (struct reap *)calloc(1, sizeof (struct reap));

	r->r_v = v;
	r->r_act = act;
	r->r_pid = pid;
	r->r_hint = strdup(hint);

	INSQUE(reapq, r);
}


static void
action_exec(u_int act, actprog_t *ap, vol_t *v)
{
	static void	action_buildenv(u_int, actprog_t *, vol_t *);
	pid_t		pid;
	struct rlimit	rlim;
	extern int 	errno;
	extern char	*vold_devdir;


	debug(1, "action_exec: \"%s\" on \"%s\", prog=\"%s\"\n",
	    actnames[act], v->v_obj.o_name, ap->ap_prog);

	if (act == ACT_EJECT) {
		/* about to launch an ejection action */
		v->v_eject++;
	}

	if ((pid = fork1()) == 0) {
		/* child */

		/*
		 * The getrlimit/setrlimit stuff is here to support
		 * the execution of binaries running in BCP mode.
		 * Since the daemon increases the number of available
		 * file descriptors, they need to be reset here or it
		 * screws up old binaries that couldn't support as many
		 * fd's.
		 */
		getrlimit(RLIMIT_NOFILE, &rlim);
		rlim.rlim_cur = original_nofile;
		if (setrlimit(RLIMIT_NOFILE, &rlim) < 0) {
			perror("vold; setrlimit");
		}

		(void) chdir(vold_devdir);	/* so core dumps'll go here */
		action_buildenv(act, ap, v);
		(void) setgid(ap->ap_gid);
		(void) setuid(ap->ap_uid);
#ifdef DEBUG_EXECS
		(void) fprintf(stderr,
		    "execing %s, pid = %d, uid = %d, gid = %d\n",
		    ap->ap_prog, getpid(), ap->ap_uid, ap->ap_gid);
#endif
		(void) execv(ap->ap_prog, ap->ap_args);
		(void) fprintf(stderr, "exec failed on %s, errno %d\n",
		    ap->ap_prog, errno);
		exit(0);
	} else if (pid == -1) {
		warning(gettext("action_exec: couldn't exec %s; %m\n"),
		    ap->ap_prog);
	} else {
		action_reaper(act, v, pid, ap->ap_prog);
	}
}


static void
action_buildenv(u_int act, actprog_t *ap, vol_t *v)
{
	extern char	*dev_getpath(dev_t);
	static void	vol_putenv(char *);
	char		namebuf[VOLD_ENVBUFLEN+1];
	char		tmpbuf[VOLD_NUMBUFLEN+1];
	struct passwd	*pw;
	char		*user;
	char		*symname;


	/*
	 * Since we only do this in the child,
	 * we don't worry about losing the memory we're
	 * about to allocate.
	 */

	(void) sprintf(namebuf, "VOLUME_ACTION=%s", actnames[act]);
	vol_putenv(strdup(namebuf));

	(void) sprintf(namebuf, "VOLUME_PATH=%s", ap->ap_matched);
	vol_putenv(strdup(namebuf));

	(void) sprintf(namebuf, "VOLUME_NAME=%s", obj_basepath(&v->v_obj));
	vol_putenv(strdup(namebuf));

	if ((act == ACT_EJECT) || (act == ACT_INSERT)) {
		(void) sprintf(namebuf, "VOLUME_DEVICE=%s",
		    dev_getpath(v->v_basedev));
		vol_putenv(strdup(namebuf));
		if ((symname = dev_symname(v->v_basedev)) != NULL) {
			(void) sprintf(namebuf, "VOLUME_SYMDEV=%s", symname);
			vol_putenv(strdup(namebuf));
		} else {
			vol_putenv("VOLUME_SYMDEV=");
		}
	} else {
		vol_putenv("VOLUME_DEVICE=");
		vol_putenv("VOLUME_SYMDEV=");
	}

	if (act == ACT_EJECT || act == ACT_NOTIFY) {
		if ((pw = getpwuid(v->v_clue.c_uid)) != NULL) {
			user = pw->pw_name;
		} else {
			(void) sprintf(tmpbuf, "%ld", v->v_clue.c_uid);
			user = tmpbuf;
		}

		(void) sprintf(namebuf, "VOLUME_USER=%s", user);
		vol_putenv(strdup(namebuf));

#ifdef	VOLMGT_DEV_TO_TTY_WORKED
		/*
		 * Converting a dev_t into a path name is a very
		 * expensive operation, so we only do it if the
		 * user has told us to by sticking the maptty
		 * flag on his action.
		 */
		if (ap->ap_maptty) {
			(void) sprintf(namebuf, "VOLUME_USERTTY=%s",
			    devtotty(v->v_clue.c_tty));
			vol_putenv(strdup(namebuf));
		} else {
			vol_putenv("VOLUME_USERTTY=");
		}
#else	/* VOLMGT_DEV_TO_TTY_WORKED */
		/* devtotty() has some bugs right now */
		(void) sprintf(namebuf, "VOLUME_USERTTY=0x%lx",
		    v->v_clue.c_tty);
		vol_putenv(strdup(namebuf));
#endif	/* VOLMGT_DEV_TO_TTY_WORKED */
	} else {
		vol_putenv("VOLUME_USER=");
		vol_putenv("VOLUME_USERTTY=");
	}
	(void) sprintf(namebuf, "VOLUME_MEDIATYPE=%s", v->v_mtype);
	vol_putenv(strdup(namebuf));
	if (v->v_ej_force == TRUE) {
		vol_putenv("VOLUME_FORCEDEJECT=true");
	} else {
		vol_putenv("VOLUME_FORCEDEJECT=false");
	}
}


static void
vol_putenv(char *env_str)
{
#ifdef	DEBUG
	debug(11, "vol_putenv: \"%s\"\n", env_str);
#endif
	(void) putenv(env_str);
}
