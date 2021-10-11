/*
 * Copyright (c) 1998 Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)policy.c	1.1	98/06/04 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/platnames.h>
#include <sys/salib.h>

/*
 * The policy file uses semantics similar to other default files processed
 * by the defopen and defread interfaces defined in deflt.h. Patterns are
 * matched at the beginning of a line, with or without case sensitivity.
 * If a pattern is matched, a pointer is returned to the remainder of the
 * line which may or may not be a NULL string but is NUL terminated.
 */

#define	POLICY_FILE	"boot.conf"
#define	ARBITRARY_LIMIT	((128 * 1024) - 1)

extern int verbosemode;

extern	caddr_t kmem_alloc(size_t);
extern	void kmem_free(caddr_t, size_t);

static char *policy_filep;
static char *policy_currentp;
static size_t policy_filesize, policy_bufsize;
static int policy_in_memory;
static int debug;

/*ARGSUSED*/
static int
my_open(char *pathname, void *arg)
{
	if (debug && verbosemode)
		printf("trying policy file '%s'\n", pathname);
	return (open(pathname, O_RDONLY));
}

/*
 * Read the policy file into memory preparing it for use.
 * If the file doesn't exist or there's a read error, note
 * the error, but continue anyway.
 */
void
policy_open(void)
{
	int fd;
	size_t i;
	ssize_t n;
	char *p, *eof, *buf;
	struct stat statbuf;
	extern char *impl_arch_name;

	if (policy_in_memory)
		return;

	policy_filesize = policy_bufsize = 0;
	policy_filep = policy_currentp = 0;

	buf = (char *)kmem_alloc(MAXPATHLEN);
	fd = open_platform_file(POLICY_FILE, my_open, NULL, buf,
	    impl_arch_name);
	kmem_free(buf, MAXPATHLEN);

	if (fd == -1)  {
		/*
		 * The policy file won't exist on the redirection slice
		 * on the CD, so don't complain about this too loudly.
		 */
		if (verbosemode)
			(void) printf("Warning: Can't open policy file <%s>\n",
			    POLICY_FILE);
		return;
	}

	if (fstat(fd, &statbuf) != 0)  {
		(void) printf("Warning: Can't stat policy file <%s>\n",
		    POLICY_FILE);
		(void) close(fd);
		return;
	}

	policy_filesize = policy_bufsize = (size_t)statbuf.st_size;
	if (debug && verbosemode)
		(void) printf("Policy file size: %d\n", policy_filesize);
	if (policy_filesize == 0) {
		(void) close(fd);
		return;
	}
	if (policy_filesize > ARBITRARY_LIMIT)  {
		(void) printf("Warning: policy file <%s> exceeds %d bytes.\n",
		    POLICY_FILE, ARBITRARY_LIMIT);
		(void) printf("processing first %d bytes only\n",
		    ARBITRARY_LIMIT);
		policy_filesize = policy_bufsize = ARBITRARY_LIMIT;
	}

	policy_filep = policy_currentp =
	    (char *)kmem_alloc(policy_filesize + 1);
	*(policy_filep + policy_filesize) = (char)0;

	i = 0;
	while (i < policy_filesize) {
		n = read(fd, policy_filep + i, policy_filesize - i);
		if (n == -1) {
			(void) printf("Warning: Policy file <%s> read error\n",
			    POLICY_FILE);
			*(policy_filep + i) = (char)0;
			policy_filesize = i;
			break;
		}
		i += n;
	}

	(void) close(fd);

	++policy_in_memory;

	/*
	 * Isolate lines in the file by NUL terminating each line.
	 */

	p = policy_filep;
	eof = p + policy_filesize;

	for (/* empty */; p < eof; ++p)
		if (*p == '\n')
			*p = (char)0;
}

/*
 * Free resources associated with the in-memory copy of the file.
 */
void
policy_close(void)
{
	if (policy_in_memory && policy_bufsize)
		kmem_free(policy_filep, policy_bufsize);
	policy_in_memory = 0;
}

/*
 * Reset the pointers to the beginning of the file in memory.
 */
static void
policy_rewind(void)
{
	policy_currentp = policy_filep;
}

/*
 * Return a pointer to the next non-NULL line from the file, returning zero
 * at EOF.  Whitespace at the beginning of a line is skipped.
 */
static char *
policy_get_line(void)
{
	char *p = policy_currentp;
	char *s;
	char *eof = policy_filep + policy_filesize;

	if ((policy_in_memory == 0) || (policy_filesize == 0))
		return (0);

	/* skip whitespace including end of lines stored as NUL chars */

	while ((p < eof) && ((*p == (char)0) || (*p == ' ') || (*p == '\t')))
		++p;

	policy_currentp = p;
	if (p >= eof)
		return (0);

	/* Advance to the end of the line which is marked by a NUL char */

	for (s = p; *s && (s < eof); ++s)
		/* empty */;
	policy_currentp = s;

	return (p);
}

/*
 * Find a pattern at the beginning of a line in the policy file.
 */
char *
policy_lookup(char *name, int ignore_case)
{
	size_t len = strlen(name);
	char *p;
	int i;

	policy_rewind();
	while ((p = policy_get_line()) != (char *)0) {
		if (ignore_case)
			i = strncasecmp(name, p, len);
		else
			i = strncmp(name, p, len);
		if (i == 0)
			return (p + len);
	}
	return (0);
}
