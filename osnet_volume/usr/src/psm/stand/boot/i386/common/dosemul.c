/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dosemul.c	1.50	99/10/25 SMI"

#include <sys/bsh.h>
#include <sys/types.h>
#include <sys/ramfile.h>
#include <sys/doserr.h>
#include <sys/dosemul.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/booti386.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/bootconf.h>
#include <sys/salib.h>
#include <sys/promif.h>

#define	EQ(a, b) (strcmp(a, b) == 0)

extern struct real_regs	*alloc_regs(void);
extern void free_regs(struct real_regs *);
extern int boldgetproplen(struct bootops *bop, char *name);
extern int boldgetprop(struct bootops *bop, char *name, void *value);
extern int boldsetprop(struct bootops *bop, char *name, char *value);
extern char *boldnextprop(struct bootops *bop, char *prevprop);
extern int bgetproplen(struct bootops *, char *, phandle_t);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int bnextprop(struct bootops *, char *, char *, phandle_t);
extern int dosCreate(char *fn, int mode);
extern int dosRename(char *nam, char *t);
extern int dosUnlink(char *nam);
extern void enter_debug(unsigned long);
extern int ischar();
extern int getchar();
extern void putchar();
extern long lseek();
extern caddr_t rm_malloc(size_t, u_int, caddr_t);
extern int gets();
extern int serial_port_enabled(int port);
extern long strtol(char *, char **, int);
extern unsigned long strtoul(const char *, char **, int);
extern void prom_rtc_date();
extern void prom_rtc_time();
extern int doint_asm();
extern int boot_pcfs_close();
extern int boot_compfs_writecheck();
extern int boot_pcfs_open();
extern int boot_pcfs_write();
extern int volume_specified();
extern void acpi_copy();

extern	struct	bootops *bop;
extern	struct	int_pb ic;

/* Set to "(handle != 1)" disable debug info for writes to stdout */
#define	SHOW_STDOUT_WRITES 1

int int21debug = 0;

/*
 * Definitions and globals for handling files created or accessed from
 * real mode modules. The first five fd's aren't accessible normally
 * because they are reserved for STDIN, STDOUT, etc.  I've left room
 * for them in the global array so that manipulation or re-use of the
 * first few fd's could be implemented if desired in the future.
 */
static dffd_t DOSfilefds[DOSfile_MAXFDS];

int	DOSfile_debug = 0;
ushort	File_doserr;

/*
 *  doint -- Wrapper that calls the new re-entrant doint() but maintains
 *		the previous global structure 'ic' interface.
 */
int
doint(void)
{
	struct real_regs *rr;
	struct real_regs *low_regs = (struct real_regs *)0;
	struct real_regs local_regs;
	int rv;
	extern ulong cursp;
	extern void getesp();

	/*
	 * Any pointer we provide to the re-entrant version of doint
	 * must be accessible from real-mode.  Therefore, if we are
	 * running on a kernel stack, we should alloc some low memory
	 * for our registers pointer, otherwise local stack storage
	 * is okay.
	 *
	 * Note that we can't just always alloc registers because we
	 * start doing doints VERY early on, before the memory allocator
	 * has even been set up!!!
	 */
	getesp();
	if (cursp > TOP_RMMEM) {
		if (!(low_regs = alloc_regs()))
			prom_panic("No low memory for a doint");
		rr = low_regs;
	} else {
		rr = &local_regs;
		bzero((char *)rr, sizeof (struct real_regs));
	}

	AX(rr) = ic.ax;
	BX(rr) = ic.bx;
	CX(rr) = ic.cx;
	DX(rr) = ic.dx;
	BP(rr) = ic.bp;
	SI(rr) = ic.si;
	DI(rr) = ic.di;
	rr->es = ic.es;
	rr->ds = ic.ds;

	rv = doint_asm(ic.intval, rr);

	ic.ax = AX(rr);
	ic.bx = BX(rr);
	ic.cx = CX(rr);
	ic.dx = DX(rr);
	ic.bp = BP(rr);
	ic.si = SI(rr);
	ic.di = DI(rr);
	ic.es = rr->es;
	ic.ds = rr->ds;

	if (low_regs)
		free_regs(low_regs);

	return (rv);
}

/*
 * doint_r - doint reentrant. transition tools so callers keep the same
 * basic model. They just supply a pointer to the ic structure which
 * should now be a stack copy.
 */
int
doint_r(struct int_pb *sic)
{
	struct real_regs *rr;
	struct real_regs *low_regs = (struct real_regs *)0;
	struct real_regs local_regs;
	int rv;
	extern ulong cursp;
	extern void getesp();

	/*
	 * Any pointer we provide to the re-entrant version of doint
	 * must be accessible from real-mode.  Therefore, if we are
	 * running on a kernel stack, we should alloc some low memory
	 * for our registers pointer, otherwise local stack storage
	 * is okay.
	 *
	 * Note that we can't just always alloc registers because we
	 * start doing doints VERY early on, before the memory allocator
	 * has even been set up!!!
	 */
	getesp();
	if (cursp > TOP_RMMEM) {
		if (!(low_regs = alloc_regs()))
			prom_panic("No low memory for a doint");
		rr = low_regs;
	} else {
		rr = &local_regs;
		bzero((char *)rr, sizeof (struct real_regs));
	}

	AX(rr) = sic->ax;
	BX(rr) = sic->bx;
	CX(rr) = sic->cx;
	DX(rr) = sic->dx;
	BP(rr) = sic->bp;
	SI(rr) = sic->si;
	DI(rr) = sic->di;
	rr->es = sic->es;
	rr->ds = sic->ds;

	rv = doint_asm(sic->intval, rr);

	sic->ax = AX(rr);
	sic->bx = BX(rr);
	sic->cx = CX(rr);
	sic->dx = DX(rr);
	sic->bp = BP(rr);
	sic->si = SI(rr);
	sic->di = DI(rr);
	sic->es = rr->es;
	sic->ds = rr->ds;

	if (low_regs)
		free_regs(low_regs);

	return (rv);
}

void
dosemul_init(void)
{
	extern dffd_t DOSfilefds[];

	if (int21debug)
		printf("Setting up int21 bootops.\n");

	hook21();

	/*
	 *  Mark STD file descriptors as in use.
	 */
	DOSfilefds[DOS_STDIN].flags  = DOSFD_INUSE | DOSFD_STDDEV;
	DOSfilefds[DOS_STDOUT].flags = DOSFD_INUSE | DOSFD_STDDEV;
	DOSfilefds[DOS_STDERR].flags = DOSFD_INUSE | DOSFD_STDDEV;
	DOSfilefds[DOS_STDAUX].flags = DOSFD_INUSE | DOSFD_STDDEV;
	DOSfilefds[DOS_STDPRN].flags = DOSFD_INUSE | DOSFD_STDDEV;
}

void
hook21(void)
{
	extern void	int21chute();
	extern ulong	old21vec;

	static short	dosemulon = 0;
	ushort	chuteoff;
	ushort	chuteseg;

	chuteoff = (ulong)int21chute%0x10000;
	chuteseg = (ulong)int21chute/0x10000;

	if (!dosemulon) {
		get_dosivec(0x21, ((ushort *)&old21vec)+1,
		    (ushort *)&old21vec);
		set_dosivec(0x21, chuteseg, chuteoff);
		dosemulon = 1;
	}

}

void
get_dosivec(int vector, ushort *vecseg, ushort *vecoff)
{
	u_short	*vecaddr;

	if (int21debug)
		printf("G-%x", vector);
	vecaddr = (u_short *)(vector*DOSVECTLEN);
	*vecseg = peeks(vecaddr+1);
	*vecoff = peeks(vecaddr);
}

void
set_dosivec(int vector, ushort newseg, ushort newoff)
{
	u_short	*vecaddr;

	if (int21debug)
		printf("S-%x", vector);
	vecaddr = (u_short *)(vector*DOSVECTLEN);
	pokes(vecaddr+1, newseg);
	pokes(vecaddr, newoff);
}

/*
 * We have a set of routines for dealing with file descriptor
 * allocations (to real mode modules).  These all have names
 * of the form DOSfile_xxxx.
 */
int
DOSfile_allocfd(void)
{
	extern dffd_t DOSfilefds[];
	int fdc;

	if (DOSfile_debug || int21debug)
		printf("DOSfile_allocfd ");

	/* Skip over in-use fd's */
	for (fdc = 0; (fdc < DOSfile_MAXFDS &&
	    (DOSfilefds[fdc].flags & DOSFD_INUSE)); fdc++);

	if (fdc == DOSfile_MAXFDS) {
		if (DOSfile_debug || int21debug)
			printf("NOFDS.\n");
		File_doserr = DOSERR_NOMOREFILES;
		return (DOSfile_ERROR);
	} else {
		if (DOSfile_debug || int21debug)
			printf("success (%d).\n", fdc);
		DOSfilefds[fdc].flags |= DOSFD_INUSE;
		return (fdc);
	}
}

int
DOSfile_checkfd(int fd)
{
	return (fd >= 0 && fd < DOSfile_MAXFDS &&
	    (DOSfilefds[fd].flags & DOSFD_INUSE));
}

int
DOSfile_freefd(int fd)
{
	extern dffd_t DOSfilefds[];

	if (DOSfile_debug || int21debug)
		printf("DOSfile_freefd:%d:", fd);

	/*
	 * For now, disallow closing of the STD descriptors.
	 */
	if (fd < DOSfile_MINFD || !DOSfile_checkfd(fd)) {
		if (DOSfile_debug || int21debug)
			printf("Bad handle to free?\n");
		File_doserr = DOSERR_INVALIDHANDLE;
		return (DOSfile_ERROR);
	}
	DOSfilefds[fd].flags = 0;
	return (DOSfile_OK);
}

static int
DOSfile_closefd(int fd)
{
	extern dffd_t DOSfilefds[];
	dffd_t *cfd;
	int rv = -1;

	if (DOSfile_debug || int21debug)
		printf("DOSfile_closefd:%d:", fd);

	if (fd < DOSfile_MINFD || !DOSfile_checkfd(fd)) {
		File_doserr = DOSERR_INVALIDHANDLE;
		if (DOSfile_debug || int21debug)
			printf("Out of range?  ");
	} else {
		cfd = &(DOSfilefds[fd]);
		if (close(cfd->actualfd) < 0) {
			File_doserr = DOSERR_INVALIDHANDLE;
			if (DOSfile_debug || int21debug)
				printf("close failure  ");
		} else
			rv = 0;
	}

	if (rv >= 0)
		(void) DOSfile_freefd(fd);
	return (rv);
}

void
DOSfile_closeall(void)
{
	int f;

	if (DOSfile_debug || int21debug)
		printf("DOSfile_closeall:");

	for (f = DOSfile_MINFD; f <= DOSfile_LASTFD; f++)
		if (DOSfilefds[f].flags & DOSFD_INUSE)
			(void) DOSfile_closefd(f);
}

/*
 * dos_gets
 *
 *	Yet another gets routine.  This one handles the new line,
 *	carriage routine in the way that DOS does.
 */
int
dos_gets(char *str, int n)
{
	int 	c;
	int	t;
	char	*p;

	p = str;
	c = 0;

	while ((t = getchar()) != '\r') {
		putchar(t);
		if (t == '\n')
			continue;
		if (t == '\b') {
			if (c) {
				printf(" \b");
				c--; p--;
			} else
				putchar(' ');
			continue;
		}
		if (c < n - 2) {
			*p++ = t;
			c++;
		}
	}
	putchar('\n');
	putchar('\r');
	*p++ = '\n'; c++;
	*p = '\0';

	return (c);
}

/*
 * dos_formpath
 *
 *	Access to files via dos modules is effectively
 *	subjected to a chroot() because we always prepend the
 *	'boottree' property to any paths accessed by the module.
 *
 *	Note that when dealing with RAMfiles we don't follow through
 *	with prepending the name.  This is because the delayed write
 *	mechanism will automatically put the file under the same
 *	'boottree' root	path we are prepending; hence we don't need
 *	to waste space prepending it to the RAMfile name.
 */
char *
dos_formpath(char *fn, ulong *attr)
{
	static char DOSpathbuf[MAXPATHLEN];
	int pplen, tlen;
	int addslash = 0;
	int isram = 0;

	DOSpathbuf[0] = '\0';

	if (!volume_specified(fn)) {
		if (isram) {
			fn = fn + 2;
			*attr |= DOSFD_RAMFILE;
		}

		addslash = (*fn != '/');
		/*
		 * DOS module file access is rooted at whatever path is
		 * defined in the 'boottree' property.
		 */
		pplen = bgetproplen(bop, "boottree", 0);
		tlen = pplen + addslash + strlen(fn);

		if (tlen >= MAXPATHLEN) {
			printf("Maximum path length exceeded.\n");
			File_doserr = DOSERR_FILENOTFOUND;
			return (NULL);
		} else {
			int fix = 0;

			if (pplen >= 0)
				(void) bgetprop(bop, "boottree",
					DOSpathbuf, pplen, 0);
			if (addslash)
				(void) strcat(DOSpathbuf, "/");
			/*
			 * Don't prepend the boottree if it is already
			 * prepended
			 */
			fix = addslash ? 1 : 0;
			if (pplen > 0)
				pplen--;
			if (strncmp(&DOSpathbuf[fix], fn, pplen-fix) != 0)
				(void) strcat(DOSpathbuf, fn);
			else
				(void) strcpy(DOSpathbuf, fn);
		}
	} else {
		(void) strcpy(DOSpathbuf, fn);
	}

	DOSpathbuf[MAXPATHLEN-1] = '\0';
	return (DOSpathbuf);
}

char *
dosfn_to_unixfn(char *fn)
{
	static char newname[MAXPATHLEN];
	char *ptr;

	/* copy the string converting upper case to lower case and \ to / */
	for (ptr = newname; ptr < &newname[MAXPATHLEN]; ptr++, fn++)
		if ((*fn >= 'A') && (*fn <= 'Z'))
			*ptr = *fn - 'A' + 'a';
		else if (*fn == '\\')
			*ptr = '/';
		else if ((*ptr = *fn) == '\0')
			break;

	newname[MAXPATHLEN - 1] = '\0';
	if (int21debug)
		printf("(new fname \"%s\") ", newname);
	return (newname);
}

/*
 * All the dos function handling routines have names of the form dosxxxx.
 */
void
dosparsefn(struct real_regs *rp)
{
	char *fn;

	fn = DS_SI(rp);

	if (int21debug)
		printf("Parse fn \"%s\"", fn);

	/* XXX for now... */
	AL(rp) = 0;
}

void
doscreatefile(struct real_regs *rp)
{
	char *fn, *afn, *ufn;
	int fd;
	dffd_t *afd;
	ulong attr;

	fn = DS_DX(rp);
	ufn = dosfn_to_unixfn(fn);

	attr = (ulong)CX(rp);
	afn = dos_formpath(ufn, &attr);

	if (int21debug)
		printf("Create \"%s\" ", fn);

	/*
	 *  Allocate a file descriptor.
	 */
	if ((fd = DOSfile_allocfd()) == DOSfile_ERROR) {
		if (int21debug)
			printf("(Failed desc alloc)");
		AX(rp) = File_doserr;
		SET_CARRY(rp);
		return;
	}
	afd = &(DOSfilefds[fd]);

	if (EQ(ufn, DOSBOOTOPC_FN)) {
		attr |= DOSFD_BOOTOPC;
		attr |= DOSFD_RAMFILE;
		attr |= DOSFD_NOSYNC;
		afn = ufn;
	} else if (EQ(ufn, DOSBOOTOPR_FN)) {
		attr |= DOSFD_BOOTOPR;
		attr |= DOSFD_RAMFILE;
		attr |= DOSFD_NOSYNC;
		afn = ufn;
	}

	if ((afd->actualfd = create(afn, attr)) < 0) {
		if (int21debug)
			printf("(Create failed)");
		(void) DOSfile_freefd(fd);
		AX(rp) = File_doserr;
		SET_CARRY(rp);
	} else {
		if (int21debug)
			printf("(success)");
		AX(rp) = (ushort)fd;
		CLEAR_CARRY(rp);
	}
}

void
dosopenfile(struct real_regs *rp)
{
	extern dffd_t DOSfilefds[];
	extern int DOSsnarf_fd;
	char *fn, *ufn, *afn;
	int fd;
	dffd_t *afd;
	ulong mode;

	fn = DS_DX(rp);
	ufn = dosfn_to_unixfn(fn);

	mode = (ulong)AL(rp);
	afn = dos_formpath(ufn, &mode);

	if (int21debug)
		printf("Open \"%s\", %x ", fn, mode);

	/*
	 *  Allocate a file descriptor.
	 */
	if ((fd = DOSfile_allocfd()) == DOSfile_ERROR) {
		if (int21debug)
			printf("(Failed desc alloc)");
		AX(rp) = File_doserr;
		SET_CARRY(rp);
		return;
	}
	afd = &(DOSfilefds[fd]);

	if (EQ(ufn, DOSBOOTOPC_FN)) {
		mode |= DOSFD_BOOTOPC;
		mode |= DOSFD_RAMFILE;
		mode |= DOSFD_NOSYNC;
		afn = ufn;
	} else if (EQ(ufn, DOSBOOTOPR_FN)) {
		mode |= DOSFD_BOOTOPR;
		mode |= DOSFD_RAMFILE;
		mode |= DOSFD_NOSYNC;
		afn = ufn;
	}

	if (mode & DOSFD_BOOTOPR) {
		/*
		 * The bootops result file is a VERY special case.
		 * We don't require it to be opened with a create()
		 * by the realmode modules using it.  We do the
		 * create on their behalf.
		 */
		afd->actualfd = DOSsnarf_fd = create(afn, mode);
	} else {
		afd->actualfd = open(afn, mode);
	}

	if (afd->actualfd < 0) {
		if (int21debug)
			printf("(File not found)");
		(void) DOSfile_freefd(fd);
		AX(rp) = File_doserr;
		SET_CARRY(rp);
	} else {
		if (int21debug)
			printf("(handle %x)", fd);
		AX(rp) = (ushort)fd;
		CLEAR_CARRY(rp);
	}
}

void
dosclosefile(struct real_regs *rp)
{
	if (int21debug)
		printf("Close %x", BX(rp));

	if (DOSfile_closefd(BX(rp)) < 0) {
		if (int21debug)
			printf("(failed)");
		AX(rp) = File_doserr;
		SET_CARRY(rp);
	} else {
		if (int21debug)
			printf("(succeeded)");
		CLEAR_CARRY(rp);
	}
}

void
dosgetchar(struct real_regs *rp)
{
	if (int21debug)
		printf("GETC, NOECHO:");

	AL(rp) = getchar();

	if (int21debug)
		printf("Return %x ", AL(rp));
}

/*
 *  dosgetvolname --
 *	Partial implementation of the DOS Set/Get Volume Information call.
 *	We only fill in the volume name with any valid info.
 *	Serial # setting is unsupported.
 */
void
dosgetvolname(struct real_regs *rp)
{
	extern void boot_compfs_getvolname(char *);
	struct dos_volinfo *dstbuf;

	if (AL(rp) == 1) {
		SET_CARRY(rp);
		return;
	}

	dstbuf = (struct dos_volinfo *)DS_DX(rp);
	dstbuf->serial = 0x54696d48;
	boot_compfs_getvolname(dstbuf->volname);
	(void) strcpy(dstbuf->rest, "FATSO");
	CLEAR_CARRY(rp);
}

void
dosreadfile(struct real_regs *rp)
{
	extern dffd_t DOSfilefds[];
	dffd_t *cfd;
	char *dstbuf;
	int nbytes;
	int handle;
	int cc;

	dstbuf = DS_DX(rp);
	nbytes = CX(rp);
	handle = BX(rp);

	if (int21debug)
		printf("Read(%x, %x, %x) ", handle, dstbuf, nbytes);

	if (!DOSfile_checkfd(handle)) {
		if (int21debug)
			printf("(bad handle)");
		AX(rp) = DOSERR_INVALIDHANDLE;
		SET_CARRY(rp);
		return;
	}
	cfd = &(DOSfilefds[handle]);

	if (handle == DOS_STDIN) {
		cc = AX(rp) = dos_gets(dstbuf, nbytes);
		if (int21debug)
			printf("(gets read %d bytes)", cc);
		CLEAR_CARRY(rp);
		return;
	} else if ((cc = read(cfd->actualfd, dstbuf, nbytes)) < 0) {
		/*
		 * Set carry flag and ax to indicate failure.
		 * XXX fix this, should be:
		 * rp->eax.eax = unix_to_dos_errno(errno);
		 */
		if (int21debug)
			printf("(fs error)");
		AX(rp) = DOSERR_INVALIDHANDLE;
		SET_CARRY(rp);
	} else {
		/* Set carry flag and ax to indicate success */
		if (int21debug)
			printf("(read %x bytes)", cc);
		AX(rp) = (ushort)cc;
		CLEAR_CARRY(rp);
	}
}

void
doswritefile(struct real_regs *rp)
{
	extern dffd_t DOSfilefds[];
	dffd_t *cfd;
	char *srcbuf;
	int nbytes;
	int handle;
	int cc;

	srcbuf = DS_DX(rp);
	nbytes = CX(rp);
	handle = BX(rp);

	if (int21debug && SHOW_STDOUT_WRITES)
		printf("Write(%x, %x, %x) ", handle, srcbuf, nbytes);

	if (!DOSfile_checkfd(handle)) {
		if (int21debug)
			printf("(bad handle)");
		AX(rp) = DOSERR_INVALIDHANDLE;
		SET_CARRY(rp);
		return;
	}
	cfd = &(DOSfilefds[handle]);

	if (((handle == DOS_STDOUT) || (handle == DOS_STDERR)) &&
	    (cfd->flags & DOSFD_STDDEV)) {
		while (nbytes-- > 0)
			putchar(*srcbuf++);
		/* Set carry flag and ax to indicate success */
		AX(rp) = CX(rp);
		CLEAR_CARRY(rp);
	} else if ((cc = write(cfd->actualfd, srcbuf, (u_int)nbytes)) < 0) {
		if (int21debug)
			printf("(write error)");
		AX(rp) = DOSERR_INSUFFICIENT_MEMORY;
		SET_CARRY(rp);
	} else {
		if (int21debug)	printf("(wrote %x bytes)", cc);
		AX(rp) = (ushort)cc;
		CLEAR_CARRY(rp);
	}
}

void
dosrenamefile(struct real_regs *rp)
{
	ulong ignored;
	char *fn, *fufn, *fafn;
	char *tn, *tufn, *tafn;
	char *ff, *tf;
	int rv;

	fn = DS_DX(rp);
	fufn = dosfn_to_unixfn(fn);
	fafn = dos_formpath(fufn, &ignored);
	if ((ff = (char *)bkmem_alloc(MAXPATHLEN)) == (char *)NULL) {
		AX(rp) = DOSERR_INSUFFICIENT_MEMORY;
		SET_CARRY(rp);
		return;
	}
	(void) strcpy(ff, fafn);

	tn = DS_DX(rp);
	tufn = dosfn_to_unixfn(tn);
	tafn = dos_formpath(tufn, &ignored);
	if ((tf = (char *)bkmem_alloc(MAXPATHLEN)) == (char *)NULL) {
		bkmem_free(ff, MAXPATHLEN);
		AX(rp) = DOSERR_INSUFFICIENT_MEMORY;
		SET_CARRY(rp);
		return;
	}
	(void) strcpy(tf, tafn);

	if (int21debug)
		printf("(fn %s:tn %s)", ff, tf);

	if ((rv = rename(ff, tf)) < 0) {
		AX(rp) = File_doserr;
		SET_CARRY(rp);
	} else {
		AX(rp) = (ushort)rv;
		CLEAR_CARRY(rp);
	}
	bkmem_free(ff, MAXPATHLEN);
	bkmem_free(tf, MAXPATHLEN);
}

void
dosunlinkfile(struct real_regs *rp)
{
	ulong ignored;
	char *fn, *ufn, *afn;
	int rv;

	fn = DS_DX(rp);
	ufn = dosfn_to_unixfn(fn);
	afn = dos_formpath(ufn, &ignored);

	if ((rv = unlink(afn)) < 0) {
		SET_CARRY(rp);
	} else {
		CLEAR_CARRY(rp);
	}
	AX(rp) = (ushort)rv;
}

void
dosseekfile(struct real_regs *rp)
{
	dffd_t *cfd;
	off_t reqoff;
	off_t rv;
	int reqtype;
	int handle;

	reqoff = (CX(rp) << 16) + DX(rp);
	handle = BX(rp);

	if (!DOSfile_checkfd(handle)) {
		if (int21debug)
			printf("SEEK %x (bad handle)", handle);
		AX(rp) = DOSERR_INVALIDHANDLE;
		SET_CARRY(rp);
		return;
	}
	cfd = &(DOSfilefds[handle]);

	if (cfd->flags & DOSFD_STDDEV) {
		/* Set carry flag and fill ax to indicate failure */
		AX(rp) = DOSERR_SEEKERROR;
		SET_CARRY(rp);
		return;
	}

	switch (AL(rp)) {
	case DOSSEEK_TOABS:
		reqtype = SEEK_SET;
		break;
	case DOSSEEK_FROMFP:
		reqtype = SEEK_CUR;
		break;
	case DOSSEEK_FROMEOF:
		reqtype = SEEK_END;
		break;
	default:
		if (int21debug)
			printf("(Bad whence %d)", AL(rp));
		AX(rp) = DOSERR_SEEKERROR;
		SET_CARRY(rp);
		return;
	}

	if (int21debug)
		printf("Seek(%x, %x, %x) ", handle, reqoff, reqtype);

	/*
	 * None of of our fs's support SEEK_END directly, so
	 * we must convert it to an absolute seek using the
	 * file size.
	 */
	if (reqtype == SEEK_END) {
		static struct stat fs;

		if (fstat(cfd->actualfd, &fs) != 0) {
			/* Indicate failure */
			if (int21debug)
				printf("(error)");
			AX(rp) = DOSERR_SEEKERROR;
			SET_CARRY(rp);
			return;
		} else {
			reqtype = SEEK_SET;
			reqoff = fs.st_size + reqoff;
		}
	}

	if ((rv = lseek(cfd->actualfd, reqoff, reqtype)) < 0) {
		if (int21debug)
			printf("(fs error)");
		AX(rp) = DOSERR_SEEKERROR;
		SET_CARRY(rp);
	} else {
		/* Clear carry flag and fill fields */
		if (int21debug)
			printf("(sought offset %x)", rv);
		DX(rp) = rv >> 16;
		AX(rp) = rv & 0xFFFF;
		CLEAR_CARRY(rp);
	}
}

void
dosattribfile(struct real_regs *rp)
{
	char *fn;

	fn = DS_DX(rp);

	if (int21debug)
		printf("%s Attr \"%s\" ", AL(rp) ? "Set" : "Get", fn);

	CLEAR_CARRY(rp);
	if (AL(rp)) {
		if (int21debug)
			printf("(fail)");	/* XXX for now */
		AX(rp) = DOSERR_INVALIDHANDLE;
		SET_CARRY(rp);
	} else {
		if (int21debug)
			printf("(succeed)");
		CX(rp) = 1;	/* Read-only */
	}
}

void
dosfiletimes(struct real_regs *rp)
{
	if (int21debug)
		printf("%s date & time ", AL(rp) ? "Set" : "Get");

	/* validate the handle we've received */
	if (!(DOSfile_checkfd(BX(rp)))) {
		if (int21debug)
			printf("(bad handle)");
		SET_CARRY(rp);
		AX(rp) = DOSERR_INVALIDHANDLE;
		return;
	}

#ifdef	notdef
	if (AL(rp)) {
		/* Set the file time */
		/* Check that file is writable */
		/* Compute current time here */
	} else {
		/* Look up the file time */
	}
#endif	/* notdef */
	CX(rp) = 0;
	DX(rp) = 0;
	CLEAR_CARRY(rp);
}

void
dosioctl(struct real_regs *rp)
{
	int handle;

	if (int21debug)
		printf("ioctl %x ", AL(rp));

	if (!DOSfile_checkfd(handle = BX(rp))) {
		if (int21debug)
			printf("(bad handle)");
		AX(rp) = DOSERR_INVALIDHANDLE;
		SET_CARRY(rp);
		return;
	}
	CLEAR_CARRY(rp);
	switch (AL(rp)) {
	case 0x0:
		if (int21debug)
			printf("handle %x ", BX(rp));
		if (handle == DOS_STDOUT)
			DX(rp) = 0xC2;		/* XXX for now */
		else if (handle == DOS_STDIN)
			DX(rp) = 0xC1;		/* XXX for now */
		else if (handle == DOS_STDERR)
			DX(rp) = 0xC0;		/* XXX for now */
		else if (handle == DOS_STDAUX)
			DX(rp) = 0xC0;		/* XXX for now */
		else if (handle == DOS_STDPRN)
			DX(rp) = 0xC0;		/* XXX for now */
		else
			DX(rp) = 0x02;		/* XXX for now */
		if (int21debug)
			printf("(return %x)", DX(rp));
		break;

	case 0x1:
		break;

	case 0x2:
		break;

	case 0x3:
		break;

	case 0x4:
		break;

	case 0x5:
		break;

	case 0x6:
		break;

	case 0x7:
		break;

	case 0x8:
		break;

	case 0x9:
		break;

	case 0xa:
		break;

	case 0xb:
		break;

	case 0xc:
		break;

	case 0xd:
		break;

	case 0xe:
		break;

	default:
		if (int21debug)
			printf("(bad sub function)");
		AX(rp) = DOSERR_INVALIDHANDLE;
		SET_CARRY(rp);
		break;
	}
}

void
dosgetdate(struct real_regs *rp)
{
	ushort y, m, d;

	if (int21debug)
		printf("\ngetdate:");

	prom_rtc_date(&y, &m, &d);
	CX(rp) = y;
	DH(rp) = (unchar)m;
	DL(rp) = (unchar)d;

	if (int21debug)
		printf("Year,Mon,Day = %d,%d,%d\n", CX(rp), DH(rp), DL(rp));
}

void
dosgettime(struct real_regs *rp)
{
	ushort h, m, s;

	if (int21debug)
		printf("\ngettime:");

	prom_rtc_time(&h, &m, &s);

	CH(rp) = (unchar)h;
	CL(rp) = (unchar)m;
	DH(rp) = (unchar)s;
	DL(rp) = 0;	/* RTC Not precise to hundredths */

	if (int21debug)
		printf("Hr,Min,Sec = %d,%d,%d\n", CH(rp), CL(rp), DH(rp));
}

void
dosterminate(struct real_regs *rp)
{
	extern void DOSexe_checkmem();
	extern void comeback_with_stack();
	extern ffinfo *DOS_ffinfo_list;
	ffinfo *ffip;


	if (int21debug)
		printf("Terminate retcode %x", AL(rp));

	/* Close all files */
	DOSfile_closeall();

	/* XXX handle psp stuff... */

	/*
	 * using pointer from freed structure, but we shouldn't
	 * be doing anything else.
	 */
	for (ffip = DOS_ffinfo_list; ffip; ffip = ffip->next) {
		if (ffip->curmatchpath)
			bkmem_free(ffip->curmatchpath, MAXPATHLEN);
		if (ffip->curmatchfile)
			bkmem_free(ffip->curmatchfile, MAXPATHLEN);
		bkmem_free((caddr_t)ffip, sizeof (*ffip));
	}
	DOS_ffinfo_list = 0;

	/* vector the calling program back where it belongs */
	rp->ip = (ushort)((unsigned)comeback_with_stack & (unsigned)0xffff);
	rp->cs = 0;
	DOSexe_checkmem();
}

static void
dosdrvdata(struct real_regs *rp)
{
	/*
	 * Unsure how important it is to get this info right.
	 * The first stat of a directory seems to be the cause
	 * of this call.  Difficult to get the info right as well;
	 * what the heck would we give it for a boot partition
	 * found solely on the UFS root filesystem?
	 */
	static char *media_id = 0;

	if (int21debug)
		printf("get drive info (%x)", DL(rp));

	if (!media_id && !(media_id = rm_malloc(1, 0, 0))) {
		printf("Failed to allocate media byte!\n");
		AL(rp) = 0xFF;
		return;
	}

	if ((DL(rp) == 0) || (DL(rp) == DOS_CDRIVE + 1)) {
		*media_id = (char)0xf8;	/* Fixed disk */
	} else {
		*media_id = (char)0xf9;
	}

	rp->ds = segpart((ulong)media_id);
	BX(rp) = offpart((ulong)media_id);
	CX(rp) = 512;
	AL(rp) = 1;
	DX(rp) = 1;
}

extern struct dos_fninfo *DOScurrent_dta;

void
dosgetdta(struct real_regs *rp)
{
	if (int21debug)
		printf("dosgetdta ");

	rp->es = segpart((ulong)DOScurrent_dta);
	BX(rp) = offpart((ulong)DOScurrent_dta);

	if (int21debug)
		printf("ES:BX=%x:%x ", rp->es, BX(rp));
}

void
dossetdta(struct real_regs *rp)
{

	if (int21debug)
		printf("dossetdta[%x])", DS_DX(rp));

	DOScurrent_dta = (struct dos_fninfo *)DS_DX(rp);
}

void
dosmemstrat(struct real_regs *rp)
{
	if (int21debug)
		printf("dosmemstrat AX=%x,BL=%x ", AX(rp), BL(rp));
	AX(rp) = 0; /* say we use low memory, first fit */
}

/*
 * dospcopy - read/write protected memory from realmode.
 * dospcopy takes a rpcopy structure as noted below, and
 * performs the requested transfer, disabling paging for
 * the duration of the copy to allow access to full 32-bit
 * address space.
 */
struct rpcopy {
	unsigned long src;	/* 32-bit linear src addr */
	unsigned long dest;	/* 32-bit linear dest addr */
	unsigned short nbytes;	/* number of bytes */
	unsigned short flags;	/* width of copy operation */
};

void
dospcopy(struct real_regs *rp)
{
	struct rpcopy *p;
	extern pagestart();
	extern pagestop();
	unsigned int cnt;
	unsigned char *cp;
	unsigned short *wp;
	unsigned long *lp;
	unsigned char *dcp;
	unsigned short *dwp;
	unsigned long *dlp;

	p = (struct rpcopy *)DS_DX(rp);	/* get struct */

	if (int21debug)
		printf("dospcopy %x %x %x %x\n", p->src, p->dest, p->nbytes,
			p->flags);


	/* sanity check */
	if (!p || p->nbytes == 0) {
		AX(rp) = 1;
		return;
	}

	/*
	 * Stop paging for duration of copy. Protected mode default descriptor
	 * allows 4gb access. Note that interrupts are disabled during
	 * int 21 calls, thus interrupts don't have to be explicitely
	 * disabled here.
	 */
	pagestop();

#define	RP_COPY8	0
#define	RP_COPY16	1
#define	RP_COPY32	2

	if (p->flags == RP_COPY8) {
		cp = (unsigned char *)p->src;
		dcp = (unsigned char *)p->dest;
		for (cnt = 0; cnt < p->nbytes; cnt++)
			*dcp++ = *cp++;
	} else if (p->flags == RP_COPY16) {
		wp = (unsigned short *)p->src;
		dwp = (unsigned short *)p->dest;
		for (cnt = p->nbytes / 2; cnt > 0; cnt--)
			*dwp++ = *wp++;
	} else if (p->flags == RP_COPY32) {
		lp = (unsigned long *)p->src;
		dlp = (unsigned long *)p->dest;
		for (cnt = p->nbytes / 4; cnt > 0; cnt--)
			*dlp++ = *lp++;
	} else {
		printf("dospcopy: Invalid flag %x\n", p->flags);
	}

	pagestart();

	AX(rp) = 0; /* success */
}

#ifdef DEBUG
void
prom_enter_mon(void)
{
	enter_debug(0);
}
#endif

#ifdef DEBUG
int
assfail(const char *assertion, const char *filename, int line_num)
{
	char buf[512];

	(void) sprintf(buf, "Assertion failed: %s, file %s, line %d\n",
		assertion, filename, line_num);
	(void) printf("%s", buf);
	enter_debug(0);
	return (0);
}
#endif

int
handle21(struct real_regs *rp)
{
	extern ulong i21cnt;
	extern void rm_check();
	short funreq;
	char *sp;
	int handled = 1;
	static char buf[20];

	/* make sure the realmem heap looks ok */
	rm_check();

	/* get and check property value length */
	if (bgetproplen(bop, "int21debug", 0) < 0)
		int21debug = 0;
	else {
		(void) bgetprop(bop, "int21debug", buf, sizeof (buf), 0);
		int21debug = strtol(buf, 0, 0);
	}

	/*
	 *  Determine which DOS function was requested.
	 */
	i21cnt++;
	funreq = (short)AH(rp);

	if (int21debug > 1)
		printf("{int 21 func %x cs:ip=%x:%x,ds=%x}", funreq,
		    rp->cs & 0xffff, rp->ip & 0xffff, rp->ds & 0xffff);
	if (int21debug > 2) {
		printf("(paused)");
		(void) gets(buf);
	}

	switch (funreq) {

	case 0x02:	/* output character in dl */
		putchar(DL(rp));
		break;

	case 0x07:	/* input character without echo */
	case 0x08:
		dosgetchar(rp);
		break;

	case 0x09:	/* output string in ds:dx */
		sp = DS_DX(rp);
		while (*sp != '$')
			putchar(*sp++);
		break;

	case 0x0b:	/* check input status */
		if (int21debug)
			printf("ischar ");
		AL(rp) = ischar() ? 0xff : 0x0;
		if (int21debug)
			printf("(%x)", AL(rp));
		break;

	case 0x19:	/* get current disk */
		if (int21debug)
			printf("get drive (%x)", DOS_CDRIVE);
		AL(rp) = DOS_CDRIVE;
		break;

	case 0x1a:	/* set address of disk transfer area */
		dossetdta(rp);
		break;

	case 0x1c:	/* info request about disk */
		dosdrvdata(rp);
		break;

	case 0x25:	/* set interrupt vector */
		set_dosivec(AL(rp), rp->ds, DX(rp));
		break;

	case 0x29:	/* parse filename */
		dosparsefn(rp);
		break;

	case 0x2a:	/* get date */
		dosgetdate(rp);
		break;

	case 0x2c:	/* get time */
		dosgettime(rp);
		break;

	case 0x2f:	/* get address of disk transfer area */
		dosgetdta(rp);
		break;

	case 0x30:	/* get MS-DOS version number */
		if (int21debug)
			printf("Get version (3.10)");
		AL(rp) = 0x3;
		AH(rp) = 0xa;
		BH(rp) = 0xff;
		break;

	case 0x33:
		if (AL(rp) <= 1) {
			if (int21debug)
				printf("(Extended break check %s)",
				    AL(rp) ? "Set" : "Get");
			DL(rp) = 0;
		} else {
			/*
			 * There are three other subfunctions that we
			 * don't handle.
			 * 02 = Get and set extended control-break checking
			 * state
			 * 05 = Get Boot Driver
			 * 06 = Get true version number
			 */
			printf("[21,%x @ cs:ip=%x:%x,ds=%x]", funreq,
			    rp->cs & 0xffff, rp->ip & 0xffff,
			    rp->ds & 0xffff);
			handled = 0;
		}
		break;

	case 0x35:	/* get interrupt vector */
		get_dosivec(AL(rp), &(rp->es), &(BX(rp)));
		break;

	case 0x3b:	/* set current directory */
		/* XXX for now just make this look like it succeeds */
		if (int21debug)
			printf("set directory (act like nop)");
		CLEAR_CARRY(rp);
		break;

	case 0x3c:	/* create file */
		doscreatefile(rp);
		break;

	case 0x3d:	/* open file */
		dosopenfile(rp);
		break;

	case 0x3e:	/* close file */
		dosclosefile(rp);
		break;

	case 0x3f:	/* read file */
		dosreadfile(rp);
		break;

	case 0x40:	/* write file */
		doswritefile(rp);
		break;

	case 0x41:	/* unlink file */
		dosunlinkfile(rp);
		break;

	case 0x42:	/* file seek */
		dosseekfile(rp);
		break;

	case 0x43:	/* get/set file attributes */
		dosattribfile(rp);
		break;

	case 0x44:	/* ioctl */
		dosioctl(rp);
		break;

	case 0x48:
		dosallocpars(rp);
		break;

	case 0x49:
		dosfreepars(rp);
		break;

	case 0x4a:
		dosreallocpars(rp);
		break;

	case 0x4b:	/* exec */
		/* XXX fail for now */
		if (int21debug)
			printf("exec \"%s\" (fail)", DS_DX(rp));
		AX(rp) = DOSERR_INSUFFICIENT_MEMORY;
		SET_CARRY(rp);
		break;

	case 0x4c:	/* terminate with extreme prejudice */
		dosterminate(rp);
		break;

	case 0x4d:	/* get return code */
		/* XXX for now */
		if (int21debug)
			printf("get return code (0)");
		AX(rp) = 0;
		break;

	case 0x47:	/* Get current directory */
		dosgetcwd(rp);
		break;

	case 0x4e:	/* find first file */
		dosfindfirst(rp);
		break;

	case 0x4f:	/* find next file */
		dosfindnext(rp);
		break;

	case 0x56:
		dosrenamefile(rp);
		break;

	case 0x57:	/* get/set file date/time */
		dosfiletimes(rp);
		break;

	case 0x69:	/* Get the current volume name */
		dosgetvolname(rp);
		break;

	case 0x58:	/* get/set memory allocation strategy */
		dosmemstrat(rp);
		break;

	case 0xfe:	/* OEM BIOS call */
		/* double-check that the oem bios call is from solaris */
		if (BX(rp) == SOL_ACPI_COPY)	/* copy ACPI board records */
			acpi_copy(rp);
		else if (BX(rp) == SOL_DOS_PCOPY) /* rw protected memory rng */
			dospcopy(rp);
		else if (BX(rp) == SOL_DOS_DEBUG) { /* debugger */
			enter_debug((unsigned long)rp);
			AX(rp) = 0;
		}
		else
			AX(rp) = 0;
		break;

	case 0xff:	/* check for redirected console */
		if (int21debug)
			printf("check for console redirection");
		if (AL(rp) == 0) {
			if (serial_port_enabled(DX(rp))) {
				SET_CARRY(rp);
			} else {
				CLEAR_CARRY(rp);
			}
			break;
		}
		/* fall through */

	default:
		printf("[21,%x @ cs:ip=%x:%x,ds=%x]", funreq,
		    rp->cs & 0xffff, rp->ip & 0xffff, rp->ds & 0xffff);
		handled = 0;
		printf("(paused)");
		(void) gets(buf);
		break;
	}

	return (handled);
}
