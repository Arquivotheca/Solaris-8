#ifndef lint
static char sccsid[] = "@(#)adrm.c 1.6 98/08/26 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

/*
 * Adr memory based translations
 */

#include <stdio.h>
#include <sys/types.h>
#include <bsm/audit.h>
#include <bsm/audit_record.h>

void
#ifdef __STDC__
adrm_start(adr_t *adr, char *p)
#else
adrm_start(adr, p)
	adr_t *adr;
	char *p;
#endif
{
	adr->adr_stream = p;
	adr->adr_now = p;
}

/*
 * adrm_char - pull out characters
 */
void
#ifdef __STDC__
adrm_char(adr_t *adr, char *cp, int count)
#else
adrm_char(adr, cp, count)
	adr_t *adr;
	char *cp;
	int count;
#endif
{
	while (count--)
		*cp++ = *adr->adr_now++;
}

/*
 * adrm_short - pull out shorts
 */
void
#ifdef __STDC__
adrm_short(adr_t *adr, short *sp, int count)
#else
adrm_short(adr, sp, count)
	adr_t *adr;
	short *sp;
	int count;
#endif
{

	while (count--) {
		*sp = *adr->adr_now++ << 8;
		*sp++ += ((short)*adr->adr_now++) & 0x00ff;
	}
}

/*
 * adrm_int32 - pull out int
 */
#ifdef __STDC__
void adrm_int(adr_t *adr, int32_t *lp, int count);
void adrm_long(adr_t *adr, int32_t *lp, int count);
void
adrm_int32(adr_t *adr, int32_t *lp, int count)
#else
void adrm_int();
void adrm_long();
void
adrm_int32(adr, lp, count)
	adr_t *adr;
	int32_t *lp;
	int count;
#endif
#pragma weak adrm_int = adrm_int32
#pragma weak adrm_long = adrm_int32
{
	int i;

	for (; count--; lp++) {
		*lp = 0;
		for (i = 0; i < 4; i++) {
			*lp <<= 8;
			*lp += ((int32_t)*adr->adr_now++) & 0x000000ff;
		}
	}
}

void
#ifdef __STDC__
adrm_string(adr_t *adr, char *p)
#else
adrm_string(adr, p)
	adr_t *adr;
	char *p;
#endif
{
	short c;

	adrm_short(adr, &c, 1);
	adrm_char(adr, p, c);
}

void
#ifdef __STDC__
adrm_int64(adr_t *adr, int64_t *lp, int count)
#else
adrm_int64(adr, lp, count)
	adr_t *adr;
	int64_t *lp;
	int count;
#endif
{
	int i;

	for (; count--; lp++) {
		*lp = 0;
		for (i = 0; i < 8; i++) {
			*lp <<= 8;
			*lp += ((int64_t)*adr->adr_now++) & 0x00000000000000ff;
		}
	}
}


#ifdef __STDC__
void adrm_u_int(adr_t *adr, uint32_t *cp, int count);
void adrm_u_long(adr_t *adr, uint32_t *cp, int count);
void
adrm_u_int32(adr_t *adr, uint32_t *cp, int count)
#else
void adrm_u_int();
void adrm_u_long();
void
adrm_u_int32(adr, cp, count)
	adr_t *adr;
	uint32_t *cp;
	int count;
#endif
#pragma weak adrm_u_int = adrm_u_int32
#pragma weak adrm_u_long = adrm_u_int32
{
	adrm_int32(adr, (int32_t *)cp, count);
}

void
#ifdef __STDC__
adrm_opaque(adr_t *adr, char *p)
#else
adrm_opaque(adr, p)
	adr_t *adr;
	char *p;
#endif
{
	adrm_string(adr, p);
}

void
#ifdef __STDC__
adrm_u_char(adr_t *adr, u_char *cp, int count)
#else
adrm_u_char(adr, cp, count)
	adr_t *adr;
	u_char *cp;
	int count;
#endif
{
	adrm_char(adr, (char *)cp, count);
}

void
#ifdef __STDC__
adrm_u_int64(adr_t *adr, uint64_t *lp, int count)
#else
adrm_u_int64(adr, lp, count)
	adr_t *adr;
	uint64_t *lp;
	int count;
#endif
{
	adrm_int64(adr, (int64_t *)lp, count);
}

void
#ifdef __STDC__
adrm_u_short(adr_t *adr, u_short *sp, int count)
#else
adrm_u_short(adr, sp, count)
	adr_t *adr;
	u_short *sp;
	int count;
#endif
{
	adrm_short(adr, (short *)sp, count);
}
