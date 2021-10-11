/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)simulator.c	1.31	97/10/22 SMI"

/* common code with bug fixes from original version in trap.c */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/fpu/fpusystm.h>
#include <sys/fpu/fpu_simulator.h>
#include <sys/fpu/globals.h>
#include <sys/inline.h>
#include <sys/debug.h>
#include <sys/privregs.h>
#include <sys/machpcb.h>
#include <sys/simulate.h>
#include <sys/proc.h>
#include <sys/cmn_err.h>
#include <sys/stack.h>
#include <sys/watchpoint.h>
#include <sys/copyops.h>

static int getreg(struct regs *, u_int, uint32_t *val, caddr_t *badaddr);
static int putreg(uint32_t data, struct regs *, u_int reg, caddr_t *badaddr);

static int aligndebug = 0;

/*
 * For the sake of those who must be compatible with unaligned
 * architectures, users can link their programs to use a
 * corrective trap handler that will fix unaligned references
 * a special trap #6 (T_FIX_ALIGN) enables this 'feature'.
 * Returns 1 for success, 0 for failure.
 */
char *sizestr[] = {"word", "byte", "halfword", "double"};

int
do_unaligned(struct regs *rp, caddr_t *badaddr)
{
	u_int	inst;
	u_int	rd, rs1, rs2;
	int	sz;
	int	floatflg;
	int	immflg;
	int 	addr;
	u_int		val;
	union ud {
		double	d;
		u_int	i[2];
		u_short	s[4];
		u_char	c[8];
	} data;

	ASSERT(USERMODE(rp->r_psr));
	inst = fetch_user_instr((caddr_t)rp->r_pc);

	rd = (inst >> 25) & 0x1f;
	rs1 = (inst >> 14) & 0x1f;
	rs2 = inst & 0x1f;
	floatflg = (inst >> 24) & 1;
	immflg = (inst >> 13) & 1;

	switch ((inst >> 19) & 3) {	/* map size bits to a number */
	case 0: sz = 4; break;
	case 1: sz = 1; break;
	case 2: sz = 2; break;
	case 3: sz = 8; break;
	}

	if (aligndebug) {
		printf("unaligned access at 0x%lx, instruction: 0x%x\n",
		    rp->r_pc, inst);
		printf("type %s %s %s\n",
			(((inst >> 21) & 1) ? "st" : "ld"),
			(((inst >> 22) & 1) ? "signed" : "unsigned"),
			sizestr[((inst >> 19) & 3)]);
		printf("rd = %d, rs1 = %d, rs2 = %d, imm13 = 0x%x\n",
		    rd, rs1, rs2, (inst & 0x1fff));
	}

	/* if not load or store, or to alternate space do nothing */
	if (((inst >> 30) != 3) ||
	    (immflg == 0 && ((inst >> 5) & 0xff)))
		return (0);		/* don't support alternate ASI */

	/* if ldstub or swap, do nothing */
	if ((inst & 0xc1680000) == 0xc0680000)
		return (0);

	(void) flush_user_windows_to_stack(NULL); /* flush windows to memory */
	if (getreg(rp, rs1, &val, badaddr))
		return (SIMU_FAULT);
	addr = val;

	/* check immediate bit and use immediate field or reg (rs2) */
	if (immflg) {
		register int imm;
		imm  = inst & 0x1fff;		/* mask out immediate field */
		imm <<= 19;			/* sign extend it */
		imm >>= 19;
		addr += imm;			/* compute address */
	} else {
		if (getreg(rp, rs2, &val, badaddr))
			return (SIMU_FAULT);
		addr += val;
	}

	if (aligndebug)
		printf("addr = 0x%x\n", addr);
	if ((u_int)addr >= KERNELBASE) {
		*badaddr = (caddr_t)addr;
		goto badret;
	}

	/* a single bit differentiates ld and st */
	if ((inst >> 21) & 1) {			/* store */
		if (floatflg) {
			/* if fp read fpu reg */
			if (fpu_exists) {
				if (((inst >> 19) & 0x3f) == 0x25) {
					_fp_read_pfsr(&data.i[0]);
				} else {
					_fp_read_pfreg(&data.i[0], rd);
					if (sz == 8)
					    _fp_read_pfreg(&data.i[1], rd+1);
				}
			} else {
				klwp_id_t lwp = ttolwp(curthread);
				kfpu_t *fp = lwptofpu(lwp);
				if (((inst >> 19) & 0x3f) == 0x25) {
					data.i[0] = (u_int)fp->fpu_fsr;
				} else {
					data.i[0] =
					    (u_int)fp->fpu_fr.fpu_regs[rd];
					if (sz == 8)
					    data.i[1] = (u_int)
						fp->fpu_fr.fpu_regs[rd+1];
				}
			}
		} else {
			if (getreg(rp, rd, &data.i[0], badaddr))
				return (SIMU_FAULT);
			if (sz == 8 &&
			    getreg(rp, rd+1, &data.i[1], badaddr))
				return (SIMU_FAULT);
		}

		if (aligndebug) {
			printf("data %x %x %x %x %x %x %x %x\n",
				data.c[0], data.c[1], data.c[2], data.c[3],
				data.c[4], data.c[5], data.c[6], data.c[7]);
		}

		if (sz == 1) {
			if (copyout((caddr_t)&data.c[3], (caddr_t)addr,
			    (u_int)sz) == -1)
				goto badret;
		} else if (sz == 2) {
			if (copyout((caddr_t)&data.s[1], (caddr_t)addr,
			    (u_int)sz) == -1)
				goto badret;
		} else {
			if (copyout((caddr_t)&data.i[0], (caddr_t)addr,
			    (u_int)sz) == -1)
				goto badret;
		}
	} else {				/* load */
		if (sz == 1) {
			if (copyin((caddr_t)addr, (caddr_t)&data.c[3],
			    (u_int)sz) == -1)
				goto badret;
			/* if signed and the sign bit is set extend it */
			if (((inst >> 22) & 1) && ((data.c[3] >> 7) & 1)) {
				data.s[0] = (u_short)-1; /* extend sign bit */
				data.c[2] = (u_char)-1;
			} else {
				data.s[0] = 0;	/* clear upper 24 bits */
				data.c[2] = 0;
			}
		} else if (sz == 2) {
			if (copyin((caddr_t)addr, (caddr_t)&data.s[1],
			    (u_int)sz) == -1)
				goto badret;
			/* if signed and the sign bit is set extend it */
			if (((inst >> 22) & 1) && ((data.s[1] >> 15) & 1))
				data.s[0] = (u_short)-1; /* extend sign bit */
			else
				data.s[0] = 0;	/* clear upper 16 bits */
		} else
			if (copyin((caddr_t)addr, (caddr_t)&data.i[0],
			    (u_int)sz) == -1)
				goto badret;

		if (aligndebug) {
			printf("data %x %x %x %x %x %x %x %x\n",
				data.c[0], data.c[1], data.c[2], data.c[3],
				data.c[4], data.c[5], data.c[6], data.c[7]);
		}

		if (floatflg) {		/* if fp, write fpu reg */
			if (fpu_exists) {
				if (((inst >> 19) & 0x3f) == 0x21) {
					_fp_write_pfsr(&data.i[0]);
				} else {
					_fp_write_pfreg(&data.i[0], rd);
					if (sz == 8)
					    _fp_write_pfreg(&data.i[1], rd+1);
				}
			} else {
				klwp_id_t lwp = ttolwp(curthread);
				kfpu_t *fp = lwptofpu(lwp);
				if (((inst >> 19) & 0x3f) == 0x21) {
					fp->fpu_fsr = data.i[0];
				} else {
					fp->fpu_fr.fpu_regs[rd] = data.i[0];
					if (sz == 8)
						fp->fpu_fr.fpu_regs[rd+1] =
						    data.i[1];
				}
			}
		} else {
			if (putreg(data.i[0], rp, rd, badaddr))
				goto badret;
			if (sz == 8 &&
			    putreg(data.i[1], rp, rd+1, badaddr))
				goto badret;
		}
	}
	return (SIMU_SUCCESS);
badret:
	return (SIMU_FAULT);
}

/*
 * simulate unimplemented instructions (swap, mul, div)
 */
int
simulate_unimp(struct regs *rp, caddr_t *badaddr)
{
	u_int		inst;
	int		rv;
	int		do_swap();

	if (USERMODE(rp->r_psr)) {	/* user land */
		inst = fetch_user_instr((caddr_t)rp->r_pc);
		if (inst == (u_int)-1)
			return (SIMU_ILLEGAL);
		/*
		 * Simulation depends on the stack having the most
		 * up-to-date copy of the register windows.
		 */
		(void) flush_user_windows_to_stack(NULL);
	} else	/* kernel land */
		inst = *(u_int *)rp->r_pc;

	/*
	 * Check for the unimplemented swap instruction.
	 *
	 * Note:  there used to be support for simulating swap
	 * instructions used by the kernel.  The kernel doesn't
	 * use swap, and shouldn't use it unless it's in hardware.
	 * If the kernel needs swap and it isn't implemented, it'll
	 * be extremely difficult to simulate.
	 */

	if ((inst & 0xc1f80000) == 0xc0780000)
		return (do_swap(rp, inst, badaddr));

#ifndef	__sparcv9cpu
	/*
	 * for mul/div instruction switch on op3 field of instruction
	 * if the two bit op field is 0x2
	 */
	if ((inst >> 30) == 0x2) {
		u_int rs1, rs2, rd;
		u_int	dest;			/* place for destination */

		rd =  (inst >> 25) & 0x1f;

		/* generate first operand rs1 */
		if (getreg(rp, ((inst >> 14) & 0x1f), &rs1, badaddr))
			return (SIMU_FAULT);

		/* check immediate bit and use immediate field or reg (rs2) */
		if ((inst >> 13) & 1) {
			register int imm;
			imm  = inst & 0x1fff;	/* mask out immediate field */
			imm <<= 19;		/* sign extend it */
			imm >>= 19;
			rs2 = imm;		/* compute address */
		} else
			if (getreg(rp, (inst & 0x1f), &rs2, badaddr))
				return (SIMU_FAULT);

		switch ((inst & 0x01f80000) >> 19) {
		case 0xa:
			rv = _ip_umul(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		case 0xb:
			rv = _ip_mul(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		case 0xe:
			rv = _ip_udiv(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		case 0xf:
			rv = _ip_div(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		case 0x1a:
			rv = _ip_umulcc(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		case 0x1b:
			rv = _ip_mulcc(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		case 0x1e:
			rv = _ip_udivcc(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		case 0x1f:
			rv = _ip_divcc(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		default:
			return (SIMU_ILLEGAL);
		}
		if (rv != SIMU_SUCCESS)
			return (rv);
		if (putreg(dest, rp, rd, badaddr))
			return (SIMU_FAULT);

		/*
		 * If it was a multiply, the routine in crt.s already moved
		 * the high result into rp->r_y
		 */
		return (SIMU_SUCCESS);
	}
#endif	/* __sparcv9cpu */

	/*
	 * Otherwise, we can't simulate instruction, its illegal.
	 */
	return (SIMU_ILLEGAL);
}

int
do_swap(struct regs *rp, u_int inst, caddr_t *badaddr)
{
	register u_int rd, rs1, rs2;
	register int immflg, s;
	u_int src, srcaddr, srcval;
	u_int dstval;
	proc_t *p = curproc;
	int mapped = 0;

	/*
	 * Check for the unimplemented swap instruction.
	 *
	 * Note:  there used to be support for simulating swap
	 * instructions used by the kernel.  The kernel doesn't
	 * use swap, and shouldn't use it unless it's in hardware.
	 * If the kernel needs swap and it isn't implemented, it'll
	 * be extremely difficult to simulate.
	 */
	if (!(USERMODE(rp->r_psr)))
		panic("kernel use of swap instruction!");

	/*
	 * Calculate address, get first src register.
	 */
	rs1 = (inst >> 14) & 0x1f;
	if (getreg(rp, rs1, &src, badaddr))
		return (SIMU_FAULT);
	srcaddr = src;

	/*
	 * Check immediate bit and use immediate field or rs2
	 * to get the second operand to build source address.
	 */
	immflg = (inst >> 13) & 1;
	if (immflg) {
		s  = inst & 0x1fff;		/* mask out immediate field */
		s <<= 19;			/* sign extend it */
		s >>= 19;
		srcaddr += s;			/* compute address */
	} else {
		rs2 =  inst & 0x1f;
		if (getreg(rp, rs2, &src, badaddr))
			return (SIMU_FAULT);
		srcaddr += src;
	}

	/*
	 * Check for unaligned address.
	 */
	if ((srcaddr&3) != 0) {
		*badaddr = (caddr_t)srcaddr;
		return (SIMU_UNALIGN);
	}

	if (p->p_warea)		/* watchpoints in effect */
		mapped = pr_mappage((caddr_t)srcaddr, sizeof (int), S_WRITE, 1);

	/*
	 * Raise priority (atomic swap operation).
	 */
	s = splhigh();

	/*
	 * Read memory at source address.
	 */
	if (default_fuword32((void *)srcaddr, &srcval) == -1) {
		*badaddr = (caddr_t)srcaddr;
		goto badret;
	}

	/*
	 * Get value from destination register.
	 */
	rd =  (inst >> 25) & 0x1f;
	if (getreg(rp, rd, &dstval, badaddr))
		goto badret;


	/*
	 * Write src address with value from destination register.
	 */
	if (default_suword32((void *)srcaddr, dstval) == -1) {
		*badaddr = (caddr_t)srcaddr;
		goto badret;
	}

	/*
	 * Update destination reg with value from memory.
	 */
	if (putreg(srcval, rp, rd, badaddr))
		goto badret;

	/*
	 * Restore priority and return success or failure.
	 */
	splx(s);
	if (mapped)
		pr_unmappage((caddr_t)srcaddr, sizeof (int), S_WRITE, 1);
	return (SIMU_SUCCESS);
badret:
	splx(s);
	if (mapped)
		pr_unmappage((caddr_t)srcaddr, sizeof (int), S_WRITE, 1);
	return (SIMU_FAULT);
}

/*
 * Get the value of a register for instruction simulation
 * by using the regs or window structure pointers.
 * Return 0 for success -1 failure.  If there is a failure,
 * save the faulting address using badaddr pointer.
 */
static int
getreg(struct regs *rp, u_int reg, uint32_t *val, caddr_t *badaddr)
{
	uint32_t *rgs, *rw;
	int rv = 0;

	rgs = (uint32_t *)&rp->r_y;		/* globals and outs */
	rw = (uint32_t *)rp->r_sp;		/* ins and locals */
	if (reg == 0) {
		*val = 0;
	} else if (reg < 16) {
		*val = rgs[reg];
	} else if (USERMODE(rp->r_psr)) {
		uint32_t res;
		proc_t *p = curproc;
		caddr_t addr = (caddr_t)&rw[reg - 16];
		int mapped = 0;

		if (p->p_warea)		/* watchpoints in effect */
			mapped = pr_mappage(addr, sizeof (res), S_READ, 1);
		if (default_fuword32(addr, &res) == -1) {
			*badaddr = addr;
			rv = -1;
		}
		if (mapped)
			pr_unmappage(addr, sizeof (res), S_READ, 1);
		*val = res;
	} else {
		*val = rw[reg - 16];
	}
	return (rv);
}

/*
 * Set the value of a register after instruction simulation
 * by using the regs or window structure pointers.
 * Return 0 for success -1 failure.
 * save the faulting address using badaddr pointer.
 */
static int
putreg(uint32_t data, struct regs *rp, u_int reg, caddr_t *badaddr)
{
	uint32_t *rgs, *rw;
	int rv = 0;

	rgs = (uint32_t *)&rp->r_y;		/* globals and outs */
	rw = (uint32_t *)rp->r_sp;		/* ins and locals */
	if (reg == 0) {
		return (0);
	} else if (reg < 16) {
		rgs[reg] = data;
	} else if (USERMODE(rp->r_psr)) {
		struct machpcb *mpcb = lwptompcb(curthread->t_lwp);
		proc_t *p = curproc;
		caddr_t addr = (caddr_t)&rw[reg - 16];
		int mapped = 0;

		if (p->p_warea)		/* watchpoints in effect */
			mapped = pr_mappage(addr, sizeof (data), S_WRITE, 1);
		if (default_suword32(addr, data) != 0) {
			*badaddr = addr;
			rv = -1;
		}
		if (mapped)
			pr_unmappage(addr, sizeof (data), S_WRITE, 1);
		/*
		 * We have changed a local or in register;
		 * nuke the watchpoint return windows.
		 */
		mpcb->mpcb_rsp[0] = NULL;
		mpcb->mpcb_rsp[1] = NULL;
	} else {
		rw[reg - 16] = data;
	}
	return (rv);
}

/*
 * Calculate a memory reference address from instruction
 * operands, used to return the address of a fault, instead
 * of the instruction when error occur.  This is code is
 * common with most of the routines that simulate instructions.
 */
int
calc_memaddr(struct regs *rp, caddr_t *badaddr)
{
	u_int	inst;
	u_int	rs1, rs2;
	int	sz;
	int	immflg;
	int 	addr;
	u_int	val;

	ASSERT(USERMODE(rp->r_psr));
	inst = fetch_user_instr((caddr_t)rp->r_pc);

	rs1 = (inst >> 14) & 0x1f;
	rs2 = inst & 0x1f;
	immflg = (inst >> 13) & 1;

	switch ((inst >> 19) & 3) {	/* map size bits to a number */
	case 0: sz = 4; break;
	case 1: return (0);
	case 2: sz = 2; break;
	case 3: sz = 8; break;
	}
	(void) flush_user_windows_to_stack(NULL); /* flush windows to memory */

	if (getreg(rp, rs1, &val, badaddr))
		return (SIMU_FAULT);
	addr = val;

	/* check immediate bit and use immediate field or reg (rs2) */
	if (immflg) {
		register int imm;
		imm  = inst & 0x1fff;		/* mask out immediate field */
		imm <<= 19;			/* sign extend it */
		imm >>= 19;
		addr += imm;			/* compute address */
	} else {
		if (getreg(rp, rs2, &val, badaddr))
			return (SIMU_FAULT);
		addr += val;
	}

	*badaddr = (caddr_t)addr;
	return (addr & (sz - 1) ? SIMU_UNALIGN : SIMU_SUCCESS);
}

/*
 * Return the size of a load or store instruction (1, 2, 4, 8).
 * Return 0 on failure (not a load or store instruction).
 */
/* ARGSUSED */
int
instr_size(struct regs *rp, caddr_t *addrp, enum seg_rw rw)
{
	u_int	inst;
	int	sz = 0;
	u_int	rd;
	int	immflg;
	int	floatflg;

	ASSERT(USERMODE(rp->r_psr));

	if (rw == S_EXEC)
		return (4);

	inst = fetch_user_instr((caddr_t)rp->r_pc);

	rd = (inst >> 25) & 0x1f;
	floatflg = (inst >> 24) & 1;
	immflg = (inst >> 13) & 1;

	/* if not load or store, or to alternate space, return 0 */
	if (((inst >> 30) != 3) ||	/* can't happen? */
	    (immflg == 0 && ((inst >> 5) & 0xff)))
		return (0);

	if (floatflg) {
		switch ((inst >> 19) & 3) {	/* map size bits to a number */
		case 0: sz = 4; break;		/* ldf/stf */
		case 1:
			if (rd == 0)
				sz = 4;		/* ldfsr/stfsr */
			else
				sz = 8;		/* ldxfsr/stxfsr */
			break;
		case 2: sz = 16; break;		/* ldqf/stqf */
		case 3: sz = 8; break;		/* lddf/sddf */
		}
	} else {
		switch ((inst >> 19) & 3) {	/* map size bits to a number */
		case 0: sz = 4; break;
		case 1: sz = 1; break;
		case 2: sz = 2; break;
		case 3: sz = 8; break;
		}
	}

	return (sz);
}

/*
 * Check for an atomic (LDST, SWAP) instruction
 * returns 1 if yes, 0 if no.
 */
int
is_atomic(struct regs *rp)
{
	u_int inst;

	if (!USERMODE(rp->r_psr))
		inst = *(u_int *)rp->r_pc;
	else
		inst = fetch_user_instr((caddr_t)rp->r_pc);

	/* True for LDSTUB(A) and SWAP(A) */
	return ((inst & 0xc1680000) == 0xc0680000);
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

	if (p->p_warea)		/* watchpoints in effect */
		mapped = pr_mappage(vaddr, sizeof (int32_t), S_READ, 1);
	if (default_fuiword32(vaddr, (uint32_t *)&instr) == -1)
		instr = -1;
	if (mapped)
		pr_unmappage(vaddr, sizeof (int32_t), S_READ, 1);
	return (instr);
}
