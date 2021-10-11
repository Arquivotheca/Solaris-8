/*
 * Copyright (c) 1995-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)simulator.c	1.37	98/02/09 SMI"

/* common code with bug fixes from original version in trap.c */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/vmsystm.h>
#include <sys/fpu/fpusystm.h>
#include <sys/inline.h>
#include <sys/debug.h>
#include <sys/privregs.h>
#include <sys/machpcb.h>
#include <sys/simulate.h>
#include <sys/proc.h>
#include <sys/cmn_err.h>
#include <sys/stack.h>
#include <sys/watchpoint.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/mman.h>
#include <sys/asi.h>
#include <sys/copyops.h>
#include <vm/as.h>
#include <vm/page.h>
#include <sys/model.h>

static int aligndebug = 0;

/*
 * For the sake of those who must be compatible with unaligned
 * architectures, users can link their programs to use a
 * corrective trap handler that will fix unaligned references
 * a special trap #6 (T_FIX_ALIGN) enables this 'feature'.
 * Returns 1 for success, 0 for failure.
 */

int
do_unaligned(struct regs *rp, caddr_t *badaddr)
{
	u_int	inst, op3, asi = 0;
	u_int	rd, rs1, rs2;
	int	sz, nf = 0, ltlend = 0;
	int	floatflg;
	int	fsrflg;
	int	immflg;
	int	lddstdflg;
	caddr_t	addr;
	uint64_t val;
	union {
		uint64_t	l[2];
		uint32_t	i[4];
		uint16_t	s[8];
		uint8_t		c[16];
	} data;

	ASSERT(USERMODE(rp->r_tstate));
	inst = fetch_user_instr((caddr_t)rp->r_pc);

	op3 = (inst >> 19) & 0x3f;
	rd = (inst >> 25) & 0x1f;
	rs1 = (inst >> 14) & 0x1f;
	rs2 = inst & 0x1f;
	floatflg = (inst >> 24) & 1;
	immflg = (inst >> 13) & 1;
	lddstdflg = fsrflg = 0;

	/* if not load or store do nothing */
	if ((inst >> 30) != 3)
		return (0);

	/* if ldstub or swap, do nothing */
	if ((inst & 0xc1680000) == 0xc0680000)
		return (0);

	if (floatflg) {
		switch ((inst >> 19) & 3) {	/* map size bits to a number */
		case 0: sz = 4; break;		/* ldf{a}/stf{a} */
		case 1: fsrflg = 1;
			if (rd == 0)
				sz = 4;		/* ldfsr/stfsr */
			else
				sz = 8;		/* ldxfsr/stxfsr */
			break;
		case 2: sz = 16; break;		/* ldqf{a}/stqf{a} */
		case 3: sz = 8; break;		/* lddf{a}/stdf{a} */
		}
		/*
		 * Fix to access extra double register encoding plus
		 * compensate to access the correct fpu_dreg.
		 */
		if ((sz > 4) && (fsrflg == 0)) {
			if ((rd & 1) == 1)
				rd = (rd & 0x1e) | 0x20;
			rd = rd >> 1;
		}
	} else {
		int sz_bits = (inst >> 19) & 0xf;
		switch (sz_bits) {		/* map size bits to a number */
		case 0:				/* lduw{a} */
		case 4:				/* stw{a} */
		case 8:				/* ldsw{a} */
		case 0xf:			/* swap */
			sz = 4; break;
		case 1:				/* ldub{a} */
		case 5:				/* stb{a} */
		case 9:				/* ldsb{a} */
		case 0xd:			/* ldstub */
			sz = 1; break;
		case 2:				/* lduh{a} */
		case 6:				/* sth{a} */
		case 0xa:			/* ldsh{a} */
			sz = 2; break;
		case 3:				/* ldd{a} */
		case 7:				/* std{a} */
			lddstdflg = 1;
			sz = 8; break;
		case 0xb:			/* ldx{a} */
		case 0xe:			/* stx{a} */
			sz = 8; break;
		}
	}


	/* only support primary and secondary asi's */
	if ((op3 >> 4) & 1) {
		if (immflg) {
			asi = (u_int)(rp->r_tstate >> TSTATE_ASI_SHIFT) &
					TSTATE_ASI_MASK;
		} else {
			asi = (inst >> 5) & 0xff;
		}
		switch (asi) {
		case ASI_P:
		case ASI_S:
			break;
		case ASI_PNF:
		case ASI_SNF:
			nf = 1;
			break;
		case ASI_PL:
		case ASI_SL:
			ltlend = 1;
			break;
		case ASI_PNFL:
		case ASI_SNFL:
			ltlend = 1;
			nf = 1;
			break;
		default:
			return (0);
		}
		/*
		 * Non-faulting stores generate a data_access_exception trap,
		 * according to the Spitfire manual, which should be signaled
		 * as an illegal instruction trap, because it can't be fixed.
		 */
		if ((nf) && ((op3 == IOP_V8_STQFA) || (op3 == IOP_V8_STDFA)))
			return (SIMU_ILLEGAL);
	}

	if (aligndebug) {
		printf("unaligned access at %p, instruction: 0x%x\n",
		    (void *)rp->r_pc, inst);
		printf("type %s", (((inst >> 21) & 1) ? "st" : "ld"));
		if (((inst >> 21) & 1) == 0)
		    printf(" %s", (((inst >> 22) & 1) ? "signed" : "unsigned"));
		printf(" asi 0x%x size %d immflg %d\n", asi, sz, immflg);
		printf("rd = %d, op3 = 0x%x, rs1 = %d, rs2 = %d, imm13=0x%x\n",
			rd, op3, rs1, rs2, (inst & 0x1fff));
	}

	(void) flush_user_windows_to_stack(NULL);
	if (getreg(rp, rs1, &val, badaddr))
		return (SIMU_FAULT);
	addr = (caddr_t)val;		/* convert to 32/64 bit address */
	if (aligndebug)
		printf("addr 1 = %p\n", (void *)addr);

	/* check immediate bit and use immediate field or reg (rs2) */
	if (immflg) {
		int imm;
		imm  = inst & 0x1fff;		/* mask out immediate field */
		imm <<= 19;			/* sign extend it */
		imm >>= 19;
		addr += imm;			/* compute address */
	} else {
		if (getreg(rp, rs2, &val, badaddr))
			return (SIMU_FAULT);
		addr += val;
	}

	/*
	 * If this is a 32-bit program, chop the address accordingly.
	 */
	if (curproc->p_model == DATAMODEL_ILP32)
		addr = (caddr_t)(caddr32_t)addr;

	if (aligndebug)
		printf("addr 2 = %p\n", (void *)addr);

	if (addr >= curproc->p_as->a_userlimit) {
		*badaddr = addr;
		goto badret;
	}

	/* a single bit differentiates ld and st */
	if ((inst >> 21) & 1) {			/* store */
		if (floatflg) {
			/* if fpu_exists read fpu reg */
			if (fpu_exists) {
				if (fsrflg) {
					_fp_read_pfsr(&data.l[0]);
				} else {
					if (sz == 4) {
						data.i[0] = 0;
						_fp_read_pfreg(
						    (unsigned *)&data.i[1], rd);
					}
					if (sz >= 8)
						_fp_read_pdreg(
							&data.l[0], rd);
					if (sz == 16)
						_fp_read_pdreg(
							&data.l[1], rd+1);
				}
			} else {
				klwp_id_t lwp = ttolwp(curthread);
				kfpu_t *fp = lwptofpu(lwp);
				if (fsrflg) {
					/* Clear reserved bits, set version=7 */
					fp->fpu_fsr &= ~0x30301000;
					fp->fpu_fsr |= 0xE0000;
					data.l[0] = fp->fpu_fsr;
				} else {
					if (sz == 4) {
						data.i[0] = 0;
						data.i[1] =
					    (unsigned)fp->fpu_fr.fpu_regs[rd];
					}
					if (sz >= 8)
						data.l[0] =
						    fp->fpu_fr.fpu_dregs[rd];
					if (sz == 16)
						data.l[1] =
						    fp->fpu_fr.fpu_dregs[rd+1];
				}
			}
		} else {
			if (lddstdflg) {
				if (getreg(rp, rd, &data.l[0], badaddr))
					return (SIMU_FAULT);
				if (getreg(rp, rd+1, &data.l[1], badaddr))
					return (SIMU_FAULT);
				data.i[0] = data.i[1];	/* combine the data */
				data.i[1] = data.i[3];
			} else {
				if (getreg(rp, rd, &data.l[0], badaddr))
					return (SIMU_FAULT);
			}
		}

		if (aligndebug) {
			if (sz == 16) {
				printf("data %x %x %x %x\n",
				    data.i[0], data.i[1], data.i[2], data.c[3]);
			} else {
				printf("data %x %x %x %x %x %x %x %x\n",
				    data.c[0], data.c[1], data.c[2], data.c[3],
				    data.c[4], data.c[5], data.c[6], data.c[7]);
			}
		}

		if (ltlend) {
			if (sz == 1) {
				if (xcopyout_little(&data.c[7], addr,
				    (u_int)sz) != 0)
					goto badret;
			} else if (sz == 2) {
				if (xcopyout_little(&data.s[3], addr,
				    (u_int)sz) != 0)
					goto badret;
			} else if (sz == 4) {
				if (xcopyout_little(&data.i[1], addr,
				    (u_int)sz) != 0)
					goto badret;
			} else {
				if (xcopyout_little(&data.l[0], addr,
				    (u_int)sz) != 0)
					goto badret;
			}
		} else {
			if (sz == 1) {
				if (copyout(&data.c[7], addr, (u_int)sz) == -1)
				goto badret;
			} else if (sz == 2) {
				if (copyout(&data.s[3], addr, (u_int)sz) == -1)
				goto badret;
			} else if (sz == 4) {
				if (copyout(&data.i[1], addr, (u_int)sz) == -1)
				goto badret;
			} else {
				if (copyout(&data.l[0], addr, (u_int)sz) == -1)
				goto badret;
			}
		}
	} else {				/* load */
		if (sz == 1) {
			if (ltlend) {
				if (xcopyin_little(addr, &data.c[7],
				    (u_int)sz) != 0) {
					if (nf)
						data.c[7] = 0;
					else
						goto badret;
				}
			} else {
				if (copyin(addr, &data.c[7], (u_int)sz) == -1) {
					if (nf)
						data.c[7] = 0;
					else
						goto badret;
				}
			}
			/* if signed and the sign bit is set extend it */
			if (((inst >> 22) & 1) && ((data.c[7] >> 7) & 1)) {
				data.i[0] = (u_int)-1;	/* extend sign bit */
				data.s[2] = (u_short)-1;
				data.c[6] = (u_char)-1;
			} else {
				data.i[0] = 0;	/* clear upper 32+24 bits */
				data.s[2] = 0;
				data.c[6] = 0;
			}
		} else if (sz == 2) {
			if (ltlend) {
				if (xcopyin_little(addr, &data.s[3],
				    (u_int)sz) != 0) {
					if (nf)
						data.s[3] = 0;
					else
						goto badret;
				}
			} else {
				if (copyin(addr, &data.s[3], (u_int)sz) == -1) {
					if (nf)
						data.s[3] = 0;
					else
						goto badret;
				}
			}
			/* if signed and the sign bit is set extend it */
			if (((inst >> 22) & 1) && ((data.s[3] >> 15) & 1)) {
				data.i[0] = (u_int)-1;	/* extend sign bit */
				data.s[2] = (u_short)-1;
			} else {
				data.i[0] = 0;	/* clear upper 32+16 bits */
				data.s[2] = 0;
			}
		} else if (sz == 4) {
			if (ltlend) {
				if (xcopyin_little(addr, &data.i[1],
				    (u_int)sz) != 0) {
					if (nf)
						data.i[1] = 0;
					else
						goto badret;
				}
			} else {
				if (copyin(addr, &data.i[1], (u_int)sz) == -1) {
					if (nf)
						data.i[1] = 0;
					else
						goto badret;
				}
			}
			/* if signed and the sign bit is set extend it */
			if (((inst >> 22) & 1) && ((data.i[1] >> 31) & 1)) {
				data.i[0] = (u_int)-1;	/* extend sign bit */
			} else {
				data.i[0] = 0;	/* clear upper 32 bits */
			}
		} else {
			if (ltlend) {
				if (xcopyin_little(addr, &data.l[0],
				    (u_int)sz) != 0) {
					if (nf)
						data.l[0] = 0;
					else
						goto badret;
				}
			} else {
				if (copyin(addr, &data.l[0], (u_int)sz) == -1) {
					if (nf)
						data.l[0] = 0;
					else
						goto badret;
				}
			}
		}

		if (aligndebug) {
			if (sz == 16) {
				printf("data %x %x %x %x\n",
				    data.i[0], data.i[1], data.i[2], data.c[3]);
			} else {
				printf("data %x %x %x %x %x %x %x %x\n",
				    data.c[0], data.c[1], data.c[2], data.c[3],
				    data.c[4], data.c[5], data.c[6], data.c[7]);
			}
		}

		if (floatflg) {		/* if fpu_exists write fpu reg */
			if (fpu_exists) {
				if (fsrflg) {
					_fp_write_pfsr(&data.l[0]);
				} else {
					if (sz == 4)
						_fp_write_pfreg(
						    (unsigned *)&data.i[1], rd);
					if (sz >= 8)
						_fp_write_pdreg(
							&data.l[0], rd);
					if (sz == 16)
						_fp_write_pdreg(
							&data.l[1], rd+1);
				}
			} else {
				klwp_id_t lwp = ttolwp(curthread);
				kfpu_t *fp = lwptofpu(lwp);
				if (fsrflg) {
					fp->fpu_fsr = data.l[0];
				} else {
					if (sz == 4)
						fp->fpu_fr.fpu_regs[rd] =
							(unsigned)data.i[1];
					if (sz >= 8)
						fp->fpu_fr.fpu_dregs[rd] =
							data.l[0];
					if (sz == 16)
						fp->fpu_fr.fpu_dregs[rd+1] =
							data.l[1];
				}
			}
		} else {
			if (lddstdflg) {		/* split the data */
				data.i[2] = 0;
				data.i[3] = data.i[1];
				data.i[1] = data.i[0];
				data.i[0] = 0;
				if (putreg(&data.l[0], rp, rd, badaddr) == -1)
					goto badret;
				if (putreg(&data.l[1], rp, rd+1, badaddr) == -1)
					goto badret;
			} else {
				if (putreg(&data.l[0], rp, rd, badaddr) == -1)
					goto badret;
			}
		}
	}
	return (SIMU_SUCCESS);
badret:
	return (SIMU_FAULT);
}

/*
 * simulate popc
 */
static int
simulate_popc(struct regs *rp, caddr_t *badaddr, u_int inst)
{
	u_int	rd, rs1, rs2;
	u_int	immflg;
	uint64_t val, cnt = 0;

	rd = (inst >> 25) & 0x1f;
	rs1 = (inst >> 14) & 0x1f;
	rs2 = inst & 0x1f;
	immflg = (inst >> 13) & 1;

	(void) flush_user_windows_to_stack(NULL);
	if (getreg(rp, rs1, &val, badaddr))
		return (SIMU_FAULT);

	/* check immediate bit and use immediate field or reg (rs2) */
	if (immflg) {
		int imm;
		imm  = inst & 0x1fff;		/* mask out immediate field */
		imm <<= 19;			/* sign extend it */
		imm >>= 19;
		if (imm != 0) {
			for (cnt = 0; imm != 0; imm &= imm-1)
				cnt++;
		}
	} else {
		if (getreg(rp, rs2, &val, badaddr))
			return (SIMU_FAULT);
		if (val != 0) {
			for (cnt = 0; val != 0; val &= val-1)
				cnt++;
		}
	}

	if (putreg(&cnt, rp, rd, badaddr) == -1)
		return (SIMU_FAULT);

	return (SIMU_SUCCESS);
}

/*
 * simulate unimplemented instructions (popc, ldqf{a}, stqf{a})
 */
int
simulate_unimp(struct regs *rp, caddr_t *badaddr)
{
	u_int	inst, optype, op3;
	u_int	rs1, rd;
	u_int	ignor, i;
	machpcb_t *mpcb = lwptompcb(ttolwp(curthread));
	int	nomatch = 0;
	caddr_t	addr = (caddr_t)rp->r_pc;
	struct as *as;
	caddr_t	ka;
	pfn_t	pfnum;
	page_t *pp;
	proc_t *p = ttoproc(curthread);

	ASSERT(USERMODE(rp->r_tstate));
	inst = fetch_user_instr(addr);
	if (inst == (u_int)-1) {
		mpcb->mpcb_illexcaddr = addr;
		mpcb->mpcb_illexcinsn = (uint32_t)-1;
		return (SIMU_ILLEGAL);
	}

	/*
	 * When fixing dirty v8 instructions there's a race if two processors
	 * are executing the dirty executable at the same time.  If one
	 * cleans the instruction as the other is executing it the second
	 * processor will see a clean instruction when it comes through this
	 * code and will return SIMU_ILLEGAL.  To work around the race
	 * this code will keep track of the last illegal instruction seen
	 * by each lwp and will only take action if the illegal instruction
	 * is repeatable.
	 */
	if (addr != mpcb->mpcb_illexcaddr ||
	    inst != mpcb->mpcb_illexcinsn)
		nomatch = 1;
	mpcb->mpcb_illexcaddr = addr;
	mpcb->mpcb_illexcinsn = inst;

	/* instruction fields */
	i = (inst >> 13) & 0x1;
	rd = (inst >> 25) & 0x1f;
	optype = (inst >> 30) & 0x3;
	op3 = (inst >> 19) & 0x3f;
	ignor = (inst >> 5) & 0xff;

	if (op3 == IOP_V8_POPC)
		return (simulate_popc(rp, badaddr, inst));
	if (optype == OP_V8_LDSTR) {
		if (op3 == IOP_V8_LDQF || op3 == IOP_V8_LDQFA ||
		    op3 == IOP_V8_STQF || op3 == IOP_V8_STQFA)
			return (do_unaligned(rp, badaddr));
	}

	if (nomatch)
		return (SIMU_RETRY);

	/*
	 * The rest of the code handles v8 binaries with instructions
	 * that have dirty (non-zero) bits in reserved or 'ignored'
	 * fields; these will cause core dumps on v9 machines.
	 *
	 * We only clean dirty instructions in 32-bit programs (ie, v8)
	 * running on SPARCv9 processors.  True v9 programs are forced
	 * to use the instruction set as intended.
	 */
#ifdef __sparcv9
	if (lwp_getdatamodel(curthread->t_lwp) != DATAMODEL_ILP32)
		return (SIMU_ILLEGAL);
#endif
	switch (optype) {
	case OP_V8_BRANCH:
	case OP_V8_CALL:
		return (SIMU_ILLEGAL);	/* these don't have ignored fields */
		/*NOTREACHED*/
	case OP_V8_ARITH:
		switch (op3) {
		case IOP_V8_RETT:
			if (rd == 0 && !(i == 0 && ignor))
				return (SIMU_ILLEGAL);
			if (rd)
				inst &= ~(0x1f << 25);
			if (i == 0 && ignor)
				inst &= ~(0xff << 5);
			break;
		case IOP_V8_TCC:
			if (i == 0 && ignor != 0) {
				inst &= ~(0xff << 5);
			} else if (i == 1 && (((inst >> 7) & 0x3f) != 0)) {
				inst &= ~(0x3f << 7);
			} else {
				return (SIMU_ILLEGAL);
			}
			break;
		case IOP_V8_JMPL:
		case IOP_V8_RESTORE:
		case IOP_V8_SAVE:
			if ((op3 == IOP_V8_RETT && rd) ||
			    (i == 0 && ignor)) {
				inst &= ~(0xff << 5);
			} else {
				return (SIMU_ILLEGAL);
			}
			break;
		case IOP_V8_FCMP:
			if (rd == 0)
				return (SIMU_ILLEGAL);
			else
				inst &= ~(0x1f << 25);
			break;
		case IOP_V8_RDASR:
			rs1 = ((inst >> 14) & 0x1f);
			if (rs1 == 1 || (rs1 >= 7 && rs1 <= 14)) {
				/*
				 * The instruction specifies an invalid
				 * state register - better bail out than
				 * "fix" it when we're not sure what was
				 * intended.
				 */
				return (SIMU_ILLEGAL);
			} else {
				/*
				 * Note: this case includes the 'stbar'
				 * instruction (rs1 == 15 && i == 0).
				 */
				if ((ignor = (inst & 0x3fff)) != 0)
					inst &= ~(0x3fff);
			}
			break;
		case IOP_V8_SRA:
		case IOP_V8_SRL:
		case IOP_V8_SLL:
			if (ignor == 0)
				return (SIMU_ILLEGAL);
			else
				inst &= ~(0xff << 5);
			break;
		case IOP_V8_ADD:
		case IOP_V8_AND:
		case IOP_V8_OR:
		case IOP_V8_XOR:
		case IOP_V8_SUB:
		case IOP_V8_ANDN:
		case IOP_V8_ORN:
		case IOP_V8_XNOR:
		case IOP_V8_ADDC:
		case IOP_V8_UMUL:
		case IOP_V8_SMUL:
		case IOP_V8_SUBC:
		case IOP_V8_UDIV:
		case IOP_V8_SDIV:
		case IOP_V8_ADDcc:
		case IOP_V8_ANDcc:
		case IOP_V8_ORcc:
		case IOP_V8_XORcc:
		case IOP_V8_SUBcc:
		case IOP_V8_ANDNcc:
		case IOP_V8_ORNcc:
		case IOP_V8_XNORcc:
		case IOP_V8_ADDCcc:
		case IOP_V8_UMULcc:
		case IOP_V8_SMULcc:
		case IOP_V8_SUBCcc:
		case IOP_V8_UDIVcc:
		case IOP_V8_SDIVcc:
		case IOP_V8_TADDcc:
		case IOP_V8_TSUBcc:
		case IOP_V8_TADDccTV:
		case IOP_V8_TSUBccTV:
		case IOP_V8_MULScc:
		case IOP_V8_WRASR:
		case IOP_V8_FLUSH:
			if (i != 0 || ignor == 0)
				return (SIMU_ILLEGAL);
			inst &= ~(0xff << 5);
			break;
		default:
			return (SIMU_ILLEGAL);
		}
		break;
	case OP_V8_LDSTR:
		switch (op3) {
		case IOP_V8_STFSR:
		case IOP_V8_LDFSR:
			if (rd == 0 && !(i == 0 && ignor))
				return (SIMU_ILLEGAL);
			if (rd)
				inst &= ~(0x1f << 25);
			if (i == 0 && ignor)
				inst &= ~(0xff << 5);
			break;
		default:
			if (optype == OP_V8_LDSTR && !IS_LDST_ALT(op3) &&
			    i == 0 && ignor)
				inst &= ~(0xff << 5);
			else
				return (SIMU_ILLEGAL);
			break;
		}
		break;
	default:
		return (SIMU_ILLEGAL);
	}

	/*
	 * A "flush" instruction using the user PC's vaddr will not work
	 * here, at least on Spitfire. Instead we create a temporary kernel
	 * mapping to the user's text page, then modify and flush that.
	 */
	as = p->p_as;
	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	pfnum = hat_getpfnum(as->a_hat, (caddr_t)rp->r_pc);
	if ((pp = page_numtopp(pfnum, SE_SHARED)) == (page_t *)0) {
		AS_LOCK_EXIT(as, &as->a_lock);
		return (SIMU_ILLEGAL);
	}
	ka = ppmapin(pp, PROT_READ|PROT_WRITE, (caddr_t)rp->r_pc);
	*(u_int *)(ka + (uintptr_t)(rp->r_pc % PAGESIZE)) = inst;
	doflush(ka + (uintptr_t)(rp->r_pc % PAGESIZE));
	ppmapout(ka);
	page_unlock(pp);
	AS_LOCK_EXIT(as, &as->a_lock);

	return (SIMU_RETRY);
}

/*
 * Get the value of a register for instruction simulation
 * by using the regs or window structure pointers.
 * Return 0 for success, and -1 for failure.  If there is a failure,
 * save the faulting address using badaddr pointer.
 * We have 64 bit globals and outs, and 32 or 64 bit ins and locals.
 * Don't truncate globals/outs for 32 bit programs, for v8+ support.
 */
int
getreg(struct regs *rp, u_int reg, uint64_t *val, caddr_t *badaddr)
{
	uint64_t *rgs, *sp;
	int rv = 0;

	rgs = (uint64_t *)&rp->r_ps;		/* globals and outs */
	sp = (uint64_t *)rp->r_sp;		/* ins and locals */
	if (reg == 0) {
		*val = 0;
	} else if (reg < 16) {
		*val = rgs[reg];
	} else if (IS_V9STACK(sp)) {
		uint64_t *rw = (uint64_t *)((uintptr_t)sp + V9BIAS64);
		uint64_t *addr = (uint64_t *)&rw[reg - 16];
		uint64_t res;

		if (USERMODE(rp->r_tstate)) {
			proc_t *p = curproc;
			int mapped = 0;

			if (p->p_warea)		/* watchpoints in effect */
				mapped = pr_mappage((caddr_t)addr, sizeof (res),
							S_READ, 1);
			if (default_fuword64(addr, &res) == -1) {
				*badaddr = (caddr_t)addr;
				rv = -1;
			}
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (res),
						S_READ, 1);
		} else {
			res = *addr;
		}
		*val = res;
	} else {
		uint32_t *rw = (uint32_t *)(caddr32_t)sp;
		uint32_t *addr = (uint32_t *)&rw[reg - 16];
		uint32_t res;

		if (USERMODE(rp->r_tstate)) {
			proc_t *p = curproc;
			int mapped = 0;

			if (p->p_warea)		/* watchpoints in effect */
				mapped = pr_mappage((caddr_t)addr, sizeof (res),
							S_READ, 1);
			if (default_fuword32(addr, &res) == -1) {
				*badaddr = (caddr_t)addr;
				rv = -1;
			}
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (res),
						S_READ, 1);
		} else {
			res = *addr;
		}
		*val = (uint64_t)res;
	}
	return (rv);
}

/*
 * Set the value of a register after instruction simulation
 * by using the regs or window structure pointers.
 * Return 0 for succes -1 failure.
 * save the faulting address using badaddr pointer.
 * We have 64 bit globals and outs, and 32 or 64 bit ins and locals.
 * Don't truncate globals/outs for 32 bit programs, for v8+ support.
 */
int
putreg(uint64_t	*data, struct regs *rp, u_int reg, caddr_t *badaddr)
{
	uint64_t *rgs, *sp;
	int rv = 0;

	rgs = (uint64_t *)&rp->r_ps;		/* globals and outs */
	sp = (uint64_t *)rp->r_sp;		/* ins and locals */
	if (reg == 0) {
		return (0);
	} else if (reg < 16) {
		rgs[reg] = *data;
	} else if (IS_V9STACK(sp)) {
		uint64_t *rw = (uint64_t *)((uintptr_t)sp + V9BIAS64);
		uint64_t *addr = (uint64_t *)&rw[reg - 16];
		uint64_t res;

		if (USERMODE(rp->r_tstate)) {
			struct machpcb *mpcb = lwptompcb(curthread->t_lwp);
			proc_t *p = curproc;
			int mapped = 0;

			if (p->p_warea)		/* watchpoints in effect */
				mapped = pr_mappage((caddr_t)addr, sizeof (res),
						S_WRITE, 1);
			res = *data;
			if (default_suword64(addr, res) != 0) {
				*badaddr = (caddr_t)addr;
				rv = -1;
			}
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (res),
						S_WRITE, 1);
			/*
			 * We have changed a local or in register;
			 * nuke the watchpoint return windows.
			 */
			mpcb->mpcb_rsp[0] = NULL;
			mpcb->mpcb_rsp[1] = NULL;
		} else {
			res = *data;
			*addr = res;
		}
	} else {
		uint32_t *rw = (uint32_t *)(caddr32_t)sp;
		uint32_t *addr = (uint32_t *)&rw[reg - 16];
		uint32_t res;

		if (USERMODE(rp->r_tstate)) {
			struct machpcb *mpcb = lwptompcb(curthread->t_lwp);
			proc_t *p = curproc;
			int mapped = 0;

			if (p->p_warea)		/* watchpoints in effect */
				mapped = pr_mappage((caddr_t)addr, sizeof (res),
						S_WRITE, 1);
			res = (u_int)*data;
			if (default_suword32(addr, res) != 0) {
				*badaddr = (caddr_t)addr;
				rv = -1;
			}
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (res),
						S_WRITE, 1);
			/*
			 * We have changed a local or in register;
			 * nuke the watchpoint return windows.
			 */
			mpcb->mpcb_rsp[0] = NULL;
			mpcb->mpcb_rsp[1] = NULL;

		} else {
			res = (u_int)*data;
			*addr = res;
		}
	}
	return (rv);
}

/*
 * Calculate a memory reference address from instruction
 * operands, used to return the address of a fault, instead
 * of the instruction when an error occurs.  This is code that is
 * common with most of the routines that simulate instructions.
 */
int
calc_memaddr(struct regs *rp, caddr_t *badaddr)
{
	u_int	inst;
	u_int	rd, rs1, rs2;
	int	sz;
	int	immflg;
	int	floatflg;
	caddr_t  addr;
	uint64_t val;

	if (USERMODE(rp->r_tstate))
		inst = fetch_user_instr((caddr_t)rp->r_pc);
	else
		inst = *(u_int *)rp->r_pc;

	rd = (inst >> 25) & 0x1f;
	rs1 = (inst >> 14) & 0x1f;
	rs2 = inst & 0x1f;
	floatflg = (inst >> 24) & 1;
	immflg = (inst >> 13) & 1;

	if (floatflg) {
		switch ((inst >> 19) & 3) {	/* map size bits to a number */
		case 0: sz = 4; break;		/* ldf/stf */
		case 1: return (0);		/* ld[x]fsr/st[x]fsr */
		case 2: sz = 16; break;		/* ldqf/stqf */
		case 3: sz = 8; break;		/* lddf/stdf */
		}
		/*
		 * Fix to access extra double register encoding plus
		 * compensate to access the correct fpu_dreg.
		 */
		if (sz > 4) {
			if ((rd & 1) == 1)
				rd = (rd & 0x1e) | 0x20;
			rd = rd >> 1;
		}
	} else {
		switch ((inst >> 19) & 0xf) {	/* map size bits to a number */
		case 0:				/* lduw */
		case 4:				/* stw */
		case 8:				/* ldsw */
		case 0xf:			/* swap */
			sz = 4; break;
		case 1:				/* ldub */
		case 5:				/* stb */
		case 9:				/* ldsb */
		case 0xd:			/* ldstub */
			sz = 1; break;
		case 2:				/* lduh */
		case 6:				/* sth */
		case 0xa:			/* ldsh */
			sz = 2; break;
		case 3:				/* ldd */
		case 7:				/* std */
		case 0xb:			/* ldx */
		case 0xe:			/* stx */
			sz = 8; break;
		}
	}

	if (USERMODE(rp->r_tstate))
		(void) flush_user_windows_to_stack(NULL);
	else
		flush_windows();

	if (getreg(rp, rs1, &val, badaddr))
		return (SIMU_FAULT);
	addr = (caddr_t)val;

	/* check immediate bit and use immediate field or reg (rs2) */
	if (immflg) {
		int imm;
		imm = inst & 0x1fff;		/* mask out immediate field */
		imm <<= 19;			/* sign extend it */
		imm >>= 19;
		addr += imm;			/* compute address */
	} else {
		if (getreg(rp, rs2, &val, badaddr))
			return (SIMU_FAULT);
		addr += val;
	}

	/*
	 * If this is a 32-bit program, chop the address accordingly.
	 */
	if (curproc->p_model == DATAMODEL_ILP32 &&
	    USERMODE(rp->r_tstate))
		addr = (caddr_t)(caddr32_t)addr;

	*badaddr = addr;
	return ((uintptr_t)addr & (sz - 1) ? SIMU_UNALIGN : SIMU_SUCCESS);
}

/*
 * Return the size of a load or store instruction (1, 2, 4, 8, 16, 64).
 * Also compute the precise address by instruction disassembly.
 * (v9 page faults only provide the page address via the hardware.)
 * Return 0 on failure (not a load or store instruction).
 */
int
instr_size(struct regs *rp, caddr_t *addrp, enum seg_rw rdwr)
{
	u_int	inst, op3, asi;
	u_int	rd, rs1, rs2;
	int	sz = 0;
	int	immflg;
	int	floatflg;
	caddr_t	addr;
	caddr_t badaddr;
	uint64_t val;

	if (rdwr == S_EXEC) {
		*addrp = (caddr_t)rp->r_pc;
		return (4);
	}

	/*
	 * Fetch the instruction from user-level.
	 * We would like to assert this:
	 *   ASSERT(USERMODE(rp->r_tstate));
	 * but we can't because we can reach this point from a
	 * register window underflow/overflow and the v9 wbuf
	 * traps call trap() with T_USER even though r_tstate
	 * indicates a system trap, not a user trap.
	 */
	inst = fetch_user_instr((caddr_t)rp->r_pc);

	op3 = (inst >> 19) & 0x3f;
	rd = (inst >> 25) & 0x1f;
	rs1 = (inst >> 14) & 0x1f;
	rs2 = inst & 0x1f;
	floatflg = (inst >> 24) & 1;
	immflg = (inst >> 13) & 1;

	/* if not load or store do nothing.  can't happen? */
	if ((inst >> 30) != 3)
		return (0);

	if (immflg)
		asi = (u_int)((rp->r_tstate >> TSTATE_ASI_SHIFT) &
				TSTATE_ASI_MASK);
	else
		asi = (inst >> 5) & 0xff;

	if (floatflg) {
		/* check for ld/st alternate and highest defined V9 asi */
		if ((op3 & 0x30) == 0x30 && asi > ASI_SNFL) {
			sz = extended_asi_size(asi);
		} else {
			switch (op3 & 3) {
			case 0:
				sz = 4;			/* ldf/stf */
				break;
			case 1:
				if (rd == 0)
					sz = 4;		/* ldfsr/stfsr */
				else
					sz = 8;		/* ldxfsr/stxfsr */
				break;
			case 2:
				sz = 16;		/* ldqf/stqf */
				break;
			case 3:
				sz = 8;			/* lddf/stdf */
				break;
			}
		}
	} else {
		switch (op3 & 0xf) {		/* map size bits to a number */
		case 0:				/* lduw */
		case 4:				/* stw */
		case 8:				/* ldsw */
		case 0xf:			/* swap */
			sz = 4; break;
		case 1:				/* ldub */
		case 5:				/* stb */
		case 9:				/* ldsb */
		case 0xd:			/* ldstub */
			sz = 1; break;
		case 2:				/* lduh */
		case 6:				/* sth */
		case 0xa:			/* ldsh */
			sz = 2; break;
		case 3:				/* ldd */
		case 7:				/* std */
		case 0xb:			/* ldx */
		case 0xe:			/* stx */
			sz = 8; break;
		}
	}

	if (sz == 0)	/* can't happen? */
		return (0);
	(void) flush_user_windows_to_stack(NULL);

	if (getreg(rp, rs1, &val, &badaddr))
		return (0);
	addr = (caddr_t)val;

	/* check immediate bit and use immediate field or reg (rs2) */
	if (immflg) {
		int imm;
		imm  = inst & 0x1fff;		/* mask out immediate field */
		imm <<= 19;			/* sign extend it */
		imm >>= 19;
		addr += imm;			/* compute address */
	} else {
		if (getreg(rp, rs2, &val, &badaddr))
			return (0);
		addr += val;
	}

	/*
	 * If this is a 32-bit program, chop the address accordingly.
	 */
	if (curproc->p_model == DATAMODEL_ILP32)
		addr = (caddr_t)(caddr32_t)addr;

	*addrp = addr;
	ASSERT(sz != 0);
	return (sz);
}

/*
 * Fetch an instruction from user-level.
 * Deal with watchpoints, if they are in effect.
 */
int32_t
fetch_user_instr(caddr_t vaddr)
{
	proc_t *p = curproc;
	int mapped = 0;
	int32_t instr;

	/*
	 * If this is a 32-bit program, chop the address accordingly.
	 */
	if (p->p_model == DATAMODEL_ILP32)
		vaddr = (caddr_t)(caddr32_t)vaddr;

	if (p->p_warea)		/* watchpoints in effect */
		mapped = pr_mappage(vaddr, sizeof (int32_t), S_READ, 1);
	if (default_fuiword32(vaddr, (uint32_t *)&instr) == -1)
		instr = -1;
	if (mapped)
		pr_unmappage(vaddr, sizeof (int32_t), S_READ, 1);
	return (instr);
}
