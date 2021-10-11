/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

#pragma ident "@(#)patch_bootstrap.c 1.1	94/05/26 SMI"

/*
 * fixbanner:
 *
 * utility program to "punch in" the correct banner on Solaris boot binaries.
 *
 * Algorithmic assumption:
 *    The data section resides after the code section in the executable
 *    file, therefore the banner that we want to replace is probably
 *    closer to the end of the file; so, we search front-to-back.
 *
 * This is an optimized brute-force search: (Knuth-Morris-Pratt search variant)
 *    The first(last) character of the search pattern is appears only once
 *    within the string; i.e., when a mismatch is detected "j" chars into
 *    the string, we don't have to back up the pointer that marks our
 *    current search position, because we know that none of the previous
 *    "j-1" chars can match the first character in the pattern.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define	FAILURE -1
#define	BANNER_SIZE 80

static unchar *findmatch(unchar *p, unchar *b, size_t fsize);
static void usage(char *);

unsigned char b[81] = "Solaris for x86                                             Multiple Device Boot\n";

/*
 * usage: fixbanner mdexec "banner string"
 *
 */

main(int argc, char *argv[])
{
	register int i, j;
	register unchar *p, *r, *s;
	register int infd;
	unchar banner[81];
	struct stat statbuf;
	int rc;
	size_t fsize;

	if (argc < 3 || strcmp("-help", argv[1]) == 0) {
		usage(argv[0]);
		exit(1);
	}

	if ((infd = open(argv[1], O_RDWR)) == FAILURE) {
		fprintf(stderr, "Cannot open input file '%s'.\n", argv[1]);
		exit(1);
	}
	b[80] = (unsigned char)255;

	/* stat the file to determine the size of buffer required */
	if (fstat(infd, &statbuf) == FAILURE) {
		fprintf(stderr, "Unable to stat file '%s'.\n", argv[1]);
		exit(1);
	}
	fsize = (size_t)statbuf.st_size;

	/* allocate an operating buffer */
	if ((p = (unchar *)malloc((size_t)statbuf.st_size)) == NULL) {
		fprintf(stderr, "Cannot alloc buf (%d).\n", (int)fsize);
		exit(1);
	}

	if (read(infd, p, fsize) != fsize) {
		fprintf(stderr, "Cannot read file '%s'.\n", argv[1]);
		free((void *)p);
		exit(1);
	}

	strncpy(banner, argv[2], BANNER_SIZE);
	banner[80] = '\0';

	if ((r = findmatch(p, b, fsize)) == NULL) {
		fprintf(stderr, "No banner found in file '%s'.\n", argv[1]);
		free((void *)p);
		exit(1);
	}

	for (i = 0, s = banner; i < BANNER_SIZE; i++)
#if defined(DEBUG)
		printf("%c", *s++);
#else				/* !defined(DEBUG) */
		*r++ = *s++;
#endif				/* defined(DEBUG) */

	if (lseek(infd, 0L, SEEK_SET) != 0L) {
		fprintf(stderr, "Unable to lseek file '%s'.\n", argv[1]);
		free((void *)p);
		exit(1);
	}

	if (write(infd, p, fsize) != fsize) {
		fprintf(stderr, "Cannot write output file '%s'.\n", argv[1]);
		free((void *)p);
		exit(1);
	}

	free((void *)p);
	exit(0);
}

static unchar *
findmatch(unchar *p, unchar *b, size_t fsize)
{
	register int i, j;

	for (i = fsize-1, j = BANNER_SIZE-1; i >= 0 && j >= 0; i--, j--)
		while (p[i] != b[j]) {
			i--;
			j = BANNER_SIZE-1;
		}

	if (j < 0) {	/* found our pattern match */
		return ((unchar *)&(p[i+1]));
	}
	else
		return (NULL);
}

static void
usage(char *p)
{
	fprintf(stderr, "usage: %s filename \"banner string...\"\n", p);
	fprintf(stderr, "replaces banner string in real mode boot binaries.\n");
	fprintf(stderr, "NOTE: the banner is expected to be an %d%s",
	    BANNER_SIZE, "-character string.\n");
}
