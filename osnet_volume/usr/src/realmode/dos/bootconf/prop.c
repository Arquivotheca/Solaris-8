/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * prop.c -- routines to handle boot properties
 */

#ident "@(#)prop.c	1.34	98/07/22 SMI"

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dostypes.h>
#include "types.h"

#include "menu.h"
#include "boot.h"
#include "bop.h"
#include "cfname.h"
#include "debug.h"
#include "err.h"
#include "gettext.h"
#include "menu.h"
#include "prop.h"
#include "tty.h"

/*
 * input options for menu listing properties
 */
static struct menu_options Props_options[] = {
	{ FKEY(2), MA_RETURN, "Back" },
	{ FKEY(3), MA_RETURN, "Change" },
	{ FKEY(4), MA_RETURN, "Create" },
	{ FKEY(5), MA_RETURN, "Delete" },
};

#define	NPROPS_OPTIONS (sizeof (Props_options) / sizeof (*Props_options))

/*
 * input options for menu which prompts user for a new value
 */
static struct menu_options New_val_options[] = {
	{ '\n', MA_RETURN, NULL },
	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Cancel" },
};

#define	NNEW_VAL_OPTIONS (sizeof (New_val_options) / sizeof (*New_val_options))

#define	MAXPROP	20
#define	MAXVAL	100

static char *Buf = NULL;
static struct menu_list *Props_list = NULL;
static int Numprops;

/*
 * Function prototypes
 */
void build_menu_prop(char *fname);
int update_prop(FILE *fp, char *prop, char *value);

/*
 * build_menu_prop -- build a menu of properties
 */
void
build_menu_prop(char *fname)
{
	FILE *fp;
	struct _stat stbuf;
	int count;
	int i;
	char *line;
	char *head;
	char *tail;

	/* free memory left over from last time */
	if (Buf) {
		free(Buf);
		Buf = NULL;
	}

	if (Props_list) {
		free(Props_list);
		Props_list = NULL;
	}

	if (_stat(fname, &stbuf) < 0) {
		Numprops = 0;
		return;
	}

	if (stbuf.st_size == 0) {
		Numprops = 0;
		return;
	}

	fp = fopen(fname, "rb");
	ASSERT(fp);

	if ((Buf = malloc((int)stbuf.st_size + 1)) == NULL)
		MemFailure();

	if (fread(Buf, 1, (int)stbuf.st_size, fp) == 0) {
		fatal("build_menu_prop: fread on %s: %!", fname);
	}
	/* null terminate the whole mess */
	Buf[stbuf.st_size] = '\0';
	fclose(fp);

	/* go through the file line-by-line and count properties */
	line = Buf;
	count = 0;
	while (*line) {
		/* skip any leading white space */
		while (*line && isspace(*line))
			line++;

		/* check for valid setprop command */
		if ((strncmp(line, "setprop", 7) == 0) &&
		    ((line[7] == ' ') || (line[7] == '\t')))
			count++;

		/* skip to next line */
		while (*line && (*line != '\n'))
			line++;
	}

	/* we will return two lines for each property */
	Numprops = count * 2;

	if ((Props_list =
	    (struct menu_list *)malloc(sizeof (struct menu_list) * count * 2))
	    == NULL)
		MemFailure();

	/* go through file and break into menu strings */
	line = Buf;
	i = 0;
	while (*line) {
		/* skip any leading white space */
		while (*line && isspace(*line))
			line++;

		/* check for valid setprop command */
		if ((strncmp(line, "setprop", 7) == 0) &&
		    ((line[7] == ' ') || (line[7] == '\t'))) {
			/* find lhs */
			for (head = &line[7]; *head; head++)
				if ((*head != ' ') && (*head != '\t'))
					break;

			/* find end of lhs */
			for (tail = head; *tail; tail++)
				if ((*tail == ' ') || (*tail == '\t'))
					break;

			ASSERT(*tail != 0);

			/* make a string out of it and put it in menu */
			*tail++ = '\0';
			ASSERT(i < Numprops);
			Props_list[i].string = head;
			Props_list[i++].flags = 0;

			/* find rhs */
			for (head = tail; *head; head++)
				if ((*head != ' ') && (*head != '\t'))
					break;

			/* find end of rhs */
			for (tail = head; *tail; tail++)
				if ((*tail == '\n') || (*tail == '\r'))
					break;

			ASSERT(*tail != 0);

			*tail++ = '\0';
			ASSERT(i < Numprops);
			Props_list[i].string = head;
			Props_list[i++].flags = MF_UNSELABLE;

			line = tail;
		} else {
			/* skip to next line */
			while (*line && (*line != '\n'))
				line++;
		}
	}
}

/*
 * menu_prop -- display menus to boot properties
 */

void
menu_prop(void)
{
	int done;
	char nprop[MAXPROP + 1];
	char nval[MAXVAL + 1];
	struct menu_list *choice;

	build_menu_prop(Machenv_name);

	done = 0;
	while (!done) {
		nprop[0] = 0;
		nval[0] = 0;
		clear_selection_menu(Props_list, Numprops);
		switch (select_menu("MENU_HELP_PROPERTIES", Props_options,
		    NPROPS_OPTIONS, Props_list, Numprops, MS_ZERO_ONE,
		    "MENU_PROPERTIES", Numprops / 2)) {

		case FKEY(2):
			/* "done" */
			done = 1;
			break;

		case FKEY(4):
			/* new property */
			switch (input_menu("MENU_HELP_PROPERTIES",
			    New_val_options, NNEW_VAL_OPTIONS, nprop,
			    MAXPROP + 1, MI_ANY, "MENU_NEW_PROP")) {

			case FKEY(2):
			case '\n':
				if ((nprop[0] == '\n') || (nprop[0] == '\0'))
					continue;	/* "go back" */
				break;

			case FKEY(3):
				/* "go back" */
				continue;
			}
			/*FALLTHROUGH*/

		case FKEY(3):
			/* change value */
			if (nprop[0] == '\0') {
				/* didn't set nprop from FKEY(4) case above */
				if ((choice = get_selection_menu(Props_list,
				    Numprops)) == NULL) {
					beep_tty();
					continue;
				}
				strcpy(nprop, choice->string);
			}
			switch (input_menu("MENU_HELP_PROPERTIES",
			    New_val_options, NNEW_VAL_OPTIONS, nval,
			    MAXVAL + 1, MI_ANY,
			    "MENU_NEW_VAL", nprop)) {

			case FKEY(2):
			case '\n':
				break;

			case FKEY(3):
				/* "go back" */
				continue;
			}
			if (nval[0] == 0) {
				/*
				 * null valued props are deleted by
				 * store_prop() if the user wishes to
				 * delete the prop they should
				 * use F5. create a blank property
				 */
				strcpy(nval, " ");
			}
			store_prop(Machenv_name, nprop, nval, TRUE);
			build_menu_prop(Machenv_name);
			break;

		case FKEY(5):
			if ((choice = get_selection_menu(Props_list,
			    Numprops)) == NULL) {
				beep_tty();
				continue;
			}
			strcpy(nprop, choice->string);
			store_prop(Machenv_name, nprop, nval, TRUE);
			build_menu_prop(Machenv_name);
			break;
		}
	}

	/* free memory left over */
	if (Buf) {
		free(Buf);
		Buf = NULL;
	}

	if (Props_list) {
		free(Props_list);
		Props_list = NULL;
	}
}

/*
 * store_prop -- store a property in a file, and set property
 * using bootops.
 */

void
store_prop(char *fname, char *prop, char *value, int do_menu)
{
	FILE *fp;
	struct _stat stbuf;
	char *buf;
	char *line;
	char *eline;
	char *oprop;
	char *eoprop;
	int found = 0;
	int proplen = strlen(prop);

retry:	buf = (char *)NULL;
	if (do_menu == TRUE)
		status_menu(Please_wait, "MENU_UPDATING_CONF", fname);

	if (_stat(fname, &stbuf) < 0) {
		if ((errno == ENOENT) && value) {
			if (value) { /* don't store null properties */
				/* this is okay, just create the file */
				if ((fp = fopen(fname, "w+")) == NULL)
					fatal("storeprop: %s: %!", fname);
				if (update_prop(fp, prop, value)) {
					goto bad;
				}
				fclose(fp);
			}
			return;
		}
		fatal("storeprop: stat: %s: %!", fname);
	}

	if (stbuf.st_size == 0) {
		if (value) { /* don't store null properties */
			if ((fp = fopen(fname, "r+")) == NULL)
				fatal("storeprop 2: %s: %!", fname);
			if (update_prop(fp, prop, value)) {
				goto bad;
			}
			fclose(fp);
		}
		return;
	}

	if ((fp = fopen(fname, "rb")) == NULL)
		fatal("storeprop: fopen: %s: %!", fname);

	if ((buf = malloc((int)stbuf.st_size + 1)) == NULL)
		fatal("storeprop: out of memory (wanted %d bytes)",
		    stbuf.st_size + 1);

	if (fread(buf, 1, (int)stbuf.st_size, fp) == 0) {
		fatal("storeprop: fread on %s: %!", fname);
	}
	fclose(fp);

	if ((fp = fopen(fname, "w")) == NULL) {
		fatal("storeprop: %s: open for writing: %!", fname);
	}

	buf[stbuf.st_size] = '\0';

	/*
	 * go through the file line-by-line and write out each line, possibly
	 * with modifications. we don't bother preserving blank lines,
	 * but any line not starting with a "setprop" command just gets
	 * copied out as-is.
	 */
	line = buf;
	while (*line) {
		/* find the end of this line */
		for (eline = line; *eline; eline++)
			if ((*eline == '\n') || (*eline == '\r'))
				break;
		/*
		 * if we found an end-of-line, write a null there and skip
		 * to the beginning of the next line.  this also removes
		 * any blank lines.
		 */
		if (*eline) {
			*eline++ = '\0';
			while ((*eline == '\n') || (*eline == '\r'))
				eline++;
		}

		/* check for setprop command */
		if (strncmp(line, "setprop", 7)) {
			/* wasn't a "setprop" line -- just write it out */
			if (fprintf(fp, "%s\n", line) < 0) {
				goto bad;
			}
		} else {
			/* find lhs */
			for (oprop = &line[7]; *oprop; oprop++)
				if ((*oprop != ' ') && (*oprop != '\t'))
					break;

			/* find end of lhs */
			for (eoprop = oprop; *eoprop; eoprop++)
				if ((*eoprop == ' ') || (*eoprop == '\t'))
					break;

			/* see if it was the prop we wanted to change */
			if (((eoprop - oprop) != proplen) ||
			    strncmp(prop, oprop, eoprop - oprop)) {
				/* nope */
				if (fprintf(fp, "%s\n", line) < 0) {
					goto bad;
				}
			} else if (!found) {
				/* if new value is null, delete the property */
				if (*value) {
					if (update_prop(fp, prop, value)) {
						goto bad;
					}
				} else {
					/* remove the property in /options */
					out_bop("dev /options\n");
					out_bop("setprop ");
					out_bop(prop);
					out_bop("\n");
				}
				found = 1;	/* remove dups */
			}
		}
		line = eline;
	}

	/* if we didn't see the line in the file, append it now if not null */
	if (!found) {
		if (*value) {
			if (update_prop(fp, prop, value))
				goto bad;
		}
	}
	if (fclose(fp)) {
		goto bad;
	}
	free(buf);
	return;
bad:
	write_err(fname);
	if (buf)
		free(buf);
	goto retry;
}

int
update_prop(FILE *fp, char *prop, char *value)
{
	char buf[PATH_MAX*2];

	(void) sprintf(buf, "setprop %s %s\n", prop, value);
	if (fprintf(fp, buf) < 0) {
		return (1); /* error */
	}
	out_bop("dev /options\n");
	out_bop(buf);
	return (0); /* success */
}

/*
 * set_boot_control_props()
 *
 * Set the root-is-mounted property to be true so that the outside
 * world knows bootconf has successfully mounted the root, built the
 * device tree, set the bootpaths, etc.
 *
 * Set the reconfigure-devices devices property if we are not
 * autobooting (rfe 4042463).
 */
void
set_boot_control_props(void)
{
	out_bop("dev /options\n");
	out_bop("setprop root-is-mounted true\n");

	if (!Autoboot) {
		char *pp, *endp, *buf, *flagp, *pbuf;

		pp = read_prop("boot-args", "chosen");
		if (pp == NULL) {
			out_bop("setprop boot-args -r\n");
		} else {
			/*
			 * Search backwards in the string for the
			 * flag character
			 */
			flagp = strrchr(pp, '-');

			/*
			 * Ensure its preceded by a space or at the
			 * beginning of the line.
			 */
			if (flagp && (flagp != pp) && (*(flagp - 1) != ' ')) {
				flagp = NULL;
			}

			/*
			 * Just return if the -r flag is already there
			 */
			if (flagp && strchr(flagp, 'r')) {
				return;
			}

			/*
			 * Add an "r" to the flags or " -r" if no flags yet.
			 */
			buf = malloc(strlen(pp) + 5);
			strcpy(buf, pp);

			endp = strchr(buf, '\n');
			if (flagp == NULL) {
				*endp++ = ' ';
				*endp++ = '-';
			}
			*endp++ = 'r';
			*endp = 0;
			pbuf = malloc(strlen(buf) + 30);
			(void) sprintf(pbuf, "setprop boot-args \"%s\"\n", buf);
			out_bop(pbuf);
			free(buf);
			free(pbuf);
		}
	} else /* restore root plat props from probed* props if autoboot */
		restore_plat_props();
}

/*
 * read_prop()
 *
 * Attempts to get the property from the specified node.  If the
 * node is not specified we default to the /options node.
 * Checks for property value greater than the input buffer size
 * currently 120 bytes). Returns the string on success, 0 on failure.
 */
char *
read_prop(char *prop, char *where)
{
	static char ibuf[PATH_MAX*2];
	char obuf[PATH_MAX]; /* 1275 property names have a max length of 31 */
	char *nl;

	if (where) {
		(void) sprintf(obuf, "dev /%s\n", where);
	} else {
		(void) sprintf(obuf, "dev /options\n");
	}
	out_bop(obuf);

	(void) sprintf(obuf, "getprop %s\n", prop);
	out_bop(obuf);

	ibuf[0] = 0;
	(void) in_bop(ibuf, PATH_MAX);
	if ((nl = strchr(ibuf, '\n')) == 0) { /* check for overflow */
		/*
		 * overflowed input buf - discard the rest
		 */
		while (in_bop(ibuf, PATH_MAX) && (strchr(ibuf, '\n') == 0)) {
			;
		}
		return (0);
	}

	(void) sprintf(obuf, "getprop: %s not found", prop);
	if ((strncmp(obuf, ibuf, strlen(obuf) - 1) == 0) || (ibuf[0] == 0)) {
		return (0);
	}
	*nl = 0;		/* Replace the trailing newline by a null */
	return (ibuf);
}

void
reset_plat_props()
{
	/*
	 * reset name and compatible properties to "i86pc"
	 */
	out_bop("dev /\n");
	out_bop("setprop name i86pc\n");

	out_bop("dev /\n");
	out_bop("setprop compatible i86pc\n");

	out_bop("dev /\n");
	out_bop("setprop si-hw-provider \n");
}

void
update_plat_prop(char *root_prop, char *option_prop)
{
	char *pp;
	char nval[4];

	extern	int Floppy;

	nval[0] = 0;	/* use to delete property */
	/* read property from root node */
	if (((pp = read_prop(root_prop, "")) == NULL) ||
		strcmp(pp, "\"\"") == 0)	/* empty property */
		pp = nval;
	if (Floppy) {
		out_bop("dev /options\n");
		out_bop("setprop ");
		out_bop(option_prop);
		out_bop(" ");
		out_bop(pp);
		out_bop("\n");
	} else
		store_prop(Machenv_name, option_prop, pp, FALSE);
}

void
save_plat_props()
{

	update_plat_prop("name", "probed-arch-name");
	update_plat_prop("compatible", "probed-compatible");
	update_plat_prop("si-hw-provider", "probed-si-hw-provider");
}

void
restore_plat_props()
{
	char *pp;

	/* from option node */
	if ((pp = read_prop("probed-arch-name", "options")) != NULL) {
		out_bop("dev /\n");
		out_bop("setprop name ");
		out_bop(pp);
		out_bop("\n");
	}
	if ((pp = read_prop("probed-compatible", "options")) != NULL) {
		out_bop("dev /\n");
		out_bop("setprop compatible ");
		out_bop(pp);
		out_bop("\n");
	}
	if ((pp = read_prop("probed-si-hw-provider", "options")) != NULL) {
		out_bop("dev /\n");
		out_bop("setprop si-hw-provider ");
		out_bop(pp);
		out_bop("\n");
	}
}
