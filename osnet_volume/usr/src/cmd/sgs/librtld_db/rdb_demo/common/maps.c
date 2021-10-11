
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)maps.c	1.8	98/08/28 SMI"


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/auxv.h>
#include <libelf.h>
#include <link.h>
#include <sys/param.h>
#include <stdarg.h>

#include "rdb.h"

char *
conv_lmid(Lmid_t ident, char * buf, size_t len)
{
	if (len < 17)
		return ((char *)0);
	if (ident == LM_ID_BASE)
		return (strncpy(buf, "  BASE  ", len));

	if (ident == LM_ID_LDSO)
		return (strncpy(buf, "  LDSO  ", len));

	(void) sprintf(buf, "0x%llx", (unsigned long long)ident);
	return (buf);
}

map_info_t *
str_to_map(struct ps_prochandle * ph, const char * soname)
{
	map_info_t *	mip;

	if (soname == PS_OBJ_LDSO)
		mip = (map_info_t *)&(ph->pp_ldsomap);
	else if (soname == PS_OBJ_EXEC)
		mip = (map_info_t *)&(ph->pp_execmap);
	else {
		for (mip = ph->pp_lmaplist.ml_head; mip; mip = mip->mi_next)
			if (strcmp(soname, mip->mi_name) == 0)
				break;
	}
	return (mip);
}

map_info_t *
addr_to_map(struct ps_prochandle * ph, ulong_t addr)
{
	map_info_t *	mip;
	if (ph->pp_lmaplist.ml_head == 0) {
		/*
		 * To early to have the full Link Map info available
		 * so we use the initial info obtained from procfs
		 */
		if ((addr >= ph->pp_ldsomap.mi_addr) &&
		    (addr <= ph->pp_ldsomap.mi_end))
			return ((map_info_t *)&(ph->pp_ldsomap));

		if ((addr >= ph->pp_execmap.mi_addr) &&
		    (addr <= ph->pp_execmap.mi_end))
			return ((map_info_t *)&(ph->pp_execmap));

		return ((map_info_t *)0);
	}

	for (mip = ph->pp_lmaplist.ml_head; mip; mip = mip->mi_next)
		if ((addr >= mip->mi_addr) &&
		    (addr <= mip->mi_end))
			return (mip);

	return ((map_info_t *)0);
}


retc_t
display_linkmaps(struct ps_prochandle * ph)
{
	map_info_t *	mip;
	if (ph->pp_lmaplist.ml_head == 0) {
		printf("link-maps not yet available\n");
		return (RET_FAILED);
	}
	printf("Link Maps\n");
	printf("---------\n");
	for (mip = ph->pp_lmaplist.ml_head; mip; mip = mip->mi_next) {
		char sbuf[32];
		rd_loadobj_t *	lp = &mip->mi_loadobj;
		printf("link-map: id: %s name: ",
			conv_lmid(lp->rl_lmident, sbuf, 32));
		if (mip->mi_refname)
			printf("%s(%s)\n", mip->mi_name,
				mip->mi_refname);
		else
			printf("%s\n", mip->mi_name);

		printf("       base: 0x%08lx   padd_base: 0x%08lx\n",
			lp->rl_base, lp->rl_padstart);
		printf("  data_base: 0x%08llx\n",
			(unsigned long long)lp->rl_data_base);
		printf("        end: 0x%08lx    padd_end: 0x%08lx\n",
			lp->rl_bend, lp->rl_padend);
	}

	return (RET_OK);
}


retc_t
display_maps(struct ps_prochandle * ph)
{
	prmap_t * 	prmap;
	int		pcnt;
	int		pfd = ph->pp_fd;
	int		i;

	if (ioctl(pfd, PIOCNMAP, &pcnt) != 0) {
		perr("display_map: PIOCNMAP");
	}

	if (pcnt < 1) {
		fprintf(stderr, "dm: PIOCNMAP returned: %d\n", pcnt);
		return (RET_FAILED);
	}

	prmap = (prmap_t *)malloc(sizeof (prmap_t) * (pcnt + 1));
	if (ioctl(pfd, PIOCMAP, prmap) != 0) {
		perror("dm: PIOCMAP");
		free(prmap);
		return (RET_FAILED);
	}

	puts("\nMappings");
	puts("--------");
	if (ph->pp_dmodel == PR_MODEL_LP64)
		puts("ind  addr               size     prot ident name");
	else
		puts("ind  addr       size     prot ident name");
	for (i = 0; i < pcnt; i++) {
		map_info_t *	mip;

		if (ph->pp_dmodel == PR_MODEL_LP64)
			printf("[%2d] %#18lx %#08lx %#04x",
				i, prmap[i].pr_vaddr, prmap[i].pr_size,
				prmap[i].pr_mflags);
		else
			printf("[%2d] 0x%08lx 0x%06lx 0x%02x",
				i, prmap[i].pr_vaddr, prmap[i].pr_size,
				prmap[i].pr_mflags);

		if ((mip = addr_to_map(ph,
		    (ulong_t)(prmap[i].pr_vaddr))) != 0) {
			if (mip->mi_refname) {
				printf(" 0x%02lx  %s(%s)",
					mip->mi_lmident, mip->mi_name,
					mip->mi_refname);
			} else
				printf(" 0x%02lx  %s",
					mip->mi_lmident, mip->mi_name);
		}
		putchar('\n');
	}
	putchar('\n');
	free(prmap);

	return (RET_OK);
}


retc_t
load_map(int fd, map_info_t * mp)
{
	Elf *		elf;
	GElf_Ehdr 	ehdr;
	GElf_Phdr	phdr;
	Elf_Scn *	scn = 0;
	int		cnt;

	if ((elf = elf_begin(fd, ELF_C_READ, 0)) == NULL) {
		fprintf(stderr, "elf_begin(): %s\n", elf_errmsg(-1));
		return (RET_FAILED);
	}

	if (elf_kind(elf) != ELF_K_ELF) {
		printf("non-elf file\n");
		elf_end(elf);
		return (RET_FAILED);
	}

	mp->mi_elf = elf;
	mp->mi_flags = 0;
	mp->mi_mapfd = fd;

	if (gelf_getehdr(mp->mi_elf, &ehdr) == NULL) {
		printf("gelf_getehdr(): %s\n", elf_errmsg(-1));
		elf_end(mp->mi_elf);
		return (RET_FAILED);
	}
	mp->mi_ehdr = ehdr;
	if (ehdr.e_type == ET_EXEC)
		mp->mi_flags |= FLG_MI_EXEC;

	mp->mi_end = 0;
	mp->mi_addr = (ulong_t)0xffffffffffffffff;
	for (cnt = 0; cnt < (int)(ehdr.e_phnum); cnt++) {
		if (gelf_getphdr(mp->mi_elf, cnt, &phdr) == NULL) {
			printf("gelf_getphdr(): %s\n", elf_errmsg(-1));
			elf_end(mp->mi_elf);
			return (RET_FAILED);
		}

		if (phdr.p_type == PT_LOAD) {
			if (mp->mi_end < (ulong_t)(phdr.p_vaddr +
			    phdr.p_memsz))
				mp->mi_end = (ulong_t)(phdr.p_vaddr +
					phdr.p_memsz);
			if (mp->mi_addr > phdr.p_vaddr)
				mp->mi_addr = phdr.p_vaddr;
		}
	}

	mp->mi_pltbase = 0;
	mp->mi_pltsize = 0;
	mp->mi_pltentsz = 0;
	while ((scn = elf_nextscn(mp->mi_elf, scn)) != 0) {
		GElf_Shdr 	shdr;
		Elf_Data *	dp;

		if (gelf_getshdr(scn, &shdr) == 0) {
			printf("gelf_getshdr(): %s\n", elf_errmsg(-1));
			elf_end(mp->mi_elf);
			return (RET_FAILED);
		}

		switch (shdr.sh_type) {
		case SHT_DYNSYM:
			dp = elf_getdata(scn, 0);
			mp->mi_dynsym.st_syms = dp;
			scn = elf_getscn(mp->mi_elf, shdr.sh_link);
			mp->mi_dynsym.st_symn =
				shdr.sh_size / shdr.sh_entsize;
			dp = elf_getdata(scn, 0);
			mp->mi_dynsym.st_strs = (char *)dp->d_buf;
			break;
		case SHT_SYMTAB:
			dp = elf_getdata(scn, 0);
			mp->mi_symtab.st_syms = dp;
			scn = elf_getscn(mp->mi_elf, shdr.sh_link);
			mp->mi_symtab.st_symn =
				shdr.sh_size / shdr.sh_entsize;
			dp = elf_getdata(scn, 0);
			mp->mi_symtab.st_strs = (char *)dp->d_buf;
			break;
		case PLTSECTT:
			if (strcmp(PLTSECT, elf_strptr(mp->mi_elf,
			    ehdr.e_shstrndx, shdr.sh_name)) == 0) {
				mp->mi_pltbase = shdr.sh_addr;
				mp->mi_pltsize = shdr.sh_size;
				/* LINTED */
				mp->mi_pltentsz = (unsigned)shdr.sh_entsize;
			}
			break;
		default:
			/* nothing */
			break;
		}
	}
	return (RET_OK);
}



static int
map_iter(const rd_loadobj_t * lop, void * cd)
{
	struct ps_prochandle *	ph = (struct ps_prochandle *)cd;
	map_info_t *		mip;
	char			buf[MAXPATHLEN];
	int			fd;

	if ((fd = ioctl(ph->pp_fd, PIOCOPENM, &(lop->rl_base))) == -1) {
		perror("mi: PIOCOPENM");
		fprintf(stderr, "mi:bad base address: 0x%lx\n", lop->rl_base);
		return (0);
	}

	if ((mip = (map_info_t *)calloc(1, sizeof (map_info_t))) == NULL) {
		fprintf(stderr, "map_iter: memory error: allocation failed\n");
		close(fd);
		return (0);
	}

	mip->mi_loadobj = *lop;

	if (proc_string_read(ph, lop->rl_nameaddr,
	    buf, MAXPATHLEN) == RET_FAILED) {
		fprintf(stderr,
			"mi: bad object name address passed: 0x%lx\n",
			lop->rl_nameaddr);
		free(mip);
		close(fd);
		return (0);
	}
	mip->mi_name = strdup(buf);


	if (lop->rl_refnameaddr) {
		if (proc_string_read(ph, lop->rl_refnameaddr, buf,
		    MAXPATHLEN) == RET_FAILED) {
			fprintf(stderr,
				"mi1: bad object name address passed: 0x%lx\n",
				lop->rl_refnameaddr);
			free(mip);
			close(fd);
			return (0);
		}
		mip->mi_refname = strdup(buf);
	} else
		mip->mi_refname = 0;

	load_map(fd, mip);
	if ((mip->mi_flags & FLG_MI_EXEC) == 0) {
		mip->mi_end += lop->rl_base;
		mip->mi_addr += lop->rl_base;
	}
	mip->mi_lmident = lop->rl_lmident;
	mip->mi_next = 0;

	if (ph->pp_lmaplist.ml_head == 0) {
		ph->pp_lmaplist.ml_head = ph->pp_lmaplist.ml_tail = mip;
		return (1);
	}

	ph->pp_lmaplist.ml_tail->mi_next = mip;
	ph->pp_lmaplist.ml_tail = mip;

	return (1);
}

void
free_linkmaps(struct ps_prochandle * ph)
{
	map_info_t *	cur, * prev;
	for (cur = ph->pp_lmaplist.ml_head, prev = 0; cur;
	    prev = cur, cur = cur->mi_next) {
		if (prev) {
			elf_end(prev->mi_elf);
			close(prev->mi_mapfd);
			free(prev->mi_name);
			if (prev->mi_refname)
				free(prev->mi_refname);
			free(prev);
		}
	}
	if (prev) {
		elf_end(prev->mi_elf);
		close(prev->mi_mapfd);
		free(prev->mi_name);
		if (prev->mi_refname)
			free(prev->mi_refname);
		free(prev);
	}
	ph->pp_lmaplist.ml_head = ph->pp_lmaplist.ml_tail = 0;
}


retc_t
get_linkmaps(struct ps_prochandle * ph)
{
	free_linkmaps(ph);
	rd_loadobj_iter(ph->pp_rap, map_iter, ph);
	return (RET_OK);
}

retc_t
set_objpad(struct ps_prochandle * ph, size_t padsize)
{
	if (rd_objpad_enable(ph->pp_rap, padsize) != RD_OK) {
		printf("rdb: error setting object padding\n");
		return (RET_FAILED);
	}
	return (RET_OK);
}
