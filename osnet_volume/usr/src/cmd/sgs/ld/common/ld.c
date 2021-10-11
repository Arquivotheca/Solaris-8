/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)ld.c	1.26	99/01/04 SMI"


#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<stdarg.h>
#include	<string.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<libintl.h>
#include	<locale.h>
#include	<fcntl.h>
#include	<sys/types.h>
#include	<sys/mman.h>
#include	<dlfcn.h>
#include	"conv.h"
#include	"libld.h"
#include	"msg.h"


/*
 * Global variables
 */


int	dbg_mask = 0;		/* lddbg enabled */

static int	class_input_file(int fd);
static int	class_of_first_file(int argc, char ** argv);
static int	prepend_ldoptions(char *, int *, char ***);


/*
 * The following prevent us from having to include ctype.h which defines these
 * functions as macros which reference the __ctype[] array.  Go through .plt's
 * to get to these functions in libc rather than have every invocation of ld
 * have to suffer the R_SPARC_COPY overhead of the __ctype[] array.
 */
extern int		isspace(int);


void
main(int argc, char ** argv, char ** envp)
{
	int		orig_argc = argc;
	char **		orig_argv = argv;
	void *		libld_h;
	void		(*libld_main)(int, char **);
	int		class;
	char *		ld_options;



	/*
	 * Establish locale.
	 */
	(void) setlocale(LC_MESSAGES, MSG_ORIG(MSG_STR_EMPTY));
	(void) textdomain(MSG_ORIG(MSG_SUNW_OST_SGS));

	/*
	 * Check the LD_OPTIONS environment variable, and if present prepend
	 * the arguments specified to the command line argument list.
	 */
	if ((ld_options = getenv(MSG_ORIG(MSG_LD_OPTIONS))) != NULL) {
		/*
		 * prevent modification of actual env strings,
		 * currently libld needs to see the same env
		 * that the front-end sees.
		 */
		ld_options = strdup(ld_options);
		if (prepend_ldoptions(ld_options, &argc, &argv) == -1)
			exit(EXIT_FAILURE);
	}

	/*
	 * Process all input files.
	 */
	class = class_of_first_file(argc, argv);

	/*
	 * dlopen() right libld implementation.
	 * Note:  the RTLD_GLOBAL flag is added to make
	 * ld behave as if libld was bound in.  Support
	 * libraries, like libldstab.so.1, may expect
	 * this, to get at libld_malloc, which they argueably
	 * shouldn't be using anyway.
	 */
	if (class == ELFCLASS64) {
		/*
		 * If we're on a 64-bit kernel, try to exec a full
		 * 64-bit version of ld.
		 */
		conv_check_native(orig_argv, envp, NULL);

		libld_h = dlopen(MSG_ORIG(MSG_LD_LIB64),
				RTLD_LAZY | RTLD_GLOBAL);
	} else {
		/*
		 * Default to Elf32 case for compatibility.
		 */
		libld_h = dlopen(MSG_ORIG(MSG_LD_LIB32),
				RTLD_LAZY | RTLD_GLOBAL);
	}
	if (libld_h == NULL) {
		eprintf(ERR_FATAL, dlerror());
		exit(EXIT_FAILURE);
	}

	/*
	 * ld_main() is what used to be main() in the old ld
	 */
	libld_main = (void (*)(int, char **)) dlsym(libld_h,
		MSG_ORIG(MSG_LD_MAIN));
	if (libld_main == NULL) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_LIB_DLSYM),
		    MSG_ORIG(MSG_STR_EMPTY));
		exit(EXIT_FAILURE);
	}

	/*
	 * Reset the arg counter, and call the regular main().
	 */
	optind = 1;
	libld_main(orig_argc, orig_argv);

	exit(EXIT_SUCCESS);
}


const char *
_ld_msg(Msg mid)
{
	return (gettext(MSG_ORIG(mid)));
}


static int
class_input_file(int fd)
{
	unsigned char ident[EI_NIDENT];

	if (read(fd, ident, EI_NIDENT) < EI_NIDENT)
		return (ELFCLASSNONE);

	if ((ident[EI_MAG0] == ELFMAG0) &&
	    (ident[EI_MAG1] == ELFMAG1) &&
	    (ident[EI_MAG2] == ELFMAG2) &&
	    (ident[EI_MAG3] == ELFMAG3))
		return ((int)ident[EI_CLASS]);

	return (ELFCLASSNONE);
}


/*
 * Prepend environment string as a series of options to the argv array.
 */
static int
prepend_ldoptions(char * ld_options, int * argcp, char *** argvp)
{
	int	nargc;			/* New argc */
	char **	nargv;			/* New argv */
	char *	arg, * string;
	int	count;

	/*
	 * Get rid of leading white space, and make sure the string has size.
	 */
	while (isspace(*ld_options))
		ld_options++;
	if (*ld_options == '\0')
		return (1);

	nargc = 0;
	arg = string = ld_options;
	/*
	 * Walk the environment string counting any arguments that are
	 * separated by white space.
	 */
	while (*string != '\0') {
		if (isspace(*string)) {
			nargc++;
			while (isspace(*string))
				string++;
			arg = string;
		} else
			string++;
	}
	if (arg != string)
		nargc++;

	/*
	 * Allocate a new argv array big enough to hold the new options
	 * from the environment string and the old argv options.
	 */
	if ((nargv = (char **)calloc(nargc + *argcp, sizeof (char *))) == 0)
		return (-1);

	/*
	 * Initialize first element of new argv array to be the first
	 * element of the old argv array (ie. calling programs name).
	 * Then add the new args obtained from the environment.
	 */
	nargv[0] = (*argvp)[0];
	nargc = 0;
	arg = string = ld_options;
	while (*string != '\0') {
		if (isspace(*string)) {
			nargc++;
			*string++ = '\0';
			nargv[nargc] = arg;
			while (isspace(*string))
				string++;
			arg = string;
		} else
			string++;
	}
	if (arg != string) {
		nargc++;
		nargv[nargc] = arg;
	}

	/*
	 * Now add the original argv array (skipping argv[0]) to the end of
	 * the new argv array, and overwrite the old argc and argv.
	 */
	for (count = 1; count < *argcp; count++) {
		nargc++;
		nargv[nargc] = (*argvp)[count];
	}
	*argcp = ++nargc;
	*argvp = nargv;

	return (1);
}


/*
 * Look for an object and type it to determine wether to open
 * the Elf32 or Elf64 libld.
 */
static int
class_of_first_file(int argc, char ** argv)
{

getmore:
	while (getopt(argc, argv, MSG_ORIG(MSG_STR_OPTIONS)) != -1)
		;

	for (; optind < argc; optind++) {
		int	fd;
		int	class;

		/*
		 * If we detect some more options return to getopt().
		 * Checking argv[optind][1] against null prevents a forever
		 * loop if an unadorned `-' argument is passed to us.
		 */
		if (argv[optind][0] == '-') {
			if (argv[optind][1] == '\0')
				continue;
			else
				goto getmore;
		}

		if ((fd = open(argv[optind], O_RDONLY)) == -1) {
			int err = errno;

			eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN),
			    argv[optind], strerror(err));
			exit(EXIT_FAILURE);
		}

		class = class_input_file(fd);
		(void) close(fd);
		if ((class == ELFCLASS32) || (class == ELFCLASS64))
			return (class);
	}

	return (ELFCLASSNONE);
}


/* VARARGS1 */
void
dbg_print(const char * format, ...)
{
	va_list		args;

	(void) fputs(MSG_INTL(MSG_DBG_FMT), stderr);
	va_start(args, format);
	(void) vfprintf(stderr, format, args);
	(void) fprintf(stderr, MSG_ORIG(MSG_STR_NL));
	va_end(args);
}


/*
 * Print a message to stdout
 */
/* VARARGS2 */
void
eprintf(Error error, const char * format, ...)
{
	va_list			args;
	static const char *	strings[ERR_NUM] = {MSG_ORIG(MSG_STR_EMPTY)};

	if (error > ERR_NONE) {
		if (error == ERR_WARNING) {
			if (strings[ERR_WARNING] == 0)
			    strings[ERR_WARNING] = MSG_INTL(MSG_ERR_WARNING);
		} else if (error == ERR_FATAL) {
			if (strings[ERR_FATAL] == 0)
			    strings[ERR_FATAL] = MSG_INTL(MSG_ERR_FATAL);
		} else if (error == ERR_ELF) {
			if (strings[ERR_ELF] == 0)
			    strings[ERR_ELF] = MSG_INTL(MSG_ERR_ELF);
		}
		(void) fputs(MSG_ORIG(MSG_STR_LDDIAG), stderr);
	}
	(void) fputs(strings[error], stderr);

	va_start(args, format);
	(void) vfprintf(stderr, format, args);
	if (error == ERR_ELF) {
		int	elferr;

		if ((elferr = elf_errno()) != 0)
			(void) fprintf(stderr, MSG_ORIG(MSG_STR_ELFDIAG),
			    elf_errmsg(elferr));
	}
	(void) fprintf(stderr, MSG_ORIG(MSG_STR_NL));
	(void) fflush(stderr);
	va_end(args);
}


/*
 *  libld is now dlopen'd so interposition of malloc
 *  needs to be done here to avoid bad free's.
 */

/* ********************************************************** */
/*
 * We interpose on malloc, calloc, & free to use libld's faster
 * memory allocation routines.
 */
#pragma weak	malloc = libld_malloc


/*
 * ld heap management structure
 */
typedef struct ld_heap Ld_heap;
struct ld_heap {
	Ld_heap *	lh_next;
	void *		lh_free;
	void *		lh_end;
};

#define	HEAPBLOCK	0x68000		/* default allocation block size */
#define	HEAPALIGN	0x8		/* heap blocks alignment requirement */

/*
 * List of allocated blocks for link-edit dynamic allocations
 */
static Ld_heap *	ld_heap;

void
/* ARGSUSED 0 */
free(void * ptr)
{
}

void *
calloc(size_t nelem, size_t elsize)
{
	return (libld_calloc(nelem, elsize));
}

void *
realloc(void *ptr, size_t size)
{
	size_t	prev_size;
	void *	vptr;

	if (ptr == NULL)
		return (libld_malloc(size));

	/*
	 * size of the allocated blocks is stored *just* before
	 * the blocks address.
	 */
	prev_size = *((size_t *)ptr - 1);
	/*
	 * If the block actually fits then just return.
	 */
	if (size <= prev_size)
		return (ptr);
	vptr = libld_malloc(size);
	(void) memcpy(vptr, ptr, prev_size);
	return (vptr);
}

/*
 * libld_malloc() and zero_map() are used for both performance
 * and for ease of programining:
 *
 * Performance:
 *	The link-edit is a short lived process which doesn't really
 *	free much of the dynamic memory that it requests.  Because
 *	of this it is much more important that we optimize for the
 *	quick memory allocations then the re-usability of the
 *	memory.
 *
 *	By also mmaping blocks of pages in from /dev/zero we don't
 *	need to waste the overhead of zeroing out these pages
 *	for calloc() requests.
 *
 * Memory Management:
 *	By doing all libld memory management through the ld_malloc
 *	routine it's much easier to free up all memory at the end
 *	by simply unmaping all off the blocks that were mapped in
 *	through zero_map.  This is much simpler then trying to
 *	track all of the libld structures and which were dynamically
 *	allocate and which are actually pointers into the ELF files.
 *
 *	It's important that we can free up all of our dynamic memory
 *	because libld is used by ld.so.1 when it performs dlopen()'s
 *	of relocatable objects.
 *
 * Format:
 *	The memory blocks for each allocation store the size of
 *	the allocation in the first 4 bytes of the block.  The
 *	pointer that is returned by *alloc() is actually the
 *	address of (block + 4):
 *
 *		(addr - 4)	block_size
 *		(addr)		<allocated block>
 *
 *	This is done because in order to implement the realloc()
 *	routine you must know the size of the old block in order
 *	to perform the memcpy().
 */

static void *
zero_map(size_t size)
{
	int	fd;
	void *	addr;
	int	err;

	if ((fd = open(MSG_ORIG(MSG_PTH_DEVZERO), O_RDWR)) == -1) {
		err = errno;
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN),
		    MSG_ORIG(MSG_PTH_DEVZERO), strerror(err));
		return (0);
	}
	if ((addr = mmap(0, size, PROT_READ | PROT_WRITE | PROT_EXEC,
	    MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		err = errno;
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MMAP),
		    MSG_ORIG(MSG_PTH_DEVZERO), strerror(err));
		return (0);
	}
	(void) close(fd);
	return (addr);
}

void *
libld_malloc(size_t size)
{
	Ld_heap *	ldhp = ld_heap;
	void *		vptr;
	size_t		asize = size + sizeof (size_t);

	/*
	 * If this is the first allocation, or the allocation request is greater
	 * than the current free space available, allocate a new heap.
	 */
	if ((ldhp == 0) ||
	    (((unsigned long)ldhp->lh_end -
	    (unsigned long)ldhp->lh_free) <= asize)) {
		Ld_heap *	new_ldhp;
		size_t	ldhpsz = (size_t)S_ROUND(sizeof (Ld_heap), HEAPALIGN);
		size_t	alloc_size = (size_t)S_ROUND((asize + ldhpsz),
				HEAPALIGN);

		/*
		 * Allocate a block that is at minimum 'HEAPBLOCK' size
		 */
		if (alloc_size < HEAPBLOCK)
			alloc_size = HEAPBLOCK;

		if ((new_ldhp = (Ld_heap *)zero_map(alloc_size)) == 0)
			return (0);

		new_ldhp->lh_next = ldhp;
		new_ldhp->lh_free = vptr = (void *)((unsigned long)new_ldhp +
			ldhpsz);
		new_ldhp->lh_end = (void *)((size_t)new_ldhp + alloc_size);
		ldhp = ld_heap = new_ldhp;
	}
	vptr = ldhp->lh_free;
	/*
	 * Assign size to head of allocated block (used by realloc)
	 */
	*((size_t *)vptr) = size;
	vptr = (void *)((uintptr_t)vptr + sizeof (size_t));

	/*
	 * Increment free to point to next available block
	 */
	ldhp->lh_free = (void *)S_ROUND((unsigned long)ldhp->lh_free +
		asize, HEAPALIGN);
	return (vptr);
}
