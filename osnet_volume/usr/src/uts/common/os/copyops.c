/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)copyops.c	1.4	98/07/17 SMI"

#include <sys/types.h>
#include <sys/copyops.h>
#include <sys/systm.h>

struct copyops default_copyops = {
	default_copyin,
	default_xcopyin,
	default_copyout,
	default_xcopyout,
	default_copyinstr,
	default_copyoutstr,
	default_fuword8,
	default_fuiword8,
	default_fuword16,
	default_fuword32,
	default_fuiword32,
	default_fuword64,
	default_suword8,
	default_suiword8,
	default_suword16,
	default_suword32,
	default_suiword32,
	default_suword64,
	default_physio
};

int
copyin(const void *uaddr, void *kaddr, size_t count)
{
	return (CP_COPYIN(uaddr, kaddr, count));
}

int
xcopyin(const void *uaddr, void *kaddr, size_t count)
{
	return (CP_XCOPYIN(uaddr, kaddr, count));
}

int
copyout(const void *kaddr, void *uaddr, size_t count)
{
	return (CP_COPYOUT(kaddr, uaddr, count));
}

int
xcopyout(const void *kaddr, void *uaddr, size_t count)
{
	return (CP_XCOPYOUT(kaddr, uaddr, count));
}

int
copyinstr(const char *uaddr, char *kaddr, size_t maxlength, size_t *lencopied)
{
	return (CP_COPYINSTR(uaddr, kaddr, maxlength, lencopied));
}

int
copyoutstr(const char *kaddr, char *uaddr, size_t maxlength, size_t *lencopied)
{
	return (CP_COPYOUTSTR(kaddr, uaddr, maxlength, lencopied));
}

int
fuword8(const void *addr, uint8_t *valuep)
{
	return (CP_FUWORD8(addr, valuep));
}

int
fuiword8(const void *addr, uint8_t *valuep)
{
	return (CP_FUIWORD8(addr, valuep));
}

int
fuword16(const void *addr, uint16_t *valuep)
{
	return (CP_FUWORD16(addr, valuep));
}

int
fuword32(const void *addr, uint32_t *valuep)
{
	return (CP_FUWORD32(addr, valuep));
}

int
fuiword32(const void *addr, uint32_t *valuep)
{
	return (CP_FUIWORD32(addr, valuep));
}

int
fulword(const void *addr, ulong_t *valuep)
{
#ifdef _LP64
	return (CP_FUWORD64(addr, (uint64_t *)valuep));
#else
	return (CP_FUWORD32(addr, (uint32_t *)valuep));
#endif
}

int
fuword64(const void *addr, uint64_t *valuep)
{
	return (CP_FUWORD64(addr, valuep));
}

/*
 * This is used for copying out strings byte by byte.
 * Seems overly weird to force the caller to use suword8.
 */
int
subyte(void *addr, uchar_t value)
{
	return (CP_SUWORD8(addr, value));
}

int
suword8(void *addr, uint8_t value)
{
	return (CP_SUWORD8(addr, value));
}

int
suiword8(void *addr, uint8_t value)
{
	return (CP_SUIWORD8(addr, value));
}

int
suword16(void *addr, uint16_t value)
{
	return (CP_SUWORD16(addr, value));
}

int
suword32(void *addr, uint32_t value)
{
	return (CP_SUWORD32(addr, value));
}

int
suiword32(void *addr, uint32_t value)
{
	return (CP_SUIWORD32(addr, value));
}

int
sulword(void *addr, ulong_t value)
{
#ifdef _LP64
	return (CP_SUWORD64(addr, (uint64_t)value));
#else
	return (CP_SUWORD32(addr, (uint32_t)value));
#endif
}

int
suword64(void *addr, uint64_t value)
{
	return (CP_SUWORD64(addr, value));
}

int
physio(int (*strat)(struct buf *), struct buf *bp, dev_t dev,
    int rw, void (*mincnt)(struct buf *), struct uio *uio)
{
	return (CP_PHYSIO(strat, bp, dev, rw, mincnt, uio));
}

void
install_copyops(kthread_id_t tp, copyops_t *cp)
{
	ASSERT(tp->t_copyops == &default_copyops);
	tp->t_copyops = cp;
}

void
remove_copyops(kthread_id_t tp)
{
	ASSERT(tp->t_copyops != &default_copyops);
	tp->t_copyops = &default_copyops;
}

copyops_t *
get_copyops(kthread_id_t tp)
{
	return (tp->t_copyops);
}
