/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)move.c	1.4	98/08/28 SMI"

#include	"msg.h"
#include	"_debug.h"
#include	"libld.h"

/*
 * Debug functions
 */

#if	!defined(_ELF64)
void
Dbg_move_adjexpandreloc(ulong_t offset, const char *name)
{
	if (DBG_NOTCLASS(DBG_MOVE|DBG_RELOC))
		return;
	if (DBG_NOTDETAIL())
		return;
	dbg_print(MSG_INTL(MSG_MV_ADJEXPAND1),
		name,
		EC_XWORD(offset));
}

void
Dbg_move_adjmovereloc(ulong_t offset1, ulong_t offset2, const char *name)
{
	if (DBG_NOTCLASS(DBG_MOVE|DBG_RELOC))
		return;
	if (DBG_NOTDETAIL())
		return;
	dbg_print(MSG_INTL(MSG_MV_ADJMOVE1),
		name,
		EC_XWORD(offset1),
		EC_XWORD(offset2));
}
#endif	/* !defined(_ELF64) */

void
Dbg_move_actsctadj(Sym_desc *s)
{
	if (DBG_NOTCLASS(DBG_MOVE|DBG_RELOC))
		return;
	if (DBG_NOTDETAIL())
		return;
	dbg_print(MSG_INTL(MSG_MV_ACTSCTADJ1),
		s->sd_name);
}

void
Dbg_move_outsctadj(Sym_desc *s)
{
	if (DBG_NOTCLASS(DBG_MOVE|DBG_RELOC))
		return;
	if (DBG_NOTDETAIL())
		return;
	dbg_print(MSG_INTL(MSG_MV_OUTSCTADJ1),
		s->sd_name);
}

#if	!defined(_ELF64)
void
Dbg_move_parexpn(const char *name)
{
	if (DBG_NOTCLASS(DBG_MOVE))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_INTL(MSG_MV_EXPAND0), name);
}

void
Dbg_move_outmove(const unsigned char *name)
{
	if (DBG_NOTCLASS(DBG_MOVE))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_INTL(MSG_MV_OUTMOVE0), name);
}
#endif	/* !defined(_ELF64) */

void
Dbg_move_psyminfo(Ofl_desc *ofl)
{
	Listnode *	lnp1;
	Listnode *	lnp2;
	Psym_info *	psym;
	Mv_itm *	mvi;

	if (DBG_NOTCLASS(DBG_MOVE))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_INTL(MSG_MOVE_TITLE10));
	for (LIST_TRAVERSE(&ofl->ofl_parsym, lnp1, psym)) {
		Sym_desc *	sdp;

		sdp = psym->psym_symd;
		for (LIST_TRAVERSE(&psym->psym_mvs, lnp2, mvi)) {
			if (sdp->sd_flags & FLG_SY_PAREXPN)
				dbg_print(MSG_INTL(MSG_MOVE_MVENTRY2),
					EC_XWORD(mvi->mv_start),
					EC_LWORD(mvi->mv_ientry->m_value),
					sdp->sd_name);
			else
				dbg_print(MSG_INTL(MSG_MOVE_MVENTRY3),
					EC_XWORD(mvi->mv_start),
					EC_LWORD(mvi->mv_ientry->m_value),
					sdp->sd_name);
		}
	}
}

#if	!defined(_ELF64)
void
Dbg_move_movedata(const char *name)
{
	if (DBG_NOTCLASS(DBG_MOVE))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_INTL(MSG_MV_MOVEDATA), name);
}

void
Dbg_move_sections(const char *name, ulong_t size)
{
	if (DBG_NOTCLASS(DBG_MOVE))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_INTL(MSG_MV_SECTIONS), name, EC_XWORD(size));
}

void
Dbg_move_sunwbss()
{
	if (DBG_NOTCLASS(DBG_MOVE))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_INTL(MSG_MV_SUNWBSS1));
}

void
Dbg_move_expanding(const unsigned char *taddr)
{
	if (DBG_NOTCLASS(DBG_MOVE))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_INTL(MSG_MV_EXPAND1), EC_ADDR(taddr));
}


void
Dbg_move_input1(const char *name)
{
	if (DBG_NOTCLASS(DBG_MOVE))
		return;
	if (DBG_NOTDETAIL())
		return;
	dbg_print(MSG_INTL(MSG_MOVE_INPUT1), name);
}

void
Dbg_move_title(int which)
{
	if (DBG_NOTCLASS(DBG_MOVE))
		return;
	if (DBG_NOTDETAIL())
		return;

	if (which == 1)
		dbg_print(MSG_INTL(MSG_MOVE_TITLE1));
	else if (which == 2)
		dbg_print(MSG_INTL(MSG_MOVE_TITLE2));
}
#endif	/* !defined(_ELF64) */

void
Dbg_move_mventry(int which, Move *mv, Sym_desc *s)
{
	if (DBG_NOTCLASS(DBG_MOVE))
		return;
	if (DBG_NOTDETAIL())
		return;
	if (which == 0)
		dbg_print(MSG_INTL(MSG_MOVE_MVENTRY100),
		EC_XWORD(mv->m_poffset),
		EC_LWORD(mv->m_value),
		mv->m_repeat,
		mv->m_stride,
		s->sd_name);
	else
		dbg_print(MSG_INTL(MSG_MOVE_MVENTRY101),
		EC_XWORD(mv->m_poffset),
		EC_LWORD(mv->m_value),
		mv->m_repeat,
		mv->m_stride,
		s->sd_name);
}

void
Dbg_move_mventry2(Move *mv)
{
	if (DBG_NOTCLASS(DBG_MOVE))
		return;
	if (DBG_NOTDETAIL())
		return;
	dbg_print(MSG_INTL(MSG_MOVE_MVENTRY4),
		EC_XWORD(mv->m_poffset),
		EC_LWORD(mv->m_value),
		mv->m_repeat,
		mv->m_stride);
}
