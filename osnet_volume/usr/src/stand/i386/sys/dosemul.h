/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_I386_SYS_DOSEMUL_H
#define	_I386_SYS_DOSEMUL_H

#pragma ident	"@(#)dosemul.h	1.25	99/10/07 SMI"

#include <sys/types.h>
#include <sys/bootlink.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *  Definitions related to file handling for real-mode modules.
 */
#define	DOSfile_ERROR	-1
#define	DOSfile_OK	0

#define	DOSfile_MINFD	5	/* 0-4 reserve for DOS_STDxxx */
#define	DOSfile_MAXFDS	20
#define	DOSfile_LASTFD	(DOSfile_MAXFDS - 1)

/*
 * Structure and defines for tracking chunks of memory
 * requested of "DOS".
 */
typedef struct dml {
	struct dml *next;
	int	seg;
	caddr_t addr;
	ulong_t	size;
} dmcl_t;

/*
 * Structure and defines for tracking fd's accessed via "DOS".
 */
#define	DOSACCESS_RDONLY	0
#define	DOSACCESS_WRONLY	1
#define	DOSACCESS_RDWR		2

#define	DOSSEEK_TOABS	0
#define	DOSSEEK_FROMFP	1
#define	DOSSEEK_FROMEOF	2

#define	DOSBOOTOPC_FN	"bootops"
#define	DOSBOOTOPR_FN	"bootops.res"

#define	DOSFD_INUSE	0x04
#define	DOSFD_BOOTOPC	0x10
#define	DOSFD_BOOTOPR	0x20
#define	DOSFD_STDDEV	0x40

#define	DOSFD_RAMFILE	0x100  /* specific request for RAM file */
#define	DOSFD_NOSYNC	0x200  /* RAM file should be not delayed-written */

typedef struct dffd {
	ulong_t	flags;
	int	actualfd;	/* if given file is from a file system */
} dffd_t;

/*
 * Find-File Info structure.  Needed to keep state between
 * findfile requests by user programs.
 */
typedef struct dffi {
	struct dffi *next;
	struct dos_fninfo *cookie;
	ushort_t curmatchattr;
	char *curmatchpath;
	char *curmatchfile;
	char *dentbuf;
	int dirfd;
	int requestreset;
	int curdoserror;
	int nextdentidx;
	int maxdent;
	int curdent;
} ffinfo;

/*
 * Signature that marks valid dos_fninfo structures
 */
#define	FFSIG	0x484D4954

/*
 * This is the header format for a loadable DOS driver.
 */
#pragma pack(1)
struct dos_drvhdr {
	ushort_t next_driver_offset;
	ushort_t next_driver_segment;
	ushort_t attrib_word;
	ushort_t strat_offset;
	ushort_t intr_offset;
	union {
		char name[8];
		struct dos_blkdrvrinfo {
			char	numunits;
			char	reserved[7];
		} blkinfo;
	} infosection;
};

/*
 * This is the minimal request structure sent to loadable DOS driver.
 */
struct dos_drvreq {
	uchar_t	reqlen;
	uchar_t	unit;
	uchar_t	command;
	ushort_t status;
	uchar_t	reserved[8];
	uchar_t	media;
	ulong_t	address;
	ushort_t count;
	ushort_t sector;
};

/*
 * exe header
 */
struct dos_exehdr {
	ushort_t sig;		/* EXE program signature */
	ushort_t nbytes;	/* number of bytes in last page */
	ushort_t npages;	/* number of 512-byte pages */
	ushort_t nreloc;	/* number of relocation table entries */
	ushort_t header_mem;	/* header size in paragraphs */
	ushort_t require_mem;	/* required memory size in paragraphs */
	ushort_t desire_mem;	/* desired memory size in paragraphs */
	ushort_t init_ss;	/* in relative paragraphs */
	ushort_t init_sp;
	ushort_t checksum;
	ushort_t init_ip;	/* at entry */
	ushort_t init_cs;	/* in paragraphs */
	ushort_t reloc_off;	/* offset of first reloc entry */
	ushort_t ovly_num;	/* overlay number */
	ushort_t reserved[16];
	ulong_t	newexe;		/* offset to additional header for new exe's */
};

struct dos_psp {
	ushort_t sig;
	ushort_t nxtgraf;
	uchar_t	skip1;
	uchar_t	cpmcall[5];
	ulong_t	isv22;
	ulong_t	isv23;
	ulong_t	isv24;
	ushort_t parent_id;
	uchar_t	htable[20];
	ushort_t envseg;
	ulong_t	savstk;
	ushort_t nhdls;
	ulong_t	htblptr;
	ulong_t	sharechn;
	uchar_t	skip2;
	uchar_t	trunamflg;
	uchar_t	skip3a[2];
	ushort_t version;
	uchar_t	skip3b[6];
	uchar_t	woldapp;
	uchar_t	skip4[7];
	uchar_t	disp[3];
	uchar_t	skip5[2];
	uchar_t	extfcb[7];
	uchar_t	fcb1[16];
	uchar_t	fcb2[20];
	uchar_t	tailc;
	uchar_t	tail[127];
};

struct dos_fcb {
	uchar_t	drive;
	char	name[8];
	char	ext[3];
	ushort_t curblock;
	ushort_t recsize;
	ulong_t	filesize;
	ushort_t cdate;
	ushort_t ctime;
	uchar_t	reserved[8];
	uchar_t	currec;
	ulong_t	relrec;
};

struct dos_efcb {
	uchar_t	sig;
	uchar_t	reserved1[5];
	uchar_t	attrib;
	uchar_t	drive;
	char	name[8];
	char	ext[3];
	ushort_t curblock;
	ushort_t recsize;
	ulong_t	filesize;
	ushort_t cdate;
	ushort_t ctime;
	uchar_t	reserved2[8];
	uchar_t	currec;
	ulong_t	relrec;
};

struct dos_fninfo {
	ulong_t	sig;
	ffinfo	*statep;
	uchar_t	rsrvd[13];
	uchar_t	attr;
	uchar_t	time[2];
	uchar_t	date[2];
	ulong_t	size;
	char    name[13];
};

struct dos_volinfo {
	ushort_t resvd;
	ulong_t	serial;
	char	volname[11];
	char	rest[8];
};
#pragma pack()

#define	EXE_SIG		0x5a4d		/* sig value in exehdr */
#define	EXE_HDR_SIZE	512		/* "standard" exe header size */
#define	COM_MEM_SIZE	(64*1024)	/* memory block required to run .com */
#define	EFCB_SIG	0xff

#define	RUN_OK		0	/* BSH Run command fully succeeded */
#define	RUN_FAIL	-1	/* BSH Run failed to execute */
#define	RUN_NOTFOUND	-2	/* BSH Run executable not found */

/*
 * Real mode memory access is limited to 20 bits of address space.
 * TOP_RMMEM marks upper bounds of where second level boot should
 * ever try to access from real-mode.
 */
#define	TOP_RMMEM	0xA0000 /* Highest code or data address */

/*
 * Int 21 (DOS soft interrupt) related defines
 */
#define	DOS_STDIN	0
#define	DOS_STDOUT	1
#define	DOS_STDERR	2
#define	DOS_STDAUX	3
#define	DOS_STDPRN	4

#define	DOS_ADRIVE	0
#define	DOS_BDRIVE	1
#define	DOS_CDRIVE	2
#define	DOS_DDRIVE	3

#define	DOS_CMDLINESIZE 256

#define	DOSVECTLEN	4	/* Real-mode vector table entry length */

#define	DOSDENTBUFSIZ	0x1000

/*
 * Restrictions on DOS file names
 */
#define	DOSMAX_NMPART	8
#define	DOSMAX_EXTPART	3
#define	DOSMAX_FILNAM	12

/*
 * DOS file attributes
 */
#define	DOSATTR_RDONLY		0x1
#define	DOSATTR_HIDDEN		0x2
#define	DOSATTR_SYSTEM		0x4
#define	DOSATTR_VOLLBL		0x8
#define	DOSATTR_DIR		0x10
#define	DOSATTR_ARCHIVE		0x20

#define	DOS_EOFCHAR		0x1A

#define	DS_DX(rp)	((char *)((rp)->ds*0x10+rp->edx.word.dx))
#define	DS_SI(rp)	((char *)((rp)->ds*0x10+rp->esi.word.si))

#define	ES_BX(rp)	((char *)((rp)->es*0x10+rp->ebx.word.bx))
#define	ES_DX(rp)	((char *)((rp)->es*0x10+rp->edx.word.dx))
#define	ES_SI(rp)	((char *)((rp)->es*0x10+rp->esi.word.si))
#define	ES_DI(rp)	((char *)((rp)->es*0x10+rp->edi.word.di))

#define	AX(rp)		((rp)->eax.word.ax)
#define	AH(rp)		((rp)->eax.byte.ah)
#define	AL(rp)		((rp)->eax.byte.al)
#define	BX(rp)		((rp)->ebx.word.bx)
#define	BH(rp)		((rp)->ebx.byte.bh)
#define	BL(rp)		((rp)->ebx.byte.bl)
#define	CX(rp)		((rp)->ecx.word.cx)
#define	CH(rp)		((rp)->ecx.byte.ch)
#define	CL(rp)		((rp)->ecx.byte.cl)
#define	DX(rp)		((rp)->edx.word.dx)
#define	DH(rp)		((rp)->edx.byte.dh)
#define	DL(rp)		((rp)->edx.byte.dl)

#define	BP(rp)		((rp)->ebp.word.bp)
#define	SI(rp)		((rp)->esi.word.si)
#define	DI(rp)		((rp)->edi.word.di)

/*
 * Int 21 Function 0xFE - Solaris OEM BIOS call sub functions (BX)
 */
#define	SOL_ACPI_COPY	0xFD	/* copy ACPI board from boot.bin to bootconf */
#define	SOL_DOS_PCOPY	0xFE	/* read-write protected memory range	*/
#define	SOL_DOS_DEBUG	0xFF	/* debugger for boot panic		*/

/*
 * Function prototypes
 */
extern int ftruncate(int fd, off_t where);
extern int create(char *fn, ulong_t attr);
extern int write(int fd, char *buf, int buflen);
extern int rename(char *ofn, char *nfn);
extern int unlink(char *fn);

extern	int	newdoint(int, struct real_regs *);
extern	int	olddoint(void);
extern	ushort_t peeks(ushort_t *);
extern	uchar_t peek8(uchar_t *);
extern	dmcl_t	*findmemreq(int, dmcl_t **);
extern	char	*dosfn_to_unixfn(char *);
extern	void	addmemreq(dmcl_t *);
extern	void	dosemul_init(void);
extern	void	dosbootop(int, char *, int);
extern	void	dosopenfile(struct real_regs *);
extern	void	doswritefile(struct real_regs *);
extern	void	dosallocpars(struct real_regs *);
extern	void	dosfreepars(struct real_regs *);
extern	void	dosreallocpars(struct real_regs *);
extern	void	dosfindfirst(struct real_regs *);
extern	void	dosfindnext(struct real_regs *);
extern	void	dosgetcwd(struct real_regs *);
extern	void	pokes(ushort_t *, ushort_t);
extern	void	poke8(uchar_t *, uchar_t);
extern	void	hook21(void);
extern	void	get_dosivec(int, ushort_t *, ushort_t *);
extern	void	set_dosivec(int, ushort_t, ushort_t);

#define	iswhitespace(c) \
	((c == ' ') || (c == '\r') || (c == '\n') || (c == '\t'))

#define	CLEAR_CARRY(rp) \
	((rp)->eflags &= (ulong_t)~CARRY_FLAG)

#define	SET_CARRY(rp) \
	((rp)->eflags |= (ulong_t)CARRY_FLAG)

#ifdef	__cplusplus
}
#endif

#endif	/* _I386_SYS_DOSEMUL_H */
