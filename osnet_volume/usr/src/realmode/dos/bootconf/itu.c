/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)itu.c	1.24	99/04/23 SMI"

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <names.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include "types.h"

#include "adv.h"
#include "boards.h"
#include "bop.h"
#include "boot.h"
#include "config.h"
#include "debug.h"
#include "devdb.h"
#include "dir.h"
#include "enum.h"
#include "err.h"
#include "escd.h"
#include "gettext.h"
#include "itu.h"
#include "menu.h"
#include "pnpbios.h"
#include "probe.h"
#include "resmgmt.h"
#include "setjmp.h"
#include "tree.h"
#include "tty.h"
#include "prop.h"

extern int stricmp(const char *, const char *);
extern int unlink(const char *);

/*
 * itu.c - Install Time Updates. Phase 1
 *
 */


#define	OVERWRITE_PROP 0
#define	UPDATE_PROP 1
#define	ZALLOC(ptr, nelm, struct_t)	\
if ((ptr = (struct_t *)calloc((nelm), sizeof (struct_t))) == NULL) {	\
		MemFailure();		\
}

#define	FREE_FLOPPY_VOLUMES()	\
{				\
	free_entries(&volist);	\
	volist.ent_hp = NULL;	\
}

#define	WAIT_FOR_ENTER() { \
	if (dflg) { \
		iprintf_tty("Press Enter to continue\n"); \
		getc_tty(); \
	} \
}

/*
 * Global variables
 */

int dflg = 1;		/* debug flag 0, 1 levels for now */

struct drvlist {
	struct drvlist *np;
	char name[PATH_MAX];
} *drvhp;

static ent_head_t volist;		/* list of floppy volumes read */

static itu_config_t *itu_config_head;	/* headptr for list of ITU config */
static itu_config_t  *itu_tail;		/* tail of ITU config structures */
static char os_ver[64];			/* os version to be installed */
static char errmsg[MAXLINE];		/* String for error messages */
static char *linebuf;			/* Large line buffer */

/*
 * Declarations of local functions
 */

void output_drvinfo();
#ifdef ITU_DEBUG
void cpy_token(char **name, char *token, int *errflg,
		itu_config_t *itu_cur);
#else /* ITU_DEBUG */
void cpy_token(char **name, char *token, int *errflg);
#endif /* ITU_DEBUG */
void insert_entry(ent_head_t *hp, entries_t *ep);
void reset_keyval_instances();
void free_entries();
void free_all_ituconfigs();
void free_itu(itu_config_t *itup);
void free_db(dbe_t *dbp);
void conv_num_to_dotnum(char *from, char *to);
void conv_dotnum_to_num(char *from, char *to);
void add_db_entries(itu_config_t *itup);
void write_prop(char *prop, char *where, char *val,
		char *sep, int flag);
void menu_list_ituos(/**/);
void output_maps();
#ifdef obsolete
void ckstr(char *comment, char *str);
#endif /* obsolete */
void create_kernel_files();
int read_itu_floppy(void);
int get_itu_bef_paths(char *path, struct flop_info **fpp);
int parse_itu(FILE *fp, itu_config_t *itu_cur, char *bef,
		dbe_t *db_common);
int getkeyword(char *token, int *errflg);
int check_token(char *token, char *name);
int check_dbes(dbe_t *dbes, itu_types_t type);
int cache_file(char *name, char **volname);
int have_read_this_floppy();
int check_default_release(char *path);
int token_is_whitespace(char *token);
int scan_headlists(Board *Head, itu_config_t *itup, char *headname);
int devid_found(Board *bp, char *itu_dev_id);
int device_exists(itu_config_t *itup);
int get_itu_info(struct flop_info *fp);
int is_pciclass(Board *bp, char *name);
char *get_volname();
char *get_os_ver(void);
char *add_db_entry(int keyword, char **curdbelm, char **commondb_elm,
		int *brace_lvl, char *sep, int *errflg);

char *get_quoted_tok(int lineno,  char *key_name, char *tok_name,
			int *errflg);


static struct menu_options Get_ITU_opts[] = {
	/*  Function key list for the main "probe" screen ...		    */

	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Cancel" },

	{ '\n', MA_RETURN, 0 },
};


#define	NGET_ITU_OPTIONS (sizeof (Get_ITU_opts) / sizeof (*Get_ITU_opts))

static struct menu_options Getnext_ITU_opts[] = {
	/* Function key list for the subsequent screen */
	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Cancel" },
	{ FKEY(4), MA_RETURN, "Done" },
	{ '\n', MA_RETURN, 0 },
};

#define	NGETNEXT_ITU_OPTIONS\
		(sizeof (Getnext_ITU_opts) / sizeof (*Getnext_ITU_opts))


/*
 * Ask the user whether she has ITU floppies to read
 */
void
ask_for_itu(void)
{
	char relpath[MAXLINE];
	int switch_screen = 0;
	int choice;
	int status;

	/*
	 * If default release file exists on the boot floppy use it.
	 */
	if (check_default_release(relpath)) {
		strcpy(os_ver, relpath);
	}
	while (1 /* CONSTANTCONDITION */) {
		if (!switch_screen)
			choice = text_menu("MENU_HELP_DU", Get_ITU_opts,
			    NGET_ITU_OPTIONS, NULL,
			    booted_from_eltorito_cdrom ? "MENU_GET_DU_CDBOOT" :
			    "MENU_GET_DU");
		else
			choice = text_menu("MENU_HELP_DU", Getnext_ITU_opts,
			    NGETNEXT_ITU_OPTIONS, NULL,
			    booted_from_eltorito_cdrom ?
			    "MENU_GET_NEXT_DU_CDBOOT" : "MENU_GET_NEXT_DU");

		switch (choice) {
		case '\n':
		case FKEY(2):
			status = read_itu_floppy();
			if (status <= 0)
				switch_screen++;
			break;

		case FKEY(3):
			os_ver[0] = '\0';
			free_all_ituconfigs();
			FREE_FLOPPY_VOLUMES();
			/*
			 * We only return from the main screen
			 */
			if (switch_screen) {
				switch_screen = 0;
				break;
			} else {
				return;
			}

		case FKEY(4):
			/* Write out the mappings */
			output_maps();
			menu_list_ituos();
			return;
		}
	}
}

static struct menu_options verify_osver_choices[] = {
	/* Function key list for version mismatch */
	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Cancel" },
	{ '\n', MA_RETURN, 0},
};

#define	NVERIFY_CHOICES \
	(sizeof (verify_osver_choices) / sizeof (*verify_osver_choices))

/*
 * This drives all the ITU processing.
 * Return values relate strictly to switching screens.
 * Return 0 or -1 will switch the screen.
 */
int
read_itu_floppy(void)
{
	static int create_kfiles = 0;
	char *sp;
	struct flop_info *finfo, *np, *pp;
	char *chosen_os_ver = NULL;
	int drvdirs;
	int drvcnt = 1;
	FILE *fp;
	char layout_ver[MAXLINE];
	/*
	 * We open the following files upfront to ensure that
	 * even if the RAM file run's out of space, we dont have
	 * problem with these files
	 */
restart:
	if (!create_kfiles) {
		create_kernel_files();
		create_kfiles++;
	}
	finfo = NULL;
	status_menu("Please wait ...", "MENU_READ_DU_FLOPPY");


	if (os_ver[0] == '\0') {
		if (((chosen_os_ver = get_os_ver()) == NULL) ||
		    (strcmp(chosen_os_ver, BACK_UP) == 0))
			return (1);
	} else {
		/*
		 * We are reading the next floppy. Remove U:
		 * This could be simplified to just *os_ver = 'A'
		 * NOTE: Get_itu_bef_paths modified this back to "U:"
		 */
		sp = strstr(os_ver, "U:");
		*sp = 'A';
		chosen_os_ver = os_ver;

		if (have_read_this_floppy())
			return (-1);
	}

	status_menu("Please wait ...", "MENU_LOAD_DU");

	drvdirs = get_itu_bef_paths(chosen_os_ver, &finfo);
	if (drvdirs <= 0) {
		/* we have problem captain */
		struct _stat st;

		sprintf(layout_ver, "A:/DU/%s", chosen_os_ver);
		if (_stat(layout_ver, &st) != 0) {
			/*
			 * A DU floppy but does not have the right release
			 */
			sp = strstr(chosen_os_ver, OSDIR_HEAD);
			conv_num_to_dotnum(sp, layout_ver);
			switch (text_menu("MENU_HELP_VERIFY_OSVER_CHOICES",
					verify_osver_choices,
					NVERIFY_CHOICES, NULL,
					"MENU_VERIFY_OSVER_CHOICES",
					layout_ver)) {
			case FKEY(3):
				os_ver[0] = '\0';
				free_all_ituconfigs();
				FREE_FLOPPY_VOLUMES();
				return (1);

			case '\n':
			case FKEY(2):
				goto restart;
			}
		}

		/*
		 * Otherwise the DU floppy has no bootable component.
		 */
		enter_menu(0, "MENU_RELOAD_DU_FLOPPY");
		status_menu("Please wait ...", "MENU_READ_DU_FLOPPY");
		goto restart;
	}
	/*
	 * Check here if the layout is ok.
	 */
	sprintf(layout_ver, "%s%s", chosen_os_ver, LAYOUT_VER_FILE);
	if ((fp = fopen(layout_ver, "r")) == NULL) {
		enter_menu(0, "DU_LAYOUT_OPEN_FAILED", layout_ver);
		return (-1);
	} else {
		/*
		 * Read exactly one line and that should match.
		 * XXX This condition may need to be changed if the
		 * relase layout version changes.
		 */
		while (((sp = fgets(layout_ver, MAXLINE, fp)) != NULL) &&
			*sp == '#');

		fclose(fp);
		if (sp) {
			while (isspace(*sp++));
			if (strncmp((sp - 1), LAYOUT_VERSION, 3) != 0) {
				*(sp + 3) = 0;
				enter_menu(0, "DU_LAYOUT_MISMATCH",
					(sp - 1), LAYOUT_VERSION);
				return (-1);
			}
		}
	}

	/* Set up the line bars */
	lb_init_menu(12);
	lb_info_menu(gettext("Building driver list..."));
	lb_scale_menu(drvdirs);
	if (finfo) {
		np = finfo;
		do {
			char *drvitu;


			drvitu = strrchr(np->itu_name, SLASH);
			if (drvitu)
			    drvitu++;
			lb_info_menu("Processing: %s", drvitu);
			lb_inc_menu();
			drvcnt++;
			if (get_itu_info(np) < 0) {
				enter_menu(0, "DU_PROCESSING_ERROR",
						np->itu_name);
				debug(D_ERR, "processing itu %s failed\n",
					np->itu_name);
			}
			pp = np;
			np = np->f_nextp;
			pp->f_nextp = NULL;
			free(pp);
		} while (np);
		/* set os_ver for the first time through */
		if (os_ver[0] == '\0')
			strcpy(os_ver, chosen_os_ver);
	}
	return (0);
}

int
check_default_release(char *relpath)
{
	int retval = 0;
	char path[MAXLINE];
	char line[MAXLINE];
	struct _stat st;
	char ver[5];
	FILE *fp;

	sprintf(path, "U:/DU/%s", DEFAULT_RELEASE);

	/*
	 * Its ok to proceed with fopen failing. This would be the
	 * normal case in where "release.def" does not exist.
	 */
	if ((fp = fopen(path, "r")) == NULL) {
		debug(D_ERR, "get_os_ver: fopen of %s FAILED\n", path);
		return (retval);
	}

	while (fgets(line, MAXLINE, fp) != NULL) {
		if (*line == '#')
			continue;
		/* we only accept 1 value */
		conv_dotnum_to_num(line, ver);
		break;
	}
	/*
	 * Check if its a valid path.
	 */
	sprintf(path, "U:/DU/%s%s", OSDIR_HEAD, ver);
	if (_stat(path, &st) == 0) {
		sprintf(relpath, "%s%s", path, DRV_DIR);
		retval++;
	}
	fclose(fp);
	return (retval);
}


static struct menu_options osver_options[] = {
	/* menu to read in the os version */
	{ FKEY(2), MA_RETURN, "Continue"},
	{ FKEY(3), MA_RETURN, "Cancel"},

};

#define	OSVER_OPTIONS (sizeof (osver_options) / sizeof (*osver_options))

/*
 * get_os_version:
 * Present the user with the of available versions. Return the selected
 * version.  If the file version.def(default version) exists, options will
 * not be presented and contents of this will be used.
 *
 * Exit: Returns NULL on error.
 *
 */
struct vlist {
	char vdisplay[MAXLINE]; /* Display this to user */
	char vname[20]; /* converted name "U:/DU/sol_251" */
};

/*
 * Dislays the available versions on the floppy and returns the chosen
 * version.
 * On error returns NULL
 */
char *
get_os_ver()
{
	struct vlist itu_vers[MAXVERS]; /* XXX limit support for 5 versions */
	struct vlist *itu_vp;
	struct menu_list *version_list;
	struct menu_list *ver_choice, *mlp;
	char ver[5]; /* stores converted version 2.5.1 =>251 */
	static char path[MAXLINE];
	int i, count;
	int fin = 0;
	char tname[15];
	struct _find_t files;


	/*
	 * Force a read of the floppy to ensure U: works ok
	 * AND make sure that we have not read this floppy before.
	 */
	if (have_read_this_floppy())
		return (NULL);

	if (check_default_release(path))
		return (path);
	/*
	 * Open the top level directory and look for dirs of type OSDIR_HEAD
	 */
	i = 0;
	strcpy(tname, "U:/DU/sol_*");
	if (_dos_findfirst(tname, _A_SUBDIR, &files) == 0) {
		if (strstr(files.name, OSDIR_HEAD)) {
			conv_num_to_dotnum(files.name, ver);
			sprintf(itu_vers[i].vdisplay, "Solaris OS %s", ver);
			sprintf(itu_vers[i].vname, "/DU/%s", files.name);
			i++;
		}
	} else {
		/*
		 * Looks like a wrong floppy. Ask for retry
		 */
		enter_menu(0, "MENU_RELOAD_DU_FLOPPY");
		status_menu("Please wait ...", "MENU_READ_DU_FLOPPY");
		return (NULL);
	}

	while (i < MAXVERS && (_dos_findnext(&files) == 0)) {
		if (strstr(files.name, OSDIR_HEAD)) {
			conv_num_to_dotnum(files.name, ver);
			sprintf(itu_vers[i].vdisplay, "Solaris OS %s", ver);
			sprintf(itu_vers[i].vname, "/DU/%s", files.name);
			i++;
		}
	}
	/* now create the menu list */

	count = i; /* total number of lines in the file */

	if ((version_list = (struct menu_list *)
		calloc(count, sizeof (struct menu_list))) == NULL) {
		    MemFailure();
	}

	for (mlp = version_list, i = 0; i < count; i++, mlp++) {
		mlp->datum = (void *)&itu_vers[i];
		mlp->string = itu_vers[i].vdisplay;
	}

	/*
	 * User is ready to see the list and make choices.
	 */
	while (!fin) {
		switch (select_menu("MENU_HELP_OSVER", osver_options,
			OSVER_OPTIONS, version_list, count,
			MS_ZERO_ONE, "MENU_DU_OSVER", count)) {
		case '\n':
		case FKEY(2):
			/*  Chose a version */
			if ((ver_choice =
				get_selection_menu(version_list,
						count)) == NULL) {
				/* user didn't pick one */
				beep_tty();
				continue;
			}
			itu_vp = (struct vlist *)ver_choice->datum;
			sprintf(path, "U:%s%s", itu_vp->vname, DRV_DIR);
			fin = 1;
			break;

		case FKEY(3):
			/* Go back to the previous screen */
			strcpy(path, BACK_UP);
			fin = 1;
			break;
		}
	}
	if (version_list)
		free(version_list);
	return (path);
}


int
get_itu_bef_paths(char *ver, struct flop_info **fpp)
{
	struct drvlist *drvp;
	char pathname[PATH_MAX];
	char itu_name[PATH_MAX], bef_name[PATH_MAX];
	struct flop_info *tn, *finfo;
	struct _find_t	files;
	int cnt = 0;

	debug(D_FLOW, "%s\n", ver);
	finfo = NULL;

	/*
	 * We do a breadth first search and collect the list of driver
	 * directories. Then we scan in each of them to make a list
	 * of supported releases.
	 *
	 * AVOID readdir/stat approach since this significantly slows down
	 * the searches in boot.bin. We use DOS functions and limit the
	 * wildcarding.
	 */
	drvhp = NULL;
	sprintf(pathname, "%s*", ver);

	/* Scan DU/sol_26/i86pc/tools/boot/"*" for eg. */
	if (_dos_findfirst(pathname, _A_SUBDIR, &files) == 0) {
		if ((files.attrib & _A_SUBDIR) && (*files.name != '.')) {
			ZALLOC(drvp, 1, struct drvlist);
			sprintf(drvp->name, "%s%s", ver, files.name);
			drvhp = drvp;
		}
	}

	while (_dos_findnext(&files) == 0) {
		if ((files.attrib & _A_SUBDIR) && (*files.name != '.')) {
			ZALLOC(drvp, 1, struct drvlist);
			sprintf(drvp->name, "%s%s", ver, files.name);
			if (drvhp != NULL)
				drvp->np = drvhp;
			drvhp = drvp;
		}
	}

	/*
	 * If we dont have any driver dirs return error
	 */
	if (drvhp == NULL)
		return (-1);
	/*
	 * For a list of .itus and .befs from the drvlist
	 */
	for (drvp = drvhp; drvp; drvp = drvp->np) {
		/* find .itu and .befs for each */
		sprintf(itu_name, "%s/*.itu", drvp->name);
		sprintf(bef_name, "%s/*.bef", drvp->name);
		if (_dos_findfirst(itu_name, _A_NORMAL, &files) == 0) {
			sprintf(itu_name, "%s/%s", drvp->name, files.name);
		} else {
			/* if no .itu no can do..continue */
			/* XXX Print out a warning !!! */
			continue;
		}

		ZALLOC(tn, 1, flop_info_t);
		strcpy(tn->itu_name, itu_name);
		/* Found .itu now check for .bef */
		if (_dos_findfirst(bef_name, _A_NORMAL, &files) == 0) {
			sprintf(tn->bef_name, "%s/%s", drvp->name,
				files.name);
		}
		if (finfo != NULL) {
			tn->f_nextp = finfo;
		}
		finfo = tn;
		cnt++;
	}
	*fpp = finfo;
	/* Free drv list */
	do {
		drvp = drvhp;
		drvhp = drvhp->np;
		free(drvp);
	} while (drvhp);

	return (cnt);
}


/*
 * This does is the main routine. It calls parse_itu to parse the .itu file,
 * and creates a list of system, map, alias entries as needed.
 *
 * On Exit: returns 0. On error -1.
 *
 */
#define	ERROR_OUT()		\
{				\
	errflg++;		\
	goto exit_get_itu_info; \
}	/*NOTREACHED*/

#define	ADD_ALIAS_ENTRY(cp, alias_name)					\
{									\
	ZALLOC(ep, 1, entries_t);					\
	/* Allocate space for 2 extra quotes */				\
	ZALLOC(cp, (strlen(alias_name)+3), char);			\
	sprintf(cp, "\"%s\"", alias_name);				\
	debug(D_FLOW, "ALIAS: %s %s\n", itu_cur->itu_drv_name, cp);	\
	cpy_token(&(ep->en_data), cp, &errflg);				\
	if (!errflg)							\
		insert_entry(&(itu_cur->itu_aliases), ep);		\
}


static struct menu_options dup_ituo[] = {
	/* Function key list for duplicate ITUOs */
	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Cancel" },
};

#define	NDUP_ITUO (sizeof (dup_ituo) / sizeof (*dup_ituo))

int
get_itu_info(struct flop_info *fp_info)
{
	dbe_t *db_common;	/* handle for processing common entries. */
	dbe_t *dbp;
	FILE *itufp;		/* File pointer to the .itu file */
	struct _stat st;
	itu_config_t *itu_cur, *itupp, *itucp;
	int retval = -1;
	entries_t *ep;
	int errflg = 0;
	int matched;
	char *tn, *cp, *befname;
	char  *ituname = NULL;
	char *volname;
	char localbuf[PATH_MAX]; /* general purpose buffer */

	ituname = fp_info->itu_name;
	if ((itufp = fopen(ituname, "rb")) == NULL) {
		enter_menu(0, "ITU_OPEN_FAILED", ituname);
		return (retval);
	}
	/*
	 * Once file has been opened, we need a itu struct. Allocate it.
	 * Allocate the db_common to hold all the common entries;
	 */
	ZALLOC(itu_cur, 1, itu_config_t);
	ZALLOC(db_common, 1, dbe_t);

	if (*(befname = fp_info->bef_name)) {
		befname = strrchr(fp_info->bef_name, SLASH);
		befname++; /* get past the "/" */
		itu_cur->itu_bef_statd = I_TRUE;
	} else {
		befname = NULL;
		itu_cur->itu_bef_statd = I_FALSE;
	}

	/*
	 * Save the path where the .itu is located.
	 */
	if ((tn = strdup(ituname)) == NULL)
		ERROR_OUT();

	/* save name with "/" at the end */
	cp = strrchr(tn, SLASH);
	if (cp++)
		*cp = 0;

	/*
	 * force the entire path to be in lower case since
	 * DOS is stupid!! Its not case sensitive but stores
	 * the names in dirents in case sensitive manner.
	 */

	itu_cur->itu_pkgpath = _strlwr(realloc(tn, strlen(tn) + 1));
	if (itu_cur->itu_pkgpath == NULL)
		ERROR_OUT();

	if (errflg = parse_itu(itufp, itu_cur, befname, db_common)) {
		/* Need a menu here XXX */
#ifdef ITU_DEBUG
		iprintf_tty("get_itu_info: parse_itu error %d\n", errflg);
		WAIT_FOR_ENTER();
#endif /* ITU_DEBUG */
		goto exit_get_itu_info;
	}

	/*
	 * Before we go any further:
	 *  Check if the device for this ITUO exists if its not legacy or
	 *   load_always.
	 * if we decide to read it in check if its a duplicate.
	 */
	if (itu_cur->itu_legacy_dev != I_TRUE &&
	    itu_cur->itu_load_always != I_TRUE) {
		if (!device_exists(itu_cur)) {
			retval = 0;
			ERROR_OUT();
		}
	}

	itupp = itucp = itu_config_head;
	matched = 0;
	do {
		if (strcmp(itucp->itu_drv_name, itu_cur->itu_drv_name) == 0) {
			matched = 1;
			break;
		}
		itupp = itucp;
		itucp = itucp->itu_nextp;
	} while (itucp == NULL);


	if (matched) {
		switch (text_menu(0, dup_ituo,
			NDUP_ITUO, NULL, "MENU_DUP_ITUO",
			itucp->itu_drv_name)) {
		case FKEY(2):
			if (itucp->itu_pstamp < itu_cur->itu_pstamp) {
				/* remove the previous one from the list */
				itupp->itu_nextp = itucp->itu_nextp;
				free_itu(itucp);
				break;
			}
			/* Fall through for the else condition */

			/*FALLTHROUGH*/
		case FKEY(3):
			/*
			 * already read the newer one so return.
			 * set errflg for clean up and set retval to
			 * itu_config_head to prevent error messages.
			 */
			retval = 0;
			ERROR_OUT();
		}
	}

	/*
	 * Save the volume name since this is for keeps.
	 */
	if (cp = get_volname())
		strcpy(itu_cur->itu_volname, cp);
	else
		ERROR_OUT();

	/*
	 * Check if a kernel driver and .conf exist. If so create a mapping
	 * entries for them. XXX Ideally We should check the loadpath when
	 * creating these entries. But we have no access to that info at
	 * this stage!!
	 */

	sprintf(localbuf, "%s%s", itu_cur->itu_pkgpath, itu_cur->itu_drv_name);

	/*
	 * Create a mapping only if there is file.
	 * Map entry for a kernel driver.
	 */

	if (_stat(localbuf, &st) == 0) {
		itu_cur->itu_kdrv_statd = I_TRUE;
		ZALLOC(tn, PATH_MAX, char);
		ZALLOC(ep, 1, entries_t);
		cache_file(localbuf, &volname);
		cp = localbuf + 2; /* point past the "U:" */
		sprintf(tn, "%s%s" SPACES "%s%s", itu_cur->itu_drvpath,
				itu_cur->itu_drv_name, volname, cp);
		ep->en_data = realloc(tn, strlen(tn)+1);
		insert_entry(&(itu_cur->itu_maps), ep);

		/*
		 * create a forceload for the module
		 *
		 * Workaround: find the last component of module name.
		 * Note that a terminating slash has been arranged above.
		 */
		itu_cur->itu_drvind = 0;

		for (tn = itu_cur->itu_drvpath; tn && *tn; tn++) {
			/* save index of last non-terminating slash */
			if (*tn == SLASH && *(tn+1) && *(tn+1) != '\n')
				itu_cur->itu_drvind =
					tn - itu_cur->itu_drvpath + 1;
		}

		ZALLOC(cp, strlen("forceload: ") +
			strlen(&itu_cur->itu_drvpath[itu_cur->itu_drvind]) +
			strlen(itu_cur->itu_drv_name) + 1, char);
		sprintf(cp, "forceload: %s%s",
			&itu_cur->itu_drvpath[itu_cur->itu_drvind],
			itu_cur->itu_drv_name);

		ZALLOC(ep, 1, entries_t);
		cpy_token(&(ep->en_data), cp, &errflg);
		free(cp);
		debug(D_FLOW, "SYSENT_FOR_DRIVER:  %s\n", ep->en_data);

		/*
		 * Insert at the head of the list for system entries.
		 * The forceload for the kernel driver should be at the
		 * start so that if any other variables are set in  the
		 * the driver thru system, they will be applied.
		 */

		if (itu_cur->itu_sysents.ent_hp)
			ep->en_nextp = itu_cur->itu_sysents.ent_hp;
		else
			itu_cur->itu_sysents.ent_tailp = ep;
		itu_cur->itu_sysents.ent_hp = ep;
	}

	/*
	 * map entry for conf files.
	 */
	strcat(localbuf, ".cnf");
	if (_stat(localbuf, &st) == 0) {
		itu_cur->itu_cnf_statd = I_TRUE;
		ZALLOC(tn, PATH_MAX, char);
		ZALLOC(ep, 1, entries_t);
		cache_file(localbuf, &volname);
		cp = localbuf + 2; /* point past "U:" */
		sprintf(tn, "%s%s.conf" SPACES "%s%s",
			itu_cur->itu_drvpath, itu_cur->itu_drv_name,
			volname, cp);

		ep->en_data = realloc(tn, strlen(tn)+1);
		debug(D_FLOW, "SYSENT_FOR_CONF: %s\n en_data %s\n",
			cp, ep->en_data);
		insert_entry(&(itu_cur->itu_maps), ep);
	}
	/*
	 *  create a map entry for the bef.
	 */
	if (befname) {
		ZALLOC(tn, PATH_MAX, char);
		ZALLOC(ep, 1, entries_t);
		sprintf(localbuf, "%s%s", itu_cur->itu_pkgpath, befname);
		cache_file(localbuf, &volname);
		/*
		 * If rmodepath is defined, legacy is ignored
		 */
		if (itu_cur->itu_rmodepath) {
			char *bpath;

			/*
			 * Since the bootconf opens bef drivers as
			 * "solaris/drivers/.../" we have to cut
			 * the realmodepath up if we are not booting
			 * from a CD (see comment on cacheing below).
			 */
			if (booted_from_eltorito_cdrom)
				bpath = itu_cur->itu_rmodepath;
			else {
				bpath = strstr(itu_cur->itu_rmodepath,
				    "solaris");
				bpath--; /* backup to / */
			}

			sprintf(tn, "%s%s" SPACES "%s%s%s",
				bpath, befname, volname,
				itu_cur->itu_pkgpath+2, befname);
		} else {
			/*
			 * The compfs code that implements cacheing does not
			 * handle filenames well.  It requires different
			 * mapped names depending on whether the boot
			 * device is a diskette (in which case the name
			 * should not start with "/boot") or a CDROM (in
			 * which case it should).
			 */
			if (itu_cur->itu_legacy_dev == I_TRUE)
				sprintf(tn, "%s%s" SPACES "%s%s%s",
					booted_from_eltorito_cdrom ?
					    CD_LEGACY_DIR : LEGACY_DIR,
					befname, volname,
					itu_cur->itu_pkgpath+2, befname);
			else
				sprintf(tn, "%s%s" SPACES "%s%s%s",
					booted_from_eltorito_cdrom ?
					    CD_SELFID_DIR : SELFID_DIR,
					befname, volname,
					itu_cur->itu_pkgpath+2, befname);
		}

		ep->en_data = realloc(tn, strlen(tn)+1);
		insert_entry(&(itu_cur->itu_maps), ep);
	}

	/*
	 * create aliases
	 */
	for (dbp = itu_cur->itu_dbes; dbp; dbp = dbp->db_nextp) {
		int nname, btype;
		char *p, *alias_name, *nxt;

		/*
		 * Create aliases only when there is no match
		 */
		if (strcmp(itu_cur->itu_drv_name, dbp->db_node_name)) {
			debug(D_FLOW, "creating aliases\n");
			/*
			 * If we have pciclass as bustype and none as nodename
			 * the alias is dev_id.
			 */
			nname = strcmp(dbp->db_node_name, "none");
			btype = strcmp(dbp->db_bus_type, "pciclass");
			if (nname == 0 || btype == 0) {
				if (nname == 0 && btype == 0) {
					p = alias_name = strdup(dbp->db_dev_id);
					if (alias_name == NULL)
						goto exit_get_itu_info;
					do {
					    if (nxt = strchr(alias_name, '|'))
						*nxt++ = 0;

					    ADD_ALIAS_ENTRY(cp, alias_name);
					    alias_name = nxt;
					} while (alias_name);
					free(p);
				} else {
					sprintf(errmsg, gettext("nodename or "
						"bustype not set correctly for "
						"pciclass\n"));
					enter_menu(0, "ITU_PARSE_ERROR",
						errmsg);
					goto exit_get_itu_info;
				}
			} else {
				alias_name = dbp->db_node_name;
			}

			ADD_ALIAS_ENTRY(cp, alias_name);
		}
	}

	if (errflg)
		goto exit_get_itu_info;
	/*
	 * Add the entries to the device database.
	 */

	add_db_entries(itu_cur);

	/*
	 * Print out the info. Debugging stuff
	 */
	debug(D_FLOW, "update: %s\n", (itu_cur->itu_type == PARTIAL) ?
						" partial": " complete");
	if (itu_cur->itu_type == PARTIAL) {
		int num_actions;

		num_actions = 0;
		/* now figure out what kind of an update */
		if (itu_cur->itu_bef_statd == I_TRUE) {
			debug(D_FLOW, "updating bef: %s path %s\n",
				itu_cur->itu_bef_name,
				itu_cur->itu_rmodepath);
			num_actions++;
		}
		if (itu_cur->itu_dbes != NULL) {
			debug(D_FLOW, "updating: master file");
			num_actions++;
		}
		if (itu_cur->itu_kdrv_statd == I_TRUE) {
			debug(D_FLOW, " - updating: kernel driver");
			num_actions++;
			if (itu_cur->itu_load_always == I_TRUE)
				debug(D_FLOW, "\"load always\"\n");
			if (itu_cur->itu_legacy_dev == I_TRUE)
				debug(D_FLOW, "legacy device");
		}
		if (itu_cur->itu_cnf_statd == I_TRUE) {
			debug(D_FLOW, " and .conf file");
			num_actions++;
		}
		if (num_actions == 0)
			debug(D_ERR, "Nothing to update ?!!! ***");
	} else {
			debug(D_FLOW, "Installing new drivers.");
	}
	debug(D_FLOW, "\n");

	debug(D_FLOW, "name: %s\n", itu_cur->itu_drv_name);
	for (ep = itu_cur->itu_classes.ent_hp; ep; ep = ep->en_nextp)
		debug(D_FLOW, "CLASS_ENTRY: %s\n", ep->en_data);
	for (ep = itu_cur->itu_sysents.ent_hp; ep; ep = ep->en_nextp)
		debug(D_FLOW, "SYSTEM_ENTRY: %s\n", ep->en_data);
	for (ep = itu_cur->itu_machents.ent_hp; ep; ep = ep->en_nextp)
		debug(D_FLOW, "MACH_ENTRY: %s\n", ep->en_data);
	for (ep = itu_cur->itu_devlink.ent_hp; ep; ep = ep->en_nextp)
		debug(D_FLOW, "DEVLINK_ENTRY: %s\n", ep->en_data);
	for (ep = itu_cur->itu_aliases.ent_hp; ep; ep = ep->en_nextp)
		debug(D_FLOW, "ALIAS_ENTRY: %s %s\n",
				itu_cur->itu_drv_name, ep->en_data);
	for (ep = itu_cur->itu_maps.ent_hp; ep; ep = ep->en_nextp)
		debug(D_FLOW, "MAP_ENTRY: %s\n", ep->en_data);

	/*
	 * print out the db entries.
	 */

	for (dbp = itu_cur->itu_dbes; dbp; dbp = dbp->db_nextp) {
		debug(D_FLOW, "DB_ENTRY %s %s %s %s %s \"%s\"\n",
			    dbp->db_dev_id, dbp->db_node_name,
			    dbp->db_dev_type, dbp->db_bus_type,
			    itu_cur->itu_bef_name, dbp->db_dev_desc);
		debug(D_FLOW, "\n\n\n");
	}

exit_get_itu_info:
	fclose(itufp);
	reset_keyval_instances();
	if (!errflg) {
		/*
		 * Add to itu_config_head list
		 */
		if (itu_config_head == NULL)
			itu_config_head = itu_tail = itu_cur;
		else {
			itu_tail->itu_nextp = itu_cur;
			itu_tail = itu_cur;
		}
		/*
		 * XXXX
		 * If there are no common elements db_common must be freed
		 */
		retval = 0;
	} else {
		if (itu_cur) {
			free_itu(itu_cur);
			free(itu_cur);
		}
		if (db_common)
			free_db(db_common);

	}
	return (retval);
}

/*
 * Parse the itu file. If successful return 0 else num of errors found.
 */

#define	FILL_COMMON_DBE(name, db_elm, errflg)				\
{									\
	if (db_common->db_elm) {					\
		int i;							\
									\
		if (dflg > 2)						\
			debug(D_FLOW, "FILL_COMMON_DBE" name);		\
		for (db_cur = db_list, i = 1; db_cur;			\
				db_cur = db_cur->db_nextp, i++) {	\
			if (db_cur->db_elm) {				\
				debug(D_FLOW, "WARNING:" name		\
					" redeclared in para %d.", i);	\
				debug(D_FLOW, "Override %s with common %s\n", \
					db_cur->db_elm, db_common->db_elm); \
			}						\
			cpy_token(&(db_cur->db_elm), db_common->db_elm,	\
								&errflg); \
		}							\
	}								\
}

int
parse_itu(FILE *fp, itu_config_t *itu_cur, char *bef, dbe_t *db_common)
{
	int val, i;
	char *line = linebuf;
	int lineno, parse_err;
	char *token, *token2, *cp;
	dbe_t *db_list, *db_cur, *db_ent;
	entries_t *ep;
	int brace_lvl;		/* nesting depths */

	/* allow legal maximum lines */
	if (!line) {
		line = linebuf = malloc(MASTER_LINE_MAX);
		if (!line)
			MemFailure();
	}

	/*
	 * Init brace_lvl and other parameters
	 */

	lineno = 0;
	brace_lvl = 0;
	parse_err = 0;

	db_list = db_cur = NULL;
	itu_cur->itu_type = UNDEFINED;
	itu_cur->itu_legacy_dev = I_FALSE;
	itu_cur->itu_load_always = I_FALSE;
	itu_cur->itu_install_always = I_FALSE;

	while (fgets(line, MASTER_LINE_MAX, fp) != NULL) {
		lineno++;
		debug(D_ITU, "%d:- read:--  %s", lineno, line);
		/* if its a comment skip */
		if (*line == '#') {
			debug(D_ITU, "line %d:- skipped\n", lineno);
			continue;
		}
		if ((token = (char *)strtok(line, CR_ASSIGN_SEP)) == NULL) {
			debug(D_ITU, "lines %d:- skipping a blank line\n",
				lineno);
			continue;
		}
		do {
			debug(D_ITU, "token: %s ", token);
			if ((cp = strpbrk(token, BRACES)) != NULL) {
				if (*cp == OPEN_BRACE) {
					if (brace_lvl == 0) {
						ZALLOC(db_ent, 1, dbe_t);
						brace_lvl++;
						/*
						 * XXX does'nt zallocing
						 * avoid this
						 */
						db_ent->db_nextp =
						    db_ent->db_prevp = NULL;
						if (!db_list) {
						    /* first time around */
						    db_list = db_cur = db_ent;
						} else {
						    db_cur->db_nextp = db_ent;
						    db_cur = db_ent;
						}
						/*
						 * Since there could be other
						 * tokens on the same line,
						 * walk past the brace and
						 * remove white space.
						 */
						cp++;
						while (isspace(*cp))
							cp++;
						/*
						 * Since CR_ASSIGN_SEP in
						 * strtok removes returns
						 * no need to check for that.
						 */
						if (*cp == '\0') {
							token = NULL;
							continue;
						}
						token = cp;
					} else {
						sprintf(errmsg,
							gettext("Line %d: "
						"Too many open braces %d"),
						lineno, brace_lvl);
						enter_menu(0, "DU_PARSE_ERROR",
									errmsg);
						parse_err++;
					}
				} else { /* CLOSE_BRACE seen */
					if (brace_lvl == 1) {
						brace_lvl--;
						/*
						 * Since we specify that no
						 * other processing will be
						 * done just continue.
						 */
						token = NULL;
						continue;
					} else {
						sprintf(errmsg,
							gettext("Line %d: "
						"Too many close braces %d"),
						lineno, brace_lvl);
						enter_menu(0, "DU_PARSE_ERROR",
									errmsg);
						parse_err++;
					}
				}
			}
			val = getkeyword(token, &parse_err);
			debug(D_ITU, "%d: val is %d for token %s\n",
				lineno, val, token);
			switch (val) {

			case IF_VERSION:
				token = (char *)strtok(NULL, WHITESP_CR_SEP);
				debug(D_ITU, "IF_VERSION: %s\n",
					(token)? token: "NIL");

				if (check_token(token, "interface_version")) {
					parse_err++;
				} else
					itu_cur->itu_if_version = atof(token);
				break;

			case PSTAMP:
				token = (char *)strtok(NULL, WHITESP_CR_SEP);
				debug(D_ITU, "PSTAMP: %s\n", token);

				if (check_token(token, "pstamp")) {
					parse_err++;
				} else
					itu_cur->itu_pstamp =
						strtoul(token, NULL, 10);
				break;

			case ITU_TYPE:
				token = (char *)strtok(NULL, WHITESP_CR_SEP);
				debug(D_ITU, "ITU_TYPE: %s\n",
					(token)? token: "NIL");

				if (check_token(token, "itu_type")) {
				    parse_err++;
				    break;
				}
				if (stricmp(token, "partial") == 0)
					itu_cur->itu_type = PARTIAL;
				else if (stricmp(token, "complete") == 0)
					itu_cur->itu_type = COMPLETE;
				else {
					sprintf(errmsg,
						gettext("unknown itu_type "
						"\" %s \"\n"), token);
					enter_menu(0, "ITU_PARSE_ERROR",
						errmsg);
					parse_err++;
				}
				break;

			case DRV_NAME:
				token = (char *)strtok(NULL, WHITESP_CR_SEP);
				debug(D_ITU, "DRV_NAME: %s\n", token);

				if (check_token(token, "name")) {
					parse_err++;
				} else {
					cpy_token(&(itu_cur->itu_drv_name),
						token, &parse_err);
				}
				break;


			case DRV_CLASS:
				/* XXX: should this use get_quoted_token? */
				token = (char *)strtok(NULL, WHITESP_CR_SEP);
				debug(D_ITU, "DRV_CLASS: %s\n", token);

				if (check_token(token, "class")) {
					parse_err++;
				} else {
					ZALLOC(ep, 1, entries_t);
					cpy_token(&(ep->en_data), token,
								&parse_err);
					insert_entry(&(itu_cur->itu_classes),
									ep);
				}
				break;

			case SYSTEM_ENTRY:
				/*
				 * Copy the entry
				 */
				token = get_quoted_tok(lineno, "system_entry",
						"SYSTEM_ENTRY", &parse_err);

				if (token) {
					ZALLOC(ep, 1, entries_t);

					if (strncmp(token,
					    BOOT_ONLY_SYS,
					    BOOT_ONLY_SYS_LEN) == 0) {
						token += BOOT_ONLY_SYS_LEN;
					}

					cpy_token(&(ep->en_data), token,
								&parse_err);
					insert_entry(&(itu_cur->itu_sysents),
									ep);
				}
				break;

			case FILE_EDIT:
				/*
				 * Copy the entry
				 */
				token = (char *)strtok(NULL, WHITESP_CR_SEP);

				token2 = get_quoted_tok(lineno, "mach_entry",
						"MACH_ENTRY", &parse_err);

				if (token && token2) {
					char *linebuf;

					linebuf = (char *)malloc(MAXLINE);
					if (!linebuf)
						MemFailure();

					sprintf(linebuf, "%s %s",
						token, token2);
					ZALLOC(ep, 1, entries_t);
					cpy_token(&(ep->en_data), linebuf,
								&parse_err);
					insert_entry(&(itu_cur->itu_edits),
									ep);
					free(linebuf);
				}
				break;
			case MACH_ENTRY:
				/*
				 * Copy the entry
				 */
				token = get_quoted_tok(lineno, "mach_entry",
						"MACH_ENTRY", &parse_err);

				if (token) {
					ZALLOC(ep, 1, entries_t);
					cpy_token(&(ep->en_data), token,
								&parse_err);
					insert_entry(&(itu_cur->itu_machents),
									ep);
				}
				break;

			case DEVLINK_ENTRY:
				token = get_quoted_tok(lineno, "devlink_entry",
						"DEVLINK_ENTRY", &parse_err);

				if (token) {
					ZALLOC(ep, 1, entries_t);
					cpy_token(&(ep->en_data), token,
							    &parse_err);
					insert_entry(&(itu_cur->itu_devlink),
									ep);
				}
				break;

			case DEV_ID:
				token = add_db_entry(val,
						&(db_cur->db_dev_id),
						&(db_common->db_dev_id),
						&brace_lvl, WHITESP_CR_SEP,
						&parse_err);
				check_token(token, "dev_id");
				break;

			case NODE_NAME:
				token = add_db_entry(val,
						&(db_cur->db_node_name),
						&(db_common->db_node_name),
						&brace_lvl,  WHITESP_CR_SEP,
						&parse_err);
				check_token(token, "node_name");
				break;

			case DEV_TYPE:
				token = add_db_entry(val,
						&(db_cur->db_dev_type),
						&(db_common->db_dev_type),
						&brace_lvl,  WHITESP_CR_SEP,
						&parse_err);
				check_token(token, "dev_type");
				break;

			case BUS_TYPE:
				token = add_db_entry(val,
						&(db_cur->db_bus_type),
						&(db_common->db_bus_type),
						&brace_lvl,  WHITESP_CR_SEP,
						&parse_err);
				check_token(token, "bus_type");
				break;

			case DEV_DESC:
				token = add_db_entry(val,
						&(db_cur->db_dev_desc),
						&(db_common->db_dev_desc),
						&brace_lvl,  QUOTE_SEP,
						&parse_err);
				break;

			case BEF_NAME:
				token = (char *)strtok(NULL, WHITESP_CR_SEP);
				debug(D_ITU, "BEF_NAME: %s\n", token);

				/*
				 * If the .bef found does not match this
				 * we barf.
				 */
				if (!check_token(token, "bef_name") && bef &&
					(strcmp(token, bef) != 0)) {
					sprintf(errmsg, "bef_name %s and"
						" file found %s dont match\n",
						token, bef);
					enter_menu(0, "ITU_PARSE_ERROR",
							errmsg);
				} else {
					cpy_token(&(itu_cur->itu_bef_name),
						token, &parse_err);
				}
				break;

			case REALMODE_PATH:
				token = strtok(NULL, WHITESP_CR_SEP);
				debug(D_ITU, "REAL_MODE_PATH: %s\n", token);

				if (check_token(token, "realmode_path")) {
					parse_err++;
					break;
				}
				for (; isspace(*token); token++);
				cpy_token(&(itu_cur->itu_rmodepath), token,
								&parse_err);
				/*
				 * The pathname should end with "/".
				 * if it does not have one add it.
				 */
				cp = strrchr(itu_cur->itu_rmodepath, 0);
				if (*(cp -1) != SLASH) {
					int newsz;

					newsz = strlen(itu_cur->itu_rmodepath)
								+ 2;
					itu_cur->itu_rmodepath =
						realloc(itu_cur->itu_rmodepath,
							newsz);
					if (itu_cur->itu_rmodepath == NULL) {
						parse_err++;
						break;
					}
					cp = strrchr(itu_cur->itu_rmodepath,
									0);
					*cp++ = SLASH; *cp = 0;
				}
				break;

			case MAP:
				/*
				 * Copy the entry line.
				 */
				token = get_quoted_tok(lineno, "map", "MAP",
						&parse_err);

				if (token) {
					ZALLOC(ep, 1, entries_t);
					cpy_token(&(ep->en_data), token,
								&parse_err);
					insert_entry(&(itu_cur->itu_maps), ep);
				}
				break;

			case LEGACY_DEV:
				token = (char *)strtok(NULL, WHITESP_CR_SEP);
				if (check_token(token, "legacy_device"))
					parse_err++;
				else if (stricmp(token, "TRUE") == 0)
					itu_cur->itu_legacy_dev = I_TRUE;
				break;

			case LOAD_ALWAYS:
				token = (char *)strtok(NULL, WHITESP_CR_SEP);
				if (check_token(token, "load_always"))
					parse_err++;
				else if (stricmp(token, "TRUE") == 0)
					itu_cur->itu_load_always = I_TRUE;
				break;

			case INSTALL_ALWAYS:
				token = (char *)strtok(NULL, WHITESP_CR_SEP);
				if (check_token(token, "install_always"))
					parse_err++;
				else if (stricmp(token, "TRUE") == 0)
					itu_cur->itu_install_always = I_TRUE;
				break;

			case DRV_PATH:
				token = strtok(NULL, WHITESP_CR_SEP);
				if (check_token(token, "driver_path")) {
					parse_err++;
					break;
				}
				cpy_token(&(itu_cur->itu_drvpath),
							token, &parse_err);
				/*
				 * The pathname should end with "/".
				 * if it does not have one add it.
				 */
				cp = strrchr(itu_cur->itu_drvpath, 0);
				if (*(cp -1) != SLASH) {
					int newsz;

					newsz = strlen(itu_cur->itu_drvpath)
							+ 2;
					itu_cur->itu_drvpath =
					realloc(itu_cur->itu_drvpath, newsz);
					if (itu_cur->itu_drvpath == NULL) {
						parse_err++;
						break;
					}
					cp = strrchr(itu_cur->itu_drvpath, 0);
					*cp++ = SLASH; *cp = 0;
				}
				break;

				/*
				 * Ignore these keywords
				 */

			case DRV_PKG:
			case BEF_PKG:
			case MAN_PKG:
			case PATCH_ID:
			case PATCH_REQUIRED:
			case PATCH_OBSOLETE:
				token = (char *)strtok(NULL, WHITESP_CR_SEP);
				token = (char *)strtok(NULL, WHITESP_CR_SEP);
				debug(D_ITU, "skipping %s token %s\n",
					keyval[val].key_name, token);
				break;


			case DRV_PKG_DESC:
			case DRV_PKG_VERS:
			case BEF_PKG_DESC:
			case BEF_PKG_VERS:
			case MAN_PKG_DESC:
			case MAN_PKG_VERS:
				token = get_quoted_tok(lineno,
						keyval[val].key_name,
						"*PKG_VERS/*PKG_DESC",
							&parse_err);

				debug(D_ITU, "skipping %s token %s\n",
					keyval[val].key_name, token);
				break;

			default:
				sprintf(errmsg, gettext("line %d: "
					"unknown keyword\n\"%s\" len %d\n"),
					lineno, token, (val = strlen(token)));
				enter_menu(0, "ITU_PARSE_ERROR", errmsg);
				/*
				 * Let's allow unrecognized lines
				 * with warnings.
				 */
				for (i = 0; i < val; i++, token++)
					debug(D_ITU, "0x%x ", *token);
				debug(D_ITU, "\n-- end\n");

			}

			token = strtok(NULL, CR_ASSIGN_SEP);
			if (token && token_is_whitespace(token))
				token = strtok(NULL, CR_ASSIGN_SEP);
		} while (token);
	} /* matches fgets() */

	/*
	 * Do error checking and fill in the gaps.
	 */

	if (parse_err)
		goto err_exit;

	if (itu_cur->itu_type == UNDEFINED) {
		sprintf(errmsg, gettext("itu_type not defined\n"));
		enter_menu(0, "ITU_PARSE_ERROR", errmsg);
		parse_err++;
		goto err_exit;
	}

	/*
	 * Done parsing the file. Checking the dependencies.
	 * If its an update we might still need to check fields. But this
	 * we will defer to a later time.
	 */

	if (itu_cur->itu_drv_name == NULL) {
		sprintf(errmsg, gettext("Driver Name not found\n"));
		enter_menu(0, "ITU_PARSE_ERROR", errmsg);
		parse_err++;
		goto err_exit;
	}

	if (brace_lvl) {
	    sprintf(errmsg, gettext("unmatched open brace at EOF!\n"));
	    enter_menu(0, "ITU_PARSE_ERROR", errmsg);
	    parse_err++;
	    goto err_exit;
	}

	/*
	 * Since we might need driver path we set it here
	 */
	if (itu_cur->itu_drvpath == NULL) {
		if ((itu_cur->itu_drvpath = strdup(DEFAULT_KDRV_DIR)) == NULL)
			MemFailure();
	}
	/*
	 * When we come here we have 2 cases for the befs
	 * 1. We have statd a bef
	 * 2. we have not statd bef but bef_name is set.
	 */

	if (itu_cur->itu_type == PARTIAL) {
		if ((bef && !itu_cur->itu_rmodepath) ||
		    (itu_cur->itu_rmodepath && !bef))  {
			if (bef) {
				/*
				 * Update and bef exists rmodepath
				 * has to exist.
				 */
				sprintf(errmsg,
					gettext("realmode_path has to be set "
					"for a bef update. bef: %s\n"), bef);
				enter_menu(0, "ITU_PARSE_ERROR", errmsg);
			} else {
				/*
				 * Update and rmodepath is set,
				 * but no bef statd.
				 */
				sprintf(errmsg,
					gettext("realmode_path set but "
					"no realmode driver(bef) found\n"));
				enter_menu(0, "ITU_PARSE_ERROR", errmsg);
			}
			parse_err++;
			goto err_exit;
		}
	} else {
		if (!bef && !itu_cur->itu_bef_name) {
			sprintf(errmsg,
			gettext("No .bef or bef_name in itu file\n"));
			enter_menu(0, "ITU_PARSE_ERROR", errmsg);
			parse_err++;
			goto err_exit;
		}
	}

	/*
	 *  Allocate space for the bef.
	 */
	if (itu_cur->itu_bef_name == NULL && bef) {
		ZALLOC(itu_cur->itu_bef_name, strlen(bef)+1, char);
		strcpy(itu_cur->itu_bef_name, bef);
	}

	if (db_list == NULL) {
		/*
		 * If db_list is NULL, db_common has to be fully populated.
		 * First check if that is the case. If that passes then
		 * setting itu_dbes to NULL can be based on checking on
		 * db_dev_id.
		 */
		parse_err = check_dbes(db_common, itu_cur->itu_type);
		if (!parse_err)
		    itu_cur->itu_dbes = (db_common->db_dev_id != NULL) ?
							db_common : NULL;
	} else {
		/*
		 * Fill in the common entry for every db_e
		 */

		FILL_COMMON_DBE("dev_id", db_dev_id, parse_err);
		FILL_COMMON_DBE("node_name", db_node_name, parse_err);
		FILL_COMMON_DBE("dev_type", db_dev_type, parse_err);
		FILL_COMMON_DBE("bus_type", db_bus_type, parse_err);
		FILL_COMMON_DBE("dev_desc", db_dev_desc, parse_err);
		itu_cur->itu_dbes = db_list;
		parse_err = check_dbes(itu_cur->itu_dbes, itu_cur->itu_type);
	}

	/*
	 * conditions for which bef_name is required.
	 * if legacy or itu_dbes != NULL or rmodepath is set
	 */

	if (!parse_err && ((itu_cur->itu_dbes != NULL) ||
	    (itu_cur->itu_rmodepath != NULL)) &&
	    (itu_cur->itu_bef_name == NULL)) {
		parse_err++;
		sprintf(errmsg, gettext("Could not find realmode driver(bef) "
			"or bef_name is missing\n"));
		enter_menu(0, "ITU_PARSE_ERROR", errmsg);
	}

err_exit:
	return (parse_err);
}

char *
get_quoted_tok(int lineno,  char *key_name, char *tok_name, int *errflg)
{
	char *token;

	token = (char *)strtok(NULL, QUOTE_SEP);

	debug(D_ITU, "%s: %s\n", tok_name, (token) ? token : "NIL");
	if (check_token(token, key_name)) {
		++*errflg;
		token = NULL;
	} else {
		/* remove leading spaces */
		if (isspace(*token))
		token = (char *)strtok(NULL, QUOTE_SEP);
		debug(D_ITU, "%s: %s\n", tok_name, (token) ? token : "NIL");
		if (check_token(token, key_name)) {
			++*errflg;
			token = NULL;
		}
	}
	if (token == NULL)
		debug(D_ERR, "line %d: Missing quotes ?\n", lineno);

	return (token);
}

/*
 * cpy_token: copy the token to *name
 */
void
cpy_token(char **name, char *token, int *errflg)
{
	char *tname;
	int len;

	len = strlen(token) + 1;
	if (tname = (char *)malloc(len)) {
		strncpy(tname, token, len);
		*name = tname;
	} else
	    ++*errflg;
}

/*
 * Add an entry to the database.
 * It parse the entry and adds it either to the commondb list or
 * to the individual db_list.
 * Entry:
 * Exit: global line entry has stripped of of one more element.
 * Return: a token if more keywords exist.
 */

char *
add_db_entry(int keyword, char **curdb_elm, char **commondb_elm,
				int *brace_lvl, char *sep, int *errflg)
{
	char *cp, *name;
	char *token, *newtok;
	char **db_elm;

	name = keyval[keyword].key_name;

	token = (char *)strtok(NULL, sep);
	if (token == NULL || (strchr(token, '\n'))) {
		++*errflg;
		return (NULL);
	}
	/*
	 * This check has been added to remove leading white space
	 * for quoted strings.
	 * Ex. describe_dev = " some thing"
	 * In this case leading white spaces cannot be easily removed since
	 * the separator is ".
	 */

	if (isspace(*token))
		token = (char *)strtok(NULL, sep);

	if (token == NULL || (strchr(token, '\n'))) {
		++*errflg;
		return (NULL);
	}
	cp = token;

	while (isspace(*cp)) /* leading skip white space */
		cp++;
	token = cp;
	/*
	 * if close brace found copy the token
	 */

	cp = strchr(token, CLOSE_BRACE);
	debug(D_ITU, "%s: %s brace_found: %s\n", name, token,
		(cp)?"TRUE":"FALSE");

	if (*brace_lvl) {
		if (cp)
			*cp = '\0';
	} else {
		if (cp) {
		    ++*errflg;
		    sprintf(errmsg, gettext("unmatched close brace"));
		    enter_menu(0, "ITU_PARSE_ERROR");
		}
	}

	newtok = NULL;
	db_elm = (*brace_lvl == 0) ? commondb_elm : curdb_elm;

	if (keyword == DEV_ID && *db_elm != NULL) {
		ZALLOC(newtok, (strlen(*db_elm) + strlen(token) + 2), char);
		sprintf(newtok, "%s|%s", *db_elm, token);
		free(*db_elm);
	}
	newtok = (newtok == NULL) ? token : newtok;
	cpy_token(db_elm, newtok, errflg);

	if (*brace_lvl)
		debug(D_ITU, "%s->%s %s\n",
			(*brace_lvl) ? "db_cur" : "db_common", name, *db_elm);

	if (cp && *brace_lvl) {
		token = NULL;
		--*brace_lvl;
	}
	return (token);
}

/*
 *
 *
 *
 */
void
insert_entry(ent_head_t *hp, entries_t *ep)
{
	if (hp->ent_hp == NULL)
		hp->ent_hp = ep;
	else
		hp->ent_tailp->en_nextp = ep;
	hp->ent_tailp = ep;
}

/*
 * getkeyword: Checks the given token for a valid keyword. Also checks for
 * valid multiple instances of the keyword.
 */
int
getkeyword(char *token, int *errflg)
{
	int  i;
	char *newtok, *cp, *dp;
	int retval = -1;

	/*
	 * Remove any spaces that in the token
	 */
	if ((newtok = malloc(strlen(token) + 1)) == NULL) {
		MemFailure();
	}
	for (cp = newtok, dp = token; *dp; dp++) {
		if (!isspace(*dp)) {
			*cp = *dp;
			cp++;
		}
	}
	*cp = '\0'; /* Null terminate the new string */

	for (i = 0; i < KEYWORDS; i++) {
		if ((strcmp(newtok, keyval[i].key_name)) == 0) {
			if ((keyval[i].multi_inst == I_TRUE) ||
			    (keyval[i].multi_inst == I_FALSE &&
			    !keyval[i].instances)) {
				keyval[i].instances++;
				retval = keyval[i].val;
			} else {
				sprintf(errmsg,
					gettext("cannot have multiple "
					"\"%s\" in itu file\n"),
					keyval[i].key_name);
				enter_menu(0, "ITU_PARSE_ERROR", errmsg);
				++*errflg;
				retval = keyval[i].val;
			}
		}
	}

	free(newtok);
	return (retval);
}
/*
 * Reset the number of instances when parsing a .itu file. This is called
 * when the entire file is parsed.
 * Entry: some of keyval[i].instances are set.
 * Exit: All to zero.
 */
void
reset_keyval_instances()
{
	int i;

	for (i = 0; i < KEYWORDS; i++)
		keyval[i].instances = 0;
}

/*
 * Free routines
 */
void
free_entries(ent_head_t *ehp)
{
	entries_t *ep, *enp;

	ep = ehp->ent_hp;
	ehp->ent_hp = NULL;
	while (ep) {
	    enp = ep->en_nextp;
	    free(ep->en_data);
	    free(ep);
	    ep = enp;
	}
}

#ifdef obsolete
void
free_drvlist()
{
	struct drvlist *drvp, *ndrvp;

	for (drvp = drvhp; drvp; drvp = ndrvp) {
		ndrvp = drvp->np;
		free(drvp);
	}
}
#endif

void
free_itu(itu_config_t *itup)
{
	char fname[PATH_MAX];

	/*
	 * delete all the files associate with this itu
	 */
	if (itup->itu_bef_statd == I_TRUE) {
		sprintf(fname, "%s%s", itup->itu_pkgpath, itup->itu_bef_name);
		unlink(fname);
	}
	if (itup->itu_kdrv_statd == I_TRUE) {
		sprintf(fname, "%s%s", itup->itu_pkgpath, itup->itu_drv_name);
		unlink(fname);
	}
	if (itup->itu_cnf_statd == I_TRUE) {
		sprintf(fname, "%s%s.cnf", itup->itu_pkgpath,
			itup->itu_drv_name);
		unlink(fname);
	}

	if (itup->itu_pkgpath)
		free(itup->itu_pkgpath);
	if (itup->itu_drv_name)
		free(itup->itu_drv_name);
	if (itup->itu_bef_name)
		free(itup->itu_bef_name);
	if (itup->itu_rmodepath)
		free(itup->itu_rmodepath);

	free_entries(&(itup->itu_sysents));
	free_entries(&(itup->itu_machents));
	free_entries(&(itup->itu_devlink));
	free_entries(&(itup->itu_aliases));
	free_entries(&(itup->itu_classes));
	free_entries(&(itup->itu_maps));
	free_db(itup->itu_dbes);
	itup->itu_dbes = NULL;
}

/*
 * release_itu - release most resources associated with a processed
 * itu. Leave the names, which will allow checking for a more
 * recent object, in the unlikely event additional diskettes are
 * inserted.
 */
void
release_itu(itu_config_t *itup)
{
	free_entries(&(itup->itu_sysents));
	free_entries(&(itup->itu_machents));
	free_entries(&(itup->itu_devlink));
	free_entries(&(itup->itu_aliases));
	free_entries(&(itup->itu_maps));
	free_db(itup->itu_dbes);
	itup->itu_dbes = NULL;
}

void
free_db(dbe_t *dbp)
{
	dbe_t *dbnp, *dbpp;

	dbnp = dbp;
	while (dbnp) {
		if (dbnp->db_dev_id)
			free(dbnp->db_dev_id);
		if (dbnp->db_node_name)
			free(dbnp->db_node_name);
		if (dbnp->db_dev_type)
			free(dbnp->db_dev_type);
		if (dbnp->db_bus_type)
			free(dbnp->db_bus_type);
		if (dbnp->db_dev_desc)
			free(dbnp->db_dev_desc);
		dbpp = dbnp;
		dbnp = dbnp->db_nextp;
		free(dbpp);
	}
}

/*
 * Exit: itu_tail = NULL, itu_config_head = NULL
 */
void
free_all_ituconfigs()
{
	itu_config_t *itu_cp, *itu_pp;

	itu_cp = itu_config_head;
	while (itu_cp) {
		free_itu(itu_cp);
		itu_pp = itu_cp;
		itu_cp = itu_cp->itu_nextp;
		free(itu_pp);
	}
	itu_config_head = itu_tail = NULL;
}

/*
 * conv_num_to_dotnum: Helper routine
 *   convert a name of type 251 to 2.5.1
 */
void
conv_num_to_dotnum(char *from, char *to)
{	char *np;
	char *cp;

	cp = strchr(from, '_');
	np = to;
	while (*cp != 0) {
		if (isdigit(*cp)) {
			*np++ = *cp;
			*np++ = '.';
		}
		if (*cp == SLASH)
			break;
		cp++;
	}
	*(--np) = 0;
}

/*
 * conv_dotnum_to_num: Helper routine.
 *  convert string like 2.5.1 to 251.
 */
void
conv_dotnum_to_num(char *from, char *to)
{
	char *cp, *cp1;

	cp = from;
	cp1 = to;
	while (isspace(*cp)) cp++;
	while (*cp != 0) {
		if (isdigit(*cp))
			*cp1++ = *cp;
		cp++;
	}
	*cp1 = 0;
}

/*
 * Check if volume name is repeated.
 * Exit: Return 1 if read or on failure. 0 if not.
 */
int
have_read_this_floppy()
{
	DIR *dp;
	int retval = 0;
	entries_t *ep, *np;
	char *tvol;

	/*
	 * do an opendir to force a floppy read.
	 */
	if ((dp = opendir(VOL_A)) != NULL) {
		closedir(dp);
	} else {
		sprintf(errmsg, gettext("Opendir of %s failed\n"), VOL_A);
		enter_menu(0, "DU_FDOPENDIR_ERROR", errmsg);
		return (++retval);
	}

	tvol = get_volname();
	if (volist.ent_hp == NULL) {
		ZALLOC(ep, 1, entries_t);
		ep->en_data = strdup(tvol);
		volist.ent_hp = volist.ent_tailp = ep;
	} else {
		/*
		 * Check if we are reading the same one.
		 */
		np = volist.ent_tailp;
		if (strcmp(np->en_data, tvol) == 0) {
			enter_menu(0, "FLOPPY_READ_LAST", tvol);
			retval = 1;
		} else {
			int match;
			int cnt;

			match = 0;
			cnt = 0;
			ep = volist.ent_hp;
			while (!match && (ep != np)) {
				if (strcmp(ep->en_data, tvol) == 0) {
					match++;
				}
				ep = ep->en_nextp;
				cnt++;
			}
			if (!match) {
				ZALLOC(ep, 1, entries_t);
				ep->en_data = strdup(tvol);
				np->en_nextp = ep;
				volist.ent_tailp = ep;
			} else {
				enter_menu(0, "FLOPPY_READ_BEFORE", tvol, cnt);
				retval = 1;
			}
		}
	}
	return (retval);
}

/*
 * Add entries to the master file.
 */
void
add_db_entries(itu_config_t *itup)
{
	char *db_entry = linebuf;
	dbe_t *dbp;

	/* allow legal maximum lines */
	if (!db_entry) {
		db_entry = linebuf = malloc(MASTER_LINE_MAX);
		if (!db_entry)
			MemFailure();
	}

	for (dbp = itup->itu_dbes; dbp; dbp = dbp->db_nextp) {
		sprintf(db_entry, "%s %s %s %s %s \"%s\"\n",
			dbp->db_dev_id, dbp->db_node_name,
			dbp->db_dev_type, dbp->db_bus_type,
			itup->itu_bef_name, dbp->db_dev_desc);
		master_file_update_devdb(db_entry);
	}
}

/*
 * Create in the RAM File these kernel files.
 */
void
create_kernel_files()
{
	FILE *fp;
	int err = 0;

	if (fp = fopen(R_DRV_ALIASES, "a+"))
		fclose(fp);
	else {
		sprintf(errmsg, gettext(R_DRV_ALIASES ": fopen failed\n"));
		enter_menu(0, "DU_RAMFILE_ERROR", errmsg);
		err++;
	}
	if (!err && (fp = fopen(R_DRV_CLASSES, "a+")))
		fclose(fp);
	else {
		sprintf(errmsg, gettext(R_DRV_CLASSES ": fopen failed\n"));
		enter_menu(0, "DU_RAMFILE_ERROR", errmsg);
		err++;
	}
	if (!err && (fp = fopen(R_SYSTEM, "a+")))
		fclose(fp);
	else {
		sprintf(errmsg, gettext(R_SYSTEM ": fopen failed\n"));
		enter_menu(0, "DU_RAMFILE_ERROR", errmsg);
		err++;
	}
	if (!err && (fp = fopen(R_DEVLINK_TAB, "a+")))
		fclose(fp);
	else {
		sprintf(errmsg, gettext(R_DEVLINK_TAB ": fopen failed\n"));
		enter_menu(0, "DU_RAMFILE_ERROR", errmsg);
		err++;
	}
	if (!err && (fp = fopen(R_MACH, "a+")))
		fclose(fp);
	else {
		sprintf(errmsg, gettext(R_MACH ": fopen failed\n"));
		enter_menu(0, "DU_RAMFILE_ERROR", errmsg);
		err++;
	}
	if (fp = fopen(R_EDITFILE, "a+"))
		fclose(fp);
	else {
		sprintf(errmsg, gettext(R_EDITFILE ": fopen failed\n"));
		enter_menu(0, "DU_RAMFILE_ERROR", errmsg);
		err++;
	}
	/*
	 * Even though we never add anything to this file,
	 * we open it here to reserve space in cache.
	 */

	if (!err && (fp = fopen(R_NAME_TO_MAJ, "a+")))
		fclose(fp);
	else {
		sprintf(errmsg, gettext(R_NAME_TO_MAJ ": fopen failed\n"));
		enter_menu(0, "DU_RAMFILE_ERROR", errmsg);
		err++;
	}
	if (err)
		MemFailure();
}

void
output_maps()
{
	itu_config_t *itup;
	entries_t *ep;
	char localbuf[PATH_MAX];

	itup = itu_config_head;
	while (itup) {
		for (ep = itup->itu_maps.ent_hp; ep; ep = ep->en_nextp) {
			sprintf(localbuf, "map %s\n", ep->en_data);
			out_bop(localbuf);
		}
		itup = itup->itu_nextp;
	}
}

/*
 * 1. Add entries to system file
 * 2. Add entries to the alias file.
 * 3. Add entries to the devlink.tab.
 * 4. If classes exist add entries to the classes file.
 * 5. If master file entries exist, create them.
 * The master file entries are created as properties to Phase 3 (if needed)
 *
 * Create properties at itu-prop for drivers, num of master file entries
 * and the entries themselves. Note. The trailing ":" in volume name is
 * stripped out out.
 */

void
output_drvinfo(void)
{
	itu_config_t *itup, *itusp;
	dbe_t *dbp;
	entries_t *ep;
	FILE *fpsys, *fpdlink, *fpalias, *fpclass, *fpmach, *fpedits;
	char prop_name[20];
	char localbuf[PATH_MAX*2], *cp;
	int err, i;
	char *tn;

	/*
	 * XXX: workaround: keep a count of ITU diskettes processed
	 * to allow later stages to determine if an ITU rather than
	 * DCB installation is occurring.
	 */
	if (volist.ent_hp) {
		i = 0;
		sprintf(prop_name, "itu-diskettes");
		for (ep = volist.ent_hp; ep; ep = ep->en_nextp)
			i++;
		sprintf(localbuf, "%d", i);
		write_prop(prop_name, ITU_PROPS, localbuf, ":", OVERWRITE_PROP);
	}

	/* quick return if nothing to do */
	if (itu_config_head == NULL)
		return;
	/*
	 * Process each ITU at a time. If we fail, we discard the
	 * rest and print to that effect.
	 */

	/*
	 * Add system file entries
	 */
	fpsys = fopen(U_SYSTEM, "a+");
	if (fpsys == NULL) {
		sprintf(errmsg,
		gettext("output_drvinfo: Open of %s failed\n"),
			U_SYSTEM);
		enter_menu(0, "DU_RAMFILE_ERROR", errmsg);
	}

	fpdlink = fopen(U_DEVLINK_TAB, "a+");
	if (fpdlink == NULL) {
		sprintf(errmsg,
		gettext("output_drvinfo: Open of %s failed\n"),
			U_DEVLINK_TAB);
		enter_menu(0, "DU_RAMFILE_ERROR", errmsg);
	}

	fpalias = fopen(U_DRV_ALIASES, "a+");
	if (fpalias == NULL) {
		sprintf(errmsg,
			gettext("output_drvinfo: Open of %s failed\n"),
				U_DRV_ALIASES);
		enter_menu(0, "DU_RAMFILE_ERROR", errmsg);
	}

	fpclass = fopen(U_DRV_CLASSES, "a+");
	if (fpclass == NULL) {
		sprintf(errmsg,
			gettext("output_drvinfo: Open of %s failed\n"),
			U_DRV_CLASSES);
		enter_menu(0, "DU_RAMFILE_ERROR", errmsg);
	}

	fpmach = fopen(U_MACH, "a+");
	if (fpmach == NULL) {
		sprintf(errmsg,
			gettext("output_drvinfo: Open of %s failed\n"),
			U_MACH);
		enter_menu(0, "DU_RAMFILE_ERROR", errmsg);
	}

	fpedits = fopen(U_EDITFILE, "w");
	if (fpedits == NULL) {
		sprintf(errmsg,
			gettext("output_drvinfo: Open of %s failed\n"),
			U_EDITFILE);
		enter_menu(0, "DU_RAMFILE_ERROR", errmsg);
	}

	if (!(fpsys && fpdlink && fpalias && fpclass && fpmach && fpedits)) {
		sprintf(errmsg, gettext("output_drvinfo_and_maps:"
				" Open of Ramfiles failed\n"));
		enter_menu(0, "DU_RAMFILE_ERROR", errmsg);
		MemFailure();
	}

	err = 0;
	for (itup = itu_config_head; itup && !err; itup = itup->itu_nextp) {
		/*
		 * Only check legacy devices since selfidentifying devices
		 * have already been pruned.
		 */
		if (itup->itu_legacy_dev == I_TRUE &&
						!device_exists(itup))
			continue;
		debug(D_FLOW, "output_drvinfo_and_maps: %s\n",
			itup->itu_drv_name);

		for (ep = itup->itu_sysents.ent_hp; ep && !err;
						ep = ep->en_nextp) {
			if (fprintf(fpsys, "%s\n", ep->en_data) < 0)
				    err++;
		}

		for (ep = itup->itu_machents.ent_hp; ep && !err;
						ep = ep->en_nextp) {
			if (fprintf(fpmach, "%s\n", ep->en_data) < 0)
				    err++;
		}

		for (ep = itup->itu_edits.ent_hp; ep && !err;
						ep = ep->en_nextp) {
			if (fprintf(fpedits, "%s\n", ep->en_data) < 0)
				    err++;
		}

		for (ep = itup->itu_devlink.ent_hp; ep && !err;
						ep = ep->en_nextp) {
			if (fprintf(fpdlink, "%s\n", ep->en_data) < 0)
				    err++;
		}

		for (ep = itup->itu_aliases.ent_hp; ep && !err;
						ep = ep->en_nextp) {
			if (fprintf(fpalias, "%s %s\n", itup->itu_drv_name,
				    ep->en_data) < 0)
				    err++;
		}

		for (ep = itup->itu_classes.ent_hp; ep && !err;
						ep = ep->en_nextp) {
			if (fprintf(fpclass, "%s %s\n", itup->itu_drv_name,
				    ep->en_data) < 0)
				    err++;
		}

		/* Master file entries */

		for (dbp = itup->itu_dbes, i = 0; dbp && !err;
					dbp = dbp->db_nextp, i++) {
			sprintf(prop_name, "%s-%d", itup->itu_drv_name, i);
			sprintf(localbuf, "'%s %s %s %s %s \"%s\"'",
					dbp->db_dev_id, dbp->db_node_name,
					dbp->db_dev_type, dbp->db_bus_type,
					itup->itu_bef_name, dbp->db_dev_desc);
			write_prop(prop_name, ITU_PROPS, localbuf, ":",
							OVERWRITE_PROP);
		}

		/*  Write out the master number of entries. */

		sprintf(prop_name, "%s-count", itup->itu_drv_name);
		sprintf(localbuf, "%d", i);
		write_prop(prop_name, ITU_PROPS, localbuf, ":", OVERWRITE_PROP);

		/*
		 * List of drivers. Needed for phase 2. Strip out ":"
		 * from the volume name. Add last component of path
		 * to allow itup2.exe to properly allocate major.
		 */

		/*
		 * Workaround: find the last component of module name.
		 * Note that a terminating slash has been arranged above.
		 */
		if (itup->itu_drvind == 0)
		    for (tn = itup->itu_drvpath; tn && *tn; tn++) {
			/* save index of last non-terminating slash */
			if (*tn == SLASH && *(tn+1) && *(tn+1) != '\n')
				itup->itu_drvind =
					tn - itup->itu_drvpath + 1;
		    }
		cp = strchr(itup->itu_volname, ':'); *cp = 0;
		sprintf(localbuf, "%s,%s%s", itup->itu_volname,
			&itup->itu_drvpath[itup->itu_drvind],
			itup->itu_drv_name);
		write_prop("drivers", ITU_PROPS, localbuf, ",", UPDATE_PROP);
	}

	if (err) {
		sprintf(errmsg, gettext("WARNING: Only the following drivers"
			    "are installed\n"));
		itusp = itu_config_head;
		while (itusp != itup)
			strcat(strcat(errmsg, itusp->itu_drv_name), " ");
		enter_menu(0, "DU_LIMIT_DRVRS", errmsg);
	}

	fclose(fpsys);
	fclose(fpmach);
	fclose(fpdlink);
	fclose(fpalias);
	fclose(fpclass);
	fclose(fpedits);

	/* release resources which are no longer needed */
	for (itup = itu_config_head; itup; itup = itusp) {
		itusp = itup->itu_nextp;
		release_itu(itup);
	}
	if (linebuf) {
		free(linebuf);
		linebuf = NULL;
	}
}

char *
get_volname()
{
	union _REGS inregs, outregs;
	struct _SREGS segregs;
	char far *fp;

	static char volname[30];

	fp = (char far *)volname;
	_segread(&segregs);
	inregs.h.bl = 1;	/* Drive A:. For default drive use 0 */
	inregs.h.ah = 0x69;	/* DOS Funtion 0x69 get volume name */
	inregs.h.al = 0x0;	/* Read volume name */
	inregs.x.dx = _FP_OFF(fp);
	segregs.ds = _FP_SEG(fp);


	(void) _int86x(0x21, &inregs, &outregs, &segregs);
	if (outregs.x.cflag) {
		enter_menu(0, "VOLNAME_ERROR", 0);
		return (NULL);
	} else {
		int i;

		for (i = 6; !isspace(volname[i]) && i < 17; i++);
		volname[i] = ':';
		volname[i+1] = 0;
		return (&volname[6]);
	}
}

/*
 * cache_file: Reads the file into memory. If it fails, then the
 * volume name for the floppy disk is returned.
 * Exit:  Return 1and volname = " " (single space)
 *	   Return 0 and volname is set.
 */
int
cache_file(char *name, char **volname)
{
	int cfd;
	char cname[PATH_MAX];
	int retval;
	/*
	 * Replace U: with R:.  We are assured that the name has U:/...
	 * so just go beyond that
	 */

	sprintf(cname, "R:%s", name+2);
	debug(D_FLOW, "cache_file: Name is %s\n", cname);
	/*
	 * This is a "special open" since it has R:. It caches the file by
	 * opening it as per the ITU design spec.
	 */

#ifdef FORCE_VOL
	*volname = get_volname();
	retval = 0;
#else
	if ((cfd = _open(cname, _O_RDONLY)) > -1) {
		*volname = " ";
		_close(cfd);
		retval = 1;
	} else {
		*volname = get_volname();
		retval = 0;
	}
#endif /* FORCE_VOL */
	debug(D_FLOW, "volume name %s\n", *volname);
	return (retval);
}

/*
 * Create a node if it does not exist and writes the property.
 */
void
write_prop(char *prop, char *where, char *val, char *sep, int flag)
{
	char ibuf[PATH_MAX*2];
	char *prp;

	/*
	 * Check if the node exists. If not create it.
	 */

	sprintf(ibuf, "dev /%s\n", where);
	out_bop(ibuf);
	ibuf[0] = 0;
	in_bop(ibuf, PATH_MAX);
	if (ibuf[0] != 0) {
		sprintf(ibuf, "mknod /%s\n", where);
		out_bop(ibuf);
	}

	if (((prp = read_prop(prop, where)) == NULL) ||
				    flag == OVERWRITE_PROP) {
		sprintf(ibuf, "dev /%s\n", where);
		out_bop(ibuf);
		sprintf(ibuf, "setprop %s %s\n", prop, val);
	} else {
		sprintf(ibuf, "setprop %s %s%s%s\n", prop, prp, sep, val);
	}
	out_bop(ibuf);
}

#define	CHECK_DBE_ELM(name, db_elm)				\
{								\
	if (db_cur->db_elm == NULL && type == COMPLETE) {	\
		debug(D_ITU, name " missing\n");		\
		err++;						\
	} else if (db_cur->db_elm != NULL)			\
		num_elms++;					\
}
#define	NUM_DB_ELMS_CHECKED 5
/*
 * Check the all master file entries..EXCEPT db_rmode_drv. For type = COMPLETE
 * it prints out specific errors, for PARTIAL, it simply checks if errors exits
 *
 * Exit: Return 0 no errors or errcnt.
 */
int
check_dbes(dbe_t *dbes, itu_types_t type)
{
	dbe_t *db_cur;
	int err = 0;
	int num_elms = 0;
	int i;

	for (db_cur = dbes, i = 0; db_cur; db_cur = db_cur->db_nextp, i++) {
		CHECK_DBE_ELM("dev_id", db_dev_id);
		CHECK_DBE_ELM("node_name", db_node_name);
		CHECK_DBE_ELM("dev_type", db_dev_type);
		CHECK_DBE_ELM("bus_type", db_bus_type);
		CHECK_DBE_ELM("dev_desc", db_dev_desc);
	}
	if (type == PARTIAL && num_elms &&
					num_elms != (NUM_DB_ELMS_CHECKED*i)) {
		debug(D_ITU, "Device not completely described."
			"  Set \"itu_type = complete\" and run again to"
			" see the errors.\n");
		err = num_elms/i;
	}
	return (err);
}

/*
 * The following drivers are grandfathered:
 * eha, aha, pe, sbpro, asy.
 * XXX: Apparently this means there isn't a reliable
 * correspondence between unix and realmode drivers.
 */

#define	GRANDFATHERED()\
((stricmp(name, "eha") == 0) || (stricmp(name, "aha") == 0) ||\
	(stricmp(name, "pe") == 0) || (stricmp(name, "sbpro") == 0) ||\
	stricmp(name, "asy") == 0)

/*ARGSUSED*/
int
scan_headlists(Board *Head, itu_config_t *itup, char *headname)
{
	Board *bp;
	devtrans *dtp;
	char *name, *cp, *p;
	int found;
	extern Serial_ports_found;

	name = itup->itu_drv_name;

	/*
	 * The strategy to load an ITUO depends on whether the ITU
	 * Object is an "update" or "new". For updates, the match is based
	 * on bef name (Bef name is same as driver name) but for new ITUOs
	 * the match is based on devids
	 */

	found = 0;
	for (bp = Head; bp && !found; bp = bp->link) {

		dtp = bp->dbentryp;

		if (itup->itu_type == PARTIAL && dtp != NULL) {
			if (!GRANDFATHERED() && strcmp(name, "none") != 0) {
				if (strcmp(name, dtp->real_driver) == 0)
				    found++;
			} else {
				/*
				 * the grandfathered exception -
				 * eha, aha, pe, sbpro, asy and case where
				 * befname is "none"
				 */
				if (strcmp(name, dtp->unix_driver) == 0)
					found++;
				/*
				 * XXX: special case mwss !! Ugh!!
				 * mwss: node is mwss and has a
				 * mwss.bef but the kernel driver is
				 * sbpro!!
				 */
				if ((!found && (strcmp(name, "sbpro") == 0)) &&
				    (strcmp(dtp->real_driver, "mwss") == 0))
					found++;

				/*
				 * XXX: special case asy.
				 * Apparently this is where exceptional
				 * matches between drivers are made.
				 * com.bef is apparently not run until
				 * after this stage, so we can only
				 * look at a special variable.
				 */
				if ((!found && (strcmp(name, "asy") == 0)) &&
				    Serial_ports_found)
					found++;
			}
		}
		if (!found) {
			dbe_t *dbp;

			(void) GetDeviceId_devdb(bp, NULL);
			/*
			 * There could be multiple master file entries.
			 * So for each entry we have to scan through all
			 * devids if "|" exists
			 */

			for (dbp = itup->itu_dbes; dbp && !found;
						dbp = dbp->db_nextp) {
				if (strchr(dbp->db_dev_id, '|')) {
					p = name = strdup(dbp->db_dev_id);
					if (name == NULL)
						MemFailure();
					do {
					    if (cp = strchr(name, '|'))
						*cp++ = 0;

					    if (devid_found(bp, name))
						found++;

					    name = cp;
					} while (name);
					free(p);
				} else {
					found = devid_found(bp, dbp->db_dev_id);
				}
			}
		}
	}
	return (found);
}

int
devid_found(Board *bp, char *itu_dev_id)
{
	char devid[32];

	(void) GetDeviceId_devdb(bp, devid);

	if (stricmp(devid, itu_dev_id) == 0)
		return (1);
	/*
	 * Bug fix 4042854 - If the pci function which has
	 * a sub vendor id isn't mapped in the database,
	 * see if the vendor/device is...
	 */
	if (bp->bustype == RES_BUS_PCI) {
		if (bp->pci_subvenid) {
			sprintf(devid, "pci%x,%x", bp->pci_venid,
				bp->pci_devid);
			if (stricmp(itu_dev_id, devid) == 0)
				return (1);
		}
	}
	if (is_pciclass(bp, itu_dev_id))
		return (1);

	return (0);
}

/*
 * Check if any device identified on the system matches this itu
 * eha is grandfathered. The check is based on the bef name or devid.
 *
 */
int
device_exists(itu_config_t *itup)
{
	int found;

	/*
	 * First scan Head_board. If not found, scan Head_prog
	 * since devices which dont have their resources allocated
	 * are on this list.
	 */

	found = scan_headlists(Head_board, itup, "Head_board");
	if (!found)
		found = scan_headlists(Head_prog, itup, "Head_prog");

	return (found);

}

static struct menu_options du_results[] = {
	{ FKEY(2), MA_RETURN, "Continue"},
};

#define	NDU_RESULTS (sizeof (du_results) / sizeof (*du_results))
/*
 * List all the ITUOs that have been loaded
 */
void
menu_list_ituos()
{
	struct menu_list *mlp, *du_listp;
	itu_config_t *itup;
	int j, nituos;
	char *cp;

	for (j = 0, itup = itu_config_head; itup; j++, itup = itup->itu_nextp);
	du_listp = (struct menu_list *)calloc(j, sizeof (struct menu_list));
	if (du_listp == 0)
		MemFailure();

	nituos = j;
	mlp = du_listp;
	for (itup = itu_config_head; itup; itup = itup->itu_nextp) {
		mlp->datum = (void *) itup;
		ZALLOC(cp, 200, char);
		if (itup->itu_legacy_dev == TRUE)
		    sprintf(cp, "%s - Device Driver", itup->itu_drv_name);
		else
		    strcpy(cp, itup->itu_drv_name);
		mlp->string = cp;
		mlp->flags |= MF_UNSELABLE;
		mlp++;
	}
	if (nituos == 0) {
		mlp->datum = (void *)itu_config_head;
		mlp->string = "none";
		mlp->flags |= MF_UNSELABLE;
		nituos++;
	}

	switch (select_menu(0, du_results, NDU_RESULTS, du_listp, nituos,
			MS_ZERO_ONE, "MENU_DU_RESULTS", nituos)) {
		case '\n':
		case FKEY(2):
			break;
	}

	/* free menu */
	for (j = 0; j < nituos; j++)
		if (du_listp && du_listp[j].string &&
		    strcmp(du_listp[j].string, "none"))
			free(du_listp[j].string);
	if (du_listp)
		free(du_listp);
}

int
is_pciclass(Board *bp, char *name)
{
	char *cp;
	long deviceid;

	if (bp->bustype == RES_BUS_PCICLASS && strstr(name, "pciclass")) {
		cp = strchr(name, ',');
		deviceid = strtol(cp, NULL, 16);
		if (deviceid == bp->pci_class)
			return (1);
	}
	return (0);
}


#ifdef obsolete
Board *
get_Boardp(itu_config_t *itup)
{
	Board *bp;
	devtrans *dtp;
	int found;
	char *name;
	/*
	 * since all we are guaranteed is that we have the driver name,
	 * we compare it bef name of the master file. The exceptions to
	 * this are eha, aha and pe. For these exceptions we compare
	 * against the nodename(unixdriver name)
	 */
	found = 0;
	name = itup->itu_drv_name;
	for (bp = Head_board; bp && !found; bp = bp->link) {
		dtp = bp->dbentryp;

		if ((strcmp(name, "eha") != 0) &&
		    (strcmp(name, "aha") != 0) &&
		    (strcmp(name, "pe") != 0)) {
			iprintf_tty("strcmp %s and real_driver %s\n", name,
				    dtp->real_driver);
			if (strcmp(name, dtp->real_driver) == 0)
			    found++;
		} else {
			/*
			 * the grandfathered exceptions to the rule that
			 * bef names match driver names
			 */
			iprintf_tty("strcmp %s and unix_driver %s\n", name,
							dtp->unix_driver);
			if (strcmp(name, dtp->unix_driver) == 0)
				found++;
		}
	}
	if (!found)
		bp = NULL;
	return (bp);
}
#endif

int
token_is_whitespace(char *cp)
{
	int ret = 1;

	for (; *cp != 0; cp++) {
		if (!isspace(*cp)) {
			ret = 0;
			break;
		}
	}
	return (ret);
}

/*
 * error handling. Checks if token is NULL.
 * Exit:  Returns 1 if null else 0.
 */
int
check_token(char *token, char *name)
{
	if (token != NULL && !strchr(token, '\n'))
	    return (0);
	debug(D_ITU, "%s  not assigned\n", name);
	return (1);
}

#ifdef obsolete
void
ckstr(char *comment, char *name)
{
	int j;
	char *tn = name;

	if (comment)
		debug(D_FLOW, "%s :::", comment);
	for (j = 1; tn && isprint(*tn); j++, tn++)
		debug(D_FLOW, "%c", *tn);
	if (tn) {
		debug(D_FLOW, "\nNon printable is @%d and is 0x%x "
			    "len %d\n", j, *tn, strlen(name));
		WAIT_FOR_ENTER();
	}
}
#endif
