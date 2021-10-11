/*
 * Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * adb - symbol table routines
 */

#pragma ident	"@(#)sym.c	1.84	99/05/04 SMI"

#include <stdio.h>
#include "adb.h"
#include <string.h>
#include "fio.h"
#include "symtab.h"
#include <link.h>
#include <sys/types.h>
#include <sys/auxv.h>
#if	!defined(KADB)
#include <unistd.h>
#include <strings.h>
#endif
#include <sys/stat.h>
#include <setjmp.h>

static void dosym(Elf32_Sym *es, int flag, char *strtab);
static void mergedefs(struct asym *s, Elf32_Sym *es, char *strtab);
void re_enter(struct asym *s, Elf32_Sym *es, int flag);
extern void bzero();

#ifdef KADB

#include <sys/sysmacros.h>
#include <sys/elf.h>
#include <sys/kobj.h>


extern Elf64_Addr dbg_kobj_getsymvalue64(char *name, int any_mod);
Elf32_Addr dbg_kobj_getsymvalue(char *name, int any_mod);
char *dbg_kobj_getsymname(Elf32_Addr value, u_int *offset);
extern char *dbg_kobj_getsymname64(Elf64_Addr value, u_int *offset);

#if defined(__ia64)
/*
 * The following is used to record the base of the text segment needed
 * for ia64 stack unwinding.
 */
Elf64_Addr text_base;
int text_modid;
extern Elf64_Addr kernel_text_base;
#endif

int kadb_curmod;
int bkpt_curmod = -1;
int current_module_mix = -1;
int dummy_module_mix = -2;

#ifdef	_LP64
u_long stubs_base_kaddr;
u_long stubs_end_kaddr;
#else 
static u_int stubs_base_kaddr;
static u_int stubs_end_kaddr;
#endif
#ifdef __sparcv9
struct modctl32 {
        caddr32_t	mod_next;
        caddr32_t	mod_prev;
        int mod_id;
        caddr32_t mod_mp;
        caddr32_t mod_inprogress_thread;
        caddr32_t mod_modinfo;
        caddr32_t mod_linkage;
        caddr32_t mod_filename;
        caddr32_t mod_modname;
        int32_t mod_busy;
        int32_t mod_stub;                   /* currently executing via a stub */
        char mod_loaded;
        char mod_installed;
        char mod_loadflags;
        char mod_want;
        caddr32_t mod_requisites; /* Modules this one depends on. */
        caddr32_t mod_dependents; /* Modules depending on this one. */  
        int32_t mod_loadcnt;
};

struct module32 {
	int total_allocated;
	Elf32_Ehdr hdr;
	caddr32_t shdrs;
	caddr32_t symhdr, strhdr;

	caddr32_t depends_on;

	uint32_t symsize;
	caddr32_t symspace;	/* symbols + strings + hashtbl, or NULL */
	int flags;

	uint32_t text_size;
	uint32_t data_size;
	caddr32_t text;
	caddr32_t data;

	unsigned int symtbl_section;
	/* pointers into symspace, or NULL */
	caddr32_t symtbl;
	caddr32_t strings;

	unsigned int hashsize;
	caddr32_t buckets;
	caddr32_t chains;

	unsigned int nsyms;

	unsigned int bss_align;
	uint32_t  bss_size;
	uint32_t  bss;

	caddr32_t filename;

	caddr32_t head, tail;
};

struct modctl *modules_kaddr;
struct modctl32  *modules_kaddr32;
int *mod_mix_changed_kaddr = &dummy_module_mix;
#else
struct modctl *modules_kaddr;
int *mod_mix_changed_kaddr = &dummy_module_mix;
#endif

#endif /* KADB */

#define INFINITE 0x7fffffff
#ifdef _LP64
extern int elf64mode;
#endif
#ifdef	_LP64
static unsigned long SHLIB_OFFSET;
#else
static int SHLIB_OFFSET;
#endif
extern int adb_debug;
int use_shlib = 0;
int	address_invalid = 1;
static jmp_buf shliberr_jmpbuff;
Elf32_Dyn dynam;           /* root data structure for dl info */
Elf32_Dyn *dynam_addr;     /* its addr in target process */
struct r_debug *ld_debug_addr; /* the substructure that hold debug info */
struct r_debug ld_debug_struct; /* and its addr*/
extern char *rtld_path;    /* path name to which kernel hands control */
extern addr_t exec_entry;         /* addr that receives control after rtld's done*/
extern addr_t rtld_state_addr;    /*in case of adb support for dlopen/dlclose */

struct	asym *curfunc, *curcommon;
struct  asym *nextglob;	/* next available global symbol slot */
struct  asym *global_segment; /* current segment of global symbol table */
#define GSEGSIZE 150	/* allocate global symbol buckets 150 per shot */
int	curfile;

static struct asym *enter();
struct	afield *field();
struct	asym * lookup_base();

static int elf_stripped = 1; /* by default executables are stripped */

#define	HSIZE	255
struct	asym *symhash[HSIZE];
struct	afield *fieldhash[HSIZE];

extern	void qsort();

#ifndef KADB
void *tmp_malloc(), free_tmpmallocs();
addr_t *address_list = NULL;
ptrdiff_t list_ndx = 0;
int cant_record_allocs = 0;

/*  One bucket can hold 256 malloc'ed pointers */
#define	NELEM_PER_BUCKET	256
#define	BUCKET_SZ		(NELEM_PER_BUCKET * sizeof (addr_t))
#endif

#if	defined(KADB) && defined(__sparcv9)
void
convert_modctl(unsigned long modp,  char *name , int *id)
{
	struct modctl32 *tmodp = (struct modctl32 *) modp;	
	strcpy(name, (char *)tmodp->mod_modname);
	*id = tmodp->mod_id;
}
#endif


int
rel_symoffset(Elf32_Sym *sp)
{
	u_int val = sp->st_value;
	Elf32_Shdr *shp;
	extern Elf32_Shdr *secthdr;

	db_printf(5, "rel_symoffset: sp=%X, st_shndx=%X", sp, sp->st_shndx);
	/*
	 * bss and COMMON symbols have no corresponding
	 * file offset so we return MAXFILE.
	 */
	if (sp->st_shndx == SHN_COMMON || sp->st_shndx == SHN_ABS ||
	    (sp->st_shndx < filhdr.e_shnum &&
	    secthdr[sp->st_shndx].sh_type == SHT_NOBITS)) {
		db_printf(5, "rel_symoffset: return val=%d", MAXFILE);
		return (MAXFILE);
	}

	if (sp->st_shndx != SHN_UNDEF && sp->st_shndx < filhdr.e_shnum) {
		shp = (Elf32_Shdr *)((uintptr_t)secthdr +
			sp->st_shndx * filhdr.e_shentsize);
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

void
sort_globals(flag)
{
	int i;

	db_printf(5, "sort_globals: flag=%D", flag);
	if (nglobals == 0)
		return;
	globals = (struct asym **)
	   	    realloc((char *)globals, nglobals * sizeof (struct asym *));
	if (globals == 0)
		outofmem();
	/* arrange the globals in ascending value order, for findsym()*/
	qsort((char *)globals, nglobals, sizeof(struct asym *), symvcmp);
	/* prepare the free space for shared libraries */
	if (flag == AOUT || flag == REL)
		globals = (struct asym **)
		    realloc((char*)globals, NGLOBALS * sizeof(struct asym*));
}

stinit(int fsym, Elf32_Shdr *sh, int nsect, int flag)
{
	int sym_sect;		/* index of symbol section's header */
	int str_sect;		/* index of string section's header */
	char *strtab;		/* ptr to string table copy */
	Elf32_Sym elfsym[BUFSIZ/sizeof(Elf32_Sym)];
				 /* symbol table entry recepticle */
	Elf32_Sym *es;		/* ptr to ELF symbol */
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
	*(int *) strtab = sh[str_sect].sh_size + (int) sizeof(sh[str_sect].sh_size);

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
			int nread = ntogo, cc;
			if (nread > BUFSIZ / sizeof (Elf32_Sym))
				nread = BUFSIZ / sizeof (Elf32_Sym);
			cc = read(fsym, (char *) elfsym,
				nread * sizeof (Elf32_Sym));
			if (cc != nread * sizeof (Elf32_Sym))
				goto readerr;
			ninbuf = nread;
			es = elfsym;
		}
		dosym(es++, flag, strtab + sizeof (sh[str_sect].sh_size));
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
dosym(Elf32_Sym *es, int flag, char *strtab)
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
			db_printf(7, "dosym: discarded %s, it has ':'\n",
				  name);
			return;		/* must be dbx stab */
		}
	}

	db_printf(7, "dosym: ELF32_ST_TYPE(es->st_info)=%D,\n\t"
		     "ELF32_ST_BIND(es->st_info)=%D",
		  ELF32_ST_TYPE(es->st_info), ELF32_ST_BIND(es->st_info));

	switch (ELF32_ST_TYPE(es->st_info)) {
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
			if (ELF32_ST_BIND (es->st_info) == STB_LOCAL) {
				db_printf(4,
					 "dosym: threw away LOCAL symbol %s",
					  s->s_name);
				return;
			}
			if (ELF32_ST_BIND (es->st_info) == STB_WEAK &&
			    (s->s_bind == STB_GLOBAL || s->s_bind == STB_WEAK)){
				db_printf(4,
					 "dosym: threw away new WEAK symbol %s",
					  s->s_name);
				return;
			}
			if (ELF32_ST_BIND (es->st_info) == STB_GLOBAL &&
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
			if (ELF32_ST_BIND (es->st_info) == STB_GLOBAL &&
			    s->s_bind == STB_WEAK) {
				/*
				 * replace the old with the current one
				 */
				db_printf(4, "dosym: replacing WEAK symbol %s with its GLOBAL", s->s_name);
				s->s_f = NULL;
				s->s_fcnt = s->s_falloc = s->s_fileloc = 0;
				s->s_type = ELF32_ST_TYPE(es->st_info);
				s->s_bind = ELF32_ST_BIND(es->st_info);
				s->s_flag = flag;
				if (flag == AOUT)
					s->s_value = es->st_value;
				else if (flag == REL)
					s->s_value = rel_symoffset(es);
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
					dynam_addr = (Elf32_Dyn *) (s->s_value);
					db_printf(2,
						  "dosym: new _DYNAMIC at %X",
						  dynam_addr);
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
				re_enter(s, es, flag);
		} else
			s = enter(es, flag, strtab);

		db_printf(7, "dosym: ELF32_ST_TYPE(es->st_info) %s STT_FUNC",
		       (ELF32_ST_TYPE(es->st_info) == STT_FUNC) ? "==" : "!=");

		if (ELF32_ST_TYPE(es->st_info) == STT_FUNC) {
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
mergedefs(struct asym *s, Elf32_Sym *es, char *strtab)
{
	char *name = (es->st_name) ? (strtab + es->st_name) : NULL;

	db_printf(6, "mergedefs: ELF32_ST_BIND(es->st_info) %s STB_GLOBAL\n",
		     (ELF32_ST_BIND(es->st_info) == STB_GLOBAL) ? "==" : "!=");
	if (ELF32_ST_BIND(es->st_info) == STB_GLOBAL) {
		s->s_name = name;
		s->s_value = es->st_value;
		db_printf(7, "mergedefs: s->s_name=%s, s->s_value=%X\n",
			s->s_name ? s->s_name : "NULL", s->s_value);
	}
	else {

		s->s_type = ELF32_ST_TYPE(es->st_info);
		s->s_bind = ELF32_ST_BIND(es->st_info);
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

static struct asym *
enter(Elf32_Sym *np, int flag, char *strtab)
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
	s->s_type = ELF32_ST_TYPE(np->st_info);
	s->s_bind = ELF32_ST_BIND(np->st_info);
	s->s_flag = flag;
	if (flag == AOUT)
		s->s_value = np->st_value;
	else if (flag == REL) {
		s->s_value = rel_symoffset(np);
	}
	else /* if (s->s_type != (N_EXT | N_UNDF)) XXXX FIXME XXXX */
             /* flag == SHLIB and not COMMON */
		s->s_value = np->st_value + SHLIB_OFFSET;
	db_printf(5, "enter: use_shlib=%D, new entry='%s'\n",
						use_shlib, s->s_name);
	if(use_shlib && strcmp((char*)s->s_name, "_DYNAMIC")==0){
		dynam_addr = (Elf32_Dyn*)(s->s_value);
		db_printf(2, "enter: found _DYNAMIC at %X\n", dynam_addr);
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

void
re_enter(struct asym *s, Elf32_Sym *es, int flag)
{
	db_printf(4, "re_enter: s->s_value=%X, flag=%D\n", s->s_value, flag);

	if (flag == REL && s->s_value == 0)
		s->s_value = rel_symoffset(es);
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
/* Navigate the stack layout that accompanies an exec() of an ELF with
 * PT_INTERP set (see eg rt_boot.s in the rtld code). Find AT_BASE and
 * return it.
 * On sparc:
 *     sp->(window save, argc, argv,0,envp,0,auxv,0,info_block)
 * On i386:
 *     sp->(argc, argv,0,envp,0,auxv,0,info_block)
 */
#ifdef _LP64
unsigned long
#endif
find_rtldbase(void)
{
#ifdef _LP64
    long base = 0;
#else
    int base = 0;
#endif
    auxv_t *auxv;
    extern auxv_t *FetchAuxv();

    for (auxv = FetchAuxv(); auxv && auxv->a_type != AT_NULL; auxv++) {
	switch(auxv->a_type) {
	    case AT_BASE:
		base = auxv->a_un.a_val;
		break;

	    case AT_ENTRY:
		exec_entry = (addr_t) auxv->a_un.a_val;
		break;
	}
    }
    if (base) {
        db_printf(2, "find_rtldbase: base of the rtld=%J\n", base);
	return base;
    } else {
	(void) printf("error rtld auxiliary vector: AT_BASE not defined\n");
	exit(1);
    }
}

void
#ifdef _LP64
read_in_shlib(char *fname, unsigned long base)
#else
read_in_shlib(char *fname, int base)
#endif
{
	off_t loc;
	int file;
	Elf32_Ehdr    hdr;
	Elf32_Shdr    *secthdr;
	Elf32_Phdr    *proghdr;
#ifdef	_LP64
	void add_map_range(struct map *, const unsigned long, const unsigned long, const unsigned long,
			   char *);
#else
	void add_map_range(struct map *, const int, const int, const int,
			   char *);
#endif

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
	proghdr = (Elf32_Phdr *) tmp_malloc(hdr.e_phentsize * hdr.e_phnum);
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
	secthdr = (Elf32_Shdr *) tmp_malloc(hdr.e_shentsize * hdr.e_shnum);
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
	SHLIB_OFFSET = base;
	db_printf(2, "read_in_shlib: SHLIB_OFFSET=%J\n", SHLIB_OFFSET);

	if (!stinit(file, secthdr, hdr.e_shnum, SHLIB)) {
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
                                loc = (off_t)((uintptr_t)strtab +
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

                                loc = (off_t)((uintptr_t)strtab +
                                        (int) secthdr[i].sh_name);
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

void
read_in_rtld(void)
{
	struct bkpt *bkptr;
	extern struct bkpt *get_bkpt(addr_t, int, int);

	db_printf(4, "read_in_rtld: called\n");

#ifdef _LP64
	if (elf64mode)
		read_in_shlib64(rtld_path, find_rtldbase());
	else
#endif
		read_in_shlib(rtld_path, find_rtldbase());
	/*
	 * set a bkpt at the executable's entry point, aka, start
	 */
	db_printf(2, "read_in_rtld: exec_entry=%X\n", exec_entry);
	bkptr = get_bkpt(exec_entry, BPINST, SZBPT);
	bkptr->flag = BKPTSET;
	bkptr->count = bkptr->initcnt = 1;
	bkptr->comm[0] = '\n';
	bkptr->comm[1] = '\0';
	db_printf(4, "read_in_rtld: bkptr=%X\n", bkptr);
}

/*
 * read the ld_debug structure
 */
char *
read_ld_debug(void)
{
	char *rvp = NULL;
        Elf32_Dyn *tmp1, *tmp2; /* pointers in debugger and target space*/

	jmp_buf save_jmpbuff;
	memcpy((char *)save_jmpbuff, (char *)shliberr_jmpbuff,
	    sizeof(save_jmpbuff));

	if(setjmp(shliberr_jmpbuff)) {
		goto out;
	}
	if (dynam_addr == 0){
		printf("_DYNAMIC is not defined \n");
		goto out;
	}
	tmp2 = dynam_addr;
	for(;;) {
	    tmp1 = (Elf32_Dyn *)get_all((int *)tmp2, sizeof(Elf32_Dyn));
	    if (tmp1->d_tag == DT_NULL) {
		goto out;
	    }
	    if(tmp1->d_tag == DT_DEBUG) {
		break;
	    }
	    free(tmp1);
	    tmp2++;
	}
	ld_debug_addr = (struct r_debug*) tmp1->d_un.d_ptr;
	rvp = get_all((int *)ld_debug_addr, sizeof(struct r_debug));
out:
	memcpy((char *)shliberr_jmpbuff, (char *)save_jmpbuff,
	    sizeof(save_jmpbuff));
	return (rvp);
}

void
scan_linkmap(void)
{
    struct r_debug *dptr;
#ifdef	_LP64
    struct r_debug32 *dptr32;
#endif
    addr_t lmp_addr;
    struct link_map *lmp;
#ifdef _LP64
    struct link_map32 *lmp32; 	/* To read Elf32 */
#endif
	struct stat buf1, buf2;
    char *name;

    db_printf(5, "scan_linkmap: called\n");
#ifdef	_LP64
    if (elf64mode)
    	dptr = (struct r_debug *) read_ld_debug64();
    else
#endif
#ifdef	_LP64
    	dptr32 = (struct r_debug32 *) read_ld_debug();
#else
    	dptr = (struct r_debug *) read_ld_debug();
#endif
    db_printf(2, "scan_linkmap: dptr=%X\n", dptr);
#ifdef	_LP64
    if(elf64mode){
	if (dptr == NULL) return;
    } else {
	if (dptr32 == NULL) return;
    }
#else
	if(dptr == NULL)
	return;
#endif
#ifdef _LP64
if (elf64mode){
#endif
    for(lmp_addr = (addr_t) dptr->r_map; lmp_addr;
					 lmp_addr = (addr_t) lmp->l_next) {
	lmp = (struct link_map *)get_all((int *)lmp_addr,
		sizeof(struct link_map));
        if(lmp->l_name) {
	    name = get_string((int *)lmp->l_name);
	    db_printf(2, "scan_linkmap: name='%s'\n",
		name ? name : "NULL");
	    if(strcmp(name, rtld_path) && strcmp(name, symfil)) {
		/*
		 * More than the name check, the i-number/dev-number check
		 * uniquely identifies regular files. This sort of check
		 * becomes necessary when 'name' is the original executable
		 * and 'symfil' is a hard link of the file (as it happens with
		 * the ":A" command)
		 */
		if ((stat(name, &buf1) == 0) && (stat(symfil, &buf2) == 0)) {
		    if ((buf1.st_dev != buf2.st_dev) ||
					    (buf1.st_ino != buf2.st_ino)) {
#ifdef	_LP64
			if (elf64mode)
			    read_in_shlib64(name, lmp->l_addr);
			else
#endif
			    read_in_shlib(name, lmp->l_addr);
		    }
		}
	    }
        }
        if(lmp->l_name)
            db_printf(5, "scan_linkmap: %s at %X\n", 
		name ? name : "NULL", lmp->l_addr);
	else
            db_printf(5, "scan_linkmap: (noname) at %X\n", lmp->l_addr);
    }
#ifdef _LP64
} else {

    for(lmp_addr = (addr_t) dptr32->r_map; lmp_addr;
					 lmp_addr = (addr_t) lmp32->l_next) {
	lmp32 = (struct link_map32 *)get_all((int *)lmp_addr,
		sizeof(struct link_map32));
        if(lmp32->l_name) {
	    name = get_string((int *)lmp32->l_name);
	    db_printf(2, "scan_linkmap: name='%s'\n",
		name ? name : "NULL");
	    if(strcmp(name, rtld_path) && strcmp(name, symfil)) {
		/*
		 * More than the name check, the i-number/dev-number check
		 * uniquely identifies regular files. This sort of check
		 * becomes necessary when 'name' is the original executable
		 * and 'symfil' is a hard link of the file (as it happens with
		 * the ":A" command)
		 */
		if ((stat(name, &buf1) == 0) && (stat(symfil, &buf2) == 0)) {
		    if ((buf1.st_dev != buf2.st_dev) ||
					    (buf1.st_ino != buf2.st_ino)) {
			read_in_shlib(name, (unsigned long) lmp32->l_addr);
		    }
		}
	    }
        }
        if(lmp32->l_name)
            db_printf(5, "scan_linkmap: %s at %X\n", 
		name ? name : "NULL", lmp32->l_addr);
	else
            db_printf(5, "scan_linkmap: (noname) at %X\n", lmp32->l_addr);
    }
}
#endif
}

#endif	/* !KADB */

struct asym *
lookup_base(char *cp)
{
	struct asym *s;
	int h;

	char *kcp;

	h = hashval(cp);
	errflg = 0;
	for (s = symhash[h]; s; s = s->s_link)
		if (!strcmp(s->s_name, cp)) {
			db_printf(3, "lookup_base('%s') = %X",
				cp ? cp : "NULL", s->s_value);
			cursym = s;
			return (s);
		}
	cursym = 0;
	db_printf(4, "lookup_base: returns 0");
	return (0);
}

/*
 * Note that for adb, lookup() and lookup_base() are identical.  They vary
 * under kadb in the order of symbol lookup and the building of symbol
 * table entries.
 */
struct asym *
lookup(char *cp)
{
	struct asym *s;
#ifdef KADB
	int any_mod;
	static char namebuf[MAXSYMSIZE];
	static struct asym asym;
#endif	/* KADB */

	db_printf(4, "lookup: cp='%s'", cp ? cp : "NULL");

#ifdef KADB
	/*
	 * kadb_curmod may be set via "<modid>::curmod" (ecmd syntax)
	 *
	 * if kadb_curmod == 0, then precedence is:
	 *
	 * 	base symbols
	 *	other module symbols in mod_id order
	 *
	 * if kadb_curmod == -1, then precedence is:
	 *	base symbols
	 *
	 * otherwise, kadb_curmod contains the id of the module
	 * to search first.
	 *
	 *	symbols from selected module
	 *	base symbols
	 *	other module symbols in mod_id order
	 */

	any_mod = 1;
	if ((s = lookup_base(cp)) != 0) {
		if (stubs_base_kaddr && stubs_end_kaddr &&
		   (s->s_value < stubs_base_kaddr ||
		    s->s_value >= stubs_end_kaddr))
			any_mod = 0;
		db_printf(4, "lookup: any_mod set to 0");
	}
#ifdef	_LP64
	if (elf64mode) {
		long val;
		if ((val = dbg_kobj_getsymvalue64(cp, any_mod)) != 0) {
			bzero((char *)&asym, sizeof (struct asym));
			if (strlen(cp) < sizeof (namebuf))
				strcpy(namebuf, cp);
			else {
				strncpy(namebuf, cp, sizeof (namebuf) - 1);
				namebuf[sizeof (namebuf) - 1] = '\0';
			}
			asym.s_name = namebuf;
			asym.s_demname = NULL;
			asym.s_type = N_GSYM;
			asym.s_flag = AOUT;
			asym.s_value = val;
			s = &asym;
		}
	} else
#endif
	{
		int val;
		if ((val = dbg_kobj_getsymvalue(cp, any_mod)) != 0) {
			bzero((char *)&asym, sizeof (struct asym));
			if (strlen(cp) < sizeof (namebuf))
				strcpy(namebuf, cp);
			else {
				strncpy(namebuf, cp, sizeof (namebuf) - 1);
				namebuf[sizeof (namebuf) - 1] = '\0';
			}
			asym.s_name = namebuf;
			asym.s_demname = NULL;
			asym.s_type = N_GSYM;
			asym.s_flag = AOUT;
			asym.s_value = val;
			s = &asym;
		}
	}

#else	/* KADB */

	s = lookup_base(cp);

#endif

	errflg = 0;
	cursym = s;
	if (s != NULL)
		db_printf(3, "lookup('%s') = %X", cp ? cp : "NULL",
			cursym->s_value);
	else
		db_printf(4, "lookup: returns 0");
	return (cursym);
}

/*
 * The type given to findsym should be an enum { NSYM, ISYM, or DSYM },
 * indicating which instruction symbol space we're looking in (no space,
 * instruction space, or data space).  On VAXen, 68k's and SPARCs,
 * ISYM==DSYM, so that distinction is vestigial (from the PDP-11).
 */
#ifdef _LP64
findsym(long val, int type)
#else
findsym(int val, int type)
#endif
{
	struct asym *s;
	int i, j, k;
	u_int offset;
	char *p;
#ifdef KADB
	static char namebuf[MAXSYMSIZE];
#endif
	static struct asym asym;

	db_printf(6, "findsym: val=%X, type=%D", val, type);
	cursym = 0;
	if (type == NSYM)
		return (MAXINT);

	bzero((char *)&asym, sizeof (struct asym));

#ifdef KADB
#ifdef	_LP64
	if (elf64mode){
		if ((p = dbg_kobj_getsymname64(val, &offset)) != NULL) {
			if (strlen(p) < sizeof (namebuf))
				strcpy(namebuf, p);
			else {
				strncpy(namebuf, p, sizeof (namebuf) - 1);
				namebuf[sizeof (namebuf) - 1] = '\0';
			}
			asym.s_name = namebuf;
			asym.s_demname = NULL;
			asym.s_type = N_GSYM;
			asym.s_flag = AOUT;
			asym.s_value = val - offset;
			s = &asym;
			goto found;
		}
	} else {
#endif
		if ((p = dbg_kobj_getsymname(val, &offset)) != NULL) {
			if (strlen(p) < sizeof (namebuf))
				strcpy(namebuf, p);
			else {
				strncpy(namebuf, p, sizeof (namebuf) - 1);
				namebuf[sizeof (namebuf) - 1] = '\0';
			}
			asym.s_name = namebuf;
			asym.s_demname = NULL;
			asym.s_type = N_GSYM;
			asym.s_flag = AOUT;
			asym.s_value = val - offset;
			s = &asym;
			goto found;
		}
#ifdef	_LP64
	}
#endif
#endif
	i = 0; j = nglobals - 1;
	while (i <= j) {
		k = (i + j) / 2;
		s = globals[k];
		if (s->s_value == val) {
			j = k;
			goto found2;
		}
		if (s->s_value > val)
			j = k - 1;
		else
			i = k + 1;
	}
	if (j < 0)
		return (MAXINT);
	s = globals[j];
found2:
	if (type == ISYM) {
		while (j < nglobals - 1 && s->s_value == globals[j+1]->s_value)
			j++;
		/* ia64 has __1_... garbage symbols that break stacktrace */
		while (globals[j]->s_type != STT_FUNC && j > 0 &&
		    (globals[j]->s_value == globals[j-1]->s_value ||
		    strncmp(globals[j]->s_name, "__1_", 4) == 0))
			j--;
		s = globals[j];
	}
#if defined(__ia64)
	text_base = kernel_text_base;
	text_modid = -1;
#endif
found:
	/*
	 * If addr is zero, fail.  Otherwise, *any*
	 * value will come out as [symbol + offset]
	 */
	if (s->s_value == 0)
		return (MAXINT);
	errflg = 0;
	db_printf(3, "findsym(%X) = %s+0x%X",
		  val, s->s_name ? s->s_name : "NULL", val - s->s_value);
	cursym = s;
	return ((int)(val - s->s_value));	/* assume this is an int */
}

/*
 * Given a value v, of type type, find out whether it's "close" enough
 * to any symbol; if so, print the symbol and offset.  The third
 * argument is a format element to follow the symbol, e.g., ":%16t".
 * If the special variable '_' is non-zero, just print the value.
 * SEE ALSO:  ssymoff, below.
 */
void
#ifdef _LP64
psymoff(long v, int type, char *s)
#else
psymoff(int v, int type, char *s)
#endif
{
	int symbolic = (var[PSYMVAR] == 0);
	unsigned w;

	if (v && symbolic)
		w = (unsigned) findsym(v, type);

	if (!(v && symbolic) || w >= maxoff) {
#ifdef _LP64
		printf("%J", v);
#else
		printf("%Z", v);
#endif
	} else {
		printf("%s", demangled(cursym));
#ifndef KADB
		fflush(stdout);
#endif
		if (w)
			printf("+%Z", w);
	}
	printf(s);
#ifndef KADB
	fflush(stdout);
#endif
}


/*
 * ssymoff is like psymoff, but uses sprintf instead of printf.
 * If we find a symbol that is close enough (< maxoff) use the
 * symbol + offset, omitting offset when zero.  If value is 0,
 * no symbol is found, value too far from symbol, or the buffer
 * is too small to hold the symbol + offset, create a string
 * that is the hex representation of the address.  Values greater
 * than or equal to 0xa have "0x" prepended.
 *
 * ssymoff returns the offset, so the caller can decide whether
 * or not to print anything.
 *
 * NOTE:  Because adb's own printf doesn't provide sprintf, we
 * must use the system's sprintf, which lacks adb's special "%Z"
 * and "%t" format effectors.
 */
#ifdef _LP64
ssymoff(long v, int type, char *buf, size_t bufsize)
#else
ssymoff(int v, int type, char *buf, size_t bufsize)
#endif
{
	unsigned w = 0;
	unsigned long ulv;
	/* two hex characters for each byte plus "+0x" plus the nul */
	char numbuf[sizeof (unsigned long) * 2 + 3 + 1];
	char *nbp;
	size_t slen, nlen;
	int done;

	db_printf(6, "ssymoff: v=%X, type=%D", v, type);

	slen = 0;
	if (v != 0) {
		w = (unsigned)findsym(v, type);
		if (w < maxoff) {
			slen = strlen(demangled(cursym));
			/*
			 * if the symbol won't fit in the buf, pretend
			 * we didn't find it, user will see its address
			 */
			if (bufsize < (slen + 1)) {
				w = maxoff;
				slen = 0;
			}
		}
	}

	ulv = v;
	if (ulv == 0 || w != 0) {
		done = 0;
		do {
			nbp = numbuf;
			if (w > 0 && w < maxoff) {
				ulv = w;
				*nbp++ = '+';
			}
			if (ulv >= 10) {
				*nbp++ = '0';
				*nbp++ = 'x';
			}
			sprintf(nbp, "%lx", ulv);
			nlen = strlen(numbuf);
			/*
			 * we don't exepct this to ever happen because
			 * all buffers should be at least big enough
			 * to hold a hex representation of 8 bytes
			 */
			if (bufsize < (nlen + 1)) {
				printf("error: buf too small for ssymoff\n");
				*buf = '\0';
				return (w);
			}

			/*
			 * see if appending the offset has made it too
			 * big for the buffer, if so loop around and get
			 * the string for the address
			 */
			if (bufsize < (slen + nlen + 1)) {
				ulv = v;
				w = maxoff;
				slen = 0;
			} else
				done = 1;
		} while (!done);
	} else
		nlen = 0;

	if (slen > 0)
		strcpy(buf, demangled(cursym));
	if (nlen > 0)
		strcpy(buf + slen, numbuf);

	db_printf(6, "ssymoff: buf='%s', returns %X", buf, w);
	return (w);
}

struct afield *
#ifdef	_LP64
field(void *np, struct afield **fpp, int *fnp, int *fap)
#else
field(Elf32_Sym *np, struct afield **fpp, int *fnp, int *fap)
#endif
{
	struct afield *f;

	if (*fap == 0) {
		*fpp = (struct afield *)
		    calloc(10, sizeof (struct afield));
		if (*fpp == 0)
			outofmem();
		*fap = 10;
	}
	if (*fnp == *fap) {
		*fap *= 2;
		*fpp = (struct afield *)
		    realloc((char *)*fpp, *fap * sizeof (struct afield));
		if (*fpp == 0)
			outofmem();
	}
	f = *fpp + *fnp; (*fnp)++;
#ifdef	_LP64
	if (elf64mode){
		f->f_name = (char *)(((Elf64_Sym *)np)->st_name);
		f->f_type = ELF64_ST_TYPE(((Elf64_Sym * )np)->st_info);
		f->f_offset = ((Elf64_Sym *) np)->st_value;
	}else{
#endif
		f->f_name = (char *)(((Elf32_Sym * )np)->st_name);
		f->f_type = ELF32_ST_TYPE(((Elf32_Sym * )np)->st_info);
		f->f_offset = ((Elf32_Sym * )np)->st_value;
#ifdef	_LP64
	}
#endif
	return (f);
}

/*
 * implement the $p directive by munging throught the global
 * symbol table and printing the names of any N_FUNs we find
 */
void
printfuns(void)
{
	struct asym **p, *q;
	int i;

	for (p = globals, i = nglobals; i > 0; p++, i--) {
		if ((q = *p)->s_type == N_FUN)
			printf("\t%s\n", q->s_name);
	}
}

void
outofmem(void)
{
	printf("ran out of memory for symbol table.\n");
#if	defined(KADB)
	exit(1, 1);
#else
	exit(1);
#endif
}

#ifdef KADB

#ifdef sparc
#define	ELF_TARGET_SPARC
#endif

#ifndef _LP64
static unsigned int
#else
unsigned int
#endif
kobj_hash_name(char *p)
{
	unsigned long g;
	unsigned int hval = 0;

	while (*p) {
		hval = (hval << 4) + *p++;
		if ((g = (hval & 0xf0000000)) != 0)
			hval ^= g >> 24;
		hval &= ~g;
	}
	return (hval);
}

#ifndef __sparcv9
struct modctl krtld_modctl;
#else
struct modctl32 krtld_modctl32;
struct modctl krtld_modctl;

#ifdef out
void modctl2modctl32(mp, mp32)
struct modctl *mp;
struct modctl32 *mp32;
{
	mp32->mod_next = (caddr32_t) mp->mod_next;
	mp32->mod_prev = (caddr32_t) mp->mod_prev;
	mp32->mod_id = (int) mp->mod_id;
	mp32->mod_mp = (caddr32_t) mp->mod_mp;
	mp32->mod_inprogress_thread = (caddr32_t) mp->mod_inprogress_thread;
	mp32->mod_modinfo = (caddr32_t) mp->mod_modinfo;
	mp32->mod_linkage = (caddr32_t) mp->mod_linkage;
	mp32->mod_busy = (int32_t) mp->mod_busy;
	mp32->mod_stub = (int32_t) mp->mod_stub;
	mp32->mod_loaded = (char) mp->mod_loaded;
	mp32->mod_installed = (char) mp->mod_installed;
	mp32->mod_loadflags = (char) mp->mod_loadflags;
	mp32->mod_want = (char) mp->mod_want;
	mp32->mod_requisites = (caddr32_t) mp->mod_requisites;
	mp32->mod_dependents = (caddr32_t) mp->mod_dependents;
	mp32->mod_loadcnt = (int32_t) mp->mod_loadcnt;
}
#endif
void mtom32(m, m32)
struct module *m;
struct module32 *m32;
{
	m32->total_allocated = m->total_allocated;
        bcopy((caddr_t) &(m->hdr), (caddr_t) &(m32->hdr), sizeof(Elf32_Ehdr));
        m32->symhdr = (caddr32_t) m->symhdr;
        m32->strhdr = (caddr32_t) m->strhdr;
        m32->depends_on = (caddr32_t) m32->depends_on;
        m32->symsize = (uint32_t) m->symsize;
        m32->symspace = (caddr32_t) m->symspace;
        m32->flags = m->flags;
        m32->text_size = (uint32_t) m->text_size;
        m32->data_size = (uint32_t) m->data_size;
        m32->text = (caddr32_t) m->text;
        m32->data = (caddr32_t) m->data;
        m32->symtbl_section = m->symtbl_section;
        m32->symtbl = (caddr32_t) m->symtbl;
        m32->strings = (caddr32_t) m->strings;
        m32->hashsize = (unsigned int) m->hashsize;
        m32->buckets = (caddr32_t) m->buckets;
        m32->chains = (caddr32_t) m->chains;
        m32->nsyms = (unsigned int) m->nsyms;
        m32->bss_align = (unsigned int) m->bss_align;
        m32->bss_size = (unsigned int) m->bss_size;
        m32->bss = (unsigned int) m->bss;
        m32->filename = (caddr32_t) m->filename;
        m32->head = (caddr32_t) m->head;
        m32->tail = (caddr32_t) m->tail;

}
#endif

void
krtld_debug_setup(Elf32_Ehdr *ehdr, char *shdrs, Elf32_Shdr *symhdr,
	char *text, char *data, u_int text_size, u_int data_size)
{
	symid_t i, *ip;
#ifdef __sparcv9
	struct module32 *mp32 = (struct module32 *)calloc(1, sizeof (struct module32));
	struct module *mp = (struct module *)calloc(1, sizeof (struct module));
#else
	struct module *mp = (struct module *)calloc(1, sizeof (struct module));
#endif

	bcopy(ehdr, &mp->hdr, sizeof (Elf32_Ehdr));
	mp->shdrs = shdrs;
	mp->text = text;
	mp->data = data;
	mp->text_size = text_size;
	mp->data_size = data_size;

	mp->symtbl = (char *)malloc(symhdr->sh_size);
	bcopy(symhdr->sh_addr, mp->symtbl, symhdr->sh_size);

	mp->symhdr = (Elf32_Shdr *)malloc(sizeof (Elf32_Shdr));
	bcopy(symhdr, mp->symhdr, sizeof (Elf32_Shdr));

	mp->symhdr->sh_addr = (Elf32_Addr)mp->symtbl;
	mp->strhdr = (Elf32_Shdr *)
	    (shdrs + (symhdr->sh_link * ehdr->e_shentsize));
	mp->strings = (char *)mp->strhdr->sh_addr;

	mp->nsyms = symhdr->sh_size / symhdr->sh_entsize;
	mp->hashsize = 53;	/* value not critical; 53 is plenty */
	mp->buckets = (symid_t *)calloc(mp->hashsize, sizeof (symid_t));
	mp->chains = (symid_t *)calloc(mp->nsyms, sizeof (symid_t));

	for (i = 1; i < mp->nsyms; i++) {
		Elf32_Sym *symp = (Elf32_Sym *)(mp->symtbl +
		    (i * mp->symhdr->sh_entsize));
		if (symp->st_name != 0 && symp->st_shndx != SHN_UNDEF) {
			if (symp->st_shndx < SHN_LORESERVE) {
				Elf32_Shdr *shp = (Elf32_Shdr *)(mp->shdrs +
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

#ifdef __sparcv9
	mtom32(mp, mp32);		/* Convert to a mp32 */
	krtld_modctl32.mod_mp = (caddr32_t) mp32;
	krtld_modctl32.mod_id = 1;
	krtld_modctl.mod_mp = mp;
	krtld_modctl.mod_id = 1;
#else
	krtld_modctl.mod_mp = mp;
	krtld_modctl.mod_id = 1;
#endif
}

/*
 * Symbol lookup cache.  Keeps track of the last few symbols used.
 * Implements LRU by doing move-to-front for successful lookups.
 */
#define	SYMCACHE_SIZE		64		/* must be a power of 2 */
#define	SYMCACHE_MASK		(SYMCACHE_SIZE - 1)

typedef struct symcache {
	struct symcache *sc_prev;
	struct symcache *sc_next;
#ifdef	_LP64
	Elf64_Addr sc_start;			/* Large enuf to hold */
	Elf64_Addr sc_end;			/* Elf64 Addr 	      */
#if defined(__ia64) && defined(_KADB)
        Elf64_Addr sc_text;
	int sc_modid;
#endif
#else
	Elf32_Addr sc_start;			/* Large enuf to hold */
	Elf32_Addr sc_end;			/* Elf64 Addr 	      */
#endif
	char *sc_name;
} symcache_t;

symcache_t symcache[SYMCACHE_SIZE];
symcache_t *symcache_head;
#ifdef _LP64
int symcache_hits;
int symcache_misses;
#else
static int symcache_hits;
static int symcache_misses;
#endif

/*
 * The following chunk of code looks perfectly innocent.  Look closer.
 * We dereference kernel pointers directly!  This works because kadb
 * runs in the same context as the kernel on every platform we support.
 * The only caveat is that you have to "know" the pointer is good.
 * We know that here because (1) we detect changes in the module list
 * (since the last time we dove into kadb) by checking mod_mix_changed;
 * (2) the module list can't change while we're in kadb (since the kernel
 * is stopped); and (3) module symbol tables cannot change while a module
 * is loaded.  So we can fairly assume that the kernel's symbol tables
 * are good -- if they weren't, kadb's symbol processing would be broken
 * in any event.  The reason any of this matters is that we're walking
 * through a lot of symbols and data structures here.  Changing this code
 * to use kernel pointers directly has yielded a marked improvement in
 * kadb's responsiveness.  It also simplified the code a LOT (~1000 lines).
 */
#ifdef _LP64
void
#else
static void
#endif
kobj_init(void)
{
	int i;
	struct asym *s;
	
	db_printf(2, "kobj_init: flushing symbol cache");

	for (i = 0; i < SYMCACHE_SIZE; i++) {
		symcache[i].sc_prev = &symcache[(i - 1) & SYMCACHE_MASK];
		symcache[i].sc_next = &symcache[(i + 1) & SYMCACHE_MASK];
		symcache[i].sc_start = 0;
		symcache[i].sc_end = 0;
	}
	symcache_head = &symcache[0];
	symcache_hits = 0;
	symcache_misses = 0;

	if ((s = lookup_base("stubs_base")) == 0)
		return;
	stubs_base_kaddr = s->s_value;
	if ((s = lookup_base("stubs_end")) == 0)
		return;
	stubs_end_kaddr = s->s_value;
#ifdef __sparcv9
	if (elf64mode) {
#endif
		if ((s = lookup_base("modules")) == 0)
			return;
		modules_kaddr = (struct modctl *)(s->s_value);
#ifdef __sparcv9
	} else modules_kaddr32 = (struct modctl32 *) lookup_base("modules")->s_value;
#endif
	if ((s = lookup_base("mod_mix_changed")) == 0)
		return;
	mod_mix_changed_kaddr = (int *)s->s_value;
	current_module_mix = *mod_mix_changed_kaddr;

	db_printf(2, "kobj_init: modmix=%D", current_module_mix);

#ifdef __sparcv9
	if (elf64mode){
#endif
		if (modules_kaddr->mod_next == NULL) {
		/*
		 * krtld hasn't initialized the modules list yet, so we
		 * fake it up here to enable symbol lookups.  This saves
		 * us from having to special-case a bunch of code elsewhere.
		 * (This will be overwritten when krtld initializes 'modules'
		 * for real, so it's harmless.)  Note: krtld_modctl is only
		 * initialized when booting with -k (RB_KRTLD), but that's
		 * OK -- we always have to check for mod_mp == NULL anyway.
		 */
			krtld_modctl.mod_next = modules_kaddr;
			modules_kaddr->mod_next = &krtld_modctl;
		}
#ifdef __sparcv9
	} else {
		if (modules_kaddr32->mod_next == NULL) {
			krtld_modctl32.mod_next = (caddr32_t) modules_kaddr32;
			modules_kaddr32->mod_next = (caddr32_t) &krtld_modctl32;		}
	}
#endif
}

#ifdef __sparcv9
void m32tom(m32, m)
struct module32 *m32;
struct module *m;
{
	m->total_allocated = m32->total_allocated;
	bcopy(&(m32->hdr), &(m->hdr), sizeof(Elf32_Ehdr)); 
	m->symhdr = (Elf32_Shdr *) m32->symhdr;
	m->strhdr = (Elf32_Shdr *) m32->strhdr;
	m->depends_on = (char *) m32->depends_on;
	m->symsize = (size_t) m32->symsize;
	m->symspace = (char *) m32->symspace;
	m->flags = m32->flags;
	m->text_size = (size_t) m32->text_size;
	m->data_size = (size_t) m32->data_size;
	m->text = (char *) m32->text;
	m->data = (char *) m32->data;
	m->symtbl_section = m32->symtbl_section;
	m->symtbl = (char *) m32->symtbl;
	m->strings = (char *) m32->strings;
	m->hashsize = (unsigned int) m32->hashsize;
	m->buckets = (symid_t *) m32->buckets;
	m->chains = (symid_t *) m32->chains;
	m->nsyms = (unsigned int) m32->nsyms;
	m->bss_align = (unsigned int) m32->bss_align;
	m->bss_size = (unsigned int) m32->bss_size;
	m->bss = (uintptr_t) m32->bss;
	m->filename = (char *) m32->filename;
	m->head = (struct module_list *) m32->head;
	m->tail = (struct module_list *) m32->tail;
}
#endif

static struct module module_dummy;

/*
 * Returns the value of the given symbol, or 0.  It does not look
 * in the base symbol table, only the tables for the modules.
 * If the symbol appears in multiple modules, the first one found
 * wins unless a preference is specified via "<mod_id>::curmod".
 * "bkpt_curmod" is used for deferred breakpoints, only look for
 * the symbol in that module.
 */
Elf32_Addr
dbg_kobj_getsymvalue(char *name, int any_mod)
{
	symid_t idx;
	u_int symsize;
	Elf32_Sym *symp;
	Elf32_Addr retval = 0;
#ifdef __sparcv9
	struct modctl32 *modp32;
	struct module32 *mp32;
	struct modctl *modp;
	struct module *mp;
#else
	struct modctl *modp;
	struct module *mp;
#endif

	if (*mod_mix_changed_kaddr != current_module_mix)
		kobj_init();

	if (kadb_curmod == -1 || (kadb_curmod == 0 && any_mod == 0))
		return (0);

#ifdef __sparcv9
	modp32 = modules_kaddr32;
	mp = (struct module *) &module_dummy;
	while ((modp32 = (struct modctl32 *)(modp32->mod_next)) != modules_kaddr32) {
		mp32 = (struct module32 *) modp32->mod_mp;
		if (mp32 == NULL) continue;
		m32tom(mp32, mp);	
		if (mp->symtbl == NULL) continue;
		symsize = mp->symhdr->sh_entsize;
		for (idx = mp->buckets[kobj_hash_name(name) % mp->hashsize];
		    idx != 0; idx = mp->chains[idx]) {
			symp = (Elf32_Sym *)(mp->symtbl + idx * symsize);
			if (strcmp(name, mp->strings + symp->st_name) == 0 &&
			    ELF32_ST_TYPE(symp->st_info) != STT_FILE &&
			    symp->st_shndx != SHN_UNDEF &&
			    symp->st_shndx != SHN_COMMON) {
				/*
				 * deferred bkpts - only look for symbol
				 * in the module specified by bkpt_curmod.
				 */
				if (bkpt_curmod != -1) {
					if (bkpt_curmod == modp32->mod_id) {
						retval = symp->st_value;
						break;
					} else {
						continue;
					}
				}
				if (kadb_curmod == modp32->mod_id)
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
#else
	modp = modules_kaddr;
	while ((modp = modp->mod_next) != modules_kaddr) {
		mp = modp->mod_mp;
		if (mp == NULL || mp->symtbl == NULL)
			continue;
		symsize = mp->symhdr->sh_entsize;
		for (idx = mp->buckets[kobj_hash_name(name) % mp->hashsize];
		    idx != 0; idx = mp->chains[idx]) {
			symp = (Elf32_Sym *)(mp->symtbl + idx * symsize);
			if (strcmp(name, mp->strings + symp->st_name) == 0 &&
			    ELF32_ST_TYPE(symp->st_info) != STT_FILE &&
			    symp->st_shndx != SHN_UNDEF &&
			    symp->st_shndx != SHN_COMMON) {
				/*
				 * deferred bkpts - only look for symbol
				 * in the module specified by bkpt_curmod.
				 */
				if (bkpt_curmod != -1) {
					if (bkpt_curmod == modp->mod_id) {
						retval = symp->st_value;
						break;
					} else {
						continue;
					}
				}
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
#endif
	return (retval);
}

/*
 * Converts value to a symbol name, or NULL.  The difference between
 * value and the real value of the symbol is stored in *offset.  The
 * returned value is a static buffer which is overwritten with each call.
 */
char *
dbg_kobj_getsymname(Elf32_Addr value, u_int *offset)
{
	u_int symsize;
	Elf32_Sym *symp, *bestsymp, *lastsymp;
	Elf32_Addr bestval, nextval;
#ifdef __sparcv9
	struct modctl32 *modp32;
	struct module32 *mp32;
	struct modctl *modp;
	struct module *mp;
#else
	struct modctl *modp;
	struct module *mp;
#endif
	symcache_t *scp;

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
#ifdef __sparcv9
	modp32 = modules_kaddr32;
	mp = (struct module *) &module_dummy;
#else
	modp = modules_kaddr;
#endif
	do {
#ifdef __sparcv9
		if ((modp32->mod_mp) == NULL)
			 continue;
		m32tom((struct module32 *) modp32->mod_mp, mp);
#else
		if ((mp = modp->mod_mp) == NULL)
			continue;
#endif
		if (value - (Elf32_Addr)mp->text < mp->text_size) {
			bestval = (Elf32_Addr)mp->text;
			nextval = (Elf32_Addr)mp->text + mp->text_size;
			break;
		}
		if (value - (Elf32_Addr)mp->data < mp->data_size) {
			bestval = (Elf32_Addr)mp->data;
			nextval = (Elf32_Addr)mp->data + mp->data_size;
			break;
		}
		if (value - (Elf32_Addr)mp->bss < mp->bss_size) {
			bestval = (Elf32_Addr)mp->bss;
			nextval = (Elf32_Addr)mp->bss + mp->bss_size;
			break;
		}
#ifdef __sparcv9
	} while ((modp32 = (struct modctl32 *)(modp32->mod_next)) != modules_kaddr32);
#else
	} while ((modp = modp->mod_next) != modules_kaddr);
#endif

	if (bestval == 0)
		return (0);

	symsize = mp->symhdr->sh_entsize;
	bestsymp = NULL;
	lastsymp = (Elf32_Sym *)(mp->symtbl + mp->nsyms * symsize);

	for (symp = (Elf32_Sym *)mp->symtbl; symp < lastsymp;
	    symp = (Elf32_Sym *)((char *)symp + symsize)) {
		Elf32_Addr curval = symp->st_value;
		if (curval >= bestval && curval < nextval) {
			u_char curinfo = symp->st_info;
			if (ELF32_ST_BIND(curinfo) == STB_GLOBAL ||
			    (ELF32_ST_BIND(curinfo) == STB_LOCAL &&
			    (ELF32_ST_TYPE(curinfo) == STT_OBJECT ||
			    ELF32_ST_TYPE(curinfo) == STT_FUNC))) {
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
#ifdef __sparcv9
	free(mp);
#endif
	return (scp->sc_name);
}

/*
 * For a given loadable module name, find the module id number.
 * Return the number if found, -1 if not found.
 *
 * Note: if the module has been unloaded, the module id will not
 * be returned, -1 will be returned.
 */
int
find_mod_id(char *modname)
{
#ifdef	__sparcv9
	struct modctl32 *modp32;
#endif
	struct modctl *modp;

	/*
	 * update kadb's symbol table
	 */
	if (*mod_mix_changed_kaddr != current_module_mix)
		kobj_init();


#ifdef __sparcv9
	if (elf64mode){
#endif
		modp = modules_kaddr;
		while ((modp = modp->mod_next) != modules_kaddr) {
			if (!(strcmp(modp->mod_modname, modname))) {
				if (modp->mod_mp != NULL)
					return (modp->mod_id);
				else
					return (-1);
                        }
		}
#ifdef __sparcv9
	}else {
		modp32 = modules_kaddr32;
		while ((modp32 = (struct modctl32 *)(modp32->mod_next)) != modules_kaddr32) {
			if (!(strcmp((char *) modp32->mod_modname, modname))){
				if (modp32->mod_mp != NULL)
					return (modp32->mod_id);
				else
					return (-1);
			}
		}
	}
#endif

	return (-1);
}
#endif /* KADB */

#ifndef KADB
void *
tmp_malloc(size)
size_t  size;
{
	addr_t  *p;
	size_t  new_size;

	/*  No space to record pointers - just do malloc and return */
	if (cant_record_allocs)
	    return (malloc(size));

	/*  If this is the first call to `malloc,' create a bucket */
	if (address_list == NULL) {
	    if ((address_list = (addr_t *) malloc(BUCKET_SZ)) == NULL) {
		cant_record_allocs = 1;
		return (malloc(size));
	    }
	} else if ((list_ndx%NELEM_PER_BUCKET) == 0) {   /* get a new bucket */
	    new_size = ((list_ndx/NELEM_PER_BUCKET) + 1) * BUCKET_SZ;
	    if ((p = (addr_t *) realloc(address_list, new_size)) == NULL) {
		cant_record_allocs = 1;
		return (malloc(size));
	    } else
		address_list = p;
	}

	if ((address_list[list_ndx] = (addr_t) malloc(size)) != NULL) {
	    list_ndx++;
	    return ((void *) address_list[list_ndx-1]);
	} else
	    return (NULL);
}

void
free_tmpmallocs()
{
	if (list_ndx) {
	    while (--list_ndx)
		free((void *)address_list[list_ndx]);
	    free((void *)address_list[list_ndx]);    /* free '0th' element */
	}

	if (address_list) {
	    free((void *)address_list);
	    address_list = NULL;
	}

	list_ndx = 0;
	cant_record_allocs = 0;
}
#endif

/*
 * Code to support demangling of C++ symbols.  This code was also
 * developed and tested with kadb linked with a static version
 * of libdemangle, but issues remain and this is currently
 * only for adb.
 */

#ifndef KADB

/*
 * Bits corresponding to the various parts of a demangled
 * symbol that will be displayed.
 */
#define	DM_QUAL		0	/* static/const/volatile qualifier */
#define	DM_SCOPE	1	/* func scope specificiers ("foo::") */
#define	DM_FUNCARG	2	/* function arguments */
#define	DM_MANGLED	3	/* mangled name */
#define	DM_NUM		4	/* must be last, for sizing array */

#define	bitval(i)	(1 << (i))
#define	DM_ALL		(bitval(DM_NUM) - 1)

#define	IS_DM_QUAL(mask)	((mask) & bitval(DM_QUAL))
#define	IS_DM_SCOPE(mask)	((mask) & bitval(DM_SCOPE))
#define	IS_DM_FUNCARG(mask)	((mask) & bitval(DM_FUNCARG))
#define	IS_DM_MANGLED(mask)	((mask) & bitval(DM_MANGLED))

/*
 * The initial demangle mask is set to display just function scope
 * specifiers.  This was a guess as to the most typical value of
 * the demangle mask.
 */
static int demangle_mask = bitval(DM_SCOPE);

/*
 * strings displayed by $g to describe the part of the display
 * each bit corresponds to
 */
static char *mask_descr[] = {
	"static/const/volatile member func qualifiers displayed", /* DM_QUAL */
	"scope resolution specifiers displayed",	/* DM_SCOPE */
	"function arguments displayed",			/* DM_FUNCARG */
	"mangled name displayed",			/* DM_MANGLED */
};

#include <dlfcn.h>
#include <demangle.h>

static char *libname = "libdemangle.so.1";
static int check_demangle_func = 1;

typedef int (*cpldem_t)(const char *, char *, size_t);
static cpldem_t demangle_func = NULL;
static int demangling_enabled = 0;

/*
 * Set the demangling function pointer.  For adb we dlopen the
 * demangling library and look up cplus_demangle().  For now for
 * kadb demangling is not enabled, but someday it might be and
 * demangle_func will be set to a version of cplus_demangle linked
 * into kadb.
 */
static void
set_demangle_func()
{
	void *handle, *funcp;

	handle = dlopen(libname, RTLD_LAZY | RTLD_LOCAL);
	if (handle != NULL) {
		funcp = dlsym(handle, "cplus_demangle");
		if (funcp != NULL)
			demangle_func = (cpldem_t)funcp;
	}

	check_demangle_func = 0;
}

/*
 * Some strings and their lengths that we may want to strip
 * out of the full demangled name.
 */
#define	STPREF	"static "
#define	STPLEN	(sizeof (STPREF) - 1)
#define	CONSUF	" const"
#define	CONLEN	(sizeof (CONSUF) - 1)
#define	VOLSUF	" volatile"
#define	VOLLEN	(sizeof (VOLSUF) - 1)

/*
 * format_demangled takes a full demangled name and applies the
 * demangle mask to it.  This can mean stripping parts of the full
 * demangled name or adding to it.  The size of the out buffer
 * must be at least as large as the size of the demangled name.
 */
static void
format_demangled(const char *mangled, char *demangled, char *out, size_t size)
{
	int len;
	char *end;
	int done;
	char *lparen, *rparen;
	char *lcol, *fspc;
	size_t space, needed;

	len = strlen(demangled);
	end = demangled + len;

	/*
	 * If the user doesn't want to see qualifiers, check for "static "
	 * at the front or " const" or " volatile" and the end and strip.
	 */
	if (!IS_DM_QUAL(demangle_mask)) {
		if (strncmp(demangled, STPREF, STPLEN) == 0)	/* static */
			demangled += STPLEN;
		do {
			done = 1;
			if (len > CONLEN &&			/* const */
			    strncmp(end - CONLEN, CONSUF, CONLEN) == 0) {
				end -= CONLEN;
				len -= CONLEN;
				*end = '\0';
				done = 0;
			}
			if (len > VOLLEN &&			/* volatile */
			    strncmp(end - VOLLEN, VOLSUF, VOLLEN) == 0) {
				end -= VOLLEN;
				len -= VOLLEN;
				*end = '\0';
				done = 0;
			}
		} while (!done);
	}

	/*
	 * If the user doesn't want function arguments displayed, strip
	 * those.
	 */
	lparen = strchr(demangled, '(');
	rparen = strrchr(demangled, ')');

	if (!IS_DM_FUNCARG(demangle_mask) && lparen != NULL && rparen != NULL) {
		rparen++;
		strcpy(lparen, rparen);
		lparen = NULL;
	}

	/*
	 * Strip function scope specifiers if desired.
	 */
	if (!IS_DM_SCOPE(demangle_mask)) {
		if (lparen != NULL)
			*lparen = '\0';
		lcol = strrchr(demangled, ':');
		fspc = strchr(demangled, ' ');
		if (lparen != NULL)
			*lparen = '(';
		if (lcol != NULL) {
			lcol++;
			if (fspc == NULL || fspc > lcol)
				fspc = demangled;
			else
				fspc++;
			strcpy(fspc, lcol);
		}
	}

	strcpy(out, demangled);

	/*
	 * Append mangled name in brackets a la "nm -C" if desired
	 * and there is space in the buffer.
	 */
	if (IS_DM_MANGLED(demangle_mask)) {
		len = strlen(out);
		end = out + len;
		space = size - len;
		needed = strlen(mangled) + 3;	/* two brackets and the nul */
		if (needed <= space || space > 3) {
			*end++ = '[';
			if (needed <= space) {
				strcpy(end, mangled);
				end += (needed - 3);
			} else {
				/* copy what we can */
				strncpy(end, mangled, space - 3);
				end += (space - 3);
			}
			*end++ = ']';
			*end = '\0';
		}
	}
}

#endif	/* !KADB */

/*
 * We get here when the user enters $G.  It enables demangling
 * if it is disabled (the initial setting) and disables it if
 * enabled.  If kadb or adb but can't find the demangling function,
 * an apology is issued.
 */
void
toggle_demangling()
{
#ifdef KADB
	printf("demangling function not available\n");
#else
	if (check_demangle_func)
		set_demangle_func();

	if (demangle_func == NULL) {
		printf("%s library not installed, demangling function not "
		    "available\n", libname);
		return;
	}

	demangling_enabled = !demangling_enabled;

	if (demangling_enabled)
		printf("C++ symbol demangling enabled\n");
	else
		printf("C++ symbol demangling disabled\n");
#endif	/* !KADB */
}

/*
 * Display the current demangle mask setting in a format that
 * is hopefully understandable.  This is called directly when
 * the user enters $g without a value, or indirectly from
 * set_demangle_mask() when $g is given a value.
 */
void
disp_demangle_mask()
{
#ifdef KADB
	printf("demangling function not available\n");
#else
	int i;

	printf("demangle mask = 0x%x\n", demangle_mask);
	printf("bit\tstatus\tdescription\n");
	printf("---\t------\t-----------\n");
	for (i = 0; i < DM_NUM; i++) {
		printf("0x%x\t%s\t%s\n", bitval(i),
		    (bitval(i) & demangle_mask) ? "on" : "off", mask_descr[i]);
	}
	if (!demangling_enabled)
		printf("symbol demangling disabled (enable with $G)\n");
#endif	/* !KADB */
}

/*
 * Set the demangle mask.  Called when user enters $g with a mask
 * value.
 */
void
set_demangle_mask(int mask)
{
#ifdef KADB
	printf("demangling function not available\n");
#else
	if (check_demangle_func)
		set_demangle_func();

	if (demangle_func == NULL) {
		printf("%s library not installed, demangling function not "
		    "available\n", libname);
		return;
	}

	demangle_mask = mask & DM_ALL;
	disp_demangle_mask();
#endif	/* !KADB */
}

/*
 * demangled is called to return the demangled version of a symbol
 * if it is a C++ symbol and demangling is enabled, or the raw
 * symbol otherwise.  Demangling is done by the library routine
 * cplus_demangle() and then modified according to the demangle
 * mask.  The result is saved and only regenerated when the demangle
 * mask changes.
 */
char *
demangled(struct asym *sym)
{
#ifdef KADB
	return (sym->s_name);
#else
	/* make buffers static for kadb if enabled to save stack space */
	char full[MAXSYMSIZE];
	char masked[MAXSYMSIZE * 2];
	int error;

	if (!demangling_enabled)
		return (sym->s_name);

	if (sym->s_demname != NULL) {
		if (sym->s_demname == sym->s_name ||	/* non C++ symbol */
		    sym->s_demmask == demangle_mask)	/* still valid dem'd */
			return (sym->s_demname);
		else
			free(sym->s_demname);
	}

	/*
	 * The cplus_demangle(3) man page isn't very clear about
	 * return values (see 4081605).  When a non-C++ symbol is
	 * passed, cplus_demangle() returns 0 and the output is a
	 * copy of the input, which is why we have to do the strcmp
	 * below.
	 */
	error = (*demangle_func)(sym->s_name, full, sizeof (full));
	if (error || strcmp(sym->s_name, full) == 0) {
		/* couldn't demangle or isn't a C++ symbol */
		sym->s_demname = sym->s_name;
	} else {
		format_demangled(sym->s_name, full, masked, sizeof (masked));
		sym->s_demname = malloc(strlen(masked) + 1);
		if (sym->s_demname == NULL)
			outofmem();
		strcpy(sym->s_demname, masked);
		sym->s_demmask = demangle_mask;
	}

	return (sym->s_demname);
#endif	/* !KADB */
}
