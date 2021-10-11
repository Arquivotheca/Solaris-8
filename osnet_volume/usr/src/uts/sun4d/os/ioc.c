/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)ioc.c	1.18	95/01/25 SMI"

#include <sys/types.h>

#include <sys/cmn_err.h>

#include <sys/machparam.h>
#include <sys/physaddr.h>
#include <sys/iocache.h>

#define	SBI_FWB		0x1
#define	SBI_IRB		0x2
#define	SBI_RSB		0x4

#define	VA_SBI_S0_BUF_CNTL(va_sbi)	(va_sbi + OFF_SBI_SLOT0_STBUF_CONTROL)
#define	VA_SBI_S1_BUF_CNTL(va_sbi)	(va_sbi + OFF_SBI_SLOT1_STBUF_CONTROL)
#define	VA_SBI_S2_BUF_CNTL(va_sbi)	(va_sbi + OFF_SBI_SLOT2_STBUF_CONTROL)
#define	VA_SBI_S3_BUF_CNTL(va_sbi)	(va_sbi + OFF_SBI_SLOT3_STBUF_CONTROL)

#define	VA_SBI_S0_CONFIG(va_sbi)	(va_sbi + OFF_SBI_SLOT0_CONFIG)
#define	VA_SBI_S1_CONFIG(va_sbi)	(va_sbi + OFF_SBI_SLOT1_CONFIG)
#define	VA_SBI_S2_CONFIG(va_sbi)	(va_sbi + OFF_SBI_SLOT2_CONFIG)
#define	VA_SBI_S3_CONFIG(va_sbi)	(va_sbi + OFF_SBI_SLOT3_CONFIG)

#define	VA_SBI_INTR_STATE(va_sbi)	(va_sbi + OFF_SBI_INTR_STATE)
#define	VA_SBI_INTR_TARGET_ID(va_sbi)	(va_sbi + OFF_SBI_INTR_TARGET_ID)

int stream_dvma = 1;
int consis_dvma = 1;

static void clr_sbus_strbuf(caddr_t p_sbi_cntl);

void
stream_dvma_init(caddr_t va_sbi)
{
	/* reset stream buffers on SBI */
	if (stream_dvma) {
		clr_sbus_strbuf(VA_SBI_S0_BUF_CNTL(va_sbi));
		clr_sbus_strbuf(VA_SBI_S1_BUF_CNTL(va_sbi));
		clr_sbus_strbuf(VA_SBI_S2_BUF_CNTL(va_sbi));
		clr_sbus_strbuf(VA_SBI_S3_BUF_CNTL(va_sbi));
	}

}

/* see sun4d arch. for algorithm to clear sbus stream buffers */
static void
clr_sbus_strbuf(caddr_t p_sbi_cntl)
{
	volatile u_int *ptmp;

	if (stream_dvma) {
		ptmp = (u_int*) p_sbi_cntl;

		*ptmp = SBI_FWB | SBI_IRB | SBI_RSB;

		while ((*ptmp & SBI_FWB) || (*ptmp & SBI_IRB))
			;
		*ptmp = 0x0;
	}

}

void
flush_sbus_wrtbuf(caddr_t va_sbi, int slot_id)
{
	volatile u_int *ptmp;

	if (stream_dvma == 0)
		return;

	switch (slot_id) {
	case 0:
		ptmp = (u_int*) VA_SBI_S0_BUF_CNTL(va_sbi);
		break;

	case 1:
		ptmp = (u_int*) VA_SBI_S1_BUF_CNTL(va_sbi);
		break;

	case 2:
		ptmp = (u_int*) VA_SBI_S2_BUF_CNTL(va_sbi);
		break;

	case 3:
		ptmp = (u_int*) VA_SBI_S3_BUF_CNTL(va_sbi);
		break;

	default:
		panic("flush_sbuf_wrtbuf: bad slot id\n");

	}

	*ptmp |= SBI_FWB;
	while (*ptmp & SBI_FWB)
		;
}

void
invalid_sbus_rdbuf(caddr_t va_sbi, int slot_id)
{
	volatile u_int *ptmp;

	if (stream_dvma == 0)
		return;

	switch (slot_id) {
	case 0:
		ptmp = (u_int*) VA_SBI_S0_BUF_CNTL(va_sbi);
		break;

	case 1:
		ptmp = (u_int*) VA_SBI_S1_BUF_CNTL(va_sbi);
		break;

	case 2:
		ptmp = (u_int*) VA_SBI_S2_BUF_CNTL(va_sbi);
		break;

	case 3:
		ptmp = (u_int*) VA_SBI_S3_BUF_CNTL(va_sbi);
		break;

	default:
		panic("flush_sbuf_wrtbuf: bad slot id\n");

	}

	*ptmp |= SBI_IRB;
	while (*ptmp & SBI_IRB)
		;
}

u_int *
get_slot_ctlreg(caddr_t va_sbi, int slot_id)
{
	u_int *ptmp;

	switch (slot_id) {
	case 0:
		ptmp = (u_int *) VA_SBI_S0_BUF_CNTL(va_sbi);
		break;

	case 1:
		ptmp = (u_int *) VA_SBI_S1_BUF_CNTL(va_sbi);
		break;

	case 2:
		ptmp = (u_int *) VA_SBI_S2_BUF_CNTL(va_sbi);
		break;

	case 3:
		ptmp = (u_int *) VA_SBI_S3_BUF_CNTL(va_sbi);
		break;

	default:
		panic("flush_sbuf_wrtbuf: bad slot id\n");

	}

	return (ptmp);
}

int
get_sbus_burst_size(caddr_t va_sbi, int slot_id)
{

	u_int *ptmp, tmp;

	switch (slot_id) {
	case 0:
		ptmp = (u_int*) VA_SBI_S0_CONFIG(va_sbi);
		break;

	case 1:
		ptmp = (u_int*) VA_SBI_S1_CONFIG(va_sbi);
		break;

	case 2:
		ptmp = (u_int*) VA_SBI_S2_CONFIG(va_sbi);
		break;

	case 3:
		ptmp = (u_int*) VA_SBI_S3_CONFIG(va_sbi);
		break;

	default:
		panic("get_sbus_burst_size: bad slot id\n");
	}

	tmp = *ptmp;

	/*
	 * extract 8, 16, 32, 64 burst mode bits from config reg and set
	 * it on tmp.
	 */
	tmp = ((tmp & 0x1e) << 2) | 0x7;

	return (tmp);

}

void
set_sbus_burst_size(caddr_t va_sbi, int slot_id, int burstsize)
{

	volatile u_int *ptmp;
	u_int tmp;

	switch (slot_id) {
	case 0:
		ptmp = (u_int*) VA_SBI_S0_CONFIG(va_sbi);
		break;

	case 1:
		ptmp = (u_int*) VA_SBI_S1_CONFIG(va_sbi);
		break;

	case 2:
		ptmp = (u_int*) VA_SBI_S2_CONFIG(va_sbi);
		break;

	case 3:
		ptmp = (u_int*) VA_SBI_S3_CONFIG(va_sbi);
		break;

	default:
		panic("set_sbus_busrt_size: bad slot id\n");
	}

	tmp = *ptmp;

	/* clr the burst mode bits */
	tmp &= (~0x1e);

	/*
	 * extract 8, 16, 32, 64 burst mode bits from burstsize and set
	 * it on tmp.
	 */
	tmp |= ((burstsize >> 2) & 0x1e);

	*ptmp = tmp;
}

u_int
get_sbus_intr_id(caddr_t va_sbi)
{
	u_int *ptmp;

	ptmp = (u_int *) VA_SBI_INTR_TARGET_ID(va_sbi);
	return (*ptmp);
}

u_int
set_sbus_intr_id(caddr_t va_sbi, u_int new_id)
{
	volatile u_int *ptmp;
	u_int tmp;

	ptmp = (u_int *) VA_SBI_INTR_TARGET_ID(va_sbi);
	tmp = *ptmp;

	*ptmp = new_id;

	return (tmp);
}
