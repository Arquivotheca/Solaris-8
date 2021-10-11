/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)load.c	1.33	99/10/07 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/bsh.h>
#include <sys/bootdef.h>
#include <sys/booti386.h>
#include <sys/bootconf.h>
#include <sys/sysenvmt.h>
#include <sys/ramfile.h>
#include <sys/dosemul.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/salib.h>
#include <sys/promif.h>

int loaddebug = 0;
int ldmemdebug = 0;

extern int boldgetproplen(struct bootops *bop, char *name);
extern int boldgetprop(struct bootops *bop, char *name, void *value);
extern int boldsetprop(struct bootops *bop, char *name, char *value);
extern char *boldnextprop(struct bootops *bop, char *prevprop);
extern int bgetproplen(struct bootops *, char *, phandle_t);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int bnextprop(struct bootops *, char *, char *, phandle_t);
extern int doint(void);
extern int dofar(ulong ptr);
extern int dofar_with_stack(ulong ptr);
extern struct bootops	*bop;
extern struct int_pb	ic;
extern int open(), close(), fstat();
extern ssize_t read();
extern long lseek();
extern int bootflags(register char *cp);
extern void unmarkpars(int seg);
extern void markpars(int seg, int size);
extern int parmarklkupsize(int seg);
extern void *memset(void *s, int c, size_t n);
extern void putchar();

extern ulong	dofar_stack;
extern char	filename[];
extern int	boothowto;

extern caddr_t rm_malloc(size_t size, u_int align, caddr_t virt);
extern	void rm_free(caddr_t, size_t);
extern	unchar	*var_ops(unsigned char *, unsigned char *, int);
extern	char	*find_fileext(char *);
extern	int	openfile(char *);
extern	int	(*readfile(int fd, int print))();
extern int rm_resize(caddr_t vad, size_t oldsz, size_t newsz);
extern void exitto();

static	void	DOSexe_reloc(struct dos_exehdr *, caddr_t, caddr_t,
    caddr_t *, ulong *);
static	void	DOSpsp_fill(struct dos_psp *, int);
static	void	cmdfromargs(int, char **, int);
static	int	findnrun(int (*)(), char *, int, char **, int);
static	int	load_pgm(char *, int *, void *, int);
static  int	load_reloc(int fd, struct dos_exehdr *hdrp,
    caddr_t *reclobufp, size_t *reclobufsizep);
static	int	loadnrun_x(int, char **, int);

int	rm_exec(struct bootops *, int, char **);
int	loadnrun_bef(char *, int, char **, int);
int	loadnrun_sys(char *, int, char **, int);
int	loadnrun_com(char *, int, char **, int);
int	loadnrun_exe(char *, int, char **, int);
int	loadnrun_elf(char *, int, char **);

static	char *DOScmdline;
static	char *DOScmdtail;
static	char *DOSenvblk;
static	char *runpath;
static	int  DOStaillen;

/*
 * cmdfromargs
 *	COMs, EXEs, and SYSs need to know what the 'command'
 *	invoked was.  This function fills a global buffer DOScmdline,
 *	with a string constructed from the args passed us by the boot
 *	shell interpreter.  It also computes the tail of the cmdline,
 *	which is the string of all arguments that followed the first.
 *
 *	5/16/95
 *	The function now also fills in the environment block for the
 *	program.  This block will be empty except for trailing
 *	information that will become argv[0].
 */

static void
cmdfromargs(int argc, char *argv[], int prt)
{
	int ac = 1;
	int len = 0;
	char *cp, *tp;

	if (!DOScmdline && !(DOScmdline = rm_malloc(DOS_CMDLINESIZE, 0, 0))) {
		/* Can't do much without a command line buffer! */
		prom_panic("no memory for DOS command line");
	}

	/*
	 *  Build command line string from arguments starting with arg one.
	 *  (Arg zero was the 'run' command to the boot shell)
	 */
	DOStaillen = 0;  	/* Set in case we bomb out early */
	DOScmdtail = NULL;	/* Ditto. */

	--argc;
	len = strlen(argv[ac]);
	(void) strncpy(DOScmdline, argv[ac++], DOS_CMDLINESIZE-1);

	/*
	 *  Convert /'s in command name's path to \'s
	 */
	for (cp = DOScmdline; *cp; cp++)
		if (*cp == '/') *cp = '\\';

	if (len > DOS_CMDLINESIZE-1)
		goto warn;

	/*
	 * Copy rest of arguments as supplied.
	 */
	while (--argc > 0) {
		int nl;
		nl = strlen(argv[ac])+1;
		if (len + nl > DOS_CMDLINESIZE-1)
			goto warn;
		len += nl;
		(void) strcat(DOScmdline, " ");
		(void) strcat(DOScmdline, argv[ac++]);
	}

	if (len + 1 >= DOS_CMDLINESIZE-1)
		goto warn;

	(void) strcat(DOScmdline, "\r");

	/*
	 * Find tail and compute its length
	 */
	tp = &(DOScmdline[strlen(argv[1])]);

	/* skip whitespace (except line-ending \r) trailing cmd's arg 0 */
	while (*tp && iswhitespace(*tp) && *tp != '\r') tp++;

	if (*tp) {
		DOScmdtail = tp;
		while (*tp && *tp != '\r') {
			tp++;
			DOStaillen++;
		}
	}
	/*
	 * else {
	 *  	DOStaillen = 0;
	 *  	DOScmdtail = NULL;
	 * }
	 *  But, we already initialized the variables
	 *  to those values, so don't need else case.
	 */

	if (prt && loaddebug) {
		printf("CmdFromArgs,");
		printf("Full Cmd:%s\n", DOScmdline);
		printf("Cmd Tail:%s\n", DOScmdtail);
	}

	/*
	 *  Now fill in environment.  No actual environment strings
	 *  are passed at this time.  Instead we indicate an empty
	 *  environment. The end of environment is indicated by two
	 *  zero bytes.  In DOS 3.0 and greater (which we are pretending
	 *  to be), the end of environment is expected to be followed
	 *  by argv[0].  Specifically, after the two end of environment
	 *  bytes should be a 1 byte and a zero byte and then the null
	 *  terminated argv[0] string.
	 */
	if (!DOSenvblk &&
	    !(DOSenvblk = rm_malloc(DOS_CMDLINESIZE, PARASIZE, 0))) {
		/* Can't do much without an environment buffer! */
		prom_panic("no memory for DOS environment buffer");
	}

	DOSenvblk[0] = '\0';
	DOSenvblk[1] = '\0';

	if (strlen(runpath) > DOS_CMDLINESIZE-5)
		goto warn;

	DOSenvblk[2] = '\1';
	DOSenvblk[3] = '\0';

	/*
	 * Copy arg 0 to tail of empty environment
	 */
	(void) strcpy(&(DOSenvblk[4]), runpath);

	/*
	 *  Convert /'s in run path to \'s
	 */
	for (cp = &(DOSenvblk[4]); *cp; cp++)
		if (*cp == '/') *cp = '\\';

	if (prt && loaddebug) {
		printf("ARG[0] is %s", &(DOSenvblk[4]));
	}
	return;
warn:
	if (prt) {
		printf("Warning: DOS command line or arg[0] did not ");
		printf("fit in provided buffer\n");
	}
}

struct {
	int		valid;
	caddr_t		stack_bot;
	caddr_t		stack_top;
	int		min_stack;
	caddr_t		end;
} de;

void
DOSexe_checkmem()
{
	/*
	 * check the size of the stack and heap of the currently
	 * running .exe.
	 */
	caddr_t tp;
	int ns, nh;

	if (!de.valid)
		return;
	for (ns = 0, tp = de.stack_bot; *tp++ == 0x5a; ns++)
		;
	for (nh = 0, tp = de.end-1; *tp-- == 0; nh++)
		;
#ifdef notdef
	printf("DOSexe_checkmem: stack_bot = 0x%x end = 0x%x\n",
	    de.stack_bot, de.end);
	printf("DOSexe_checkmem: unused stack = %d, unused heap = %d\n",
	    ns, nh);
#endif
}

static void
DOSexe_reloc(struct dos_exehdr *hp, caddr_t exep, caddr_t relocbuf,
    caddr_t *calladdr, ulong *stackaddr)
{
	ulong *entry;
	ulong offset;
	ushort relseg;
	int nr;

	relseg = segpart((ulong)exep);
	entry = (ulong *)(relocbuf + hp->reloc_off);


	nr = hp->nreloc;
	while (nr) {
		offset = *((ushort *)entry) +
		    PARASIZE*(*(((ushort *)entry)+1));
		*((ushort *)(exep+offset)) += relseg;
		nr--; entry++;
	}

	/* Set correct stack address */
	*((ushort *)stackaddr) = hp->init_sp;
	*(((ushort *)stackaddr)+1) = relseg + hp->init_ss;
	de.stack_bot = (caddr_t)((unsigned)(relseg + hp->init_ss) << 4);
	de.stack_top = de.stack_bot + hp->init_sp;
	(void) memset(de.stack_bot, 0x5a, (unsigned)hp->init_sp&0xffff);

	/* Set correct call address */
	*((ushort *)calladdr) = hp->init_ip;
	*(((ushort *)calladdr)+1) = relseg + hp->init_cs;
}

static void
DOSpsp_fill(struct dos_psp *pspp, int size)
{
	extern struct dos_fninfo *DOScurrent_dta;

	/* fill in psp */
	bzero((caddr_t)pspp, sizeof (*pspp));
	pspp->sig = 0x20cd;
	pspp->nxtgraf = ((ulong)pspp + size + PARASIZE - 1) / PARASIZE;

	/* argv[0] */
	pspp->envseg = segpart((ulong)DOSenvblk);

	DOScurrent_dta = (struct dos_fninfo *)&(pspp->tailc);

	if (DOStaillen < 0x100 && DOScmdtail) {
		pspp->tailc = (unchar)DOStaillen;
		(void) strcpy((char *)pspp->tail, DOScmdtail);
	} else
		pspp->tail[0] = (unchar)'\r';

	get_dosivec(0x22,
	    ((ushort *)&(pspp->isv22)+1),
	    (ushort *)&(pspp->isv22));

	get_dosivec(0x23,
	    ((ushort *)&(pspp->isv23)+1),
	    (ushort *)&(pspp->isv23));

	get_dosivec(0x24,
	    ((ushort *)&(pspp->isv24)+1),
	    (ushort *)&(pspp->isv24));
}

#define	EXE_EXTRA_HEAP_SLUSH	0x4000	/* 0x10000 */
#define	EXE_EXTRA_HEAP_DEC	0x1000

caddr_t
getexespace(size_t minsize, size_t *allocsize)
{
	caddr_t ap;
	int extra;

	extra = EXE_EXTRA_HEAP_SLUSH;
	while (extra >= 0) {
		if (ap = rm_malloc(minsize + extra, 0, 0)) {
			*allocsize = minsize + extra;
			break;
		}
		extra -= EXE_EXTRA_HEAP_DEC;
	}
	return (ap);
}

static int
load_pgm(char *pgm, int *sp, void *bp, int prt)
{
	/*
	 *  Load a DOS-like executeable:
	 *
	 *  Opens the "pgm" file and figures out how big it is.  If "sp"
	 *  is null, reads the entire file into the buffer at "bp".  If
	 *  "sp" points to a non-zero word, reads this number of bytes
	 *  into the buffer at "bp". If "sp" points to a word of zeros,
	 *  allocates a buffer big enough to hold the file, reads it in,
	 *  and stores the buffer address at "*bp".
	 *
	 *  Returns the open file descriptor if it works, RUN_NOTFOUND
	 *  for a missing file, and RUN_FAIL otherwise.  If the
	 *  "prt" flag is non-zero, prints an error message on failure.
	 */

	int fd;

	if ((fd = open(pgm, O_RDONLY)) >= 0) {
		/*
		 *  File opens successfully, let's see how big it is ...
		 */

		caddr_t buf;
		struct stat ss;
		size_t size;
		int fx = 0;

		if (fstat(fd, &ss) == 0) {
			/*
			 *  The number of bytes we're reading may be
			 *  somewhat less than the total file size.
			 *  Actual read length is determined from
			 *  the value of "sp" ...
			 */

			if (sp == 0) {
				/*
				 *  If "sp" is null, read the entire
				 *  file into the buffer provided.
				 */

				if ((size = ss.st_size) > COM_MEM_SIZE) {
					/*
					 *  Only "loadnrun_com" calls us with
					 *  a null "*sp" arg, and we refuse to
					 *  load .com files bigger than 64K!
					 */

					if (prt) {
						printf("Load Error: ");
						printf("\"%s\"; ", pgm);
						printf("too large for .COM\n");
					}
					goto er;
				}

				buf = (caddr_t)bp;

			} else if ((size = *sp) != 0) {
				/*
				 *  If the word at "*sp" is non-zero, read
				 *  that many bytes into the buffer provided.
				 */

				buf = (caddr_t)bp;

			} else if (buf =
			    rm_malloc(size = ss.st_size, PARASIZE, 0)) {
				/*
				 * If "*sp" points to a word of zeros,
				 * allocate a low-memory buffer and read the
				 * file into it.  Return the buffer address
				 * at "*bp".
				 */

				if (prt && ldmemdebug) {
					printf("Pgm Buf @%x, ", buf);
					printf("size=%x\n", ss.st_size);
				}
				*(caddr_t *)bp = buf;
				fx = 1;

			} else {
				/*
				 *  Ran out of memory.  Print error message
				 *  (if requested to do so) and bail out.
				 */

				if (prt) {
					printf("Load Error: \"%s\"; ", pgm);
					printf("out of low memory\n");
				}
				goto er;
			}

			if (read(fd, buf, size) == size) {
				/*
				 *  File is now loaded.  Put file length in
				 *  the word at "*sp" and return the open file
				 *  descriptor (caller may have more data
				 *  to read).
				 */

				if (sp) *sp = ss.st_size;
				goto ex;

			} else if (prt) {
				/*
				 *  Short read.  This is probably an I/O
				 *  error, but it could also happen if the
				 *  file is corrupted.
				 */

				printf("Load Error: \"%s\"; I/O error\n", pgm);
			}

			if (fx)
				rm_free(buf, size);

		} else if (prt) {
			/*
			 *  This shouldn't happen.  If we can open a file, we
			 *  should be able to fstat it!
			 */

			printf("Load Error: cannot stat \"%s\"\n", pgm);
		}

er:		(void) close(fd);
		fd = RUN_FAIL;

	} else {
		fd = RUN_NOTFOUND;
	}
ex:
	return (fd);
}

/*
 * collect the full header.  returns -1 on error, full header size otherwise.
 */

static int
load_reloc(int fd, struct dos_exehdr *hdrp, caddr_t *relocbufp,
    size_t *relocbufsizep)
{
	if (hdrp->nreloc == 0) {
		/* no relocation information */
		*relocbufp = NULL;
		return (EXE_HDR_SIZE);
	}

	/* rewind the file */
	if (lseek(fd, 0, 0) < 0) {
		printf("lseek failed\n");
		return (-1);
	}

	/* figure out how big the full header is */
	*relocbufsizep = hdrp->header_mem * PARASIZE;

	/* get space for the full header */
	if ((*relocbufp = (caddr_t)bkmem_alloc(*relocbufsizep)) == NULL)
		return (-1);

	/* read in full header */
	if (read(fd, *relocbufp, *relocbufsizep) < *relocbufsizep)
		return (-1);

	return (*relocbufsizep);
}

/*
 *  findnrun
 *	Attempt load and run, following any user defined search path.
 */

static int
findnrun(int (*loader)(), char *name, int argc, char *argv[], int prt)
{
	int	success = RUN_NOTFOUND;

	if (!runpath && !(runpath = (char *)bkmem_alloc(MAXPATHLEN))) {
		/* Can't do much without a run path buffer! */
		prom_panic("no memory for run path");
	}

	(void) strcpy(runpath, name);
	success = (*loader)(runpath, argc, argv, prt);
	if (prt && success == RUN_NOTFOUND)
		printf("Run Error: File not found.\n");
	return (success);
}

static int
loadnrun_x(int argc, char **argv, int elf_ok)
{
	char *extension;
	int	(*loaderp)();

	loaderp = (int (*)())0;
	(void) strcpy(filename, argv[1]);

	/*
	 * Default file type will assumed to be elf. Only load
	 * and run something else if an extension exists that
	 * seems to indicate a different type of executable.
	 */
	extension = find_fileext(filename);

	if (extension && (strcmp(extension, "bef") == 0))
		loaderp = loadnrun_bef;
	else if (extension && (strcmp(extension, "sys") == 0))
		loaderp = loadnrun_sys;
	else if (extension && (strcmp(extension, "com") == 0))
		loaderp = loadnrun_com;
	else if (extension && (strcmp(extension, "exe") == 0))
		loaderp = loadnrun_exe;
	else if (elf_ok) {
		loaderp = loadnrun_elf;
	}

	if (loaderp)
		return (findnrun(loaderp, argv[1], argc, argv, elf_ok));
	/* Silently do nothing for no loader ?? */
	return (RUN_OK);
}

/*ARGSUSED*/
int
rm_exec(struct bootops *bop, int argc, char **argv)
{	/* Bootops interface to "run" command	*/
	return (loadnrun_x(argc, argv, 0));
}

void
loadnrun(int argc, char **argv)
{
	(void) loadnrun_x(argc, argv, 1);
}

/*ARGSUSED*/
int
loadnrun_bef(char *befpath, int argc, char **argv, int prt)
{
	caddr_t buf;
	int size = 0;
	int fd, rc = RUN_NOTFOUND;

	if ((fd = load_pgm(befpath, &size, (void *)&buf, prt)) >= 0) {
		/*
		 *  We got the file loaded.  Now verify that it is,
		 *  indeed, a ".bef" module.
		 */

		if (*((ushort *)buf) == BEF_SIG) {
			/*
			 *  It's a .bef file, all right.  Compute the
			 *  load address and branch into it.  The .bef
			 *  will relocate itself to a higher location
			 *  if it probes successfully.
			 */

			caddr_t call_addr = buf + *((u_short *)(buf+8))*16;

			ic.ds = segpart((ulong)call_addr);
			ic.bp = ic.es = ic.si = ic.di = 0;
			ic.intval = ic.ax = ic.bx = 0;
			ic.cx = ic.dx = 0;

			rc = dofar(mk_farp((ulong)call_addr));

		} else if (prt) {
			/*
			 *  The extension says ".bef", but the text doesn't
			 *  look too promising!
			 */

			printf("Load Error: \"%s\" ;", argv[1]);
			printf("bad signature for a .BEF\n");
			rc = RUN_FAIL;
		}

		rm_free(buf, size);
		(void) close(fd);
	}

	return (rc);
}

int
loadnrun_sys(char *syspath, int argc, char **argv, int prt)
{
	int size = 0;
	int fd, rc = RUN_NOTFOUND;
	int bksize;
	struct dos_drvreq *drp;
	struct dos_drvhdr *dhp;

	rc = RUN_FAIL;
	if ((fd = load_pgm(syspath, &size, (void *)&dhp, prt)) >= 0) {
		/*
		 *  We've got the module loaded, now we need a driver work
		 *  area.  We can't put the "dos_drvreq" structure on the
		 *  stack because it has to reside in low memory!
		 */

		if (prt && ldmemdebug)
			printf("SYS@0x%x, size = 0x%x. ", dhp, size);

		if (drp = (struct dos_drvreq *)
		    rm_malloc(sizeof (*drp), 0, 0)) {
			/*
			 *  We've got everything we need now.  Call the driver
			 *  at its strategy and interrupt entry points ...
			 */

			cmdfromargs(argc, argv, prt);

			drp->reqlen = (unchar)sizeof (struct dos_drvreq);
			drp->sector = segpart((ulong)DOScmdline);
			drp->count = offpart((ulong)DOScmdline);
			ic.es = segpart((ulong)drp);
			ic.bx = offpart((ulong)drp);

			ic.bp = ic.si = ic.di = 0;
			ic.intval = ic.ax = ic.cx = ic.dx = 0;
			(void) dofar(MK_FP((ic.ds = segpart((ulong)dhp)),
			    dhp->strat_offset));

			ic.bp = ic.si = ic.di = 0;
			ic.intval = ic.ax = ic.cx = ic.dx = 0;
			rc = dofar(MK_FP((ic.ds = segpart((ulong)dhp)),
			    dhp->intr_offset));

			/*
			 * Use break value to resize the amount
			 * of memory allocated to the .SYS
			 */
			if ((bksize =
			    (mk_flatp(drp->address) - (ulong)dhp)) > 0) {
				if (prt && ldmemdebug) {
					printf("Shrinking SYS down ");
					printf("to 0x%x bytes.\n", bksize);
				}
				(void) rm_resize((caddr_t)dhp, size, bksize);
			} else {
				rm_free((caddr_t)dhp, size);
			}

			rm_free((caddr_t)drp, sizeof (*drp));

		} else if (prt) {

			rm_free((caddr_t)dhp, size);
			printf("Load Error: \"%s\"; out of low memory\n",
			    argv[1]);
			rc = RUN_FAIL;
		}

		(void) close(fd);
	}

	return (rc);
}

int
loadnrun_com(char *compath, int argc, char **argv, int prt)
{
	int rc = RUN_NOTFOUND;
	int fd = -1;
	struct dos_psp *pspp;
	int buf_len = COM_MEM_SIZE + sizeof (struct dos_psp);
	int final_len;

	if (pspp = (struct dos_psp *)rm_malloc(buf_len, PARASIZE, 0)) {
		/*
		 *  Pre-allocate the program buffer so that we know there's
		 *  enough space for the psp ...
		 */
		int comseg = segpart((ulong)pspp);

		if (prt && ldmemdebug)
			printf("COM@0x%x, size = 0x%x. ", pspp, buf_len);

		/*
		 * Make a note of the memory the module is assigned, in
		 * case it does DOS memory management calls later.
		 */
		markpars(comseg, buf_len);

		if ((fd = load_pgm(compath, 0, (void *)&pspp[1], prt)) >= 0) {
			/*
			 *  We should be ready to execute now.  Fix up psp,
			 *  convert load address to segment:offset format,
			 *  then call into the realmode code.
			 */

			ulong far_addr = mk_farp((ulong)pspp);

			cmdfromargs(argc, argv, prt);
			DOSpsp_fill(pspp, buf_len);

			ic.ds = ic.es = (ushort)comseg;
			ic.intval = ic.ax = ic.bx = 0;
			ic.bp = ic.si = ic.di = 0;
			ic.cx = ic.dx = 0;

			/*
			 * XXX need to set ss:sp to top of COM_MEM_SIZE chunk
			 * XXX but dofar doesn't support setting them yet!
			 */

			dofar_stack = far_addr + 0xfffe;
			*((ushort *)(((caddr_t)&pspp[1]) + 0xfffe)) = 0;

			rc = dofar_with_stack((ulong)far_addr + 0x100);
			(void) close(fd);
		}

		/*
		 * We need to lookup the current size of memory allocated
		 * to the COM, as it may have grown during execution.
		 */
		if ((final_len = parmarklkupsize(comseg)) < 0) {
			/* No memory to free!? */
			printf("WARNING: Lost track of COM memory!!");
		} else {
			unmarkpars(comseg);
			rm_free((caddr_t)pspp, final_len);
		}

	} else if (prt) {

		printf("Load Error: \"%s\"; out of low memory\n", argv[1]);
		rc = RUN_FAIL;
	}

	return (rc);
}

int
loadnrun_exe(char *exepath, int argc, char **argv, int prt)
{
	int fd, rc = RUN_NOTFOUND;
	int size = EXE_HDR_SIZE;
	int stkreach;
	union { struct dos_exehdr hdr; char buf[EXE_HDR_SIZE]; } u;

	if ((fd = load_pgm(exepath, &size, (void *)u.buf, prt)) >= 0) {
		/*
		 *  We successfully loaded the header portion.
		 *  Use it to determine how to load the rest
		 *  of the module ...
		 */

		rc = RUN_FAIL;
		if ((u.hdr.sig == EXE_SIG) &&
		    (u.hdr.header_mem >= (EXE_HDR_SIZE/PARASIZE))) {
			/*
			 *  Header has the proper signature and length.
			 *  Now buy a buffer into which we can place
			 *  the program proper.
			 */

			struct dos_psp *pspp;
			caddr_t relocbuf;
			size_t relocbufsize;
			int full_header_size;
			int rdlen;

			if ((full_header_size = load_reloc(fd, &u.hdr,
			    &relocbuf, &relocbufsize)) < 0) {
				printf("Load Error: \"%s\";", argv[1]);
				printf(" I/O error during load_reloc\n");
			}

			rdlen = size - full_header_size;

			/*
			 * Even though rm_malloc would align the size for
			 * us, we need to know exactly how much we can
			 * tell the module he has, so we pre-align the size.
			 */
			size = rdlen + sizeof (*pspp);
			size += u.hdr.require_mem * PARASIZE;
			size = roundup(size, PARASIZE);

			stkreach = u.hdr.init_ss*PARASIZE + u.hdr.init_sp;
			stkreach = roundup(stkreach, PARASIZE);

			/*
			 * Must make certain we protect the .EXE's
			 * runtime stack. Picking the largest size will
			 * guarantee we protect our stack.
			 */
			size = MAX(size, stkreach);

			pspp = (struct dos_psp *)
			    getexespace(size, (size_t *)&size);

			if (prt && ldmemdebug)
				printf("EXE@0x%x, size = 0x%x. ", pspp, size);

			if (pspp) {
				/*
				 *  We've allocated the program text buffer,
				 *  now read it in.
				 */
				int exeseg, finalsz, rs;

				exeseg = segpart((ulong)pspp);

				/*
				 * Make a note of this allocated memory.
				 * During execution, the module will likely try
				 * to resize the block we have allocated it.
				 */
				markpars(exeseg, size);

				rs = read(fd, (char *)&pspp[1],
					(unsigned long)rdlen);
				if (rs == rdlen) {
					/*
					 *  Read complete, now we can set up
					 *  the command line and actually
					 *  invoke the program.
					 */

					caddr_t	call_addr;

					de.valid = 1;
					de.end = (caddr_t)pspp + size;
					cmdfromargs(argc, argv, prt);
					DOSpsp_fill(pspp, size);
					DOSexe_reloc(
					    &u.hdr, (caddr_t)&pspp[1],
					    relocbuf, &call_addr,
					    &dofar_stack);
					if (relocbuf)
						bkmem_free(relocbuf,
						    relocbufsize);
					ic.ds = ic.es = (ushort)exeseg;
					ic.intval = ic.ax = ic.bx = 0;
					ic.bp = ic.si = ic.di = 0;
					ic.cx = ic.dx = 0;

					rc = dofar_with_stack((ulong)call_addr);

				} else if (prt) {
					/*
					 *  I/O error or corrupted file.
					 */

					printf("Load Error: \"%s\";", argv[1]);
					printf(" I/O error\n");
				}

				/*
				 * We need to lookup the current size
				 * of memory allocated to the EXE, as
				 * it may have grown during execution.
				 */
				if ((finalsz = parmarklkupsize(exeseg)) < 0) {
					/* No memory to free!? */
					printf("WARNING: Lost track of EXE ");
					printf("memory!!");
				} else {
					/*
					 * XXX should free up all dosalloc'd
					 * memory here. Make sure we handle
					 * and resizes properly. There is also
					 * allocs that could have come through
					 * bootops.
					 */
					unmarkpars(exeseg);
					if (prt && ldmemdebug) {
						printf("FREE@0x%x, ", pspp);
						printf("size 0x%x.\n", finalsz);
					}
					rm_free((caddr_t)pspp, finalsz);
				}

			} else if (prt) {
				/*
				 *  Can't get a buffer large enough to hold
				 *  the program.
				 */

				printf("Load Error: \"%s\"; ", argv[1]);
				printf("out of low memory\n");
			}

		} else if (prt) {
			/*
			 *  This doesn't look like a ".exe" file to me!
			 */
			printf("Load Error: \"%s\"; ", argv[1]);
			printf("non-standard header\n");
		}

		(void) close(fd);
	}

	return (rc);
}

/*ARGSUSED*/
int
loadnrun_elf(char *elfpath, int argc, char **argv)
{
	extern void setbopvers(void);
	extern void limit_debug(char *, unsigned long);
	extern void p0_mapin(void);
	int (*go2)();
	int fd;
	int print = 0;
	char bargs[256];

	/*
	 * Note how we copy the path into this global 'filename'.
	 * This is REQUIRED, because the iload() routine that does
	 * our dynamic loading of ELFs cleverly(?) refers to the filename
	 * global to determine the module path for loadable pieces.
	 */
	(void) strcpy(filename, elfpath);

	/*
	 * "bootargs" may have been set during execution
	 * of bootrc, so we set boothowto here since
	 * it may affect loading.
	 */
	if (bgetprop(bop, "bootargs", bargs, 0, 0) != -1)
		boothowto = bootflags(bargs);

	/*
	 *  Adjust bootops version if necessary.
	 */
	setbopvers();

	/*
	 * The "whoami" property is initialized to "ufsboot"; however, when we
	 * run a standalone client, the property should be reset to the name
	 * of that program.
	 */
	if ((fd = openfile(filename)) == -1) {
		return (RUN_NOTFOUND);
	}

	go2 = readfile(fd, print);
	(void) close(fd);

	if (go2 == (int(*)())-1) {
		printf("Load Error: Read of %s failed\n", filename);
		return (RUN_FAIL);
	}

	(void) bsetprop(bop, "whoami", filename, 0, 0);

	/* Prevent interaction between boot.bin debug and kernel */
	limit_debug(elfpath, (unsigned long)go2);

	/* boot.bin normally runs with page 0 unmapped to trap
	 * null-pointer references.  Restore page 0 mapping before
	 * running the kernel (and any ELFs that we run first)
	 * because the kernel reads the boot.bin mappings to
	 * generate PAE page tables.  Null pointer protection
	 * will be lost, but the peeks/pokes routines will not
	 * need to parse PAE-style page tables.
	 */
	p0_mapin();

	exitto(go2);

	/*
	 * Note that we aren't expecting to return from the exitto,
	 * but we should return something sensible just in case.
	 */
	return (RUN_FAIL);
}
