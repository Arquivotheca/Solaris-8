#if !defined(lint) && defined(SCCSIDS)
static char	*bsm_sccsid = "@(#)mkdevmaps.c 1.5 97/11/17 SMI; SunOS BSM";
#endif

/*
 * scan /dev directory for mountable objects and construct device_maps
 * file for allocate....
 *
 * devices are:
 *	tape (cartridge)
 *		/dev/rst*
 *		/dev/nrst*
 *		/dev/rmt/...
 *	audio
 *		/dev/audio
 *		/dev/audioctl
 *		/dev/sound/...
 *	floppy
 *		/dev/diskette
 *		/dev/fd*
 *		/dev/rdiskette
 *		/dev/rfd*
 *	CD
 *		/dev/sr*
 *		/dev/nsr*
 *		/dev/dsk/c0t?d0s?
 *		/dev/rdsk/c0t?d0s?
 */

#include <sys/types.h>	/* for stat(2), etc. */
#include <sys/stat.h>
#include <dirent.h>	/* for readdir(3), etc. */
#include <unistd.h>	/* for readlink(2) */
#include <string.h>	/* for strcpy(3), etc. */
#include <stdio.h>	/* for perror(3) */
#include <stdlib.h>	/* for atoi(3) */
#include <locale.h>
#include <libintl.h>

#ifndef TEXT_DOMAIN
#define	TEXT_DOMAIN	"SUNW_OST_OSCMD"
#endif


/* "/dev/rst...", "/dev/nrst...", "/dev/rmt/..." */
struct tape {
	char	*name;
	char	*device;
	int	number;
} tape[128];
#define	SIZE_OF_RST  3		/* |rmt| */
#define	SIZE_OF_NRST 4		/* |nrmt| */
#define	SIZE_OF_TMP  4		/* |/tmp| */
#define	SIZE_OF_RMT  8		/* |/dev/rmt| */

/* "/dev/audio", "/dev/audioctl", "/dev/sound/..." */
struct audio {
	char	*name;
	char	*device;
	int	number;
} audio[128];
#define	SIZE_OF_SOUND 10	/* |/dev/sound| */

/* "/dev/sr", "/dev/nsr", "/dev/dsk/c0t?d0s?", "/dev/rdsk/c0t?d0s?" */
struct cd {
	char	*name;
	char	*device;
	int	id;
	int	number;
} cd[128];
#define	SIZE_OF_SR  2		/* |sr| */
#define	SIZE_OF_RSR 3		/* |rsr| */
#define	SIZE_OF_DSK 8		/* |/dev/dsk| */
#define	SIZE_OF_RDSK 9		/* |/dev/rdsk| */

/* "/dev/fd0*", "/dev/rfd0*", "/dev/fd1*", "/dev/rfd1*" */
struct fp {
	char *name;
	char *device;
	int number;
} fp[16];
#define	SIZE_OF_FD0  3		/* |fd0| */
#define	SIZE_OF_RFD0 4		/* |rfd0| */

static void dotape();
static void doaudio();
static void dofloppy();
static void docd();

main()
{
	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	dotape();		/* do tape */
	doaudio();		/* do audio */
	dofloppy();		/* do floppy */
	docd();			/* do cd */
}

static void
dotape()
{
	DIR	*dirp;
	struct dirent *dep;		/* directory entry pointer */
	int	i, j, n;
	char	*nm;		/* name/device of special device */
	char	linkvalue[2048];	/* symlink value */
	struct stat stat;		/* determine if it's a symlink */
	int	sz;		/* size of symlink value */
	char	*cp;		/* pointer into string */
	int	first = 0;	/* kludge to put space between entries */

	/*
					 * look for rst* and nrst*
					 */

	if ((dirp = opendir("/dev")) == NULL) {
		perror("open /dev failure");
		exit(1);
	}

	i = 0;
	while (dep = readdir(dirp)) {
		/* ignore if neither rst* nor nrst* */
		if (strncmp(dep->d_name, "rst", SIZE_OF_RST) &&
		    strncmp(dep->d_name, "nrst", SIZE_OF_NRST))
			continue;

		/* save name */
		nm = (char *)malloc(strlen(dep->d_name) + 1 + SIZE_OF_TMP);
		(void) strcpy(nm, "/dev/");
		(void) strcat(nm, dep->d_name);
		tape[i].name = nm;

		/* ignore if not symbolic link (note i not incremented) */
		if (lstat(tape[i].name, &stat) < 0) {
			perror(gettext("stat(2) failed "));
			exit(1);
		}
		if ((stat.st_mode & S_IFMT) != S_IFLNK)
			continue;
		/* get name from symbolic link */
		if ((sz = readlink(tape[i].name, linkvalue, sizeof (linkvalue)))
				< 0)
			continue;
		nm = (char *)malloc(sz + 1);
		(void) strncpy(nm, linkvalue, sz);
		nm[sz] = '\0';
		tape[i].device = nm;

		/* get device number */
		cp = strrchr(tape[i].device, '/');
		cp++;				/* advance to device # */
		(void) sscanf(cp, "%d", &tape[i].number);

		i++;
	}

	(void) closedir(dirp);

	/*
	 * scan /dev/rmt and add entry to table
	 */

	if ((dirp = opendir("/dev/rmt")) == NULL) {
		perror(gettext("open /dev failure"));
		exit(1);
	}

	while (dep = readdir(dirp)) {
		/* skip . .. etc... */
		if (strncmp(dep->d_name, ".", 1) == NULL)
			continue;
		/* save name */
		nm = (char *)malloc(strlen(dep->d_name) + 1 + SIZE_OF_RMT);
		(void) strcpy(nm, "/dev/rmt/");
		(void) strcat(nm, dep->d_name);
		tape[i].name = nm;

		/* save device name */
		nm = (char *)malloc(strlen(dep->d_name) + 1 + SIZE_OF_TMP);
		(void) strcpy(nm, "rmt/");
		(void) strcat(nm, dep->d_name);
		tape[i].device = nm;

		(void) sscanf(dep->d_name, "%d", &tape[i].number);

		i++;
	}
	n = i;

	(void) closedir(dirp);

	/* remove duplicate entries */
	for (i = 0; i < n - 1; i++) {
		for (j = i + 1; j < n; j++) {
			if (strcmp(tape[i].device, tape[j].device))
				continue;
			tape[j].number = -1;
		}
	}

	/* print out device_maps entries for tape devices */
	for (i = 0; i < 8; i++) {
		for (j = 0; j < n; j++) {
			if (tape[j].number != i)
				continue;
			if (first)
				(void) printf(" ");
			else {
				(void) printf("st%d:\\\n", i);
				(void) printf("\trmt:\\\n");
				(void) printf("\t");
				first++;
			}
			(void) printf("%s", tape[j].name);
		}
		if (first) {
			(void) printf(":\\\n\n");
			first = 0;
		}
	}

}

static void
doaudio()
{
	DIR	*dirp;
	struct dirent *dep;		/* directory entry pointer */
	int	i, j, n;
	char	*nm;		/* name/device of special device */
	char	linkvalue[2048];	/* symlink value */
	struct stat stat;		/* determine if it's a symlink */
	int	sz;		/* size of symlink value */
	char	*cp;		/* pointer into string */
	int	first = 0;	/* kludge to put space between entries */

	if ((dirp = opendir("/dev")) == NULL) {
		perror(gettext("open /dev failure"));
		exit(1);
	}

	i = 0;
	while (dep = readdir(dirp)) {
		if (strcmp(dep->d_name, "audio") &&
		    strcmp(dep->d_name, "audioctl"))
			continue;
		/* save name */
		nm = (char *)malloc(strlen(dep->d_name) + 1 + SIZE_OF_TMP);
		(void) strcpy(nm, "/dev/");
		(void) strcat(nm, dep->d_name);
		audio[i].name = nm;

		/* ignore if not symbolic link (note i not incremented) */
		if (lstat(audio[i].name, &stat) < 0) {
			perror(gettext("stat(2) failed "));
			exit(1);
		}
		if ((stat.st_mode & S_IFMT) != S_IFLNK)
			continue;
		/* get name from symbolic link */
		if ((sz = readlink(audio[i].name, linkvalue,
				sizeof (linkvalue))) < 0)
			continue;
		nm = (char *)malloc(sz + 1);
		(void) strncpy(nm, linkvalue, sz);
		nm[sz] = '\0';
		audio[i].device = nm;

		cp = strrchr(audio[i].device, '/');
		cp++;				/* advance to device # */
		(void) sscanf(cp, "%d", &audio[i].number);

		i++;
	}

	(void) closedir(dirp);

	if ((dirp = opendir("/dev/sound")) == NULL) {
		goto skip;
	}

	while (dep = readdir(dirp)) {
		/* skip . .. etc... */
		if (strncmp(dep->d_name, ".", 1) == NULL)
			continue;
		/* save name */
		nm = (char *)malloc(strlen(dep->d_name) + 1 + SIZE_OF_SOUND);
		(void) strcpy(nm, "/dev/sound/");
		(void) strcat(nm, dep->d_name);
		audio[i].name = nm;

		nm = (char *)malloc(strlen(dep->d_name) + 1 + SIZE_OF_SOUND);
		(void) strcpy(nm, "/dev/sound/");
		(void) strcat(nm, dep->d_name);
		audio[i].device = nm;

		(void) sscanf(dep->d_name, "%d", &audio[i].number);

		i++;
	}

	(void) closedir(dirp);

skip:
	n = i;

	/* remove duplicate entries */
	for (i = 0; i < n - 1; i++) {
		for (j = i + 1; j < n; j++) {
			if (strcmp(audio[i].device, audio[j].device))
				continue;
			audio[j].number = -1;
		}
	}

	/* print out device_maps entries for tape devices */
	for (i = 0; i < 8; i++) {
		for (j = 0; j < n; j++) {
			if (audio[j].number != i)
				continue;
			if (first)
				(void) printf(" ");
			else {
				(void) printf("audio:\\\n");
				(void) printf("\taudio:\\\n");
				(void) printf("\t");
				first++;
			}
			(void) printf("%s", audio[j].name);
		}
		if (first) {
			(void) printf(":\\\n\n");
			first = 0;
		}
	}

}

static void
dofloppy()
{
	DIR *dirp;
	struct dirent *dep;	/* directory entry pointer */
	int i, j, n;
	char *nm;		/* name/device of special device */
	char linkvalue[2048];	/* symlink value */
	struct stat stat;	/* determine if it's a symlink */
	int sz;			/* size of symlink value */
	char *cp;		/* pointer into string */
	int first = 0;		/* kludge to put space between entries */

	/*
	 * look for fd0* and rfd0*
	 */

	if ((dirp = opendir("/dev")) == NULL) {
		perror(gettext("open /dev failure"));
		exit(1);
	}

	i = 0;
	while (dep = readdir(dirp)) {
		/* ignore if neither rst* nor nrst* */
		if (strncmp(dep->d_name, "fd0", SIZE_OF_FD0) &&
		strncmp(dep->d_name, "rfd0", SIZE_OF_RFD0) &&
		strncmp(dep->d_name, "fd1", SIZE_OF_FD0) &&
		strncmp(dep->d_name, "rfd1", SIZE_OF_RFD0))
			continue;

		/* save name */
		nm = (char *)malloc(strlen(dep->d_name)+1+SIZE_OF_TMP);
		strcpy(nm, "/dev/");
		strcat(nm, dep->d_name);
		fp[i].name = nm;

		/* ignore if not symbolic link (note i not incremented) */
		if (lstat(fp[i].name, &stat) < 0) {
			perror(gettext("stat(2) failed "));
			exit(1);
		}
		if ((stat.st_mode&S_IFMT) != S_IFLNK)
			continue;

		/* get name from symbolic link */
		if ((sz = readlink(fp[i].name, linkvalue, sizeof (linkvalue)))
			< 0)
				continue;
		nm = (char *)malloc(sz+1);
		strncpy(nm, linkvalue, sz);
		nm[sz] = '\0';
		fp[i].device = nm;

		/* get device number */
		cp = strchr(fp[i].name, 'd');
		cp++;				/* advance to device # */
		cp = strchr(cp, 'd');
		cp++;				/* advance to device # */
		sscanf(cp, "%d", &fp[i].number);
		i++;
	}

	closedir(dirp);
	n = i;
	closedir(dirp);

	/* print out device_maps entries for floppy devices */
	for (i = 0; i < 8; i++) {
		for (j = 0; j < n; j++) {
			if (fp[j].number != i)
				continue;
			if (first)
				printf(" ");
			else {
				printf("fd%d:\\\n", i);
				printf("\tfd:\\\n");
				printf("\t");
				if (i == 0)
					printf("/dev/diskette /dev/rdiskette ");
				first++;
			}
			printf("%s", fp[j].name);
		}
		if (first) {
			printf(":\\\n\n");
			first = 0;
		}
	}
}

static void
docd()
{
	DIR *dirp;
	struct dirent *dep;		/* directory entry pointer */
	int	i, j, n;
	char	*nm;		/* name/device of special device */
	char	linkvalue[2048];	/* symlink value */
	struct stat stat;		/* determine if it's a symlink */
	int	sz;		/* size of symlink value */
	char	*cp;		/* pointer into string */
	int	first = 0;	/* kludge to put space between entries */
	int	id;		/* disk id */

	/*
	 * look for sr* and rsr*
	 */

	if ((dirp = opendir("/dev")) == NULL) {
		perror(gettext("open /dev failure"));
		exit(1);
	}

	i = 0;
	while (dep = readdir(dirp)) {
		/* ignore if neither sr* nor rsr* */
		if (strncmp(dep->d_name, "sr", SIZE_OF_SR) &&
		    strncmp(dep->d_name, "rsr", SIZE_OF_RSR))
			continue;

		/* save name */
		nm = (char *)malloc(strlen(dep->d_name) + 1 + SIZE_OF_TMP);
		(void) strcpy(nm, "/dev/");
		(void) strcat(nm, dep->d_name);
		cd[i].name = nm;

		/* save id # */
		if (dep->d_name[0] == 'r')
			(void) sscanf(dep->d_name, "rsr%d", &cd[i].id);
			else
			(void) sscanf(dep->d_name, "sr%d", &cd[i].id);

		/* ignore if not symbolic link (note i not incremented) */
		if (lstat(cd[i].name, &stat) < 0) {
			perror(gettext("stat(2) failed "));
			exit(1);
		}
		if ((stat.st_mode & S_IFMT) != S_IFLNK)
			continue;
		/* get name from symbolic link */
		if ((sz = readlink(cd[i].name, linkvalue,
				sizeof (linkvalue))) < 0)
			continue;
		nm = (char *)malloc(sz + 1);
		(void) strncpy(nm, linkvalue, sz);
		nm[sz] = '\0';
		cd[i].device = nm;

		cp = strrchr(cd[i].device, '/');
		cp++;				/* advance to device # */
		(void) sscanf(cp, "c0t%d", &cd[i].number);

		i++;
	}
	n = i;

	(void) closedir(dirp);

	/*
					 * scan /dev/dsk for cd devices
					 */

	if ((dirp = opendir("/dev/dsk")) == NULL) {
		perror(gettext("open /dev/dsk failure"));
		exit(1);
	}

	while (dep = readdir(dirp)) {
		/* skip . .. etc... */
		if (strncmp(dep->d_name, ".", 1) == NULL)
			continue;

		/* get device # (disk #) */
		if (sscanf(dep->d_name, "c0t%d", &id) <= 0)
			continue;

		/* see if this is one of the cd special devices */
		for (j = 0; j < n; j++) {
			if (cd[j].number == id)
				goto found;
		}
		continue;

		/* add new entry to table */
found:
		nm = (char *)malloc(strlen(dep->d_name) + 1 + SIZE_OF_DSK);
		(void) strcpy(nm, "/dev/dsk/");
		(void) strcat(nm, dep->d_name);
		cd[i].name = nm;

		cd[i].id = cd[j].id;

		cd[i].device = "";

		cd[i].number = id;

		i++;
	}

	(void) closedir(dirp);

	/*
					 * scan /dev/rdsk for cd devices
					 */

	if ((dirp = opendir("/dev/rdsk")) == NULL) {
		perror(gettext("open /dev/dsk failure"));
		exit(1);
	}

	while (dep = readdir(dirp)) {
		/* skip . .. etc... */
		if (strncmp(dep->d_name, ".", 1) == NULL)
			continue;

		/* get device # (disk #) */
		if (sscanf(dep->d_name, "c0t%d", &id) <= 0)
			continue;

		/* see if this is one of the cd special devices */
		for (j = 0; j < n; j++) {
			if (cd[j].number == id)
				goto found1;
		}
		continue;

		/* add new entry to table */
found1:
		nm = (char *)malloc(strlen(dep->d_name) + 1 + SIZE_OF_RDSK);
		(void) strcpy(nm, "/dev/rdsk/");
		(void) strcat(nm, dep->d_name);
		cd[i].name = nm;

		cd[i].id = cd[j].id;

		cd[i].device = "";

		cd[i].number = id;

		i++;
	}

	(void) closedir(dirp);

	n = i;

	/* print out device_maps entries for tape devices */
	for (i = 0; i < 8; i++) {
		for (j = 0; j < n; j++) {
			if (cd[j].id != i)
				continue;
			if (first)
				(void) printf(" ");
			else {
				(void) printf("sr%d:\\\n", i);
				(void) printf("\tsr:\\\n");
				(void) printf("\t");
				first++;
			}
			(void) printf("%s", cd[j].name);
		}
		if (first) {
			(void) printf(":\\\n\n");
			first = 0;
		}
	}

}
