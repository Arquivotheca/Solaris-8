/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gelf.c	1.17	99/02/16 SMI"

#include <string.h>
#include "gelf.h"
#include "decl.h"
#include "msg.h"


/*
 * Find elf or it's class from a pointer to an Elf_Data struct.
 * Warning:  this ASSumes that the Elf_Data is part of a libelf
 * Dnode structure, which is expected to be true for any Elf_Data
 * passed into libelf *except* for the xlatetof() and xlatetom()
 * functions.
 */
#define	EDATA_CLASS(edata) \
	(((Dnode *)(edata))->db_scn->s_elf->ed_class)

#define	EDATA_ELF(edata) \
	(((Dnode *)(edata))->db_scn->s_elf)

#define	EDATA_SCN(edata) \
	(((Dnode *)(edata))->db_scn)

#define	EDATA_READLOCKS(edata) \
	READLOCKS(EDATA_ELF((edata)), EDATA_SCN((edata)))

#define	EDATA_READUNLOCKS(edata) \
	READUNLOCKS(EDATA_ELF((edata)), EDATA_SCN((edata)))


size_t
gelf_fsize(Elf * elf, Elf_Type type, size_t count, unsigned ver)
{
	int class;

	if (elf == NULL)
		return (NULL);

	class = gelf_getclass(elf);
	if (class == ELFCLASS32)
		return (elf32_fsize(type, count, ver));
	else if (class == ELFCLASS64)
		return (elf64_fsize(type, count, ver));

	_elf_seterr(EREQ_CLASS, 0);
	return (0);
}


int
gelf_getclass(Elf * elf)
{
	if (elf == NULL)
		return (NULL);

	/*
	 * Don't rely on the idents, a new ehdr doesn't have it!
	 */
	return (elf->ed_class);
}


GElf_Ehdr *
gelf_getehdr(Elf * elf, GElf_Ehdr * dst)
{
	int class;

	if (elf == NULL)
		return (NULL);

	class = gelf_getclass(elf);
	if (class == ELFCLASS32) {
		Elf32_Ehdr * e		= elf32_getehdr(elf);

		if (e == NULL)
			return (NULL);

		ELFRLOCK(elf);
		(void) memcpy(dst->e_ident, e->e_ident, EI_NIDENT);
		dst->e_type		= e->e_type;
		dst->e_machine		= e->e_machine;
		dst->e_version		= e->e_version;
		dst->e_entry		= (Elf64_Addr)e->e_entry;
		dst->e_phoff		= (Elf64_Off)e->e_phoff;
		dst->e_shoff		= (Elf64_Off)e->e_shoff;
		dst->e_flags		= e->e_flags;
		dst->e_ehsize		= e->e_ehsize;
		dst->e_phentsize	= e->e_phentsize;
		dst->e_phnum		= e->e_phnum;
		dst->e_shentsize	= e->e_shentsize;
		dst->e_shnum		= e->e_shnum;
		dst->e_shstrndx		= e->e_shstrndx;
		ELFUNLOCK(elf);

		return (dst);
	} else if (class == ELFCLASS64) {
		Elf64_Ehdr * e		= elf64_getehdr(elf);

		if (e == NULL)
			return (NULL);

		ELFRLOCK(elf);
		*dst			= *e;
		ELFUNLOCK(elf);

		return (dst);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}


int
gelf_update_ehdr(Elf * elf, GElf_Ehdr * src)
{
	int class;

	if (elf == NULL)
		return (NULL);

	/*
	 * In case elf isn't cooked.
	 */
	class = gelf_getclass(elf);
	if (class == ELFCLASSNONE)
		class = src->e_ident[EI_CLASS];


	if (class == ELFCLASS32) {
		Elf32_Ehdr * d	= elf32_getehdr(elf);

		if (d == NULL)
			return (0);

		ELFWLOCK(elf);
		(void) memcpy(d->e_ident, src->e_ident, EI_NIDENT);
		d->e_type	= src->e_type;
		d->e_machine	= src->e_machine;
		d->e_version	= src->e_version;
		/* LINTED */
		d->e_entry	= (Elf32_Addr)src->e_entry;
		/* LINTED */
		d->e_phoff	= (Elf32_Off)src->e_phoff;
		/* LINTED */
		d->e_shoff	= (Elf32_Off)src->e_shoff;
		/* could memcpy the rest of these... */
		d->e_flags	= src->e_flags;
		d->e_ehsize	= src->e_ehsize;
		d->e_phentsize	= src->e_phentsize;
		d->e_phnum	= src->e_phnum;
		d->e_shentsize	= src->e_shentsize;
		d->e_shnum	= src->e_shnum;
		d->e_shstrndx	= src->e_shstrndx;
		ELFUNLOCK(elf);

		return (1);
	} else if (class == ELFCLASS64) {
		Elf64_Ehdr * d	= elf64_getehdr(elf);

		if (d == NULL)
			return (0);

		ELFWLOCK(elf);
		*d		= *(Elf64_Ehdr *)src;
		ELFUNLOCK(elf);

		return (1);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (0);
}


unsigned long
gelf_newehdr(Elf * elf, int class)
{
	if (elf == NULL)
		return (NULL);

	if (class == ELFCLASS32)
		return ((unsigned long)elf32_newehdr(elf));
	else if (class == ELFCLASS64)
		return ((unsigned long)elf64_newehdr(elf));

	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}


GElf_Phdr *
gelf_getphdr(Elf * elf, int ndx, GElf_Phdr * dst)
{
	int class;

	if (elf == NULL)
		return (NULL);

	class = gelf_getclass(elf);
	if (class == ELFCLASS32) {
		Elf32_Phdr *p	= &((Elf32_Phdr *)elf32_getphdr(elf))[ndx];

		ELFRLOCK(elf);
		dst->p_type	= p->p_type;
		dst->p_flags	= p->p_flags;
		dst->p_offset	= (Elf64_Off)p->p_offset;
		dst->p_vaddr	= (Elf64_Addr)p->p_vaddr;
		dst->p_paddr	= (Elf64_Addr)p->p_paddr;
		dst->p_filesz	= (Elf64_Xword)p->p_filesz;
		dst->p_memsz	= (Elf64_Xword)p->p_memsz;
		dst->p_align	= (Elf64_Xword)p->p_align;
		ELFUNLOCK(elf);

		return (dst);
	} else if (class == ELFCLASS64) {
		Elf64_Phdr *phdrs = elf64_getphdr(elf);

		ELFRLOCK(elf);
		*dst = ((GElf_Phdr *)phdrs)[ndx];
		ELFUNLOCK(elf);

		return (dst);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}


int
gelf_update_phdr(Elf * elf, int ndx, GElf_Phdr * src)
{
	int class;

	if (elf == NULL)
		return (NULL);

	class = gelf_getclass(elf);
	if (class == ELFCLASS32) {
		Elf32_Phdr *dst	= &((Elf32_Phdr *)elf32_getphdr(elf))[ndx];

		ELFWLOCK(elf);
		dst->p_type	= src->p_type;
		dst->p_flags	= src->p_flags;
		/* LINTED */
		dst->p_offset	= (Elf32_Off)src->p_offset;
		/* LINTED */
		dst->p_vaddr	= (Elf32_Addr)src->p_vaddr;
		/* LINTED */
		dst->p_paddr	= (Elf32_Addr)src->p_paddr;
		/* LINTED */
		dst->p_filesz	= (Elf32_Word)src->p_filesz;
		/* LINTED */
		dst->p_memsz	= (Elf32_Word)src->p_memsz;
		/* LINTED */
		dst->p_align	= (Elf32_Word)src->p_align;
		ELFUNLOCK(elf);

		return (1);
	} else if (class == ELFCLASS64) {
		Elf64_Phdr *dst = elf64_getphdr(elf);

		ELFWLOCK(elf);
		dst[ndx] = *(GElf_Phdr *)src;
		ELFUNLOCK(elf);

		return (1);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (0);
}


unsigned long
gelf_newphdr(Elf * elf, size_t phnum)
{
	int class;

	if (elf == NULL)
		return (NULL);

	class = gelf_getclass(elf);
	if (class == ELFCLASS32)
		return ((unsigned long)elf32_newphdr(elf, phnum));
	else if (class == ELFCLASS64)
		return ((unsigned long)elf64_newphdr(elf, phnum));

	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}


GElf_Shdr *
gelf_getshdr(Elf_Scn * scn,  GElf_Shdr * dst)
{
	if (scn == NULL)
		return (NULL);

	if (scn->s_elf->ed_class == ELFCLASS32) {
		Elf32_Shdr *s		= elf32_getshdr(scn);

		if (s == NULL)
			return (NULL);

		READLOCKS(scn->s_elf, scn);
		dst->sh_name		= s->sh_name;
		dst->sh_type		= s->sh_type;
		dst->sh_flags		= (Elf64_Xword)s->sh_flags;
		dst->sh_addr		= (Elf64_Addr)s->sh_addr;
		dst->sh_offset		= (Elf64_Off)s->sh_offset;
		dst->sh_size		= (Elf64_Xword)s->sh_size;
		dst->sh_link		= s->sh_link;
		dst->sh_info		= s->sh_info;
		dst->sh_addralign	= (Elf64_Xword)s->sh_addralign;
		dst->sh_entsize		= (Elf64_Xword)s->sh_entsize;
		READUNLOCKS(scn->s_elf, scn);

		return (dst);
	} else if (scn->s_elf->ed_class == ELFCLASS64) {
		Elf64_Shdr *s		= elf64_getshdr(scn);

		if (s == NULL)
			return (NULL);

		READLOCKS(scn->s_elf, scn);
		*dst			= *(Elf64_Shdr *)s;
		READUNLOCKS(scn->s_elf, scn);

		return (dst);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}


int
gelf_update_shdr(Elf_Scn * scn, GElf_Shdr * src)
{
	if (scn == NULL)
		return (NULL);

	if (scn->s_elf->ed_class == ELFCLASS32) {
		Elf32_Shdr * dst	= elf32_getshdr(scn);

		if (dst == NULL)
			return (NULL);

		ELFWLOCK(scn->s_elf);
		dst->sh_name		= src->sh_name;
		dst->sh_type		= src->sh_type;
		/* LINTED */
		dst->sh_flags		= (Elf32_Word)src->sh_flags;
		/* LINTED */
		dst->sh_addr		= (Elf32_Addr)src->sh_addr;
		/* LINTED */
		dst->sh_offset		= (Elf32_Off) src->sh_offset;
		/* LINTED */
		dst->sh_size		= (Elf32_Word)src->sh_size;
		dst->sh_link		= src->sh_link;
		dst->sh_info		= src->sh_info;
		/* LINTED */
		dst->sh_addralign	= (Elf32_Word)src->sh_addralign;
		/* LINTED */
		dst->sh_entsize		= (Elf32_Word)src->sh_entsize;

		ELFUNLOCK(scn->s_elf);
		return (1);
	} else if (scn->s_elf->ed_class == ELFCLASS64) {
		Elf64_Shdr * dst	= elf64_getshdr(scn);

		if (dst == NULL)
			return (NULL);

		ELFWLOCK(scn->s_elf);
		*dst			= *(Elf64_Shdr *)src;
		ELFUNLOCK(scn->s_elf);
		return (1);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (0);
}


/*
 * gelf_xlatetof/gelf_xlatetom use 'elf' to find the class
 * because these are the odd case where the Elf_Data structs
 * might not have been allocated by libelf (and therefore
 * don't have Dnode's associated with them).
 */
Elf_Data *
gelf_xlatetof(Elf * elf, Elf_Data * dst, const Elf_Data * src, unsigned encode)
{
	int class;

	if ((elf == NULL) || (dst == NULL) || (src == NULL))
		return (NULL);

	class = gelf_getclass(elf);
	if (class == ELFCLASS32)
		return (elf32_xlatetof(dst, src, encode));
	else if (class == ELFCLASS64)
		return (elf64_xlatetof(dst, src, encode));

	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}


Elf_Data *
gelf_xlatetom(Elf * elf, Elf_Data * dst, const Elf_Data * src, unsigned encode)
{
	int class;

	if ((elf == NULL) || (dst == NULL) || (src == NULL))
		return (NULL);

	class = gelf_getclass(elf);
	if (class == ELFCLASS32)
		return (elf32_xlatetom(dst, src, encode));
	else if (class == ELFCLASS64)
		return (elf64_xlatetom(dst, src, encode));

	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}


GElf_Sym *
gelf_getsym(Elf_Data * data, int ndx, GElf_Sym * dst)
{
	int class;

	if (data == NULL)
		return (NULL);

	class = EDATA_CLASS(data);
	if (class == ELFCLASS32) {
		Elf32_Sym * s;

		EDATA_READLOCKS(data);
		s		= &(((Elf32_Sym *)data->d_buf)[ndx]);
		dst->st_name	= s->st_name;
		dst->st_value	= (Elf64_Addr)s->st_value;
		dst->st_size	= (Elf64_Xword)s->st_size;
		dst->st_info	= ELF64_ST_INFO(ELF32_ST_BIND(s->st_info),
					ELF32_ST_TYPE(s->st_info));
		dst->st_other	= s->st_other;
		dst->st_shndx	= s->st_shndx;
		EDATA_READUNLOCKS(data);

		return (dst);
	} else if (class == ELFCLASS64) {
		EDATA_READLOCKS(data);
		*dst = ((GElf_Sym *)data->d_buf)[ndx];
		EDATA_READUNLOCKS(data);

		return (dst);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}


int
gelf_update_sym(Elf_Data * dst, int ndx, GElf_Sym * src)
{
	int class;

	if (dst == NULL)
		return (NULL);

	class = EDATA_CLASS(dst);
	if (class == ELFCLASS32) {
		Elf32_Sym * d;

		ELFWLOCK(EDATA_ELF(dst));
		d		= &(((Elf32_Sym *)dst->d_buf)[ndx]);
		d->st_name	= src->st_name;
		/* LINTED */
		d->st_value	= (Elf32_Addr)src->st_value;
		/* LINTED */
		d->st_size	= (Elf32_Word)src->st_size;
		d->st_info	= ELF32_ST_INFO(ELF64_ST_BIND(src->st_info),
					ELF64_ST_TYPE(src->st_info));
		d->st_other	= src->st_other;
		d->st_shndx	= src->st_shndx;
		ELFUNLOCK(EDATA_ELF(dst));

		return (1);
	} else if (class == ELFCLASS64) {
		ELFWLOCK(EDATA_ELF(dst));
		((Elf64_Sym *)dst->d_buf)[ndx] = *((Elf64_Sym *)src);
		ELFUNLOCK(EDATA_ELF(dst));

		return (1);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}


GElf_Syminfo *
gelf_getsyminfo(Elf_Data * data, int ndx, GElf_Syminfo * dst)
{
	int class;

	if (data == NULL)
		return (NULL);

	class = EDATA_CLASS(data);
	if (class == ELFCLASS32) {
		Elf32_Syminfo *	si;

		EDATA_READLOCKS(data);
		si		= &(((Elf32_Syminfo *)data->d_buf)[ndx]);
		dst->si_boundto = si->si_boundto;
		dst->si_flags	= si->si_flags;
		EDATA_READUNLOCKS(data);

		return (dst);
	} else if (class == ELFCLASS64) {
		EDATA_READLOCKS(data);
		*dst		= ((GElf_Syminfo *)data->d_buf)[ndx];
		EDATA_READUNLOCKS(data);

		return (dst);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}

int
gelf_update_syminfo(Elf_Data * dst, int ndx, GElf_Syminfo * src)
{
	int class;

	if (dst == NULL)
		return (NULL);

	class = EDATA_CLASS(dst);
	if (class == ELFCLASS32) {
		Elf32_Syminfo * d	= &(((Elf32_Syminfo *)dst->d_buf)[ndx]);

		ELFWLOCK(EDATA_ELF(dst));
		d->si_boundto		= src->si_boundto;
		d->si_flags		= src->si_flags;
		ELFUNLOCK(EDATA_ELF(dst));

		return (1);
	} else if (class == ELFCLASS64) {
		ELFWLOCK(EDATA_ELF(dst));
		((Elf64_Syminfo *)dst->d_buf)[ndx] = *((Elf64_Syminfo *)src);
		ELFUNLOCK(EDATA_ELF(dst));

		return (1);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}

GElf_Dyn *
gelf_getdyn(Elf_Data * src, int ndx, GElf_Dyn * dst)
{
	int class;

	if (src == NULL)
		return (NULL);

	class = EDATA_CLASS(src);
	if (class == ELFCLASS32) {
		Elf32_Dyn * d = &((Elf32_Dyn *)src->d_buf)[ndx];

		EDATA_READLOCKS(src);
		dst->d_tag	= (Elf32_Sword)d->d_tag;
		dst->d_un.d_val	= (Elf32_Word) d->d_un.d_val;
		EDATA_READUNLOCKS(src);

		return (dst);
	} else if (class == ELFCLASS64) {
		EDATA_READLOCKS(src);
		*dst = ((Elf64_Dyn *)src->d_buf)[ndx];
		EDATA_READUNLOCKS(src);

		return (dst);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}


int
gelf_update_dyn(Elf_Data * dst, int ndx, GElf_Dyn * src)
{
	int class;

	if (dst == NULL)
		return (NULL);

	class = EDATA_CLASS(dst);
	if (class == ELFCLASS32) {
		Elf32_Dyn * d = &((Elf32_Dyn *)dst->d_buf)[ndx];

		ELFWLOCK(EDATA_ELF(dst));
		/* LINTED */
		d->d_tag	= (Elf32_Word)src->d_tag;
		/* LINTED */
		d->d_un.d_val	= (Elf32_Word)src->d_un.d_val;
		ELFUNLOCK(EDATA_ELF(dst));

		return (1);
	} else if (class == ELFCLASS64) {
		ELFWLOCK(EDATA_ELF(dst));
		((Elf64_Dyn *)dst->d_buf)[ndx] = *(Elf64_Dyn*)src;
		ELFUNLOCK(EDATA_ELF(dst));

		return (1);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (0);
}


GElf_Move *
gelf_getmove(Elf_Data *  src, int ndx, GElf_Move * dst)
{
	int class;

	if (src == NULL)
		return (NULL);

	class = EDATA_CLASS(src);
	if (class == ELFCLASS32) {
		Elf32_Move * m = &((Elf32_Move *)src->d_buf)[ndx];
		EDATA_READLOCKS(src);
		dst->m_poffset = (Elf64_Word)m->m_poffset;
		dst->m_repeat = (Elf64_Xword)m->m_repeat;
		dst->m_stride = (Elf64_Half)m->m_stride;
		dst->m_value = (Elf64_Xword)m->m_value;
		dst->m_info = ELF64_M_INFO(
			ELF32_M_SYM(m->m_info),
			ELF32_M_SIZE(m->m_info));
		EDATA_READUNLOCKS(src);

		return (dst);
	} else if (class == ELFCLASS64) {
		EDATA_READLOCKS(src);
		*dst = ((Elf64_Move *)src->d_buf)[ndx];
		EDATA_READUNLOCKS(src);

		return (dst);
	}
	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}

int
gelf_update_move(Elf_Data * dest, int ndx, GElf_Move * src)
{
	int class;

	if (dest == NULL)
		return (NULL);

	class = EDATA_CLASS(src);
	if (class == ELFCLASS32) {
		Elf32_Move * m = &((Elf32_Move *)dest->d_buf)[ndx];

		ELFWLOCK(EDATA_ELF(dest));
		m->m_poffset = (Elf32_Word)src->m_poffset;
		m->m_repeat = (Elf32_Half)src->m_repeat;
		m->m_stride = (Elf32_Half)src->m_stride;
		m->m_value = (Elf32_Lword)src->m_value;
		m->m_info = (Elf32_Word)ELF32_M_INFO(
				ELF64_M_SYM(src->m_info),
				ELF64_M_SIZE(src->m_info));
		ELFUNLOCK(EDATA_ELF(dest));

		return (1);
	} else if (class == ELFCLASS64) {
		ELFWLOCK(EDATA_ELF(dest));
		((Elf64_Move *)dest->d_buf)[ndx] = *(Elf64_Move *)src;
		ELFUNLOCK(EDATA_ELF(dest));

		return (1);
	}
	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}


GElf_Rela *
gelf_getrela(Elf_Data * src, int ndx, GElf_Rela * dst)
{
	int class;

	if (src == NULL)
		return (NULL);

	class = EDATA_CLASS(src);
	if (class == ELFCLASS32) {
		Elf32_Rela * r = &((Elf32_Rela *)src->d_buf)[ndx];

		EDATA_READLOCKS(src);
		dst->r_offset	= (GElf_Addr)r->r_offset;
		dst->r_addend	= (GElf_Addr)r->r_addend;

		/*
		 * Elf32 will never have the extra data field that
		 * Elf64's r_info field can have, so ignore it.
		 */
		/* LINTED */
		dst->r_info	= ELF64_R_INFO(
		    ELF32_R_SYM(r->r_info),
		    ELF32_R_TYPE(r->r_info));
		EDATA_READUNLOCKS(src);

		return (dst);
	} else if (class == ELFCLASS64) {
		EDATA_READLOCKS(src);
		*dst = ((Elf64_Rela *)src->d_buf)[ndx];
		EDATA_READUNLOCKS(src);

		return (dst);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}


int
gelf_update_rela(Elf_Data * dst, int ndx, GElf_Rela * src)
{
	int class;

	if (dst == NULL)
		return (NULL);

	class = EDATA_CLASS(dst);
	if (class == ELFCLASS32) {
		Elf32_Rela * r = &((Elf32_Rela *)dst->d_buf)[ndx];

		ELFWLOCK(EDATA_ELF(dst));
		/* LINTED */
		r->r_offset	= (Elf32_Addr) src->r_offset;
		/* LINTED */
		r->r_addend	= (Elf32_Sword)src->r_addend;

		/*
		 * Elf32 will never have the extra data field that
		 * Elf64's r_info field can have, so ignore it.
		 */
		/* LINTED */
		r->r_info	= ELF32_R_INFO(
					ELF64_R_SYM(src->r_info),
					ELF64_R_TYPE(src->r_info));
		ELFUNLOCK(EDATA_ELF(dst));

		return (1);
	} else if (class == ELFCLASS64) {
		ELFWLOCK(EDATA_ELF(dst));
		((Elf64_Rela *)dst->d_buf)[ndx] = *(Elf64_Rela *)src;
		ELFUNLOCK(EDATA_ELF(dst));

		return (1);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (0);
}


GElf_Rel *
gelf_getrel(Elf_Data * src, int ndx, GElf_Rel * dst)
{
	int class;

	if (src == NULL)
		return (NULL);

	class = EDATA_CLASS(src);
	if (class == ELFCLASS32) {
		Elf32_Rel * r = &((Elf32_Rel *)src->d_buf)[ndx];

		EDATA_READLOCKS(src);
		dst->r_offset	= (GElf_Addr)r->r_offset;

		/*
		 * Elf32 will never have the extra data field that
		 * Elf64's r_info field can have, so ignore it.
		 */
		/* LINTED */
		dst->r_info	= ELF64_R_INFO(
					ELF32_R_SYM(r->r_info),
					ELF32_R_TYPE(r->r_info));
		EDATA_READUNLOCKS(src);

		return (dst);
	} else if (class == ELFCLASS64) {
		EDATA_READLOCKS(src);
		*dst = ((Elf64_Rel *)src->d_buf)[ndx];
		EDATA_READUNLOCKS(src);

		return (dst);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (NULL);
}


int
gelf_update_rel(Elf_Data * dst, int ndx, GElf_Rel * src)
{
	int class;

	if (dst == NULL)
		return (NULL);

	class = EDATA_CLASS(dst);
	if (class == ELFCLASS32) {
		Elf32_Rel * r = &((Elf32_Rel *)dst->d_buf)[ndx];

		ELFWLOCK(EDATA_ELF(dst));
		/* LINTED */
		r->r_offset	= (Elf32_Addr) src->r_offset;

		/*
		 * Elf32 will never have the extra data field that
		 * Elf64's r_info field can have, so ignore it.
		 */
		/* LINTED */
		r->r_info	= ELF32_R_INFO(
					ELF64_R_SYM(src->r_info),
					ELF64_R_TYPE(src->r_info));

		ELFUNLOCK(EDATA_ELF(dst));

		return (1);
	} else if (class == ELFCLASS64) {
		ELFWLOCK(EDATA_ELF(dst));
		((Elf64_Rel *)dst->d_buf)[ndx] = *(Elf64_Rel *)src;
		ELFUNLOCK(EDATA_ELF(dst));

		return (1);
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (0);
}

long
gelf_checksum(Elf * elf)
{
	int class = gelf_getclass(elf);

	if (class == ELFCLASS32)
		return (elf32_checksum(elf));
	else if (class == ELFCLASS64)
		return (elf64_checksum(elf));

	_elf_seterr(EREQ_CLASS, 0);
	return (0);
}
