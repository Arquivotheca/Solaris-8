/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ftp_var.h 1.15	99/08/17 SMI"

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
 * 	(c) 1986-1990,1996-1999  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 		All rights reserved.
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/ttold.h>
#include <sys/stropts.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/ftp.h>
#include <arpa/telnet.h>
#include <arpa/inet.h>
#include <setjmp.h>
#include <libintl.h>
#include <strings.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <widec.h>
#include <signal.h>
#include <netdb.h>
#include <pwd.h>
#include <locale.h>
#include <limits.h>
#include <fnmatch.h>
#include <dirent.h>
#include <termio.h>
#include <stdarg.h>
#include <unistd.h>
#include <malloc.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>

#define	signal(s, f)	sigset(s, f)
#define	setjmp(e)	sigsetjmp(e, 1)
#define	longjmp(e, v)	siglongjmp(e, v)
#define	jmp_buf		sigjmp_buf

/*
 * FTP global variables.
 */
#ifndef	EXTERN
#define	EXTERN	extern
#endif

/*
 * Options and other state info.
 */
EXTERN int	trace;		/* trace packets exchanged */
EXTERN int	hash;		/* print # for each buffer transferred */
EXTERN int	sendport;	/* use PORT cmd for each data connection */
EXTERN int	verbose;	/* print messages coming back from server */
EXTERN int	connected;	/* connected to server */
EXTERN int	fromatty;	/* input is from a terminal */
EXTERN int	interactive;	/* interactively prompt on m* cmds */
EXTERN int	debug;		/* debugging level */
EXTERN int	bell;		/* ring bell on cmd completion */
EXTERN int	doglob;		/* glob local file names */
EXTERN int	autologin;	/* establish user account on connection */
EXTERN int	proxy;		/* proxy server connection active */
EXTERN int	proxflag;	/* proxy connection exists */
EXTERN int	sunique;	/* store files on server with unique name */
EXTERN int	runique;	/* store local files with unique name */
EXTERN int	mcase;		/* map upper to lower case for mget names */
EXTERN int	ntflag;		/* use ntin ntout tables for name translation */
EXTERN int	mapflag;	/* use mapin mapout templates on file names */
EXTERN int	code;		/* return/reply code for ftp command */
EXTERN int	crflag;		/* if 1, strip car. rets. on ascii gets */
EXTERN char	pasv[64];	/* passive port for proxy data connection */
EXTERN char	*altarg;	/* argv[1] with no shell-like preprocessing  */
EXTERN char	ntin[17];	/* input translation table */
EXTERN char	ntout[17];	/* output translation table */
EXTERN char	mapin[MAXPATHLEN]; /* input map template */
EXTERN char	mapout[MAXPATHLEN]; /* output map template */
EXTERN char	typename[32];	/* name of file transfer type */
EXTERN int	type;		/* file transfer type */
EXTERN char	structname[32];	/* name of file transfer structure */
EXTERN int	stru;		/* file transfer structure */
EXTERN char	formname[32];	/* name of file transfer format */
EXTERN int	form;		/* file transfer format */
EXTERN char	modename[32];	/* name of file transfer mode */
EXTERN int	mode;		/* file transfer mode */
EXTERN char	bytename[32];	/* local byte size in ascii */
EXTERN int	bytesize;	/* local byte size in binary */

EXTERN char	*hostname;	/* name of host connected to */
EXTERN char	*home;
EXTERN char	*globerr;

EXTERN struct	servent *sp;	/* service spec for tcp/ftp */

#define	FTPBUFSIZ	BUFSIZ*16

EXTERN char *buf;		/* buffer for binary sends and gets */

EXTERN jmp_buf toplevel;	/* non-local goto stuff for cmd scanner */


EXTERN char	line[200];	/* input line buffer */
EXTERN char	*stringbase;	/* current scan point in line buffer */
EXTERN char	argbuf[200];	/* argument storage buffer */
EXTERN char	*argbase;	/* current storage point in arg buffer */
EXTERN int	margc;		/* count of arguments on input line */
EXTERN char	*margv[20];	/* args parsed from input line */
EXTERN int	cpend;		/* flag: if != 0, then pending server reply */
EXTERN int	mflag;		/* flag: if != 0, then active multi command */

EXTERN int	options;	/* used during socket creation */

EXTERN int	timeout;	/* connection timeout */
EXTERN int	timeoutms;	/* connection timeout in msec */
EXTERN jmp_buf	timeralarm;	/* to recover from global timeout */


/*
 * Format of command table.
 */
struct cmd {
	char	*c_name;	/* name of command */
	char	*c_help;	/* help string */
	char	c_bell;		/* give bell when command completes */
	char	c_conn;		/* must be connected to use command */
	char	c_proxy;	/* proxy server may execute */
	void	(*c_handler)(int argc, char *argv[]); /* function to call */
};

struct macel {
	char mac_name[9];	/* macro name */
	char *mac_start;	/* start of macro in macbuf */
	char *mac_end;		/* end of macro in macbuf */
};

EXTERN int macnum;			/* number of defined macros */
EXTERN struct macel macros[16];
EXTERN char macbuf[4096];

extern void macdef(int argc, char *argv[]);
extern void doproxy(int argc, char *argv[]);
extern void setpeer(int argc, char *argv[]);
extern void rmthelp(int argc, char *argv[]);
extern void settype(int argc, char *argv[]);
extern void setbinary(int argc, char *argv[]);
extern void setascii(int argc, char *argv[]);
extern void settenex(int argc, char *argv[]);
extern void setebcdic(int argc, char *argv[]);
extern void setmode(int argc, char *argv[]);
extern void setform(int argc, char *argv[]);
extern void setstruct(int argc, char *argv[]);
extern void put(int argc, char *argv[]);
extern void mput(int argc, char *argv[]);
extern void get(int argc, char *argv[]);
extern void mget(int argc, char *argv[]);
extern void status(int argc, char *argv[]);
extern void setbell(int argc, char *argv[]);
extern void settrace(int argc, char *argv[]);
extern void sethash(int argc, char *argv[]);
extern void setverbose(int argc, char *argv[]);
extern void setport(int argc, char *argv[]);
extern void setprompt(int argc, char *argv[]);
extern void setglob(int argc, char *argv[]);
extern void setdebug(int argc, char *argv[]);
extern void cd(int argc, char *argv[]);
extern void lcd(int argc, char *argv[]);
extern void delete(int argc, char *argv[]);
extern void mdelete(int argc, char *argv[]);
extern void renamefile(int argc, char *argv[]);
extern void ls(int argc, char *argv[]);
extern void mls(int argc, char *argv[]);
extern void shell(int argc, char *argv[]);
extern void user(int argc, char *argv[]);
extern void pwd(int argc, char *argv[]);
extern void makedir(int argc, char *argv[]);
extern void removedir(int argc, char *argv[]);
extern void quote(int argc, char *argv[]);
extern void rmthelp(int argc, char *argv[]);
extern void quit(int argc, char *argv[]);
extern void disconnect(int argc, char *argv[]);
extern void account(int argc, char *argv[]);
extern void setcase(int argc, char *argv[]);
extern void setcr(int argc, char *argv[]);
extern void setntrans(int argc, char *argv[]);
extern void setnmap(int argc, char *argv[]);
extern void setsunique(int argc, char *argv[]);
extern void setrunique(int argc, char *argv[]);
extern void cdup(int argc, char *argv[]);
extern void domacro(int argc, char *argv[]);
extern void help(int argc, char *argv[]);
extern void reset(int argc, char *argv[]);

extern void fatal(char *msg);
extern int getreply(int expecteof);
extern void call(void (*routine)(int argc, char *argv[]), ...);
extern void sendrequest(char *cmd, char *local, char *remote, int allowpipe);
extern void recvrequest(char *cmd, char *local, char *remote, char *mode,
    int allowpipe);
extern void makeargv(void);
extern int login(char *host);
extern int command(char *fmt, ...);
extern char **glob(char *v);
extern void blkfree(char **);
extern void pswitch(int flag);

extern char *hookup(char *host, ushort_t port);
extern char *mygetpass(char *prompt);
extern void lostpeer(int sig);
extern int ruserpass(char *host, char **aname, char **apass, char **aacct);
extern FILE *mypopen(char *cmd, char *mode);
extern int mypclose(FILE *ptr);
extern struct cmd *getcmd(char *name);

extern void stop_timer(void);
extern void reset_timer(void);
extern int getpagesize(void);
