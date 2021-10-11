/*
 * Copyright (c) 1986 - 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * adb - symbol table routines
 */

#ident	"@(#)sym64.c	1.9	98/08/21 SMI"

#include <stdio.h>
#include "adb.h"
#include "fio.h"
#include "symtab.h"
#include <link.h>
#include <sys/types.h>
#include <sys/auxv.h>
#include <string.h>
#include <strings.h>
#if	!defined(KADB)
#include <unistd.h>
#endif
#include <setjmp.h>

static void	dosym();
static void	mergedefs();
void	re_enter64();
#ifdef KADB

#define _ELF64
#include <sys/sysmacros.h>
#include <sys/elf.h>
#include <sys/kobj.h>

Elf64_Addr dbg_kobj_getsymvalue64(char *name, int any_mod);
char *dbg_kobj_getsymname64(Elf64_Addr value, u_int *offset);
extern void kobj_init();
extern unsigned int kobj_hash_name();



#endif /* KADB */

#define INFINITE 0x7fffffff
static unsigned long SHLIB_OFFSET;
extern int adb_debug;
static jmp_buf shliberr_jmpbuff;
Elf64_Dyn dynam64;           /* root data structure for dl info */
Elf64_Dyn *dynam_addr64;     /* its addr in target process */
extern struct r_debug *ld_debug_addr; /* the substructure that hold debug info */
extern struct r_debug ld_debug_struct; /* and its addr*/
extern char *rtld_path;    /* path name to which kernel hands control */
extern addr_t exec_entry;         /* addr that receives control after rtld's done*/


#define GSEGSIZE 150	/* allocate global symbol buckets 150 per shot */

extern	addr_t rtld_state_addr;    /*in case of adb support for dlopen/dlclose */
extern	struct  asym *nextglob;	/* next available global symbol slot */
extern	struct  asym *global_segment; /* current segment of global symbol table */
extern int	use_shlib;
extern int	curfile;
extern struct	asym *curfunc, *curcommon;
static struct	asym *enter64();
extern struct	afield *field();
extern struct	asym * lookup_base();
#define	HSIZE	255

static int elf_stripped = 1; /* by default executables are stripped */

extern	struct	asym *symhash[HSIZE];
extern  struct modctl krtld_modctl;
extern  int *mod_mix_changed_kaddr;
extern	int current_module_mix;
extern	int dummy_module_mix;
extern	struct modctl *modules_kaddr;
extern	int kadb_curmod;

#ifndef KADB
extern void *tmp_malloc();
#endif


long
rel_symoffset64(Elf64_Sym *sp)
{
	u_int val = sp->st_value;
	Elf64_Shdr *shp;
	extern Elf64_Shdr *secthdr64;

	db_printf(5, "rel_symoffset: sp=%X, st_shndx=%X", sp, sp->st_shndx);
	/*
	 * bss and COMMON symbols have no corresponding
	 * file offset so we return MAXFILE.
	 */
	if (sp->st_shndx == SHN_COMMON || sp->st_shndx == SHN_ABS ||
	    (sp->st_shndx < filhdr64.e_shnum &&
	    secthdr64[sp->st_shndx].sh_type == SHT_NOBITS)) {
		db_printf(5, "rel_symoffset: return val=%ld", MAXFILE);
		return (MAXFILE);
	}

	if (sp->st_shndx != SHN_UNDEF && sp->st_shndx < filhdr64.e_shnum) {
		shp = (Elf64_Shdr *)((u_long)secthdr64 +
			sp->st_shndx * filhdr64.e_shentsize);
		val += shp->sh_offset;
	}
	db_printf(5, "rel_symoffset: return val=%X", val);
	return (val);
}

static int
symvcmp(const void *p1, const void *p2)
{
	struct asym **s1 = (struct asym **)p1;
	struct asym **s2 = (struct asym **)p2;
	return ((*s1)->s_value > (*s2)->s_value) ? 1 :
		(((*s1)->s_value == (*s2)->s_value) ? 0 : -1);
}


stinit64(int fsym, Elf64_Shdr *sh, int nsect, int flag)
{
	int sym_sect;		/* index of symbol section's header */
	int str_sect;		/* index of string section's header */
	char *strtab;		/* ptr to string table copy */
	Elf64_Sym elfsym[BUFSIZ/sizeof(Elf64_Sym)];
				 /* symbol table entry recepticle */
	Elf64_Sym *es;		/* ptr to ELF symbol */
	int ntogo;		/* number of symbols */
	int ninbuf = 0;		/* number of unconsumed of symbols in buffer */


	/* Look for the symbol section header. */
	for (sym_sect = 0; sym_sect < nsect; sym_sect++)
		if (sh[sym_sect].sh_type == SHT_SYMTAB) {
			elf_stripped = 0;
			break;
		}
	/*
	 * If executable has been stripped then the symbol table is gone
	 * and in that case, use the symbol table that is reserved for
	 * the runtime loader
	 */
	if (sym_sect == nsect) {
		for (sym_sect = 0; sym_sect < nsect; sym_sect++)
			if (sh[sym_sect].sh_type == SHT_DYNSYM)
				break;
		if (sym_sect == nsect)
			return(-1);	/* No symbol section there */
	}

	/* Check the associated string table. */
	str_sect = sh[sym_sect].sh_link;
	if (str_sect == nsect || sh[str_sect].sh_size == 0) {
#ifdef	KADB
		printf("Warning - empty string table; no symbols.\n");
#else	/* KADB */
		fprintf(stderr, "Warning - empty string table; no symbols.\n");
		fflush(stderr);
#endif	/* KADB */
		return(-1);
	}

	/* Get space for a copy of the string table. */
	strtab = (char *) malloc(sh[str_sect].sh_size +
		sizeof(sh[str_sect].sh_size) + 8);
	if (strtab == 0)
		outofmem();
	*(long *) strtab = sh[str_sect].sh_size + sizeof(sh[str_sect].sh_size);

	/* Read the string table. */
	(void) lseek(fsym, (long) sh[str_sect].sh_offset, 0);
	if (read(fsym, strtab + sizeof(sh[str_sect].sh_size),
		sh[str_sect].sh_size) != sh[str_sect].sh_size)
		goto readerr;

	/* Read and process all symbols */
	(void) lseek(fsym, (long)sh[sym_sect].sh_offset, 0);
	ntogo = sh[sym_sect].sh_size / sh[sym_sect].sh_entsize;

	if (ntogo < 1) {
		db_printf(4, "stinit: no symbols?");
		return -1;
	}
	while (ntogo) {
		if (ninbuf == 0) {	/* more in buffer? */
			long nread = ntogo, cc;
			if (nread > BUFSIZ / sizeof (Elf64_Sym))
				nread = BUFSIZ / sizeof (Elf64_Sym);
			cc = read(fsym, (char *) elfsym,
				nread * sizeof (Elf64_Sym));
			if (cc != nread * sizeof (Elf64_Sym))
				goto readerr;
			ninbuf = nread;
			es = elfsym;
		}
		dosym(es++, flag, strtab + sizeof(sh[str_sect].sh_size));
		ninbuf--;
		ntogo--;
	}
	sort_globals(AOUT);

	return(0);
readerr:
	if (!elf_stripped)
		printf("error reading symbol or string table\n");
#if	defined(KADB)
	exit(1, 1);
#else
	exit(1);
#endif
}


static void 
dosym(Elf64_Sym *es, int flag, char *strtab)
{
	char *cp;		/* scratch ptr */
	struct asym *s;		/* debugger symbol ptr */
	char *name = (es->st_name) ? (strtab + es->st_name) : NULL;

	db_printf(5, "dosym: es=%X, es->st_name='%s', es->st_shndx=%D, flag=%D",
		es, (name == NULL) ? "NULL" : name, es->st_shndx, flag);
	/* ignore undefine symbols */
	if(es->st_shndx == SHN_UNDEF)
	    return;
	/*
	 * Discard symbols containing a ":" -- they
	 * are dbx symbols, and will confuse us.
	 * Same with the "-lg" symbol and nameless symbols
	 */
	if (name == NULL) {
		db_printf(7, "dosym: discarded, nameless symbol\n");
		return;				/* it's nameless */
	}
	if (strcmp("-lg", name) == 0) {
		db_printf(7, "dosym: discarded -lg");
		return;
	}
	/*
	 * XXX - Disregard symbols starting with a ".".
	 */
	cp = name;
	if (*cp == '.')
		return;
	for (; *cp; cp++) {
		if (*cp == ':') {
			db_printf(7, "dosym: discarded %s, it has ':'\n", name);
			return;		/* must be dbx stab */
		}
	}

	db_printf(7, "dosym: ELF64_ST_TYPE(es->st_info)=%D,\n\t"
		     "ELF64_ST_BIND(es->st_info)=%D",
		  ELF64_ST_TYPE(es->st_info), ELF64_ST_BIND(es->st_info));

	switch (ELF64_ST_TYPE(es->st_info)) {
	case STT_FUNC:
		/*
		 * disgard symbols containing a .
		 * this may be a short-term kludge, to keep
		 * us from printing "routine" names like
		 * "scan.o" in stack traces, rather that
		 * real routine names like "_scanner".
		 */
		 for (cp = name; *cp; cp++)
		     if (*cp == '.') return;
		/* fall thru */
	case STT_OBJECT:
	case STT_NOTYPE:
		s = lookup_base(name);
		if (s) {
#if defined(i386) && !defined(KADB)
			if (ELF64_ST_BIND (es->st_info) == STB_LOCAL) {
				db_printf(4,
					 "dosym: threw away LOCAL symbol %s",
					  s->s_name);
				return;
			}
			if (ELF64_ST_BIND (es->st_info) == STB_WEAK &&
			    (s->s_bind == STB_GLOBAL || s->s_bind == STB_WEAK)){
				db_printf(4,
					 "dosym: threw away new WEAK symbol %s",
					  s->s_name);
				return;
			}
			if (ELF64_ST_BIND (es->st_info) == STB_GLOBAL &&
			    s->s_bind == STB_GLOBAL) {
				db_printf(4,
					  "dosym: threw away duplicate GLOBAL symbol %s, kept old one",
				       	  name);
				/*
				 * These most definitely come from
				 * ld.so.1 and libc.so.1
				 */
				return;
			}
			if (ELF64_ST_BIND (es->st_info) == STB_GLOBAL &&
			    s->s_bind == STB_WEAK) {
				/*
				 * replace the old with the current one
				 */
				db_printf(4, "dosym: replacing WEAK symbol %s with its GLOBAL", s->s_name);
				s->s_f = NULL;
				s->s_fcnt = s->s_falloc = s->s_fileloc = 0;
				s->s_type = ELF64_ST_TYPE(es->st_info);
				s->s_bind = ELF64_ST_BIND(es->st_info);
				s->s_flag = flag;
				if (flag == AOUT)
					s->s_value = es->st_value;
				else if (flag == REL)
					s->s_value = rel_symoffset64(es);
				else {
					/*
					 * XXX - FIXME - XXX
					 * if (s->s_type != (N_EXT | N_UNDF))
					 * flag == SHLIB and not COMMON
					 */
					s->s_value =
						es->st_value + SHLIB_OFFSET;
				}
				db_printf(5, "dosym: use_shlib=%D, name='%s'",
                                          use_shlib, s->s_name);
				if(use_shlib &&
				   strcmp(s->s_name, "_DYNAMIC") == 0) {
					dynam_addr64 = (Elf64_Dyn *) (s->s_value);
					db_printf(2,
						  "dosym: new _DYNAMIC at %X",
						  dynam_addr64);
				}
				if(use_shlib && strcmp((char*)s->s_name,
					               "_r_debug_state") == 0) {
					rtld_state_addr = (addr_t) (s->s_value);
					db_printf(2, "dosym: new _r_debug_state at %X",
                                                  rtld_state_addr);
				}
			}
#endif	/* defined(i386) && !defined(KADB) */
			if (flag == AOUT)
				mergedefs(s, es, strtab);
		 	else
				re_enter64(s, es, flag);
		} else
			s = enter64(es, flag, strtab);

		db_printf(7, "dosym: ELF64_ST_TYPE(es->st_info) %s STT_FUNC",
		       (ELF64_ST_TYPE(es->st_info) == STT_FUNC) ? "==" : "!=");

		if (ELF64_ST_TYPE(es->st_info) == STT_FUNC) {
			if (curfunc && curfunc->s_f) {
				curfunc->s_f = (struct afield *)
				    realloc((char *)curfunc->s_f,
					curfunc->s_fcnt *
					  sizeof (struct afield));
				if (curfunc->s_f == 0)
					outofmem();
				curfunc->s_falloc = curfunc->s_fcnt;
			}
			curfunc = s;
			db_printf(7, "dosym: curfunc=%X", curfunc);
		}
		break;
	case STT_FILE:
		curfile = fenter(name);
		db_printf(7, "dosym: curfile=%D", curfile);
		break;
	default:
		break;
	}
}

static void
mergedefs(struct asym *s, Elf64_Sym *es, char *strtab)
{
	char *name = (es->st_name) ? (strtab + es->st_name) : NULL;

	db_printf(6, "mergedefs: ELF64_ST_BIND(es->st_info) %s STB_GLOBAL\n",
		     (ELF64_ST_BIND(es->st_info) == STB_GLOBAL) ? "==" : "!=");
	if (ELF64_ST_BIND(es->st_info) == STB_GLOBAL) {
		s->s_name = name;
		s->s_value = es->st_value;
		db_printf(7, "mergedefs: s->s_name=%s, s->s_value=%X\n",
			s->s_name ? s->s_name : "NULL", s->s_value);
	}
	else {

		s->s_type = ELF64_ST_TYPE(es->st_info);
		s->s_bind = ELF64_ST_BIND(es->st_info);
	}

	db_printf(7, "mergedefs: s->s_type=%D, s->s_bind=%D\n",
		  s->s_type, s->s_bind);
}

static int
hashval(char *cp)
{
        int h = 0;
 
#if defined(KADB)
        while (*cp == '_')
                cp++;
        while (*cp && (*cp != '_' || cp[1])) {
#else /* defined(KADB) */
        while (*cp) {
#endif /* defined(KADB) */
                h *= 2;
                h += *cp++;
        }
        h %= HSIZE;
        if (h < 0)
                h += HSIZE;
        return (h);
}



static char *
get_all(int *addr, int leng)
{
	int *copy, *save;
	char *saveflg = errflg;

	db_printf(5, "get_all: addr=%X, leng=%D\n", addr, leng);
	errflg =0;
	/* allocate 4 more, let get to clobber it */
	save = copy = (int*)malloc(leng+4);
	for(; leng > 0; addr++, copy++){
			*copy = get(addr,DSP);
			leng = leng - 4;
			db_printf(2, "get_all: *copy=%D, leng=%D\n", *copy, leng);
	}
	if(errflg) {
		printf("error while reading shared library: %s\n",errflg);
		db_printf(3, "get_all: '%s'\n", errflg);
		errflg = saveflg;
		db_printf(2, "get_all: jumping to shliberr_jmpbuff\n");
		longjmp(shliberr_jmpbuff,1);
	}
	errflg = saveflg;
	db_printf(5, "get_all: returns %X\n", (char *) save);
	return (char*)save;
}

static char *
get_string(int *addr)
{
	int *copy;
	char *c, *save = 0;
	int done = 0;
	char *saveflg = errflg;

	if (addr == NULL){
		printf("nil address found while reading shared library\n");
		return 0;
	}
	db_printf(2, "get_string: reading shared library at addr=%X\n", addr);
	/* assume the max path of shlib is 1024 */
	copy = (int*)malloc(1024);
	if (copy == 0)
		outofmem();
	save = (char*)copy;
	errflg =0;
	while (!done){
		*copy = get(addr, DSP);
		c = (char*)copy;
		if(c[0] == '\0' || c[1] == '\0' || c[2] == '\0' || c[3] == '\0')
			done++;
		copy++;
		addr++;
	}
	if(errflg) {
		printf("error while reading shared library:%s\n", errflg);
		db_printf(3, "get_string: '%s'\n", errflg);
		errflg = saveflg;
		db_printf(2, "get_string: jumping to shliberr_jmpbuff\n");
		longjmp(shliberr_jmpbuff,1);
	}
	errflg = saveflg;
	db_printf(5, "get_string: returns %X\n", save);
	return save;
}



static struct asym *
enter64(Elf64_Sym *np, int flag, char *strtab)
{
	struct asym *s;
	char *name = (np->st_name) ? (strtab + np->st_name) : NULL;
	int h;

	db_printf(5, "enter: np=%X, flag=%D, NGLOBALS=%D\n",
							np, flag, NGLOBALS);
	/*
	 * if this is the first global entry,
	 * allocate the global symbol spaces
	 */
	if (NGLOBALS == 0) {
		NGLOBALS = GSEGSIZE;
		globals = (struct asym **)
		    malloc(GSEGSIZE * sizeof (struct asym *));
		if (globals == 0)
			outofmem();
		global_segment = nextglob = (struct asym *)
		    malloc(GSEGSIZE * sizeof(struct asym));
		if (nextglob == 0)
			outofmem();
	}
	/*
	 * if we're full up, reallocate the pointer array,
	 * and give us a new symbol segment
	 */
	if (nglobals == NGLOBALS) {
		NGLOBALS += GSEGSIZE;
		globals = (struct asym **)
		    realloc((char *)globals, NGLOBALS*sizeof (struct asym *));
		if (globals == 0)
			outofmem();
		global_segment = nextglob = (struct asym *)
		    malloc( GSEGSIZE*sizeof (struct asym));
		if (nextglob == 0)
			outofmem();
	}
	globals[nglobals++] = s = nextglob++;
	s->s_f = 0; s->s_fcnt = 0; s->s_falloc = 0;
	s->s_fileloc = 0;
	s->s_name = name;
	s->s_demname = NULL;
	s->s_type = ELF64_ST_TYPE(np->st_info);
	s->s_bind = ELF64_ST_BIND(np->st_info);
	s->s_flag = flag;
	if (flag == AOUT)
		s->s_value = np->st_value;
	else if (flag == REL) {
		s->s_value = rel_symoffset64(np);
	}
	else /* if (s->s_type != (N_EXT | N_UNDF)) XXXX FIXME XXXX */
             /* flag == SHLIB and not COMMON */
		s->s_value = np->st_value + SHLIB_OFFSET;
	db_printf(5, "enter: use_shlib=%D, new entry='%s'\n",
						use_shlib, s->s_name);
	if(use_shlib && strcmp((char*)s->s_name, "_DYNAMIC")==0){
		dynam_addr64 = (Elf64_Dyn*)(s->s_value);
		db_printf(2, "enter: found _DYNAMIC at %X\n", dynam_addr64);
	}
	if(use_shlib && strcmp((char*)s->s_name, "_r_debug_state")==0){
		rtld_state_addr = (addr_t)(s->s_value);
		db_printf(2, "enter: found _r_debug_state at %X\n",
							rtld_state_addr);
	}
	h = hashval(name);
	s->s_link = symhash[h];
	symhash[h] = s;
	db_printf(5, "enter: '%s', value %X hashed at %D\n",
						s->s_name, s->s_value, h);
	return (s);
}

void
re_enter64(struct asym *s, Elf64_Sym *es, int flag)
{
	db_printf(4, "re_enter: s->s_value=%X, flag=%D\n", s->s_value, flag);

	if (flag == REL && s->s_value == 0)
		s->s_value = rel_symoffset64(es);
	else
#if defined(KADB)
	if (flag != AOUT && s->s_value == 0)
#else
	if (flag == SHLIB)
#endif
		s->s_value = es->st_value + SHLIB_OFFSET;

	db_printf(4, "re_enter: s->s_value=%X\n", s->s_value);
}

#ifndef	KADB

void
read_in_shlib64(char *fname, unsigned long base)
{
	off_t loc;
	int file;
	Elf64_Ehdr    hdr;
	Elf64_Shdr    *secthdr;
	Elf64_Phdr    *proghdr;
	void add_map_range(struct map *, const unsigned long, const unsigned long, const unsigned long,
			   char *);

	db_printf(4, "read_in_shlib: fname='%s', base=%X\n",
		fname ? fname : "NULL", base);
	file = getfile(fname, INFINITE);
	if (file == -1) {
		(void) printf("Unable to open shared library %s\n", fname);
		return;
	}

	if (read(file, (char *) &hdr, sizeof hdr) != sizeof hdr ||
	    hdr.e_ident[EI_MAG0] != ELFMAG0 ||
	    hdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    hdr.e_ident[EI_MAG2] != ELFMAG2 ||
	    hdr.e_ident[EI_MAG3] != ELFMAG3 ||
	    hdr.e_type != ET_DYN || hdr.e_version != EV_CURRENT) {
		(void) printf("invalid ELF header for %s\n", fname);
		(void) close(file);
		return;
	}
	db_printf(2, "read_in_shlib: read the ELF header for %s\n",
		fname ? fname : "NULL");

	if (hdr.e_phnum == 0) {
		(void) printf("No rtld program header for %s\n", fname);
		(void) close(file);
		return;
	}
	/* Get space for a copy of the program header. */
	proghdr = (Elf64_Phdr *) tmp_malloc(hdr.e_phentsize * hdr.e_phnum);
	if (proghdr == NULL) {
		printf("Unable to allocate program header for %s\n", fname);
		(void) close(file);
		outofmem();
	}
	/* Seek to program header table and read it. */
	if ((loc = lseek(file, hdr.e_phoff, SEEK_SET) != hdr.e_phoff) ||
	    (read(file, (char *)proghdr,
		  hdr.e_phentsize * hdr.e_phnum) !=
	     hdr.e_phentsize * hdr.e_phnum)) {
		(void) printf("Unable to read program header for %s\n", fname);
		(void) close(file);
		return;
	}
	db_printf(2, "read_in_shlib: read the program header for %s\n",
		fname ? fname : "NULL");
	/* Get space for the section header. */
	secthdr = (Elf64_Shdr *) tmp_malloc (hdr.e_shentsize * hdr.e_shnum);
	if (secthdr == NULL) {
		(void) printf("Unable to allocate section header for %s\n",
			      fname);
		(void) close(file);
		outofmem();
	}
	/* Seek to section header and read it. */
	if ((loc = lseek(file, hdr.e_shoff, SEEK_SET) == -1) ||
	    (read(file, (char *)secthdr, hdr.e_shentsize * hdr.e_shnum) !=
					    hdr.e_shentsize * hdr.e_shnum)) {
		(void) printf("Unable to read section header for %s\n", fname);
		(void) close(file);
		return;
	}
	db_printf(2, "read_in_shlib: read the section header for %s\n",
		fname ? fname : "NULL");
	SHLIB_OFFSET = (unsigned long) base;
	db_printf(2, "read_in_shlib: SHLIB_OFFSET=%J\n", SHLIB_OFFSET);

	if (!stinit64(file, secthdr, hdr.e_shnum, SHLIB)) {
		int i;
		char *strtab = NULL;
		off_t loc;

		/*
		 * Find the string table section of the shared object.
		 */
		for (i = 0; i < (int) hdr.e_shnum; i++)
                        if (secthdr[i].sh_type == SHT_STRTAB) {
                                if ((strtab = (char *)
				    tmp_malloc(secthdr[i].sh_size)) == NULL) {
                                        (void) printf("Unable to allocate"
						      " section name table.\n");
					(void) close(file);
                                        outofmem();
                                }
                                if ((loc = lseek(file,
						 secthdr[i].sh_offset,
						 0) == -1) ||
                                    (read(file, strtab, secthdr[i].
                                          sh_size) != secthdr[i].sh_size)) {
                                        (void) printf("Unable to read section"
						      " names.\n");
					(void) close(file);
					return;
                                }
                                loc = (off_t) ((uintptr_t)strtab +
					       (int) secthdr[i].sh_name);
                                if (!strcmp((char *) loc, ".shstrtab"))
                                        break;          /* found it */
                        }
		if (i == (int) hdr.e_shnum) {
			(void) close(file);
			return;
		}

		/*
		 * Add the text and data sections of the shared objects
		 * to txtmap and datmap lists respectively.
		 */
		for (i = 0; i < (int) hdr.e_shnum; i++)
			if (secthdr[i].sh_type == SHT_PROGBITS &&
			    secthdr[i].sh_flags & SHF_EXECINSTR) {
				struct map *map;

                                loc = (off_t) ((uintptr_t) strtab +
                                        (uintptr_t) secthdr[i].sh_name);
                                if (!strcmp((char *) loc, ".text"))
					map = &txtmap;
				else if (!strcmp((char *) loc, ".data"))
					map = &datmap;
				else
					continue;
				add_map_range(map,
					      SHLIB_OFFSET + secthdr[i].sh_addr,
					      SHLIB_OFFSET + secthdr[i].sh_addr
					    		   + secthdr[i].sh_size,
					      secthdr[i].sh_offset,
					      fname);
			}

	}
	(void) close(file);
	return;
}

/*
 * read the ld_debug structure
 */
char *
read_ld_debug64(void)
{
	char *rvp = NULL;
        Elf64_Dyn *tmp1, *tmp2; /* pointers in debugger and target space*/

	jmp_buf save_jmpbuff;
	memcpy((char *)save_jmpbuff, (char *)shliberr_jmpbuff,
	    sizeof(save_jmpbuff));

	if(setjmp(shliberr_jmpbuff)) {
		goto out;
	}
	if (dynam_addr64 == 0){
		printf("_DYNAMIC is not defined \n");
		goto out;
	}
	tmp2 = dynam_addr64;
	db_printf(2, "read_ld_debug64: dynam_addr64 %J\n", dynam_addr64);
	for(;;) {
		db_printf(2, "read_ld_debug64: tmp2 %J\n", tmp2);
	    tmp1 = (Elf64_Dyn *)get_all((int *)tmp2, sizeof(Elf64_Dyn));
	    if (tmp1->d_tag == DT_NULL) {
		goto out;
	    }
	    if(tmp1->d_tag == DT_DEBUG) {
		break;
	    }
	    free(tmp1);
	    tmp2++;
	}
	db_printf(2, "read_ld_debug64: out of loop\n");
	ld_debug_addr = (struct r_debug *) tmp1->d_un.d_ptr;
	db_printf(2, "read_ld_debug64: %J\n", ld_debug_addr);
	rvp = get_all((int *)ld_debug_addr, sizeof(struct r_debug));
out:
	memcpy((char *)shliberr_jmpbuff, (char *)save_jmpbuff,
	    sizeof(save_jmpbuff));
	return (rvp);
}


#endif	/* !KADB */

#ifdef KADB


void
krtld_debug_setup64(Elf64_Ehdr *ehdr, char *shdrs, Elf64_Shdr *symhdr,
	char *text, char *data, u_int text_size, u_int data_size)
{
	symid_t i, *ip;
	struct module *mp = (struct module *)calloc(1, sizeof (struct module));

	bcopy(ehdr, &mp->hdr, sizeof (Elf64_Ehdr));
	mp->shdrs = shdrs;
	mp->text = text;
	mp->data = data;
	mp->text_size = text_size;
	mp->data_size = data_size;

	mp->symtbl = (char *)malloc(symhdr->sh_size);
	bcopy((void *)symhdr->sh_addr, mp->symtbl, symhdr->sh_size);

	mp->symhdr = (Elf64_Shdr *)malloc(sizeof (Elf64_Shdr));
	bcopy(symhdr, mp->symhdr, sizeof (Elf64_Shdr));

	mp->symhdr->sh_addr = (Elf64_Addr)mp->symtbl;
	mp->strhdr = (Elf64_Shdr *)
	    (shdrs + (symhdr->sh_link * ehdr->e_shentsize));
	mp->strings = (char *)mp->strhdr->sh_addr;

	mp->nsyms = symhdr->sh_size / symhdr->sh_entsize;
	mp->hashsize = 53;	/* value not critical; 53 is plenty */
	mp->buckets = (symid_t *)calloc(mp->hashsize, sizeof (symid_t));
	mp->chains = (symid_t *)calloc(mp->nsyms, sizeof (symid_t));

	for (i = 1; i < mp->nsyms; i++) {
		Elf64_Sym *symp = (Elf64_Sym *)(mp->symtbl +
		    (i * mp->symhdr->sh_entsize));
		if (symp->st_name != 0 && symp->st_shndx != SHN_UNDEF) {
			if (symp->st_shndx < SHN_LORESERVE) {
				Elf64_Shdr *shp = (Elf64_Shdr *)(mp->shdrs +
				    symp->st_shndx * mp->hdr.e_shentsize);
				symp->st_value += shp->sh_addr;
			}
			ip = &mp->buckets[kobj_hash_name(mp->strings +
			    symp->st_name) % mp->hashsize];
			while (*ip)
				ip = &mp->chains[*ip];
			*ip = i;	/* append to tail of hash chain */
		}
	}

	krtld_modctl.mod_mp = mp;
	krtld_modctl.mod_id = 1;
}
Elf64_Addr
dbg_kobj_getsymvalue64(char *name, int any_mod)
{
	symid_t idx;
	u_int symsize;
	Elf64_Sym *symp;
	Elf64_Addr retval = 0;
	struct modctl *modp;
	struct module *mp;

	if (*mod_mix_changed_kaddr != current_module_mix)
		kobj_init();

	if (kadb_curmod == -1 || (kadb_curmod == 0 && any_mod == 0))
		return (0);

	modp = modules_kaddr;
	while ((modp = modp->mod_next) != modules_kaddr) {
		mp = modp->mod_mp;
		if (mp == NULL || mp->symtbl == NULL)
			continue;
		symsize = mp->symhdr->sh_entsize;
		for (idx = mp->buckets[kobj_hash_name(name) % mp->hashsize];
		    idx != 0; idx = mp->chains[idx]) {
			symp = (Elf64_Sym *)(mp->symtbl + idx * symsize);
			if (strcmp(name, mp->strings + symp->st_name) == 0 &&
			    ELF64_ST_TYPE(symp->st_info) != STT_FILE &&
			    symp->st_shndx != SHN_UNDEF &&
			    symp->st_shndx != SHN_COMMON) {
				if (kadb_curmod == modp->mod_id)
					return (symp->st_value);
				if (symp->st_shndx == SHN_ABS) {
					retval = symp->st_value;
					break;
				}
				if (kadb_curmod == 0 && any_mod)
					return (symp->st_value);
				if (retval == 0 && any_mod) {
					retval = symp->st_value;
					break;
				}
			}
		}
	}
	return (retval);
}

typedef struct symcache {
        struct symcache *sc_prev;
        struct symcache *sc_next;
#ifdef  _LP64
        Elf64_Addr sc_start;                    /* Large enuf to hold */
        Elf64_Addr sc_end;                      /* Elf64 Addr         */ 
#else
        Elf32_Addr sc_start;                    /* Large enuf to hold */
        Elf32_Addr sc_end;                      /* Elf64 Addr         */ 
#endif
        char *sc_name;
} symcache_t;



/*
 * Converts value to a symbol name, or NULL.  The difference between
 * value and the real value of the symbol is stored in *offset.  The
 * returned value is a static buffer which is overwritten with each call.
 */
char *
dbg_kobj_getsymname64(Elf64_Addr value, u_int *offset)
{
	u_int symsize;
	Elf64_Sym *symp, *bestsymp, *lastsymp;
	Elf64_Addr bestval, nextval;
	struct modctl *modp;
	struct module *mp;
	symcache_t *scp;
	extern symcache_t *symcache_head;
	extern symcache_t symcache[];
	extern int symcache_hits, symcache_misses;

	
	if (*mod_mix_changed_kaddr != current_module_mix)
		kobj_init();

	for (scp = symcache_head; scp->sc_next != symcache_head;
	    scp = scp->sc_next) {
		if (scp->sc_start <= value && value < scp->sc_end) {
			if (scp != symcache_head) {
				scp->sc_prev->sc_next = scp->sc_next;
				scp->sc_next->sc_prev = scp->sc_prev;
				scp->sc_prev = symcache_head->sc_prev;
				scp->sc_next = symcache_head;
				scp->sc_prev->sc_next = scp;
				scp->sc_next->sc_prev = scp;
				symcache_head = scp;
			}
			symcache_hits++;
			*offset = value - scp->sc_start;
			return (scp->sc_name);
		}
	}
	symcache_misses++;

	db_printf(2, "dbg_kobj_getsymname(%X): symcache miss (rate %d%%)",
	    value, (symcache_misses * 100) / (symcache_hits + symcache_misses));

	bestval = 0;
	nextval = 0;
	modp = modules_kaddr;
	do {
		if ((mp = modp->mod_mp) == NULL)
			continue;
		if (value - (Elf64_Addr)mp->text < mp->text_size) {
			bestval = (Elf64_Addr)mp->text;
			nextval = (Elf64_Addr)mp->text + mp->text_size;
			break;
		}
		if (value - (Elf64_Addr)mp->data < mp->data_size) {
			bestval = (Elf64_Addr)mp->data;
			nextval = (Elf64_Addr)mp->data + mp->data_size;
			break;
		}
		if (value - (Elf64_Addr)mp->bss < mp->bss_size) {
			bestval = (Elf64_Addr)mp->bss;
			nextval = (Elf64_Addr)mp->bss + mp->bss_size;
			break;
		}
	} while ((modp = modp->mod_next) != modules_kaddr);

	if (bestval == 0)
		return (0);

	symsize = mp->symhdr->sh_entsize;
	bestsymp = NULL;
	lastsymp = (Elf64_Sym *)(mp->symtbl + mp->nsyms * symsize);

	for (symp = (Elf64_Sym *)mp->symtbl; symp < lastsymp;
	    symp = (Elf64_Sym *)((char *)symp + symsize)) {
		Elf64_Addr curval = symp->st_value;
		if (curval >= bestval && curval < nextval) {
			u_char curinfo = symp->st_info;
			if (ELF64_ST_BIND(curinfo) == STB_GLOBAL ||
			    (ELF64_ST_BIND(curinfo) == STB_LOCAL &&
			    (ELF64_ST_TYPE(curinfo) == STT_OBJECT ||
			    ELF64_ST_TYPE(curinfo) == STT_FUNC))) {
				if (value >= curval) {
					bestsymp = symp;
					bestval = curval;
				} else {
					nextval = curval;
				}
			}
		}
	}

	if (bestsymp == NULL || bestsymp->st_name == 0 ||
	    bestval + bestsymp->st_size < value) {
		db_printf(2, "dbg_kobj_getsymname: failed for %X", value);
		return (0);
	}

	scp->sc_start = bestval;
	scp->sc_end = nextval;
	scp->sc_name = mp->strings + bestsymp->st_name;
	symcache_head = scp;

	*offset = value - bestval;
	return (scp->sc_name);
}

#endif /* KADB */


