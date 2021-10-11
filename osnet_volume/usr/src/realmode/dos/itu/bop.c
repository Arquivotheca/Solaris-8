/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)bop.c   1.3   97/12/09 SMI"

/*
 * bop.c -- handles bootops both read and write
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dos.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bop2.h"
static char fname_bop[] = "bootops";
static char fname_res_bop[] = "bootops.res";
static FILE *fd_bop;
static FILE *fd_res_bop;

#ifdef obsolete
void
ckstr(char *comment, char *name)
{
	int j;
	char *tn = name;

	if (comment)
		printf("%s :::", comment);
	for (j = 1; tn && isprint(*tn); j++, tn++)
		printf("%c", *tn);

	if (tn) {
		printf("\nNon printable is @%d and is 0x%x "
			"len %d\n", j, *tn, strlen(name));
	}
}
#endif

void
init_bop()
{
	/*
	 * Open up bootops, and results files here and save the handles
	 */
	if ((fd_bop = fopen(fname_bop, "wb")) == NULL) {
		printf("Couldn't open %s file\n", fname_bop);
		exit(1);
	}

	/*
	 * Ensure each line goes out seperately
	 */
	setvbuf(fd_bop, NULL, _IONBF, 0);

	if ((fd_res_bop = fopen(fname_res_bop, "r")) == NULL) {
		printf("Couldn't open %s file\n", fname_res_bop);
		exit(1);
	}

	/*
	 * Ensure no input buffering.
	 */
	setvbuf(fd_res_bop, NULL, _IONBF, 0);
}

void
out_bop(char *buf)
{
	fputs(buf, fd_bop);
}

char *
in_bop(char *buf, unsigned short len)
{
	char *ret = fgets(buf, len, fd_res_bop);
	return (ret);
}

/*
 * send the entire file through bootops
 */
void
file_bop(char *fname)
{
	FILE *fp;
	char buf[_MAX_PATH*2];

	if ((fp = fopen(fname, "r")) == NULL) {
		printf("Couldn't open %s\n", fname);
		exit(1);
	}
	out_bop("dev /options\n");
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (buf[0] != '#') { /* dont output comments */
			out_bop(buf);
		}
	}
	fclose(fp);
}

/*
 * read_prop()
 *
 * Attempts to get the property from the specified node.  If the
 * node is not specified we default to the /options node.
 * Checks for property value greater than the input buffer size
 * currently _MAX_PATH bytes). Returns the string on success, 0 on failure.
 */
char *
read_prop(char *prop, char *where)
{
	static char ibuf[_MAX_PATH*2];
	char obuf[60]; /* 1275 property names have a max length of 31 */

	if (where) {
		(void) sprintf(obuf, "dev /%s\n", where);
	} else {
		(void) sprintf(obuf, "dev /options\n");
	}
	out_bop(obuf);

	(void) sprintf(obuf, "getprop %s\n", prop);
	out_bop(obuf);

	ibuf[0] = 0;
	(void) in_bop(ibuf, _MAX_PATH);
	if (strchr(ibuf, '\n') == 0) { /* check for overflow */
		/*
		 * overflowed input buf - discard the rest
		 */
		while (in_bop(ibuf, _MAX_PATH) && (strchr(ibuf, '\n') == 0)) {
			;
		}
		return (0);
	}

	(void) sprintf(obuf, "getprop: %s not found", prop);
	if ((strncmp(obuf, ibuf, strlen(obuf) - 1) == 0) || (ibuf[0] == 0)) {
		return (0);
	}
	return (ibuf);
}


void
write_prop(char *prop, char *where, char *val, char *sep, int flag)
{
	char ibuf[_MAX_PATH*2];
	char *prp;

	/*
	 * Check if the node exists. If not create it.
	 */

	sprintf(ibuf, "dev /%s\n", where);
	out_bop(ibuf);
	ibuf[0] = 0;
	in_bop(ibuf, _MAX_PATH);
	if (ibuf[0] != 0) {
		sprintf(ibuf, "mknod /%s\n", where);
		out_bop(ibuf);
	}

	if (((prp = read_prop(prop, where)) == NULL) ||
	    flag == OVERWRITE_PROP) {
		sprintf(ibuf, "dev /%s\n", where);
		out_bop(ibuf);
		sprintf(ibuf, "setprop %s %s\n", prop, val);
	} else {
		char *cp;

		if (cp = strchr(prp, '\n'))
		    *cp = 0x0;
		sprintf(ibuf, "setprop %s %s%s%s\n", prop, prp, sep, val);
	}
	out_bop(ibuf);
}
