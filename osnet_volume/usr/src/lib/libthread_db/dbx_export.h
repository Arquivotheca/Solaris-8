
/* @(#)dbx_export.h 1.2 91/07/10 SMI */
/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#ifndef dbx_export_h
#define dbx_export_h

/*
 * Stuff to be used by modules of dbx maintained by other groups.
 * The clients of this file are:
 *   - thread_db.o	the thread interface
 */


/*
 * From defs.h
 */

typedef int Boolean;
#define false 0
#define true (!false)
typedef unsigned int Address;
typedef enum {LO, HI} Level_e;

#define xlat_text(t) (t)		/* I18N placeholder */

typedef struct  LoadObj_t * LoadObj;	/* it's a class really */


/*
 * From "library.h"
 */
void	panic(char *s, ...);


/*
 * Symbol table aids
 */
unsigned int quick_lookup(char *name);		/* 0 if no symbol */


/*
 * From "proc.h"
 */
typedef struct Proc_t *Proc;

#include "proc_io.h"

#include "regset.h"

/*
 * From "vcpu.h"
 */
typedef struct VCpu_t	* VCpu;
typedef struct RegFSet_t* RegFSet;
typedef struct RegGSet_t* RegGSet;

Address		VCpu_pc(VCpu);
VCpu		VCpu_by_id(Proc, int id, Boolean install);

RegGSet		VCpu_get_regs(VCpu);
RegFSet		VCpu_get_fregs(VCpu);
void		VCpu_make_dirty(VCpu);

#endif	/* dbx_export_h */
