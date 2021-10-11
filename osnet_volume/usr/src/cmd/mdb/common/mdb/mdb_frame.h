/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_FRAME_H
#define	_MDB_FRAME_H

#pragma ident	"@(#)mdb_frame.h	1.1	99/08/11 SMI"

#include <mdb/mdb_module.h>
#include <mdb/mdb_addrvec.h>
#include <mdb/mdb_list.h>
#include <mdb/mdb_umem.h>
#include <mdb/mdb_vcb.h>
#include <mdb/mdb_wcb.h>
#include <mdb/mdb.h>
#include <setjmp.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct mdb_cmd {
	mdb_list_t c_list;		/* List forward/back pointers */
	mdb_idcmd_t *c_dcmd;		/* Dcmd to invoke */
	mdb_argvec_t c_argv;		/* Arguments for this command */
	mdb_addrvec_t c_addrv;		/* Addresses for this command */
	mdb_vcb_t *c_vcbs;		/* Variable control block list */
} mdb_cmd_t;

typedef struct mdb_frame {
	mdb_list_t f_cmds;		/* List of commands to execute */
	mdb_wcb_t *f_wcbs;		/* Walk control blocks for GC */
	mdb_mblk_t *f_mblks;		/* Memory blocks for GC */
	mdb_cmd_t *f_pcmd;		/* Next cmd in pipe (if pipe active) */
	mdb_cmd_t *f_cp;		/* Pointer to executing command */
	mdb_iob_stack_t f_istk;		/* Stack of input i/o buffers */
	mdb_iob_stack_t f_ostk;		/* Stack of output i/o buffers */
	struct mdb_frame *f_prev;	/* Previous frame pointer */
	jmp_buf f_pcb;			/* Control block for longjmp */
	uint_t f_flags;			/* Volatile flags to save/restore */
} mdb_frame_t;

#ifdef _MDB

extern mdb_cmd_t *mdb_cmd_create(mdb_idcmd_t *, mdb_argvec_t *);
extern void mdb_cmd_destroy(mdb_cmd_t *);
extern void mdb_cmd_reset(mdb_cmd_t *);

extern void mdb_frame_reset(mdb_frame_t *);
extern void mdb_frame_push(mdb_frame_t *);
extern void mdb_frame_pop(mdb_frame_t *, int);

#endif	/* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_FRAME_H */
