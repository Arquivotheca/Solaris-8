/*
 * Copyright (c) 1995 by Sun Microsystems, Inc
 *
 * Strap.com
 * I'm sure the name will change at some point but for histories
 * sake we'll keep it around in the file.
 *
 * Project    : Boot Hill
 * Date       : 10-Aug-1994
 * Programmer : Rick McNeal
 *
 * Object of program is to boot what's being called the second level
 * boot program. This second level boot is now a combo of ufsboot and
 * inetboot for the x86 world.
 *
 * Here's the steps
 * 0) Let user pick which OS to run if more than one is available. 
 *	We take a look at the fdisk a see how many bootable partitions
 *	are available. The Solaris partition will be ignored and only
 *	the Solaris Boot partition will count. This will give the user
 *	a choice between Solaris and DOS.
 * 1) See if there's a file called \Solaris\locale
 *      If so, read the contents, which should be no more than
 *      8 bytes. Use that as an additional level of indirection.
 * 2) Check for \Solaris\<LangOp>\Strap.rc
 *      If available, display the first string of each line, which
 *      is enclosed in double quotes, keeping the second part hidden.
 *      What ever selection the user chooses the second part is used
 *      as a boot file. Example:
 *
 *  "Network booting" \SOLARIS\US\NETBOOT.EXE
 *  "Hard Disk booting" \SOLARIS\US\UFSBOOT.EXE
 *
 * 3) If no Strap.rc file was found boot \Solaris\<LangOp>\boot.bin
 */

#include "disk.h"

/*
 *[]------------------------------------------------------------[]
 * | Structure defines that are only used in strap.c		|
 *[]------------------------------------------------------------[]
 */
typedef union {
	struct {
		char	*offp;	
		u_short	segp;
	} s;
	u_long	l;
} segment_u;

typedef struct _boot_file_ {
	struct _boot_file_ *forw;	/* next entry */
	char	*user_name;		/* name user will see displayed */
	char	*boot_name;		/* file to boot */
	char	*options;		/* options for each boot file */
} _boot_file_t, *_boot_file_p;

typedef struct option_table {
	char	*name;			/* name of entry */
					/* function to call on match if ... */
	char	*(*func)(char *, struct option_table *);
	int	*var;			/* .. this is not zero */
} _option_table_t, *_option_table_p;

/*
 *[]------------------------------------------------------------[]
 * | Forward references for routines used and defined in strap.c|
 *[]------------------------------------------------------------[]
 */
static void		load_strap(char *);
static void		title_strap(void);
static void		pickboot_strap(void);
static void		message_strap(char *);
static void		tgets_strap(char *, int , int , int , int);
static char *		ptype_strap(int);
static char *		setcomm_strap(char *, _option_table_p);
static char *		setload_strap(char *, _option_table_p);
static char *		setchar_strap(char *, _option_table_p);
static char *		setint_strap(char *, _option_table_p);
static _boot_file_p	parserc_strap(char *, int);

/*
 *[]------------------------------------------------------------[]
 * | Defines only used in strap.c				|
 *[]------------------------------------------------------------[]
 */
#define BOOTDIR         "\\Solaris"
#define LANGFILE        "locale"
#define RCFILE          "Strap.rc"
#define LOADSIZE	0x1000		/* ... # of bytes read during load */
/*
 * The maximum boot path is defined by at most two directories and
 * one file name. Under DOS that's 1 character for each the '\' and 
 * '.' (name extension seperator), 8 characters for the name and
 * 3 for the extension. 
 */
#define MAXBOOTPATH	((1 + 8 + 1 + 3) * 3)
#define MENU_START	8
#define PartTab1	"Current Disk Partition Information"
#define PartTab2	" Part #   Status    Type      Start      Length"
#define PartTab3	"================================================"

/*
 *[]------------------------------------------------------------[]
 * | Global variables.						|
 *[]------------------------------------------------------------[]
 */
char *		TitleStr = "\\Sc\\Sp0.0Solaris Boot\\Sp0.69Version 1.0";
int		Default_choice = 1;
int		Screen_timeout = 30;
short		BootDev = 0;
char		Spinner[] = "|/-\\";
int		Verbose = 0;
int		Defaultos = -1;
segment_u	LoadAddr;
#ifdef DEBUG
long		Debug = DBG_MALLOC;
#endif

_option_table_t OptionTable[] = {
	{ "bootpart=", setint_strap, &Defaultos },
	{ "default=", setint_strap, &Default_choice },
	{ "timeout=", setint_strap, &Screen_timeout },
	{ "verbose=", setint_strap, &Verbose },
	{ "comm=", setcomm_strap, (int *)0 },
	{ "load=", setload_strap, (int *)0 },
	{ "title=", setchar_strap, (int *)&TitleStr },
	{ (char *)0, 0, (int *)0}
};

/*
 *[]------------------------------------------------------------[]
 * | main_strap -- Here's where the whole shebang starts. 	|
 * | Before this point the assemble language had saved 'dx	|
 * | into BootDev and set up our stack. 			|
 *[]------------------------------------------------------------[]
 */
main_strap()
{
        _dir_entry_t d;
        char bootpath[MAXBOOTPATH];
        char langdir[NAMESIZ + 1];
        _file_desc_p fp;
        int rtn, i;
	struct _SREGS sregs;
	union _REGS regs;
	u_long *lp;
	char *bp;
	_boot_file_p boot_files = (_boot_file_p)0;
	char s[4];

	/* ---- Load Address of boot program, override in strap.rc ---- */
	LoadAddr.s.segp = 0x0000;
	LoadAddr.s.offp = (char *)0x8000;
	
        /* ---- Initialize the disk and filesystem ---- */
        if (init_fat(TYPE_SOLARIS_BOOT))
		/* 
		 * If we can't find a solaris boot partition then use the
		 * active dos which must be the partition we're loaded
		 * from.
		 */
		(void)init_fat(TYPE_DOS);

        if (cd_dos(BOOTDIR))
		message_strap("Failed to find boot directory\n");

        if ((fp = open_dos(LANGFILE, FILE_READ)) != (_file_desc_p)0) {
                rtn = read_dos(fp, &langdir[0], NAMESIZ);
                if (rtn > 0) {
                        for (i = 0; i < rtn; i++)
                                if (langdir[i] == '\r') {
                                        langdir[i] = '\0';
                                        break;
                                }
                        if (i == rtn)
                                langdir[rtn] = '\0';

                        if (cd_dos(&langdir[0]))
                                message_strap("Can't find locale directory\n");
                }
        }

        /* --- This open implies a close to LANGFILE ---- */
        if ((fp = open_dos(RCFILE, FILE_READ)) != (_file_desc_p)0) {
        	/*
        	 * Don't ever free this space allocated here. Not unless
        	 * the parsing routine also changes. Currently variables and
        	 * such assume that this space will not disappear. 
        	 * Rick McNeal -- 12-Jun-1995
        	 */
		bp = (char *)malloc_util((u_int)fp->f_len);
		
                if ((rtn = read_dos(fp, bp, (u_int)fp->f_len)) > 0)
                        boot_files = parserc_strap(bp, rtn);
                else {
                        printf_util("Failed to read boot.rc filen");
                        exit_util(0);
                }
        }
        else {
                bcopy_util("boot.bin", &bootpath[0], 9);
        }

	/* ---- show initial screen ---- */
	title_strap();

	/*
	 * Alow the user to choose which OS they wish to run. If we
	 * return from this call we can just continue because one of
	 * two things occured. 1) We're the only boot partition available
	 * or 2) the user selected to run Solaris instead of DOS (Good choice).
	 */
	pickboot_strap();
	
	if (boot_files != (_boot_file_p)0)
		selboot_strap(boot_files, &bootpath[0]);
		
        load_strap(&bootpath[0]);
}

/*
 *[]------------------------------------------------------------[]
 * | SearchOptions --  is given a null terminated string	|
 * | list of name=val pairs.					|
 *[]------------------------------------------------------------[]
 */
SearchOptions_strap(char *buf)
{
	_option_table_p op;

	do {
		for (op = &OptionTable[0]; op->name; op++)
		  if (strncmp_util(buf, op->name, strlen_util(op->name)) == 0) {
			  buf = (*op->func)(strchr_util(buf, '=') + 1, op);
			  break;
		  }

		/* ---- if we didn't find a match leave, error message? ---- */
		if (op->name == (char *)0)
		  break;

		/* ---- eat the white space ---- */
		while ((*buf == ' ') || (*buf == '\t'))
		  buf++;
	} while (*buf);
}

/*
 *[]------------------------------------------------------------[]
 * | parserc_strap -- parse the rc file.			|
 *[]------------------------------------------------------------[]
 */ 
static _boot_file_p
parserc_strap(char *buf, int size)
{
	_boot_file_p bp, pbp, Head;
	char *p;

	bp = (_boot_file_p)malloc_util(sizeof(_boot_file_t));
	pbp = (_boot_file_p)0;
	Head = (_boot_file_p)0;
	
        while (buf && *buf && bp) {
        	/*
        	 * Comment character found. Just dump the rest of the 
        	 * line.
        	 */
		if (*buf == '#') {
			if (buf = strchr_util(buf, '\r'))
				buf++;
			else
				break;
                }
                /*
                 * Options keyword found. Need to process everything on the
                 * rest of this current line as an option statement.
                 */
		else if ((*buf == 'O') && !strncmp_util(buf, "Options", 7)) {
			if (p = strchr_util(buf, '\r'))
				*p++ = '\0';
			if (buf = strchr_util(buf, ' ')) {
				SearchOptions_strap(buf + 1);
			}
			buf = p;
		/*
		 * At this point either the next character is a double quote
		 * which starts a valid line or we need to chuck the 
		 * character. That's why I advance buf pointer in the if
		 * statement.
		 */
		} else if (*buf++ == '"') {
			/* ---- Keep head if valid entry found ---- */
			if (Head == (_boot_file_p)0)
				Head = bp;
				
			/* ---- only link in when we know we've got more ---- */
			if (pbp != (_boot_file_p)0)
				pbp->forw = bp;
				
			/* ---- keep the pointer to the name ---- */
			bp->user_name = buf;
                        while (*buf && *buf != '"')
                                buf++;
				
			/* ---- null terminate user_name string ---- */
			if (*buf == '"')
				*buf++ = '\0';
			/* else
			 *	Syntax error folks. I'm not going to worry
			 *	about it for two reasons. 1) only intelligent
			 *	engineers :-) should be creating this file
			 *	and 2) the alogrithm will just exit the loop
			 *	creating null poniters for the boot_name.
			 */
				
			/* ---- search for start of boot_name ---- */
                        while (*buf && (*buf == ' ' || *buf == '\t'))
                                buf++;
				
                        bp->boot_name = buf;
			
			/* 
			 * look for either end of line or the start of the
			 * optional arguments
			 */
                        while (*buf && *buf != '\r' && *buf != ' ' && 
                            *buf != '\t')
                                buf++;
			
			/* ---- null terminate boot_name ---- */
                        if (*buf && *buf != '\r') {
				/* ---- boot_name is followed by options ---- */
                                *buf++ = '\0';
				
				while (*buf && (*buf == ' ' || *buf == '\t'))
					buf++;
				
				/*
				 * These boot options aren't processed until
				 * this boot file is choosen in selboot_strap()
				 */
				bp->options = buf;
				
				while (*buf && *buf != '\r')
					buf++;
				if (*buf)
					*buf++ = '\0';
			}
                        else if (*buf)
				/* ---- boot_name is followed by newline ---- */
				*buf++ = '\0';
			else
				/* ---- boot_name is followed by EOF ---- */
                                break;
				
			/* ---- next storage point ---- */
			pbp = bp;
			bp = (_boot_file_p)malloc_util(sizeof(_boot_file_t));
                }
        }
	return Head;
}

/*
 *[]------------------------------------------------------------[]
 * | selboot_strap -- If there's more than one boot file defined|
 * | in the rc file allow the user to choose. Else just copy the|
 * | boot file name into the boot name pointer.			|
 *[]------------------------------------------------------------[]
 */
selboot_strap(_boot_file_p Head, char *bnp)
{
	_boot_file_p bp;
        char *p, s[20];
	u_long sel;
	int pi;

	title_strap();
	
	if (Head->forw == (_boot_file_p)0) {
		bcopy_util(Head->boot_name, bnp,
			strlen_util(Head->boot_name) + 1);
		if (Head->options)
		  SearchOptions_strap(Head->options);
	}
	else {
		setprint_util(MENU_START, 27, "Optional Boot Program Table");
		
		/*
		 * I'm not going to worry about the case in which there's
		 * more boot files then lines on the screen. This program
		 * is supposed to be small and fast. If the user wants a 
		 * fancy GUI let them boot UNIX and start X.
		 * Rick McNeal -- 12-Jun-1995
		 */
		for (pi = 0, bp = Head; bp; pi++, bp = bp->forw) {
			setprint_util(MENU_START + 2 + pi, 27, " %12s %2d) %s", 
			    (pi + 1) == Default_choice ? "default --> " : "",
			    pi + 1, bp->user_name);
		}
		
	        while (1) {
			/* 
			 * print the selection line with space to clear
			 * any previous answers and then set the cursor
			 * input to be one space after the colon
			 */
	                setprint_util(MENU_START + 3 + pi, 27, 
	                	"Please enter selection:     ");
	                tgets_strap(s, 4, Screen_timeout, MENU_START + 3 + pi, 
	                	27 + 25);
			if (*s == '\0')
				sel = Default_choice;
			else
				sel = strtol_util(s, 0, 0);
	                if ((sel > 0) && (sel <= pi))
	                        break;
	        }
		for (bp = Head; sel; bp = bp->forw, sel--)
			if (sel == 1) {
				/* ---- add 1 to pick up the null  ---- */
			        bcopy_util(bp->boot_name, bnp, 
			        	strlen_util(bp->boot_name) + 1);
				if (bp->options)
					SearchOptions_strap(bp->options);
				break;
			}
		if (sel == 0)
			printf_util("Internal error!\n");
	}
}

/*
 *[]------------------------------------------------------------[]
 * | load_strap -- We've got a file name. Load it and jump.	|
 *[]------------------------------------------------------------[]
 */
static void
load_strap(char *bp)
{
        int rsize;			/* ---- read size returned */
        char *p;			/* ---- current offset into segment */
	segment_u s;			/* ---- used to adjust segment */
	u_short segment;		/* ---- segment where boot is loaded */
	_file_desc_p fp;		/* ---- file pointer for boot file */
	int rcount;			/* ---- return count */
	int i;				/* ---- general counter */
	u_long *lp;			/* ---- RedZone check */
	char *rbp;			/* ---- read buffer for load */

	/* ---- clear screen and display info about this load ---- */
	title_strap();
	
	/* ---- center the load message on the screen ---- */
	i = (80 - (strlen_util(bp) + 8)) / 2;
	setprint_util(MENU_START, i, "Loading %s", bp);

	if (Verbose)
	  printf_util("\\Sp23.0Load Address: %X", (u_long)LoadAddr.l);
	
	if ((fp = open_dos(bp, FILE_READ)) == (_file_desc_p)0) {
		setprint_util(MENU_START + 2, 0, "Can't open %s\n", bp);
		return;
	}
	
	/* ---- initialize the load address/segment for the boot program --- */
	segment = LoadAddr.s.segp;
        p = LoadAddr.s.offp;
	i = 0;
	if ((rbp = (char *)malloc_util(LOADSIZE)) == 0) {
		printf_util("Can't alloc memory to load in boot.bin\n");
		while(1);
	}
	while ((rsize = read_dos(fp, rbp, LOADSIZE)) > 0) {
	
		/* ---- Copy data using alternate segment ---- */
		bcopy_seg(rbp, p, rsize, segment);
		s.s.offp = p;
		s.s.segp = 0;
		s.l += rsize;
		if (s.s.segp)
			s.s.segp <<= 12;
		s.s.segp += segment;
		p = s.s.offp;
		segment = s.s.segp;
		
		setprint_util(MENU_START + 2, 40, "%c", Spinner[i++ % 4]);
	}
	free_util((u_int)rbp, LOADSIZE);

	title_strap();
	i = (80 - (strlen_util(bp) + 10)) / 2;
	setprint_util(MENU_START, i, "Executing %s", bp);
	
	/* ---- have output from program start here ---- */
	setcursor_util(MENU_START + 2, 0);

	/* ---- prep_and_go places sets up jump addr ---- */
	prep_and_go(LoadAddr.s.offp, LoadAddr.s.segp);
}

/*
 *[]------------------------------------------------------------[]
 * | pickboot_strap -- If there's more than one valid partition	|
 * | type alow the user to choose between the different OS's.	|
 * | If Defaultos is not -1 then boot from that partition if	|
 * | a timeout or return is entered.				|
 *[]------------------------------------------------------------[]
 */
static void
pickboot_strap(void)
{
	u_char lblk[SECSIZ];
	_fdisk_p	fp;
	struct {
		int	c_active;
		int	c_type;
		long	c_sec;
		long	c_len;
	} choice[FDISK_PARTS];
	int			ci, i;
	char			s[10];
	int			selectos;
	
	if (BootDev == 0)
		return;
	
	if (ReadSect_bios(&lblk[0], 0, 1))
		return;
	
	ci = 0;
	
	if ((lblk[SECSIZ - 2] == 0x55) && (lblk[SECSIZ - 1] == 0xaa)) {

		/*
		 * Normally defaultos will be -1 and therefore so will 
		 * selectos. What that means is that if more than one os
		 * is available the default action will be to boot Solaris.
		 * If strap.rc is modified to have "defaultpart=2", part. 2
		 * being the dos partion for example, selectos will be mapped
		 * to match the dos entry in 'choice'. If no selection is
		 * made dos gets booted by default.
		 *
		 * This was a request so that someone who knows what they're
		 * doing could boot Solaris and have another person who knows
		 * nothing about UNIX have dos run by default on the same
		 * machine.
		 * Rick McNeal -- 07-Jun-1995
		 */
		selectos = Defaultos;
		
		/*
		 * Create a table of just the valid entries. This means 
		 * any partition type which is non zero and not the old
		 * Solaris boot partition.
		 * Rick McNeal -- 07-Jun-1995
		 */
		for (fp = (_fdisk_p)&lblk[FDISK_START];
		    fp < (_fdisk_p)&lblk[SECSIZ - 2]; fp++) {
			if (fp->fd_type && fp->fd_type != TYPE_SOLARIS) {
				if (Defaultos != -1) {
					if (!--selectos) {
						selectos = ci;
						Defaultos = -1;
					}
				}
				choice[ci].c_active = fp->fd_active;
				choice[ci].c_type = fp->fd_type;
				choice[ci].c_sec = fp->fd_start_sec;
				choice[ci].c_len = fp->fd_part_len;
				ci++;
			}
		}

		/*
		 * if there's only one choice it must be us so just return
		 */
		if (ci <= 1)
			return;
		
		title_strap();
		setprint_util(MENU_START, 23, PartTab1);
		setprint_util(MENU_START + 2, 16, PartTab2);
		setprint_util(MENU_START + 3, 16, PartTab3);
		fp = (_fdisk_p)&lblk[FDISK_START];
		for (i = 0; i < ci; i++) {
			setprint_util(MENU_START + 4 + i, 16,
				"%4d%11s   %8.8s %8ld   %8ld\n", i + 1,
				choice[i].c_active == 0x80 ? "Active" : "",
				ptype_strap(choice[i].c_type),
				choice[i].c_sec, choice[i].c_len);
			if (choice[i].c_active == 0x80)
				Defaultos = i;
		}
		
		/* ---- see if an override occured ---- */
		if (selectos != -1) Defaultos = selectos;
			
		setprint_util(MENU_START + 8, 10, 
"Please select the partition you wish to boot (default is %d):", 
Defaultos + 1);
		do {
			/*
			 * I should do something about this '17 + 47' business.
			 * 17 is the column number where I have the
			 * "Please select ..." message starting. 47 is the
			 * length of the string. I decided not to add the
			 * number together so that it would be obvious as to
			 * what I was doing.
			 * Rick McNeal -- 07-Jun-1995
			 */
			setprint_util(MENU_START + 8, 10 + 60, "   ");
			tgets_strap(s, 3, Screen_timeout, MENU_START + 8, 
			    10 + 61);
			
			/* 
			 * '*s ' will be null if the user typed just a return
			 * or the timeout occurred. Either way we use the 
			 * default boot partition which is normally ourselfs
			 * unless defaultos has been set in the strap.rc file.
			 */
			if (*s == '\0') {
				if (selectos != -1) 
					i = selectos;
				else
					return;
			}
			else
				i = strtol_util(s, (char **)0, 0) - 1;
		} while ((i < 0) || (i >= ci));
		
		if (choice[i].c_active != FDISK_ACTIVE) {
			/* 
			 * update in core copy of masterboot. First clear
			 * all partition of any active bits. Then mark
			 * the partition which was picked by the user and
			 * mark it active.
			 */
			for (fp = (_fdisk_p)&lblk[FDISK_START];
			    fp < (_fdisk_p)&lblk[SECSIZ - 2]; fp++) {
			    	if (choice[i].c_sec == fp->fd_start_sec)
					fp->fd_active = FDISK_ACTIVE;
				else
					fp->fd_active = FDISK_INACTIVE;
			}
			
			/* ---- copy masterboot to correct location ---- */
			bcopy_seg(&lblk[0], (char *)0x7c00, SECSIZ, 0);

			/* ---- clear the screen of our menu ---- */
			title_strap();
			setcursor_util(1, 0);
			
			/* ---- now jump to that in core copy ---- */
			jump_master();
		}
	}
}

/*
 *[]------------------------------------------------------------[]
 * | ptype_strap -- convert a partition type into a user	|
 * | readable string.						|
 *[]------------------------------------------------------------[]
 */
char *
ptype_strap(int type)
{
	switch(type) {
		case TYPE_EMPTY:
			return "Empty";
		case TYPE_DOS_12:
		case TYPE_DOS_16:
			return "DOS";
		case TYPE_HUGH:
			return "BIGDOS";
		case TYPE_COMPAQ:
			return "Compaq";
		case TYPE_SOLARIS_BOOT:
			return "Solaris";
		/* ---- This should happen ---- */
		default:
			return "Unknown";
	}
}

/*
 *[]------------------------------------------------------------[]
 * | title_strap -- print a title message which also clears the	|
 * | screen.							|
 *[]------------------------------------------------------------[]
 */
static void
title_strap(void)
{
	printf_util(TitleStr);
}

/*
 *[]------------------------------------------------------------[]
 * | tgets_strap -- get a string from the user which times out	|
 * | after a given period of time.				|
 *[]------------------------------------------------------------[]
 */ 
static void
tgets_strap(char *s, int cc, int timeout, int rows, int cols)
{
	u_long nextsec = get_time() + TICKS_PER_SEC;

	setcursor_util(rows, cols);
	if (!timeout)
		gets_util(s, cc);
	else {
		for (; !nowaitc_util() && timeout; ) {
		    	if (get_time() >= nextsec) {
				setprint_util(23, 0, "%2d", timeout--);
				setcursor_util(rows, cols);
				nextsec = get_time() + TICKS_PER_SEC;
			}
		}
	
		if (timeout == 0)
			*s = '\0';
		else
			gets_util(s, cc);
	}
}

/*
 *[]------------------------------------------------------------[]
 * | setcomm_strap -- initialize a comm port so that input &	|
 * | output is directed there.					|
 *[]------------------------------------------------------------[]
 */ 
static char *
setcomm_strap(char *s, _option_table_p op)
{
	serial_init(strtol_util(s, &s, 0));
	return s;
}

/*
 *[]------------------------------------------------------------[]
 * | This routine is called because someone has set 'load=...'	|
 * | in the strap.rc file. The syntax is: xxxx.xxxx		|
 * | The first four digits are the segment number and the second|
 * | four are the offset. The number are always interpreted as	|
 * | hex and only the significant numbers need to be present.	|
 * | i.e. 0.8000 or 800.0 are both valid and represent the same	|
 * | physical address. The dot '.' can be any non valid hex	|
 * | character.							|
 * | Rick McNeal -- 12-Jun-1995					|
 *[]------------------------------------------------------------[]
 */
static char *
setload_strap(char *s, _option_table_p op)
{
	LoadAddr.s.segp = strtol_util(s, &s, 16);
	LoadAddr.s.offp = (char *)strtol_util(s + 1, &s, 16);
	return s;
}

/*
 *[]------------------------------------------------------------[]
 * | setchar_strap -- stores a pointer to the string found in	|
 * | the rc file in a global variable. The length of the string	|
 * | is either the end of line or terminated by a double quote.	|
 *[]------------------------------------------------------------[]
 */
static char *
setchar_strap(char *s, _option_table_p op)
{
	char *p;

	/*
	 * If the string value starts with a quote character use everything
	 * up to the next quote returning the buffer pointer after our last
	 * quote character. Else assume they want everything.
	 */
	if ((*s == '"') && (p = strchr_util(++s, '"')))
	  *p++ = '\0';
	else
	  p = (char *)0;
	*((char **)op->var) = s;
	
	return p;
}

/*
 *[]------------------------------------------------------------[]
 * | setint_strap -- converts character string found in rc file	|
 * | to integer and stores that in the global variable for later|
 * | use by strap.						|
 *[]------------------------------------------------------------[]
 */
static char *
setint_strap(char *s, _option_table_p op)
{
	*op->var = strtol_util(s, &s, 0);
	return s;
}

/*
 *[]------------------------------------------------------------[]
 * | Display a message on the screen and pause for 5 seconds.	|
 * | Continue immediately if the user presses a carriage return.|
 * | Routine is currently used when an error message would be	|
 * | cleared immediately and the user wouldn't have a chance to	|
 * | take note of the problem.					|
 * | Rick McNeal -- 12-Jun-1995					|
 *[]------------------------------------------------------------[]
 */
static void
message_strap(char *m)
{
	char s[4];
	printf_util(m);
	tgets_strap(s, 4, 5, 0, 0);
}



