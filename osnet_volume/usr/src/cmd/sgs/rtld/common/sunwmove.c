/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)sunwmove.c	1.11	99/05/27 SMI"

/*
 * Object file dependent support for ELF objects.
 */
#include	"_synonyms.h"

#include	<stdio.h>
#include	<sys/procfs.h>
#include	<sys/mman.h>
#include	<string.h>
#include	<dlfcn.h>
#include	"conv.h"
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"
#include	"msg.h"
#include	"debug.h"

/*
 * Copy data
 */
static void
transfer_data(Move *mv, Sym *s, Rt_map *lmp, unsigned long flg)
{
	unsigned char *	taddr = (unsigned char *)s->st_value;
	unsigned int 	stride;
	int 		i;

	if (flg == 0)
		taddr = taddr + mv->m_poffset + ADDR(lmp);
	else
		taddr = taddr + mv->m_poffset;
	stride = mv->m_stride + 1;

	DBG_CALL(Dbg_move_title(2));
	DBG_CALL(Dbg_move_mventry2(mv));
	for (i = 0; i < mv->m_repeat; i++) {
		/* LINTED */
		DBG_CALL(Dbg_move_expanding((const unsigned char *)taddr));
		/* LINTED */
		switch (ELF_M_SIZE(mv->m_info)) {
		case 1:
			/* LINTED */
			*taddr = mv->m_value;
			taddr += stride;
			break;
		case 2:
			/* LINTED */
			*((Half *)taddr) = (Half)mv->m_value;
			taddr += 2*stride;
			break;
		case 4:
			/* LINTED */
			*((Word *)taddr) = (Word)mv->m_value;
			taddr += 4*stride;
			break;
		case 8:
			/* LINTED */
			*((unsigned long long *)taddr) =
				mv->m_value;
			taddr += 8*stride;
			break;
		default:
			eprintf(ERR_NONE, MSG_INTL(MSG_MOVE_ERR1));
			break;
		}
	}
}

/*
 * Move data
 */
void
move_data(Rt_map * lmp)
{
	Move *		mv;
	Sym *		s;
	Phdr *		pptr = SUNWBSS(lmp);
	int 		i;

	DBG_CALL(Dbg_move_movedata(NAME(lmp)));
	mv = MOVETAB(lmp);
	for (i = 0; i < MOVESZ(lmp)/MOVEENT(lmp); i++, mv++) {
		s = (Sym *)SYMTAB(lmp) + ELF_M_SYM(mv->m_info);

		/*
		 * If the target address needs to be mapped in,
		 * map it first.
		 *	(You have to protect the code, thread safe)
		 */
		if (lmp->rt_flags & FLG_RT_SUNWBSS) {
			long	zlen;
			Off	foff;
			caddr_t	zaddr, eaddr;

			foff = (Off) (pptr->p_vaddr + ADDR(lmp));
			zaddr = (caddr_t) M_PROUND(foff);
			eaddr = pptr->p_vaddr + ADDR(lmp) +
				(caddr_t) pptr->p_memsz;
			zero((caddr_t)foff, (long)(zaddr - foff));
			zlen = eaddr - zaddr;
			if (zlen > 0) {
				if (dz_map(zaddr, zlen, PROT_READ | PROT_WRITE,
				    MAP_FIXED | MAP_PRIVATE) == (caddr_t)-1)
					return;
			}

			/*
			 * Since the space is not mapped yet, do it here.
			 */
			lmp->rt_flags &= ~FLG_RT_SUNWBSS;
			DBG_CALL(Dbg_move_sunwbss());
		}

		if (s != NULL) {
			transfer_data(mv, s, lmp,
			    lmp->rt_flags & FLG_RT_ISMAIN);
		}
	}
}
