/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_REGSET_H
#define	_SYS_REGSET_H

#pragma ident	"@(#)regset.h	1.24	99/10/05 SMI"	/* SVr4.0 1.1	*/

#include <sys/feature_tests.h>

#if !defined(_ASM)
#include <sys/int_types.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Location of the users' stored registers relative to R0.
 * Usage is as an index into a gregset_t array or as u.u_ar0[XX].
 */
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

#if defined(__sparcv9)
#define	REG_CCR	(0)
#if defined(__sparcv9cpu)
#define	REG_PSR	(0)
#endif
#else
#define	REG_PSR	(0)
#endif	/* __sparcv9 */

#define	REG_PC	(1)
#define	REG_nPC	(2)
#define	REG_Y	(3)
#define	REG_G1	(4)
#define	REG_G2	(5)
#define	REG_G3	(6)
#define	REG_G4	(7)
#define	REG_G5	(8)
#define	REG_G6	(9)
#define	REG_G7	(10)
#define	REG_O0	(11)
#define	REG_O1	(12)
#define	REG_O2	(13)
#define	REG_O3	(14)
#define	REG_O4	(15)
#define	REG_O5	(16)
#define	REG_O6	(17)
#define	REG_O7	(18)
#if defined(__sparcv9)
#define	REG_ASI	(19)
#define	REG_FPRS (20)
#endif	/* __sparcv9 */

/* the following defines are for portability */
#if !defined(__sparcv9)
#define	REG_PS	REG_PSR
#endif	/* __sparcv9 */
#define	REG_SP	REG_O6
#define	REG_R0	REG_O0
#define	REG_R1	REG_O1
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#ifndef	_ASM

#if defined(_LP64) || defined(_I32LPx)
typedef long	greg_t;
#else
typedef int	greg_t;
#endif

#if defined(_SYSCALL32)

typedef int32_t greg32_t;
typedef int64_t greg64_t;

#endif	/* _SYSCALL32 */

/*
 * A gregset_t is defined as an array type for compatibility with the reference
 * source. This is important due to differences in the way the C language
 * treats arrays and structures as parameters.
 *
 * Note that NGREG is really (sizeof (struct regs) / sizeof (greg_t)),
 * but that the SPARC V8 ABI defines it absolutely to be 19.
 */
#if defined(__sparcv9)
#define	_NGREG	21
#else	/* __sparcv9 */
#define	_NGREG	19
#endif	/* __sparcv9 */
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	NGREG	_NGREG
#endif

typedef greg_t	gregset_t[_NGREG];

#if defined(_SYSCALL32)

#define	_NGREG32	19
#define	_NGREG64	21

typedef	greg32_t gregset32_t[_NGREG32];
typedef greg64_t gregset64_t[_NGREG64];

#endif	/* _SYSCALL32 */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
/*
 * The following structures define how a register window can appear on the
 * stack. This structure is available (when required) through the `gwins'
 * field of an mcontext (nested within ucontext). SPARC_MAXWINDOW is the
 * maximum number of outstanding regiters window defined in the SPARC
 * architecture (*not* implementation).
 */
#define	SPARC_MAXREGWINDOW	31	/* max windows in SPARC arch. */

struct rwindow {
	greg_t	rw_local[8];		/* locals */
	greg_t	rw_in[8];		/* ins */
};

#if defined(_SYSCALL32)

struct rwindow32 {
	greg32_t rw_local[8];		/* locals */
	greg32_t rw_in[8];		/* ins */
};

struct rwindow64 {
	greg64_t rw_local[8];		/* locals */
	greg64_t rw_in[8];		/* ins */
};

#if defined(_KERNEL)
extern	void	rwindow_nto32(struct rwindow *, struct rwindow32 *);
extern	void	rwindow_32ton(struct rwindow32 *, struct rwindow *);
#endif

#endif	/* _SYSCALL32 */

#define	rw_fp	rw_in[6]		/* frame pointer */
#define	rw_rtn	rw_in[7]		/* return address */

typedef struct gwindows {
	int		wbcnt;
	greg_t		*spbuf[SPARC_MAXREGWINDOW];
	struct rwindow	wbuf[SPARC_MAXREGWINDOW];
} gwindows_t;

#if defined(_SYSCALL32)

typedef struct gwindows32 {
	int32_t		wbcnt;
	caddr32_t	spbuf[SPARC_MAXREGWINDOW];
	struct rwindow32 wbuf[SPARC_MAXREGWINDOW];
} gwindows32_t;

typedef struct gwindows64 {
	int		wbcnt;
	greg64_t	*spbuf[SPARC_MAXREGWINDOW];
	struct rwindow64 wbuf[SPARC_MAXREGWINDOW];
} gwindows64_t;

#endif	/* _SYSCALL32 */


/*
 * Floating point definitions.
 */

#define	MAXFPQ	16	/* max # of fpu queue entries currently supported */

/*
 * struct fq defines the minimal format of a floating point instruction queue
 * entry. The size of entries in the floating point queue are implementation
 * dependent. The union FQu is guarenteed to be the first field in any ABI
 * conformant system implementation. Any additional fields provided by an
 * implementation should not be used applications designed to be ABI conformant.
 */

struct fpq {
	unsigned int *fpq_addr;		/* address */
	unsigned int fpq_instr;		/* instruction */
};

struct fq {
	union {				/* FPU inst/addr queue */
		double whole;
		struct fpq fpq;
	} FQu;
};

#if defined(_SYSCALL32)

struct fpq32 {
	caddr32_t	fpq_addr;	/* address */
	uint32_t	fpq_instr;	/* instruction */
};

struct fq32 {
	union {				/* FPU inst/addr queue */
		double whole;
		struct fpq32 fpq;
	} FQu;
};

#endif	/* _SYSCALL32 */

/*
 * struct fpu is the floating point processor state. struct fpu is the sum
 * total of all possible floating point state which includes the state of
 * external floating point hardware, fpa registers, etc..., if it exists.
 *
 * A floating point instuction queue may or may not be associated with
 * the floating point processor state. If a queue does exist, the field
 * fpu_q will point to an array of fpu_qcnt entries where each entry is
 * fpu_q_entrysize long. fpu_q_entry has a lower bound of sizeof (union FQu)
 * and no upper bound. If no floating point queue entries are associated
 * with the processor state, fpu_qcnt will be zeo and fpu_q will be NULL.
 */

#define	FPU_REGS_TYPE		uint32_t
#define	FPU_DREGS_TYPE		uint64_t
#define	V7_FPU_FSR_TYPE		uint32_t
#define	V9_FPU_FSR_TYPE		uint64_t
#define	V9_FPU_FPRS_TYPE	uint32_t

#ifdef	__sparcv9cpu
#define	FPU_FSR_TYPE		V9_FPU_FSR_TYPE
#else
#define	FPU_FSR_TYPE		V7_FPU_FSR_TYPE
#endif

#if defined(__sparcv9)

struct fpu {
	union {					/* FPU floating point regs */
		FPU_REGS_TYPE	fpu_regs[32];	/* 32 singles */
		double		fpu_dregs[32];	/* 32 doubles */
		long double	fpu_qregs[16];	/* 16 quads */
	} fpu_fr;
	struct fq	*fpu_q;			/* ptr to array of FQ entries */
	V9_FPU_FSR_TYPE	fpu_fsr;		/* FPU status register */
	uint8_t		fpu_qcnt;		/* # of entries in saved FQ */
	uint8_t		fpu_q_entrysize;	/* # of bytes per FQ entry */
	uint8_t		fpu_en;			/* flag specifying fpu in use */
};

#else	/* __sparcv9 */

struct fpu {
	union {					/* FPU floating point regs */
		FPU_REGS_TYPE	fpu_regs[32];	/* 32 singles */
		double		fpu_dregs[16];	/* 16 doubles */
	} fpu_fr;
	struct fq	*fpu_q;			/* ptr to array of FQ entries */
	V7_FPU_FSR_TYPE	fpu_fsr;		/* FPU status register */
	uint8_t		fpu_qcnt;		/* # of entries in saved FQ */
	uint8_t		fpu_q_entrysize;	/* # of bytes per FQ entry */
	uint8_t		fpu_en;			/* flag signifying fpu in use */
};

#endif	/* __sparcv9 */

typedef struct fpu	fpregset_t;

#if defined(_SYSCALL32)

/* Kernel view of user sparcv7/v8 fpu structure */

struct fpu32 {
	union {					/* FPU floating point regs */
		uint32_t	fpu_regs[32];	/* 32 singles */
		double		fpu_dregs[16];	/* 16 doubles */
	} fpu_fr;
	caddr32_t	fpu_q;			/* ptr to array of FQ entries */
	uint32_t	fpu_fsr;		/* FPU status register */
	uint8_t		fpu_qcnt;		/* # of entries in saved FQ */
	uint8_t		fpu_q_entrysize;	/* # of bytes per FQ entry */
	uint8_t		fpu_en;			/* flag signifying fpu in use */
};

typedef struct fpu32	fpregset32_t;

#endif	/* _SYSCALL32 */

#ifdef __sparcv9cpu
/*
 * struct v9_fpu contains the extra V9 floating point processor state.
 * This is a separate definition because struct fpu is defined by the
 * V8 ABI. Note that the fprs could be 64 bits, but that seems excessive
 * considering its current V9 definition of 3 bits.
 */

struct v9_fpu {
	union _fpu_fr {				/* V9 FPU floating point regs */
		FPU_REGS_TYPE	fpu_regs[32];	/* 32 singles */
		FPU_DREGS_TYPE	fpu_dregs[32];	/* 32 doubles */
		long double	fpu_qregs[16];	/* 16 quads */
	} fpu_fr;
	V9_FPU_FSR_TYPE	fpu_fsr;		/* FPU status register */
	V9_FPU_FPRS_TYPE fpu_fprs;		/* fprs register */
	struct fq	*fpu_q;
	uint8_t		fpu_qcnt;
	uint8_t		fpu_q_entrysize;
	uint8_t		fpu_en;			/* flag signifying fpu in use */
};

typedef struct v9_fpu	v9_fpregset_t;

#endif
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#ifdef	_KERNEL
/*
 * The abi uses struct fpu, so we use this to describe the kernel's
 * view of the fpu, which changes from v7 to v9.
 */
#ifdef	__sparcv9cpu
typedef	struct v9_fpu	kfpu_t;
#else
typedef	struct fpu	kfpu_t;
#endif
#endif


#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
/*
 * The following structure is for associating extra register state with
 * the ucontext structure and is kept within the uc_mcontext filler area.
 *
 * If (xrs_id == XRS_ID) then the xrs_ptr field is a valid pointer to
 * extra register state. The exact format of the extra register state
 * pointed to by xrs_ptr is platform-dependent.
 *
 * Note: a platform may or may not manage extra register state.
 */
typedef struct {
	unsigned int	xrs_id;		/* indicates xrs_ptr validity */
	caddr_t		xrs_ptr;	/* ptr to extra reg state */
} xrs_t;

#define	XRS_ID			0x78727300	/* the string "xrs" */

#if defined(_SYSCALL32)

typedef	struct {
	uint32_t	xrs_id;		/* indicates xrs_ptr validity */
	caddr32_t	xrs_ptr;	/* ptr to extra reg state */
} xrs32_t;

#endif	/* _SYSCALL32 */

#if defined(__sparcv9)

/*
 * Ancillary State Registers
 *
 * The SPARC V9 architecture defines 25 ASRs, numbered from 7 through 31.
 * ASRs 16 through 31 are available to user programs, though the meaning
 * and content of these registers is implementation dependent.
 */
typedef	int64_t	asrset_t[16];	/* %asr16 - > %asr31 */

#endif	/* __sparcv9 */

/*
 * Structure mcontext defines the complete hardware machine state. If
 * the field `gwins' is non NULL, it points to a save area for register
 * window frames. If `gwins' is NULL, the register windows were saved
 * on the user's stack.
 *
 * The filler of 21 longs is historical (now filler[19] plus the xrs_t
 * field). The value was selected to provide binary compatibility with
 * statically linked ICL binaries. It is in the ABI (do not change). It
 * actually appears in the ABI as a single filler of 44 is in the field
 * uc_filler of struct ucontext. It is split here so that ucontext.h can
 * (hopefully) remain architecture independent.
 *
 * Note that 2 longs of the filler are used to hold extra register state info.
 */
typedef struct {
	gregset_t	gregs;	/* general register set */
	gwindows_t	*gwins;	/* POSSIBLE pointer to register windows */
	fpregset_t	fpregs;	/* floating point register set */
	xrs_t		xrs;	/* POSSIBLE extra register state association */
#if defined(__sparcv9)
	asrset_t	asrs;		/* ancillary registers */
	long		filler[4];	/* room for expansion */
#else	/* __sparcv9 */
	long		filler[19];
#endif	/* __sparcv9 */
} mcontext_t;

#if defined(_SYSCALL32)

typedef struct {
	gregset32_t	gregs;	/* general register set */
	caddr32_t	gwins;	/* POSSIBLE pointer to register windows */
	fpregset32_t	fpregs;	/* floating point register set */
	xrs32_t		xrs;	/* POSSIBLE extra register state association */
	int32_t		filler[19];
} mcontext32_t;

#endif /* _SYSCALL32 */

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */
#endif	/* _ASM */

#if !defined(__sparcv9)
#if !defined(_KERNEL) && !defined(_XPG4_2) || defined(__EXTENSIONS__)
#include <v7/sys/privregs.h>
#endif	/* !_KERNEL && !_XPG4_2 || __EXTENSIONS__ */
#endif	/* __sparcv9 */

/*
 * The following is here for XPG4.2 standards compliance.
 * regset.h is included in ucontext.h for the definition of
 * mcontext_t, all of which breaks XPG4.2 namespace.
 */

#if defined(_XPG4_2) && !defined(__EXTENSIONS__)
/*
 * The following is here for UNIX 95 compliance (XPG Issue 4, Version 2
 * System Interfaces and Headers. The structures included here are identical
 * to those visible elsewhere in this header except that the structure
 * element names have been changed in accordance with the X/Open namespace
 * rules.  Specifically, depending on the name and scope, the names have
 * been prepended with a single or double underscore (_ or __).  See the
 * structure definitions in the non-X/Open namespace for more detailed
 * comments describing each of these structures.
 */

#ifndef	_ASM

/*
 * The following structures define how a register window can appear on the
 * stack.
 */
#define	_SPARC_MAXREGWINDOW	31		/* max windows in SPARC arch. */

struct	__rwindow {
	greg_t	__rw_local[8];		/* locals */
	greg_t	__rw_in[8];		/* ins */
};

#define	__rw_fp		__rw_in[6]		/* frame pointer */
#define	__rw_rtn	__rw_in[7]		/* return address */

struct __gwindows {
	int		__wbcnt;
	greg_t		*__spbuf[_SPARC_MAXREGWINDOW];
	struct __rwindow	__wbuf[_SPARC_MAXREGWINDOW];
};

typedef struct __gwindows	gwindows_t;

/*
 * The fq structure defines the minimal format of a floating point
 * instruction queue entry.
 */

struct __fpq {
	unsigned int *__fpq_addr;	/* address */
	unsigned int __fpq_instr;	/* instruction */
};

struct __fq {
	union {				/* FPU inst/addr queue */
		double __whole;
		struct __fpq __fpq;
	} _FQu;
};

/*
 * The fpu structure is the floating point processor state.
 */

#define	_FPU_REGS_TYPE		uint32_t
#define	_FPU_DREGS_TYPE		uint64_t
#define	_V7_FPU_FSR_TYPE	uint32_t
#define	_V9_FPU_FSR_TYPE	uint64_t
#define	_V9_FPU_FPRS_TYPE	uint32_t

#ifdef	__sparcv9cpu
#define	_FPU_FSR_TYPE		_V9_FPU_FSR_TYPE
#else
#define	_FPU_FSR_TYPE		_V7_FPU_FSR_TYPE
#endif

#if defined(__sparcv9)

/*
 * SPARC Version 9 floating point
 */

struct __fpu {
	union {					/* FPU floating point regs */
		_FPU_REGS_TYPE	__fpu_regs[32];		/* 32 singles */
		double		__fpu_dregs[32];	/* 32 doubles */
		long double	__fpu_qregs[16];	/* 16 quads */
	} __fpu_fr;
	struct __fq	*__fpu_q;		/* ptr to array of FQ entries */
	_V9_FPU_FSR_TYPE	__fpu_fsr;	/* FPU status register */
	uint8_t		__fpu_qcnt;		/* # of entries in saved FQ */
	uint8_t		__fpu_q_entrysize;	/* # of bytes per FQ entry */
	uint8_t		__fpu_en;		/* flag signifying fpu in use */
};

#else	/* __sparcv9 */

/*
 * SPARC Version 7 and 8 floating point
 */

struct __fpu {
	union {					/* FPU floating point regs */
		_FPU_REGS_TYPE	__fpu_regs[32];		/* 32 singles */
		double		__fpu_dregs[16];	/* 16 doubles */
	} __fpu_fr;
	struct __fq	*__fpu_q;		/* ptr to array of FQ entries */
	_V7_FPU_FSR_TYPE	__fpu_fsr;	/* FPU status register */
	uint8_t		__fpu_qcnt;		/* # of entries in saved FQ */
	uint8_t		__fpu_q_entrysize;	/* # of bytes per FQ entry */
	uint8_t		__fpu_en;		/* flag signifying fpu in use */
};

#endif	/* __sparcv9 */

typedef struct __fpu	fpregset_t;

#if defined(__sparcv9cpu) && !defined(__sparcv9)

/*
 * The v9_fpu structure contains the extra V9 floating point processor state.
 */

struct __v9_fpu {
	union {				/* V9 FPU floating point regs */
		_FPU_REGS_TYPE	__fpu_regs[32];		/* 32 singles */
		_FPU_DREGS_TYPE	__fpu_dregs[32];	/* 32 doubles */
		long double	__fpu_qregs[16];	/* 16 quads */
	} __fpu_fr;
	_V9_FPU_FSR_TYPE	__fpu_fsr;	/* FPU status register */
	_V9_FPU_FPRS_TYPE	__fpu_fprs;	/* fprs register */
	struct __fq	*__fpu_q;
	uint8_t		__fpu_qcnt;
	uint8_t		__fpu_q_entrysize;
	uint8_t		__fpu_en;		/* flag signifying fpu in use */
};

typedef struct __v9_fpu	v9_fpregset_t;

#endif	/* __sparcv9cpu && !__sparcv9 */

/*
 * The xrs_t structure is for associating extra register state with
 * the ucontext structure and is kept within the uc_mcontext filler area.
 */
typedef struct {
	unsigned int	__xrs_id;	/* indicates xrs_ptr validity */
	caddr_t		__xrs_ptr;	/* ptr to extra reg state */
} xrs_t;

#define	_XRS_ID			0x78727300	/* the string "xrs" */

#if defined(__sparcv9)

/*
 * Ancillary State Registers
 *
 * The SPARC V9 architecture defines 25 ASRs, numbered from 7 through 31.
 * ASRs 16 through 31 are available to user programs, though the meaning
 * and content of these registers is implementation dependent.
 */
typedef	int64_t	asrset_t[16];	/* %asr16 - > %asr31 */

#endif	/* __sparcv9 */

/*
 * Structure mcontext defines the complete hardware machine state.
 */
typedef struct {
	gregset_t	__gregs; /* general register set */
	gwindows_t	*__gwins; /* POSSIBLE pointer to register windows */
	fpregset_t	__fpregs; /* floating point register set */
	xrs_t		__xrs;	/* POSSIBLE extra register state association */
#if defined(__sparcv9)
	asrset_t	__asrs;		/* ancillary registers */
	long		__filler[4];	/* room for expansion */
#else	/* __sparcv9 */
	long		__filler[19];
#endif	/* __sparcv9 */
} mcontext_t;

#endif	/* _ASM */
#endif /* defined(_XPG4_2) && !defined(__EXTENSIONS__) */


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_REGSET_H */
