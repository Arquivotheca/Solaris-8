/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ksyms_snapshot.c	1.3	99/12/04 SMI"

#include <sys/ksyms.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>

static const char ksyms_shstrtab[] = "\0.symtab\0.strtab\0.shstrtab\0";

#define	KSHDR_NULL	0
#define	KSHDR_SYMTAB	1
#define	KSHDR_STRTAB	2
#define	KSHDR_SHSTRTAB	3
#define	KSHDR_NUM	4

typedef struct ksyms_header {
	Ehdr	elf_hdr;		/* Elf file header */
	Phdr	text_phdr;		/* text program header */
	Phdr	data_phdr;		/* data program header */
	Shdr	shdr[KSHDR_NUM];	/* section headers */
	char	shstrings[sizeof (ksyms_shstrtab)];	/* shstrtab strings */
} ksyms_header_t;

#define	KW_HEADER	0x1
#define	KW_LOCALS	0x2
#define	KW_GLOBALS	0x4
#define	KW_STRINGS	0x8

typedef struct ksyms_walkinfo {
	void	(*kw_emit)(const void *, void *, size_t);
	char	*kw_target;
	ssize_t	kw_resid;
	int	kw_actions;
	size_t	kw_size[KW_STRINGS + 1];
} ksyms_walkinfo_t;

krwlock_t ksyms_lock;
vmem_t *ksyms_arena;

static void
ksyms_emit(ksyms_walkinfo_t *kwp, void *src, size_t size, int action)
{
	if (kwp->kw_actions & action) {
		if ((kwp->kw_resid -= size) >= 0)
			kwp->kw_emit(src, kwp->kw_target, size);
		kwp->kw_target += size;
	}
	kwp->kw_size[action] += size;
}

/*ARGSUSED*/
static void
ksyms_walk_one(void *arg, void *base, size_t size)
{
	ksyms_walkinfo_t *kwp = arg;
	Shdr *symhdr = base;
	Shdr *strhdr = symhdr + symhdr->sh_link;
	size_t symsize = symhdr->sh_entsize;
	size_t nsyms = symhdr->sh_size / symsize;
	char *strings = (char *)strhdr->sh_addr;
	int i;

	for (i = 1; i < nsyms; i++) {
		Sym *sym = (Sym *)(symhdr->sh_addr + i * symsize);
		Sym tmp = *sym;
		char *name = strings + sym->st_name;
		tmp.st_name = kwp->kw_size[KW_STRINGS];
		tmp.st_shndx = SHN_ABS;
		ksyms_emit(kwp, &tmp, sizeof (Sym),
		    ELF_ST_BIND(sym->st_info) == STB_LOCAL ?
		    KW_LOCALS : KW_GLOBALS);
		ksyms_emit(kwp, name, strlen(name) + 1, KW_STRINGS);
	}
}

static void
ksyms_walk(ksyms_walkinfo_t *kwp, void *target, void *limit,
	void (*emit)(const void *, void *, size_t), void *src, int actions)
{
	Sym tmp;

	bzero(kwp, sizeof (ksyms_walkinfo_t));
	kwp->kw_emit = emit;
	kwp->kw_target = target;
	kwp->kw_resid = (ssize_t)limit - (ssize_t)target;
	kwp->kw_actions = actions;

	ksyms_emit(kwp, src, sizeof (ksyms_header_t), KW_HEADER);
	/*
	 * The first symbol table entry is all zeroes; it's unused
	 * because index 0 marks the end of symbol hash chains.
	 */
	bzero(&tmp, sizeof (Sym));
	ksyms_emit(kwp, &tmp, sizeof (Sym), KW_LOCALS);
	ksyms_emit(kwp, &tmp, 1, KW_STRINGS);
	vmem_walk(ksyms_arena, VMEM_ALLOC, ksyms_walk_one, kwp);
}

size_t
ksyms_snapshot(void (*emit)(const void *, void *, size_t),
	void *buf, size_t len)
{
	ksyms_walkinfo_t kw;
	ksyms_header_t hdr;
	char *limit = (char *)buf + len;
	Shdr *shp;

	rw_enter(&ksyms_lock, RW_READER);

	/*
	 * Compute the size of the header, locals, globals, and strings.
	 */
	ksyms_walk(&kw, NULL, NULL, NULL, NULL,
	    KW_HEADER | KW_LOCALS | KW_GLOBALS | KW_STRINGS);

	/*
	 * Construct the ELF header.
	 */
	bzero(&hdr, sizeof (hdr));

	hdr.elf_hdr = ((struct module *)modules.mod_mp)->hdr;
	hdr.elf_hdr.e_phoff = offsetof(ksyms_header_t, text_phdr);
	hdr.elf_hdr.e_shoff = offsetof(ksyms_header_t, shdr);
	hdr.elf_hdr.e_phnum = 2;
	hdr.elf_hdr.e_shnum = KSHDR_NUM;
	hdr.elf_hdr.e_shstrndx = KSHDR_SHSTRTAB;

	hdr.text_phdr.p_type = PT_LOAD;
	hdr.text_phdr.p_vaddr = (Addr)s_text;
	hdr.text_phdr.p_memsz = (Word)(e_text - s_text);
	hdr.text_phdr.p_flags = PF_R | PF_X;

	hdr.data_phdr.p_type = PT_LOAD;
	hdr.data_phdr.p_vaddr = (Addr)s_data;
	hdr.data_phdr.p_memsz = (Word)(e_data - s_data);
	hdr.data_phdr.p_flags = PF_R | PF_W | PF_X;

	shp = &hdr.shdr[KSHDR_SYMTAB];
	shp->sh_name = 1;	/* ksyms_shstrtab[1] = ".symtab" */
	shp->sh_type = SHT_SYMTAB;
	shp->sh_offset = kw.kw_size[KW_HEADER];
	shp->sh_size = kw.kw_size[KW_LOCALS] + kw.kw_size[KW_GLOBALS];
	shp->sh_link = KSHDR_STRTAB;
	shp->sh_info = kw.kw_size[KW_LOCALS] / sizeof (Sym);
	shp->sh_addralign = sizeof (Addr);
	shp->sh_entsize = sizeof (Sym);

	shp = &hdr.shdr[KSHDR_STRTAB];
	shp->sh_name = 9;	/* ksyms_shstrtab[9] = ".strtab" */
	shp->sh_type = SHT_STRTAB;
	shp->sh_offset = kw.kw_size[KW_HEADER] +
	    kw.kw_size[KW_LOCALS] + kw.kw_size[KW_GLOBALS];
	shp->sh_size = kw.kw_size[KW_STRINGS];
	shp->sh_addralign = 1;

	shp = &hdr.shdr[KSHDR_SHSTRTAB];
	shp->sh_name = 17;	/* ksyms_shstrtab[17] = ".shstrtab" */
	shp->sh_type = SHT_STRTAB;
	shp->sh_offset = offsetof(ksyms_header_t, shstrings);
	shp->sh_size = sizeof (ksyms_shstrtab);
	shp->sh_addralign = 1;

	bcopy(ksyms_shstrtab, hdr.shstrings, sizeof (ksyms_shstrtab));

	/*
	 * Emit the symbol table.
	 */
	ksyms_walk(&kw, buf, limit, emit, &hdr, KW_HEADER);
	ksyms_walk(&kw, kw.kw_target, limit, emit, NULL, KW_LOCALS);
	ksyms_walk(&kw, kw.kw_target, limit, emit, NULL, KW_GLOBALS);
	ksyms_walk(&kw, kw.kw_target, limit, emit, NULL, KW_STRINGS);

	rw_exit(&ksyms_lock);

	return ((size_t)kw.kw_target - (size_t)buf);
}
