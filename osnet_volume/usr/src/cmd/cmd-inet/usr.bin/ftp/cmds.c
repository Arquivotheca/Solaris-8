/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)cmds.c 1.25	99/10/20 SMI"

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
 * 	(c) 1986-1993,1995-1997,1999  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */


/*
 * FTP User Program -- Command Routines.
 */
#include "ftp_var.h"

static char *mname;
static jmp_buf jabort;
static jmp_buf abortprox;


static char *remglob(char *argv[], int doswitch);
static char *onoff(int bool);
static int confirm(char *cmd, char *file);
static int globulize(char **cpp);
static void proxabort(int sig);
static void mabort(int sig);
static char *dotrans(char *name);
static char *domap(char *name);


/* Prompt for command argument, add to buffer with space separator */
static int
prompt_for_arg(char *buffer, int buffer_size, char *prompt)
{
	if (strlen(buffer) > buffer_size - 2) {
		printf("Line too long\n");
		return (-1);
	}
	strcat(buffer, " ");
	stop_timer();
	printf("(%s) ", prompt);
	if (fgets(buffer + strlen(buffer), buffer_size - strlen(buffer), stdin)
	    == NULL) {
		reset_timer();
		return (-1);
	}

	/* Flush what didn't fit in the buffer */
	if (buffer[strlen(buffer)-1] != '\n') {
		while (fgetc(stdin) != '\n' && !ferror(stdin) && !feof(stdin))
			;
		printf("Line too long\n");
		reset_timer();
		return (-1);
	} else
		buffer[strlen(buffer)-1] = 0;

	reset_timer();
	return (0);
}


/*
 * Connect to peer server and
 * auto-login, if possible.
 */
void
setpeer(int argc, char *argv[])
{
	char *host;
	ushort_t port;

	if (connected) {
		printf("Already connected to %s, use close first.\n",
			hostname);
		code = -1;
		return;
	}
	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "to") == -1) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc > 3 || argc < 2) {
		printf("usage: %s host-name [port]\n", argv[0]);
		code = -1;
		return;
	}
	port = sp->s_port;
	if (argc > 2) {
		port = atoi(argv[2]);
		if (port == 0) {
			printf("%s: bad port number-- %s\n", argv[1], argv[2]);
			printf("usage: %s host-name [port]\n", argv[0]);
			code = -1;
			return;
		}
		port = htons(port);
	}
	strcpy(typename, "ascii");
	host = hookup(argv[1], port);
	if (host) {
		connected = 1;
		if (autologin)
			(void) login(argv[1]);
	}
}

static struct types {
	char	*t_name;
	char	*t_mode;
	int	t_type;
	char	*t_arg;
} types[] = {
	{ "ascii",	"A",	TYPE_A,	0 },
	{ "binary",	"I",	TYPE_I,	0 },
	{ "image",	"I",	TYPE_I,	0 },
	{ "ebcdic",	"E",	TYPE_E,	0 },
	{ "tenex",	"L",	TYPE_L,	bytename },
	0
};

/*
 * Set transfer type.
 */
void
settype(int argc, char *argv[])
{
	register struct types *p;
	int comret;

	if (argc > 2) {
		char *sep;

		printf("usage: %s [", argv[0]);
		sep = " ";
		for (p = types; p->t_name; p++) {
			printf("%s%s", sep, p->t_name);
			if (*sep == ' ')
				sep = " | ";
		}
		printf(" ]\n");
		code = -1;
		return;
	}
	if (argc < 2) {
		printf("Using %s mode to transfer files.\n", typename);
		code = 0;
		return;
	}
	for (p = types; p->t_name; p++)
		if (strcmp(argv[1], p->t_name) == 0)
			break;
	if (p->t_name == 0) {
		printf("%s: unknown mode\n", argv[1]);
		code = -1;
		return;
	}
	if ((p->t_arg != NULL) && (*(p->t_arg) != '\0'))
		comret = command("TYPE %s %s", p->t_mode, p->t_arg);
	else
		comret = command("TYPE %s", p->t_mode);
	if (comret == COMPLETE) {
		(void) strcpy(typename, p->t_name);
		type = p->t_type;
	}
}

/*
 * Set binary transfer type.
 */
/*ARGSUSED*/
void
setbinary(int argc, char *argv[])
{
	call(settype, "type", "binary", 0);
}

/*
 * Set ascii transfer type.
 */
/*ARGSUSED*/
void
setascii(int argc, char *argv[])
{
	call(settype, "type", "ascii", 0);
}

/*
 * Set tenex transfer type.
 */
/*ARGSUSED*/
void
settenex(int argc, char *argv[])
{
	call(settype, "type", "tenex", 0);
}

/*
 * Set ebcdic transfer type.
 */
/*ARGSUSED*/
void
setebcdic(int argc, char *argv[])
{
	call(settype, "type", "ebcdic", 0);
}

/*
 * Set file transfer mode.
 */
/*ARGSUSED*/
void
setmode(int argc, char *argv[])
{
	printf("We only support %s mode, sorry.\n", modename);
	code = -1;
}

/*
 * Set file transfer format.
 */
/*ARGSUSED*/
void
setform(int argc, char *argv[])
{
	printf("We only support %s format, sorry.\n", formname);
	code = -1;
}

/*
 * Set file transfer structure.
 */
/*ARGSUSED*/
void
setstruct(int argc, char *argv[])
{

	printf("We only support %s structure, sorry.\n", structname);
	code = -1;
}

/*
 * Send a single file.
 */
void
put(int argc, char *argv[])
{
	char *cmd;
	int loc = 0;
	char *oldargv1;

	if (argc == 2) {
		argc++;
		argv[2] = argv[1];
		loc++;
	}
	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "local-file") == -1) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
usage:
		printf("usage:%s local-file remote-file\n", argv[0]);
		code = -1;
		return;
	}
	if (argc < 3) {
		if (prompt_for_arg(line, sizeof (line), "remote-file") == -1) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 3)
		goto usage;
	oldargv1 = argv[1];
	if (!globulize(&argv[1])) {
		code = -1;
		return;
	}
	/*
	 * If "globulize" modifies argv[1], and argv[2] is a copy of
	 * the old argv[1], make it a copy of the new argv[1].
	 */
	if (argv[1] != oldargv1 && argv[2] == oldargv1) {
		argv[2] = argv[1];
	}
	cmd = (argv[0][0] == 'a') ? "APPE" : ((sunique) ? "STOU" : "STOR");
	if (loc && ntflag) {
		argv[2] = dotrans(argv[2]);
	}
	if (loc && mapflag) {
		argv[2] = domap(argv[2]);
	}
	sendrequest(cmd, argv[1], argv[2], 1);
}

/*ARGSUSED*/
static void
mabort(int sig)
{
	int ointer;

	printf("\n");
	(void) fflush(stdout);
	if (mflag && fromatty) {
		ointer = interactive;
		interactive = 1;
		if (confirm("Continue with", mname)) {
			interactive = ointer;
			longjmp(jabort, 0);
		}
		interactive = ointer;
	}
	mflag = 0;
	longjmp(jabort, 0);
}

/*
 * Send multiple files.
 */
void
mput(int argc, char *argv[])
{
	register int i;
	int ointer;
	void (*oldintr)();
	char *tp;
	int	len;

	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "local-files") == -1) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
		printf("usage:%s local-files\n", argv[0]);
		code = -1;
		return;
	}
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void) setjmp(jabort);
	if (proxy) {
		char *cp, *tp2, tmpbuf[MAXPATHLEN];

		while ((cp = remglob(argv, 0)) != NULL) {
			if (*cp == 0) {
				mflag = 0;
				continue;
			}
			if (mflag && confirm(argv[0], cp)) {
				tp = cp;
				if (mcase) {
					while (*tp) {
						if ((len =
						    mblen(tp, MB_CUR_MAX)) <= 0)
							len = 1;
						if (islower(*tp))
							break;
						tp += len;
					}
					if (!*tp) {
						tp = cp;
						tp2 = tmpbuf;
						while (*tp) {
							if ((len = mblen(tp,
							    MB_CUR_MAX)) <= 0)
								len = 1;
							memcpy(tp2, tp, len);
							if (isupper(*tp2)) {
								*tp2 = 'a' +
								    *tp2 - 'A';
							}
							tp += len;
							tp2 += len;
						}
						*tp2 = 0;
						tp = tmpbuf;
					}
				}
				if (ntflag) {
					tp = dotrans(tp);
				}
				if (mapflag) {
					tp = domap(tp);
				}
				sendrequest((sunique) ? "STOU" : "STOR",
				    cp, tp, 0);
				if (!mflag && fromatty) {
					ointer = interactive;
					interactive = 1;
					if (confirm("Continue with", "mput")) {
						mflag++;
					}
					interactive = ointer;
				}
			}
		}
		(void) signal(SIGINT, oldintr);
		mflag = 0;
		return;
	}
	for (i = 1; i < argc; i++) {
		register char **cpp, **gargs;

		if (!doglob) {
			if (mflag && confirm(argv[0], argv[i])) {
				tp = (ntflag) ? dotrans(argv[i]) : argv[i];
				tp = (mapflag) ? domap(tp) : tp;
				sendrequest((sunique) ? "STOU" : "STOR",
				    argv[i], tp, 1);
				if (!mflag && fromatty) {
					ointer = interactive;
					interactive = 1;
					if (confirm("Continue with", "mput")) {
						mflag++;
					}
					interactive = ointer;
				}
			}
			continue;
		}
		gargs = glob(argv[i]);
		if (globerr != NULL) {
			printf("%s\n", globerr);
			if (gargs)
				blkfree(gargs);
			continue;
		}
		for (cpp = gargs; cpp && *cpp != NULL; cpp++) {
			if (mflag && confirm(argv[0], *cpp)) {
				tp = (ntflag) ? dotrans(*cpp) : *cpp;
				tp = (mapflag) ? domap(tp) : tp;
				sendrequest((sunique) ? "STOU" : "STOR",
				    *cpp, tp, 0);
				if (!mflag && fromatty) {
					ointer = interactive;
					interactive = 1;
					if (confirm("Continue with", "mput")) {
						mflag++;
					}
					interactive = ointer;
				}
			}
		}
		if (gargs != NULL)
			blkfree(gargs);
	}
	(void) signal(SIGINT, oldintr);
	mflag = 0;
}

/*
 * Receive one file.
 */
void
get(int argc, char *argv[])
{
	int loc = 0;
	int len;
	int allowpipe = 1;

	if (argc == 2) {
		argc++;
		argv[2] = argv[1];
		/* Only permit !file if two arguments. */
		allowpipe = 0;
		loc++;
	}
	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "remote-file") == -1) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
usage:
		printf("usage: %s remote-file [ local-file ]\n", argv[0]);
		code = -1;
		return;
	}
	if (argc < 3) {
		if (prompt_for_arg(line, sizeof (line), "local-file") == -1) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 3)
		goto usage;
	if (!globulize(&argv[2])) {
		code = -1;
		return;
	}
	if (loc && mcase) {
		char *tp = argv[1], *tp2, tmpbuf[MAXPATHLEN];

		while (*tp) {
			if ((len = mblen(tp, MB_CUR_MAX)) <= 0)
				len = 1;
			if (islower(*tp))
				break;
			tp += len;
		}
		if (!*tp) {
			tp = argv[2];
			tp2 = tmpbuf;
			while (*tp) {
				if ((len = mblen(tp, MB_CUR_MAX)) <= 0)
					len = 1;
				memcpy(tp2, tp, len);
				if (isupper(*tp2))
					*tp2 = 'a' + *tp2 - 'A';
				tp += len;
				tp2 += len;
			}
			*tp2 = 0;
			argv[2] = tmpbuf;
		}
	}
	if (loc && ntflag) {
		argv[2] = dotrans(argv[2]);
	}
	if (loc && mapflag) {
		argv[2] = domap(argv[2]);
	}
	recvrequest("RETR", argv[2], argv[1], "w", allowpipe);
}

/*
 * Get multiple files.
 */
void
mget(int argc, char *argv[])
{
	char *cp, *tp, *tp2, tmpbuf[MAXPATHLEN];
	int ointer;
	void (*oldintr)();
	int need_convert;
	int	len;

	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "remote-files") < 0) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
		printf("usage:%s remote-files\n", argv[0]);
		code = -1;
		return;
	}
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void) setjmp(jabort);
	while ((cp = remglob(argv, proxy)) != NULL) {
		if (*cp == '\0') {
			mflag = 0;
			continue;
		}
		if (mflag && confirm(argv[0], cp)) {
			strcpy(tmpbuf, cp);
			tp =  tmpbuf;
			need_convert = 1;
			if (mcase) {
				tp2 = tp;
				while (*tp2 && need_convert) {
				/* Need any case convert? */
					if (islower(*tp2))
						need_convert = 0;
					if ((len = mblen(tp2, MB_CUR_MAX)) <= 0)
						len = 1;
					tp2 += len;
				}
				tp2 = tp;
				while (need_convert && *tp2) {
				/* Convert to lower case */
					if (isupper(*tp2))
						*tp2 = tolower(*tp2);
					if ((len = mblen(tp2, MB_CUR_MAX)) <= 0)
						len = 1;
					tp2 += len;
				}
			}

			if (ntflag) {
				tp = dotrans(tp);
			}
			if (mapflag) {
				tp = domap(tp);
			}
			recvrequest("RETR", tp, cp, "w", 0);
			if (!mflag && fromatty) {
				ointer = interactive;
				interactive = 1;
				if (confirm("Continue with", "mget")) {
					mflag++;
				}
				interactive = ointer;
			}
		}
	}
	(void) signal(SIGINT, oldintr);
	mflag = 0;
}

static char *
remglob(char *argv[], int doswitch)
{
	char temp[16];
	static char buf[MAXPATHLEN];
	static FILE *ftemp = NULL;
	static char **args;
	int oldverbose, oldhash;
	char *cp, *mode;

	if (!mflag) {
		if (!doglob) {
			args = NULL;
		} else {
			if (ftemp) {
				(void) fclose(ftemp);
				ftemp = NULL;
			}
		}
		return (NULL);
	}
	if (!doglob) {
		if (args == NULL)
			args = argv;
		if ((cp = *++args) == NULL)
			args = NULL;
		return (cp);
	}
	if (ftemp == NULL) {
		(void) strcpy(temp, "/tmp/ftpXXXXXX");
		(void) mktemp(temp);
		oldverbose = verbose, verbose = 0;
		oldhash = hash, hash = 0;
		if (doswitch) {
			pswitch(!proxy);
		}
		for (mode = "w"; *++argv != NULL; mode = "a")
			recvrequest("NLST", temp, *argv, mode, 0);
		if (doswitch) {
			pswitch(!proxy);
		}
		verbose = oldverbose; hash = oldhash;
		ftemp = fopen(temp, "r");
		(void) unlink(temp);
		if (ftemp == NULL) {
			printf("can't find list of remote files, oops\n");
			return (NULL);
		}
	}
	reset_timer();
	if (fgets(buf, sizeof (buf), ftemp) == NULL) {
		(void) fclose(ftemp), ftemp = NULL;
		return (NULL);
	}
	if ((cp = index(buf, '\n')) != NULL)
		*cp = '\0';
	return (buf);
}

static char *
onoff(int bool)
{
	return (bool ? "on" : "off");
}

/*
 * Show status.
 */
/*ARGSUSED*/
void
status(int argc, char *argv[])
{
	int i;

	if (connected)
		printf("Connected to %s.\n", hostname);
	else
		printf("Not connected.\n");
	if (!proxy) {
		pswitch(1);
		if (connected) {
			printf("Connected for proxy commands to %s.\n",
			    hostname);
		} else {
			printf("No proxy connection.\n");
		}
		pswitch(0);
	}
	printf("Mode: %s; Type: %s; Form: %s; Structure: %s\n",
		modename, typename, formname, structname);
	printf("Verbose: %s; Bell: %s; Prompting: %s; Globbing: %s\n",
		onoff(verbose), onoff(bell), onoff(interactive),
		onoff(doglob));
	printf("Store unique: %s; Receive unique: %s\n", onoff(sunique),
		onoff(runique));
	printf("Case: %s; CR stripping: %s\n", onoff(mcase), onoff(crflag));
	if (ntflag) {
		printf("Ntrans: (in) %s (out) %s\n", ntin, ntout);
	} else {
		printf("Ntrans: off\n");
	}
	if (mapflag) {
		printf("Nmap: (in) %s (out) %s\n", mapin, mapout);
	} else {
		printf("Nmap: off\n");
	}
	printf("Hash mark printing: %s; Use of PORT cmds: %s\n",
		onoff(hash), onoff(sendport));
	if (macnum > 0) {
		printf("Macros:\n");
		for (i = 0; i < macnum; i++) {
			printf("\t%s\n", macros[i].mac_name);
		}
	}
	code = 0;
}

/*
 * Set beep on cmd completed mode.
 */
/*ARGSUSED*/
void
setbell(int argc, char *argv[])
{
	bell = !bell;
	printf("Bell mode %s.\n", onoff(bell));
	code = bell;
}

/*
 * Turn on packet tracing.
 */
/*ARGSUSED*/
void
settrace(int argc, char *argv[])
{
	trace = !trace;
	printf("Packet tracing %s.\n", onoff(trace));
	code = trace;
}

/*
 * Toggle hash mark printing during transfers.
 */
/*ARGSUSED*/
void
sethash(int argc, char *argv[])
{
	hash = !hash;
	printf("Hash mark printing %s", onoff(hash));
	code = hash;
	if (hash)
		printf(" (%d bytes/hash mark)", BUFSIZ * 8);
	printf(".\n");
}

/*
 * Turn on printing of server echo's.
 */
/*ARGSUSED*/
void
setverbose(int argc, char *argv[])
{
	verbose = !verbose;
	printf("Verbose mode %s.\n", onoff(verbose));
	code = verbose;
}

/*
 * Toggle PORT cmd use before each data connection.
 */
/*ARGSUSED*/
void
setport(int argc, char *argv[])
{
	sendport = !sendport;
	printf("Use of PORT cmds %s.\n", onoff(sendport));
	code = sendport;
}

/*
 * Turn on interactive prompting
 * during mget, mput, and mdelete.
 */
/*ARGSUSED*/
void
setprompt(int argc, char *argv[])
{
	interactive = !interactive;
	printf("Interactive mode %s.\n", onoff(interactive));
	code = interactive;
}

/*
 * Toggle metacharacter interpretation
 * on local file names.
 */
/*ARGSUSED*/
void
setglob(int argc, char *argv[])
{
	doglob = !doglob;
	printf("Globbing %s.\n", onoff(doglob));
	code = doglob;
}

/*
 * Set debugging mode on/off and/or
 * set level of debugging.
 */
void
setdebug(int argc, char *argv[])
{
	int val;

	if (argc > 1) {
		val = atoi(argv[1]);
		if (val < 0) {
			printf("%s: bad debugging value.\n", argv[1]);
			code = -1;
			return;
		}
	} else
		val = !debug;
	debug = val;
	if (debug)
		options |= SO_DEBUG;
	else
		options &= ~SO_DEBUG;
	printf("Debugging %s (debug=%d).\n", onoff(debug), debug);
	code = debug > 0;
}

/*
 * Set current working directory
 * on remote machine.
 */
void
cd(int argc, char *argv[])
{
	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "remote-directory") <
		    0) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
		printf("usage:%s remote-directory\n", argv[0]);
		code = -1;
		return;
	}
	(void) command("CWD %s", argv[1]);
}

/*
 * Set current working directory
 * on local machine.
 */
void
lcd(int argc, char *argv[])
{
	char buf[MAXPATHLEN], *bufptr;

	if (argc < 2)
		argc++, argv[1] = home;
	if (argc != 2) {
		printf("usage:%s local-directory\n", argv[0]);
		code = -1;
		return;
	}
	if (!globulize(&argv[1])) {
		code = -1;
		return;
	}
	if (chdir(argv[1]) < 0) {
		perror(argv[1]);
		code = -1;
		return;
	}
	bufptr = getcwd(buf, MAXPATHLEN);
	/*
	 * Even though chdir may succeed, getcwd may fail if a component
	 * of the pwd is unreadable. In this case, print the argument to
	 * chdir as the resultant directory, since we know it succeeded above.
	 */
	printf("Local directory now %s\n", (bufptr ? bufptr : argv[1]));
	code = 0;
}

/*
 * Delete a single file.
 */
void
delete(int argc, char *argv[])
{

	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "remote-file") < 0) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
		printf("usage:%s remote-file\n", argv[0]);
		code = -1;
		return;
	}
	(void) command("DELE %s", argv[1]);
}

/*
 * Delete multiple files.
 */
void
mdelete(int argc, char *argv[])
{
	char *cp;
	int ointer;
	void (*oldintr)();

	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "remote-files") < 0) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
		printf("usage:%s remote-files\n", argv[0]);
		code = -1;
		return;
	}
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void) setjmp(jabort);
	while ((cp = remglob(argv, 0)) != NULL) {
		if (*cp == '\0') {
			mflag = 0;
			continue;
		}
		if (mflag && confirm(argv[0], cp)) {
			(void) command("DELE %s", cp);
			if (!mflag && fromatty) {
				ointer = interactive;
				interactive = 1;
				if (confirm("Continue with", "mdelete")) {
					mflag++;
				}
				interactive = ointer;
			}
		}
	}
	(void) signal(SIGINT, oldintr);
	mflag = 0;
}

/*
 * Rename a remote file.
 */
void
renamefile(int argc, char *argv[])
{

	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "from-name") < 0) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
usage:
		printf("%s from-name to-name\n", argv[0]);
		code = -1;
		return;
	}
	if (argc < 3) {
		if (prompt_for_arg(line, sizeof (line), "to-name") < 0) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 3)
		goto usage;
	if (command("RNFR %s", argv[1]) == CONTINUE)
		(void) command("RNTO %s", argv[2]);
}

/*
 * Get a directory listing
 * of remote files.
 */
void
ls(int argc, char *argv[])
{
	char *cmd;

	if (argc < 2)
		argc++, argv[1] = NULL;
	if (argc < 3)
		argc++, argv[2] = "-";
	if (argc > 3) {
		printf("usage: %s remote-directory local-file\n", argv[0]);
		code = -1;
		return;
	}
	cmd = argv[0][0] == 'l' ? "NLST" : "LIST";
	if (strcmp(argv[2], "-") && !globulize(&argv[2])) {
		code = -1;
		return;
	}
	recvrequest(cmd, argv[2], argv[1], "w", 1);
}

/*
 * Get a directory listing
 * of multiple remote files.
 */
void
mls(int argc, char *argv[])
{
	char *cmd, mode[1], *dest;
	int ointer, i;
	void (*oldintr)();

	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "remote-files") < 0) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 3) {
		if (prompt_for_arg(line, sizeof (line), "local-file") < 0) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 3) {
		printf("usage:%s remote-files local-file\n", argv[0]);
		code = -1;
		return;
	}
	dest = argv[argc - 1];
	argv[argc - 1] = NULL;
	if (strcmp(dest, "-") && *dest != '|')
		if (!globulize(&dest) ||
		    !confirm("output to local-file:", dest)) {
			code = -1;
			return;
		}
	cmd = argv[0][1] == 'l' ? "NLST" : "LIST";
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void) setjmp(jabort);
	for (i = 1; mflag && i < argc-1; ++i) {
		*mode = (i == 1) ? 'w' : 'a';
		recvrequest(cmd, dest, argv[i], mode, 1);
		if (!mflag && fromatty) {
			ointer = interactive;
			interactive = 1;
			if (confirm("Continue with", argv[0])) {
				mflag ++;
			}
			interactive = ointer;
		}
	}
	(void) signal(SIGINT, oldintr);
	mflag = 0;
}

/*
 * Do a shell escape
 */
/*ARGSUSED*/
void
shell(int argc, char *argv[])
{
	pid_t pid;
	int i;
	void (*old1)(), (*old2)();
	char *shellstring, *namep;
	int status;

	stop_timer();
	old1 = signal(SIGINT, SIG_IGN);
	old2 = signal(SIGQUIT, SIG_IGN);
	if ((pid = fork()) == 0) {
		for (i = 3; i < 20; i++)
			(void) close(i);
		(void) signal(SIGINT, SIG_DFL);
		(void) signal(SIGQUIT, SIG_DFL);
		shellstring = getenv("SHELL");
		if (shellstring == NULL)
			shellstring = "/bin/sh";
		namep = rindex(shellstring, '/');
		if (namep == NULL)
			namep = shellstring;
		if (argc > 1) {
			if (debug) {
				printf("%s -c %s\n", shellstring, altarg);
				(void) fflush(stdout);
			}
			execl(shellstring, namep, "-c", altarg, (char *)0);
		} else {
			if (debug) {
				printf("%s\n", shellstring);
				(void) fflush(stdout);
			}
			execl(shellstring, namep, (char *)0);
		}
		perror(shellstring);
		code = -1;
		exit(1);
		}
	if (pid > 0)
		while (wait(&status) != pid)
			;
	(void) signal(SIGINT, old1);
	(void) signal(SIGQUIT, old2);
	reset_timer();
	if (pid == (pid_t)-1) {
		perror("Try again later");
		code = -1;
	} else {
		code = 0;
	}
}

/*
 * Send new user information (re-login)
 */
void
user(int argc, char *argv[])
{
	char acct[80];
	int n, aflag = 0;

	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "username") < 0) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc > 4) {
		printf("usage: %s username [password] [account]\n", argv[0]);
		code = -1;
		return;
	}
	if (argv[1] == 0) {
		printf("access for user (nil) denied\n");
		code = -1;
		return;
	}
	n = command("USER %s", argv[1]);
	if (n == CONTINUE) {
		if (argc < 3)
			argv[2] = mygetpass("Password: "), argc++;
		n = command("PASS %s", argv[2]);
	}
	if (n == CONTINUE) {
		if (argc < 4) {
			printf("Account: "); (void) fflush(stdout);
			stop_timer();
			(void) fgets(acct, sizeof (acct) - 1, stdin);
			reset_timer();
			acct[strlen(acct) - 1] = '\0';
			argv[3] = acct; argc++;
		}
		n = command("ACCT %s", argv[3]);
		aflag++;
	}
	if (n != COMPLETE) {
		fprintf(stdout, "Login failed.\n");
		return;
	}
	if (!aflag && argc == 4) {
		(void) command("ACCT %s", argv[3]);
	}
}

/*
 * Print working directory.
 */
/*ARGSUSED*/
void
pwd(int argc, char *argv[])
{
	(void) command("PWD");
}

/*
 * Make a directory.
 */
void
makedir(int argc, char *argv[])
{
	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "directory-name") <
		    0) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
		printf("usage: %s directory-name\n", argv[0]);
		code = -1;
		return;
	}
	(void) command("MKD %s", argv[1]);
}

/*
 * Remove a directory.
 */
void
removedir(int argc, char *argv[])
{
	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "directory-name") <
		    0) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
		printf("usage: %s directory-name\n", argv[0]);
		code = -1;
		return;
	}
	(void) command("RMD %s", argv[1]);
}

/*
 * Send a line, verbatim, to the remote machine.
 */
void
quote(int argc, char *argv[])
{
	int i;
	char buf[FTPBUFSIZ];

	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line),
		    "command line to send") == -1) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
		printf("usage: %s line-to-send\n", argv[0]);
		code = -1;
		return;
	}
	(void) strcpy(buf, argv[1]);
	for (i = 2; i < argc; i++) {
		(void) strcat(buf, " ");
		(void) strcat(buf, argv[i]);
	}
	if (command(buf) == PRELIM) {
		while (getreply(0) == PRELIM);
	}
}

/*
 * Ask the other side for help.
 */
void
rmthelp(int argc, char *argv[])
{
	int oldverbose = verbose;

	verbose = 1;
	(void) command(argc == 1 ? "HELP" : "HELP %s", argv[1]);
	verbose = oldverbose;
}

/*
 * Terminate session and exit.
 */
/*ARGSUSED*/
void
quit(int argc, char *argv[])
{
	if (connected)
		disconnect(0, NULL);
	pswitch(1);
	if (connected) {
		disconnect(0, NULL);
	}
	exit(0);
}

/*
 * Terminate session, but don't exit.
 */
void
disconnect(int argc, char *argv[])
{
	extern FILE *ctrl_in, *ctrl_out;
	extern int data;

	if (!connected)
		return;
	(void) command("QUIT");
	if (ctrl_in) {
		reset_timer();
		(void) fclose(ctrl_in);
	}
	if (ctrl_out) {
		reset_timer();
		(void) fclose(ctrl_out);
	}
	ctrl_out = ctrl_in = NULL;
	connected = 0;
	data = -1;
	if (!proxy) {
		macnum = 0;
	}
}

static int
confirm(char *cmd, char *file)
{
	char line[FTPBUFSIZ];

	if (!interactive)
		return (1);
	stop_timer();
	printf("%s %s? ", cmd, file);
	(void) fflush(stdout);
	(void) fgets(line, sizeof (line), stdin);
	reset_timer();
	return (*line != 'n' && *line != 'N');
}

void
fatal(char *msg)
{
	fprintf(stderr, "ftp: %s\n", msg);
	exit(1);
}

/*
 * Glob a local file name specification with
 * the expectation of a single return value.
 * Can't control multiple values being expanded
 * from the expression, we return only the first.
 */
static int
globulize(char **cpp)
{
	char **globbed;

	if (!doglob)
		return (1);
	globbed = glob(*cpp);
	if (globerr != NULL) {
		printf("%s: %s\n", *cpp, globerr);
		if (globbed)
			blkfree(globbed);
		return (0);
	}
	if (globbed) {
		*cpp = strdup(*globbed);
		blkfree(globbed);
		if (!*cpp)
			return (0);
	}
	return (1);
}

void
account(int argc, char *argv[])
{
	char acct[50], *ap;

	if (argc > 1) {
		++argv;
		--argc;
		(void) strncpy(acct, *argv, 49);
		acct[49] = '\0';
		while (argc > 1) {
			--argc;
			++argv;
			(void) strncat(acct, *argv, 49 - strlen(acct));
		}
		ap = acct;
	} else {
		ap = mygetpass("Account:");
	}
	(void) command("ACCT %s", ap);
}

/*ARGSUSED*/
static void
proxabort(int sig)
{
	extern int proxy;

	if (!proxy) {
		pswitch(1);
	}
	if (connected) {
		proxflag = 1;
	} else {
		proxflag = 0;
	}
	pswitch(0);
	longjmp(abortprox, 1);
}

void
doproxy(int argc, char *argv[])
{
	void (*oldintr)();
	register struct cmd *c;

	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "command") == -1) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
		printf("usage:%s command\n", argv[0]);
		code = -1;
		return;
	}
	c = getcmd(argv[1]);
	if (c == (struct cmd *)-1) {
		printf("?Ambiguous command\n");
		(void) fflush(stdout);
		code = -1;
		return;
	}
	if (c == 0) {
		printf("?Invalid command\n");
		(void) fflush(stdout);
		code = -1;
		return;
	}
	if (!c->c_proxy) {
		printf("?Invalid proxy command\n");
		(void) fflush(stdout);
		code = -1;
		return;
	}
	if (setjmp(abortprox)) {
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, (void (*)())proxabort);
	pswitch(1);
	if (c->c_conn && !connected) {
		printf("Not connected\n");
		(void) fflush(stdout);
		pswitch(0);
		(void) signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	(*c->c_handler)(argc-1, argv+1);
	if (connected) {
		proxflag = 1;
	} else {
		proxflag = 0;
	}
	pswitch(0);
	(void) signal(SIGINT, oldintr);
}

/*ARGSUSED*/
void
setcase(int argc, char *argv[])
{
	mcase = !mcase;
	printf("Case mapping %s.\n", onoff(mcase));
	code = mcase;
}

/*ARGSUSED*/
void
setcr(int argc, char *argv[])
{
	crflag = !crflag;
	printf("Carriage Return stripping %s.\n", onoff(crflag));
	code = crflag;
}

void
setntrans(int argc, char *argv[])
{
	if (argc == 1) {
		ntflag = 0;
		printf("Ntrans off.\n");
		code = ntflag;
		return;
	}
	ntflag++;
	code = ntflag;
	(void) strncpy(ntin, argv[1], 16);
	ntin[16] = '\0';
	if (argc == 2) {
		ntout[0] = '\0';
		return;
	}
	(void) strncpy(ntout, argv[2], 16);
	ntout[16] = '\0';
}

static char *
dotrans(char *name)
{
	static char new[MAXPATHLEN];
	char *cp1, *cp2 = new;
	register int i, ostop, found;

	for (ostop = 0; *(ntout + ostop) && ostop < 16; ostop++);
	for (cp1 = name; *cp1; cp1++) {
		found = 0;
		for (i = 0; *(ntin + i) && i < 16; i++) {
			if (*cp1 == *(ntin + i)) {
				found++;
				if (i < ostop) {
					*cp2++ = *(ntout + i);
				}
				break;
			}
		}
		if (!found) {
			*cp2++ = *cp1;
		}
	}
	*cp2 = '\0';
	return (new);
}

void
setnmap(int argc, char *argv[])
{
	char *cp;

	if (argc == 1) {
		mapflag = 0;
		printf("Nmap off.\n");
		code = mapflag;
		return;
	}
	if (argc < 3) {
		if (prompt_for_arg(line, sizeof (line), "mapout") == -1) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 3) {
		printf("Usage: %s [mapin mapout]\n", argv[0]);
		code = -1;
		return;
	}
	mapflag = 1;
	code = 1;
	cp = index(altarg, ' ');
	if (proxy) {
		while (*++cp == ' ')
			/* NULL */;
		altarg = cp;
		cp = index(altarg, ' ');
	}
	*cp = '\0';
	(void) strncpy(mapin, altarg, MAXPATHLEN - 1);
	while (*++cp == ' ')
		/* NULL */;
	(void) strncpy(mapout, cp, MAXPATHLEN - 1);
}

static char *
domap(char *name)
{
	static char new[MAXPATHLEN];
	register char *cp1 = name, *cp2 = mapin;
	char *tp[9], *te[9];
	int i, toks[9], toknum, match = 1;
	wchar_t	wc1, wc2;
	int	len1, len2;

	for (i = 0; i < 9; ++i) {
		toks[i] = 0;
	}
	while (match && *cp1 && *cp2) {
		if ((len1 = mbtowc(&wc1, cp1, MB_CUR_MAX)) <= 0) {
			wc1 = (unsigned char)*cp1;
			len1 = 1;
		}
		cp1 += len1;
		if ((len2 = mbtowc(&wc2, cp2, MB_CUR_MAX)) <= 0) {
			wc2 = (unsigned char)*cp2;
			len2 = 1;
		}
		cp2 += len2;

		switch (wc2) {
		case '\\':
			if ((len2 = mbtowc(&wc2, cp2, MB_CUR_MAX)) <= 0) {
				wc2 = (unsigned char)*cp2;
				len2 = 1;
			}
			cp2 += len2;
			if (wc2 != wc1)
				match = 0;
			break;

		case '$':
			if (*cp2 >= '1' && *cp2 <= '9') {
				if ((len2 =
				    mbtowc(&wc2, cp2 + 1, MB_CUR_MAX)) <= 0) {
					wc2 = (unsigned char)*(cp2 + 1);
					len2 = 1;
				}
				if (wc1 != wc2) {
					toks[toknum = *cp2 - '1']++;
					tp[toknum] = cp1 - len1;
					while (*cp1) {
						if ((len1 = mbtowc(&wc1,
						    cp1, MB_CUR_MAX)) <= 0) {
							wc1 =
							    (unsigned char)*cp1;
							len1 = 1;
						}
						cp1 += len1;
						if (wc2 == wc1)
							break;
					}
					if (*cp1 == 0 && wc2 != wc1)
						te[toknum] = cp1;
					else
						te[toknum] = cp1 - len1;
				}
				cp2++;			/* Consume the digit */
				if (wc2)
					cp2 += len2;	/* Consume wide char */
				break;
			}
			/* intentional drop through */
		default:
			if (wc2 != wc1)
				match = 0;
			break;
		}
	}

	cp1 = new;
	*cp1 = '\0';
	cp2 = mapout;
	while (*cp2) {
		match = 0;
		switch (*cp2) {
		case '\\':
			cp2++;
			if (*cp2) {
				if ((len2 = mblen(cp2, MB_CUR_MAX)) <= 0)
					len2 = 1;
				memcpy(cp1, cp2, len2);
				cp1 += len2;
				cp2 += len2;
			}
			break;

		case '[':
LOOP:
			cp2++;
			if (*cp2 == '$' && isdigit(*(cp2+1))) {
				if (*++cp2 == '0') {
					char *cp3 = name;

					while (*cp3) {
						*cp1++ = *cp3++;
					}
					match = 1;
				} else if (toks[toknum = *cp2 - '1']) {
					char *cp3 = tp[toknum];

					while (cp3 != te[toknum]) {
						*cp1++ = *cp3++;
					}
					match = 1;
				}
			} else {
				while (*cp2 && *cp2 != ',' && *cp2 != ']') {
					if (*cp2 == '\\') {
						cp2++;
						continue;
					}

					if (*cp2 == '$' && isdigit(*(cp2+1))) {
						if (*++cp2 == '0') {
							char *cp3 = name;

							while (*cp3)
								*cp1++ = *cp3++;
							continue;
						}
						if (toks[toknum = *cp2 - '1']) {
							char *cp3 = tp[toknum];

							while (cp3 !=
							    te[toknum])
								*cp1++ = *cp3++;
						}
						continue;
					}
					if (*cp2) {
						if ((len2 =
						    mblen(cp2, MB_CUR_MAX)) <=
						    0) {
							len2 = 1;
						}
						memcpy(cp1, cp2, len2);
						cp1 += len2;
						cp2 += len2;
					}
				}
				if (!*cp2) {
					printf("nmap: unbalanced brackets\n");
					return (name);
				}
				match = 1;
			}
			if (match) {
				while (*cp2 && *cp2 != ']') {
					if (*cp2 == '\\' && *(cp2 + 1)) {
						cp2++;
					}
					if ((len2 = mblen(cp2, MB_CUR_MAX)) <=
					    0)
						len2 = 1;
					cp2 += len2;
				}
				if (!*cp2) {
					printf("nmap: unbalanced brackets\n");
					return (name);
				}
				cp2++;
				break;
			}
			switch (*++cp2) {
				case ',':
					goto LOOP;
				case ']':
					break;
				default:
					cp2--;
					goto LOOP;
			}
			cp2++;
			break;
		case '$':
			if (isdigit(*(cp2 + 1))) {
				if (*++cp2 == '0') {
					char *cp3 = name;

					while (*cp3) {
						*cp1++ = *cp3++;
					}
				} else if (toks[toknum = *cp2 - '1']) {
					char *cp3 = tp[toknum];

					while (cp3 != te[toknum]) {
						*cp1++ = *cp3++;
					}
				}
				cp2++;
				break;
			}
			/* intentional drop through */
		default:
			if ((len2 = mblen(cp2, MB_CUR_MAX)) <= 0)
				len2 = 1;
			memcpy(cp1, cp2, len2);
			cp1 += len2;
			cp2 += len2;
			break;
		}
	}
	*cp1 = '\0';
	if (!*new) {
		return (name);
	}
	return (new);
}

/*ARGSUSED*/
void
setsunique(int argc, char *argv[])
{
	sunique = !sunique;
	printf("Store unique %s.\n", onoff(sunique));
	code = sunique;
}

/*ARGSUSED*/
void
setrunique(int argc, char *argv[])
{
	runique = !runique;
	printf("Receive unique %s.\n", onoff(runique));
	code = runique;
}

/* change directory to perent directory */
/*ARGSUSED*/
void
cdup(int argc, char *argv[])
{
	(void) command("CDUP");
}

void
macdef(int argc, char *argv[])
{
	char *tmp;
	int c;

	if (macnum == 16) {
		printf("Limit of 16 macros have already been defined\n");
		code = -1;
		return;
	}
	if (argc < 2) {
		if (prompt_for_arg(line, sizeof (line), "macro name") == -1) {
			code = -1;
			return;
		}
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc != 2) {
		printf("Usage: %s macro_name\n", argv[0]);
		code = -1;
		return;
	}
	if (interactive) {
		printf("Enter macro line by line, terminating "
		    "it with a null line\n");
	}
	(void) strncpy(macros[macnum].mac_name, argv[1], 8);
	if (macnum == 0) {
		macros[macnum].mac_start = macbuf;
	} else {
		macros[macnum].mac_start = macros[macnum - 1].mac_end + 1;
	}
	tmp = macros[macnum].mac_start;
	while (tmp != macbuf+4096) {
		if ((c = getchar()) == EOF) {
			printf("macdef:end of file encountered\n");
			code = -1;
			return;
		}
		if ((*tmp = c) == '\n') {
			if (tmp == macros[macnum].mac_start) {
				macros[macnum++].mac_end = tmp;
				code = 0;
				return;
			}
			if (*(tmp-1) == '\0') {
				macros[macnum++].mac_end = tmp - 1;
				code = 0;
				return;
			}
			*tmp = '\0';
		}
		tmp++;
	}
	for (;;) {
		while ((c = getchar()) != '\n' && c != EOF)
			/* NULL */;
		if (c == EOF || getchar() == '\n') {
			printf("Macro not defined - 4k buffer exceeded\n");
			code = -1;
			return;
		}
	}
}
