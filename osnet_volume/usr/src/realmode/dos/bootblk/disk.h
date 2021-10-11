/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _DISK_H
#define	_DISK_H

#ident	"@(#)disk.h	1.11	99/01/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef unsigned char   u_char;
typedef unsigned short  u_short;
typedef unsigned long   u_long;

#define SECSIZ	512
#define BSHIFT	9
#define BOFF	(SECSIZ - 1)
#define btodb(x) ((x) >> BSHIFT)
#define btodbr(x) (((x) + SECSIZ - 1) >> BSHIFT)
#define dbtob(x) ((x) << BSHIFT)

#define EQ(a,b) (strcmp_util(a, b) == 0)
#define EQN(a,b,n) (strncmp_util(a,b,n) == 0)
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define spc() (Fatc.f_bpb.bs_sectors_per_cluster)
#define bpc() (spc() * SECSIZ)
#define curdir() (Fatc.f_dclust)
#define setcurdir(c) (Fatc.f_dclust = c)
#define MAXCLST12FAT	4087		/* ... max clusters per 12bit fat */
#define TICKS_PER_SEC	18		/* It's really 18.2! */

struct _WORDREGS {
	u_short ax;
	u_short bx;
	u_short cx;
	u_short dx;
	u_short si;
	u_short di;
	u_short flag;
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

/* segment registers */

struct _SREGS {
	u_short es;
	u_short cs;
	u_short ss;
	u_short ds;
};

#define CARRY_FLAG	0x0001
#define PARITY_FLAG	0x0004
#define AUX_FLAG	0x0010
#define ZERO_FLAG	0x0040
#define SIGN_FLAG	0x0080
#define TRAP_FLAG	0x0100
#define INTEN_FLAG	0x0200
#define DIR_FLAG	0x0400
#define OVERFLOW_FLAG	0x0800

/*
 * Access permissions for dosAccess(), dosOpen()
 */
#define FILE_EXISTS     0x0001
#define FILE_READ       0x0002
#define FILE_WRITE      0x0004
#define FILE_CREATE	0x0008
#define FILE_TRUNC	0x0010

#define BREAD		2		/* ... read op */
#define BWRITE		3		/* ... write op */
typedef struct {
        u_char    b_drive_type;
        u_short   b_cylinders;
        u_char    b_sectors;
        u_char    b_heads;
} bios_data_t, *bios_data_p;

typedef struct {
	u_long	c_start;	/* ... starting sector of cache */
	u_short	c_spt;		/* ... sectors per track */
	u_short	c_fills;	/* ... number of track fills */
	char	*c_buf;		/* ... cache buffer */
	enum {CacheValid, CachePrimed, CacheInvalid} c_state;
} track_cache_t, *track_cache_p;

#define TYPE_EMPTY	0x00		/* undefined partition */
#define TYPE_DOS	0x13		/* causes fatInit() to search for
					 * active partition */
#define TYPE_DOS_12	0x01		/* partition with FAT12 filesys */
#define TYPE_DOS_16	0x04		/* partition with FAT16 filesys */
#define TYPE_DOS_EXT	0x05		/* not bootable, ignore */
#define TYPE_HUGH	0x06		/* HUGH partition */
#define TYPE_COMPAQ	0x12		/* Compaq's diag partition */
#define TYPE_SOLARIS	0x82
#define TYPE_SOLARIS_BOOT	0xBE	/* For "boot hill" project */

#define FDISK_START	0x1be		/* location in first sector where
					 * the fdisk starts.
					 */
#define FDISK_PARTS	4		/* Number of partitions in a fdisk */
#define FDISK_ACTIVE	0x80		/* indicates partition is active */
#define FDISK_INACTIVE	0x00		/*  " partition inactive */

#pragma pack(1)
struct _fdisk_partition_ {
        u_char  fd_active;
        u_char  fd_b_head;
        u_char  fd_b_sec;
        u_char  fd_b_cyl;
        u_char  fd_type;
        u_char  fd_e_head;
        u_char  fd_e_sec;
        u_char  fd_e_cyl;
	union {
	        long    fd_start_sec_long;
		struct {
			u_short low;
			u_short high;
		} s;
	} u;
        long    fd_part_len;
};
#define fd_start_sec u.fd_start_sec_long
#define fd_partition fd_type
typedef struct _fdisk_partition_ _fdisk_t, *_fdisk_p;
#pragma pack()

#pragma pack(1)
struct _boot_sector_ {
        u_char	bs_jump_code[3];
        char    bs_oem_name[8];
        short   bs_bytes_sector;
        u_char	bs_sectors_per_cluster;
        short   bs_resv_sectors;
        char    bs_num_fats;
        short   bs_num_root_entries;
        short   bs_sectors_in_volume;
        char    bs_media;
        short   bs_sectors_per_fat;
        short   bs_sectors_per_track;
        short   bs_heads;
        long    bs_hidden_sectors;
        long    bs_sectors_in_logical_volume;
        char    bs_phys_drive_num;
        char    bs_reserved;
        char    bs_ext_signature;
        long    bs_volume_id;
        char    bs_volume_label[11];
	char	bs_type[8];

	/* ---- ADDED BY SUNSOFT FOR MDBOOT ---- */
	u_short	bs_offset_high;
	u_short	bs_offset_low;
};
#pragma pack( )
typedef struct _boot_sector_  _boot_sector_t, *_boot_sector_p;

/*
 * Cluster types
 */
#define CLUSTER_AVAIL   0x00
#define CLUSTER_RES_12_0        0x0ff0  /* 12bit fat, first reserved */
#define CLUSTER_RES_12_6        0x0ff6  /* 12bit fat, last reserved */
#define CLUSTER_RES_16_0        0xfff0  /* 16bit fat, first reserved */
#define CLUSTER_RES_16_6        0xfff6  /* 16bit fat, last reserved */
#define CLUSTER_BAD_12  0x0ff7  /* 12bit fat, bad entry */
#define CLUSTER_BAD_16  0xfff7  /* 16bit fat, bad entry */
#define CLUSTER_EOF		0xfff8
#define CLUSTER_EOF_12_0        0x0ff8  /* 12bit fat, EOF first entry */
#define CLUSTER_EOF_12_8        0x0fff  /* 12bit fat, EOF last entry */
#define CLUSTER_EOF_16_0        0xfff8  /* 16bit fat, EOF first entry */
#define CLUSTER_EOF_16_8        0xffff  /* 16bit fat, EOF last entry */

/*
 * Cluster operations for allocation
 */
#define CLUSTER_NOOP		0x0001 /* just allocate cluster */
#define CLUSTER_ZEROFILL	0x0002 /* zero fill the alloc'd cluster */

#define CLUSTER_FIRST		0x0002
#define CLUSTER_ROOTDIR		0

/*
 * This structure is filled in by init_fat()
 */
typedef struct {
        union {
                _boot_sector_t  fu_bpb;  /* boot parameter block */
                u_char          fu_sector[SECSIZ];
        } fu;
        u_long	f_adjust;	/* starting sec for part. */
        u_long	f_rootsec;	/* root dir starting sec. */
        u_long	f_rootlen;	/* length of root in sectors */
        u_long	f_filesec;	/* adjustment for clusters */
        short	f_dclust;	/* cur dir cluster */
	int	f_nxtfree;	/* next free cluster */
	u_short	f_ncluster;	/* number of clusters in part. */
        char	f_16bit:1,	/* 1 if 16bit fat entries */
		f_flush:1;	/* flush the fat */
} fat_cntlr_t, *fat_cntlr_p;

#define f_bpb fu.fu_bpb
#define f_sector fu.fu_sector

#define NAMESIZ         8
#define EXTSIZ          3
#pragma pack(1)
struct _dir_entry_ {
        char    d_name[NAMESIZ];
        char    d_ext[EXTSIZ];
        u_char  d_attr;
        char    d_res[10];
        short   d_time;
        short   d_date;
        short	d_cluster;
        long    d_size;
};
#pragma pack( )
typedef struct _dir_entry_ _dir_entry_t, *_dir_entry_p;

/*
 * Number of entries in one sector
 */
#define DIRENTS (SECSIZ / sizeof(_dir_entry_t))

/*
 * Directory entry attributes
 */
#define DE_READONLY             0x01
#define DE_HIDDEN               0x02
#define DE_SYSTEM               0x04
#define DE_LABEL                0x08
#define DE_DIRECTORY            0x10
#define DE_ARCHIVE              0x20
#define DE_RESERVED1            0x40
#define DE_RESERVED2            0x80

struct _de_info_ {
	u_long	di_block;	/* block number dir entry */
	int	di_index;	/* index into block */
};
typedef struct _de_info_ _de_info_t, *_de_info_p;

struct _file_descriptor_ {
        short	f_startclust;	/* starting cluster number */
        long    f_off;		/* current offset */
        long    f_len;		/* size of file */
	_de_info_t f_di;	/* pointer to directory node */
};
typedef struct _file_descriptor_ _file_desc_t, *_file_desc_p;

struct malloc_s {
	struct malloc_s	*next;
	u_short size;
};
typedef struct malloc_s _malloc_t, *_malloc_p;

/*
 * Function prototypes
 */
/* ---- bios.c ---- */
void		init_bios(void);
void		InitCache_bios(void);
int		ReadSect_bios(char far *, u_long, int);

/* ---- dos.c ---- */
int		cd_dos(char *);
int		stat_dos(char *, _dir_entry_p);
_file_desc_p	open_dos(char *, int);
int		read_dos(_file_desc_p, char *, u_short);
int		write_dos(_file_desc_p, char *, u_short);
int		create_dos(char *, int);
int		readdir_dos(int, _dir_entry_p);
static void	copyup_dos(char *, char *, int);
static void	ftruncate_dos(_file_desc_p);
int		updatedir_dos(_file_desc_p);
static int	lookuppn_dos(char *, _dir_entry_p, _de_info_p);
static int	lookup_dos(char *, _dir_entry_p, short, _de_info_p);
static int	entrycmp_dos(char *, char *, char *ext);
static int	component_dos(char *, char *, int);
static int	namecmp_dos(char *, char *);

/* ---- fat.c ---- */
int		init_fat(int);
int		Fdisk_fat(int, _fdisk_p);
u_short		map_fat(u_short, int);
int		alloc_fat(_file_desc_p, int, int);
int		clalloc_fat(int);
void		setcluster_fat(int, int);
void		flush_fat(void);
u_long		ctodb_fat(u_short, int);

/* ---- util.c ---- */
static void	collape_util(_malloc_p);
static void	mallocinit_util(void);
u_short		malloc_util(u_short);
void		free_util(u_short, u_short);
void		clear_util(void);
void		setcursor_util(int, int);
void		putc_util(char);
char		getc_util(void);
int		nowaitc_util(void);
void		gets_util(char *, int);
void		exit_util(int);
void		setprint_util(int, int, char *, ... );
void		printf_util(char *, ... );
static char *	HandleSeq_util(char *);
static void	doprint_util(char *, ... );
static void	printnum_util(u_long, int, int, int);
int		isprint_util(char);
char		toupper_util(char);
int		validc_util(short, int);
int		strlen_util(char *);
int		strncmp_util(char *, char *, int);
int		strcmp_util(char *, char *);
u_long		strtol_util(char *, char **, int);
char *		strrchr_util(char *, char);
char *		strchr_util(char *, char);
void 		bcopy_util(char far *, char far *, int);
void		bzero_util(char *, int);
void		memset_util(char *, int, int);

/* ---- int.s ---- */
void bcopy_seg(char *, char *, int, int);
u_long get_time(void);
short ask_page(void);

#ifdef DEBUG
extern long Debug;

/*
 * Debug flags
 * These defines are really only used to keep track of which bits
 * are being used and where
 */
# define DBG_BIOS_0     0x0001  /* bios.c - cyl, head, sec */
# define DBG_BIOS_1     0x0002  /* bios.c - show reg info for int */
# define DBG_CAT_0      0x0004  /* test.c - turn off cat disp. */
# define DBG_LIST_0     0x0008  /* test.c - verbose listing */
# define DBG_LOOKUP_0   0x0010  /* dos.c - show each entry */
# define DBG_FAT_0	0x0020	/* fat.c - show detail fat figures */
# define DBG_DOS_CREAT	0x0040	/* dos.c - what happens during create */
# define DBG_DOS_UPD	0x0080	/* dos.c - directory update */
# define DBG_DOS_GEN	0x0100	/* dos.c - general debug stuff */
# define DBG_MALLOC	0x0200	/* util.c - malloc errors */

# define DPrint(f, p) if (Debug & f) printf_util p
#else
# define DPrint(f, p)
#endif

#ifdef	__cplusplus
}
#endif

#endif /* _DISK_H */
