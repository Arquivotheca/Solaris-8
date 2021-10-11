/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)cmdtab.c	1.6	99/08/17 SMI"	/* SVr4.0 1.1	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986-1990,1996  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */


#include "ftp_var.h"

/*
 * User FTP -- Command Tables.
 */

static char	accounthelp[] =	"send account command to remote server";
static char	appendhelp[] =	"append to a file";
static char	asciihelp[] =	"set ascii transfer type";
static char	beephelp[] =	"beep when command completed";
static char	binaryhelp[] =	"set binary transfer type";
static char	casehelp[] =	"toggle mget upper/lower case id mapping";
static char	cdhelp[] =	"change remote working directory";
static char	cduphelp[] = 	"change remote working directory to parent "
				"directory";
static char	connecthelp[] =	"connect to remote tftp";
static char	crhelp[] =	"toggle carriage return stripping on ascii "
				"gets";
static char	deletehelp[] =	"delete remote file";
static char	debughelp[] =	"toggle/set debugging mode";
static char	dirhelp[] =	"list contents of remote directory";
static char	disconhelp[] =	"terminate ftp session";
static char	domachelp[] = 	"execute macro";
static char	formhelp[] =	"set file transfer format";
static char	globhelp[] =	"toggle metacharacter expansion of local file "
				"names";
static char	hashhelp[] =	"toggle printing `#' for each buffer "
				"transferred";
static char	helphelp[] =	"print local help information";
static char	lcdhelp[] =	"change local working directory";
static char	lshelp[] =	"nlist contents of remote directory";
static char	macdefhelp[] =  "define a macro";
static char	mdeletehelp[] =	"delete multiple files";
static char	mdirhelp[] =	"list contents of multiple remote directories";
static char	mgethelp[] =	"get multiple files";
static char	mkdirhelp[] =	"make directory on the remote machine";
static char	mlshelp[] =	"nlist contents of multiple remote directories";
static char	modehelp[] =	"set file transfer mode";
static char	mputhelp[] =	"send multiple files";
static char	nmaphelp[] =	"set templates for default file name mapping";
static char	ntranshelp[] =	"set translation table for default file name "
				"mapping";
static char	porthelp[] =	"toggle use of PORT cmd for each data "
				"connection";
static char	prompthelp[] =	"force interactive prompting on multiple "
				"commands";
static char	proxyhelp[] =	"issue command on alternate connection";
static char	pwdhelp[] =	"print working directory on remote machine";
static char	quithelp[] =	"terminate ftp session and exit";
static char	quotehelp[] =	"send arbitrary ftp command";
static char	receivehelp[] =	"receive file";
static char	remotehelp[] =	"get help from remote server";
static char	renamehelp[] =	"rename file";
static char	rmdirhelp[] =	"remove directory on the remote machine";
static char	runiquehelp[] = "toggle store unique for local files";
static char	resethelp[] =	"clear queued command replies";
static char	sendhelp[] =	"send one file";
static char	shellhelp[] =	"escape to the shell";
static char	statushelp[] =	"show current status";
static char	structhelp[] =	"set file transfer structure";
static char	suniquehelp[] = "toggle store unique on remote machine";
static char	tenexhelp[] =	"set tenex file transfer type";
static char	tracehelp[] =	"toggle packet tracing";
static char	typehelp[] =	"set file transfer type";
static char	userhelp[] =	"send new user information";
static char	verbosehelp[] =	"toggle verbose mode";

struct cmd cmdtab[] = {
	{ "!",		shellhelp,	0,	0,	0,	shell },
	{ "$",		domachelp,	1,	0,	0,	domacro },
	{ "account",	accounthelp,	0,	1,	1,	account},
	{ "append",	appendhelp,	1,	1,	1,	put },
	{ "ascii",	asciihelp,	0,	1,	1,	setascii },
	{ "bell",	beephelp,	0,	0,	0,	setbell },
	{ "binary",	binaryhelp,	0,	1,	1,	setbinary },
	{ "bye",	quithelp,	0,	0,	0,	quit },
	{ "case",	casehelp,	0,	0,	1,	setcase },
	{ "cd",		cdhelp,		0,	1,	1,	cd },
	{ "cdup",	cduphelp,	0,	1,	1,	cdup },
	{ "close",	disconhelp,	0,	1,	1,	disconnect },
	{ "cr",		crhelp,		0,	0,	0,	setcr },
	{ "delete",	deletehelp,	0,	1,	1,	delete },
	{ "debug",	debughelp,	0,	0,	0,	setdebug },
	{ "dir",	dirhelp,	1,	1,	1,	ls },
	{ "disconnect",	disconhelp,	0,	1,	1,	disconnect },
	{ "form",	formhelp,	0,	1,	1,	setform },
	{ "get",	receivehelp,	1,	1,	1,	get },
	{ "glob",	globhelp,	0,	0,	0,	setglob },
	{ "hash",	hashhelp,	0,	0,	0,	sethash },
	{ "help",	helphelp,	0,	0,	1,	help },
	{ "lcd",	lcdhelp,	0,	0,	0,	lcd },
	{ "ls",		lshelp,		1,	1,	1,	ls },
	{ "macdef",	macdefhelp,	0,	0,	0,	macdef },
	{ "mdelete",	mdeletehelp,	1,	1,	1,	mdelete },
	{ "mdir",	mdirhelp,	1,	1,	1,	mls },
	{ "mget",	mgethelp,	1,	1,	1,	mget },
	{ "mkdir",	mkdirhelp,	0,	1,	1,	makedir },
	{ "mls",	mlshelp,	1,	1,	1,	mls },
	{ "mode",	modehelp,	0,	1,	1,	setmode },
	{ "mput",	mputhelp,	1,	1,	1,	mput },
	{ "nmap",	nmaphelp,	0,	0,	1,	setnmap },
	{ "ntrans",	ntranshelp,	0,	0,	1,	setntrans },
	{ "open",	connecthelp,	0,	0,	1,	setpeer },
	{ "prompt",	prompthelp,	0,	0,	0,	setprompt },
	{ "proxy",	proxyhelp,	0,	0,	1,	doproxy },
	{ "sendport",	porthelp,	0,	0,	0,	setport },
	{ "put",	sendhelp,	1,	1,	1,	put },
	{ "pwd",	pwdhelp,	0,	1,	1,	pwd },
	{ "quit",	quithelp,	0,	0,	0,	quit },
	{ "quote",	quotehelp,	1,	1,	1,	quote },
	{ "recv",	receivehelp,	1,	1,	1,	get },
	{ "remotehelp",	remotehelp,	0,	1,	1,	rmthelp },
	{ "rename",	renamehelp,	0,	1,	1,	renamefile },
	{ "reset",	resethelp,	0,	1,	1,	reset },
	{ "rmdir",	rmdirhelp,	0,	1,	1,	removedir },
	{ "runique",	runiquehelp,	0,	0,	1,	setrunique },
	{ "send",	sendhelp,	1,	1,	1,	put },
	{ "status",	statushelp,	0,	0,	1,	status },
	{ "struct",	structhelp,	0,	1,	1,	setstruct },
	{ "sunique",	suniquehelp,	0,	0,	1,	setsunique },
	{ "tenex",	tenexhelp,	0,	1,	1,	settenex },
	{ "trace",	tracehelp,	0,	0,	0,	settrace },
	{ "type",	typehelp,	0,	1,	1,	settype },
	{ "user",	userhelp,	0,	1,	1,	user },
	{ "verbose",	verbosehelp,	0,	0,	0,	setverbose },
	{ "?",		helphelp,	0,	0,	1,	help },
	{ 0 },
};

int	NCMDS = (sizeof (cmdtab) / sizeof (cmdtab[0])) - 1;
