/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)_setup.c	1.51	99/02/28 SMI"

/*
 * i386 specific setup routine  -  relocate ld.so's symbols, setup its
 * environment, map in loadable sections of the executable.
 *
 * Takes base address ld.so was loaded at, address of ld.so's dynamic
 * structure, address of process environment pointers, address of auxiliary
 * vector and * argv[0] (process name).
 * If errors occur, send process signal - otherwise
 * return executable's entry point to the bootstrap routine.
 */
#include	"_synonyms.h"

#include	<signal.h>
#include	<stdlib.h>
#include	<sys/auxv.h>
#include	<sys/types.h>
#include	<sys/sysconfig.h>
#include	<sys/stat.h>
#include	<link.h>
#include	<string.h>
#include	<dlfcn.h>
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"
#include	"msg.h"
#ifdef PRF_RTLD
#include	"profile.h"
#endif
#include	"debug.h"

extern int	_end;
extern int	_etext;

/*
 * Define for the executable's interpreter.
 * Usually it is ld.so.1, but for the first release of ICL binaries
 * it is libc.so.1.  We keep this information so that we don't end
 * up mapping libc twice if it is the interpreter.
 */
static Interp _interp;

/* VARARGS */
unsigned long
_setup(Boot * ebp, Dyn * ld_dyn)
{
	unsigned long	reladdr, etext, ld_base = 0;
	unsigned long	strtab, soname, entry, interp_base = 0;
	unsigned long	relcount;
	char *		c, * _rt_name, ** _envp, ** _argv, * _pr_name = 0;
	int		_syspagsz, phsize, phnum, i;
	int		_flags = 0, _name = 0, aoutflag = 0;
	Dyn *		dyn_ptr;
	Phdr *		phdr;
	Ehdr *		ehdr;
	int		fd = -1;
	int		dz_fd = FD_UNAVAIL;
	Fct *		ftp;
	Rt_map *	lmp;
	auxv_t *	auxv, * _auxv;
	size_t		eaddr, esize;
	uid_t		uid = -1, euid = -1;
	gid_t		gid = -1, egid = -1;
	char *		_platform = 0;
	char *		_execname = 0;

	/*
	 * Scan the bootstrap structure to pick up the basics.
	 */
	for (; ebp->eb_tag != EB_NULL; ebp++)
		switch (ebp->eb_tag) {
		case EB_DYNAMIC:
			aoutflag = 1;
			break;
		case EB_LDSO_BASE:
			ld_base = (long)ebp->eb_un.eb_val;
			break;
		case EB_ARGV:
			_argv = (char **)ebp->eb_un.eb_ptr;
			_pr_name = *_argv;
			break;
		case EB_ENVP:
			_envp = (char **)ebp->eb_un.eb_ptr;
			break;
		case EB_AUXV:
			_auxv = (auxv_t *)ebp->eb_un.eb_ptr;
			break;
		case EB_DEVZERO:
			dz_fd = (int)ebp->eb_un.eb_val;
			break;
		case EB_PAGESIZE:
			_syspagsz = (int)ebp->eb_un.eb_val;
			break;
		}

	/*
	 * Search the aux. vector for the information passed by exec.
	 */
	for (auxv = _auxv; auxv->a_type != AT_NULL; auxv++) {
		switch (auxv->a_type) {
		case AT_EXECFD:
			/* this is the old exec that passes a file descriptor */
			fd = auxv->a_un.a_val;
			break;
		case AT_FLAGS:
			/* processor flags (MAU available, etc) */
			_flags = auxv->a_un.a_val;
			break;
		case AT_PAGESZ:
			/* system page size */
			_syspagsz = auxv->a_un.a_val;
			break;
		case AT_PHDR:
			/* address of the segment table */
			phdr = (Phdr *) auxv->a_un.a_ptr;
			break;
		case AT_PHENT:
			/* size of each segment header */
			phsize = auxv->a_un.a_val;
			break;
		case AT_PHNUM:
			/* number of program headers */
			phnum = auxv->a_un.a_val;
			break;
		case AT_BASE:
			/* interpreter base address */
			if (ld_base == 0)
				ld_base = auxv->a_un.a_val;
			interp_base = auxv->a_un.a_val;
			break;
		case AT_ENTRY:
			/* entry point for the executable */
			entry = auxv->a_un.a_val;
			break;
		case AT_SUN_UID:
			/* effective user id for the executable */
			euid = auxv->a_un.a_val;
			break;
		case AT_SUN_RUID:
			/* real user id for the executable */
			uid = auxv->a_un.a_val;
			break;
		case AT_SUN_GID:
			/* effective group id for the executable */
			egid = auxv->a_un.a_val;
			break;
		case AT_SUN_RGID:
			/* real group id for the executable */
			gid = auxv->a_un.a_val;
			break;
#ifdef	AT_SUN_PLATFORM			/* Defined on SunOS 5.5 & greater. */
		case AT_SUN_PLATFORM:
			/* platform name */
			_platform = auxv->a_un.a_ptr;
			break;
#endif
#ifdef	AT_SUN_EXECNAME			/* Defined on SunOS 5.6 & greater. */
		case AT_SUN_EXECNAME:
			/* full pathname of execed object */
			_execname = auxv->a_un.a_ptr;
			break;
#endif
		}
	}

	/*
	 * Get needed info from ld.so's dynamic structure.
	 */
	/* LINTED */
	dyn_ptr = (Dyn *)((char *)ld_dyn + ld_base);
	for (ld_dyn = dyn_ptr; ld_dyn->d_tag != DT_NULL; ld_dyn++) {
		switch (ld_dyn->d_tag) {
		case DT_REL:
			reladdr = ld_dyn->d_un.d_ptr + ld_base;
			break;
		case DT_RELCOUNT:
			relcount = ld_dyn->d_un.d_val;
			break;
		case DT_STRTAB:
			strtab = ld_dyn->d_un.d_ptr + ld_base;
			break;
		case DT_SONAME:
			soname = ld_dyn->d_un.d_val;
			break;
		}
	}
	_rt_name = (char *)strtab + soname;

	/*
	 * Relocate all symbols in ld.so.
	 *
	 * Because ld.so.1 is built with -Bsymbolic there should
	 * only be 'RELATIVE' relocations and 'JMPSLOT'
	 * relocations, both of which get relative additions against
	 * them.
	 */
	for (; relcount; relcount--) {
		uintptr_t roffset;

		roffset = ((Rel *)reladdr)->r_offset + ld_base;
		*((ulong_t *) roffset) += ld_base;
		reladdr += sizeof (Rel);
	}

	/*
	 * Now that ld.so has relocated itself, initialize any global variables.
	 */
	_environ = _envp;

	flags = _flags;
	if (dz_fd != FD_UNAVAIL)
		dz_init(dz_fd);
	platform = _platform;

	/*
	 * If pagesize is unspecified find its value.
	 */
	if ((syspagsz = _syspagsz) == 0)
		syspagsz = _sysconfig(_CONFIG_PAGESIZE);
	fmap->fm_msize = syspagsz;

	/*
	 * Add the unused portion of the last data page to the free space list.
	 * The page size must be set before doing this.  Here, _end refers to
	 * the end of the runtime linkers bss.  Note that we do not use the
	 * unused data pages from any included .so's to supplement this free
	 * space as badly behaved .os's may corrupt this data space, and in so
	 * doing ruin our data.
	 */
	eaddr = S_DROUND((size_t)&_end);
	esize = eaddr % syspagsz;
	if (esize) {
		esize = syspagsz - esize;
		addfree((void *)eaddr, esize);
	}

	/*
	 * Establish the applications name.
	 */
	if (_pr_name) {
		/*
		 * Some troublesome programs will change the value of argv[0].
		 * Dupping this string protects ourselves from such programs.
		 */
		if ((pr_name = (const char *)strdup(_pr_name)) == 0)
			exit(1);
	} else
		pr_name = (const char *)MSG_INTL(MSG_STR_UNKNOWN);

	/*
	 * Determine whether we have a secure executable.
	 */
	security(uid, euid, gid, egid);

	/*
	 * We copy rtld's name here rather than just setting a pointer to it so
	 * that it will appear in the data segment and thus in any core file.
	 */
	if ((c = malloc(strlen(_rt_name) + 1)) == 0)
		exit(1);
	(void) strcpy(c, _rt_name);

	/*
	 * Get the filename of the rtld for use in any diagnostics (but
	 * save the full name in the link map for future comparisons)
	 */
	rt_name = _rt_name = c;
	while (*c) {
		if (*c++ == '/')
			rt_name = c;
	}

	/*
	 * Create a link map structure for ld.so.  We assign the NAME() after
	 * link-map creation to avoid fullpath() processing within elf_new_lm(),
	 * this is carried out later (setup()) when the true interpretor path
	 * (as defined within the application) is known.
	 */
	ftp = &elf_fct;
	if ((lmp = ftp->fct_new_lm(&lml_rtld, 0, 0,
	    dyn_ptr, ld_base, (unsigned long)&_etext,
	    (unsigned long)(eaddr - ld_base), 0, 0, 0, 0, ld_base,
	    (unsigned long)(eaddr - ld_base))) == 0) {
		exit(1);
	}
	NAME(lmp) = (char *)_rt_name;
	COUNT(lmp) = 1;
	MODE(lmp) |= (RTLD_LAZY | RTLD_NODELETE | RTLD_GLOBAL | RTLD_WORLD);
	FLAGS(lmp) |= (FLG_RT_ANALYZED | FLG_RT_RELOCED | FLG_RT_INITDONE |
			FLG_RT_INITCLCT | FLG_RT_FINICLCT);

	/*
	 * There is no need to analyze ld.so because we don't map in any of
	 * its dependencies.  However we may map these dependencies in later
	 * (as if ld.so had dlopened them), so initialize the plt and the
	 * permission information.
	 */
	if (PLTGOT(lmp))
		elf_plt_init((unsigned int *)(PLTGOT(lmp)), (caddr_t)lmp);

	/*
	 * Look for environment strings (allows debugging to get switched on).
	 */
	if ((_flags = readenv((const char **)_envp, 0)) == -1)
		exit(1);

#ifdef	PRF_RTLD
	/*
	 * Have we been requested to profile ourselves.
	 */
	if (profile_name)
		profile_rtld = profile_setup((Link_map *)lmp);
	PRF_MCOUNT(5, _setup);
#endif

	_name = 0;
	/*
	 * Map in the file, if exec has not already done so.  If it has,
	 * simply create a new link map structure for the executable.
	 */
	if (fd != -1) {
		struct stat	status;

		/*
		 * Find out what type of object we have.
		 */
		(void) fstat(fd, &status);
		fmap->fm_fd = fd;
		fmap->fm_fsize = status.st_size;
		if ((ftp = are_u_this(pr_name)) == 0)
			exit(1);

		/*
		 * Map in object.
		 */
		if ((lmp = (ftp->fct_map_so)(&lml_main, 0, pr_name)) == 0)
			exit(1);

	} else {
		/*
		 * Set up function ptr and arguments according to the type
		 * of file class the executable is. (Currently only supported
		 * type is ELF format.)  Then create a link map for the
		 * executable.
		 */
		if (aoutflag) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_UNKNFILE), pr_name);
			exit(1);
		} else {
			Phdr *		pptr;
			Phdr *		firstptr = 0;
			Phdr *		lastptr;
			Dyn *		dyn = 0;
			Off		i_offset;
			Addr		base = 0;
			char *		name = 0;
			unsigned long	memsize;

			/*
			 * Using the executables phdr address determine the base
			 * address of the input file.  Determine from the elf
			 * header if we're been called from a shared object or
			 * dynamic executable.  If the latter then any
			 * addresses within the object are to be used as is.
			 * The addresses within shared objects must be added to
			 * the process's base address.
			 */
			ehdr = (Ehdr *)((int)phdr - (int)phdr->p_offset);
			if (ehdr->e_type == ET_DYN) {
				base = (Addr)ehdr;
				name = (char *)pr_name;
				_name = 1;
			}

			/*
			 * Extract the needed information from the segment
			 * headers.
			 */
			for (i = 0, pptr = phdr; i < phnum; i++) {
				if (pptr->p_type == PT_INTERP) {
					interp = &_interp;
					i_offset = pptr->p_offset;
					interp->i_faddr =
					    (caddr_t)interp_base;
				}
				if ((pptr->p_type == PT_LOAD) &&
				    pptr->p_filesz) {
					if (!firstptr)
						firstptr = pptr;
					lastptr = pptr;
					if ((pptr->p_offset <= i_offset) &&
					    (i_offset <= (pptr->p_memsz +
					    pptr->p_offset))) {
						interp->i_name = (char *)
						    pptr->p_vaddr + i_offset -
						    pptr->p_offset + base;
					}
					if (!(pptr->p_flags & PF_W))
						etext = pptr->p_vaddr +
							pptr->p_memsz + base;
				} else if (pptr->p_type == PT_DYNAMIC)
					dyn = (Dyn *)(pptr->p_vaddr + base);
				pptr = (Phdr *)((unsigned long)pptr + phsize);
			}
			ftp = &elf_fct;
			memsize = (lastptr->p_vaddr + lastptr->p_memsz) -
				S_ALIGN(firstptr->p_vaddr, syspagsz);
			if (!(lmp = (ftp->fct_new_lm)(&lml_main, name, 0,
			    dyn, (Addr)ehdr, etext, memsize, entry,
			    phdr, phnum, phsize, (unsigned long)ehdr,
			    memsize))) {
				exit(1);
			}
		}
	}

	/*
	 * Having mapped the executable in and created its link map, initialize
	 * the name and flags entries as necessary.  Note that any object that
	 * starts the process is identifed as `main', even shared objects.
	 * This assumes that the starting object will call .init and .fini from
	 * its own crt use (this is a pretty valid assumption as the crts also
	 * provide the necessary entry point).
	 */
	if (_name == 0)
		NAME(lmp) = (char *)pr_name;
	PATHNAME(lmp) = _execname;
	LIST(lmp)->lm_flags |= _flags;

	/*
	 * Initialize the dyn_plt_ent_size field.  It currently contains the
	 * size of the dyn_plt_template.  It still needs to be aligned and have
	 * space for the 'dyn_data' area added.
	 */
	dyn_plt_ent_size += ROUND(dyn_plt_ent_size, 4) + sizeof (uintptr_t) +
	    sizeof (uintptr_t) + sizeof (ulong_t) + sizeof (ulong_t) +
	    sizeof (Sym);

	/*
	 * Continue with generic startup processing.
	 */
	if (setup(lmp, (unsigned long)_envp, (unsigned long)_auxv) == 0) {
		exit(1);
	}

	DBG_CALL(Dbg_util_call_main(pr_name));
	return (LM_ENTRY_PT(lmp)());
}
