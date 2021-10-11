#include "disk.h"
#include "chario.h"

/*
 *[]------------------------------------------------------------[]
 * | Forward references						|
 *[]------------------------------------------------------------[]
 */
static void	list_test(char *);
static void	exit_test(char *);
static void	blk_test(char *);
static void	cat_test(char *);
static void	dump_test(char *);
static void	chdir_test(char *);
static void	fat_test(char *);
static void	fdisk_test(char *);
static void	mdboot_test(char *);
static void	params_test(char *);
static void	serial_test(char *);
static void	forth_test(char *);
static void	bitchar_test(char *, int);
static int	more_test(int *, char *, int);
static void	dumpdata_test(char *);

/*
 *[]------------------------------------------------------------[]
 * | Global definitions and externals.				|
 *[]------------------------------------------------------------[]
 */
#define ISTR_SIZE	32	/* ... size of input buffer for test */
#ifdef DEBUG
long    Debug = DBG_MALLOC;
static void	debug_test(char *);
#endif
extern fat_cntlr_t	Fatc;
extern bios_data_t	Biosd;
extern _char_io_t	console;
int			Verbose = 0;
int			PartType = 0;
int			BootDev = 0;
int			Meml = -1, Memsize;
char *			Mems;
char *			Memp;
char *			Month[] = { "", "Jan", "Feb", "Mar", "Apr", 
	"May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
char *			Attrlist = "\05d-\01rw\03s-\02h-\04v-\06a-";

typedef struct {
        char    *c_name;
        void    (*c_func)();
        char    *c_helpstr;
} cmd_tbl_t, *cmd_tbl_p;

cmd_tbl_t cmd_tbl[] = {
#ifdef DEBUG
        { "debug", debug_test, "Set global debug flag" },
#endif
        { "ls", list_test, "List all directory entries" },
        { "blk", blk_test, "List block numbers of <file>" },
        { "cat", cat_test, "Dump the contents of the file"},
        { "cd", chdir_test, "Change directory for other ops" },
        { "dump", dump_test, "Dump contents of sector"},
	{ "fat", fat_test, "Find the translation of a FAT cluster" },
	{ "fdisk", fdisk_test, "Edit fdisk table"},
	{ "mdboot", mdboot_test, "Write out mdboot to block"},
	{ "params", params_test, "Show disk parameters" },
	{ "serial", serial_test, "Setup serial comm port" },
	{ "forth", forth_test, "current test case" },
        { "exit", exit_test, "Exit program" },
	{ "reset", 0, "Reset the program" },
        { "", 0, "" },
};

/*
 *[]------------------------------------------------------------[]
 * | main_test -- this is a small program i whipped up to test	|
 * | various functions that are used in strap.com		|
 *[]------------------------------------------------------------[]
 */
main_test()
{
	char		s[ISTR_SIZE];	/* ... input buffer */
	cmd_tbl_p	p;		/* ... search for command to execute */
	u_long		n;		/* ... what boot device requested */

        printf_util("Enter drive number:[%d] ", BootDev);
        gets_util(s, ISTR_SIZE);
	n = (*s == '\0') ? BootDev : strtol_util(s, 0, 0);
	BootDev = n;

	/* ---- Show the fdisk table so that I can pick which type ---- */
	if (BootDev == 0x80) {
		init_bios();
		fdisk_test((char *)0);
		
		printf_util("Enter partition type: (default is Active partition) ");
		gets_util(s, ISTR_SIZE);
		PartType = (*s == '\0') ? TYPE_DOS : strtol_util(s, 0, 0);
	}
	else
		PartType = TYPE_DOS;

        init_fat(PartType);

        while(1) {
                printf_util("> ");
                gets_util(s, ISTR_SIZE);
                if (*s == '\0')
                        continue;
		if (strncmp_util(s, "reset", 5) == 0) {
			/* ---- should free up memory ---- */
			console.next = (_char_io_p)0;
			break;
		}
                for (p = &cmd_tbl[0]; p->c_name[0] != '\0'; p++)  {
                        if (!strncmp_util(s, p->c_name, 
                            strlen_util(p->c_name))) {
                                (*p->c_func)(strchr_util(s, ' '));
                                break;
                        }
                }
                if (*p->c_name == '\0') {
                        printf_util("Invalid function: %s\n", s);
                        for (p = &cmd_tbl[0]; *p->c_name != '\0'; p++)
                                printf_util("%20s: %s\n",
                                        p->c_name, p->c_helpstr);
                }
        }
}

/*
 *[]------------------------------------------------------------[]
 * | forth_test -- start the forth interpreter			|
 *[]------------------------------------------------------------[]
 */
static void
forth_test(char *s)
{
	extern _file_desc_p	inputfd;
	extern int		debugvar;
	_dir_entry_t		d;
	
	debugvar = 0x10;
	if (s) {
		stat_dos(++s, &d);
		inputfd = open_dos(s, 0);
		if (Mems = (char *)malloc_util((u_int)d.d_size)) {
			Memsize = Meml = d.d_size;
			Memp = Mems;
			if (read_dos(inputfd, Memp, Meml) != Meml) {
				printf_util("Failed to read in forth file\n");
				return;
			}
		}
	}
	forth_init();
	forth_call();
}

/*
 *[]------------------------------------------------------------[]
 * | serial_test -- enable a serial port			|
 *[]------------------------------------------------------------[]
 */
static void
serial_test(char *s)
{
	_char_io_p	p;	/* ... search though current list */
	
	if (!s)
		for (p = &console; p; p = p->next) {
			if (p->flags & CHARIO_DISABLED)
				printf_util("%s: Port has been disabled\n",
					p->name);
			else
				printf_util("%s: In %d, Out %d, Errs %d\n",
					p->name, p->in, p->out, p->errs);
		}
	else
		serial_init(strtol_util(s, (char **)0, 0));
}

/*
 *[]------------------------------------------------------------[]
 * | params_test -- dump out various bits of information	|
 * | regarding the FAT and BPB.					|
 *[]------------------------------------------------------------[]
 */
static void
params_test(char *s)
{
        printf_util("%10s : %d\n%10s : %d\n%10s : %d\n", 
        	"cyl", Biosd.b_cylinders, 
                "head", Biosd.b_heads, "sector", Biosd.b_sectors);
        printf_util("%10s : %ld\n%10s : %s\n%10s : %ld\n%10s : %ld\n",
                "Adjust", Fatc.f_adjust,
                "16Bit", Fatc.f_16bit ? "True" : "False",
                "Root Dir", Fatc.f_rootsec,
                "Root Len", Fatc.f_rootlen);
        printf_util("%10s : %d\n",
                "File Start", Fatc.f_filesec);
        printf_util("%10s : %d\n",
                "SecClust", spc());
        printf_util("OEM: %.8s, Label: %.11s, ID: %X\n",
                Fatc.f_bpb.bs_oem_name,
                Fatc.f_bpb.bs_volume_label,
                Fatc.f_bpb.bs_volume_id);
}

/*
 *[]------------------------------------------------------------[]
 * | mdboot_test -- create a new partition with our mdboot	|
 *[]------------------------------------------------------------[]
 */
static void
mdboot_test(char *s)
{
	_file_desc_p	fp;			/* ... to read mdboot */
	char		lblk[SECSIZ * 2];	/* ... holds mdboot in mem. */
	_boot_sector_p	bp;			/* ... create new bpb */
	_fdisk_t	fd;			/* ... which partition to wrt */
	long		bootlen;		/* ... how big is mdboot */
	_dir_entry_t	d;			/* ... info about mdboot */
	short		rdirsec, i, j;		/* ... for loop counts */
	int		oldBootDev;		/* ... saved value */
	char		q[ISTR_SIZE];		/* ... input buffer */
	char *		fat_rdir_p;		/* ... ptr to root blks */

	oldBootDev = -1;
	/* ---- see if mdboot is on current drive ---- */
	if (stat_dos("\\mdboot", &d)) {
		
		/* ---- if not & BootDev is hard disk, check the floppy ---- */
		if (BootDev & 0x80) {
			oldBootDev = BootDev;
			BootDev = 0x00;
			init_fat(TYPE_DOS);
			if (stat_dos("\\mdboot", &d)) {
				BootDev = oldBootDev;
				init_fat(PartType);
				printf_util("Can't find mdboot\n");
				return;
			}
		}
		else {
			printf_util("Failed to find mdboot on floppy\n");
			return;
		}
	}	
	
	if (d.d_size > (SECSIZ * 2)) {
		printf_util("mdboot to big.\n");
		return;
	}
	
	if ((fp = open_dos("\\mdboot", FILE_READ)) == (_file_desc_p)0) {
		printf_util("Can't open mdboot\n");
		return;
	}
	
	if ((bootlen = read_dos(fp, &lblk[0], SECSIZ * 2)) <= 0) {
		printf_util("Failed to read mdboot program\n");
		return;
	}
	if (bootlen != d.d_size) {
		printf_util("Invalid size returned from dosRead.\nNeeded %d bytes, got %d\n",
			d.d_size, bootlen);
		return;
	}

	bp = (_boot_sector_p)&lblk[0];
	printf_util("MDBboot Name: %.8s Id: %X\n",
		bp->bs_oem_name, bp->bs_volume_id);
	
	if (!(BootDev & 0x80)) {
		if (oldBootDev == -1)
			oldBootDev = BootDev;
		BootDev = 0x80;
		init_bios();
	}
	fdisk_test((char *)0);
	printf_util("Select partition number: ");
	gets_util(q, ISTR_SIZE);
	
	FdiskPart_fat((int)strtol_util(q, (char **)0, 0), &fd);

	if (fd.fd_part_len > 0xffff)
	  bp->bs_sectors_in_volume = 0;
	else
	  bp->bs_sectors_in_volume = fd.fd_part_len;
	bp->bs_sectors_in_logical_volume = fd.fd_part_len;
	bp->bs_sectors_per_track = Biosd.b_sectors;
	bp->bs_phys_drive_num = 0x80;
	bp->bs_heads = Biosd.b_heads;
	bp->bs_sectors_per_cluster = 1;
	bp->bs_media = 0xf8;
	bp->bs_num_root_entries = 224;
	bp->bs_offset_high = fd.u.s.high;
	bp->bs_offset_low = fd.u.s.low;
	bp->bs_num_fats	= 2;
	
	/*
	 * Create fat table based on the size of disk. If more than 4087
	 * sectors use 16bit fats with 256 cluster per sector. Else use
	 * 340 (it's really 341.3333) for the 12bit fat
	 * XXX At some point I need to write a 32bit divide routine using
	 * 16bit registers so that instead of doing the shift I can divide
	 * by 256 or 341.
	 */
	if (fd.fd_part_len > MAXCLST12FAT)
		bp->bs_sectors_per_fat = fd.fd_part_len >> 8;
	else
		bp->bs_sectors_per_fat = fd.fd_part_len >> 8;

	printf_util("start=%X part_len = %X sectors_per_fat = 0x%x\n",
	      fd.fd_start_sec, fd.fd_part_len, bp->bs_sectors_per_fat);
	
	if (WriteSect_bios(&lblk[0], fd.fd_start_sec, 1)) {
		printf_util("Failed to write mdboot at block %ld (0x%lx)\n",
			fd.fd_start_sec, fd.fd_start_sec);
		return;
	}
	fd.fd_start_sec++;
	bootlen -= SECSIZ;
	fat_rdir_p = (char *)malloc_util(2 * SECSIZ);
	fat_rdir_p[0] = bp->bs_media;
	fat_rdir_p[1] = 0xff;
	fat_rdir_p[2] = 0xff;
	if (fd.fd_part_len > 4079) {
		fat_rdir_p[3] = 0xff;
		fat_rdir_p[4] = CLUSTER_BAD_16 & 0xff;
		fat_rdir_p[5] = (CLUSTER_BAD_16 >> 8) & 0xff;
	}
	else {
		fat_rdir_p[3] = CLUSTER_BAD_12 & 0xff;
		fat_rdir_p[4] = (CLUSTER_BAD_12 >> 8) & 0x0f;
	}
	
	for (i = 0; i < bp->bs_num_fats; i++) {
		if (WriteSect_bios(fat_rdir_p, fd.fd_start_sec, 1)) {
			printf_util("Failed to write fat numbar %d\n", i + 1);
			free_util((u_int)fat_rdir_p, 2*SECSIZ);
			return;
		}
		fd.fd_start_sec++;
		for (j = 0; j < (bp->bs_sectors_per_fat - 1); j++) {
			if (WriteSect_bios(&fat_rdir_p[SECSIZ], 
				fd.fd_start_sec, 1)) {
				printf_util("Failed to write fat num %d\n",
					 i + 1);
				free_util((u_int)fat_rdir_p, 2*SECSIZ);
				return;
			}
			fd.fd_start_sec++;
		}
	}
	rdirsec = (sizeof(_dir_entry_t) * bp->bs_num_root_entries) / SECSIZ;
	bzero_util(fat_rdir_p, SECSIZ);
	for (i = 0; i < rdirsec; i++) {
		if (WriteSect_bios(fat_rdir_p, fd.fd_start_sec, 1)) {
			printf_util("Failed to write directory block %ld\n",
				fd.fd_start_sec);
			free_util((u_int)fat_rdir_p, 2*SECSIZ);
			return;
		}
		fd.fd_start_sec++;
	}
	printf_util("Second block being written at %ld\n", fd.fd_start_sec);
	printf_util("block[0....3]= %02x %02x %02x %02x\n",
		lblk[SECSIZ], lblk[SECSIZ+1], lblk[SECSIZ+2], lblk[SECSIZ+3]);
	if (WriteSect_bios(&lblk[SECSIZ], fd.fd_start_sec, 1)) {
		printf_util("Failed to write last block of mdboot at %ld\n",
			fd.fd_start_sec);
		free_util((u_int)fat_rdir_p, 2*SECSIZ);
		return;
	}
	
	free_util((u_int)fat_rdir_p, 2*SECSIZ);
	if (oldBootDev != -1)
		init_fat(PartType);
}

/*
 *[]------------------------------------------------------------[]
 * | fdisk_test -- print out fdisk table with the option to	|
 * | modify it if need be.					|
 *[]------------------------------------------------------------[]
 */
static void
fdisk_test(char *s)
{
	char lbuf[SECSIZ];
	short *p;
	_fdisk_p fd;
	int i, num, dowrite = 0;
	u_long size, lastsec, SpC;
	
	if (ReadSect_bios((char *)&lbuf[0], 0, 1)) {
		printf_util("Can't read fdisk info\n");
		return;
        }
        p = (short *)&lbuf[SECSIZ - sizeof(short)];
        if (*p != 0xaa55) {
		printf_util("Didn't find a fdisk partition\n");
		return;
	}
        fd = (_fdisk_p)&lbuf[FDISK_START];
	if (s && *++s != 0) {
		num = strtol_util(s, &s, 0);
		if (strncmp_util(s, " type", 5) == 0) {
			fd[num].fd_type = strtol_util(s + 5, 0, 0);
			dowrite = 1;
		}
		else if (strncmp_util(s, " active", 7) == 0) {
			for (i = 0; i < FDISK_PARTS; i++, fd++) {
				if (fd->fd_active == FDISK_ACTIVE)
					fd->fd_active = FDISK_INACTIVE;
				if (i == num)
					fd->fd_active = FDISK_ACTIVE;
			}
			dowrite = 1;
		}
		else if (strncmp_util(s, " size", 5) == 0) {
			/* ---- determine partition size in bytes ---- */
			size = strtol_util(s + 5, &s, 0);

			/* ---- multiple by Mb or Kb if requested, normally true ---- */
			if (*s == 'm')
				size = size * (u_long)1024 * (u_long)1024;
			else if (*s == 'k')
				size *= 1024;
				
			/* ---- back that down to sectors ---- */
			size = size / 512;
			printf_util("Requested # of sectors %X\n", size);
			
			/* ---- round up to cylinders ---- */
			SpC = Biosd.b_sectors * Biosd.b_heads;
			size = ((size + SpC - 1) / SpC) * SpC;
			printf_util("Rounded up to cylinder bouder, sector %X\n", size);
			
			/* ---- reclaim the space of requested partition ---- */
			fd[num].fd_part_len = 0;
			
			/* XXXX
			 * Need some intelligence here. Currently we just find
			 * the last sector in use. That doesn't take into
			 * accout any holes that might be large enough to use.
			 */
			for (lastsec = 0, i = 0; i < 4; i++)
				if (fd[i].fd_start_sec && 
				   ((fd[i].fd_start_sec + fd[i].fd_part_len) > lastsec))
				   	lastsec = fd[i].fd_start_sec + fd[i].fd_part_len;
			
			/* ---- stuff values into requested partition ---- */
			fd[num].fd_start_sec = lastsec;
			fd[num].fd_part_len = size;
			
			/* ---- have the data written out ---- */
			dowrite = 1;
		}
		else
			printf_util("Invalid choice %s\n", s);
	}
        fd = (_fdisk_p)&lbuf[FDISK_START];
	printf_util("Part. Active  Head  Sec  Cyl  Type  Sec.Start  Sec.Len\n");
        for (i = 0; i < 4; i++, fd++) {
		printf_util("%2d:     %02x     %02x    %02x   %02x   %02x   %X  %X\n",
			i, fd->fd_active, fd->fd_b_head, fd->fd_b_sec,
			fd->fd_b_cyl, fd->fd_type, 
			fd->fd_start_sec, fd->fd_part_len);
        }

	if (dowrite) {
		printf_util("Updating fdisk partition\n");
		if (WriteSect_bios((char *)&lbuf[0], 0, 1)) {
			printf_util("Can't write fdisk info\n");
			return;
	        }
	}
}

/*
 *[]------------------------------------------------------------[]
 * | fat_test -- show what the next mapping is for a cluster	|
 *[]------------------------------------------------------------[]
 */
static void
fat_test(char *s)
{
	int c;

	if (s == 0)
		return;
	c = strtol_util(s, 0, 0);
	printf_util("Cluster %d becomes sector %d\n", c, map_fat(c, 0));
}

#ifdef DEBUG
/*
 *[]------------------------------------------------------------[]
 * | debug_test -- set the debug variable.			|
 *[]------------------------------------------------------------[]
 */
static void
debug_test(char *s)
{
        long v;
	if (s == 0)
		return;
	v = strtol_util(s, 0, 0);
        printf_util("Debug: 0x%lx -> 0x%lx\n", Debug, v);
	Debug = v;
}
#endif

/*
 *[]------------------------------------------------------------[]
 * | chdir_test -- change the current directory			|
 *[]------------------------------------------------------------[]
 */
static void
chdir_test(char *s)
{
        while (s && *s && *s == ' ')
                s++;
        if (s && *s) {
                if (cd_dos(s))
                        printf_util("cd: bad directory %s\n", s);
        }
        else
                printf_util("cd: missing arg\n");
}

/*
 *[]------------------------------------------------------------[]
 * | list_test -- list the current directory contents.		|
 *[]------------------------------------------------------------[]
 */
static void
list_test(char *s)
{
        int di = 0, current, i = 0;
        _dir_entry_t d;
        int newline = 0;
        long total_bytes = 0;
        int total_files = 0;
	int verbose = 0;
	
	if (s && s[1] == '-' && s[2] == 'a')
		verbose = 1;
        do {
                current = di;
                di = readdir_dos(di, &d);
                if (di > 0) {
                        if ((u_char)d.d_name[0] == (u_char)0xe5)
                                continue;
                        else if (d.d_name[0] == 0) {
                                break;  /* EOD */
                        }
                        else if (!(d.d_attr & DE_HIDDEN) || verbose) {
                                for (i = 0; i < 8; i++)
                                        if (d.d_name[i] == ' ')
                                                d.d_name[i] = '\0';
                                printf_util("%05u %8.8s%c%.3s %10ld  %02d:%02d:%02d %02d-%s-19%d  ",
                                        d.d_cluster,
                                        d.d_name,
                                        d.d_ext[0] == ' ' ? ' ' : '.',
                                        d.d_ext,
                                        d.d_size,
                                        (d.d_time >> 0xB) & 0x1f,
                                        (d.d_time >> 0x5) & 0x3f,
                                        (d.d_time & 0x1f) * 2,
                                        d.d_date & 0x1f,
                                        Month[(d.d_date >> 0x5) & 0x0f],
                                        ((d.d_date >> 0x9) & 0x7f) + 80);
                                bitchar_test(Attrlist, d.d_attr);
                                total_files++;
                                total_bytes += d.d_size;
                                putc_util('\r');
                                if (!more_test(&newline, "\n", 20))
                                        return;
                        }
                }
        } while (di != 0 && di != -1);
        printf_util("\t%d files(s)\t%ld bytes\n", total_files, total_bytes);
}

/*
 *[]------------------------------------------------------------[]
 * | cat_test -- display the contents of a file			|
 *[]------------------------------------------------------------[]
 */
static void
cat_test(char *s)
{
        _file_desc_p fp;
        long rtn;
        int newline, i;
        char ch, *lblk;
        long mycnt;

	if (s == 0)
		return;
        if ((fp = open_dos(++s, FILE_READ)) == (_file_desc_p)0) {
                printf_util("Can't open file\n");
                return;
        }
        newline = 0;
        mycnt = 0;
        if (!(lblk = (char *)malloc_util(4096))) {
        	printf_util("Can't malloc enough memory for read\n");
        	return;
        }
        do {
                if ((rtn = read_dos(fp, lblk, 4096)) == -1) {
                        printf_util("Read failed\n");
                        return;
                }
                if (rtn == 0)
			break;
                mycnt += rtn;
#ifdef DEBUG
                if (!(Debug & DBG_CAT_0))
#endif
                        for (i = 0; i < rtn; i++) {
                                if (lblk[i] == '\n') {
                                        if (!more_test(&newline, "\n", 20))
						break;
                                }
                                else
                                        putc_util(lblk[i]);
                        }
        } while(1);
        free_util((u_int)lblk, 4096);
}

/*
 *[]------------------------------------------------------------[]
 * | blk_test -- show the cluster chain for a file		|
 *[]------------------------------------------------------------[]
 */
static void
blk_test(char *n)
{
        _dir_entry_t d;		/* ---- need to find the starting cluster */
        u_short c, cn;		/* ---- current and next clusters */
        int contig = 1;		/* ---- prove that the file is not contig. */

	if (n++ == 0)
		return;
		
        if (stat_dos(n, &d) == -1) {
                printf_util("Failed to open %s\n", n);
                return;
        }
        printf_util(" %s: %x:%lx ", n, d.d_cluster, 
		ctodb_fat(d.d_cluster, 0));
        c = d.d_cluster;
        while (validc_util(c, 0)) {
                cn = map_fat(c, 0);
                printf_util("%x:%lx ", cn, ctodb_fat(cn, 0));
                if (contig && validc_util(cn, 0)) contig = (c + 1) == cn;
                c = cn;
        }
        printf_util("File is %scontiguous\n", !contig ? "not " : "");
}

/*
 *[]------------------------------------------------------------[]
 * | dump_test -- dump a absolute sector on the disk		|
 *[]------------------------------------------------------------[]
 */
static void
dump_test(char *s)
{
        u_long sector;
        char ch;
	char lblk[SECSIZ];

	if (s == 0)
		return;
        sector = strtol_util(s, 0, 0);
        while (1) {
                printf_util("Read sector %ld\n", sector);
                if (ReadSect_bios((char *)&lblk[0],
                    sector, 1)) {
                        printf_util(" -- Failed --\n");
                          return;
                }
                dumpdata_test(lblk);
                printf_util("Next sector? (n = no)");
                ch = getc_util();
                printf_util("\n");
                if (ch == 'n')
                        return;
                sector++;
        }
}

/*
 *[]------------------------------------------------------------[]
 * | exit_test -- exit program. only works under DOS		|
 *[]------------------------------------------------------------[]
 */
static void
exit_test(char *s)
{
	exit_util(1);
}

/*
 *[]------------------------------------------------------------[]
 * | more_test -- will pause display with a question. If user	|
 * | wishes to continue or not enough lines have been displayed	|
 * | this routine will return a 1. Else 0.			|
 *[]------------------------------------------------------------[]
 */
static
more_test(int *newline, char *extra, int lines)
{
        char ch;

        if ((++*newline % lines) == 0) {
                printf_util("%s<More?>[yn] ", extra);
                ch = getc_util();
                if (ch == 'n') {
                        printf_util("\n");
                        return 0;
                }
                else
                        printf_util("\r            \r");
        }
        else
                putc_util('\n');
        return 1;
}

/*
 *[]------------------------------------------------------------[]
 * | dumpdata_test -- print binary data in human readable form.	|
 *[]------------------------------------------------------------[]
 */
static void
dumpdata_test(char *sector)
{
        char *p;
        int i, j;
        char ch;
        int newline = 0;

        p = sector;
        for (j = 0; j < (512 / 16); j++) {
                if (j && !more_test(&newline, "\n", 16))
                        return;
                printf_util("%04x: ", j * 16);
                for (i = 0; i < 16; i++)
                        printf_util("%02x ", p[i] & 0xff);
                printf_util("  ");
                for (i = 0; i < 16; i++)
                        if (isprint_util(p[i]))
                                printf_util("%c", p[i]);
                        else
                                printf_util(".");
                p += 16;
                putc_util('\r');
        }
	printf_util("\n");
}

/*
 *[]------------------------------------------------------------[]
 * | bitchar_test -- print out ascii characters for bit fields	|
 *[]------------------------------------------------------------[]
 */
static void
bitchar_test(char *bitvalstr, int val)
{
        while (*bitvalstr)
                if ((1 << (*bitvalstr++ - 1)) & val) {
                        putc_util(*bitvalstr++);
                        bitvalstr++;
                }
                else {
                        bitvalstr++;
                        putc_util(*bitvalstr++);
                }
}






