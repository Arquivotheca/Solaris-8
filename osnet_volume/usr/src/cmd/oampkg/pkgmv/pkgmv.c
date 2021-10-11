
#ident	"@(#)pkgmv.c	1.21	95/01/18 SMI"

/*
 *	Copyright (c) 1995 Sun Microsystems, Inc.  All Rights Reserved.
 *	Sun considers its source code as an unpublished, proprietary
 *	trade secret, and it is available only under strict license
 *	provisions.  This copyright notice is placed here only to protect
 *	Sun in the event the source is deemed a published work.	 Dissassembly,
 *	decompilation, or other means of reducing the object code to human
 *	readable form is prohibited by the license agreement under which
 *	this code is provided to the user or company in possession of this
 *	copy.
 *
 *	RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the
 *	Government is subject to restrictions as set forth in subparagraph
 *	(c)(1)(ii) of the Rights in Technical Data and Computer Software
 *	clause at DFARS 52.227-7013 and in similar clauses in the FAR and
 *	NASA FAR Supplement.
 */

/*
 * pkgmv - moves package content file entries from one contents file
 *	   to another contents file.
 */

#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <libintl.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pkglocs.h>
#include <errno.h>
#include <pkgstrct.h>
#include <pkginfo.h>
#include <pkglib.h>
#include "libadm.h"
#include "libinst.h"

enum rule_type {pkgname, pkg_xlate};

struct rule {
	struct rule	*next;
	enum rule_type	type;
	char		*input;
	char		*output;
	};

static struct rule *rules = NULL;
static int	    rcount = 0;

struct cfent_hold {
	struct cfent_hold	*next;
	struct cfent		*pcfent;
	};

#define	WRN_PKG		"package <%s> conflict - use removef\n"
#define	ERR_MEMORY	"memory allocation failure, errno=%d"
#define	ERR_CONTWRITE	"could not write records to <t.%s>"

char		*pkginst = NULL;

int		warnflag = 0,
		nocnflct = 1,
		nosetuid = 0,
		nointeract = 0;


static struct cfent	**eptlist;

static char	*lstrndup(char *string, int count);

static struct	pinfo *remove_dups(struct cfent *cf_ent, struct cfent *el_ent);

static int	mergedb(FILE *fpin, FILE *fpout, struct cfent **eptlist);
static int	xlate_path(struct cfent *cf_ent);

static void	keep_record(struct cfent *cf_ent);
static void	remove_pinfo(struct rule *rules, struct cfent *cf_ent);
static void	quit(int exitval), sort_rules(void), usage(void);


static struct cfent_hold	*hold_rec = NULL;
static struct cfent_hold	*crec = NULL;
static int	nrecs = 0;
static int	delete_entry = 0,
		sort_entries = 0,
		ascii_out = 0;
static char	*source_dir = NULL,
		*dest_dir = NULL;

main(int argc, char *argv[])
{

	struct rule	*trules = NULL;
	int c;
	char	*pt = NULL;
	FILE	*fpin, *fpout_source, *fpout_dest;
	char	contents[PATH_MAX];
	int	got_record, warn;
	int	n, i, j;
	extern char	*optarg;
	extern int	optind;

	struct cfent	cf_ent;
	struct pinfo	*pinfoptr;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	(void) set_prog_name(argv[0]);

	while ((c = getopt(argc, argv, "R:S:zo?")) != EOF)
		switch (c) {

		    case 'R':
			source_dir = flex_device(optarg, 0);
			break;

		    case 'S':
			dest_dir = flex_device(optarg, 0);
			break;

		    case 'o':
			ascii_out++;
			break;

		    case 'z':
			delete_entry++;
			break;

		    case '?':
			usage();
			quit(1);
			/*NOTREACHED*/

		    case 's':
			sort_entries++;
			break;

		    default:
			usage();
			quit(1);
		}

	if (!dest_dir && !ascii_out) {
		progerr(gettext("either -S or -o must be specified"));
		quit(1);
	}

	if (dest_dir && (dest_dir[0] != '/')) {
		progerr(gettext("path name must be full path name"));
		quit(1);
	}

	/*  Build the list of packages and rules */

	while (optind < argc) {
		if (!rules) {
			rules = (struct rule *) malloc(sizeof (struct rule));
			trules = rules;
		} else {
			trules->next = (struct rule *)
			    malloc(sizeof (struct rule));
			trules = trules->next;
		}
		trules->next = NULL;
		rcount++;
		if ((pt = strstr(argv[optind], "=")) != NULL) {
			trules->type = pkg_xlate;
			trules->input = lstrndup(argv[optind], (pt -
				(char *)(argv[optind])));
			trules->output = strdup(pt+1);
		} else {
			trules->type = pkgname;
			trules->input = strdup((argv[optind]));
		}
		optind++;
	}

	sort_rules();
	set_PKGpaths(source_dir);
	(void) sprintf(contents, "%s/contents", get_PKGADM());

	if (delete_entry)
		n = ocfile(&fpin, &fpout_source, 0L);
	else {
		fpin = fopen(contents, "r+");
		fpout_source = NULL;
	}

	if ((fpin == NULL) || (n == -1)) {
		progerr(gettext("could not access <%s>"), contents);
		quit(99);
	}

	/*
	 * now that the input contents file is open, and the output
	 * contents file is opened it is time to do something.  Start by
	 * srchcfile with a path of *, to step through the input contents
	 * file.  Then see if a record is selected, if so keep it otherwise
	 * don't.
	 */

	cf_ent.pinfo = NULL;
	while ((n = srchcfile(&cf_ent, "*", fpin, fpout_source)) != 0) {
		trules = rules;
		got_record = 0;
		warn = 0;
		while (trules) {
			pinfoptr = cf_ent.pinfo;
			if (trules->type == pkgname) {
				while (pinfoptr) {
					if (strcmp(pinfoptr->pkg, trules->input)
					    == 0) {
						if (!got_record) {
							(void) xlate_path
							    (&cf_ent);
							keep_record(&cf_ent);
							got_record++;
							nrecs++;
						}
					} else {
						if (got_record && !warn) {
							(void) printf(
							    gettext(WRN_PKG),
							    pinfoptr->pkg);
							warn++;
						}
					}
					pinfoptr = pinfoptr->next;
				}
			} else {
				if (xlate_path(&cf_ent) == 1) {
					keep_record(&cf_ent);
					got_record++;
					nrecs++;
					break;
				}
			}
			trules = trules->next;
		}
		if (delete_entry && got_record)
			remove_pinfo(rules, &cf_ent);
		if (cf_ent.npkgs <= 0)
			continue;
		if (delete_entry)
			if (putcfile(&cf_ent, fpout_source) != 0) {
				progerr(gettext(ERR_CONTWRITE), contents);
				quit(99);
		}
	}
	(void) fclose(fpin);

	/*
	 * At this point in time all of the records are collected in a an
	 * linked list pointed to by hold_rec.  It is now time to create
	 * a sorted array called eptlist, of the records.
	 */

	if ((eptlist = (struct cfent **) calloc(nrecs + 1,
	    sizeof (struct cfent *))) == NULL) {
		progerr(gettext(ERR_MEMORY), errno);
		quit(99);
	}

	crec = hold_rec;
	i = 0;
	while (crec) {
		eptlist[i++] = crec->pcfent;
		crec = crec->next;
	}

	/*
	 *  Sort the entries. This is currently a bubble sort that is
	 *  very slow.  It would be better to change this to a quick
	 *  sort, or even a heap sort.
	 */
	if (sort_entries) {
		for (i = 0; i < nrecs; i++)
			for (j = 0; j < nrecs; j++) {
				n = strcmp(eptlist[i]->path, eptlist[j]->path);
				if (n < 0) {
					struct cfent *ptemp;
					ptemp = eptlist[i];
					eptlist[i] = eptlist[j];
					eptlist[j] = ptemp;
				}
			}
	}

	if (dest_dir) {
		set_PKGpaths(dest_dir);
		if (ocfile(&fpin, &fpout_dest, 0L) == -1)
			quit(99);
		i = mergedb(fpin, fpout_dest, eptlist);
		if (i != nrecs) {
			progerr(
			    gettext("bad merge of contents file ... aborted"));
			quit(99);
		}
		if (swapcfile(fpin, fpout_dest, "move"))
			quit(99);
		if (delete_entry) {
			set_PKGpaths(source_dir);
			if (swapcfile(NULL, fpout_source, "move"))
				quit(99);
		}
	}

	if (ascii_out)
		for (i = 0; i < nrecs; i++)
			putcfile(eptlist[i], stdout);

	quit(0);
	/*NOTREACHED*/
#ifdef lint
	return (0);
#endif	/* lint */
}

static char *
lstrndup(char *string, int count)
{
	char *pt;

	if (count <= 0)
		return (0);

	if (!string)
		return (0);

	if ((pt = (char *) malloc(count+1)) == NULL) {
		progerr(gettext(ERR_MEMORY), errno);
		return (0);
	}

	if (strncpy(pt, string, count) != pt)
		return (0);

	return (pt);
}

static void
quit(int retcode)
{

	if (retcode > 1)
		(void) printf(
		    gettext("%s: ERROR: internal error - nothing done\n"),
		    get_prog_name());
	exit(retcode);
}

static void
usage(void)
{
	(void) printf(gettext("usage: %s [RSosd] pkginst | path1=path2"),
	    get_prog_name());
	quit(1);
}

static void
keep_record(struct cfent *cf_ent)
{
	struct cfent	*pentry;
	struct pinfo	*ppinfo, *npinfo,
			*pppinfo = NULL;

	/*
	 * now that we know there is work to do allocate memory and
	 * place the record into it.
	 */

	if ((pentry = (struct cfent *) malloc(sizeof (struct cfent) + 1))
	    == NULL) {
		progerr(gettext(ERR_MEMORY), errno);
		quit(99);
	}
	(void) memcpy(pentry, cf_ent, sizeof (struct cfent));
	if ((pentry->path = strdup(cf_ent->path)) == NULL) {
		progerr(gettext(ERR_MEMORY), errno);
		quit(99);
	}
	if (cf_ent->ainfo.local != NULL)
		if ((pentry->ainfo.local = strdup(cf_ent->ainfo.local)) ==
		    NULL) {
			progerr(gettext(ERR_MEMORY), errno);
			quit(99);
	}

	ppinfo = cf_ent->pinfo;

	while ((ppinfo != NULL)) {
		if ((npinfo = (struct pinfo *) malloc(sizeof (struct pinfo)))
		    == NULL) {
			progerr(gettext(ERR_MEMORY), errno);
			quit(99);
		}
		(void) memcpy(npinfo, ppinfo, sizeof (struct pinfo));
		if (!pppinfo)
			pentry->pinfo = npinfo;

		if (pppinfo)
			pppinfo->next = npinfo;
		pppinfo = npinfo;
		ppinfo = ppinfo->next;
	}

	if (!hold_rec) {
		hold_rec = (struct cfent_hold *)
		    malloc(sizeof (struct cfent_hold));
		crec = hold_rec;
	} else {
		crec->next = (struct cfent_hold *)
		    malloc(sizeof (struct cfent_hold));
		crec = crec->next;
	}

	if (!crec || !hold_rec) {
		progerr(gettext(ERR_MEMORY), errno);
		quit(99);
	}

	crec->pcfent = pentry;
	crec->next = NULL;
}

static int
mergedb(FILE *fpin, FILE *fpout, struct cfent **eptlist)
{
	int	i = 0, n;
	struct cfent	cf_ent;

	cf_ent.pinfo = NULL;
	cf_ent.ainfo.local = NULL;
	for (;;) {
		n = srchcfile(&cf_ent, eptlist[i] ? eptlist[i]->path : NULL,
		    fpin, fpout);
		if (eptlist[i])
			break;

		if (n < 0) {
			progerr(gettext("bad destination contents file"));
			quit(99);
		}

		if (n == 1) {
			cf_ent.pinfo = remove_dups(&cf_ent, eptlist[i]);

			if (putcfile(&cf_ent, fpout) == -1) {
				progerr(
				    gettext("could not write to t.contents"));
				    quit(99);
			}
		} else if (putcfile(eptlist[i], fpout) == -1) {
			progerr(gettext("could not write to t.contents"));
				quit(99);
		}
		i++;
	}
	return (i);
}

static struct pinfo *
remove_dups(struct cfent *cf_ent, struct cfent *el_ent)
{
	struct pinfo	*pinfo, *pinfo1, *last_pinfo = NULL;
	int		found, moved;

	for (pinfo = cf_ent->pinfo; pinfo->next; pinfo = pinfo->next);
	pinfo->next = el_ent->pinfo;

	for (pinfo = cf_ent->pinfo; pinfo; pinfo = pinfo->next) {
		found = 0;
		for (pinfo1 = cf_ent->pinfo; pinfo1; pinfo1 = pinfo1->next) {
			moved = 0;
			if (strcmp(pinfo->pkg, pinfo1->pkg) == 0) {
				if (found) {
					last_pinfo->next = pinfo1->next;
					moved++;
				} else {
					found++;
				}
			}
			if (!moved)
				last_pinfo = pinfo1;
		}
	}
	return (cf_ent->pinfo);
}

static void
remove_pinfo(struct rule *rules, struct cfent *cf_ent)
{
	struct pinfo	*pinfo, *last_pinfo = NULL;
	int		moved;
	struct rule	*current_rule;

	for (pinfo = cf_ent->pinfo; pinfo; pinfo = pinfo->next) {
		moved = 0;
		for (current_rule = rules; current_rule;
		    current_rule = current_rule->next) {
			if (current_rule->type != pkgname)
				continue;
			if (strcmp(current_rule->input, pinfo->pkg) == 0) {
				moved++;
				cf_ent->npkgs--;
				if (!last_pinfo)
					cf_ent->pinfo = pinfo->next;
				else
					last_pinfo->next = pinfo->next;
			}
		}
		if ((moved && !last_pinfo) || moved)
			continue;

		last_pinfo = pinfo;
	}
}

static int
xlate_path(struct cfent *cf_ent)
{
	struct rule	*prules;

	for (prules = rules; prules; prules = prules->next) {
		if (strstr(cf_ent->path, prules->input) == cf_ent->path) {
			char *pc, *pc1;
			signed int  i, j;

			i = strlen(cf_ent->path);
			j = strlen(prules->input);
			if (i >= j)
				pc = cf_ent->path + j;
			else
				return (-1);

			if ((*pc != '/') && (*pc != NULL))
				return (0);

			pc1 = (char *) malloc(strlen(pc) +
			    strlen(prules->output) + 1);
			(void) strcpy(pc1, prules->output);
			(void) strcat(pc1, pc);
			(void) free(cf_ent->path);
			cf_ent->path = pc1;
			return (1);
		}
	}
	return (0);
}

static void
sort_rules(void)
{
	struct rule	**srules;
	struct rule	*prules;
	int 		i = 0, j;

	srules = (struct rule **)calloc(rcount + 1, sizeof (struct rule *));
	if (srules == NULL) {
		progerr(gettext(ERR_MEMORY), errno);
		quit(99);
	}

	for (prules = rules; prules; prules = prules->next)
		srules[i++] = prules;

	for (i = 0; srules[i]; i++) {
		for (j = 0; srules[j]; j++) {
			int swap = 0;

			if ((srules[i]->type == pkgname) &&
			    (srules[j]->type == pkg_xlate))
				swap++;

			if ((srules[i]->type == pkg_xlate) &&
			    (srules[j]->type == pkg_xlate))
				if ((int) strlen(srules[i]->input) >
				    (int) strlen(srules[j]->input))
					swap++;

			if (swap) {
				struct rule *temp;
				temp = srules[i];
				srules[i] = srules[j];
				srules[j] = temp;
			}
		}
	}
	for (i = 0; srules[i]; i++)
		srules[i]->next = srules[i + 1];
	rules = srules[0];
	(void) free(srules);
}
