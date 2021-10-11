/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)utility.c	1.7	99/11/24 SMI"

#include "utility.h"
#include "streams_common.h"

#define	XBUFFER_SIZE	(16 * KILOBYTE)

#define	EXIT_OK		0
#define	EXIT_FAILURE	1
#define	EXIT_ERROR	2
#define	EXIT_INTERNAL	3

static int held_fd = -1;
static stream_t	*cleanup_chain = NULL;
static stream_t	*output_guard = NULL;
static char *output_filename = NULL;

void
swap(void **a, void **b)
{
	void *t;

	t = *a;
	*a = *b;
	*b = t;
}

int
bump_file_template(char *T)
{
	int n = strlen(T);
	int i;

	for (i = n - 1; isdigit(T[i]); i--) {
		T[i]++;
		if (T[i] > '9')
			T[i] = '0';
		else
			return (0);
	}

	/* template has been exhausted */
	return (-1);
}

size_t
strtomem(char *S)
{
	const char *format_str = "%lf%c";
	double val = 0.0;
	size_t retval;
	char units = 'k';
	size_t phys_total = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE);

	if (sscanf(S, format_str, &val, &units) < 1 || val < 0)
		return (0);

	if (units == '%') {
		if (val < 0 || val > 100)
			return (0);
		val *= phys_total / 100;
	} else
		switch (units) {
			case 't' : /* terabytes */
			case 'T' :
				val *= 1024;
				/*FALLTHROUGH*/
			case 'g' : /* gigabytes */
			case 'G' :
				val *= 1024;
				/*FALLTHROUGH*/
			case 'm' : /* megabytes */
			case 'M' :
				val *= 1024;
				/*FALLTHROUGH*/
			case 'k' : /* kilobytes */
			case 'K' :
				val *= 1024;
				/*FALLTHROUGH*/
			case 'b' : /* bytes */
			case 'B' :
				break;
			default :
				/*
				 * default is kilobytes
				 */
				val *= 1024;
				break;
		}

	if (val > SIZE_MAX)
		return (0);

	retval = val;

	return (retval);
}

size_t
available_memory(size_t mem_limit)
{
	size_t	phys_avail = sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGESIZE);

	if (mem_limit != 0)
#ifdef DEBUG
		/*
		 * In the debug case, we want to test the temporary files
		 * handling, so no lower bound on the memory limit is imposed.
		 */
		return (mem_limit);
#else
		return (MAX(64 * KILOBYTE, mem_limit));
#endif /* DEBUG */

	return (MAX(64 * KILOBYTE, MIN(AV_MEM_MULTIPLIER * phys_avail /
	    AV_MEM_DIVISOR, 16 * MEGABYTE)));
}

void
set_output_file(char *of)
{
	output_filename = of;
}

void
set_memory_ratio(sort_t *S, int *numerator, int *denominator)
{
	if (S->m_c_locale) {
		*numerator = CHAR_AVG_LINE;
		*denominator = sizeof (line_rec_t) + sizeof (line_rec_t *) +
		    CHAR_AVG_LINE + CHAR_AVG_LINE;
		return;
	}

	if (S->m_single_byte_locale) {
		*numerator = CHAR_AVG_LINE;
		*denominator = sizeof (line_rec_t) + sizeof (line_rec_t *) +
		    CHAR_AVG_LINE + XFRM_MULTIPLIER * CHAR_AVG_LINE;
		return;
	}

	*numerator = WCHAR_AVG_LINE;
	*denominator = sizeof (line_rec_t) + sizeof (line_rec_t *) +
	    WCHAR_AVG_LINE + WCHAR_AVG_LINE;
}

static void
usage()
{
	(void) fprintf(stderr,
	    gettext("usage: %s [-cmu] [-o output] [-T directory] [-S mem]"
	    " [-z recsz]\n\t[-dfiMnr] [-b] [-t char] [-k keydef]"
	    " [+pos1 [-pos2]] files...\n"), CMDNAME);
}

void
set_cleanup_chain(stream_t *str)
{
	cleanup_chain = str;
}

void
set_output_guard(stream_t *str)
{
	output_guard = str;
}

void
warning(const char *format, ...)
{
	va_list ap;

	(void) fprintf(stderr, "%s: ", CMDNAME);
	va_start(ap, format);
	(void) vfprintf(stderr, format, ap);
}

void *
safe_realloc(void *ptr, size_t sz)
{
	/*
	 * safe_realloc() is not meant as an alternative free() mechanism--we
	 * disallow reallocations to size zero.
	 */
	ASSERT(sz != 0);

	if ((ptr = realloc(ptr, sz)) != NULL)
		return (ptr);

	terminate(SE_REALLOCATE_BUFFER);
	/*NOTREACHED*/
}

void
safe_free(void *ptr)
{
	if (ptr)
		free(ptr);
}

void
terminate(int error_code, ...)
{
	int exit_status = EXIT_OK;
	va_list	ap;

	va_start(ap, error_code);

	switch (error_code) {
		case SE_USAGE:
			/*FALLTHROUGH*/
		case SE_BAD_SPECIFIER:
			usage();
			exit_status = EXIT_ERROR;
			break;
		case SE_CHECK_ERROR:
			warning(gettext("check option (-c) only for use with"
			    " a single file\n"));
			exit_status = EXIT_ERROR;
			break;
		case SE_CHECK_FAILED:
			exit_status = EXIT_FAILURE;
			break;
		case SE_CHECK_SUCCEED:
			exit_status = EXIT_OK;
			break;
		case SE_MMAP_FAILED:
			warning(gettext("unable to mmap: %s\n"),
			    strerror(errno));
			exit_status = EXIT_ERROR;
			break;
		case SE_MUNMAP_FAILED:
			warning(gettext("unable to munmap %s: %s\n"),
			    (char *)va_arg(ap, char *), strerror(errno));
			exit_status = EXIT_ERROR;
			break;
		case SE_REALLOCATE_BUFFER:
			warning(gettext("unable to reallocate buffer\n"));
			exit_status = EXIT_ERROR;
			break;
		case SE_BAD_STREAM:
			warning(gettext("stream of type %d seen\n"),
			    (int)va_arg(ap, int));
			exit_status = EXIT_ERROR;
			break;
		case SE_BAD_FIELD:
			warning(gettext("field of type %d seen\n"),
			    (int)va_arg(ap, int));
			exit_status = EXIT_ERROR;
			break;
		case SE_CANT_OPEN_FILE:
			/*FALLTHROUGH*/
		case SE_CANT_MMAP_FILE:
			warning(gettext("can't open %s: %s\n"),
			    (char *)va_arg(ap, char *), strerror(errno));
			exit_status = EXIT_ERROR;
			break;
		case SE_READ_FAILED:
			/*FALLTHROUGH*/
		case SE_STAT_FAILED:
			warning(gettext("can't read %s: %s\n"),
			    (char *)va_arg(ap, char *), strerror(errno));
			exit_status = EXIT_ERROR;
			break;
		case SE_WRITE_FAILED:
			warning(gettext("can't write %s: %s\n"),
			    (char *)va_arg(ap, char *), strerror(errno));
			exit_status = EXIT_ERROR;
			break;
		case SE_UNLINK_FAILED:
			warning(gettext("error unlinking %s: %s\n"),
			    (char *)va_arg(ap, char *), strerror(errno));
			exit_status = EXIT_ERROR;
			break;
		case SE_CANT_SET_SIGNAL:
			exit_status = EXIT_ERROR;
			break;
		case SE_ILLEGAL_CHARACTER:
			warning(gettext("can't translate illegal wide"
			    " character\n"));
			exit_status = EXIT_ERROR;
			break;
		case SE_TOO_MANY_TEMPFILES:
			warning(gettext("temporary file template exhausted\n"));
			exit_status = EXIT_ERROR;
			break;
		case SE_INSUFFICIENT_MEMORY:
			warning(gettext("insufficient memory;"
			    " use -S option to increase allocation\n"));
			exit_status = EXIT_ERROR;
			break;
		case SE_INSUFFICIENT_DESCRIPTORS:
			warning(gettext("insufficient available file"
			    " descriptors\n"));
			exit_status = EXIT_ERROR;
			break;
		case SE_CAUGHT_SIGNAL:
			exit_status = 127 + (int)va_arg(ap, int);
			break;
		default:
			warning(gettext("internal error %d\n"), error_code);
			exit_status = EXIT_INTERNAL;
			break;
	}

	va_end(ap);

	if (output_guard)
		xcp(output_filename, output_guard->s_filename,
		    output_guard->s_filesize);

	if (cleanup_chain)
		stream_unlink_temporaries(cleanup_chain);

	ASSERT(exit_status != EXIT_ERROR && exit_status <= 127);

	exit(exit_status);
}

void *
xzmap(void *addr, size_t len, int prot, int flags, off_t off)
{
	void *pa;

	pa = mmap(addr, len, prot, flags | MAP_ANON, -1, off);
	if (pa == MAP_FAILED)
		terminate(SE_MMAP_FAILED);

	return (pa);
}

/*
 * hold_file_descriptor() and release_file_descriptor() reserve a single file
 * descriptor entry for later use.  We issue the hold prior to any loop that has
 * an exit condition based on the receipt of EMFILE from an open() call; once we
 * have exited, we can release, typically prior to opening a file for output.
 */
void
hold_file_descriptor()
{
	ASSERT(held_fd == -1);

	if ((held_fd = open("/dev/null", O_RDONLY)) == -1)
		terminate(SE_INSUFFICIENT_DESCRIPTORS);
}

void
release_file_descriptor()
{
	ASSERT(held_fd != -1);

	(void) close(held_fd);
	held_fd = -1;
}

void
copy_line_rec(const line_rec_t *a, line_rec_t *b)
{
	(void) memcpy(b, a, sizeof (line_rec_t));
}

void
trip_eof(FILE *f)
{
	if (feof(f))
		return;

	(void) ungetc(fgetc(f), f);
}

/*
 * cxwrite() implements a buffered version of fwrite(ptr, nitems, 1, .) on file
 * descriptors.  It returns -1 in the case that the write() call fails to print
 * the current buffer contents.  cxwrite() must be flushed before being applied
 * to a new file descriptor.
 */
ssize_t
cxwrite(int fd, char *ptr, size_t nitems)
{
	static char buffer[XBUFFER_SIZE];
	static size_t offset = 0;
	size_t mitems;

	if (ptr == NULL) {
		if (write(fd, buffer, offset) == offset) {
			offset = 0;
			return (0);
		}
		return (-1);
	}

	while (nitems != 0) {
		if (offset + nitems >= XBUFFER_SIZE)
			mitems = XBUFFER_SIZE - offset;
		else
			mitems = nitems;

		(void) memcpy(buffer + offset, ptr, mitems);
		nitems -= mitems;
		offset += mitems;
		ptr += mitems;

		if (nitems) {
			if (write(fd, buffer, offset) == offset)
				offset = 0;
			else
				return (-1);
		}
	}

	return (0);
}

ssize_t
wxwrite(int fd, wchar_t *ptr, size_t nwchars)
{
	static char *convert_buffer;
	static size_t convert_bufsize = 1024;
	size_t req_bufsize;

	if (ptr == NULL)
		return (cxwrite(NULL, 0, 1));

	if (convert_buffer == NULL)
		convert_buffer = safe_realloc(NULL, convert_bufsize);

	req_bufsize = wcstombs(NULL, ptr, convert_bufsize);
	if (req_bufsize > convert_bufsize) {
		convert_bufsize = req_bufsize + 1;
		convert_buffer = safe_realloc(convert_buffer, convert_bufsize);
	}

	(void) wcstombs(convert_buffer, ptr, convert_bufsize);

	if (cxwrite(fd, convert_buffer, req_bufsize) == req_bufsize)
		return (nwchars);

	return (0);
}

int
xstreql(const char *a, const char *b)
{
	return (strcmp(a, b) == 0);
}

int
xstrneql(const char *a, const char *b, const size_t l)
{
	return (strncmp(a, b, l) == 0);
}

char *
xstrnchr(const char *S, const int c, const size_t n)
{
	const char	*eS = S + n;

	do {
		if (*S == (char)c)
			return ((char *)S);
	} while (++S < eS);

	return (NULL);
}

void
xstrninv(char *s, ssize_t start, ssize_t length)
{
	ssize_t i;

	for (i = start; i < start + length; i++)
		s[i] = UCHAR_MAX - s[i];
}

int
xwcsneql(const wchar_t *a, const wchar_t *b, const size_t length)
{
	return (wcsncmp(a, b, length) == 0);
}

wchar_t *
xwsnchr(const wchar_t *ws, const wint_t wc, const size_t n)
{
	const wchar_t	*ews = ws + n;

	do {
		if (*ws == (wchar_t)wc)
			return ((wchar_t *)ws);
	} while (++ws < ews);

	return (NULL);
}

void
xwcsninv(wchar_t *s, ssize_t start, ssize_t length)
{
	ssize_t	i;

	for (i = start; i < start + length; i++)
		s[i] = WCHAR_MAX - s[i];
}

wchar_t *
xmemwchar(wchar_t *s, wchar_t w, ssize_t length)
{
	ssize_t i = length;

	while (--i > 0) {
		if (*s == w)
			return (s);
		s++;
	}

	return (NULL);
}

void
xcp(char *dst, char *src, off_t size)
{
	int fd_in, fd_out;
	void *mm_in, *mm_out;
	size_t chunksize = SSIZE_MAX / 2;	/* need to leave room for pgm */
	int i;
	ssize_t nchunks = size / chunksize;
	ssize_t lastchunk = size % chunksize;

	if ((fd_in = open(src, O_RDONLY)) < 0)
		terminate(SE_CANT_OPEN_FILE, src);
	if ((fd_out = open(dst, O_RDWR | O_CREAT | O_TRUNC, OUTPUT_MODE)) < 0)
		terminate(SE_CANT_OPEN_FILE, dst);

	if (lseek(fd_out, size - 1, SEEK_SET) == -1 ||
	    write(fd_out, "", 1) != 1)
		terminate(SE_WRITE_FAILED);

	for (i = 0; i < nchunks; i++) {
		if ((mm_in = mmap(0, chunksize, PROT_READ, MAP_SHARED, fd_in,
		    i * chunksize)) == MAP_FAILED)
			terminate(SE_CANT_MMAP_FILE);
		if ((mm_out = mmap(0, chunksize, PROT_READ | PROT_WRITE,
		    MAP_SHARED, fd_out, i * chunksize)) == MAP_FAILED)
			terminate(SE_CANT_MMAP_FILE);

		(void) memcpy(mm_out, mm_in, chunksize);

		(void) munmap(mm_in, chunksize);
		if (munmap(mm_out, chunksize) == -1)
			terminate(SE_MUNMAP_FAILED, dst);
	}

	if (lastchunk) {
		if ((mm_in = mmap(0, lastchunk, PROT_READ, MAP_SHARED, fd_in,
		    nchunks * chunksize)) == MAP_FAILED)
			terminate(SE_CANT_MMAP_FILE);
		if ((mm_out = mmap(0, lastchunk, PROT_READ | PROT_WRITE,
		    MAP_SHARED, fd_out, nchunks * chunksize)) == MAP_FAILED)
			terminate(SE_CANT_MMAP_FILE);

		(void) memcpy(mm_out, mm_in, lastchunk);

		(void) munmap(mm_in, lastchunk);
		if (munmap(mm_out, lastchunk) == -1)
			terminate(SE_MUNMAP_FAILED, dst);
	}

	(void) close(fd_in);
	(void) close(fd_out);
}
