/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)form.c	1.10	93/10/21 SMI"	/* SVr4.0 1.9	*/

#include <locale.h>
#include "stdio.h"

#include "string.h"

#include "lp.h"
#include "form.h"
#include "access.h"
#include "msgs.h"

#define	WHO_AM_I	I_AM_LPSTAT
#include "oam.h"

#include "lpstat.h"

static void		putfline ( FORM * );

/**
 ** do_form()
 **/

void
do_form(char **list)
{
	FORM			form;

	while (*list) {
		if (STREQU(NAME_ALL, *list))
			while (getform(NAME_ALL, &form, (FALERT *)0, (FILE **)0) != -1)
				putfline (&form);

		else if (getform(*list, &form, (FALERT *)0, (FILE **)0) != -1) {
			putfline (&form);

		} else {
			LP_ERRMSG1 (ERROR, E_LP_NOFORM, *list);
			exit_rc = 1;
		}

		list++;
	}
	printsdn_unsetup ();
	return;
}

/**
 ** putfline()
 **/

static void
putfline(FORM *pf)
{
	register MOUNTED	*pm;


	(void) printf(gettext("form %s"), pf->name);

	(void) printf(gettext(" is %s to you"),
			is_user_allowed_form(getname(), pf->name) ?
				gettext( "available") :
				gettext("not available"));

	for (pm = mounted_forms; pm->forward; pm = pm->forward)
		if (STREQU(pm->name, pf->name)) {
			if (pm->printers) {
				(void) printf(gettext(", mounted on "));
				printlist_setup (0, 0, ",", "");
				printlist (stdout, pm->printers);
				printlist_unsetup();
			}
			break;
		}

	(void) printf("\n");

	if (verbosity & V_LONG) {

		printsdn_setup (gettext("\tPage length: "), 0, 0);
		printsdn (stdout, pf->plen);

		printsdn_setup (gettext("\tPage width: "), 0, 0);
		printsdn (stdout, pf->pwid);

		(void) printf(gettext("\tNumber of pages: %d\n"), pf->np);

		printsdn_setup (gettext("\tLine pitch: "), 0, 0);
		printsdn (stdout, pf->lpi);

		(void) printf(gettext("\tCharacter pitch:"));
		if (pf->cpi.val == N_COMPRESSED)
			(void) printf(" %s\n", NAME_COMPRESSED);
		else {
			printsdn_setup (" ", 0, 0);
			printsdn (stdout, pf->cpi);
		}

		(void) printf(gettext("\tCharacter set choice: %s%s\n"),
			(pf->chset? pf->chset : NAME_ANY),
			(pf->mandatory ? ", mandatory" : ""));

		(void) printf(gettext("\tRibbon color: %s\n"),
			(pf->rcolor? pf->rcolor : NAME_ANY));

		if (pf->paper)
			(void) printf(gettext("\tpaper: %s\n"), pf->paper);

		if (pf->comment)
			(void) printf(gettext("\tComment:\n\t%s\n"),
				pf->comment);
	}
	return;
}

/**
 ** do_paper()
 **/

void
do_paper(char **list)
{
	while (*list) {
		if (STREQU(NAME_ALL, *list)) {
			send_message (S_PAPER_ALLOWED, "");
			(void)output(R_PAPER_ALLOWED);
		} else {
			send_message (S_PAPER_ALLOWED, *list);
			switch (output(R_PAPER_ALLOWED)) {
			case MNODEST:
				LP_ERRMSG1 (ERROR, E_LP_NOPRINTER, *list);
				exit_rc = 1;
				break;
			}
		}

		list++;
	}
	printsdn_unsetup ();
}

/**
 ** putppline()
 **/

void
putppline(char *printer, char *paperAllowed)
{
   char *ptr;

	ptr = paperAllowed;
	while (ptr = strchr(ptr,' ')) {
		*ptr = ',';
		ptr++;
	}
	(void) printf(gettext("paper allowed for printer %s: %s\n"), printer,
		  paperAllowed);
}
