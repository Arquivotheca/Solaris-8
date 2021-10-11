/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)bindings.c	1.14	98/08/28 SMI"


#include	<sys/types.h>
#include	<string.h>
#include	"msg.h"
#include	"_debug.h"


/*
 * Normally we don't want to display any ld.so.1 bindings (i.e. the bindings
 * to these calls themselves). So, if a Dbg_bind_global() originates from
 * ld.so.1 don't print anything.  If we really want to see the ld.so.1 bindings,
 * simply give the run-time linker a different SONAME.
 */
void
Dbg_bind_global(const char * ffile, caddr_t fabs, caddr_t frel, uint_t pltndx,
	const char * tfile, caddr_t tabs, caddr_t trel, const char * sym)
{
	if (DBG_NOTCLASS(DBG_BINDINGS))
		return;
#ifdef _ELF64
	if (strcmp(ffile, MSG_ORIG(MSG_PTH_RTLD_64)) == 0)
#else
	if (strcmp(ffile, MSG_ORIG(MSG_PTH_RTLD)) == 0)
#endif
		return;

	if (DBG_NOTDETAIL())
		dbg_print(MSG_INTL(MSG_BND_TITLE), ffile, tfile, sym);
	else {
		if ((Sxword)pltndx != -1) {
			/*
			 * Called from a plt offset.
			 */
			dbg_print(MSG_INTL(MSG_BND_PLT), ffile, EC_ADDR(fabs),
			    EC_ADDR(frel), EC_WORD(pltndx), tfile,
			    EC_ADDR(tabs), EC_ADDR(trel), sym);
		} else if ((fabs == 0) && (frel == 0)) {
			/*
			 * Called from a dlsym().  We're not really performing
			 * a relocation, but are handing the address of the
			 * symbol back to the user.
			 */
			dbg_print(MSG_INTL(MSG_BND_DLSYM), ffile, tfile,
			    EC_ADDR(tabs), EC_ADDR(trel), sym);
		} else {
			/*
			 * Standard relocation.
			 */
			dbg_print(MSG_INTL(MSG_BND_DEFAULT), ffile,
				EC_ADDR(fabs), EC_ADDR(frel), tfile,
				EC_ADDR(tabs), EC_ADDR(trel), sym);
		}
	}
}

void
Dbg_bind_weak(const char * ffile, caddr_t fabs, caddr_t frel, const char * sym)
{
	if (DBG_NOTCLASS(DBG_BINDINGS))
		return;

	if (DBG_NOTDETAIL())
		dbg_print(MSG_INTL(MSG_BND_WEAK_1), ffile, sym);
	else
		dbg_print(MSG_INTL(MSG_BND_WEAK_2), ffile, EC_ADDR(fabs),
		    EC_ADDR(frel), sym);
}

void
Dbg_bind_profile(uint_t ndx, uint_t count)
{
	if (DBG_NOTCLASS(DBG_BINDINGS))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_INTL(MSG_BND_PROFILE), EC_WORD(ndx), EC_WORD(count));
}
