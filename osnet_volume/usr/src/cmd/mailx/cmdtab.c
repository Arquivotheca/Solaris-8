/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)cmdtab.c	1.18	95/08/30 SMI"	/* from SVr4.0 1.3.2.2 */

#include "def.h"

/*
 * mailx -- a modified version of a University of California at Berkeley
 *	mail program
 *
 * Define all of the command names and bindings.
 */

/*
 * Since the type of the argument to the procedures in the
 * command table depends on the flags in the table, and since
 * the argument type must be specified in struct cmd for C++
 * compatibility, and since I didn't want to change all the
 * command procedures to expect an argument of type void *,
 * the following cast "makes it all work".  Yes, it's ugly.
 */
#define	C	(int (*)(void *))

extern const struct cmd cmdtab[] = {
	"next",		C next,		NDMLIST,	0,	MMNDEL,
	"alias",	C group,	M|RAWLIST,	0,	1000,
	"print",	C type,		MSGLIST,	0,	MMNDEL,
	"type",		C type,		MSGLIST,	0,	MMNDEL,
	"Type",		C Type,		MSGLIST,	0,	MMNDEL,
	"Print",	C Type,		MSGLIST,	0,	MMNDEL,
	"visual",	C visual,	I|MSGLIST,	0,	MMNORM,
	"top",		C top,		MSGLIST,	0,	MMNDEL,
	"touch",	C stouch,	W|MSGLIST,	0,	MMNDEL,
	"preserve",	C preserve,	I|W|MSGLIST,	0,	MMNDEL,
	"delete",	C delm,		W|P|MSGLIST,	0,	MMNDEL,
	"dp",		C deltype,	W|MSGLIST,	0,	MMNDEL,
	"dt",		C deltype,	W|MSGLIST,	0,	MMNDEL,
	"undelete",	C undelete,	P|MSGLIST,	MDELETED,MMNDEL,
	"unset",	C unset,	M|RAWLIST,	1,	1000,
	"mail",		C sendm,	R|M|I|STRLIST,	0,	0,
	"Mail",		C Sendm,	R|M|I|STRLIST,	0,	0,
	"mbox",		C mboxit,	W|MSGLIST,	0,	0,
	"more",		C more,		MSGLIST,	0,	MMNDEL,
	"page",		C more,		MSGLIST,	0,	MMNDEL,
	"More",		C More,		MSGLIST,	0,	MMNDEL,
	"Page",		C More,		MSGLIST,	0,	MMNDEL,
	"unread",	C unread,	MSGLIST,	0,	MMNDEL,
	"Unread",	C unread,	MSGLIST,	0,	MMNDEL,
	"new",		C unread,	MSGLIST,	0,	MMNDEL,
	"New",		C unread,	MSGLIST,	0,	MMNDEL,
	"!",		C shell,	I|STRLIST,	0,	0,
	"copy",		C copycmd,	M|STRLIST,	0,	0,
	"Copy",		C Copy,		M|MSGLIST,	0,	0,
	"chdir",	C schdir,	M|STRLIST,	0,	0,
	"cd",		C schdir,	M|STRLIST,	0,	0,
	"save",		C save,		STRLIST,	0,	0,
	"Save",		C Save,		MSGLIST,	0,	0,
	"source",	C source,	M|STRLIST,	0,	0,
	"set",		C set,		M|RAWLIST,	0,	1000,
	"shell",	C dosh,		I|NOLIST,	0,	0,
	"version",	C pversion,	M|NOLIST,	0,	0,
	"group",	C group,	M|RAWLIST,	0,	1000,
	"unalias",	C ungroup,	M|RAWLIST,	0,	1000,
	"ungroup",	C ungroup,	M|RAWLIST,	0,	1000,
	"write",	C swrite,	STRLIST,	0,	0,
	"from",		C from,		MSGLIST,	0,	MMNORM,
	"followup",	C followup,	R|I|MSGLIST,	0,	MMNDEL,
	"Followup",	C Followup,	R|I|MSGLIST,	0,	MMNDEL,
	"file",		C file,		T|M|RAWLIST,	0,	1,
	"folder",	C file,		T|M|RAWLIST,	0,	1,
	"folders",	C folders,	T|M|RAWLIST,	0,	1,
	"?",		C help,		M|NOLIST,	0,	0,
	"z",		C scroll,	M|STRLIST,	0,	0,
	"headers",	C headers,	MSGLIST,	0,	MMNDEL,
	"help",		C help,		M|NOLIST,	0,	0,
	"=",		C pdot,		NOLIST,		0,	0,
	"Reply",	C Respond,	R|I|MSGLIST,	0,	MMNDEL,
	"Respond",	C Respond,	R|I|MSGLIST,	0,	MMNDEL,
	"reply",	C respond,	R|I|MSGLIST,	0,	MMNDEL,
	"respond",	C respond,	R|I|MSGLIST,	0,	MMNDEL,
	"replyall",	C replyall,	R|I|MSGLIST,	0,	MMNDEL,
	"replysender",	C replysender,	R|I|MSGLIST,	0,	MMNDEL,
	"edit",		C editor,	I|MSGLIST,	0,	MMNORM,
	"echo",		C echo,		M|RAWLIST,	0,	1000,
	"quit",		C edstop,	NOLIST, 	0,	0,
	"list",		C pcmdlist,	M|NOLIST,	0,	0,
	"load",		C loadmsg,	W|STRLIST,	0,	0,
	"xit",		C rexit,	M|NOLIST,	0,	0,
	"exit",		C rexit,	M|NOLIST,	0,	0,
	"size",		C messize,	MSGLIST,	0,	MMNDEL,
	"hold",		C preserve,	I|W|MSGLIST,	0,	MMNDEL,
	"if",		C ifcmd,	F|M|RAWLIST,	1,	1,
	"else",		C elsecmd,	F|M|RAWLIST,	0,	0,
	"endif",	C endifcmd,	F|M|RAWLIST,	0,	0,
	"alternates",	C alternates,	M|RAWLIST,	0,	1000,
	"ignore",	C igfield,	M|RAWLIST,	0,	1000,
	"discard",	C igfield,	M|RAWLIST,	0,	1000,
	"unignore",	C unigfield,	M|RAWLIST,	0,	1000,
	"undiscard",	C unigfield,	M|RAWLIST,	0,	1000,
	"retain",	C retfield,	M|RAWLIST,	0,	1000,
	"unretain",	C unretfield,	M|RAWLIST,	0,	1000,
/*	"Header",	C Header,	STRLIST,	0,	1000,	*/
	"#",		C null,		M|NOLIST,	0,	0,
	"pipe",		C dopipe,	STRLIST,	0,	0,
	"|",		C dopipe,	STRLIST,	0,	0,
	"inc",		C inc,		T|NOLIST,	0,	0,
	"field",	C field,	STRLIST,	0,	0,
	"put",		C sput,		STRLIST,	0,	0,
	"Put",		C Sput,		STRLIST,	0,	0,
	0,		C 0,		0,		0,	0
};
