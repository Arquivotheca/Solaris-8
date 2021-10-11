/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)utils.c	1.4	98/03/18 SMI"


#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libelf.h>
#include <sys/param.h>

#include "rdb.h"


void
perr(char * s)
{
	perror(s);
	exit(1);
}


ulong_t
hexstr_to_num(const char * str)
{
	ulong_t		num = 0;
	size_t		len = strlen(str);
	size_t		i;

	for (i = 0; i < len; i++)
		if (str[i] >= '0' && str[i] <= '9')
			num = num * 16 +((int)str[i] - (int)'0');
		else if (str[i] >= 'a' && str[i] <= 'f')
			num = num * 16 +((int)str[i] - (int)'a' + 10);
		else if (str[i] >= 'A' && str[i] <= 'F')
			num = num * 16 + ((int)str[i] - (int)'A' + 10);
	return (num);
}


#define	STBUFSIZ	1024
retc_t
proc_string_read(struct ps_prochandle * ph, ulong_t addr,
	char * buf, int bufsiz)
{
	char	intbuf[STBUFSIZ];
	int	bufind = 0;
	int	intbufind = STBUFSIZ;
	ssize_t	bufbytes = 0;
	int	cont = 1;

	if (lseek(ph->pp_fd, addr, SEEK_SET) == -1)
		return (RET_FAILED);
	while (cont && (bufind < bufsiz)) {
		if (intbufind >= bufbytes) {
			if ((bufbytes = read(ph->pp_fd, intbuf,
			    STBUFSIZ)) == -1)
				return (RET_FAILED);
			intbufind = 0;
		}
		buf[bufind] = intbuf[intbufind];
		if (buf[bufind] == '\0')
			return (RET_OK);
		bufind++;
		intbufind++;
	}
	return (RET_FAILED);
}


void
print_varstring(struct ps_prochandle * ph, const char * varname)
{
	printf("print_varstring: %s\n", varname);
	if (strcmp(varname, "regs") == 0) {
		(void) display_all_regs(ph);
		return;
	}
	print_mach_varstring(ph, varname);
}

void
print_mem(struct ps_prochandle * ph, ulong_t address, int count,
	char * format)
{
	printf("\n%17s:",
		print_address_ps(ph, address, FLG_PAP_SONAME));

	if ((*format == 'X') || (*format == 'x')) {
		int	i;

		for (i = 0; i < count; i++) {
			unsigned long word;
			if ((i % 4) == 0)
				printf("\n  0x%08lx: ", address);

			if (ps_pread(ph, address, (char *)&word,
			    sizeof (unsigned long)) != PS_OK) {
				printf("\nfailed to read memory at: 0x%lx\n",
					address);
				return;
			}
			printf("  0x%08lx", word);
			address += 4;
		}
		putchar('\n');
		return;
	}

	if (*format == 'b') {
		int	i;

		for (i = 0; i < count; i++, address ++) {
			unsigned char	byte;
			if ((i % 8) == 0)
				printf("\n 0x%08lx: ", address);
			if (ps_pread(ph, address, (char *)&byte,
			    sizeof (unsigned char)) != PS_OK) {
				fprintf(stderr, "\nfailed to read byte "
					"at: 0x%lx\n", address);
				return;
			}
			printf("  %02x", (unsigned)byte);
		}
		putchar('\n');
		return;
	}

	if (*format == 's') {
		char	buf[MAXPATHLEN];
		if (proc_string_read(ph, address, buf,
		    MAXPATHLEN) != RET_OK) {
			printf("unable to read string at: %s\n", address);
			return;
		}
		printf(" %s\n", buf);
		return;
	}
}
