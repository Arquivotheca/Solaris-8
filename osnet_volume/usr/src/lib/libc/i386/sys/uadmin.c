/*
 * Copyright (c) 1996, 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)uadmin.c   1.6     98/10/16 SMI"

#ifdef __STDC__
#pragma weak uadmin = _uadmin
#endif

/*
 * Wrapper function to implement reboot w/ arguments on x86
 * platforms. Extract reboot arguments and place them in
 * in the file /platform/i86pc/solaris/bootargs.rc.
 * All other commands are passed through.
 */

#include "synonyms.h"
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uadmin.h>

#define	BFILE	"/platform/i86pc/boot/solaris/bootargs.rc"
#define	MAX_BOOTARG	256

static int
legal_arg(char *bargs)
{
	int i;

	for (i = 0; i < MAX_BOOTARG; i++, bargs++) {
		if (*bargs == 0 && i > 0)
			return (i);
		if (!isprint(*bargs))
			break;
	}
	return (-1);
}

static char setbuf[] = "set ";
static char rbabuf[] = "rb_args ";
static char rbfbuf[] = "rb_file ";
static char nl[] = "\n";
static char quote[] = "\'";

int
uadmin(int cmd, int fcn, uintptr_t mdep)
{
	extern int __uadmin(int cmd, int fcn, uintptr_t mdep);
	int bfd, len;
	char *bargs;

	bargs = (char *)mdep;
	bfd = -1;
	if (geteuid() == 0 && (cmd == A_SHUTDOWN || cmd == A_REBOOT)) {
		switch (fcn) {
		case AD_IBOOT:
		case AD_SBOOT:
		case AD_SIBOOT:
			/*
			 * These functions fabricate appropriate bootargs.
			 * If bootargs are passed in, map these functions
			 * to AD_BOOT.
			 */
			if (bargs == 0) {
				switch (fcn) {
				case AD_IBOOT:
					bargs = "-a";
					break;
				case AD_SBOOT:
					bargs = "-s";
					break;
				case AD_SIBOOT:
					bargs = "-sa";
					break;
				}
			}
			/*FALLTHROUGH*/
		case AD_BOOT:
			if (bargs == 0)
				break;	/* no args */
			if ((len = legal_arg(bargs)) < 0)
				break;	/* bad args */
			bfd = open(BFILE, O_RDWR|O_TRUNC|O_CREAT, 0644);
			if (bfd < 0)
				break;	/* cannot open arg file */
			/*
			 * parse the bootargs string to see if there
			 * is an alternated boot file specified.
			 */
			if (*bargs != '-') {
				/*
				 * alternate boot file specified,
				 * setup boot-file property.
				 */
				(void) write(bfd, setbuf, strlen(setbuf));
				(void) write(bfd, rbfbuf, strlen(rbfbuf));
				while (*bargs != 0 && !isspace((int)*bargs)) {
					(void) write(bfd, bargs, 1);
					bargs++;
					len--;
				}
				(void) write(bfd, nl, strlen(nl));
				while (*bargs != 0 && isspace((int)*bargs)) {
					bargs++;
					len--;
				}
			}
			if (*bargs == '-') {
				/*
				 * boot flags specified,
				 * setup boot-args property.
				 */
				(void) write(bfd, setbuf, strlen(setbuf));
				(void) write(bfd, rbabuf, strlen(rbabuf));
				(void) write(bfd, quote, strlen(quote));
				(void) write(bfd, bargs, len);
				(void) write(bfd, quote, strlen(quote));
				(void) write(bfd, nl, strlen(nl));
			}

			break;
		}
		if (bfd >= 0) {
			(void) fsync(bfd);
			(void) close(bfd);
		}
	}
	return (__uadmin(cmd, fcn, mdep));
}
