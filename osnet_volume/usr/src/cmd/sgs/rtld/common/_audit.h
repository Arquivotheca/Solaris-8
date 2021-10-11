/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#ifndef	__AUDIT_DOT_H
#define	__AUDIT_DOT_H

#pragma ident	"@(#)_audit.h	1.9	99/05/27 SMI"


#ifndef _ASM

#include <sys/types.h>
#include <rtld.h>

/*
 * Define all auditing structures.
 *
 * A shared object may be a client of an audit library, in which case the
 * identify of the shared object is passed to the auditor using a cookie.
 */
typedef struct {
	Rt_map *	ac_lmp;		/* audit library identifier */
	uintptr_t	ac_cookie;	/* cookie assigned to audit library */
	Word		ac_flags;	/*	and its associated flags */
} Audit_client;

#define	FLG_AC_BINDTO	0x00001
#define	FLG_AC_BINDFROM	0x00002

/*
 * Each shared object being audited may provide a list of client structures
 * and dynamic plts (one per auditor).
 */
struct audit_info {
	uint_t		ai_cnt;		/* no. of clients */
	Audit_client *	ai_clients;	/* array of client structures */
	void *		ai_dynplts;	/* array of dynamic plts */
};

/*
 * Define an Audit Descriptor - each audit object is added to this descriptor
 * as an Audit Interface.  There is one global audit descriptor - auditors,
 * and a specific object my require its own - AUDITORS(lmp).
 */
struct audit_desc {
	char *		ad_name;	/* originating audit names */
	List		ad_list;	/* audit objs Audit Interface list */
	uint_t		ad_cnt;		/* No of audit objs in this desc. */
	uint_t		ad_flags;	/* audit capabilities found */
};

/*
 * Define an Audit List descriptor for each audit object.
 */
typedef struct {
	const char *	al_libname;	/* object name for diagnostics */
	Rt_map *	al_lmp;		/* object link-map */
	uint_t (*	al_version)(uint_t);
	void (*		al_preinit)(uintptr_t *);
	char * (*	al_objsearch)(const char *, uintptr_t *, uint_t);
	uint_t (*	al_objopen)(Link_map *, Lmid_t, uintptr_t *);
	uint_t (*	al_objclose)(uintptr_t *);
	void (*		al_activity)(uintptr_t *, uint_t);
#ifdef _LP64
	uintptr_t (*	al_pltenter)(Sym *, uint_t, uintptr_t *, uintptr_t *,
				void *, uint_t *, const char *);
	uintptr_t (*	al_pltexit)(Sym *, uint_t, uintptr_t *, uintptr_t *,
				uintptr_t, const char *);
	uintptr_t (*	al_symbind)(Sym *, uint_t, uintptr_t *,
				uintptr_t *, uint_t *, const char *);
#else
	uintptr_t (*	al_pltenter)(Sym *, uint_t, uintptr_t *, uintptr_t *,
				void *, uint_t *);
	uintptr_t (*	al_pltexit)(Sym *, uint_t, uintptr_t *, uintptr_t *,
				uintptr_t);
	uintptr_t (*	al_symbind)(Sym *, uint_t, uintptr_t *,
				uintptr_t *, uint_t *);
#endif /* _LP64 */
	uint_t		al_vernum;	/* object version */
} Audit_list;


/*
 * Link-Edit audit functions
 */
extern int		audit_setup(Rt_map *, Audit_desc *);

extern void		audit_desc_cleanup(Audit_desc *);
extern void		audit_info_cleanup(Audit_info *);

extern int		audit_objopen(Rt_map *, Rt_map *);
extern void		audit_activity(Rt_map *, uint_t);
extern void		audit_preinit(Rt_map *);
extern char *		audit_objsearch(Rt_map *, const char *, uint_t);
extern void		audit_objclose(Rt_map *, Rt_map *);
extern void		_audit_objclose(List *, Rt_map *);
extern Addr		audit_symbind(Rt_map *, Rt_map *, Sym *, uint_t,
			    Addr value, uint_t *);
extern Addr		audit_pltenter(Rt_map *, Rt_map *, Sym *, uint_t,
			    void *, uint_t *);
extern Addr		audit_pltexit(uintptr_t, Rt_map *, Rt_map *, Sym *,
			    uint_t);

extern uint_t		audit_flags;

#endif /* _ASM */

/*
 * Values for audit_flags.  Intended to be the same as the LML equivalents
 * but kept in a separate variable to simplify boot_elf.s coding.
 */
#define	AF_PLTENTER	0x01		/* same as LML_AUD_PLTENTER */
#define	AF_PLTEXIT	0x02		/* Same as LML_AUD_PLTEXIT */

#endif /* __AUDIT_DOT_H */
