/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Solaris x86 realmode device driver management:
 *
 * This file implements the server (framework) side of the extended real-
 * mode driver (".bef") interface. Three routines make up this interface:
 *
 *	LoadBef_befext()  ...  Loads a realmode driver into memory
 *	FreeBef_befext()  ...  Unloads a driver
 *	CallBef_befext()  ...  Invokes a loaded driver at a given entry point
 *
 * We also include a "GetBefPath_befext" routine which, given the name of
 * a realmode driver, will find the fully qualified pathname of that driver.
 */

#ident "@(#)befext.c   1.51   99/11/03 SMI"

#include "types.h"
#include "bios.h"
#include "menu.h"
#include "prop.h"
#include "boot.h"
#include "debug.h"
#include <sys/stat.h>

#include <befext.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>

#include "dir.h"
#include "err.h"
#include "gettext.h"
#include "tty.h"

/*
 * bef_pbase and bef_plen are the paragraph (i.e. segment) address and
 * initial size, in paragraphs, of the current befbuf.  Note that
 * bef_plen does not get adjusted during a mem_adjust callback.
 */
static unsigned short bef_pbase;	/* befbuf base in paragraphs */
static unsigned short bef_plen;		/* befbuf length in paragraphs */
static unsigned short bios_primary_bef_pbase; /* same for bios primary bef */

#undef	roundup
#define	PARASIZE 16
#define	roundup(x, y) (((unsigned long)(x)+(y)-1) & ~((y)-1))

#define	MAXBEFBUFS	2
#define	MAXBEFBUFSIZE	0x2020	/* 128K + 512 bytes in paragraphs */

struct befBufs {
	unsigned short base;
	unsigned short size;
	int free;
} befBufTab[MAXBEFBUFS];

/*
 *  The realmode driver interface is only fully functional when run from the
 *  second level boot (duh!).  The following subroutines provide the mechan-
 *  ism by which the large-model bootconf driver is able to call small-model
 *  drivers (and vice versa).
 */

#include <io.h>		/* Get DOS I/O prototypes			    */
#define	dw(O) jmp $+O	/* Dummy "define word" for built-in assembler!	    */

/* Increment pointer at "p" by 64k bytes ...				    */
#define	p64k(p) (char *)((unsigned long)p + 0x10000000)

/*
 * mem_adjust is called by realmode drivers via the callback vector
 * to request changes to the size of their bef buffer.  This feature
 * no longer really changes the size of the buffer.  If the request
 * is for a size no larger than the buffer we report success, otherwise
 * failure.
 */
int
mem_adjust(unsigned short base, unsigned short len)
{
	int i;

	for (i = 0; i < MAXBEFBUFS; i++) {
		if (base == befBufTab[i].base) {
			return (len <= befBufTab[i].size ? 0 : -1);
		}
	}
	return (-1);
}

/*
 * Allocates a segment of size "len".
 * Returns 0 on success, -1 on failure.
 * *base is set to segment selector on success and largest
 * available block on failure.
 */
int
mem_alloc(unsigned short len, unsigned short *base)
{
	int ans = -1;
	unsigned short b = 0;

#ifdef __lint
	ans = *base + len;
#else
	_asm {
		/*
		 *  Use DOS "get segment" call to allocate
		 *  len paragraphs.
		 */
		push  bx
		mov   bx, len
		mov   ax, 4800h
		int   21h
		mov   b, bx
		jc    skp
		mov   b, ax
		mov   ans, 0
	skp:	pop   bx
	}
#endif
	*base = b;
	return (ans);
}

/*
 * mem_free - free memory allocated via mem_alloc.  No error checking.
 */
void
/*ARGSUSED*/
mem_free(unsigned short base)
{

#ifndef __lint
	_asm {
		push	es
		mov	ax, base
		mov	es, ax
		mov	ax, 4900h
		int	21h
		pop   es
	}
#endif
}

static unsigned short
MaxBufAvail()
{
	unsigned short memavail;

#ifdef __lint
	memavail = 10;
#else
	_asm {
		/*
		 * Use DOS "get segment" call to find
		 * size of largest available segment
		 */
		push  bx
		mov   bx, 0ffffh
		mov   ax, 4800h
		int   21h
		mov   memavail, bx
		pop   bx
	}
#endif
	return (memavail);
}

void
/*ARGSUSED*/
fini_befbufs(void *arg, int exitcode)
{
	int i;

	for (i = 0; i < MAXBEFBUFS; i++) {
		if (befBufTab[i].free) {
			befBufTab[i].free = 0;
			mem_free(befBufTab[i].base);
		}
	}
}

/*
 * Allocate all bef buffers on the first request.
 *
 * Allocate a number of paragraphs specified by a property before
 * allocating the bef buffers.  Then allocate equal sized buffers
 * as large as possible up to the maximum.
 *
 * Normally we expect the property not to be set and the buffers
 * to be maximum size.  If a configuration requires an unusal
 * amount of allocated memory, or further development changes
 * memory usage significantly, bootconf could run out of mallocable
 * memory, in which case the property value can be used to change
 * the balance between mallocable memory and bef buffers.  If that
 * reduces the bef buffer size too far then large befs could fail to
 * load.  If we reach that point it is probably time to move parts
 * of bootconf into boot.bin or modules that can run in bef buffers.
 *
 * Arrange to free any unallocated bef buffers on exit.  At present
 * there is no shortage of memory once bootconf exits, but that could
 * change.
 */
void
PreAllocBefBufs(void)
{
	int i;
	unsigned short size;
	unsigned short reserve_base = 0;
	char *propstr;

	if (propstr = read_prop("befbuf-memreserve", "options")) {
		size = atoi(propstr);
		if (size) {
			if (mem_alloc(size, &reserve_base) != 0)
				reserve_base = 0;
		}
	}

	/*
	 * When boot.bin starts bootconf the majority of free realmode
	 * memory is in one big chunk.
	 */
	size = MaxBufAvail();
	debug(D_MEMORY, "\nPreAllocBefBufs: available memory = %x\n",
	    size);
	size /= MAXBEFBUFS;
	if (size > MAXBEFBUFSIZE)
		size = MAXBEFBUFSIZE;
	for (i = 0; i < MAXBEFBUFS; i++) {
		if (mem_alloc(size, &befBufTab[i].base) == 0) {
			befBufTab[i].size = size;
			befBufTab[i].free = 1;
			debug(D_MEMORY, "PreAllocBefBufs: buffer %d %x at "
				"%x\n", i, size, befBufTab[i].base);
		} else {
			debug(D_MEMORY, "PreAllocBefBufs: failed to allocate "
				"%x for buffer %d, %x available\n",
				size, i, MaxBufAvail());
			befBufTab[i].base = befBufTab[i].size = 0;
			befBufTab[i].free = 0;
		}
	}
	if (reserve_base)
		mem_free(reserve_base);

	/* have our "free" routine called when the program completes */
	ondoneadd(fini_befbufs, 0, CB_EVEN_FATAL);
}

/*
 * Preallocate BEF buffers on the first call.  Allocate the first
 * free one when called.
 */
GetBefBuf(unsigned long n)
{
	static int firsttime = 1;
	unsigned short para_size;
	int i;

	if (firsttime) {
		firsttime = 0;
		PreAllocBefBufs();
	}

	para_size = (unsigned short)((n + 15) >> 4);
	for (i = 0; i < MAXBEFBUFS; i++) {
		if (befBufTab[i].free && para_size <= befBufTab[i].size) {
			befBufTab[i].free = 0;
			bef_pbase = befBufTab[i].base;
			bef_plen = befBufTab[i].size;
			debug(D_MEMORY, "GetBefBuf: allocating buffer %d\n",
				i);
			return (1);
		}
	}
	debug(D_MEMORY | D_ERR, "GetBefBuf: no buffer available\n");
	return (0);
}

static void
FreeBefBuf(unsigned int p)
{
	int i;

	for (i = 0; i < MAXBEFBUFS; i++) {
		if (befBufTab[i].base == p) {
			if (befBufTab[i].free) {
				debug(D_ERR, "Attempt to re-free driver"
					"buffer %x.\n", p);
				return;
			}
			befBufTab[i].free = 1;
			return;
		}
	}
	debug(D_ERR, "Attempt to free non driver buffer %x.\n", p);
}

static char *
repartition(unsigned int s)
{
	/*
	 * Given the segment address of the driver buffer, examine the
	 * driver header and calculate the address of the start of the
	 * driver itself.  Return the address with a 0 offet.
	 *
	 * Header length in paragraphs is at header offset 8.
	 */

	char *p = 0;

#ifdef __lint
	p += s;
#else
	_asm {
		push  bx
		push  es
		mov   ax, s
		mov   es, ax
		xor   bx, bx
		add   ax, es:[bx+8]
		mov   word ptr [p+2], ax
		mov   word ptr [p], 0
		pop   es
		pop   bx
	}
#endif
	return (p);
}

static int
/*ARGSUSED0*/
callout(int op, char *ep, struct bef_interface *ap)
{
	/*
	 * Call into the driver:
	 * Pass the address of the bef interface struct
	 * at "ap" to the driver via "ES:DI"
	 */

	int x = 0;
#ifndef __lint
	_asm {
		les   di, ap;			ES:DI will contain ap
		mov   ax, op;			Option to %ax
		call  ep;			call out
		mov   x, ax;			return result
	}
#endif
	return (x);
}

typedef struct {
	unsigned short sig;		/* EXE program signature */
	unsigned short nbytes;		/* number of bytes in last page */
	unsigned short npages;		/* number of 512-byte pages */
	unsigned short nreloc;		/* number of relocation table entries */
	unsigned short header_mem;	/* header size in paragraphs */
	unsigned short require_mem;	/* required memory size in paragraphs */
	unsigned short desire_mem;	/* desired memory size in paragraphs */
	unsigned short init_ss;		/* in relative paragraphs */
	unsigned short init_sp;
	unsigned short checksum;
	unsigned short init_ip;		/* at entry */
	unsigned short init_cs;		/* in paragraphs */
	unsigned short reloc_off;	/* offset of first reloc entry */
	unsigned short ovly_num;	/* overlay number */
	unsigned short reserved[16];
	unsigned long  newexe;		/* offset to additional header */
} exehdr_t;

long
LoadSize_befext(int fd, long st_size)
{
	/*
	 * Given the current size of the file from stat() see if
	 * we need more space by looking at the exec header for the
	 * bef. Currently .befs don't have any bss and using the file
	 * size is okay. We'll be changing the drivers to use a bss segment.
	 */

	exehdr_t h;
	int x = _read(fd, &h, sizeof (h));
	(void) _lseek(fd, 0, 0); /* Rewind to start of file for text read */

	if (x == sizeof (h)) {
		/*
		 * Make sure that we rewind the file so that other reads
		 * get all of the data available.
		 */

		unsigned long n;
		(void) _lseek(fd, 0, 0);

		n = (((unsigned long)h.npages * 512L) + h.nbytes)
		    + (((unsigned long)h.header_mem
			    + (unsigned long)h.require_mem) * 16L);

		if (n > (unsigned long)st_size) st_size = n;
	}

	return (st_size);
}

char *
LoadBef_befext(char *file)
{
	/*
	 *  Load a realmode driver:
	 *
	 *  This routine reads the realmode driver (".bef" file) contained
	 *  in the input "file" into a dynamically allocated buffer and returns
	 *  the load point to the caller.  If we're unable to load the driver,
	 *  or if after loading it we can't find the appropriate magic numbers,
	 *  we set the "errno" word and return a null pointer.
	 */

	int fd = -1;
	struct _stat st;

	errno = 0;

	if (((fd = _open(file, _O_RDONLY+_O_BINARY)) >= 0) &&
	    _fstat(fd, &st) == 0) {
		/*
		 *  The driver file opened, and we now know how big it is.
		 *  Allocate an execution buffer ...
		 */

		char *xpt;
		long blen;
		long len = st.st_size;

		blen = LoadSize_befext(fd, len);
		if (GetBefBuf(blen)) {
			/*
			 *  We just allocated a buffer that should be large
			 *  enough to hold the .bef file.  Read the entire
			 *  .bef file into this buffer.
			 */

			char *lpt;
			char *cp = strrchr(file, '/');
			if (!cp++) cp = file;

			while (*cp && (*cp++ != '.'))
				continue;
			lpt = MK_PTR(bef_pbase, 0);

			for (xpt = lpt; len > 0; xpt = p64k(xpt)) {
				/*
				 *  Read the driver into the "befbuf".  Since
				 *  drivers may be more than 64k-1 bytes long
				 *  (the maximum we can transfer in one call
				 *  to "read") break the request into 32k-byte
				 *  chunks.  We read two of these chunks on
				 *  each iteration of this loop and then use
				 *  the magic "p64k" macro to advance the buf-
				 *  fer pointer into the next 64k segment.
				 */

				unsigned n = 0x8000;
				if (len < n) n = len;
				if (_read(fd, xpt, n) != n) goto nox;

				if ((len -= 0x8000) > 0) {
					/*
					 *  The first 32k read in OK, now do
					 *  the next chunk.
					 */

					if (len < n) n = len;
					n -= _read(fd, xpt+0x8000, n);
					if (n != 0) goto nox;
					len -= 0x8000;
				}
			}

			if (*(short *)lpt == BEF_SIGNATURE) {
				/*
				 *  So far so good.  We don't check for the
				 *  presence of a .bef extension magic number
				 *  because we don't know what the caller wants
				 *  to do with this thing!
				 */

				(void) _close(fd);

				return (repartition(bef_pbase));

			} else {
				/*
				 *  This doesn't look like a realmode driver
				 *  to me!  Set the "errno" word before falling
				 *  into the common erro exit code.
				 */

			nox:	errno = ENOEXEC;
				FreeBef_befext();
			}

		} else {
			/*
			 *  Caller is hogging all the memory (or the driver
			 *  is unusally large).  We can't do anything without
			 *  buffer big enough to hold the driver!
			 */

			errno = ENOMEM;
		}
	}

	if (fd >= 0)
		(void) _close(fd);
	return (0);
}

void
FreeBef_befext()
{
	/*
	 *  Unload a realmode driver:
	 *
	 *  This routine may be used to unload realmode drivers loaded by
	 *  "LoadBef".  We only allow one driver to be loaded at any given
	 *  time.
	 */

	if (bef_pbase) {
		/*
		 *  There's nothing to do unless we have a ".bef" file loaded.
		 *  If we do, free up the buffer that contains it.
		 */

		FreeBefBuf(bef_pbase);
		bef_pbase = 0;
	}
}

int
CallBef_befext(int op, struct bef_interface *bif)
{
	/*
	 *  Call into driver:
	 *
	 *  This routine is used to call into a driver loaded by "LoadBef".
	 *  The caller specifies the "op"eration the driver is to perform and
	 *  a pointer to the callback interface structure ("bif").
	 *
	 *  Returns the value of %ax when the driver exits.  If an error is
	 *  detected, this will be one of the "BEF_*" error codes listed in
	 *  <befext.h>.
	 */

	int rval = BEF_LOADFAIL;
	int offset = -1;

	if ((bef_pbase != 0) &&
	    (!op || (*(long *)(repartition(bef_pbase) + BEF_EXTMAGOFF) ==
			BEF_EXTMAGIC))) {
		/*
		 *  Buffer has been allocated, there's a driver in it, and
		 *  the driver supports the requested "op"eration.  Convert
		 *  the option code to an entry point offset ("x" register).
		 */

		switch (op) {
		case BEF_LEGACYPROBE:
		case BEF_INSTALLONLY:
			/*
			 *  We know about legacy probes and install only mode.
			 *  These are implemented via the .bef extension entry.
			 */

			offset = BEF_EXTENTRY;
			break;

		case BEF_PROBEINSTAL:
			/*
			 *  The old "probe & install" function is supported
			 *  at the initial entry point (offset 0).
			 */

			offset = 0;
			break;

		default:
			/*
			 *  Everything else is garbage ...
			 */

			break;
		}
	}

	if (offset >= 0) {
		bif->mem_base = bef_pbase;
		bif->mem_size = bef_plen;
		rval = callout(op, repartition(bef_pbase) + offset, bif);
	}

	/*
	 * If the bef has printed anything then request
	 * that the user hit enter before overwriting the
	 * output with the next menu.
	 */
	if (Bef_printfs_done_tty) {
		(void) iprintf_tty(gettext(
		    "End of bef printfs, hit Enter to continue\n"));
		while (getc_tty() != '\n') {
			beep_tty();
		}
		Bef_printfs_done_tty = 0;
	}

	return (rval);
}

char *
GetBefPath_befext(char *file)
{
	/*
	 *  Find driver path:
	 *
	 *  This routine will search the various realmode driver directories
	 *  looking for the driver with the specified "file"name.  When (and
	 *  if) we find it, we return a pointer to the complete path name of
	 *  that driver.  Returns NULL if the driver doesn't exist.
	 */

	DIR *dp;
	char *fp, *cp = 0;
	struct dirent *dep;
	static char *pathname;
	static int pathlen;

	if (!pathname) {		/* error check buf overflow */
		pathlen = MAXLINE;
		pathname = malloc(pathlen);
		if (!pathname)
			MemFailure();
	}

	(void) strcpy(pathname, "solaris/drivers");
	if (!(fp = strrchr(file, '/'))) fp = file;

	if (dp = opendir(pathname)) {
		/*
		 *  Got the drivers directory open, now let's search all
		 *  subdirectories for the one containing the desired driver.
		 */

		struct _stat st;
		char *xp = strrchr(pathname, 0);

		while (!cp && (dep = readdir(dp))) {
			/*
			 *  Check this directory and each of its subdirectories
			 *  for the presence of the named file ..
			 */
			if (strlen(pathname) + 16 > pathlen) {
				/* error check buf overflow */
				pathlen += MAXLINE/2;
				pathname = realloc(pathname, pathlen);
				if (!pathname)
					MemFailure();
				xp =  strrchr(pathname, 0);
			}

			if (strcmp(dep->d_name, "..") != 0) {
				/*
				 *  .. but don't climb back up the directory
				 *  tree!  We check for the presence of the
				 *  requested file by concatenating its name
				 *  onto the current subdirectory name and
				 *  issuing a "stat" call.
				 */

				char *ip = dep->d_name;
				char *op = xp;
				*op++ = '/';

				while (*ip++) *op++ = tolower(ip[-1]);
				*op++ = '/';

				for (ip = fp; *ip; ip++) *op++ = tolower(*ip);
				*op = '\0';

				if (strcmp(op-4, ".bef"))
					(void) strcat(op, ".bef");
				if (!_stat(pathname, &st)) cp = pathname;
			}
		}

		closedir(dp);
	}

	return (cp);
}

void
save_biosprim_buf_befext()
{
	bios_primary_bef_pbase = bef_pbase;
	bef_pbase = 0;
}

void
free_biosprim_buf_befext(void)
{
	FreeBefBuf(bios_primary_bef_pbase);
	bios_primary_bef_pbase = 0;
}
