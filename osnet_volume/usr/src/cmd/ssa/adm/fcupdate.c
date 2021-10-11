/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)fcupdate.c 1.6     97/06/06 SMI"

#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <siginfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/exec.h>
#include <sys/exechdr.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socreg.h>
#include <sys/time.h>


#define	FEPROM_SIZE		256*1024
#define	FEPROM_MAX_PROGRAM	25
#define	FEPROM_MAX_ERASE	1000

#define	FEPROM_READ_MEMORY	0x00
#define	FEPROM_ERASE		0x20
#define	FEPROM_ERASE_VERIFY	0xa0
#define	FEPROM_PROGRAM		0x40
#define	FEPROM_PROGRAM_VERIFY	0xc0
#define	FEPROM_RESET		0xff

#define	FOUND			0
#define	NOT_FOUND		1

#define	PROM_SIZ		0x20010
/*
 * Workaround for a bug in sbusmem driver
 */
#define	PROM_SIZ_ROUNDED	0x22000
#define	SAMPLE_SIZ		0x100

#define	REG_OFFSET		0x20000

#define	VERSION_STRING		"1."
#define	VERSION_LEN		32
#define	ONBOARD_SOC		"SUNW,soc@d"

static u_char	buffer[FEPROM_SIZE];
static char	soc_name[] = "SUNW,soc";
static char	fcodename[] = 	"soc.img.1.33";
static char	sbus_list[128][PATH_MAX];
static char	sbussoc_list[128][PATH_MAX];
static char	bootpath[PATH_MAX];
static char	version[VERSION_LEN];

static u_int	findstring(u_char *, u_char *, u_char *, u_int);
static u_int	getsbuslist(void);
static void	load_file(char *, caddr_t, volatile soc_reg_t *);
static void	buserr(int);
static void	usec_delay(int);
static void	getbootdev(unsigned int);
#ifdef	DEBUG
static void	printbuf(unsigned char *, int);
#endif
static int	read_prom(u_char *, int);
static int	warn(void);
static int	write_feprom(u_char *, u_char *, volatile soc_reg_t *);
static int	feprom_erase(volatile u_char *, volatile soc_reg_t *);
#ifdef	notdef
static char	get_runlevel(void);
#endif

static u_int	gotbuserr;
static jmp_buf	jmpbuf;

static struct exec exec;

static char *warnstring =
"WARNING!! This program will update the Fcode in this FC/S Sbus Card.\n";
static char *warnstring1 =
"This may take a few (5) minutes. Please be patient.\n";
static char *warnstring2 = "Do you wish to continue ? (y/n) ";


void
fc_update(unsigned int verbose, unsigned int force, char *file)
{
	int fd;
	caddr_t addr;
	u_int i;
	u_int fflag = 0;
	u_int vflag = 0;
	u_int numslots;
	volatile soc_reg_t *regs;
	char *slotname;
	char *fcode = fcodename;

	if (!file)
		vflag++;
	else {
#ifdef		notdef
		char runlevel;
		runlevel = get_runlevel();
		if (runlevel == 0) {
			(void) fprintf(stderr, "Cannot determine run level\n");
			return;
		} else if ((runlevel != 's') && (runlevel != 'S')) {
			(void) fprintf(stderr, "The system is currently at "
				"run level %c\n"
				"Please execute this command when system is "
				"in single-user mode\n", runlevel);
			return;
		}
#endif
		fflag++;
		fcode = file;
	}

	/*
	 * Get count of, and names of SBus slots using the SBus memory
	 * interface.
	 */
	(void) getbootdev(verbose);
	if (verbose)
		(void) fprintf(stdout, "Bootpath = %s\n", bootpath);

	numslots = getsbuslist();
	if (verbose)
		(void) fprintf(stdout, "Found %d FC/S Cards\n", numslots);

	for (i = 0; i < numslots; i++) {

		/*
		 * Open SBus memory for this slot.
		 */
		slotname = &sbus_list[i][0];
		if (fflag && (strcmp(slotname, bootpath) == 0)) {
			(void) fprintf(stderr, " Ignoring %s (bootpath)\n",
					slotname);
			continue;
		}

		if (verbose)
			(void) fprintf(stdout, "Opening %s\n", slotname);

		fd = open(slotname, O_RDWR);

		if (fd < 0) {
			perror("open of slotname");
			continue;
		}

		/*
		 * Mmap that SBus memory into my memory space.
		 */
		addr = mmap((caddr_t)0, PROM_SIZ_ROUNDED, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);

		if (addr == MAP_FAILED) {
			perror("mmap1");
			(void) close(fd);
			continue;
		}

		if ((int)addr == -1) {
			perror("mmap");
			(void) close(fd);
			continue;
		}

		regs = (soc_reg_t *)((int)addr + REG_OFFSET);

		/*
		 * Read the first few hundred bytes of the
		 * SBus card's memory. We do not use mmap() but use read()
		 * instead, because it ensures that device exists and will
		 * not be fatal if it does not.
		 */
		(void) memset((char *)&buffer[0], 0, FEPROM_SIZE);
		if (read_prom(buffer, fd) == NOT_FOUND) {
			if (munmap(addr, PROM_SIZ) == -1) {
				perror("munmap");
			}
			(void) fprintf(stderr, "read_prom failed for %s\n",
				slotname);
			(void) close(fd);
			continue;
		}

		/*
		 * Search for the string SUNW,soc in the FCODE.  Note it's
		 * not NULL terminated, so you can't use string handling
		 * library calls.
		 */
		if (force || findstring((u_char *)NULL, (u_char *) soc_name,
				(u_char *) buffer,
					SAMPLE_SIZ) == FOUND) {
			(void) fprintf(stdout, "Found an FC/S card in slot: "
					"%s\n", slotname);

			/*
			 * At this point we have found an FC/S card.
			 */
			/*
			 * Load the New FCode
			 */
			if (fflag) {
				if (!warn())
					load_file(fcode, addr, regs);
			} else if (vflag) {
#ifdef	DEBUG
				printbuf(buffer, 256);
#endif
				if (findstring((u_char *)&version[0],
				    (u_char *)VERSION_STRING, buffer,
				    SAMPLE_SIZ) == FOUND) {
					(void) fprintf(stdout, "Detected FC/S "
						"Version: %s for %s\n",
						version, slotname);
				}
			}
		}

		if (munmap(addr, PROM_SIZ) == -1) {
			perror("munmap");
		}

		(void) close(fd);

	}
	if (verbose)
		(void) fprintf(stdout, "Complete\n");
}

/*
 * program an FEprom with data from 'source_address'.
 *	program the FEprom with zeroes,
 *	erase it,
 *	program it with the real data.
 */
static int
feprom_program(u_char *source_address, u_char *dest_address,
	volatile soc_reg_t *regs)
{
	int i;

	(void) fprintf(stdout, "Filling with zeroes...\n");
	if (!write_feprom((u_char *)0, dest_address, regs)) {
		(void) fprintf(stderr, "FEprom at 0x%x: zero fill failed\n",
					(int)dest_address);
		return (0);
	}

	(void) fprintf(stdout, "Erasing...\n");
	for (i = 0; i < FEPROM_MAX_ERASE; i++) {
		if (feprom_erase(dest_address, regs))
			break;
	}

	if (i >= FEPROM_MAX_ERASE) {
		(void) fprintf(stderr, "FEprom at 0x%x: failed to erase\n",
			(int)dest_address);
		return (0);
	} else if (i > 0) {
		(void) fprintf(stderr, "FEprom erased after %d attempt%s\n",
			i, (i == 1 ? "" : "s"));
	}

	(void) fprintf(stdout, "Programming...\n");
	if (!(write_feprom(source_address, dest_address, regs))) {
		(void) fprintf(stderr, "FEprom at 0x%x: write failed\n",
				(int)dest_address);
		return (0);
	}

	/* select the zeroth bank at end so we can read it */
	regs->soc_cr.w &= ~(0x30000);
	(void) fprintf(stdout, "Programming done\n");
	return (1);
}

/*
 * program an FEprom one byte at a time using hot electron injection.
 */
static int
write_feprom(u_char *source_address, u_char *dest_address,
	volatile soc_reg_t *regs)
{
	int pulse, i;
	u_char *s = source_address;
	volatile u_char *d;

	for (i = 0; i < FEPROM_SIZE; i++, s++) {

		if ((i & 0xffff) == 0) {
			(void) fprintf(stdout, "selecting bank %d\n", i>>16);
			regs->soc_cr.w &= ~(0x30000);
			regs->soc_cr.w |= i & 0x30000;
		}

		d = dest_address + (i & 0xffff);

		for (pulse = 0; pulse < FEPROM_MAX_PROGRAM; pulse++) {
			*d = FEPROM_PROGRAM;
			*d = source_address ? *s : 0;
			usec_delay(50);
			*d = FEPROM_PROGRAM_VERIFY;
			usec_delay(30);
			if (*d == (source_address ? *s : 0))
					break;
		}

		if (pulse >= FEPROM_MAX_PROGRAM) {
			*dest_address = FEPROM_RESET;
			return (0);
		}
	}

	*dest_address = FEPROM_RESET;
	return (1);
}

/*
 * erase an FEprom using Fowler-Nordheim tunneling.
 */
static int
feprom_erase(volatile u_char *dest_address, volatile soc_reg_t *regs)
{
	int i;
	volatile u_char *d = dest_address;

	*d = FEPROM_ERASE;
	usec_delay(50);
	*d = FEPROM_ERASE;

	usec_delay(10000); /* wait 10ms while FEprom erases */

	for (i = 0; i < FEPROM_SIZE; i++) {

		if ((i & 0xffff) == 0) {
			regs->soc_cr.w &= ~(0x30000);
			regs->soc_cr.w |= i & 0x30000;
		}

		d = dest_address + (i & 0xffff);

		*d = FEPROM_ERASE_VERIFY;
		usec_delay(50);
		if (*d != 0xff) {
			*dest_address = FEPROM_RESET;
			return (0);
		}
	}
	*dest_address = FEPROM_RESET;
	return (1);
}

static void
usec_delay(int s)
{
	hrtime_t now, then;

	now = gethrtime();
	then = now + s*1000;
	do {
		now = gethrtime();
	} while (now < then);
}

static u_int
findstring(u_char *ret_string, u_char *string, u_char *target, u_int len)
{
	u_char *strp, *targetp;
	u_char *strp2, *targetp2;
	u_int i, strln;
	u_int notfound;

	strln = strlen((char *)string);
	strp = string;
	targetp = target;

	for (i = 0; i < (len - strln); ) {

		notfound = 0;

		/*
		 * Found our first character match
		 */
		if (*strp == *targetp) {

			/*
			 * start to look at next character
			 */
			strp2 = strp + 1;
			targetp2 = targetp + 1;

			/*
			 * increment counter of target buffer.
			 */
			i++;

			/*
			 * Check rest of characters in string until
			 * end of string.
			 */
			while (*strp2 != '\0') {

				/*
				 * We are at end of buffer so we only
				 * matched the first part of the string.
				 */
				if (i > (len - strln)) {
					return (NOT_FOUND);
				}

				/*
				 * Mismatch
				 */
				if (*strp2 != *targetp2) {
					notfound = 1;
					break;
				} else {
					/*
					 * Match, so continue testing string
					 */
					strp2++;
					targetp2++;
					i++;
				}
			}
			/*
			 * we found a match by the end of the string
			 */
			if (notfound == 0) {
				if (ret_string) {
					while ((*targetp2 >= '0') &&
					    (*targetp2++ <= '9')) {
						;
					}
					strln = targetp2 - targetp;
					(void) memcpy(ret_string, targetp,
								strln);
					ret_string[strln] = '\0';
				}
				return (FOUND);
			}
		}

		/*
		 * Increment count into target buffer.
		 */
		targetp++;
		i++;
	}

	/*
	 * got to end of buffer.
	 */
	return (NOT_FOUND);
}

static u_int
getsbuslist(void)
{
	int fildes[2], len;
	char *sp;
	pid_t pid;

	if (pipe(fildes) == -1) {
		perror("pipe");
		return (0);
	}

	if ((pid = fork()) == -1) {
		perror("fork");
		return (0);
	} else if (pid == 0) {
		/*
		 * child
		 * Close stdin, stdout, and stderr.  Dup stdin and
		 * out to push the output throught the pipe.
		 * Then execute the find command.
		 */
		(void) close(0);
		(void) close(1);
		(void) close(2);
		(void) dup(fildes[0]);
		(void) dup(fildes[1]);
		(void) execl("/bin/find", "find", "/devices",
			"-name", "SUNW,soc*", "-print", 0);
		perror("exec");
		return (0);
	} else {
		FILE *ifile;
		u_int k = 0;
		int fgret;

		/*
		 * parent
		 */

		(void) sleep(2);
		/*
		 * close pipe used for writing
		 */
		(void) close(fildes[1]);

		/*
		 * get FILE structure for pipe used for reading
		 */
		if ((ifile = fdopen(fildes[0], "r")) == NULL) {
			perror("fdopen");
			exit(-1);
		}

		for (;;) {
			fgret = fscanf(ifile, "%s*s", &sbussoc_list[k][0]);
			if (fgret == (int)EOF)
				break;
			else if (fgret == NULL)
				break;
			if (strstr(&sbussoc_list[k][0], "SUNW,soc@d"))
				continue;
			if (strstr(&sbussoc_list[k][0], "SUNW,socal"))
				continue;
			sp = strstr(&sbussoc_list[k][0], "SUNW,soc");
			len = strlen(&sbussoc_list[k][0]) - strlen(sp);
			sbussoc_list[k][len] = '\0';
			sp += strlen("SUNW,soc@");
			(void) sprintf(&sbus_list[k][0],
					"%ssbusmem@%c,0:slot%c",
					&sbussoc_list[k][0], sp[0], sp[0]);
			k++;
		}
		return (k);
	}

	/*NOTREACHED*/
}

#ifdef	DEBUG
void
show_buf(char *buf, u_int len)
{
	u_int i, j;
	char c;

	for (j = 0; j < (len / 16); j++) {
		(void) fprintf(stdout, "%06x:  ", j * 16);
		for (i = 0; i < 16; i++) {
		    (void) fprintf(stdout, "%02x ", (buf[(j * 16) + i]) & 0xff);
		}
		(void) fprintf(stdout, "  ");
		for (i = 0; i < 16; i++) {
			c = buf[(j * 16) + i];
			(void) fprintf(stdout, "%c",
				(isprint(c))? (c): ((c == 0)? '.': '-'));
		}
		(void) fprintf(stdout, "\n");
	}
}
#endif

#ifdef	notdef
static char
get_runlevel(void)
{
	char *who = "who -r | awk '{print $3}'";
	char buf[BUFSIZ];
	FILE *ptr;

	if ((ptr = popen(who, "r")) != NULL) {
		(void) fgets(buf, BUFSIZ, ptr);
		(void) pclose(ptr);
		return (buf[0]);
	}

	return ((char)0);
}
#endif

static void
getbootdev(unsigned int verbose)
{
	char *df = "df /";
	FILE *ptr;
	char *p;
	char bootdev[PATH_MAX];
	char buf[BUFSIZ];
	int foundroot = 0;


	if ((ptr = popen(df, "r")) != NULL) {
		while (fgets(buf, BUFSIZ, ptr) != NULL) {
			if (p = strstr(buf, "/dev/dsk/")) {
				(void) memset((char *)&bootdev[0], 0,
					PATH_MAX);
				while (*p != '\0') {
					if (!isalpha(*p) && (*p != '/'))
						*p = ' ';
					p++;
				}
				(void) sscanf(p, "%s", bootdev);
				foundroot = 1;
			}
		}
		if (!foundroot) {
			if (verbose)
				(void) fprintf(stderr, "ssaadm: root is not on "
						"local disk!\n");
			(void) memset((char *)&bootpath[0], 0, PATH_MAX);
			return;
		}
		(void) pclose(ptr);
		if (bootdev[0]) {
			char *ls;
			char *p1;
			char *p2;
			char *sbusmem = "/sbusmem@";
			char *slot = ",0:slot";

			ls = (char *)malloc(PATH_MAX);
			(void) memset((char *)ls, NULL, PATH_MAX);
			(void) strcpy(ls, "ls -l ");
			(void) strcat(ls, bootdev);
			if ((ptr = popen(ls, "r")) != NULL) {
				while (fgets(buf, BUFSIZ, ptr) != NULL) {
					if (p = strstr(buf, "/devices")) {
					    if (p1 = strstr(buf, "sbus")) {
						while (*p1 != '/')
							p1++;
						p2 = strstr(p1, "@");
						++p2;
						*p1 = '\0';
					    } else {
						if (p1 = strstr(buf,
						    "SUNW,soc")) {
							p2 = strstr(p1, "@");
							++p2;
							--p1;
							*p1 = '\0';
						}
					    }
					}
				}
				(void) pclose(ptr);
			}
			(void) memset((char *)&bootdev[0], 0, PATH_MAX);
			(void) sscanf(p, "%s", bootdev);
			(void) memset((char *)&bootpath[0], 0, PATH_MAX);
			(void) strcat(bootpath, bootdev);
			(void) strcat(bootpath, sbusmem);
			if (p2) {
				(void) strncat(bootpath, p2, 1);
				(void) strcat(bootpath, slot);
				(void) strncat(bootpath, p2, 1);
			}
		}
	}
}

static int
read_prom(u_char *buf, int fd)
{
	volatile u_int i;

	if (sigsetjmp(jmpbuf, 0) != 0) {
		return (NOT_FOUND);
	}

	(void) sigset(SIGBUS, buserr);
	(void) sigset(SIGSEGV, buserr);
	gotbuserr = 0;
	for (i = 0; i < SAMPLE_SIZ; i++) {

		/*
		 * Check for delayed bus errors from previous reads
		 */
		if (gotbuserr)
			return (NOT_FOUND);

		if (read(fd, &buf[i], sizeof (char)) < 0) {
			(void) fprintf(stderr, "could not read prom\n");
			return (NOT_FOUND);
		}
		usec_delay(10000);

		/*
		 * Check for Bus Errors accessing the SBus card.
		 * gotbuserr is set from signal catcher
		 */
		if (gotbuserr)
			return (NOT_FOUND);
	}
	return (FOUND);
}

static void
load_file(char *file, caddr_t prom, volatile soc_reg_t *regs)
{
	int ffd = open(file, 0);

	if (ffd < 0) {
		perror("open of file");
		exit(1);
	}
	(void) fprintf(stdout, "Loading FCode: %s\n", file);

	if (read(ffd, &exec, sizeof (exec)) != sizeof (exec)) {
		perror("read exec");
		exit(1);
	}

	if (exec.a_trsize || exec.a_drsize) {
		(void) fprintf(stderr, "%s: is relocatable\n", file);
		exit(1);
	}

	if (exec.a_data || exec.a_bss) {
		(void) fprintf(stderr, "%s: has data or bss\n", file);
		exit(1);
	}

	if (exec.a_machtype != M_SPARC) {
		(void) fprintf(stderr, "%s: not for SPARC\n", file);
		exit(1);
	}

	(void) fprintf(stdout, "Loading 0x%x bytes from %s at offset 0x%x\n",
		(int)exec.a_text, file, 0);

	if (read(ffd, &buffer, exec.a_text) != exec.a_text) {
		perror("read");
		exit(1);
	}

	(void) close(ffd);

	(void) feprom_program((u_char *)buffer, (u_char *)prom, regs);
}

static void
buserr(int sig)
{
	static int xx = 0;
	xx++;

	gotbuserr = sig;
	(void) sigset(SIGBUS, buserr);
	(void) sigset(SIGSEGV, buserr);
	(void) siglongjmp(jmpbuf, sig);
}

static int
warn(void)
{
	char input[1024];

	input[0] = '\0';

	(void) fprintf(stderr, "%s", warnstring);
	(void) fprintf(stderr, "%s", warnstring1);
loop1:
	(void) fprintf(stderr, "%s", warnstring2);

	(void) gets(input);

	if ((strcmp(input, "y") == 0) || (strcmp(input, "yes") == 0)) {
		return (FOUND);
	} else if ((strcmp(input, "n") == 0) || (strcmp(input, "no") == 0)) {
		(void) fprintf(stderr, "Not Downloading FCode\n");
		return (NOT_FOUND);
	} else {
		(void) fprintf(stderr, "Invalid input\n");
		goto loop1;
	}
}
#ifdef	DEBUG
void
printbuf(unsigned char *buf, int len)
{
	int j = 0;

	printf("buffer=\n");
	while (j < len) {
		printchar(buf[j++]);
		if (j < len) printchar(buf[j++]);
		if (j < len) printchar(buf[j++]);
		if (j < len) printchar(buf[j++]);
		if (j < len) printchar(buf[j++]);
		if (j < len) printchar(buf[j++]);
		if (j < len) printchar(buf[j++]);
		if (j < len) printchar(buf[j++]);
		printf("\n");
	}
}
printchar(unsigned char c)
{
	if (isalpha((int)c))
		printf(" %c ", c);
	else
		printf(" %2x ", c);
}
#endif
