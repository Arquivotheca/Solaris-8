/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * cfname.c -- routines for multiconf stuff
 */

#ident	"@(#)cfname.c	1.57	99/10/07 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <dostypes.h>
#include <dos.h>
#include <io.h>
#include "types.h"

#include "menu.h"
#include "bop.h"
#include "boot.h"
#include "cfname.h"
#include "debug.h"
#include "dir.h"
#include "devdb.h"
#include "err.h"
#include "escd.h"
#include "gettext.h"
#include "kbd.h"
#include "open.h"
#include "main.h"
#include "menu.h"
#include "prop.h"
#include "tty.h"
#include "biosprim.h"
#include "spmalloc.h"

#define	MAXFIGS		10	/* maximum allowed saved configurations */
#define	MAXCONFIG	20	/* maximum length of configuration name */
#define	AUTO_TOB_LEN	18	/* size of input buffer for auto timeout buf */

int No_cfname = 1; /* set if new configuration */
char *Machenv_name = 0;

static char Bootenv_name[] = "solaris/bootenv.rc";

static char machdir[] = "solaris/machines";
static char conffile[] = "conf";
static char escdext[] = ".rf";
static char nameext[] = ".nam";
static struct menu_list figs[MAXFIGS + 1];	/* allow room for "New" */
static int this_conf = -1; /* the saved configuration number */

/*
 * Local function prototypes
 */
int dup_cfname(const char *name);

/*
 * options for menu which prompts user for an existing configuration name
 */
static struct menu_options Fig_options[] = {
	{ FKEY(2), MA_RETURN, "Continue" },
};

#define	NFIG_OPTIONS (sizeof (Fig_options) / sizeof (*Fig_options))

/*
 * init_cfname -- figure out the names of the config files
 *
 * if the user tells us to use an existing config, we will know the kbd
 * type.  if it is a new config, we must ask for the kbd type before we
 * can prompt the user for a string.  so this module is also responsible
 * for calling the *_kbd() routines.
 */
void
init_cfname()
{
	char linebuf[MAXLINE];		/* input line buffer */
	struct menu_list *choice;
	int nfigs;	/* number of saved configurations */
	FILE *fp;
	char *np;
	int i;
	int close(int fd);
	int free_conf;
	int fd;

	if (Autoboot) {
		char *buf;

		if (buf = read_prop(Auto_boot_cfg_num, "options")) {
			this_conf = atoi(buf);
			if (this_conf != -1) {
				goto got_config;
			}
		}
		/*
		 * use defaults left over from previous boot
		 */
		Escd_name = "escd.rf";
		Machenv_name = Bootenv_name;
		return;
	}

	/* loop until user picks a config */
	this_conf = -1;
	while (this_conf == -1) {
		/* find all saved configs */
		nfigs = 0;
		free_conf = -1;
		for (i = 0; i < MAXFIGS; i++) {
			(void) sprintf(linebuf, "%s/%s%03d%s",
			    machdir, conffile, i, nameext);
			if ((fp = fopen(linebuf, "r")) != NULL) {
				np = fgets(linebuf, MAXLINE, fp);
				fclose(fp);
				if ((np != NULL) && (*linebuf != '\n')) {
					/* chop the newline */
					linebuf[strlen(linebuf) - 1] = '\0';
					/* free strings from last time */
					if (figs[nfigs].string)
						free(figs[nfigs].string);
					figs[nfigs].string = strdup(linebuf);
					figs[nfigs].flags = 0;
					figs[nfigs++].datum = (void *)i;
					continue;
				}
			}

			/* file didn't exist or was empty or had blank line */
			if (free_conf == -1)
				free_conf = i;	/* remember lowest free */
		}

		if (nfigs == 0) {
			/* no saved configurations -- use defaults */
			Escd_name = "escd.rf";

			/* truncate the .rf file - if running off floppy */
			if (Floppy) {
				fd = _open(Escd_name, _O_TRUNC|_O_WRONLY);
				if ((fd == -1) && (errno != ENOENT)) {
					do {
						write_err(Escd_name);
					} while ((fd = _open(Escd_name,
					    _O_TRUNC|_O_WRONLY)) == -1);
				}
				close(fd);
			}

			/* initialise machenv */
			Machenv_name = Bootenv_name;

			check_kbd();
			return;
		}
		figs[nfigs].string = "New Configuration";
		figs[nfigs].flags = MF_SELECTED;
		figs[nfigs++].datum = (void *)(MAXFIGS + 1);

		/* display a list of saved configurations */
again:
		switch (select_menu("MENU_HELP_SEL_CONFIG",
		    Fig_options, NFIG_OPTIONS, figs, nfigs,
		    MS_ZERO_ONE, "MENU_SEL_CONFIG")) {

		case FKEY(2):
			/* user selected a configuration */
			if ((choice = get_selection_menu(figs,
			    nfigs)) == NULL) {
				beep_tty();
				goto again;
			}
			this_conf = (int)choice->datum;

			if (this_conf == (MAXFIGS + 1)) {

				/* create a new configuration */
				Escd_name = "escd.rf";

				/* truncate the .rf file */
				fd = _open(Escd_name, _O_TRUNC|_O_WRONLY);
				if ((fd == -1) && (errno != ENOENT)) {
					do {
						write_err(Escd_name);
					} while ((fd = _open(Escd_name,
					    _O_TRUNC|_O_WRONLY)) == -1);
				}
				close(fd);

				/*
				 * machenv stuff is just kept in the bootenv
				 * file
				 */
				Machenv_name = Bootenv_name;

				check_kbd();

				status_menu(Please_wait, "MENU_CHECKING_CONF");

				return;
			} else
				No_cfname = 0;
		}
	}

	/* got a configuration */
	status_menu(Please_wait, "MENU_CHECKING_CONF");
got_config:
	(void) sprintf(linebuf, "%s/%s%03d%s", machdir, conffile, this_conf,
		escdext);
	if ((Escd_name = strdup(linebuf)) == NULL)
		MemFailure();
	(void) sprintf(linebuf, "%s/%s%03d.rc", machdir, conffile, this_conf);
	if ((Machenv_name = strdup(linebuf)) == NULL)
		MemFailure();
	file_bop(Machenv_name);
}

/*
 * input options for menu which prompts user for a new name
 */
static struct menu_options New_fig_options[] = {
	{ '\n', MA_RETURN, NULL },
	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Cancel" },
};

#define	NNEW_FIG_OPTIONS (sizeof (New_fig_options) / sizeof (*New_fig_options))

/*
 * save current configuration (currently in Escd_name)
 */
void
save_cfname(void)
{
	char linebuf[MAXLINE];		/* input line buffer */
	char newconf[MAXCONFIG + 1];	/* config name from user */
	FILE *fp;
	char *np;
	char buf[512];
	int nfd, ofd;
	int i, n;
	int free_conf = -1;

	/*
	 * Calculate the next free configuration number
	 */
	for (i = 0; i < MAXFIGS; i++) {
		(void) sprintf(linebuf, "%s/%s%03d%s", machdir, conffile, i,
			nameext);
		if ((fp = fopen(linebuf, "r")) != NULL) {
			np = fgets(linebuf, MAXLINE, fp);
			fclose(fp);
			if ((np != NULL) && (*linebuf != '\n')) {
				continue;
			}
		}

		/* file didn't exist or was empty or had blank line */
		if (free_conf == -1) {
			free_conf = i;	/* remember lowest free */
		}
	}
	if (free_conf < 0) {
		enter_menu(0, "SAVE CONFIG ERR2", MAXFIGS);
		return;
	}
	write_escd();

again:
	*newconf = '\0';
	if (input_menu("MENU_HELP_NEW_CONFIG", New_fig_options,
	    NNEW_FIG_OPTIONS, newconf, MAXCONFIG + 1, MI_READABLE,
	    "MENU_NEW_CONFIG", MAXCONFIG) == FKEY(3)) {
		/* ie cancel */
		return;
	}
	if ((*newconf == '\0') || (*newconf == '\n')) {
		beep_tty();
		goto again;
	}

	/* user entered a name */
	status_menu(Please_wait, "MENU_UPDATING_CONF");
	/*
	 * check for duplicate names
	 * goto again;
	 */
	if (dup_cfname(newconf)) {
		goto again;
	}

	/*
	 * create the name file
	 */
	(void) sprintf(linebuf, "%s/%s%03d%s", machdir, conffile, free_conf,
		nameext);
retry:
	while ((fp = fopen(linebuf, "wb")) == NULL) {
		write_err(linebuf);
	}
	if ((fprintf(fp, "%s\n", newconf) < 0) || fclose(fp)) {
		fclose(fp);
		write_err(linebuf);
		goto retry;
	}

	/*
	 * Copy the default .rf files to the new location
	 */
	(void) sprintf(linebuf, "%s/%s%03d%s", machdir, conffile, free_conf,
		escdext);

	if (copy_file_cfname(linebuf, Escd_name)) {
		goto bad;
	}

	if ((Escd_name = strdup(linebuf)) == NULL) {
		MemFailure();
	}

	/*
	 * Copy the default .rc files to the new location
	 */
	(void) sprintf(linebuf, "%s/%s%03d.rc", machdir, conffile, free_conf);
	ofd = _open(Bootenv_name, _O_RDONLY|_O_BINARY);
	if (ofd < 0) {
		fatal("Couldn't open %s", Bootenv_name);
	}
	nfd = _open(linebuf, _O_WRONLY|_O_CREAT|_O_TRUNC|_O_BINARY, 0666);
	if (nfd < 0) {
		enter_menu(0, "FILE_CREATE_ERR", linebuf);
		return;
	}

	while ((n = _read(ofd, buf, 512)) > 0) {
		if (_write(nfd, buf, n) < 0) {
			goto bad;
		}
	}
	_close(nfd);
	_close(ofd);
	if ((Machenv_name = strdup(linebuf)) == NULL) {
		MemFailure();
	}
	this_conf = free_conf;
	return;
bad:
	fatal("write error: media full?\n");
}


/*
 * options for delete configuration menu
 */
static struct menu_options Fig_del_opts[] = {
	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Delete" },
};

#define	NFIG_DEL_OPTS (sizeof (Fig_del_opts)/sizeof (*Fig_del_opts))

/*
 * delete a configuration
 */
void
delete_cfname(void)
{
	char linebuf[MAXLINE];		/* input line buffer */
	struct menu_list *choice;
	int nfigs;	/* number of saved configurations */
	FILE *fp;
	char *np;
	int this_conf;	/* the saved configuration number we're going to use */
	int i;
	int free_conf;
	int deleted_one = 0;
	int fd;

	/*
	 * keep displaying menu of remaining configurations
	 * until user requests a return or deletes the last one.
	 */
	this_conf = -1;
	while (this_conf == -1) {
		/* find all saved configs */
		nfigs = 0;
		free_conf = -1;
		for (i = 0; i < MAXFIGS; i++) {
			(void) sprintf(linebuf, "%s/%s%03d%s",
			    machdir, conffile, i, nameext);
			if ((fp = fopen(linebuf, "r")) != NULL) {
				np = fgets(linebuf, MAXLINE, fp);
				fclose(fp);
				if ((np != NULL) && (*linebuf != '\n')) {
					/* chop the newline */
					linebuf[strlen(linebuf) - 1] = '\0';
					/* free strings from last time */
					if (figs[nfigs].string)
						free(figs[nfigs].string);
					figs[nfigs].string = strdup(linebuf);
					figs[nfigs].flags = 0;
					figs[nfigs++].datum = (void *)i;
					continue;
				}
			}

			/* file didn't exist or was empty or had blank line */
			if (free_conf == -1)
				free_conf = i;	/* remember lowest free */
		}

		if (nfigs == 0) {
			/*
			 * special case if the user deletes the last
			 * configuration then just return
			 */
			if (deleted_one) {
				return;
			}
			/* no saved configurations -- tell user... */
			enter_menu(0, "MENU_NO_CONFIG", 0);
			return;
		} else {
			/* display a list of saved configurations */
again:
			switch (select_menu("MENU_HELP_DEL_CONFIG",
			    Fig_del_opts, NFIG_DEL_OPTS, figs, nfigs,
			    MS_ZERO_ONE, "MENU_DEL_CONFIG")) {

			case FKEY(2):
				return;
			case FKEY(3):
				/* delete a configuration */
				if ((choice = get_selection_menu(figs,
				    nfigs)) == NULL) {
					beep_tty();
					goto again;
				}
				status_menu(Please_wait, "MENU_UPDATING_CONF");
				/* truncate the .nam file */
				(void) sprintf(linebuf,
				    "%s/%s%03d%s", machdir, conffile,
				    (int)choice->datum, nameext);
				fd = _open(linebuf, _O_TRUNC|_O_WRONLY);
				if ((fd == -1) && (errno != ENOENT)) {
					do {
						write_err(linebuf);
					} while ((fd = _open(linebuf,
					    _O_TRUNC|_O_WRONLY)) == -1);
				}
				_close(fd);

				/* truncate the .rf file */
				(void) sprintf(linebuf,
				    "%s/%s%03d%s", machdir, conffile,
				    (int)choice->datum, escdext);
				_close(_open(linebuf, _O_TRUNC|_O_WRONLY));
				deleted_one = 1;
				/*
				 * Check if we are deleting the config
				 * we are currently using
				 */
				if (strcmp(linebuf, Escd_name) == 0) {
					Escd_name = "escd.rf";
					Machenv_name = Bootenv_name;
					file_bop(Machenv_name);
				}
				return;
			}
		}
	}
}

/*
 * Check for name in the config files
 */
int
dup_cfname(const char *name)
{
	char linebuf[MAXLINE];		/* input line buffer */
	int nfigs = 0;
	FILE *fp;
	char *np;
	int i;
	int ret = 0;

	for (i = 0; i < MAXFIGS; i++) {
		(void) sprintf(linebuf, "%s/%s%03d%s", machdir, conffile, i,
			nameext);
		if ((fp = fopen(linebuf, "r")) != NULL) {
			np = fgets(linebuf, MAXLINE, fp);
			fclose(fp);
			if ((np != NULL) && (*linebuf != '\n')) {
				/* chop the newline */
				linebuf[strlen(linebuf) - 1] = '\0';
				/* free strings from last time */
				if (figs[nfigs].string)
					free(figs[nfigs].string);
				figs[nfigs].string = strdup(linebuf);
				figs[nfigs].flags = MF_UNSELABLE;
				figs[nfigs++].datum = (void *)i;
				/*
				 * check for identical name
				 */
				if (strcmp(name, linebuf) == 0) {
					ret = 1;
				}
				continue;
			}
		}
	}
	/*
	 * display error and list of names
	 */
	if (ret) {
		list_menu(0, figs, nfigs, "MENU_NAMES_CONFIG", name);
	}
	return (ret);
}

void
set_default_boot_dev(bef_dev *bdp)
{
	char bootpath[120];

	get_path_from_bdp(bdp, bootpath, 0);
	store_prop(Machenv_name, "bootpath", bootpath, FALSE);
}

void
free_boot_list(struct menu_list *boot_list, int nboot_list)
{
	int i;

	for (i = 0; i < nboot_list; i++) {
		if (boot_list && boot_list[i].string)
			spcl_free(boot_list[i].string);
	}
	if (boot_list)
		spcl_free(boot_list);
}

static struct menu_options aboot_opts[] = {
	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Cancel" },
};

#define	NABOOT_OPTS (sizeof (aboot_opts) / sizeof (*aboot_opts))

static struct menu_list onoff_list[] = {
	{ "ON", (void *)1, 0 },
	{ "OFF", (void *)0, 0 },
};
#define	NONOFF_LIST (sizeof (onoff_list) / sizeof (struct menu_list))

void *save_prop(void *, int);
void *bootdev_prop(void *, int work);
void *timeout_prop(void *, int work);
void *ab_onoff_prop(void *, int work);

struct _autoboot_props_ {
	int	flag;		/* ... indicates if this prop was modified */
	void	*(*func)(void *, int);
	void	*cookie;	/* ... "func"'s private data */
} ab_prop_list[] = {
	{ 0, save_prop, (void *)0, },
	{ 0, bootdev_prop, (void *)0, },
	{ 0, timeout_prop, (void *)0, },
	{ 0, ab_onoff_prop, (void *)0, },
	{ 0, 0, 0, },
};
typedef struct _autoboot_props_ ab_prop_t, *ab_prop_p;

#define	ABFLAG_MOD	0x0001	/* ... property has been modified */

#define	ABWORK_SHOW	0	/* ... print the current value of */
#define	ABWORK_MOD	1	/* ... modify the current value of */
#define	ABWORK_SAVE	2	/* ... save the current value of */
#define	ABWORK_FREE	3	/* ... free any space malloc'd */
#define	ABWORK_INIT	4	/* ... do any one time initialization */

void *
save_prop(void *prime, int work)
{
	ab_prop_p p = (ab_prop_p)prime;
	ab_prop_p x;

	switch (work) {
	case ABWORK_INIT:
		for (x = ab_prop_list; x->func; x++) {
			/*
			 * make sure to skip our own pointer otherwise we'll
			 * loop forever.
			 */
			if (x == p)
				continue;
			(*x->func)(x, ABWORK_INIT);
		}
		return ((void *)0);

	case ABWORK_SHOW:
	case ABWORK_SAVE:
		return ((void *)0);

	case ABWORK_FREE:
		for (x = ab_prop_list; x->func; x++) {
			/*
			 * make sure to skip our own pointer otherwise we'll
			 * loop forever.
			 */
			if (x == p)
				continue;
			(*x->func)(x, ABWORK_FREE);
		}

		return ((void *)0);

	case ABWORK_MOD:
		for (x = ab_prop_list; x->func; x++) {
			if (x == p)
				continue;
			if (x->flag & ABFLAG_MOD) {
				(void) (*x->func)(x, ABWORK_SAVE);
				x->flag &= ~ABFLAG_MOD;
			}
			(*x->func)(x, ABWORK_FREE);
		}
		return ((void *)1);
	}
	return ((void *)0);
}

void *
ab_onoff_prop(void *prime, int work)
{
	ab_prop_p p = (ab_prop_p)prime;
	struct menu_list *choice;
	void *saved_val = p->cookie;
	char *buf;
	char cfg_num[4];

	switch (work) {
	case ABWORK_INIT:
		if (((buf = read_prop(Auto_boot, "options")) != 0) &&
		    (strncmp(buf, "true", 4) == 0))
			p->cookie = (void *)1;
		else
			p->cookie = (void *)0;
		break;

	case ABWORK_SHOW:
		return ((void *)(p->cookie ? "ON" : "OFF"));

	case ABWORK_MOD:
		for (; ; ) {
			switch (select_menu("MENU_HELP_AUTO_BOOT", aboot_opts,
			    NABOOT_OPTS, onoff_list, NONOFF_LIST, MS_ZERO_ONE,
			    "MENU_ONOFF_BOOT", p->cookie ? "ON" : "OFF")) {
			case FKEY(2):
				if ((choice = get_selection_menu(onoff_list,
				    NONOFF_LIST)) == NULL) {
					beep_tty();
				} else {
					choice->flags &= ~MF_SELECTED;
					p->cookie = choice->datum;
					p->flag |= ABFLAG_MOD;
					return ((void *)0);
				}
				break;

			case FKEY(3):
				p->cookie = saved_val;
				return ((void *)0);
			}
		}
		/*NOTREACHED*/
		break;

	case ABWORK_SAVE:
		(void) sprintf(cfg_num, "%d", this_conf);
		store_prop(Machenv_name, Auto_boot,
		    (int)p->cookie ? "true" : "false", FALSE);
		store_prop(Machenv_name, Auto_boot_cfg_num, cfg_num, FALSE);
		if (strcmp(Bootenv_name, Machenv_name) != 0) {
			store_prop(Bootenv_name, Auto_boot,
			    (int)p->cookie ? "true" : "false", FALSE);
			store_prop(Bootenv_name, Auto_boot_cfg_num, cfg_num,
			    FALSE);
		}

		break;

	case ABWORK_FREE:
		break;
	}
	return ((void *)0);
}

void *
timeout_prop(void *prime, int work)
{
	ab_prop_p p = (ab_prop_p)prime;
	char *buf, tout_val[AUTO_TOB_LEN];

	memset(tout_val, 0, AUTO_TOB_LEN);

	switch (work) {
	case ABWORK_INIT:
		if ((buf = read_prop(Auto_boot_timeout, "options")) != 0)
			p->cookie = (void *)strtol(buf, 0, 0);
		else
			p->cookie = (void *)0;
		break;

	case ABWORK_SHOW:
		return (p->cookie);

	case ABWORK_MOD:

		switch (input_menu("MENU_HELP_AUTO_BOOT", aboot_opts,
		    NABOOT_OPTS, tout_val, AUTO_TOB_LEN - 1, MI_NUMERIC,
		    "MENU_AUTO_BOOT_TIMEOUT", (int)p->cookie)) {

		case FKEY(2):
			if (tout_val[0] == 0)
				(void) sprintf(tout_val, "%d", (int)p->cookie);
			p->cookie = (void *)strtol(tout_val, 0, 0);
			p->flag |= ABFLAG_MOD;
			break;

		case FKEY(3):
			return ((void *)0);
		}
		break;

	case ABWORK_SAVE:
		(void) sprintf(tout_val, "%d", (int)p->cookie);
		store_prop(Machenv_name, Auto_boot_timeout, tout_val, FALSE);
		if (strcmp(Bootenv_name, Machenv_name) != 0) {
			store_prop(Bootenv_name, Auto_boot_timeout, tout_val,
			    FALSE);
		}
		break;

	case ABWORK_FREE:
		break;
	}
	return ((void *)0);
}

/*
 * bd_storage is a private structure for bootdev_prop().
 */
typedef struct {
	bef_dev	*bdp;
	char	*bootpath;
} bd_storage_t, *bd_storage_p;

void *
bootdev_prop(void *prime, int work)
{
	ab_prop_p p = (ab_prop_p)prime;
	struct menu_list *boot_list, *single;
	int nboot_list;
	bd_storage_p bsp;

	if (p->cookie == NULL) {
		if ((bsp = (bd_storage_p)malloc(sizeof (bd_storage_t))) ==
		    NULL)
			MemFailure();
		memset(bsp, 0, sizeof (bd_storage_t));
		if ((bsp->bootpath = malloc(120)) == NULL)
			MemFailure();

		p->cookie = (void *)bsp;
	} else {
		bsp = (bd_storage_p)p->cookie;
	}

	switch (work) {
	case ABWORK_INIT:
		/*
		 * See if we can find the default boot device
		 * from the list of bootable devices.
		 */

		make_boot_list(&boot_list, &nboot_list);
		for (single = boot_list; single < &boot_list[nboot_list];
		    single++) {
			if (single->flags & MF_SELECTED) {

				bsp->bdp = (bef_dev *)single->datum;
			}
		}
		free_boot_list(boot_list, nboot_list);
		break;

	case ABWORK_SHOW:

		if (bsp->bdp != NULL) {
			(void) make_boot_desc(bsp->bdp, bsp->bootpath, 0);
			return ((void *)bsp->bootpath);
		} else {
			return ((void *)"None");
		}
		/*NOTREACHED*/
		break;

	case ABWORK_MOD:

		for (; ; ) {
			make_boot_list(&boot_list, &nboot_list);
			switch (select_menu("MENU_HELP_DEFAULT_BOOT",
			    aboot_opts, NABOOT_OPTS,
			    boot_list, nboot_list,
			    MS_ZERO_ONE, "MENU_DEFAULT_BOOT",
			    nboot_list)) {
			case FKEY(2):
				free_boot_list(boot_list, nboot_list);
				if ((single = get_selection_menu(boot_list,
				    nboot_list)) == NULL) {
					beep_tty();
				} else {
					bsp->bdp = (bef_dev *)single->datum;
					p->flag |= ABFLAG_MOD;
					return ((void *)0);
				}
				break;

			case FKEY(3):
				free_boot_list(boot_list, nboot_list);
				return ((void *)0);
			}
		}
		/*NOTREACHED*/
		break;

	case ABWORK_SAVE:

		if (bsp->bdp) {
			set_default_boot_dev(bsp->bdp);
		}
		if (get_bootpath())
			bootpath_set = parse_bootpath();
		break;

	case ABWORK_FREE:

		free(bsp->bootpath);
		free(bsp);
		p->cookie = NULL;
		break;
	}
	return ((void *)0);
}

struct menu_list autoboot_list[] = {
	{ "Accept Settings", &ab_prop_list[0], MF_SELECTED, 0 },
	{ "Set Default Boot Device", &ab_prop_list[1], 0, 0 },
	{ "Set Autoboot Timeout", &ab_prop_list[2], 0, 0 },
	{ "Set Autoboot (ON/OFF)", &ab_prop_list[3], 0, 0 },
};
#define	NAUTOBOOT_LIST (sizeof (autoboot_list) / sizeof (struct menu_list))

void
auto_boot_cfname(void)
{
	struct menu_list *choice;

	ab_prop_list[0].func(&ab_prop_list[0], ABWORK_INIT);

	for (; ; ) {
		switch (select_menu("MENU_HELP_AUTO_BOOT", aboot_opts,
		    NABOOT_OPTS, autoboot_list, NAUTOBOOT_LIST, MS_ZERO_ONE,
		    "MENU_AUTO_BOOT",
		    ab_prop_list[1].func(&ab_prop_list[1], ABWORK_SHOW),
		    (int)ab_prop_list[2].func(&ab_prop_list[2], ABWORK_SHOW),
		    ab_prop_list[3].func(&ab_prop_list[3], ABWORK_SHOW))) {
		case FKEY(2):
			if ((choice = get_selection_menu(autoboot_list,
			    NAUTOBOOT_LIST)) == NULL) {
				beep_tty();
			} else {
				/*
				 * The next statement has a strange
				 * indenting only because cstyle complains
				 * about the line being greater than
				 * 80 characters otherwise.
				 */
	if ((*((ab_prop_p)choice->datum)->func)(choice->datum, ABWORK_MOD))
					return;
			}
			break;

		case FKEY(3):
			ab_prop_list[0].func(&ab_prop_list[0], ABWORK_FREE);
			return;
		}
	}
}

#define	TBUF_SIZE 512

int
copy_file_cfname(char *new_file, char *old_file)
{
	int nfd, ofd;
	int n;
	char tbuf[TBUF_SIZE];

	if ((ofd = _open(old_file, _O_RDONLY|_O_BINARY)) >= 0) {
		nfd = _open(new_file, _O_WRONLY|_O_CREAT|_O_TRUNC|_O_BINARY,
			0666);
		if (nfd < 0) {
			enter_menu(0, "FILE_CREATE_ERR", new_file);
			return (1);
		}

		while ((n = _read(ofd, tbuf, TBUF_SIZE)) > 0) {
			if (_write(nfd, tbuf, n) < 0) {
				return (1);
			}
		}
		_close(nfd);
		_close(ofd);
	}
	return (0);
}
