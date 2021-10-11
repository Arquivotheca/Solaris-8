/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)itup2.c   1.7   99/04/23 SMI"

/*
 * Phase 2 of Install Time Updates.
 *
 * Driver entries like aliases, devlink.tab from ITUOs are merged with
 * root. Name to Major entries are created if needed.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bop2.h"
#include "dir.h"
#include "itu.h"

/*
 * Prepare to use regexp
 */
#define	INIT	register char *sp = instring;
#define	GETC()	(*(global_sp = sp++))
#define	PEEKC()	(*(global_sp = sp))
#define	UNGETC(c)	(global_sp =  --sp)
#define	RETURN(c)	return (c)
#define	ERROR(c)	regexp_error(c)

char *global_sp = NULL;

#include "regexp.h"

#define	PROP_SEP ",\n"
#define	ERR_PHASE2 "Error itu phase 2: "
#define	N2MHDR "**************** NEW NAME_TO_MAJ **************\n"

#define	WAIT_FOR_ENTER() \
{\
	printf("Hit enter to continue\n");\
	getch();\
}
typedef struct drvlist {
	struct drvlist *nextp;
	char path_name[80]; /* very arib namelength */
	char drv_name[80]; /* very arib namelength */
} drvl_t;

typedef struct _edit {
	struct _edit *next;
	char	*cmd;
} edit_t;

typedef struct _mapping {
	struct _mapping *next;
	char *unix_name;
	char *dos_name;
	edit_t *edits;
} mapping_t;

int vflg;
drvl_t *drvhp;
mapping_t *maplist;


static void fatal_error(int exit_val);
static void map_kernel_files();
static void copy_kernel_files();
static int parse_prop(char *drvs);
static int find_avail_num();
static void add_mapped_name(char *unix_name, char *dos_name);
static void find_mapped_name(char *unix_name, char *dos_name);
static char * get_mapped_name(char *unix_name);
static mapping_t *find_mapping(char *unix_name);
static char *find_unix_name(char *dos_name);
static char *find_dos_name(char *unix_name);
static void read_file_edits();
static void add_file_edit(char *unix_name, char *cmd);
static void do_line_edit(edit_t *edit, char *line);
static int regexp_error(int error_code);

/*
 * Fatal error condition
 */
static void
fatal_error(int exit_val)
{
	printf("A fatal error has occured while processing the update.\n");
	/* NEEDSWORK: more dire message contents */
	WAIT_FOR_ENTER();
	exit(exit_val);
	/* NOTREACHED */
}

/*
 *
 */
static void
add_file_edit(char *unix_name, char *cmd)
{
	mapping_t *map;
	edit_t *edit;

	/*
	 * force a mapping to be created if need be
	 * and the mapping_t for it
	 */
	get_mapped_name(unix_name);
	map = find_mapping(unix_name);
	if (map == NULL) {
		printf(ERR_PHASE2 "Internal error (add_file_edit)\n");
		fatal_error(1);
		/* NOTREACHED */
	}

	if ((edit = (edit_t *) malloc(sizeof (edit_t))) == NULL) {
		printf(ERR_PHASE2 "Out of memory (add_file_edit)\n");
		fatal_error(1);
		/* NOTREACHED */
	}

	edit->cmd = (char *) malloc(strlen(cmd)+1);
	if (edit->cmd == NULL) {
		printf(ERR_PHASE2 "Out of memory (add_file_edit)\n");
		fatal_error(1);
		/* NOTREACHED */
	}
	strcpy(edit->cmd, cmd);

	edit->next = map->edits;
	map->edits = edit;
}

/*
 *
 */
static void
read_file_edits()
{
	FILE	*fedits;
	char buffer[MAXLINE];
	char	*fname, *cmd;

	if ((fedits = fopen(U_EDITFILE, "r")) == NULL)
		return;		/* NEEDSWORK: */

	while (fgets(buffer, MAXLINE, fedits)) {
		fname = buffer;
		if ((cmd = strchr(buffer, ' ')) == NULL) {
			printf(ERR_PHASE2 "Internal error (read_file_edits)\n");
			fatal_error(1);
			/* NOTREACHED */
		}
		*cmd++ = '\0';

		add_file_edit(fname, cmd);
	}

	fclose(fedits);
	unlink(U_EDITFILE);
}

/*
 * line editor
 */
static void
do_line_edit(edit_t *edit, char *line)
{
	int	c, need_cmd = 0, done = 0;
	char *expbuf;
	char *cp = edit->cmd;
	char *fromp, *top;
	char eof_char = '/';

	if (!cp)
		return;

	expbuf = (char *) malloc(MAXLINE);
	if (expbuf == NULL) {
		printf(ERR_PHASE2 "Out of memory (do_line_edit)\n");
		fatal_error(1);
		/* NOTREACHED */
	}

	while (!done && (c = *cp++)) {
		switch (c) {
		case '/':		/* does this line match the RE? */
			if (need_cmd)
				goto edit_error;
			compile(cp, expbuf, expbuf+MAXLINE, '/');
			if (!step(line, expbuf)) {
				done = 1;
				break;		/* not a match */
			}
			cp = global_sp + 1;	/* skip '/' */
			need_cmd = 1;
			continue;
			/* NOTREACHED */
		case 'd':
			*line = '\0';
			return;
			/* NOTREACHED */
		case 's':
			if (*cp == '\0')
				goto edit_error;
			eof_char = *cp++;
			compile(cp, expbuf, expbuf+MAXLINE, eof_char);
			if (!step(line, expbuf)) {
				done = 1;
				break;		/* not a match */
			}
			/* matched RE; now replace substr */

			cp = global_sp + 1;	/* skip eof_char */
			/* we're done with expbuf - we can re-use it */

			/*
			 * Copy part of line before RE
			 */
			fromp = line;
			top = expbuf;
			while (fromp != loc1)
				*top++ = *fromp++;

			/*
			 * Make the actual substitution
			 */
			while (*cp && (*cp != eof_char))
				*top++ = *cp++;
			/* be a stickler for that trailing eof_char */
			if (*cp == '\0')
				goto edit_error;

			/*
			 * Copy part of line after RE
			 */
			fromp = loc2;
			while (*fromp)
				*top++ = *fromp++;

			/*
			 * Make sure to copy the '\0' ;-)
			 */
			*top++ = *fromp++;

			/*
			 * Put the finished line back
			 */
			strcpy(line, expbuf);
			done = 1;
			break;

		default:
edit_error:
			printf(ERR_PHASE2
				"Illegal edit command <%s>\n", edit->cmd);
			fatal_error(1);
			/* NOTREACHED */
		}
	}
	free(expbuf);

}

int
regexp_error(int error_code)
{
	printf(ERR_PHASE2 "regexp failure (%d)\n", error_code);
	fatal_error(1);
	/* NOTREACHED */
}

/*
 * Filename mapping support
 */

/*
 * Add a name mapping to the list
 */
static void
add_mapped_name(char *unix_name, char *dos_name)
{
	mapping_t *mp;
	char *un, *dn;

	mp = (mapping_t *) malloc(sizeof (mapping_t));
	un = (char *) malloc(strlen(unix_name)+1);
	dn = (char *) malloc(strlen(dos_name)+1);
	if ((mp == NULL) || (un == NULL) || (dn == NULL)) {
		printf("itup2: out of memory (amn)\n");
		fatal_error(1);
		/* NOTREACHED */
	}

	strcpy(un, unix_name);
	strcpy(dn, dos_name);
	mp->unix_name = un;
	mp->dos_name = dn;
	mp->edits = NULL;
	mp->next = maplist;
	maplist = mp;
	if (vflg) {
		printf("itup2: added mapping <%s> <%s>\n", un, dn);
	}
}

/*
 *  Look for mappings.
 */
static mapping_t *
find_mapping(char *unix_name)
{
	mapping_t *mp;

	if (unix_name == NULL)
		return (NULL);

	for (mp = maplist; mp != NULL; mp = mp->next)
		if (strcmp(mp->unix_name, unix_name) == 0)
			return (mp);

	return (NULL);
}

static char *
find_dos_name(char *unix_name)
{
	mapping_t *mp;

	if (unix_name == NULL)
		return (NULL);

	for (mp = maplist; mp != NULL; mp = mp->next)
		if (strcmp(mp->unix_name, unix_name) == 0)
			return (mp->dos_name);

	return (NULL);
}

static char *
find_unix_name(char *dos_name)
{
	mapping_t *mp;

	if (dos_name == NULL)
		return (NULL);

	for (mp = maplist; mp != NULL; mp = mp->next)
		if (strcmp(mp->dos_name, dos_name) == 0)
			return (mp->unix_name);

	return (NULL);
}

/*
 * Get a mapped name for DOS use.
 * We always create a mapped name since odds are we'll always need to.
 */
static char *
get_mapped_name(char *unix_name)
{
	static int name_number = 0;
	char *dos_name;
	char newname[MAXLINE];


	if ((dos_name = find_dos_name(unix_name)) != NULL)
		return (dos_name);

	sprintf(newname, "U:file%04.4x", name_number++);
	add_mapped_name(unix_name, newname);
	return (find_dos_name(unix_name));
}

/*
 * Write out all the maps for all mapped files.
 */
void
map_kernel_files()
{
	char *mp;
	mapping_t *map;

	if ((mp = (char *) malloc(PATH_MAX)) == NULL) {
		printf(ERR_PHASE2 "Out of Memory\n");
		fatal_error(1);
		/* NOTREACHED */
	}

	for (map = maplist; map != NULL; map = map->next) {
		sprintf(mp, "map %s" SPACES "/%s\n", map->unix_name,
			map->dos_name + 2); /* skip past U: */
		out_bop(mp);
	}

	free(mp);
}

/*
 * Copy mapped files to RAM disk.
 */
void
copy_kernel_files()
{
	FILE *kfp, *rfp;
	char buffer[MAXLINE];
	mapping_t *map;

	for (map = maplist; map != NULL; map = map->next) {
		kfp = fopen(map->unix_name, "r");
		rfp = fopen(map->dos_name, "a+");
		if (kfp == NULL || rfp == NULL) {
			if (kfp == NULL)
				printf(ERR_PHASE2 "open failed for %s\n",
					map->unix_name);
			if (rfp == NULL)
				printf(ERR_PHASE2 "open failed for %s\n",
					map->dos_name);
			fatal_error(1);
			/* NOTREACHED */
		}

		while (fgets(buffer, MAXLINE, kfp)) {
			edit_t *edit;

			/* Apply all edits */
			for (edit = map->edits; edit; edit = edit->next)
				do_line_edit(edit, buffer);

			if (fputs(buffer, rfp) == EOF) {
				printf(ERR_PHASE2 "write failed for %s\n",
					map->dos_name);
				fatal_error(1);
				/* NOTREACHED */
			}
		}

		fclose(rfp);
		fclose(kfp);
	}
}

/*
 * The drivers property list consists of the follwoing sequence
 * <volume>,<drvname>.
 * parses the drivers list and returns count.
 * Exit: -1 on error.
 */
int
parse_prop(char *drvs)
{
	char *token;
	int  count;
	drvl_t *dp;
	char *path;

	if (vflg) {
		printf("parse_prop: list %s\n", drvs);
		WAIT_FOR_ENTER();
	}
	count = 0;
	if (token = (char *)strtok(drvs, PROP_SEP)) {
		/*
		 * Only even numbers are drivers
		 */
		do {
			if (++count % 2)
				continue;
			if (vflg) {
				printf("parse_prop: token %s\n", token);
				WAIT_FOR_ENTER();
			}
			if ((dp = calloc(1, sizeof (drvl_t))) == NULL) {
				printf(ERR_PHASE2 " Memory Failure\n");
				return (-1);
			}

			/*
			 * Workaround for non-driver objects:
			 * save path component if it exists on
			 * driver name, and use it to determine
			 * if major number should be allocated.
			 */
			path = strrchr(token, '/');
			if (path) {
				*path = '\0';
				strcpy(dp->path_name, token);
				token = path + 1;
			} else
				strcpy(dp->path_name, "drv");

			strcpy(dp->drv_name, token);
			if (drvhp != NULL)
				dp->nextp = drvhp;
			drvhp = dp;

		} while (token = (char *)strtok(NULL, PROP_SEP));
	}
	return (count / 2);
}

/*
 * This routine checks to see if the ITU'ed driver has a name_to_major
 * number on the mounted realmode root. If it does not a number gets
 * assigned.
 *
 * Exit: -1 or 0 on error.
 */

int
find_avail_num()
{
	FILE *n2mfp, *rfp;
	char name[30];
	drvl_t *dp, *dpp;
	int n2m_last;
	int num;

	n2m_last = -1;
	if ((n2mfp = fopen("/etc/name_to_major", "r")) == NULL) {
		return (-1);
	}
	/*
	 * Open the RAM file.
	 */
	if ((rfp = fopen(U_NAME_TO_MAJ, "a+")) == NULL) {
		printf(ERR_PHASE2
			"unable to open RAM file-" U_NAME_TO_MAJ);
		fclose(n2mfp);
		return (-1);
	}

	while (fscanf(n2mfp, "%s%d", name, &num) != EOF) {
		/*
		 * Check if name matches the one in drivers prop
		 */
		for (dp = dpp = drvhp; dp; dpp = dp, dp = dp->nextp) {
			if (strcmp(name, dp->drv_name) == 0) {
				/* A match remove this from list */
				if (vflg) {
					printf("%s deleted from list\n",
						dp->drv_name);
					WAIT_FOR_ENTER();
				}
				if (dp == drvhp) {
					drvhp = dp->nextp;
					continue;
				}
				dpp->nextp = (dp->nextp) ? dp->nextp :
								NULL;
			}
		} /* matched for( .. */
		if (num > n2m_last)
			n2m_last = num;
	}

	if (vflg) {
		printf("highest num found %d\n", n2m_last);
		WAIT_FOR_ENTER();
	}
	fclose(n2mfp);
	fclose(rfp);
	return (n2m_last + 1);
}

static void
dump_file(char *name)
{
	FILE *fp;
	char drvs[PATH_MAX];
	int linecnt;

	if ((fp = fopen(name, "r")) == NULL) {
		printf(ERR_PHASE2 "open of %s failed\n",
							U_NAME_TO_MAJ);
		fatal_error(1);
		/* NOTREACHED */
	}
	linecnt = 0;
	while (fgets(drvs, 80, fp)) {
		printf("%s", drvs);
		linecnt = (linecnt == 22) ? 0 : linecnt++;
		if (linecnt == 0)
			WAIT_FOR_ENTER();
	}
	fclose(fp);
}

#define	ERROR_OUT(val)\
{\
	exitcode = val;\
	goto exit_main;\
}

main(int argc, char **argv)
{
	int c, errflg, exitcode;
	char drvs[PATH_MAX];
	char obtree_prop[PATH_MAX];
	char *prp;
	drvl_t *dp;
	int num_drvs, n2m_avail, i;
	FILE *fp;

	errflg = 0;
	exitcode = 0;
	if (argc > 2)
		errflg++;
	if (argc > 1) {
		if (strcmp(argv[1], "-v") == 0)
		    vflg++;
		else
		    errflg++;
	}

	if (errflg) {
		printf("itup2.exe [-v]\n");
		fatal_error(1);
		/* NOTREACHED */
	}
	init_bop();

	/*
	 * for the duration of phase 2 we modify the property
	 * boottree.
	 */
	if ((prp = read_prop("boottree", "chosen")) == NULL) {
		printf(ERR_PHASE2 "getprop of boottree failed\n");
		fatal_error(1);
		/* NOTREACHED */
	}
	strcpy(obtree_prop, prp);

	write_prop("boottree", "chosen", "/", ":", OVERWRITE_PROP);
	if ((prp = read_prop("drivers", ITU_PROPS)) == NULL) {
		if (vflg) {
			printf("No drivers to update!!\n");
			WAIT_FOR_ENTER();
		}
		ERROR_OUT(0);
	}

	strcpy(drvs, prp);
	if (vflg) {
		printf("list of drivers %s\n", drvs);
		WAIT_FOR_ENTER();
	}
	if ((num_drvs = parse_prop(drvs)) == 0) {
		if (vflg)
			printf(ERR_PHASE2 "Error parsing property drivers\n");
		ERROR_OUT(1);
	}

	if (vflg) {
	    printf("Found %d ITU drivers\n", num_drvs);
		WAIT_FOR_ENTER();
	}

	if ((n2m_avail = find_avail_num()) <= 0) {
		printf(ERR_PHASE2 "Error reading /etc/name_to_major\n");
		ERROR_OUT(1);
	}
	if ((fp = fopen(U_NAME_TO_MAJ, "a+")) == NULL) {
		printf(ERR_PHASE2 "Error opening %s\n", U_NAME_TO_MAJ);
		ERROR_OUT(1);
	}

	/*
	 * Write out the information to RAM file and
	 * name-to-major properity.
	 */

	for (dp = drvhp; dp; dp = dp->nextp) {

		/*
		 * Workaround: for non-driver objects:
		 * only allocate major numbers for drivers.
		 */
		if (!strstr(dp->path_name, "drv"))
			continue;

		fprintf(fp, "%s %d\n", dp->drv_name, n2m_avail);
		sprintf(drvs, "%s-%d", dp->drv_name, n2m_avail);
		if (vflg) {
			printf("namtomaj: prop  %s\n", drvs);
			WAIT_FOR_ENTER();
		}
		write_prop("name-to-major", ITU_PROPS, drvs, ":",
								UPDATE_PROP);
		n2m_avail++;
	}
	fclose(fp);

	/*
	 * Add the default name mappings
	 */
	add_mapped_name("/etc/driver_aliases", U_DRV_ALIASES);
	add_mapped_name("/etc/driver_classes", U_DRV_CLASSES);
	add_mapped_name("/etc/system", U_SYSTEM);
	add_mapped_name("/etc/devlink.tab", U_DEVLINK_TAB);
	add_mapped_name("/etc/mach", U_MACH);
	if (drvhp != NULL)
		add_mapped_name("/etc/name_to_major", U_NAME_TO_MAJ);

	/*
	 * Read the file_edit commands
	 */
	read_file_edits();

	/*
	 * Copy all mapped files to RAM disk and create mappings
	 */
	copy_kernel_files();
	map_kernel_files();

	if (vflg) {
		printf("Name to Major: \n");
		dump_file(U_NAME_TO_MAJ);
		printf("Driver classes: \n");
		dump_file(U_DRV_CLASSES);
	}

	/*
	 * Revert the property for boottree back
	 */
exit_main:
	write_prop("boottree", "chosen", obtree_prop, ":", OVERWRITE_PROP);
	exit(exitcode);
}
