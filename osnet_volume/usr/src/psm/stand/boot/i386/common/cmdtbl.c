/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ident "@(#)cmdtbl.c	1.23	97/11/25 SMI"

/* boot shell command table */

#include <sys/bsh.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <sys/bootconf.h>
#include <sys/bootcmn.h>

#include <sys/salib.h>
#include <sys/promif.h>
#include <sys/fcntl.h>

extern void console_cmd();
extern void echo_cmd();
extern void else_cmd();
extern void elseif_cmd();
extern void endif_cmd();
extern void getprop_cmd();
extern void getproplen_cmd();
extern void help_cmd();
extern void if_cmd();
extern void map_cmd();
extern void maps_cmd();
extern void printenv_cmd();
extern void read_cmd();
extern void readt_cmd();
extern void resetdtree_cmd();
extern void set_cmd();
extern void setcolor_cmd();
extern void setenv_cmd();
extern void setprop_cmd();
extern void setbin_cmd();
extern void singlestep_cmd();
extern void source_cmd();
extern void unset_cmd();
extern void verbose_cmd();
extern void loadnrun();

extern void dev_cmd();		/* Device tree commands ...		*/
extern void ls_cmd();
extern void pwd_cmd();
extern void mknod_cmd();
extern void props_cmd();
extern void show_cmd();
extern void mount_cmd();

extern void claim_cmd();
extern void release_cmd();

extern int ischar();
extern int getchar();
extern void putchar();
extern ssize_t read();
extern long lseek();
extern caddr_t rm_malloc(size_t size, u_int align, caddr_t virt);
extern void rm_free(caddr_t, size_t);

#define	DENT_CHUNK	256
#define	NUM_CON_LINES	25

static void
more_cmd(int argc, char **argv)
{
	/*
	 * Debug command that will attempt to open and display
	 * the contents of a file.
	 */
	extern struct bootops *bop;
	char buf[DENT_CHUNK];
	char *lp, *sp;
	char sv;
	int lc, fd, c;

	if (argc != 2) {
		/* --- Must specify file name --- */
		printf("Usage: %s filename\n", argv[0]);
		return;
	}

	if ((fd = BOP_OPEN(bop, argv[1], O_RDONLY)) < 0) {
		printf("can't open %s\n", argv[1]);
		return;
	}

	lc = 0;
	while ((c = BOP_READ(bop, fd, buf, DENT_CHUNK - 1)) > 0) {
		sp = buf;
		buf[c] = '\0';
		for (lp = buf; lp < (buf + c); lp++) {
			if (lc == (NUM_CON_LINES - 1)) {
				sv = *lp;
				*lp = '\0';
				printf("%s", buf);
				(void) goany();
				lc = 0;
				*lp = sv;
				sp = lp;
			}
			if (*lp == '\n') {
				lc++;
			}
		}
		printf("%s", sp);
	}

	printf("\n");
	BOP_CLOSE(bop, fd);
}

static void
dir_cmd(int argc, char **argv)
{
	/*
	 *  DOS-like "dir" command for debugging "BOP_GETDENTS"
	 *
	 *  Since there's no way to set a current working directory ("cd" works
	 *  on the device tree, not the file system), the full pathname of the
	 *  directory to be listed must be specified!
	 */
	int fd = -1;
	struct stat ss;
	extern struct bootops *bop;
	int n;
	char buf[DENT_CHUNK];
	struct dirent *dep;

	if (argc != 2) {
		/* --- Must specify directory name --- */
		printf("usage: %s pathname\n", argv[0]);
		return;
	}

	if ((fd = BOP_OPEN(bop, argv[1], O_RDONLY)) < 0) {
		printf("can't open %s\n", argv[1]);
		return;
	}

	if (BOP_GETATTR(bop, fd, &ss) != 0) {
		printf("can't stat %s\n", argv[1]);
		return;
	}



	switch (ss.st_mode & S_IFMT) {

	case S_IFDIR:
	case S_IFLNK:	/* --- Only works on directories --- */

		dep = (struct dirent *)buf;
		while ((n = BOP_GETDENTS(bop, fd, dep, sizeof (buf))) > 0) {

			while (n-- > 0) {
				char *tp;

				tp = (char *)dep;
				if (tp < &buf[0] || tp >= &buf[DENT_CHUNK]) {
					goto bad_dir;
				}
				printf("%s ", dep->d_name);
				tp = (char *)dep + dep->d_reclen;
				dep = (struct dirent *)tp;
			}
			dep = (struct dirent *)buf;
		}
		printf("\n");
		break;

	default:
		printf("%s is not a directory\n", argv[1]);
		break;
	}
bad_dir:
	BOP_CLOSE(bop, fd);
}

/*ARGSUSED*/
static
void
voltell_cmd(int argc, char **argv)
{
	/*
	 * splat out the current volume name
	 */
	extern void boot_compfs_getvolname(char *);
	static char vbuf[VOLLABELSIZE+1];

	boot_compfs_getvolname(vbuf);
	printf(":%s:\n", vbuf);
}

int xxx_gulp;

static void
gulp_cmd(int argc, char **argv)
{
	/*
	 *  gulp data repeatedly from a given file to test
	 *  fs, driver, etc.
	 */

	int fd;
	struct stat ss;
	extern struct bootops *bop;
	int n;
	char *buf;

#define	GULPSIZ	(100*1024)
/* extern int nfs_readsize; */
unsigned btime, rtime;
int count;

	if (argc != 2) {
		/* --- Must specify directory name --- */
		printf("usage: %s pathname\n", argv[0]);
		return;
	}

	if ((fd = BOP_OPEN(bop, argv[1], O_RDONLY)) < 0) {
		printf("can't open %s\n", argv[1]);
		return;
	}

	if (BOP_GETATTR(bop, fd, &ss) != 0) {
		printf("can't stat %s\n", argv[1]);
		goto out;
	}
	if ((buf = rm_malloc(GULPSIZ, 0, 0)) == 0) {
		printf("can't allocate read buffer\n");
		goto out;
	}



	switch (ss.st_mode & S_IFMT) {

	case S_IFREG: 	/* --- Only works on regular files --- */

		/* nfs_readsize = 8192; */
		do {
			/* printf("nfs_readsize = %d ",nfs_readsize); */
			btime = prom_gettime();
			count = 0;
			while ((n = read(fd, buf, 100*1024)) > 0) {
				count += n;
				putchar('.');
				if (ischar()) {
					switch (getchar()) {
					case 'd':
						xxx_gulp = !xxx_gulp;
						break;
					default:
						n = -1;
						break;
					}
					if (n == -1)
						break;
				}
			}
			rtime = prom_gettime() - btime;
			printf(" %d bps", (count*1000)/rtime);
			printf("\n");
			/*
			switch (nfs_readsize) {
			case 1000: nfs_readsize = 2000; break;
			case 2000: nfs_readsize = 4000; break;
			case 4000: nfs_readsize = 8000; break;
			case 8000: nfs_readsize = 1000; break;
			}
			*/
			(void) lseek(fd, 0, 0);
		} while (n == 0);
		break;

	default:
		printf("%s is not a regular file\n", argv[1]);
		break;
	}
	rm_free(buf, GULPSIZ);
out:
	BOP_CLOSE(bop, fd);
}

/* NOTE: command names MUST be in lexical sort order */

struct cmd cmd[] = {
	{ ".properties", props_cmd },
	{ "cd", dev_cmd },
	{ "claim", claim_cmd },
	{ "console", console_cmd },
	{ "dev", dev_cmd },
	{ "dir", dir_cmd },
	{ "echo", echo_cmd },
	{ "else", else_cmd },
	{ "elseif", elseif_cmd },
	{ "endif", endif_cmd },
	{ "getprop", getprop_cmd },
	{ "getproplen", getproplen_cmd },
	{ "gulp", gulp_cmd },
	{ "help", help_cmd },
	{ "if", if_cmd },
	{ "ls", ls_cmd },
	{ "map", map_cmd },
	{ "maps", maps_cmd },
	{ "mknod", mknod_cmd },
	{ "more", more_cmd },
	{ "mount", mount_cmd },
	{ "printenv", printenv_cmd },
	{ "pwd", pwd_cmd },
	{ "read", read_cmd },
	{ "readt", readt_cmd },
	{ "release", release_cmd },
	{ "resetdtree", resetdtree_cmd },
	{ "run", loadnrun },
	{ "set", set_cmd },
	{ "setbinprop", setbin_cmd },
	{ "setcolor", setcolor_cmd },
	{ "setenv", setenv_cmd },
	{ "setprop", setprop_cmd },
	{ "show-devs", show_cmd },
	{ "singlestep", singlestep_cmd },
	{ "source", source_cmd },
	{ "unset", unset_cmd },
	{ "verbose", verbose_cmd },
	{ "voltell", voltell_cmd },
};

int cmd_count = sizeof (cmd)/sizeof (cmd[0]);
