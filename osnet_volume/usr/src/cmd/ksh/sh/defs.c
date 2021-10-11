#ident	"@(#)defs.c	1.9	96/08/16 SMI"	/* From AT&T Toolchest */

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Ksh - AT&T Bell Laboratories
 * Written by David Korn
 * This file defines all the  read/write shell global variables
 */

#include	"defs.h"
#include	"jobs.h"
#include	"sym.h"
#include	"history.h"
#include	"edit.h"
#include	"timeout.h"


struct sh_scoped	st;
struct sh_static	sh;

#ifdef VSH
    struct	edit	editb;
#else
#   ifdef ESH
	struct	edit	editb;
#   endif /* ESH */
#endif	/* VSH */

struct history	*hist_ptr;
struct jobs	job;
int		sh_lastbase = 10; 
longlong_t	sh_mailchk = 600;
#ifdef TIMEOUT
    longlong_t		sh_timeout = TIMEOUT;
#else
    longlong_t		sh_timeout = 0;
#endif /* TIMEOUT */
char		io_tmpname[] = "/tmp/shxxxxxx.aaa";

#ifdef 	NOBUF
    char	_sibuf[IOBSIZE+1];
    char	_sobuf[IOBSIZE+1];
#endif	/* NOBUF */

struct fileblk io_stdin = { (char *)_sibuf, (char *)_sibuf,
				(char *)_sibuf, 0, IOREAD, 0, F_ISFILE};
struct fileblk io_stdout = { (char *)_sobuf, (char *)_sobuf,
				(char *)_sobuf+IOBSIZE, 0, IOWRT,2};
struct fileblk *io_ftable[NFILE+USERIO] = { 0, &io_stdout, &io_stdout};
