/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)subr.c	1.56	99/09/13 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/vmparam.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/tuneable.h>
#include <sys/cpuvar.h>
#include <sys/archsystm.h>
#include <sys/vmem.h>
#include <vm/seg_kmem.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/atomic.h>
#include <sys/model.h>
#include <sys/kmem.h>
#include <sys/memlist.h>
#include <sys/autoconf.h>

/*
 * Routine which sets a user error; placed in
 * illegal entries in the bdevsw and cdevsw tables.
 */

int
nodev()
{
	return (curthread->t_lwp ?
	    ttolwp(curthread)->lwp_error = ENXIO : ENXIO);
}

/*
 * Null routine; placed in insignificant entries
 * in the bdevsw and cdevsw tables.
 */

int
nulldev()
{
	return (0);
}

static kmutex_t udevlock;

/*
 * Generate an unused major device number.
 */
major_t
getudev()
{
	static major_t next = 0;
	major_t ret;

	/*
	 * Ensure that we start allocating major numbers above the 'devcnt'
	 * count.  The only limit we place on the number is that it should be a
	 * legal 32-bit SVR4 major number and be greater than or equal to devcnt
	 * in the current system).
	 */
	mutex_enter(&udevlock);
	if (next == 0)
		next = devcnt;
	if (next <= L_MAXMAJ32 && next >= devcnt)
		ret = next++;
	else {
		/*
		 * If we fail to allocate a major number because devcnt has
		 * reached L_MAXMAJ32, we may be the victim of a sparsely
		 * populated devnames array.  We scan the array backwards
		 * looking for an empty slot;  if we find one, mark it as
		 * DN_GETUDEV so it doesn't get taken by subsequent consumers
		 * users of the devnames array, and issue a warning.
		 * It is vital for this routine to take drastic measures to
		 * succeed, since the kernel really needs it to boot.
		 */
		int i;
		for (i = devcnt - 1; i >= 0; i--) {
			if (devnamesp[i].dn_name == NULL &&
			    ((devnamesp[i].dn_flags & DN_TAKEN_GETUDEV) == 0))
				break;
		}
		if (i != -1) {
			cmn_err(CE_WARN, "Reusing device major number %d.", i);
			ASSERT(i >= 0 && i < devcnt);
			devnamesp[i].dn_flags |= DN_TAKEN_GETUDEV;
			ret = (major_t)i;
		} else {
			ret = (major_t)-1;
		}
	}
	mutex_exit(&udevlock);
	return (ret);
}


/*
 * Compress 'long' device number encoding to 32-bit device number
 * encoding.  If it won't fit, we return failure, but set the
 * device number to 32-bit NODEV for the sake of our callers.
 */
int
cmpldev(dev32_t *dst, dev_t dev)
{
#if defined(_LP64)
	if (dev == NODEV) {
		*dst = NODEV32;
	} else {
		major_t major = dev >> L_BITSMINOR;
		minor_t minor = dev & L_MAXMIN;

		if (major > L_MAXMAJ32 || minor > L_MAXMIN32) {
			*dst = NODEV32;
			return (0);
		}

		*dst = (dev32_t)((major << L_BITSMINOR32) | minor);
	}
#else
	*dst = (dev32_t)dev;
#endif
	return (1);
}

/*
 * Expand 32-bit dev_t's to long dev_t's.  Expansion always "fits"
 * into the return type, but we're careful to expand NODEV explicitly.
 */
dev_t
expldev(dev32_t dev32)
{
#ifdef _LP64
	if (dev32 == NODEV32)
		return (NODEV);
	return (makedevice((dev32 >> L_BITSMINOR32) & L_MAXMAJ32,
	    dev32 & L_MAXMIN32));
#else
	return ((dev_t)dev32);
#endif
}

/*
 * Compare two byte streams.  Returns 0 if they're identical, 1
 * if they're not.
 */
int
bcmp(const void *s1_arg, const void *s2_arg, size_t len)
{
	const char *s1 = s1_arg;
	const char *s2 = s2_arg;

	while (len--)
		if (*s1++ != *s2++)
			return (1);
	return (0);
}

#if defined(_SYSCALL32_IMPL) || defined(__lint)
/*
 * This routine is used to supply the actual value of the
 * current resource limit at the time it is being checked.
 *
 * Because 32-bit processes are less capable than 64-bit processes,
 * we use an additional infinity map to implement the more constrained
 * resource limits.
 */
rlim64_t
p_curlimit(struct proc *p, int type, model_t dmodel)
{
	rlim64_t r = PTOU(p)->u_rlimit[type].rlim_cur;

	if (dmodel == DATAMODEL_NATIVE) {
		if (r == RLIM64_INFINITY)
			return (rlim_infinity_map[type]);
	} else {
		rlim64_t r32 = rlim_infinity_map_32[type];

		if (r == RLIM64_INFINITY || r > r32)
			return (r32);
	}
	return (r);
}
#endif	/* _SYSCALL32_IMPL || __lint */

/*
 * Here we use the rlim_infinity_map[] to help us with the correct
 * system maximum for the concept "infinity".  This routine maps
 * an unlimited or "infinite" value to a system defined maximum.
 *
 * See also the macros in user.h used to get the actual values of
 * the system limits to handle resources marked as "unlimited".
 */
int
rlimit(int resource, rlim64_t softlimit, rlim64_t hardlimit)
{
	struct proc *p = ttoproc(curthread);

	rlim64_t actual_softlimit, actual_hardlimit;
	rlim64_t current_hardlimit;
	rlim64_t rlim_infinity = rlim_infinity_map[resource];

	if (softlimit == RLIM64_INFINITY && hardlimit != RLIM64_INFINITY)
		return (EINVAL);

	actual_softlimit = (softlimit == RLIM64_INFINITY) ?
		rlim_infinity : softlimit;
	actual_hardlimit = (hardlimit == RLIM64_INFINITY) ?
		rlim_infinity : hardlimit;

	if (actual_softlimit > actual_hardlimit)
		return (EINVAL);

#ifdef _LP64
	current_hardlimit = u.u_rlimit[resource].rlim_max;
#else
	/*
	 * Resource limits are now longlong's and therefore
	 * reads are no longer atomic.
	 */
	mutex_enter(&p->p_lock);
	current_hardlimit = u.u_rlimit[resource].rlim_max;
	mutex_exit(&p->p_lock);
#endif

	if (current_hardlimit == RLIM64_INFINITY)
		current_hardlimit = rlim_infinity;

	if (actual_hardlimit > current_hardlimit && !suser(CRED()))
		return (EPERM);

	if (actual_softlimit > rlim_infinity)
		softlimit = rlim_infinity;
	if (actual_hardlimit > rlim_infinity)
		hardlimit = rlim_infinity;

	/*
	 * Prevent multiple threads from updating the
	 * rlimit members in a random order.
	 */
	mutex_enter(&p->p_lock);
	u.u_rlimit[resource].rlim_cur = softlimit;
	u.u_rlimit[resource].rlim_max = hardlimit;
	mutex_exit(&p->p_lock);

	return (0);
}

#ifndef _LP64
/*
 * Keep these entry points for 32-bit systems but enforce the use
 * of MIN/MAX macros on 64-bit systems.  The DDI header files already
 * define min/max as macros so drivers shouldn't need these functions.
 */

int
min(int a, int b)
{
	return (a < b ? a : b);
}

int
max(int a, int b)
{
	return (a > b ? a : b);
}

uint_t
umin(uint_t a, uint_t b)
{
	return (a < b ? a : b);
}

uint_t
umax(uint_t a, uint_t b)
{
	return (a > b ? a : b);
}

#endif /* !_LP64 */

/*
 * Return bit position of least significant bit set in mask,
 * starting numbering from 1.
 */
int
ffs(long mask)
{
	int i;

	if (mask == 0)
		return (0);
	for (i = 1; i <= NBBY * sizeof (mask); i++) {
		if (mask & 1)
			return (i);
		mask >>= 1;
	}
	return (0);
}

/*
 * Parse suboptions from a string.
 * Same as getsubopt(3C).
 */
int
getsubopt(char **optionsp, char * const *tokens, char **valuep)
{
	char *s = *optionsp, *p;
	int i;
	size_t optlen;

	*valuep = NULL;
	if (*s == '\0')
		return (-1);
	p = strchr(s, ',');		/* find next option */
	if (p == NULL) {
		p = s + strlen(s);
	} else {
		*p++ = '\0';		/* mark end and point to next */
	}
	*optionsp = p;			/* point to next option */
	p = strchr(s, '=');		/* find value */
	if (p == NULL) {
		optlen = strlen(s);
		*valuep = NULL;
	} else {
		optlen = p - s;
		*valuep = ++p;
	}
	for (i = 0; tokens[i] != NULL; i++) {
		if ((optlen == strlen(tokens[i])) &&
		    (strncmp(s, tokens[i], optlen) == 0))
			return (i);
	}
	/* no match, point value at option and return error */
	*valuep = s;
	return (-1);
}

/*
 * Append the suboption string 'opt' starting at the position 'str'
 * within the buffer defined by 'buf' and 'len'. If 'buf' is not null,
 * a comma is appended first.
 * Return a pointer to the end of the resulting string (the null byte).
 * Return NULL if there isn't enough space left to append 'opt'.
 */
char *
append_subopt(const char *buf, size_t len, char *str, const char *opt)
{
	size_t l = strlen(opt);

	/*
	 * Include a ',' if this is not the first option.
	 * Include space for the null byte.
	 */
	if (strlen(buf) + (buf[0] != '\0') + l + 1 > len)
		return (NULL);

	if (buf[0] != '\0')
		*str++ = ',';
	(void) strcpy(str, opt);
	return (str + l);
}

/*
 * Tables to convert a single byte to/from binary-coded decimal (BCD).
 */
uchar_t byte_to_bcd[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
};

uchar_t bcd_to_byte[256] = {		/* CSTYLED */
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,
	10, 11, 12, 13, 14, 15, 16, 17, 18, 19,  0,  0,  0,  0,  0,  0,
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29,  0,  0,  0,  0,  0,  0,
	30, 31, 32, 33, 34, 35, 36, 37, 38, 39,  0,  0,  0,  0,  0,  0,
	40, 41, 42, 43, 44, 45, 46, 47, 48, 49,  0,  0,  0,  0,  0,  0,
	50, 51, 52, 53, 54, 55, 56, 57, 58, 59,  0,  0,  0,  0,  0,  0,
	60, 61, 62, 63, 64, 65, 66, 67, 68, 69,  0,  0,  0,  0,  0,  0,
	70, 71, 72, 73, 74, 75, 76, 77, 78, 79,  0,  0,  0,  0,  0,  0,
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89,  0,  0,  0,  0,  0,  0,
	90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
};

/*
 * Hot-patch a single instruction in the kernel's text.
 * If you want to patch multiple instructions you must
 * arrange to do it so that all intermediate stages are
 * sane -- we don't stop other cpus while doing this.
 * Size must be 1, 2, or 4 bytes with iaddr aligned accordingly.
 */
void
hot_patch_kernel_text(caddr_t iaddr, uint32_t new_instr, uint_t size)
{
	caddr_t vaddr;
	uintptr_t off = (uintptr_t)iaddr & PAGEOFFSET;

	vaddr = vmem_alloc(heap_arena, PAGESIZE, VM_SLEEP);

	hat_devload(kas.a_hat, vaddr, PAGESIZE, hat_getkpfnum(iaddr - off),
	    PROT_READ | PROT_WRITE, HAT_LOAD_LOCK | HAT_LOAD_NOCONSIST);

	switch (size) {
	case 1:
		*(uint8_t *)(vaddr + off) = new_instr;
		break;
	case 2:
		*(uint16_t *)(vaddr + off) = new_instr;
		break;
	case 4:
		*(uint32_t *)(vaddr + off) = new_instr;
		break;
	default:
		panic("illegal hot-patch");
	}

	membar_enter();
	sync_icache(vaddr + off, size);
	sync_icache(iaddr, size);
	hat_unload(kas.a_hat, vaddr, PAGESIZE, HAT_UNLOAD_UNLOCK);
	vmem_free(heap_arena, vaddr, PAGESIZE);
}

/*
 * Routine to report an attempt to execute non-executable data.  If the
 * address executed lies in the stack, explicitly say so.
 */
void
report_stack_exec(proc_t *p, caddr_t addr)
{
	if (!noexec_user_stack_log)
		return;

	if (addr < p->p_usrstack && addr >= (p->p_usrstack - p->p_stksize)) {
		cmn_err(CE_NOTE, "%s[%d] attempt to execute code "
		    "on stack by uid %d", p->p_user.u_comm,
		    p->p_pid, p->p_cred->cr_ruid);
	} else {
		cmn_err(CE_NOTE, "%s[%d] attempt to execute non-executable "
		    "data at 0x%p by uid %d", p->p_user.u_comm,
		    p->p_pid, (void *) addr, p->p_cred->cr_ruid);
	}

	delay(hz / 50);
}

/*
 * Determine whether the address range [addr, addr + len) is in memlist mp.
 */
int
address_in_memlist(struct memlist *mp, uint64_t addr, size_t len)
{
	while (mp != 0)	 {
		if ((addr >= mp->address) &&
		    (addr + len <= mp->address + mp->size))
			return (1);	 /* TRUE */
		mp = mp->next;
	}
	return (0);	/* FALSE */
}
