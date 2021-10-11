/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)modstubs.s	1.127	99/11/11 SMI"

#include <sys/asm_linkage.h>

#if defined(lint)

char stubs_base[1], stubs_end[1];

#else	/* lint */

/*
 * WARNING: there is no check for forgetting to write END_MODULE,
 * and if you do, the kernel will most likely crash.  Be careful
 *
 * This file assumes that all of the contributions to the data segment
 * will be contiguous in the output file, even though they are separated
 * by pieces of text.  This is safe for all assemblers I know of now...
 */

/*
 * This file uses ansi preprocessor features:
 *
 * 1. 	#define mac(a) extra_ ## a     -->   mac(x) expands to extra_a
 * The old version of this is
 *      #define mac(a) extra_/.*.*./a
 * but this fails if the argument has spaces "mac ( x )"
 * (Ignore the dots above, I had to put them in to keep this a comment.)
 *
 * 2.   #define mac(a) #a             -->    mac(x) expands to "x"
 * The old version is
 *      #define mac(a) "a"
 *
 * For some reason, the 5.0 preprocessor isn't happy with the above usage.
 * For now, we're not using these ansi features.
 *
 * The reason is that "the 5.0 ANSI preprocessor" is built into the compiler
 * and is a tokenizing preprocessor. This means, when confronted by something
 * other than C token generation rules, strange things occur. In this case,
 * when confronted by an assembly file, it would turn the token ".globl" into
 * two tokens "." and "globl". For this reason, the traditional, non-ANSI
 * prepocessor is used on assembly files.
 *
 * It would be desirable to have a non-tokenizing cpp (accp?) to use for this.
 */

/*
 * This file contains the stubs routines for modules which can be autoloaded.
 */


/*
 * See the 'struct mod_modinfo' definition to see what this structure
 * is trying to achieve here.
 */
#ifdef __sparcv9
/*
 * XX64 - This still needs some repair.
 * (a) define 'pointer alignment' and use it
 * (b) define '.pword' or equivalent, and use it (to mean .word or .xword).
 */
#define	MODULE(module,namespace)	\
	.seg	".data";		\
module/**/_modname:			\
	.ascii	"namespace/module";	\
	.byte	0;			\
	.align	CPTRSIZE;		\
	.global	module/**/_modinfo;	\
	.type	module/**/_modinfo, #object;	\
	.size	module/**/_modinfo, 16;	\
module/**/_modinfo:			\
	.word 0;			\
	.word module/**/_modname;	\
	.word 0;			\
	.word 0;

#define	END_MODULE(module)		\
	.align 8; .word 0; .word 0	/* FIXME: .xword 0 */

#else	/* __sparcv9 */

#define MODULE(module,namespace)	\
	.seg	".data";		\
module/**/_modname:			\
	.ascii	"namespace/module";	\
	.byte	0;			\
	.align	4;			\
	.global module/**/_modinfo;	\
	.type	module/**/_modinfo, #object;	\
	.size	module/**/_modinfo, 8;	\
module/**/_modinfo:			\
	.word module/**/_modname;	\
	.word 0		/* storage for modctl pointer */

	/* then stub_info structures follow until a mods_func_adr is 0 */

#define END_MODULE(module)		\
	.word 0		/* a zero where the next mods_func_adr would be */

#endif	/* __sparcv9 */


#define STUB(module, fcnname, retfcn)	\
		STUB_COMMON(module, fcnname, mod_hold_stub, retfcn, 0)

/*
 * "weak stub", don't load on account of this call
 */
#define WSTUB(module, fcnname, retfcn)	\
		STUB_COMMON(module, fcnname, retfcn, retfcn, 1)

/*
 * "non-unloadable stub", don't bother 'holding' module if it's already loaded
 * since the module cannot be unloaded.
 *
 * User *MUST* guarantee the module is not unloadable (no _fini routine).
 */
#define NO_UNLOAD_STUB(module, fcnname, retfcn)	\
		STUB_UNLOADABLE(module, fcnname, retfcn, retfcn, 2)

/*
 * Macro for modstubbed system calls whose modules are not unloadable.
 *
 * System call modstubs needs special handling for the case where
 * the modstub is a system call, because %fp comes from user frame.
 */
#define	SCALL_NU_STUB(module, fcnname, retfcn)	\
		SCALL_UNLOADABLE(module, fcnname, retfcn, retfcn, 2)
/* "weak stub" for non-unloadable module, don't load on account of this call */
#define NO_UNLOAD_WSTUB(module, fcnname, retfcn) \
		STUB_UNLOADABLE(module, fcnname, retfcn, retfcn, 3)

#ifdef __sparcv9

#define	STUB_DATA(module, fcnname, install_fcn, retfcn, weak)		\
	.seg	".data";						\
	.align	8;							\
fcnname/**/_info:							\
	.word	0;			/* 0 */				\
	.word	install_fcn;		/* 4 */				\
	.word	0;			/* 8 */				\
	.word	module/**/_modinfo;	/* c */				\
	.word	0;			/* 10 */			\
	.word	fcnname;		/* 14 */			\
	.word	0;			/* 18 */			\
	.word	retfcn;			/* 1c */			\
	.word   weak			/* 20 */

#define STUB_COMMON(module, fcnname, install_fcn, retfcn, weak)		\
	ENTRY_NP(fcnname);						\
	save	%sp, -SA(MINFRAME), %sp;	/* new window */	\
	set	fcnname/**/_info, %l5;					\
	ld	[%l5 + 0x20], %l1;	/* weak?? */			\
	cmp	%l1, 0;							\
	be,a	1f;			/* not weak */			\
	restore;							\
	ldn	[%l5 + 0x18], %l1;	/* yes, installed?? */		\
	ldn	[%l5], %l0;						\
	cmp	%l1, %l0;						\
	bne,a,pt %xcc, 1f;		/* yes, do mod_hold thing */	\
	restore;							\
	mov	%l0, %g1;						\
	jmp	%g1;			/* no, just jump to retfcn */	\
	restore;							\
1:	sub	%sp, %fp, %g1;	/* get (-)size of callers stack */	\
	save	%sp, %g1, %sp;	/* create new frame same size */	\
	sub	%g0, %g1, %l4;  /* size of stack frame */		\
	sethi	%hi(fcnname/**/_info), %l5;				\
	b	stubs_common_code;					\
	or	%l5, %lo(fcnname/**/_info), %l5;			\
	SET_SIZE(fcnname);						\
	STUB_DATA(module, fcnname, install_fcn, retfcn, weak)

#define STUB_UNLOADABLE(module, fcnname, install_fcn, retfcn, weak)	\
	ENTRY_NP(fcnname);						\
	save	%sp, -SA(MINFRAME), %sp;	/* new window */	\
	set	fcnname/**/_info, %l5;					\
	ldn	[%l5 + 0x18], %l1;		/* installed?? */	\
	ldn	[%l5], %g1;						\
	cmp	%l1, %g1;						\
	be,a,pt	%xcc, 1f;			/* no, load module */	\
	restore;							\
	jmp	%g1;				/* yes, off we go */	\
	restore;							\
1:	sub	%sp, %fp, %g1;	/* get (-)size of callers frame */	\
	save	%sp, %g1, %sp;	/* create new frame same size */	\
	sub	%g0, %g1, %l4;  /* size of stack frame */		\
	sethi	%hi(fcnname/**/_info), %l5;				\
	b	stubs_common_code;					\
	or	%l5, %lo(fcnname/**/_info), %l5;			\
	SET_SIZE(fcnname);						\
	STUB_DATA(module, fcnname, install_fcn, retfcn, weak)

#define SCALL_UNLOADABLE(module, fcnname, install_fcn, retfcn, weak)	\
	ENTRY_NP(fcnname);						\
	save	%sp, -SA(MINFRAME), %sp;	/* new window */	\
	set	fcnname/**/_info, %l5;					\
	ldn	[%l5 + 0x18], %l1;		/* installed?? */	\
	ldn	[%l5], %g1;						\
	cmp	%l1, %g1;						\
	be,a,pt	%xcc, 1f;			/* no, load module */	\
	restore;							\
	jmp	%g1;				/* yes, off we go */	\
	restore;							\
1:	save	%sp, -SA(MINFRAME), %sp;/* new frame */			\
	sub	%g0, -SA(MINFRAME), %l4;/* size of stack frame */	\
	sethi	%hi(fcnname/**/_info), %l5;				\
	b	stubs_common_code;					\
	or	%l5, %lo(fcnname/**/_info), %l5;			\
	SET_SIZE(fcnname);						\
	STUB_DATA(module, fcnname, install_fcn, retfcn, weak)

#else	/* __sparcv9 */

#define	STUB_DATA(module, fcnname, install_fcn, retfcn, weak)		\
	.seg ".data";							\
	.align	 4;							\
fcnname/**/_info:							\
	.word	install_fcn;		/* 0 */				\
	.word	module/**/_modinfo;	/* 4 */				\
	.word	fcnname;		/* 8 */				\
	.word	retfcn;			/* c */				\
	.word   weak			/* 10 */

#define STUB_COMMON(module, fcnname, install_fcn, retfcn, weak)		\
	ENTRY_NP(fcnname);						\
	save	%sp, -SA(MINFRAME), %sp;	/* new window */	\
	sethi	%hi(fcnname/**/_info), %l5;				\
	or	%l5, %lo(fcnname/**/_info), %l5;			\
	ld	[%l5 + 0x10], %l1;	/* weak?? */			\
	cmp	%l1, 0;							\
	be,a	1f;			/* not weak */			\
	restore;							\
	ld	[%l5 + 0xc], %l1;	/* yes, installed?? */		\
	ld	[%l5], %l0;						\
	cmp	%l1, %l0;						\
	bne,a	1f;		/* yes, so do the mod_hold thing */	\
	restore;							\
	mov	%l0, %g1;						\
	jmp	%g1;			/* no, just jump to retfcn */	\
	restore;							\
1:	sub	%sp, %fp, %g1;	/* get (-)size of callers stack frame */ \
	save	%sp, %g1, %sp;	/* create new frame same size */	\
	sub	%g0, %g1, %l4;  /* size of stack frame */		\
	sethi	%hi(fcnname/**/_info), %l5;				\
	b	stubs_common_code;					\
	or	%l5, %lo(fcnname/**/_info), %l5;			\
	SET_SIZE(fcnname);						\
	STUB_DATA(module, fcnname, install_fcn, retfcn, weak)

#define STUB_UNLOADABLE(module, fcnname, install_fcn, retfcn, weak)	\
	ENTRY_NP(fcnname);						\
	save	%sp, -SA(MINFRAME), %sp;	/* new window */	\
	sethi	%hi(fcnname/**/_info), %l5;				\
	or	%l5, %lo(fcnname/**/_info), %l5;			\
	ld	[%l5 + 0xc], %l1;		/* installed?? */	\
	ld	[%l5], %g1;						\
	cmp	%l1, %g1;						\
	be,a	1f;				/* no, load module */	\
	restore;							\
	jmp	%g1;				/* yes, off we go */	\
	restore;							\
1:	sub	%sp, %fp, %g1;	/* get (-)size of callers stack frame */ \
	save	%sp, %g1, %sp;	/* create new frame same size */	\
	sub	%g0, %g1, %l4;  /* size of stack frame */		\
	sethi	%hi(fcnname/**/_info), %l5;				\
	b	stubs_common_code;					\
	or	%l5, %lo(fcnname/**/_info), %l5;			\
	SET_SIZE(fcnname);						\
	STUB_DATA(module, fcnname, install_fcn, retfcn, weak)

#define SCALL_UNLOADABLE(module, fcnname, install_fcn, retfcn, weak)	\
	ENTRY_NP(fcnname);						\
	save	%sp, -SA(MINFRAME), %sp;	/* new window */	\
	sethi	%hi(fcnname/**/_info), %l5;				\
	or	%l5, %lo(fcnname/**/_info), %l5;			\
	ld	[%l5 + 0xc], %l1;		/* installed?? */	\
	ld	[%l5], %g1;						\
	cmp	%l1, %g1;						\
	be,a	1f;			/* no, load module */		\
	restore;							\
	jmp	%g1;			/* yes, off we go */		\
	restore;							\
1:	save	%sp, -SA(MINFRAME), %sp;/* new frame */			\
	mov	SA(MINFRAME), %l4;	/* size of stack frame */	\
	sethi	%hi(fcnname/**/_info), %l5;				\
	b	stubs_common_code;					\
	or	%l5, %lo(fcnname/**/_info), %l5;			\
	SET_SIZE(fcnname);						\
	STUB_DATA(module, fcnname, install_fcn, retfcn, weak)

#endif	/* __sparcv9 */

	.section	".text"

	/*
	 * We branch here with the fcnname_info pointer in l5
	 * and the frame size in %l4.
	 */
	ENTRY_NP(stubs_common_code)
#ifdef __sparcv9
	cmp	%l4, SA(MINFRAME)
	ble,a,pn %xcc, 2f
	nop

	sub	%l4, 0x80, %l4		/* skip locals and outs */
	add	%sp, 0x80, %l0
	add	%fp, 0x80, %l1		/* get original sp before save */
1:
	/* Copy stack frame */
	ldn	[%l1 + STACK_BIAS], %l2
	inc	8, %l1
	stn	%l2, [%l0 + STACK_BIAS]
	deccc	8, %l4
	bg,a	1b
	inc	8, %l0
2:
	call	mod_hold_stub		/* Hold the module */
	mov	%l5, %o0
	cmp	%o0, -1			/* if error then return error */
	bne,a	1f
	nop
	ldn	[%l5 + 0x18], %i0
	call	%i0
	nop
	ret
	restore	%o0, 0, %o0
1:
	ldn	[%l5], %g1
	mov	%i0, %o0	/* copy over incoming args, if number of */
	mov	%i1, %o1	/* args is > 6 then we copied them above */
	mov	%i2, %o2
	mov	%i3, %o3
	mov	%i4, %o4
	call	%g1		/* jump to the stub function */
	mov	%i5, %o5
	mov	%o0, %i0	/* copy any return values */
	mov	%o1, %i1
	call	mod_release_stub	/* release hold on module */
	mov	%l5, %o0
	ret			/* return to caller */
	restore

#else	/* __sparcv9 */

	cmp	%l4, SA(MINFRAME)	/* If stack <= SA(MINFRAME) */
	ble,a	2f			/* then don't copy anything */
	ld	[%fp + 0x40], %g1	/* hidden param for aggregate return */

	sub	%l4, 0x40, %l4		/* skip locals and outs */
	add	%sp, 0x40, %l0
	add	%fp, 0x40, %l1		/* get original sp before save */
1:
	/* Copy stack frame */
	ldd	[%l1], %l2
	inc	8, %l1
	std	%l2, [%l0]
	deccc	8, %l4
	bg,a	1b
	inc	8, %l0
2:
	st	%g1, [%sp + 0x40]	/* aggregate return value */
	call	mod_hold_stub		/* Hold the module */
	mov	%l5, %o0
	cmp	%o0, -1 		/* If error then return error (panic?)*/
	bne,a	1f
	mov	%o0, %l0
	ld	[%l5 + 0xc], %i0
	call    %i0
	nop
	mov     %o0, %i0                /* copy any return values */
	ret
	restore	%g0, %o1, %o1		/* for 64 bit returns */
1:
	ld	[%l5], %g1
	mov	%i0, %o0	/* copy over incoming args, if number of */
	mov	%i1, %o1	/* args is > 6 then we copied them above */
	mov	%i2, %o2
	mov	%i3, %o3
	mov	%i4, %o4
	call	%g1		/* jump to the stub function */
	mov	%i5, %o5
	mov	%o0, %i0	/* copy any return values */
	mov	%o1, %i1
	mov	%l5, %o0
	call	mod_release_stub	/* release hold on module */
	mov	%l0, %o1
	ret			/* return to caller */
	restore

#endif	/* __sparcv9 */

	SET_SIZE(stubs_common_code)

! this is just a marker for the area of text that contains stubs 
	.seg ".text"
	.global stubs_base
stubs_base:
	nop

/*
 * WARNING WARNING WARNING!!!!!!
 * 
 * On the MODULE macro you MUST NOT use any spaces!!! They are
 * significant to the preprocessor.  With ansi c there is a way around this
 * but for some reason (yet to be investigated) ansi didn't work for other
 * reasons!  
 *
 * When zero is used as the return function, the system will call
 * panic if the stub can't be resolved.
 */

/*
 * Stubs for specfs. A non-unloadable module.
 */

#ifndef SPEC_MODULE
	MODULE(specfs,fs);
	NO_UNLOAD_STUB(specfs, common_specvp,  	nomod_zero);
	NO_UNLOAD_STUB(specfs, makectty,		nomod_zero);
	NO_UNLOAD_STUB(specfs, makespecvp,      	nomod_zero);
	NO_UNLOAD_STUB(specfs, smark,           	nomod_zero);
	NO_UNLOAD_STUB(specfs, spec_segmap,     	nomod_einval);
	NO_UNLOAD_STUB(specfs, specfind,        	nomod_zero);
	NO_UNLOAD_STUB(specfs, specvp,          	nomod_zero);
	NO_UNLOAD_STUB(specfs, stillreferenced, 	nomod_zero);
	NO_UNLOAD_STUB(specfs, devi_stillreferenced, 	nomod_zero);
	NO_UNLOAD_STUB(specfs, spec_getvnodeops,	nomod_zero);
	NO_UNLOAD_STUB(specfs, spec_char_map,	nomod_zero);
	END_MODULE(specfs);
#endif


/*
 * Stubs for sockfs. A non-unloadable module.
 */
#ifndef SOCK_MODULE
	MODULE(sockfs,fs);
	SCALL_NU_STUB(sockfs, so_socket,  	nomod_zero);
	SCALL_NU_STUB(sockfs, so_socketpair,	nomod_zero);
	SCALL_NU_STUB(sockfs, bind,  		nomod_zero);
	SCALL_NU_STUB(sockfs, listen,  		nomod_zero);
	SCALL_NU_STUB(sockfs, accept,  		nomod_zero);
	SCALL_NU_STUB(sockfs, connect,  	nomod_zero);
	SCALL_NU_STUB(sockfs, shutdown,  	nomod_zero);
	SCALL_NU_STUB(sockfs, recv,  		nomod_zero);
	SCALL_NU_STUB(sockfs, recvfrom,  	nomod_zero);
	SCALL_NU_STUB(sockfs, recvmsg,  	nomod_zero);
	SCALL_NU_STUB(sockfs, send,  		nomod_zero);
	SCALL_NU_STUB(sockfs, sendmsg,  	nomod_zero);
	SCALL_NU_STUB(sockfs, sendto,	  	nomod_zero);
	SCALL_NU_STUB(sockfs, getpeername,  	nomod_zero);
	SCALL_NU_STUB(sockfs, getsockname,  	nomod_zero);
	SCALL_NU_STUB(sockfs, getsockopt,  	nomod_zero);
	SCALL_NU_STUB(sockfs, setsockopt,  	nomod_zero);
	SCALL_NU_STUB(sockfs, sockconfig,  	nomod_zero);
	NO_UNLOAD_STUB(sockfs, sock_getmsg,  	nomod_zero);
	NO_UNLOAD_STUB(sockfs, sock_putmsg,  	nomod_zero);
	END_MODULE(sockfs);
#endif

#ifndef	IPSECAH_MODULE
	MODULE(ipsecah,drv);
	WSTUB(ipsecah,	getahassoc,	nomod_zero);
	END_MODULE(ipsecah);
#endif

#ifndef	IPSECESP_MODULE
	MODULE(ipsecesp,drv);
	WSTUB(ipsecesp,	getespassoc,	nomod_zero);
	END_MODULE(ipsecesp);
#endif

#ifndef KEYSOCK_MODULE
	MODULE(keysock,drv);
	WSTUB(keysock,	keysock_plumb_ipsec,	nomod_zero);
	END_MODULE(keysock);
#endif

/*
 * Stubs for nfs common code.
 * XXX nfs_getvnodeops should go away with removal of kludge in vnode.c
 */
#ifndef NFS_MODULE
	MODULE(nfs,fs);
	WSTUB(nfs,	nfs_getvnodeops,	nomod_zero);
	WSTUB(nfs,	nfs_perror,		nomod_zero);
	WSTUB(nfs,	nfs_cmn_err,		nomod_zero);
	END_MODULE(nfs);
#endif

/*
 * Stubs for nfs_dlboot (diskless booting).
 */
#ifndef NFS_DLBOOT_MODULE
	MODULE(nfs_dlboot,misc);
	STUB(nfs_dlboot,	mount_root,	nomod_minus_one);
	END_MODULE(nfs_dlboot);
#endif

/*
 * Stubs for nfs server-only code.
 */
#ifndef NFSSRV_MODULE
	MODULE(nfssrv,misc);
	STUB(nfssrv,		lm_nfs3_fhtovp,	nomod_minus_one);
	STUB(nfssrv,		lm_fhtovp,	nomod_minus_one);
	STUB(nfssrv,		exportfs,	nomod_minus_one);
	STUB(nfssrv,		nfs_getfh,	nomod_minus_one);
	STUB(nfssrv,		nfsl_flush,	nomod_minus_one);
	NO_UNLOAD_STUB(nfssrv,	nfs_svc,	nomod_zero);
	END_MODULE(nfssrv);
#endif

/*
 * Stubs for kernel lock manager.
 */
#ifndef KLM_MODULE
	MODULE(klmmod,misc);
	NO_UNLOAD_STUB(klmmod, lm_svc,		nomod_zero);
	NO_UNLOAD_STUB(klmmod, lm_shutdown,	nomod_zero);
	NO_UNLOAD_STUB(klmmod, lm_cprresume,	nomod_zero);
	NO_UNLOAD_STUB(klmmod, lm_cprsuspend,	nomod_zero); 
	NO_UNLOAD_STUB(klmmod, lm_safelock, nomod_zero);
	NO_UNLOAD_STUB(klmmod, lm_safemap, nomod_zero);
	NO_UNLOAD_STUB(klmmod, lm_has_sleep, nomod_zero);
	END_MODULE(klmmod);
#endif

#ifndef KLMOPS_MODULE
	MODULE(klmops,misc);
	NO_UNLOAD_STUB(klmops, lm_frlock,	nomod_zero);
	NO_UNLOAD_STUB(klmops, lm4_frlock,	nomod_zero);
	NO_UNLOAD_STUB(klmops, lm_shrlock,	nomod_zero);
	NO_UNLOAD_STUB(klmops, lm4_shrlock,	nomod_zero);
	NO_UNLOAD_STUB(klmops, lm_nlm_dispatch,	nomod_zero);
	NO_UNLOAD_STUB(klmops, lm_nlm4_dispatch,	nomod_zero);
	NO_UNLOAD_STUB(klmops, lm_nlm_reclaim,	nomod_zero);
	NO_UNLOAD_STUB(klmops, lm_nlm4_reclaim,	nomod_zero);
	NO_UNLOAD_STUB(klmops, lm_register_lock_locally, nomod_zero);
	END_MODULE(klmops);
#endif

/*
 * Stubs for kernel TLI module
 *   XXX currently we never allow this to unload
 */
#ifndef TLI_MODULE
	MODULE(tlimod,misc);
	NO_UNLOAD_STUB(tlimod, t_kopen,		nomod_minus_one);
	NO_UNLOAD_STUB(tlimod, t_kunbind,  	nomod_zero);
	NO_UNLOAD_STUB(tlimod, t_kadvise,  	nomod_zero);
	NO_UNLOAD_STUB(tlimod, t_krcvudata,  	nomod_zero);
	NO_UNLOAD_STUB(tlimod, t_ksndudata,  	nomod_zero);
	NO_UNLOAD_STUB(tlimod, t_kalloc,  	nomod_zero);
	NO_UNLOAD_STUB(tlimod, t_kbind,  	nomod_zero);
	NO_UNLOAD_STUB(tlimod, t_kclose,  	nomod_zero);
	NO_UNLOAD_STUB(tlimod, t_kspoll,  	nomod_zero);
	NO_UNLOAD_STUB(tlimod, t_kfree,  	nomod_zero);
	END_MODULE(tlimod);
#endif

/*
 * Stubs for kernel RPC module
 *   XXX currently we never allow this to unload
 */
#ifndef RPC_MODULE
	MODULE(rpcmod,misc);
	NO_UNLOAD_STUB(rpcmod, clnt_tli_kcreate,	nomod_minus_one);
	NO_UNLOAD_STUB(rpcmod, svc_tli_kcreate,		nomod_minus_one);
	NO_UNLOAD_STUB(rpcmod, bindresvport,		nomod_minus_one);
	NO_UNLOAD_STUB(rpcmod, xdrmblk_init,		nomod_zero);
	NO_UNLOAD_STUB(rpcmod, xdrmem_create,		nomod_zero);
	END_MODULE(rpcmod);
#endif

/*
 * Stubs for des
 */
#ifndef DES_MODULE
	MODULE(des,misc);
	STUB(des, cbc_crypt, 	 	nomod_zero);
	STUB(des, ecb_crypt, 		nomod_zero);
	STUB(des, _des_crypt,		nomod_zero);
	END_MODULE(des);
#endif

/*
 * Stubs for procfs. A non-unloadable module.
 */
#ifndef PROC_MODULE
	MODULE(procfs,fs);
	NO_UNLOAD_STUB(procfs, prfree,		nomod_zero);
	NO_UNLOAD_STUB(procfs, prexit,		nomod_zero);
	NO_UNLOAD_STUB(procfs, prlwpexit,	nomod_zero);
	NO_UNLOAD_STUB(procfs, prinvalidate,	nomod_zero);
	NO_UNLOAD_STUB(procfs, prnsegs,		nomod_zero);
	NO_UNLOAD_STUB(procfs, prgetcred,	nomod_zero);
	NO_UNLOAD_STUB(procfs, prgetstatus,	nomod_zero);
	NO_UNLOAD_STUB(procfs, prgetlwpstatus,	nomod_zero);
	NO_UNLOAD_STUB(procfs, prgetpsinfo,	nomod_zero);
	NO_UNLOAD_STUB(procfs, prgetlwpsinfo,	nomod_zero);
	NO_UNLOAD_STUB(procfs, oprgetstatus,	nomod_zero);
	NO_UNLOAD_STUB(procfs, oprgetpsinfo,	nomod_zero);
#ifdef _SYSCALL32_IMPL
	NO_UNLOAD_STUB(procfs, prgetstatus32,	nomod_zero);
	NO_UNLOAD_STUB(procfs, prgetlwpstatus32, nomod_zero);
	NO_UNLOAD_STUB(procfs, prgetpsinfo32,	nomod_zero);
	NO_UNLOAD_STUB(procfs, prgetlwpsinfo32,	nomod_zero);
	NO_UNLOAD_STUB(procfs, oprgetstatus32,	nomod_zero);
	NO_UNLOAD_STUB(procfs, oprgetpsinfo32,	nomod_zero);
#endif	/* _SYSCALL32_IMPL */
	NO_UNLOAD_STUB(procfs, prnotify,	nomod_zero);
	NO_UNLOAD_STUB(procfs, prexecstart,	nomod_zero);
	NO_UNLOAD_STUB(procfs, prexecend,	nomod_zero);
	NO_UNLOAD_STUB(procfs, prrelvm,		nomod_zero);
	NO_UNLOAD_STUB(procfs, prbarrier,	nomod_zero);
	NO_UNLOAD_STUB(procfs, estimate_msacct,	nomod_zero);
	NO_UNLOAD_STUB(procfs, pr_getprot,	nomod_zero);
	NO_UNLOAD_STUB(procfs, pr_getsegsize,	nomod_zero);
	NO_UNLOAD_STUB(procfs, pr_isobject,	nomod_zero);
	NO_UNLOAD_STUB(procfs, pr_isself,	nomod_zero);
	NO_UNLOAD_STUB(procfs, pr_allstopped,	nomod_zero);
	NO_UNLOAD_STUB(procfs, pr_free_my_pagelist, nomod_zero);
	END_MODULE(procfs);
#endif

/*
 * Stubs for fifofs
 */
#ifndef FIFO_MODULE
	MODULE(fifofs,fs);
	STUB(fifofs, fifovp,      	0);
	STUB(fifofs, fifo_getinfo,	0);
	STUB(fifofs, fifo_vfastoff,	0);
	END_MODULE(fifofs);
#endif

/*
 * Stubs for ufs
 *
 * This is needed to support the old quotactl system call.
 * When the old sysent stuff goes away, this will need to be revisited.
 */
#ifndef UFS_MODULE
	MODULE(ufs,fs);
	STUB(ufs, quotactl, nomod_minus_one);
	END_MODULE(ufs);
#endif

/*
 * Stubs for namefs
 */
#ifndef NAMEFS_MODULE
	MODULE(namefs,fs);
	STUB(namefs, nm_unmountall, 	0);
	END_MODULE(namefs);
#endif

/*
 * Stubs for ts_dptbl
 */
#ifndef TS_DPTBL_MODULE
	MODULE(TS_DPTBL,sched);
	STUB(TS_DPTBL, ts_getdptbl,		0);
	STUB(TS_DPTBL, ts_getkmdpris,		0);
	STUB(TS_DPTBL, ts_getmaxumdpri,	0);
	END_MODULE(TS_DPTBL);
#endif

/*
 * Stubs for rt_dptbl
 */
#ifndef RT_DPTBL_MODULE
	MODULE(RT_DPTBL,sched);
	STUB(RT_DPTBL, rt_getdptbl,		0);
	END_MODULE(RT_DPTBL);
#endif

/*
 * Stubs for ia_dptbl
 */
#ifndef IA_DPTBL_MODULE
	MODULE(IA_DPTBL,sched);
	STUB(IA_DPTBL, ia_getdptbl,		0);
	STUB(IA_DPTBL, ia_getkmdpris,		0);
	STUB(IA_DPTBL, ia_getmaxumdpri,	0);
	END_MODULE(IA_DPTBL);
#endif

/*
 * Stubs for kb (only needed for 'win')
 */
#ifndef KB_MODULE
	MODULE(kb,strmod);
	STUB(kb, strsetwithdecimal,	0);
	END_MODULE(kb);
#endif

/*
 * Stubs for swapgeneric
 */
#ifndef SWAPGENERIC_MODULE
	MODULE(swapgeneric,misc);
	STUB(swapgeneric, rootconf,     0);
	STUB(swapgeneric, getfstype,    0);
	STUB(swapgeneric, getswapdev,   0);
	STUB(swapgeneric, getrootdev,   0);
	STUB(swapgeneric, getfsname,    0);
	STUB(swapgeneric, loadrootmodules, 0);
	STUB(swapgeneric, loaddrv_hierarchy, 0);
	STUB(swapgeneric, getlastfrompath, 0);
	END_MODULE(swapgeneric);
#endif

/*
 * Stubs for bootdev
 */
#ifndef BOOTDEV_MODULE
	MODULE(bootdev,misc);
	STUB(bootdev, i_devname_to_promname,     0);
	STUB(bootdev, i_promname_to_devname,     0);
	END_MODULE(bootdev);
#endif

/*
 * stubs for strplumb...
 */
#ifndef STRPLUMB_MODULE
	MODULE(strplumb,misc);
	STUB(strplumb, strplumb,     0);
	STUB(strplumb, strplumb_get_driver_list, 0);
	END_MODULE(strplumb);
#endif

/*
 * Stubs for console configuration module
 */
#ifndef CONSCONFIG_MODULE
	MODULE(consconfig,misc);
	STUB(consconfig, consconfig,     0);
	END_MODULE(consconfig);
#endif

/*
 * Stubs for zs (uart) module
 */
#ifndef ZS_MODULE
	MODULE(zs,drv);
	STUB(zs, zsgetspeed,		0);
	END_MODULE(zs);
#endif

/* 
 * Stubs for accounting.
 */
#ifndef SYSACCT_MODULE
	MODULE(sysacct,sys);
	WSTUB(sysacct, acct,  		nomod_zero);
	END_MODULE(sysacct);
#endif

/*
 * Stubs for semaphore routines. sem.c
 */
#ifndef SEMSYS_MODULE
	MODULE(semsys,sys);
	WSTUB(semsys, semexit,		nomod_zero);
	END_MODULE(semsys);
#endif

/*
 * Stubs for shmem routines. shm.c
 */
#ifndef SHMSYS_MODULE
	MODULE(shmsys,sys);
	WSTUB(shmsys, shmexit,		nomod_zero);
	WSTUB(shmsys, shmfork,		nomod_zero);
	WSTUB(shmsys, shmgetid,		nomod_zero);
	END_MODULE(shmsys);
#endif

/*
 * Stubs for doors
 */
#ifndef DOORFS_MODULE
	MODULE(doorfs,sys);
	WSTUB(doorfs, door_slam,			nomod_zero);
	WSTUB(doorfs, door_exit,			nomod_zero);
	WSTUB(doorfs, door_fork,			nomod_zero);
	NO_UNLOAD_STUB(doorfs, door_get_activation,	nomod_zero);
	NO_UNLOAD_STUB(doorfs, door_release_activation,	nomod_zero);
	NO_UNLOAD_STUB(doorfs, door_upcall,		nomod_einval);
	NO_UNLOAD_STUB(doorfs, door_ki_create,		nomod_einval);
	NO_UNLOAD_STUB(doorfs, door_ki_open,		nomod_einval);
	WSTUB(doorfs, door_ki_upcall,			nomod_einval);
	WSTUB(doorfs, door_ki_hold,			nomod_zero);
	WSTUB(doorfs, door_ki_rele,			nomod_zero);
	WSTUB(doorfs, door_ki_info,			nomod_einval);
	END_MODULE(doorfs);
#endif

/*
 * Stubs for dma routines. dmaga.c
 * (These are only needed for cross-checks, not autoloading)
 */
#ifndef DMA_MODULE
	MODULE(dma,drv);
	WSTUB(dma, dma_alloc,		nomod_zero); /* (DMAGA *)0 */
	WSTUB(dma, dma_free,		nomod_zero); /* (DMAGA *)0 */
	END_MODULE(dma);
#endif

/*
 * Stubs for auditing.
 */
#ifndef C2AUDIT_MODULE
	MODULE(c2audit,sys);
	STUB(c2audit,  audit_init,			nomod_zero);
	STUB(c2audit,  _auditsys,			nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_free,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_start, 		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_finish,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_suser,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_newproc,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_pfree,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_thread_free,	nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_thread_create,	nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_falloc,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_unfalloc,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_closef,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_copen,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_core_start,	nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_core_finish,	nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_stropen,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_strclose,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_strioctl,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_strputmsg,	nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_c2_revoke,	nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_savepath,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_anchorpath,	nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_addcomponent,	nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_exit,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_exec,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_symlink,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_vncreate_start,	nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_vncreate_finish,	nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_enterprom,	nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_exitprom,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_chdirec,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_getf,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_setf,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_sock,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_strgetmsg,	nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_ipc,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_ipcget,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_lookupname,	nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_pathcomp,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_fdsend,		nomod_zero);
	NO_UNLOAD_STUB(c2audit, audit_fdrecv,		nomod_zero);
	END_MODULE(c2audit);
#endif

/*
 * Stubs for kernel rpc security service module
 */
#ifndef RPCSEC_MODULE
	MODULE(rpcsec,misc);
	NO_UNLOAD_STUB(rpcsec, sec_clnt_revoke,		nomod_zero);
	NO_UNLOAD_STUB(rpcsec, authkern_create,		nomod_zero);
	NO_UNLOAD_STUB(rpcsec, sec_svc_msg,		nomod_zero);
	NO_UNLOAD_STUB(rpcsec, sec_svc_control,		nomod_zero);
	END_MODULE(rpcsec);
#endif

/*
 * Stubs for rpc RPCSEC_GSS security service module
 */
#ifndef RPCSEC_GSS_MODULE
	MODULE(rpcsec_gss,misc);
	NO_UNLOAD_STUB(rpcsec_gss, __svcrpcsec_gss,		nomod_zero);
	NO_UNLOAD_STUB(rpcsec_gss, rpc_gss_set_svc_name,	nomod_zero);
	NO_UNLOAD_STUB(rpcsec_gss, rpc_gss_getcred,		nomod_zero);
	NO_UNLOAD_STUB(rpcsec_gss, rpc_gss_set_callback,	nomod_zero);
	NO_UNLOAD_STUB(rpcsec_gss, rpc_gss_secget,		nomod_zero);
	NO_UNLOAD_STUB(rpcsec_gss, rpc_gss_secfree,		nomod_zero);
	NO_UNLOAD_STUB(rpcsec_gss, rpc_gss_seccreate,		nomod_zero);
	NO_UNLOAD_STUB(rpcsec_gss, rpc_gss_set_defaults,	nomod_zero);
	NO_UNLOAD_STUB(rpcsec_gss, rpc_gss_revauth,		nomod_zero);
	NO_UNLOAD_STUB(rpcsec_gss, rpc_gss_secpurge,		nomod_zero);
	NO_UNLOAD_STUB(rpcsec_gss, rpc_gss_cleanup,		nomod_zero);
	NO_UNLOAD_STUB(rpcsec_gss, rpc_gss_get_versions,	nomod_zero);
	NO_UNLOAD_STUB(rpcsec_gss, rpc_gss_max_data_length,	nomod_zero);
	NO_UNLOAD_STUB(rpcsec_gss, rpc_gss_svc_max_data_length,	nomod_zero);
	END_MODULE(rpcsec_gss);
#endif

#ifndef SAD_MODULE
	MODULE(sad,drv);
	STUB(sad, sadinit, 0);
	STUB(sad, ap_free, 0);
	END_MODULE(sad);
#endif

#ifndef WC_MODULE
	MODULE(wc,drv);
	STUB(wc, wcvnget, 0);
	STUB(wc, wcvnrele, 0);
	END_MODULE(wc);
#endif

#ifndef IWSCN_MODULE
	MODULE(iwscn,drv);
	STUB(iwscn, srpop, 0);
	END_MODULE(iwscn);
#endif

/*
 * Stubs for checkpoint-resume module
 */
#ifndef CPR_MODULE
        MODULE(cpr,misc);
        STUB(cpr, cpr, 0);
        END_MODULE(cpr);
#endif

#ifdef	__sparcv9cpu
/*
 * Stubs for VIS module
 */
#ifndef VIS_MODULE
        MODULE(vis,misc);
        STUB(vis, vis_fpu_simulator, 0);
        STUB(vis, vis_fldst, 0);
        STUB(vis, vis_rdgsr, 0);
        STUB(vis, vis_wrgsr, 0);
        END_MODULE(vis);
#endif
#endif

/*
 * Stubs for kernel probes (tnf module).  Not unloadable.
 */
#ifndef TNF_MODULE
	MODULE(tnf,drv);
	NO_UNLOAD_STUB(tnf, tnf_ref32_1,	nomod_zero);
	NO_UNLOAD_STUB(tnf, tnf_string_1,	nomod_zero);
	NO_UNLOAD_STUB(tnf, tnf_opaque_array_1,	nomod_zero);
	NO_UNLOAD_STUB(tnf, tnf_struct_tag_1,	nomod_zero);
	NO_UNLOAD_STUB(tnf, tnf_allocate,	nomod_zero);
	END_MODULE(tnf);
#endif

/*
 * Clustering: stubs for bootstrapping.
 */
#ifndef CL_BOOTSTRAP
	MODULE(cl_bootstrap,misc);
	NO_UNLOAD_WSTUB(cl_bootstrap, clboot_modload, nomod_minus_one);
	NO_UNLOAD_WSTUB(cl_bootstrap, clboot_loadrootmodules, nomod_zero);
	NO_UNLOAD_WSTUB(cl_bootstrap, clboot_rootconf, nomod_zero);
	NO_UNLOAD_WSTUB(cl_bootstrap, clboot_mountroot, nomod_zero);
	NO_UNLOAD_WSTUB(cl_bootstrap, clconf_init, nomod_zero);
	NO_UNLOAD_WSTUB(cl_bootstrap, clconf_get_nodeid, nomod_zero);
	NO_UNLOAD_WSTUB(cl_bootstrap, clconf_maximum_nodeid, nomod_zero);
	NO_UNLOAD_WSTUB(cl_bootstrap, cluster, nomod_zero);
	END_MODULE(cl_bootstrap);
#endif

/*
 * Clustering: stubs for cluster infrastructure.
 */	
#ifndef CL_COMM_MODULE
	MODULE(cl_comm,misc);
	NO_UNLOAD_STUB(cl_comm, cladmin, nomod_minus_one);
	END_MODULE(cl_comm);
#endif

/*
 * Clustering: stubs for global file system operations.
 */
#ifndef PXFS_MODULE
	MODULE(pxfs,fs);
	NO_UNLOAD_WSTUB(pxfs, clpxfs_aio_read, nomod_zero);
	NO_UNLOAD_WSTUB(pxfs, clpxfs_aio_write, nomod_zero);
	NO_UNLOAD_WSTUB(pxfs, cl_flk_state_transition_notify, nomod_zero);
	END_MODULE(pxfs);
#endif

! this is just a marker for the area of text that contains stubs
	.seg ".text"
	.global stubs_end
stubs_end:
	nop

#endif	/* lint */
