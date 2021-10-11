/*
 * Copyright (c) 1991 Sun Microsystems, Inc.  All Rights Reserved. Sun
 * considers its source code as an unpublished, proprietary trade secret, and
 * it is available only under strict license provisions.  This copyright
 * notice is placed here only to protect Sun in the event the source is
 * deemed a published work.  Dissassembly, decompilation, or other means of
 * reducing the object code to human readable form is prohibited by the
 * license agreement under which this code is provided to the user or company
 * in possession of this copy.
 *
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the Government
 * is subject to restrictions as set forth in subparagraph (c)(1)(ii) of the
 * Rights in Technical Data and Computer Software clause at DFARS 52.227-7013
 * and in similar clauses in the FAR and NASA FAR Supplement.
 */

#pragma ident "@(#)tty_help.h   1.1     93/10/07 SMI"

#ifndef _TTY_HELP_H
#define	_TTY_HELP_H

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum {
		HELP_NONE = -1,
		HELP_TOPIC = 'C',
		HELP_HOWTO = 'P',
		HELP_REFER = 'R'
	} help_t;

	typedef struct {
		help_t	type;
		char	*title;
	} HelpEntry;

	extern void do_help_index(WINDOW *, help_t, char *);

	extern char *helpdir;
	extern HelpEntry toplevel_help;

#ifdef __cplusplus
}
#endif

#endif
