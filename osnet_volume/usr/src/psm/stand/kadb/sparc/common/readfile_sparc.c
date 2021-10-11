/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)readfile_sparc.c	1.28	99/03/23 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/reboot.h>
#include <sys/bootconf.h>
#include <sys/debug/debugger.h>
#include <sys/elf.h>
#include <sys/elf_notes.h>
#include <sys/link.h>
#include <sys/auxv.h>
#include "adb.h"
#include <sys/modctl.h>
#include <sys/fcntl.h>

char bootname[50];	/* space for "/unix" and room to patch to whatever */
char target_bootname[80];	/* the actual thing we load */
char target_bootargs[80];	/* the arguments we pass to it */
struct bootops *bootops;
extern int use_align;
extern int pagesize;
extern int interactive;
extern char *prompt;
extern char *strrchr();
extern char *rindex();

extern void
krtld_debug_setup(Elf32_Ehdr *ehdr, char *shdrs, Elf32_Shdr *symhdr,
	char *text, char *data, u_int text_size, u_int data_size);

extern int get_path_name(char *filename);

static void parseparam(char *, int, char *, char *);

func_t iload(char *, Elf32_Phdr *, Elf32_Phdr *, auxv_t **);
static caddr_t segbrk(caddr_t *, unsigned int, int);
static char *getmodpath(char *);

Elf32_Boot	*elfbootvec;
char		*module_path;	/* path for kernel modules */
int		howto;
int		npagesize;
char		*cpulist = NULL;

#define	ALIGN(x, a)	\
	((a) == 0 ? (int)(x) : (((int)(x) + (a) - 1) & ~((a) - 1)))

#define	__BOOT_NAUXV_IMPL	20

/*
 * Prompt for name of file to be read into memory for debugging.
 */
func_t
load_it(arg)
	register char **arg;
{
	register int io;
	func_t go2;
	char bargs[50];
	extern char myname_default[];

	*arg = "";

	if (*myname == '\0') {
		if (BOP_GETPROP(bootops->bsys_super, "whoami", myname) == -1)
			(void) strcpy(myname, myname_default);
		/*
		 * Only give the last component of the path as the prompt.
		 */
		if ((prompt = rindex(myname, '/')) == NULL)
			prompt = myname;
		else
			prompt++;
	} else if (interactive == 0) {
		/*
		 * 2nd time thru and we are not interactive,
		 * return fatal error code back to caller.
		 */
		return ((func_t)-2);
	}

	if (BOP_GETPROP(bootops->bsys_super, "boot-args", bargs) != -1)
		howto = bootflags(bargs);
	else {
		prom_printf("missing boot-args property. boot problem?\n");
		howto = 0;
	}

	interactive = (RB_DEBUG | RB_KRTLD) & howto;

	/*
	 * Now we have to ask for the name of the program to load
	 * if we are interactive. If not, we check to see if someone
	 * has already patched the default bootname string, and
	 * if they haven't, we ask boot for the name.
	 */
	*aline = '\0';
	if (interactive) {
		printf("%s: ", prompt);
		getsn(aline, sizeof (aline));
	}

	if (*aline == '\0') {
		if (*bootname != '\0') {
			register char *s, *p;

			s = aline;
			p = bootname;
			while (*p)
				*s++ = *p++;
			*s = '\0';
		} else
			(void) BOP_GETPROP(bootops->bsys_super,
			    "default-name", aline);
		printf("%s: %s\n", prompt, aline);
	}

	/*
	 * It looks like we have to expand the name of the kernel to
	 * boot for the KBI stuff.
	 */
	if (get_path_name(aline) != 0)
		printf("Path name expansion failed:%s:\n", aline);

	parseparam(aline, howto, target_bootname, target_bootargs);
	*arg = target_bootname;
	io = open(aline, 0);
	if (io >= 0) {
		go2 = readfile(io, 1, aline);
		close(io);		/* Done with it. */
	} else {
		printf("boot failed\n");
		go2 = (func_t)-1;
	}
	return (go2);
}

/*
 * Macros to add attribute/values
 * to the ELF bootstrap vector
 * and the aux vector.
 */
#define	AUX(p, a, v)	{ (p)->a_type = (a); \
			((p)++)->a_un.a_val = (long)(v); }

#define	EBV(p, a, v)	{ (p)->eb_tag = (a); \
			((p)++)->eb_un.eb_val = (Elf32_Word)(v); }

/*
 * Read in a Unix executable file and return its entry point.
 * Handle the various a.out formats correctly.
 * "Io" is the standalone file descriptor to read from.
 * Print informative little messages if "print" is on.
 * Returns -1 for errors.
 */
func_t
readfile(io, print, name)
	register int io;
	int print;
	char *name;
{
	Elf32_Ehdr elfhdr;
	Elf32_Phdr *phdr;
	Elf32_Nhdr *nhdr;
	int nphdrs, phdrsize;
	caddr_t allphdrs, err;
	int i;
	u_int loadaddr, size, base;
	u_int entrypt;
	int off;
	caddr_t	namep, descp;
	u_int align, prog_size = 0;
	static char dlname[MODMAXNAMELEN];
	int interp = 0;
	unsigned int dynamic;
	Elf32_Phdr *tphdr;
	Elf32_Phdr *dphdr;
	extern u_int icache_flush;

	/*
	 * Do things one at a time, and be more forthright at the points
	 * of failure.  The previous version of this function was too hard
	 * to debug.
	 */
	if (read(io, &elfhdr, sizeof (elfhdr)) != sizeof (elfhdr)) {
		printf("Unable to read ELF header");
		goto elferr;
	}
	if (elfhdr.e_ident[EI_MAG0] != ELFMAG0 ||
		elfhdr.e_ident[EI_MAG1] != ELFMAG1 ||
		elfhdr.e_ident[EI_MAG2] != ELFMAG2 ||
		elfhdr.e_ident[EI_MAG3] != ELFMAG3 ||
		elfhdr.e_type != ET_EXEC || elfhdr.e_version != EV_CURRENT) {
		printf("Erroneous ELF header");
		goto elferr;
	}
	if (elfhdr.e_phnum == 0) {
		printf("No ELF program header");
		goto elferr;
	}

	entrypt = (u_int)elfhdr.e_entry;

	/*
	 * Allocate and read in all the program headers.
	 */
	allphdrs = NULL;
	nhdr = NULL;
	nphdrs = elfhdr.e_phnum;
	phdrsize = nphdrs * elfhdr.e_phentsize;
	allphdrs = (caddr_t)malloc(phdrsize);
	if (allphdrs == NULL) {
		printf("Unable to malloc proghdr copy");
		goto elferr;
	}
	if (lseek(io, elfhdr.e_phoff, 0) == -1) {
		printf("Unable to find ELF program header");
		goto elferr;
	}
	if (read(io, allphdrs, phdrsize) != phdrsize) {
		printf("Unable to read ELF program header");
		goto elferr;
	}

	/*
	 * First look for PT_NOTE headers that tell us what pagesize to
	 * use in allocating program memory.
	 */
	npagesize = 0;
	for (i = 0; i < nphdrs; i++) {
		phdr = (Elf32_Phdr *)(allphdrs + elfhdr.e_phentsize * i);
		if (phdr->p_type != PT_NOTE)
			continue;
		nhdr = (Elf32_Nhdr *)malloc(phdr->p_filesz);
		if (nhdr == NULL) {
			printf("Unable to malloc notehdr copy");
			goto elferr;
		}
		if (lseek(io, phdr->p_offset, 0) == -1) {
			printf("Unable to find ELF note header");
			goto elferr;
		}
		if (read(io, (caddr_t)nhdr, phdr->p_filesz) != phdr->p_filesz) {
			printf("Unable to read ELF note header");
			goto elferr;
		}
		namep = (caddr_t)(nhdr + 1);
		if (nhdr->n_namesz == strlen(ELF_NOTE_SOLARIS) + 1 &&
		    strcmp(namep, ELF_NOTE_SOLARIS) == 0 &&
		    nhdr->n_type == ELF_NOTE_PAGESIZE_HINT) {
			descp = namep + roundup(nhdr->n_namesz, 4);
			npagesize = *(int *)descp;
		}
		free(nhdr);
		nhdr = NULL;
	}

	/*
	 * Next look for PT_LOAD headers to read in.
	 */
	if (print)
		printf("Size: ");
	for (i = 0; i < nphdrs; i++) {
		phdr = (Elf32_Phdr *)(allphdrs + elfhdr.e_phentsize * i);
		if (phdr->p_type == PT_LOAD) {
			if (lseek(io, phdr->p_offset, 0) == -1) {
				printf("Unable to find program section");
				goto elferr;
			}
			if (phdr->p_flags & PF_X) {
				if (print)
					printf("%d+", phdr->p_filesz);

				if (phdr->p_flags & PF_W)
					dphdr = phdr;
				else
					tphdr = phdr;
				/*
				 * If we found a new pagesize above, use
				 * it to adjust the memory allocation.
				 */
				loadaddr = phdr->p_vaddr;
				if (use_align && npagesize != 0)
					align = npagesize;
				else
					align = pagesize;
				off = loadaddr & (align - 1);
				size = roundup(phdr->p_memsz + off, align);
				base = loadaddr - off;

				err = BOP_ALLOC(bootops, (caddr_t)base, size,
					use_align ? align : BO_NO_ALIGN);
				if (err != (caddr_t)base)
					prom_panic("Unable to get memory "
					    "for text_seg.\n");

				prog_size += phdr->p_memsz;
			} else if (phdr->p_vaddr == 0) {
				/*
				 * It's a PT_LOAD segment that is
				 * not executable and has a vaddr
				 * of zero.  We allocate boot memory
				 * for this segment, since we don't want
				 * it mapped in permanently as part of
				 * the kernel image.
				 */
				if ((loadaddr = (u_int)
				    malloc(phdr->p_memsz)) == NULL)
					goto shread;
				/*
				 * Save this to pass on
				 * to the interpreter.
				 */
				phdr->p_vaddr = loadaddr;
			}
			if (read(io, loadaddr, phdr->p_filesz) !=
			    phdr->p_filesz)
				goto shread;

			/* zero out BSS */
			if (phdr->p_memsz > phdr->p_filesz) {
				loadaddr += phdr->p_filesz;
				bzero(loadaddr, phdr->p_memsz - phdr->p_filesz);
				if (print)
					printf("%d Bytes\n",
					    phdr->p_memsz - phdr->p_filesz);
			}
		} else if (phdr->p_type == PT_INTERP) {
			/*
			 * Dynamically-linked executable.
			 */
			interp = 1;
			if (lseek(io, phdr->p_offset, 0) == -1) {
				goto elferr;
			}
			/*
			 * Get the name of the interpreter.
			 */
			if (read(io, dlname, phdr->p_filesz) != phdr->p_filesz)
				goto elferr;
			dlname[sizeof (dlname)-1] = (char)0;

		} else if (phdr->p_type == PT_DYNAMIC) {
			dynamic = phdr->p_vaddr;
		}
	}
	/*
	 * Load the interpreter,
	 * if there is one.
	 */
	if (interp) {
		Elf32_Boot bootv[EB_MAX];		/* Bootstrap vector */
		auxv_t auxv[__BOOT_NAUXV_IMPL];	/* Aux vector */
		Elf32_Boot *bv = bootv;
		auxv_t *av = auxv;
		struct bkpt *bkptr;
		extern struct bkpt	*get_bkpt();
		extern void		 setup_aux(void);

		/*
		 * Load it.
		 */
		entrypt = (u_int)iload(dlname, tphdr, dphdr, &av);

		/*
		 * Build bootstrap and aux vectors.
		 */
		setup_aux();
		EBV(bv, EB_AUXV, 0); /* fill in later */
		EBV(bv, EB_PAGESIZE, pagesize);
		EBV(bv, EB_DYNAMIC, dynamic);
		EBV(bv, EB_NULL, 0);

		AUX(av, AT_BASE, entrypt);
		AUX(av, AT_ENTRY, elfhdr.e_entry);
		AUX(av, AT_PAGESZ, pagesize);
		AUX(av, AT_PHDR, allphdrs);
		AUX(av, AT_PHNUM, elfhdr.e_phnum);
		AUX(av, AT_PHENT, elfhdr.e_phentsize);
		if (npagesize)
			AUX(av, AT_SUN_LPAGESZ, npagesize);
		AUX(av, AT_SUN_IFLUSH, icache_flush);
		if (cpulist != NULL)
			AUX(av, AT_SUN_CPU, cpulist);
		AUX(av, AT_NULL, 0);
		/*
		 * Realloc vectors and copy them.
		 */
		size = (caddr_t)bv - (caddr_t)bootv;
		if ((elfbootvec = (Elf32_Boot *)malloc(size)) == NULL)
			return ((int (*)()) -1);
		bcopy(bootv, elfbootvec, size);

		size = (caddr_t)av - (caddr_t)auxv;
		if (size > sizeof (auxv)) {
			printf("readelf: overrun of available aux vectors\n");
			goto elferr;
		}
		if ((elfbootvec->eb_un.eb_ptr =
		    (Elf32_Addr)malloc(size)) == NULL)
			return ((int (*)()) -1);
		bcopy(auxv, elfbootvec->eb_un.eb_ptr, size);

		/*
		 * If RB_DEBUG (kadb -d) is set then set
		 * a break point at the entry-point for the
		 * primary object (unix).  This way we can
		 * let krtld run and relocate itself and unix
		 * and then have kadb stop at the first entry
		 * point of unix itself.  This isn't really where
		 * life begins, but it's all that most people
		 * are interested in.
		 */
		if (interactive & RB_DEBUG) {
			bkptr = get_bkpt(elfhdr.e_entry, BPINST, SZBPT);
			bkptr->flag = BKPT_TEMP;
			bkptr->count = bkptr->initcnt = 1;
			bkptr->comm[0] = '\n';
			bkptr->comm[1] = '\0';
		}
	} else {
		/*
		 * If there is no interpreter (krtld) then
		 * RD_DEBUG should mimic RB_KRTLD which will
		 * cause kadb to halt before any instructions
		 * have been executed.
		 */
		if (interactive & RB_DEBUG) {
			interactive |= RB_KRTLD;
			interactive &= ~RB_DEBUG;
		}

		free(allphdrs, phdrsize);
	}

	pagesused = btopr(prog_size);
	debuginit(io, &elfhdr, allphdrs, name);
	return ((func_t)entrypt);
elferr:
	printf("; cannot load program.\n");
	return ((func_t)-1);

shread:
	printf("Truncated file\n");
	return ((func_t)-1);
}

/*
 * Load the interpreter.  It expects a
 * relocatable .o capable of bootstrapping
 * itself.
 */
func_t
iload(char *rtld, Elf32_Phdr *tphdr, Elf32_Phdr *dphdr,
	auxv_t **avp)
{
	Elf32_Ehdr *ehdr;
	unsigned int i;
	int fd;
	int size;
	caddr_t dl_entry = (caddr_t)0;
	caddr_t shdrs;
	caddr_t etext, edata, orig_etext, orig_edata;
	Elf32_Shdr *symhdr = NULL;

	etext = orig_etext = (caddr_t)tphdr->p_vaddr + tphdr->p_memsz;
	edata = orig_edata = (caddr_t)dphdr->p_vaddr + dphdr->p_memsz;

	/*
	 * Get the module path.
	 */
	module_path = getmodpath(aline);

	if ((fd = openpath(module_path, rtld, O_RDONLY)) < 0) {
		printf("Error opening %s\n", rtld);
		return ((func_t)-1);
	}
	AUX(*avp, AT_SUN_LDNAME, rtld);
	/*
	 * Allocate and read the ELF header.
	 */
	if ((ehdr = (Elf32_Ehdr *)malloc(sizeof (Elf32_Ehdr))) == NULL)
		return ((func_t)-1);

	if (read(fd, ehdr, sizeof (*ehdr)) != sizeof (*ehdr)) {
		printf("Error reading ELF header (%s).\n", rtld);
		return ((func_t)-1);
	}

	size = ehdr->e_shentsize * ehdr->e_shnum;
	if ((shdrs = (caddr_t)malloc(size)) == NULL)
		return ((func_t)-1);
	/*
	 * Read the section headers.
	 */
	if (lseek(fd, ehdr->e_shoff, 0) == -1 ||
	    read(fd, shdrs, size) != size) {
		printf("Error reading section headers\n");
		return ((func_t)-1);
	}
	AUX(*avp, AT_SUN_LDELF, ehdr);
	AUX(*avp, AT_SUN_LDSHDR, shdrs);

	/*
	 * Load sections into the appropriate dynamic segment.
	 */
	for (i = 1; i < ehdr->e_shnum; i++) {
		Elf32_Shdr *sp;
		caddr_t *spp;
		caddr_t load;

		sp = (Elf32_Shdr *)(shdrs + (i*ehdr->e_shentsize));
		/*
		 * If it's not allocated and not required
		 * to do relocation, skip it.
		 */
		if (!(sp->sh_flags & SHF_ALLOC) &&
		    sp->sh_type != SHT_SYMTAB &&
		    sp->sh_type != SHT_STRTAB &&
		    sp->sh_type != SHT_RELA)
			continue;
		/*
		 * If the section is read-only,
		 * it goes in as text.
		 */
		spp = (sp->sh_flags & SHF_WRITE)? &edata: &etext;
		/*
		 * Make some room for it.
		 */
		load = segbrk(spp, sp->sh_size, sp->sh_addralign);
		if (load == (caddr_t)0) {
			printf("Allocating space for sections failed\n");
			return ((func_t)-1);
		}
		if (dl_entry == (caddr_t)0 &&
		    !(sp->sh_flags & SHF_WRITE) &&
		    (sp->sh_flags & SHF_EXECINSTR)) {
			dl_entry = load + ehdr->e_entry;
		}
		/*
		 * If it's bss, just zero it out.
		 */
		if (sp->sh_type == SHT_NOBITS) {
			bzero(load, sp->sh_size);
		} else {
			/*
			 * Read the section contents.
			 */
			if (lseek(fd, sp->sh_offset, 0) == -1 ||
			    read(fd, load, sp->sh_size) != sp->sh_size) {
				printf("Error reading sections\n");
				return ((func_t)-1);
			}
		}
		/*
		 * Assign the section's virtual addr.
		 */
		sp->sh_addr = (Elf32_Off)load;
		if (sp->sh_type == SHT_SYMTAB)
			symhdr = sp;
	}

	if (interactive & RB_KRTLD)
		krtld_debug_setup(ehdr, shdrs, symhdr, orig_etext, orig_edata,
		    etext - orig_etext, edata - orig_edata);

	/*
	 * Update sizes of segments.
	 */
	tphdr->p_memsz = (Elf32_Word)etext - tphdr->p_vaddr;
	dphdr->p_memsz = (Elf32_Word)edata - dphdr->p_vaddr;
	close(fd);
	return ((func_t)dl_entry);
error:
	close(fd);
	printf("Error loading interpreter (%s)\n", rtld);
	return ((func_t)-1);
}

/*
 * Extend the segment's "break" value by bytes.
 */
static caddr_t
segbrk(caddr_t *spp, unsigned int bytes, int align)
{
	caddr_t va, pva;
	int size = 0;
	unsigned int alloc_pagesize = pagesize;
	unsigned int alloc_align = BO_NO_ALIGN;

	if (npagesize) {
		alloc_align = npagesize;
		alloc_pagesize = npagesize;
	}

	va = (caddr_t)ALIGN(*spp, align);
	pva = (caddr_t)roundup((u_int)*spp, alloc_pagesize);
	/*
	 * Need more pages?
	 */
	if (va + bytes > pva) {
		size = roundup((bytes - (pva - va)), alloc_pagesize);

		if (BOP_ALLOC(bootops, pva, size, alloc_align) != pva) {
			printf("segbrk failed, obrk = 0x%x, bytes = 0x%x, "
				"align = 0x%x\n", *spp, bytes, alloc_align);
			return ((caddr_t)0);
		}
	}
	*spp = va + bytes;

	return (va);
}

/*
 * Open the file using a search path and
 * return the file descriptor (or -1 on failure).
 */
static int
openpath(path, fname, flags)
char *path;
char *fname;
int flags;
{
	register char *p, *q;
	char buf[MAXPATHLEN];
	int fd;

	/*
	 * If the file name is absolute,
	 * don't use the module search path.
	 */
	if (fname[0] == '/')
		return (open(fname, flags));

	for (p = path; 1; p = q) {

		while (*p == ' ' || *p == '\t' || *p == ':')
			p++;
		if (*p == '\0')
			break;
		q = p;
		while (*q && *q != ' ' && *q != '\t' && *q != ':')
			q++;
		strncpy(buf, p, q - p);
		if (q[-1] != '/')
			buf[q - p] = '/';
		strcpy(&buf[q - p + 1], fname);

		if ((fd = open(buf, flags)) > 0)
			return (fd);
	}
	return (-1);
}

/*
 * Get the module search path.
 */
static char *
getmodpath(fname)
char *fname;
{
	register char *p = strrchr(fname, '/');
	register char *path;
	int len;
	char iarch[MAXNAMELEN];
	extern void mod_path_uname_m(char *, char *);

	if (p == fname)
		p++;

	len = p - fname;
	path = (char *)malloc(MOD_MAXPATH);
	strncpy(path, fname, len);
	path[len] = 0;

	(void) BOP_GETPROP(bootops->bsys_super, "impl-arch-name", iarch);
	mod_path_uname_m(path, iarch);
	strcat(path, " ");
	strcat(path, MOD_DEFPATH);

	if (howto & RB_ASKNAME) {
		char buf[MOD_MAXPATH];

		printf("Enter default directory for modules [%s]: ", path);
		getsn(buf, sizeof (buf));
		if (buf[0] != '\0') {
			(void) free(path, MOD_MAXPATH);
			path = (char *)malloc(strlen(buf)+1);
			strcpy(path, buf);
		}
	}
	return (path);
}

struct bootf {
	char	let;
	u_int	bit;
} bootf[] = {
	'a',	RB_ASKNAME,
	's',	RB_SINGLE,
	'i',	0,
	'h',	RB_HALT,
	'b',	RB_NOBOOTRC,
	'd',	RB_DEBUG,
	'k',	RB_KRTLD,
	'w',	RB_WRITABLE,
	'c',	RB_CONFIG,
	'r',	RB_RECONFIG,
	'v',	RB_VERBOSE,
	'f',	RB_FLUSHCACHE,
	'x',	RB_NOBOOTCLUSTER,
	0,	0
};

/*
 * Parse the boot line to determine boot flags
 */
bootflags(cp)
	register char *cp;
{
	register int i, boothowto = 0;

	while (cp && *cp && *cp != '-')
		cp++;

	if (cp && *cp++ == '-') {
		do {
			for (i = 0; bootf[i].let; i++) {
				if (*cp == bootf[i].let) {
					boothowto |= bootf[i].bit;
					break;
				}
			}
			cp++;
		} while (bootf[i].let && *cp);
	}
	return (boothowto);
}

/*
 * Parse the boot line and put it in boot property strings for the kernel
 * we're trying to debug. Stuff in -a -s or -as if s/he only typed one
 * argument and if they were in effect before.
 */
static void
parseparam(char *line, int defaults, char *bootname, char *bootargs)
{
	register int	nargs, i;

	while (*line && *line != ' ')
		*bootname++ = *bootargs++ = *line++;
	*bootname = '\0';	/* terminate the kernels "whoami" string */

	if (*line == ' ')
		nargs = 2;
	else
		nargs = 1;

	*line++ = '\0';		/* terminate line for open */
	*bootargs++ = ' ';	/* to separate the args */

	if (nargs == 2) {
		/*
		 * Copy all the switches, and append an extra 'd' too
		 */
		if (*line != '-')
			*bootargs++ = '-';
		while (*line && *line != ' ')
			*bootargs++ = *line++;
		*bootargs++ = 'd';
		*bootargs = '\0';
	} else {
		/*
		 * Stuff in default switches if user didn't respecify
		 */
		defaults |= RB_DEBUG;		/* or in debug flag */
		*bootargs++ = '-';
		for (i = 0; bootf[i].let; i++) {
			if (defaults & bootf[i].bit)
				*bootargs++ = bootf[i].let;
		}
		*bootargs = '\0';
	}
}
