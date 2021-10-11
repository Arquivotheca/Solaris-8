/*
 *	@(#)global.h	1.2
 */

#define ZERO_FLAG 0x40		/* bit 5 in the status reg is the zero flag */
#define MK_FP(seg, off) (void __far *) \
		( (((unsigned long)(seg)) << 16) | (unsigned long)(off))

typedef unsigned char	u_char;
typedef unsigned short	u_short;
typedef unsigned int	u_int;
typedef unsigned long	u_long;

struct _WORDREGS {
	u_int ax;
	u_int bx;
	u_int cx;
	u_int dx;
	u_int si;
	u_int di;
	u_int flag;
};

/* byte registers */

struct _BYTEREGS {
	u_char al, ah;
	u_char bl, bh;
	u_char cl, ch;
	u_char dl, dh;
};

union _REGS {
	struct _WORDREGS x;
	struct _BYTEREGS h;
};
