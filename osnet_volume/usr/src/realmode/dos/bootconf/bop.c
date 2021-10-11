/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * bop.c -- handles bootops both read and write
 */

#ident	"<@(#)bop.c	1.10	97/03/21 SMI>"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "types.h"

#include "bop.h"
#include "debug.h"
#include "devdb.h"
#include "err.h"

static char fname_bop[] = "bootops";
static char fname_res_bop[] = "bootops.res";
static FILE *fd_bop;
static FILE *fd_res_bop;

void
init_bop()
{
	/*
	 * Open up bootops, and results files here and save the handles
	 */
	if ((fd_bop = fopen(fname_bop, "wb")) == NULL) {
		fatal("Couldn't open %s file\n", fname_bop);
	}

	/*
	 * Ensure each line goes out seperately
	 */
	setvbuf(fd_bop, NULL, _IONBF, 0);

	if ((fd_res_bop = fopen(fname_res_bop, "r")) == NULL) {
		fatal("Couldn't open %s file\n", fname_res_bop);
	}

	/*
	 * Ensure no input buffering.
	 */
	setvbuf(fd_res_bop, NULL, _IONBF, 0);
}

void
out_bop(char *buf)
{
	debug(D_BOOTOPS, "out_bop: %s\n", buf);
	fputs(buf, fd_bop);
}

char *
in_bop(char *buf, u_short len)
{
	char *ret = fgets(buf, len, fd_res_bop);

	if (ret) {
		debug(D_BOOTOPS, "in_bop: %s\n", buf);
	} else {
		debug(D_BOOTOPS, "in_bop: EOF\n");
	}
	return (ret);
}

/*
 * send the entire file through bootops
 */
void
file_bop(char *fname)
{
	FILE *fp;
	char buf[250];

	if ((fp = fopen(fname, "r")) == NULL) {
		fatal("Couldn't open %s\n", fname);
	}
	out_bop("dev /options\n");
	while (fgets(buf, 250, fp) != NULL) {
		if (buf[0] != '#') { /* dont output comments */
			out_bop(buf);
		}
	}
	fclose(fp);
}
