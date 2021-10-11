/*
 * Copyright (c) 1990 - 1991, 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)module_mp.c	1.11	94/12/09 SMI"

#include <sys/machparam.h>
#include <sys/module.h>

extern void mp_mmu_flushall();
extern void mp_mmu_flushctx();
extern void mp_mmu_flushrgn();
extern void mp_mmu_flushseg();
extern void mp_mmu_flushpage();
extern void mp_mmu_flushpagectx();
extern void mp_pac_pageflush();

void	(*s_mmu_flushall)() = 0;
void	(*s_mmu_flushctx)() = 0;
void	(*s_mmu_flushrgn)() = 0;
void	(*s_mmu_flushseg)() = 0;
void	(*s_mmu_flushpage)() = 0;
void	(*s_mmu_flushpagectx)() = 0;
void	(*s_pac_pageflush)() = 0;

extern void mp_vac_usrflush();
extern void mp_vac_ctxflush();
extern void mp_vac_rgnflush();
extern void mp_vac_segflush();
extern void mp_vac_pageflush();
extern void mp_vac_flush();
extern void mp_vac_allflush();

void	(*s_vac_usrflush)() = 0;
void	(*s_vac_ctxflush)() = 0;
void	(*s_vac_rgnflush)() = 0;
void	(*s_vac_segflush)() = 0;
void	(*s_vac_pageflush)() = 0;
void	(*s_vac_flush)() = 0;
void	(*s_vac_allflush)() = 0;


extern void mp_vac_color_flush();
void	(*s_vac_color_flush)() = 0;

/* IFLUSH support for MP	*/
extern void mp_ic_flush();
void		(*s_ic_flush)() = 0;

/*
 * Support for multiple processors
 */
#define	TAKE(name)	{ s_##name = v_##name; v_##name = mp_##name; }

void
mp_setfunc()
{
	extern int mxcc;

	TAKE(mmu_flushall);
	TAKE(mmu_flushctx);
	TAKE(mmu_flushrgn);
	TAKE(mmu_flushseg);
	TAKE(mmu_flushpage);
	TAKE(mmu_flushpagectx);
	TAKE(vac_usrflush);
	TAKE(vac_ctxflush);
	TAKE(vac_rgnflush);
	TAKE(vac_segflush);
	TAKE(vac_pageflush);
	TAKE(vac_flush);
	TAKE(vac_allflush);
	TAKE(ic_flush);			/* IFLUSH support		*/
	TAKE(vac_color_flush);
	/*
	 * On Sun4m platforms we only flush Viking/Viking E-Cache processor
	 * caches for SX. We need to use the cross calls for flushing
	 * Viking only MP configurations. The algorithm used by the MXCC
	 * cache flush routine does not require cross calls.
	 */
	if (!mxcc) {
		TAKE(pac_pageflush);
	}
#undef	TAKE
	v_mmu_writepte = v_mp_mmu_writepte;	/* use the module's mp func */
	v_window_overflow = v_mp_window_overflow;
	v_window_underflow = v_mp_window_underflow;
	v_vac_color_sync = v_mp_vac_color_sync;
}
