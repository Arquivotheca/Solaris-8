
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
#pragma ident	"@(#)ps.c	1.11	98/08/28 SMI"


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
#include <sys/param.h>
#include <stdarg.h>

#include <proc_service.h>

#include "rdb.h"
#include "disasm.h"


static void
elf32_sym_to_gelf(Elf32_Sym * src, GElf_Sym * dst)
{
	dst->st_name	= src->st_name;
	dst->st_value	= (GElf_Addr)src->st_value;
	dst->st_size	= (GElf_Xword)src->st_size;
	dst->st_info	= GELF_ST_INFO(ELF32_ST_BIND(src->st_info),
				ELF32_ST_TYPE(src->st_info));
	dst->st_other	= src->st_other;
	dst->st_shndx	= src->st_shndx;
}

static void
gelf_sym_to_elf32(GElf_Sym * src, Elf32_Sym * dst)
{
	dst->st_name	= src->st_name;
	/* LINTED */
	dst->st_value	= (Elf32_Addr)src->st_value;
	/* LINTED */
	dst->st_size	= (Elf32_Word)src->st_size;
	dst->st_info	= ELF32_ST_INFO(GELF_ST_BIND(src->st_info),
				GELF_ST_TYPE(src->st_info));
	dst->st_other	= src->st_other;
	dst->st_shndx	= src->st_shndx;
}

retc_t
ps_init(int pfd, struct ps_prochandle * procp)
{
	int		fd;
	rd_notify_t	rd_notify;
	long		pflags;

	procp->pp_fd = pfd;

	if (ps_pdmodel(procp, &procp->pp_dmodel) != PS_OK)
		perr("psi: data model");

#if	!defined(_LP64)
	if (procp->pp_dmodel == PR_MODEL_LP64)
		perr("psi:  run 64-bit rdb to debug a 64-bit process");
#endif

	if ((procp->pp_ldsobase = get_ldbase(pfd, procp->pp_dmodel)) !=
	    (ulong_t)-1) {
		if ((fd = ioctl(pfd, PIOCOPENM, &(procp->pp_ldsobase))) == -1)
			perr("ip: PIOCOPENM");

		load_map(fd, &(procp->pp_ldsomap));
		procp->pp_ldsomap.mi_addr += procp->pp_ldsobase;
		procp->pp_ldsomap.mi_end += procp->pp_ldsobase;
		procp->pp_ldsomap.mi_name = "<procfs: interp>";
		close(fd);
	}
	if ((fd = ioctl(pfd, PIOCOPENM, 0)) == -1)
		perr("ip1: PIOCOPENM");

	load_map(fd, &(procp->pp_execmap));
	procp->pp_execmap.mi_name = "<procfs: exec>";
	close(fd);
	procp->pp_breakpoints = 0;
	procp->pp_flags = FLG_PP_PACT | FLG_PP_PLTSKIP;
	procp->pp_lmaplist.ml_head = 0;
	procp->pp_lmaplist.ml_tail = 0;
	procp->pp_auxvp = 0;
	if ((procp->pp_rap = rd_new(procp)) == 0) {
		fprintf(stderr, "rdb: rtld_db: rd_new() call failed\n");
		exit(1);
	}
	rd_event_enable(procp->pp_rap, 1);

	/*
	 * For those architectures that increment the PC on
	 * a breakpoint fault we enable the PR_BPTADJ adjustments.
	 */
	pflags = PR_BPTADJ;
	if (ioctl(procp->pp_fd, PIOCSET, &pflags) != 0)
		perr("ps_init: PIOCSET(PR_BPTADJ)");


	/*
	 * Set breakpoints for special handshakes between librtld_db.so
	 * and the debugger.  These include:
	 *	PREINIT		- before .init processing.
	 *	POSTINIT	- after .init processing
	 *	DLACTIVITY	- link_maps status has changed
	 */
	if (rd_event_addr(procp->pp_rap, RD_PREINIT, &rd_notify) == RD_OK) {
		if (set_breakpoint(procp, rd_notify.u.bptaddr,
		    FLG_BP_RDPREINIT) != RET_OK)
			fprintf(stderr,
				"psi: failed to set BP for preinit at: 0x%lx\n",
				rd_notify.u.bptaddr);
	} else
		fprintf(stderr, "psi: no event registered for preinit\n");

	if (rd_event_addr(procp->pp_rap, RD_POSTINIT, &rd_notify) == RD_OK) {
		if (set_breakpoint(procp, rd_notify.u.bptaddr,
		    FLG_BP_RDPOSTINIT) != RET_OK)
			fprintf(stderr,
			    "psi: failed to set BP for postinit at: 0x%lx\n",
			    rd_notify.u.bptaddr);
	} else
		fprintf(stderr, "psi: no event registered for postinit\n");

	if (rd_event_addr(procp->pp_rap, RD_DLACTIVITY, &rd_notify) == RD_OK) {
		if (set_breakpoint(procp, rd_notify.u.bptaddr,
		    FLG_BP_RDDLACT) != RET_OK)
			fprintf(stderr,
				"psi: failed to set BP for dlact at: 0x%lx\n",
				rd_notify.u.bptaddr);
	} else
		fprintf(stderr, "psi: no event registered for dlact\n");

	return (RET_OK);
}


retc_t
ps_close(struct ps_prochandle * ph)
{
	if (ph->pp_auxvp)
		free(ph->pp_auxvp);
	delete_all_breakpoints(ph);
	free_linkmaps(ph);
	return (RET_OK);
}


ps_err_e
ps_auxv(struct ps_prochandle * ph, auxv_t ** auxvp)
{
	int		auxnum;

	if (ph->pp_auxvp != 0) {
		*auxvp = ph->pp_auxvp;
		return (PS_OK);
	}

	if (ioctl(ph->pp_fd, PIOCNAUXV, &auxnum) != 0)
		return (PS_ERR);

	if (auxnum < 1)
		return (PS_ERR);

	ph->pp_auxvp = (auxv_t *)malloc(sizeof (auxv_t) * auxnum);

	if (ioctl(ph->pp_fd, PIOCAUXV, auxvp) != 0) {
		free(ph->pp_auxvp);
		return (PS_ERR);
	}
	*auxvp = ph->pp_auxvp;
	return (PS_OK);
}


ps_err_e
ps_pdmodel(struct ps_prochandle * ph, int * dm)
{
	prpsinfo_t info;

	if (ioctl(ph->pp_fd, PIOCPSINFO, &info) != 0)
	    return (PS_ERR);

	*dm = (int)info.pr_dmodel;
	return (PS_OK);
}


ps_err_e
ps_pread(struct ps_prochandle * ph, psaddr_t addr, void * buf,
	size_t size)
{
	if (pread(ph->pp_fd, buf, size, (off_t)addr) != size)
		return (PS_ERR);

	return (PS_OK);
}


ps_err_e
ps_pwrite(struct ps_prochandle * ph, psaddr_t addr, const void * buf,
	size_t size)
{
	if (pwrite(ph->pp_fd, buf, size, (off_t)addr) != size)
		return (PS_ERR);

	return (PS_OK);
}


ps_err_e
ps_pglobal_sym(struct ps_prochandle * ph,
	const char * object_name, const char * sym_name,
	ps_sym_t * symp)
{
	map_info_t *	mip;
	/* LINTED */
	GElf_Sym *	gsymp = (GElf_Sym *)symp;
	GElf_Sym	gsym;

	if ((mip = str_to_map(ph, object_name)) == 0)
		return (PS_ERR);

	if (ph->pp_dmodel == PR_MODEL_ILP32) {
		elf32_sym_to_gelf((Elf32_Sym *)symp, &gsym);
		gsymp = &gsym;
	}

	if (str_map_sym(sym_name, mip, gsymp, NULL) == RET_FAILED)
		return (PS_ERR);

	if (ph->pp_dmodel == PR_MODEL_ILP32)
		gelf_sym_to_elf32(gsymp, (Elf32_Sym *)symp);

	return (PS_OK);
}


ps_err_e
ps_pglobal_lookup(struct ps_prochandle * ph,
	const char * object_name, const char * sym_name,
	ulong_t * sym_addr)
{
	GElf_Sym	sym;
	map_info_t *	mip;

	if ((mip = str_to_map(ph, object_name)) == 0)
		return (PS_ERR);

	if (str_map_sym(sym_name, mip, &sym, NULL) == RET_FAILED)
		return (PS_ERR);

	*sym_addr = sym.st_value;

	return (PS_OK);
}


ps_err_e
ps_lgetregs(struct ps_prochandle * ph, lwpid_t lid,
	prgregset_t gregset)
{
	int	lwpfd;

	if ((lwpfd = ioctl(ph->pp_fd, PIOCOPENLWP, &lid)) == -1)
		return (PS_ERR);
	if (ioctl(lwpfd, PIOCGREG, gregset) != 0)
		return (PS_ERR);
	close(lwpfd);
	return (PS_OK);
}


void
ps_plog(const char * fmt, ...)
{
	va_list		args;
	static FILE *	log_fp = 0;

	if (log_fp == 0) {
		char		log_fname[256];
		(void) sprintf(log_fname, "/tmp/tdlog.%d", getpid());
		if ((log_fp = fopen(log_fname, "w")) == 0) {
			/*
			 * unable to open log file - default to
			 * stderr.
			 */
			fprintf(stderr, "unable to open %s, logging "
				"redirected to stderr", log_fname);
			log_fp = stderr;
		}
	}

	va_start(args, fmt);
	vfprintf(log_fp, fmt, args);
	va_end(args);
	fflush(log_fp);
}
