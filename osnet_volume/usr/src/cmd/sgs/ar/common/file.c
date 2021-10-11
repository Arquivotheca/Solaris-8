/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)file.c	1.20	99/10/29 SMI"

#include "gelf.h"
#include "inc.h"
#include "extern.h"

static char *str_base;	/* start of string table for names */
static char *str_top;	/* pointer to next available location */
static char *str_base1, *str_top1;


/*
 * Function Prototypes
 */
static long mklong_tab(int *);
static char *trimslash(char *s);

static long mksymtab(long **, Cmd_info *, int *);
static int writesymtab(char *, int, long, long, long *);
static void savename(char *);
static void savelongname(ARFILE *, char *);
static void sputl(long, char *);

static int sizeofmembers();
static int sizeofnewarchive(int, int);

static int search_sym_tab(Elf *, Elf_Scn *,
	long, long *, long **, char *, int *);

#ifdef BROWSER
static void sbrowser_search_stab(Elf *, int, int, char *);
#endif


int
getaf(Cmd_info *cmd_info)
{
	Elf_Cmd cmd;
	int fd;
	char *arnam = cmd_info->arnam;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		error_message(ELF_VERSION_ERROR,
		LIBELF_ERROR, elf_errmsg(-1));
		exit(1);
	}

	if ((cmd_info->afd = fd = open(arnam, O_RDONLY)) == -1) {
		if (errno == ENOENT) {
			/* archive does not exist yet, may have to create one */
			return (fd);
		} else {
			/* problem other than "does not exist" */
			error_message(SYS_OPEN_ERROR,
			SYSTEM_ERROR, strerror(errno), arnam);
			exit(1);
		}
	}

	cmd = ELF_C_READ;
	cmd_info->arf = elf_begin(fd, cmd, (Elf *)0);

	if (elf_kind(cmd_info->arf) != ELF_K_AR) {
		error_message(NOT_ARCHIVE_ERROR,
		PLAIN_ERROR, (char *)0, arnam);
		if (opt_FLAG(cmd_info, a_FLAG) || opt_FLAG(cmd_info, b_FLAG))
		    error_message(USAGE_06_ERROR,
		    PLAIN_ERROR, (char *)0, cmd_info->ponam);
		exit(1);
	}
	return (fd);
}

ARFILE *
getfile(Cmd_info *cmd_info)
{
	Elf_Arhdr *mem_header;
	register ARFILE	*file;
	char *tmp_rawname, *file_rawname;
	Elf *elf;
	char *arnam = cmd_info->arnam;
	int fd = cmd_info->afd;
	Elf *arf = cmd_info->arf;

	if (fd == -1)
		return (NULL); /* the archive doesn't exist */

	if ((elf = elf_begin(fd, ELF_C_READ, arf)) == 0)
		return (NULL);  /* the archive is empty or have hit the end */

	if ((mem_header = elf_getarhdr(elf)) == NULL) {
		error_message(ELF_MALARCHIVE_ERROR,
		LIBELF_ERROR, elf_errmsg(-1),
		arnam, elf_getbase(elf));
		exit(1);
	}

	/* zip past special members like the symbol and string table members */

	while (strncmp(mem_header->ar_name, "/", 1) == 0 ||
		strncmp(mem_header->ar_name, "//", 2) == 0) {
		(void) elf_next(elf);
		(void) elf_end(elf);
		if ((elf = elf_begin(fd, ELF_C_READ, arf)) == 0)
			return (NULL);
			/* the archive is empty or have hit the end */
		if ((mem_header = elf_getarhdr(elf)) == NULL) {
			error_message(ELF_MALARCHIVE_ERROR,
			LIBELF_ERROR, elf_errmsg(-1),
			arnam, elf_getbase(elf));
			exit(0);
		}
	}

	/*
	 * NOTE:
	 *	The mem_header->ar_name[] is set to a NULL string
	 *	if the archive member header has some error.
	 *	(See elf_getarhdr() man page.)
	 *	It is set to NULL for example, the ar command reads
	 *	the archive files created by SunOS 4.1 system.
	 *	See c block comment in cmd.c, "Incompatible Archive Header".
	 */
	file = newfile();
	(void) strncpy(file->ar_name, mem_header->ar_name, SNAME);

	if ((file->ar_longname
	    = (char *)malloc(strlen(mem_header->ar_name) * sizeof (char *)))
	    == NULL) {
		error_message(MALLOC_ERROR,
		PLAIN_ERROR, (char *)0);
		exit(1);
	}
	(void) strcpy(file->ar_longname, mem_header->ar_name);
	if ((file->ar_rawname
	    = (char *)malloc(strlen(mem_header->ar_rawname)*sizeof (char *)))
	    == NULL) {
		error_message(MALLOC_ERROR,
		PLAIN_ERROR, (char *)0);
		exit(1);
	}
	tmp_rawname = mem_header->ar_rawname;
	file_rawname = file->ar_rawname;
	while (!isspace(*tmp_rawname) &&
		((*file_rawname = *tmp_rawname) != '\0')) {
		file_rawname++;
		tmp_rawname++;
	}
	if (!(*tmp_rawname == '\0'))
		*file_rawname = '\0';

	file->ar_date = mem_header->ar_date;
	file->ar_uid  = mem_header->ar_uid;
	file->ar_gid  = mem_header->ar_gid;
	file->ar_mode = (unsigned long) mem_header->ar_mode;
	file->ar_size = mem_header->ar_size;

	/* reverse logic */
	if (!(opt_FLAG(cmd_info, t_FLAG) && !opt_FLAG(cmd_info, s_FLAG))) {
		size_t ptr;
		file->ar_flag = F_ELFRAW;
		if ((file->ar_contents = elf_rawfile(elf, &ptr))
		    == NULL) {
			if (ptr != 0) {
				error_message(ELF_RAWFILE_ERROR,
				LIBELF_ERROR, elf_errmsg(-1));
				exit(1);
			}
		}
		file->ar_elf = elf;
	}
	(void) elf_next(elf);
	return (file);
}

ARFILE *
newfile()
{
	static ARFILE	*buffer =  NULL;
	static int	count = 0;
	register ARFILE	*fileptr;

	if (count == 0) {
		if ((buffer = (ARFILE *) calloc(CHUNK, sizeof (ARFILE)))
		    == NULL) {
			error_message(MALLOC_ERROR,
			PLAIN_ERROR, (char *)0);
			exit(1);
		}
		count = CHUNK;
	}
	count--;
	fileptr = buffer++;

	if (listhead)
		listend->ar_next = fileptr;
	else
		listhead = fileptr;
	listend = fileptr;
	return (fileptr);
}

static char *
trimslash(char *s)
{
	static char buf[SNAME];

	(void) strncpy(buf, trim(s), SNAME - 2);
	buf[SNAME - 2] = '\0';
	return (strcat(buf, "/"));
}

char *
trim(char *s)
{
	register char *p1, *p2;

	for (p1 = s; *p1; p1++)
		;
	while (p1 > s) {
		if (*--p1 != '/')
			break;
		*p1 = 0;
	}
	p2 = s;
	for (p1 = s; *p1; p1++)
		if (*p1 == '/')
			p2 = p1 + 1;
	return (p2);
}


static long
mksymtab(long **symlist, Cmd_info *cmd_info, int *found_obj)
{
	register ARFILE	*fptr;
	long	mem_offset = 0;
	Elf *elf;
	Elf_Scn	*scn;
	GElf_Ehdr ehdr;
	int newfd;
	long nsyms = 0;
	int class;
	Elf_Data *data;
	char *sbshstr;
	char *sbshstrtp;
	int sbstabsect = -1;
	int sbstabstrsect = -1;
	int num_errs = 0;

	newfd = 0;
	for (fptr = listhead; fptr; fptr = fptr->ar_next) {
		/* determine if file is coming from the archive or not */
		if ((fptr->ar_elf != 0) && (fptr->ar_pathname == NULL)) {
			/*
			 * I can use the saved elf descriptor.
			 */
			elf = fptr->ar_elf;
		} else if ((fptr->ar_elf == 0) &&
		    (fptr->ar_pathname != NULL)) {
			if ((newfd  =
			    open(fptr->ar_pathname, O_RDONLY)) == -1) {
				error_message(SYS_OPEN_ERROR,
				SYSTEM_ERROR, strerror(errno),
				fptr->ar_pathname);
				num_errs++;
				continue;
			}

			if ((elf = elf_begin(newfd,
					ELF_C_READ,
					(Elf *)0)) == 0) {
				if (fptr->ar_pathname != NULL)
					error_message(ELF_BEGIN_02_ERROR,
					LIBELF_ERROR, elf_errmsg(-1),
					fptr->ar_pathname);
				else
					error_message(ELF_BEGIN_03_ERROR,
					LIBELF_ERROR, elf_errmsg(-1));
				(void) close(newfd);
				newfd = 0;
				num_errs++;
				continue;
			}
			if (elf_kind(elf) == ELF_K_AR) {
				if (fptr->ar_pathname != NULL)
					error_message(ARCHIVE_IN_ARCHIVE_ERROR,
					PLAIN_ERROR, (char *)0,
					fptr->ar_pathname);
				else
					error_message(ARCHIVE_USAGE_ERROR,
					PLAIN_ERROR, (char *)0);
				if (newfd) {
					(void) close(newfd);
					newfd = 0;
				}
				(void) elf_end(elf);
				continue;
			}

			class = gelf_getclass(elf);
		} else {
			error_message(INTERNAL_01_ERROR,
			PLAIN_ERROR, (char *)0);
			exit(1);
		}
		if (gelf_getehdr(elf, &ehdr) != 0) {
			scn = elf_getscn(elf, ehdr.e_shstrndx);
			if (scn == NULL) {
				if (fptr->ar_pathname != NULL)
					error_message(ELF_GETSCN_01_ERROR,
					LIBELF_ERROR, elf_errmsg(-1),
					fptr->ar_pathname);
				else
					error_message(ELF_GETSCN_02_ERROR,
					LIBELF_ERROR, elf_errmsg(-1));
				num_errs++;
				if (newfd) {
					(void) close(newfd);
					newfd = 0;
				}
				(void) elf_end(elf);
				continue;
			}

			data = 0;
			data = elf_getdata(scn, data);
			if (data == NULL) {
				if (fptr->ar_pathname != NULL)
					error_message(ELF_GETDATA_01_ERROR,
					LIBELF_ERROR, elf_errmsg(-1),
					fptr->ar_pathname);
				else
					error_message(ELF_GETDATA_02_ERROR,
					LIBELF_ERROR, elf_errmsg(-1));
				num_errs++;
				if (newfd) {
					(void) close(newfd);
					newfd = 0;
				}
				(void) elf_end(elf);
				continue;
			}
			if (data->d_size == 0) {
				if (fptr->ar_pathname != NULL)
					error_message(W_ELF_NO_DATA_01_ERROR,
					PLAIN_ERROR, (char *)0,
					fptr->ar_pathname);
				else
					error_message(W_ELF_NO_DATA_02_ERROR,
					PLAIN_ERROR, (char *)0);
				if (newfd) {
					(void) close(newfd);
					newfd = 0;
				}
				(void) elf_end(elf);
				num_errs++;
				continue;
			}
			sbshstr = (char *)data->d_buf;

			/* loop through sections to find symbol table */
			scn = 0;
			while ((scn = elf_nextscn(elf, scn)) != 0) {
				GElf_Shdr shdr;
				if (gelf_getshdr(scn, &shdr) == NULL) {
					if (fptr->ar_pathname != NULL)
						error_message(
						ELF_GETDATA_01_ERROR,
						LIBELF_ERROR, elf_errmsg(-1),
						fptr->ar_pathname);
					else
						error_message(
						ELF_GETDATA_02_ERROR,
						LIBELF_ERROR, elf_errmsg(-1));
					if (newfd) {
						(void) close(newfd);
						newfd = 0;
					}
					num_errs++;
					(void) elf_end(elf);
					continue;
				}
				*found_obj = 1;
				if (shdr.sh_type == SHT_SYMTAB)
				    if (search_sym_tab(elf,
						scn,
						mem_offset,
						&nsyms,
						symlist,
						fptr->ar_pathname,
						&num_errs) == -1) {
					if (newfd) {
						(void) close(newfd);
						newfd = 0;
					}
					continue;
				    }
#ifdef BROWSER
				/*
				 * XX64:  sbrowser_search_stab() currently gets
				 *	confused by sb-tabs in v9.  at this
				 *	point, no one knows what v9 sb-tabs are
				 *	supposed to look like.
				 */
/*				if (shdr.sh_name != 0) { */
				if ((class == ELFCLASS32) &&
				    (shdr.sh_name != 0)) {
					sbshstrtp = (char *)
						((long)sbshstr + shdr.sh_name);
					if (strcmp(sbshstrtp, ".stab") == 0) {
						sbstabsect = elf_ndxscn(scn);
					} else if (strcmp(sbshstrtp,
							".stabstr") == 0) {
						sbstabstrsect = elf_ndxscn(scn);
					}
				}
#endif
			}
#ifdef BROWSER
			if (sbstabsect != -1 || sbstabstrsect != -1) {
				sbrowser_search_stab(
					elf,
					sbstabsect,
					sbstabstrsect,
					cmd_info->arnam);
				sbstabsect = -1;
				sbstabstrsect = -1;
			}
#endif
		}
		mem_offset += sizeof (struct ar_hdr) + fptr->ar_size;
		if (fptr->ar_size & 01)
			mem_offset++;
		(void) elf_end(elf);
		if (newfd) {
			(void) close(newfd);
			newfd = 0;
		}
	} /* for */
	if (num_errs)
		exit(1);
	return (nsyms);
}

/*
 * This routine writes an archive symbol table for the
 * output archive file. The symbol table is built if
 * there was at least one object file specified.
 * In rare case, there could be no symbol.
 * In this case, str_top and str_base can not be used to
 * make the string table. So the routine adjust the size
 * and make a dummy string table. String table is needed
 * by elf_getarsym().
 */
static int
writesymtab(char *dst, int longnames,
	long long_tab_size, long nsyms, long *symlist)
{
	long	offset;
	char	buf1[sizeof (struct ar_hdr) + 1];
	register char	*buf2, *bptr;
	int	i, j;
	long	*ptr;
	long	sym_tab_size = 0;
	int sum = 0;

	/*
	 * patch up archive pointers and write the symbol entries
	 */
	while ((str_top - str_base) & 03)	/* round up string table */
		*str_top++ = '\0';
	sym_tab_size = (nsyms +1) * 4 + sizeof (char) * (str_top - str_base);
	if (nsyms == 0)
		sym_tab_size += 4;
	offset = (nsyms + 1) * 4 + sizeof (char) * (str_top - str_base)
		+ sizeof (struct ar_hdr) + SARMAG;

	(void) sprintf(buf1, FORMAT, SYMDIRNAME, time(0), (unsigned)0,
		(unsigned)0, (unsigned)0, (long)sym_tab_size, ARFMAG);

	if (longnames)
		offset += long_tab_size + sizeof (struct ar_hdr);

	if (strlen(buf1) != sizeof (struct ar_hdr)) {
		error_message(INTERNAL_02_ERROR);
		exit(1);
	}

	if ((buf2 = (char *)malloc(4 * (nsyms + 1))) == NULL) {
		error_message(MALLOC_ERROR);
		error_message(DIAG_01_ERROR, errno);
		exit(1);
	}
	sputl(nsyms, buf2);
	bptr = buf2 + 4;

	for (i = 0, j = SYMCHUNK, ptr = symlist; i < nsyms; i++, j--, ptr++) {
		if (!j) {
			j = SYMCHUNK;
			ptr = (long *)*ptr;
		}
		*ptr += offset;
		sputl(*ptr, bptr);
		bptr += 4;
	}
	(void) memcpy(dst, buf1, sizeof (struct ar_hdr));
	dst += sizeof (struct ar_hdr);
	sum += sizeof (struct ar_hdr);

	(void) memcpy(dst, buf2, (nsyms + 1) * 4);
	dst += (nsyms + 1)*4;
	sum += (nsyms + 1)*4;

	if (nsyms != 0) {
		(void) memcpy(dst, str_base, (str_top - str_base));
		dst += str_top - str_base;
		sum += str_top - str_base;
	} else {
		/*
		 * Writing a dummy string table.
		 */
		int i;
		for (i = 0; i < 4; i++)
			*dst++ = 0;
		sum += 4;
	}

	free(buf2);
	return (sum);
}

static void
savename(char *symbol)
{
	static int str_length = BUFSIZ * 5;
	register char *p, *s;
	register unsigned int i;
	int diff;

	diff = 0;
	if (str_base == (char *)0) {
		/* no space allocated yet */
		if ((str_base = (char *)malloc((unsigned)str_length))
		    == NULL) {
			error_message(MALLOC_ERROR,
			PLAIN_ERROR, (char *)0);
			exit(1);
		}
		str_top = str_base;
	}

	p = str_top;
	str_top += strlen(symbol) + 1;

	if (str_top > str_base + str_length) {
		char *old_base = str_base;

		str_length += BUFSIZ * 2;
		if ((str_base = (char *)realloc(str_base, str_length)) ==
		    NULL) {
			error_message(MALLOC_ERROR,
			PLAIN_ERROR, (char *)0);
			exit(1);
		}
		/*
		 * Re-adjust other pointers
		 */
		diff = str_base - old_base;
		p += diff;
	}
	for (i = 0, s = symbol;
		i < strlen(symbol) && *s != '\0'; i++) {
		*p++ = *s++;
	}
	*p++ = '\0';
	str_top = p;
}

static void
savelongname(ARFILE *fptr, char *ptr_index)
{
	static int str_length = BUFSIZ * 5;
	register char *p, *s;
	register unsigned int i;
	int diff;
	static int bytes_used;
	int index;
	char	ptr_index1[SNAME-1];

	diff = 0;
	if (str_base1 == (char *)0) {
		/* no space allocated yet */
		if ((str_base1 = (char *)malloc((unsigned)str_length))
		    == NULL) {
			error_message(MALLOC_ERROR,
			PLAIN_ERROR, (char *)0);
			exit(1);
		}
		str_top1 = str_base1;
	}

	p = str_top1;
	str_top1 += strlen(fptr->ar_longname) + 2;

	index = bytes_used;
	(void) sprintf(ptr_index1, "%d", index); /* holds digits */
	(void) sprintf(ptr_index, "%-16s", SYMDIRNAME);
	ptr_index[1] = '\0';
	(void) strcat(ptr_index, ptr_index1);
	(void) strcpy(fptr->ar_name, ptr_index);
	bytes_used += strlen(fptr->ar_longname) + 2;

	if (str_top1 > str_base1 + str_length) {
		char *old_base = str_base1;

		str_length += BUFSIZ * 2;
		if ((str_base1 = (char *)realloc(str_base1, str_length))
		    == NULL) {
			error_message(MALLOC_ERROR,
			PLAIN_ERROR, (char *)0);
			exit(1);
		}
		/*
		 * Re-adjust other pointers
		 */
		diff = str_base1 - old_base;
		p += diff;
	}
	for (i = 0, s = fptr->ar_longname;
		i < strlen(fptr->ar_longname) && *s != '\0';
		i++) {
		*p++ = *s++;
	}
	*p++ = '/';
	*p++ = '\n';
	str_top1 = p;
}

char *
writefile(Cmd_info *cmd_info)
{
	register ARFILE	* fptr;
	char buf[sizeof (struct ar_hdr) + 1];
	char buf11[sizeof (struct ar_hdr) + 1];
	register int i;
	int longnames = 0;
	long long_tab_size = 0;
	long nsyms;
	long *symlist = 0;
	int new_archive = 0;
	char *name = cmd_info->arnam;
	int arsize;
	char *dst;
	char *tmp_dst;
	int nfd;
	int found_obj = 0;

	long_tab_size = mklong_tab(&longnames);
	nsyms = mksymtab(&symlist, cmd_info, &found_obj);

	for (i = 0; signum[i]; i++)
		/* started writing, cannot interrupt */
		(void) signal(signum[i], SIG_IGN);


	/* Is this a new archive? */
	if ((access(cmd_info->arnam,  0)  < 0) && (errno == ENOENT)) {
	    new_archive = 1;
	    if (!opt_FLAG(cmd_info, c_FLAG)) {
		error_message(BER_MES_CREATE_ERROR,
		PLAIN_ERROR, (char *)0, cmd_info->arnam);
	    }
	}
	else
	    new_archive = 0;

	/*
	 * Calculate the size of the new archive
	 */
	arsize = sizeofnewarchive(nsyms, longnames);

	/*
	 * Dummy symtab ?
	 */
	if (nsyms == 0 && found_obj != 0)
		/*
		 * 4 + 4 = First 4 bytes to keep the number of symbols.
		 *	   The second 4 bytes for string table.
		 */
		arsize += sizeof (struct ar_hdr) + 4 + 4;

	tmp_dst = dst = malloc(arsize);
	if (dst == NULL) {
		error_message(MALLOC_ERROR,
		PLAIN_ERROR, (char *)0);
		exit(1);
	}

	(void) memcpy(tmp_dst, ARMAG, SARMAG);
	tmp_dst += SARMAG;

	if (nsyms || found_obj != 0) {
		int diff;
		diff = writesymtab(tmp_dst, longnames,
		    long_tab_size, nsyms, symlist);
		tmp_dst += diff;
	}

	if (longnames) {
		(void) sprintf(buf11, FORMAT, LONGDIRNAME, time(0),
			(unsigned)0, (unsigned)0, (unsigned)0,
			(long)long_tab_size, ARFMAG);
		(void) memcpy(tmp_dst, buf11, sizeof (struct ar_hdr));
		tmp_dst += sizeof (struct ar_hdr);
		(void) memcpy(tmp_dst, str_base1, str_top1 - str_base1);
		tmp_dst += str_top1 - str_base1;
	}
	for (fptr = listhead; fptr; fptr = fptr->ar_next) {

	/*
	 * NOTE:
	 *	The mem_header->ar_name[] is set to a NULL string
	 *	if the archive member header has some error.
	 *	(See elf_getarhdr() man page.)
	 *	It is set to NULL for example, the ar command reads
	 *	the archive files created by SunOS 4.1 system.
	 *	See c block comment in cmd.c, "Incompatible Archive Header".
	 */
		if (fptr->ar_name[0] == 0) {
			fptr->ar_longname = fptr->ar_rawname;
			(void) strncpy(fptr->ar_name, fptr->ar_rawname, SNAME);
		}
		if (strlen(fptr->ar_longname) <= (unsigned)SNAME-2)
			(void) sprintf(buf, FORMAT,
			    trimslash(fptr->ar_longname), fptr->ar_date,
			    (unsigned)fptr->ar_uid, (unsigned)fptr->ar_gid,
			    (unsigned)fptr->ar_mode, fptr->ar_size, ARFMAG);
		else
			(void) sprintf(buf, FORMAT, fptr->ar_name,
			    fptr->ar_date, (unsigned)fptr->ar_uid,
			    (unsigned)fptr->ar_gid, (unsigned)fptr->ar_mode,
			    fptr->ar_size, ARFMAG);

		(void) memcpy(tmp_dst, buf, sizeof (struct ar_hdr));
		tmp_dst += sizeof (struct ar_hdr);
		(void) memcpy(tmp_dst, fptr->ar_contents, fptr->ar_size);
		tmp_dst += fptr->ar_size;

		if (fptr->ar_size & 0x1) {
			(void) memcpy(tmp_dst, "\n", 1);
			tmp_dst++;
		}
	}

	/*
	 * All preparation for writing is done.
	 */
	(void) elf_end(cmd_info->arf);
	(void) close(cmd_info->afd);

	/*
	 * Write out to the file
	 */
	if (new_archive) {
		/*
		 * create a new file
		 */
		nfd = creat(name, 0666);
		if (nfd == -1) {
			error_message(SYS_CREATE_01_ERROR,
			SYSTEM_ERROR, strerror(errno), name);
			exit(1);
		}
	} else {
		/*
		 * Open the new file
		 */
		nfd = open(name, O_RDWR|O_TRUNC);
		if (nfd == -1) {
			error_message(SYS_WRITE_02_ERROR,
			SYSTEM_ERROR, strerror(errno), name);
			exit(1);
		}
	}
#ifndef XPG4
	if (opt_FLAG(cmd_info, v_FLAG)) {
	    error_message(BER_MES_WRITE_ERROR,
	    PLAIN_ERROR, (char *)0,
	    cmd_info->arnam);
	}
#endif
	if (write(nfd, dst, arsize) != arsize) {
		error_message(SYS_WRITE_04_ERROR,
			SYSTEM_ERROR, strerror(errno), name);
		if (!new_archive)
			error_message(WARN_USER_ERROR,
			PLAIN_ERROR, (char *)0);
		exit(2);
	}
	return (dst);
}

static long
mklong_tab(int *longnames)
{
	ARFILE  *fptr;
	char ptr_index[SNAME+1];
	long ret = 0;

	for (fptr = listhead; fptr; fptr = fptr->ar_next) {
		if (strlen(fptr->ar_longname) >= (unsigned)SNAME-1) {
			(*longnames)++;
			savelongname(fptr, ptr_index);
			(void) strcpy(fptr->ar_name, ptr_index);
		}
	}
	if (*longnames) {
		/* round up table that keeps the long filenames */
		while ((str_top1 - str_base1) & 03)
			*str_top1++ = '\n';
		ret = sizeof (char) * (str_top1 - str_base1);
	}
	return (ret);
}

/* Put bytes in archive header in machine independent order.  */

static void
sputl(long n, char *cp)
{
	*cp++ = n >> 24;
	*cp++ = n >> 16;
	*cp++ = n >> 8;

	*cp++ = n & 255;
}

#ifdef BROWSER

static void
sbrowser_search_stab(Elf *elf, int stabid, int stabstrid, char *arnam)
{
	Elf_Scn		*stab_scn;
	Elf_Scn		*stabstr_scn;
	Elf_Data	*data;
	struct nlist	*np;
	struct nlist	*stabtab;
	char		*stabstrtab;
	char		*stabstroff;
	char		*symname;
	int		prevstabstrsz;
	GElf_Xword	nstab;
	GElf_Shdr	shdr;

	/* use the .stab and stabstr section index to find the data buffer */
	if (stabid == -1) {
		error_message(SBROW_01_ERROR,
		PLAIN_ERROR, (char *)0);
		return;
	}
	stab_scn = elf_getscn(elf, (size_t)stabid);
	if (stab_scn == NULL) {
		error_message(ELF_GETDATA_02_ERROR,
		LIBELF_ERROR, elf_errmsg(-1));
		return;
	}

	(void) gelf_getshdr(stab_scn, &shdr);
	/*
	 * Zero length .stab sections have been produced by some compilers so
	 * ignore the section if this is the case.
	 */
	if ((nstab = shdr.sh_size / shdr.sh_entsize) == 0)
		return;

	if (stabstrid == -1) {
		error_message(SBROW_02_ERROR,
		PLAIN_ERROR, (char *)0);
		return;
	}
	stabstr_scn = elf_getscn(elf, (size_t)stabstrid);
	if (stabstr_scn == NULL) {
		error_message(ELF_GETDATA_02_ERROR,
		LIBELF_ERROR, elf_errmsg(-1));
		return;
	}
	if (gelf_getshdr(stabstr_scn, &shdr) == NULL) {
		error_message(ELF_GETDATA_02_ERROR,
		LIBELF_ERROR, elf_errmsg(-1));
		return;
	}
	if (shdr.sh_size == 0) {
		error_message(SBROW_03_ERROR,
		PLAIN_ERROR, (char *)0);
		return;
	}

	data = 0;
	data = elf_getdata(stabstr_scn, data);
	if (data == NULL) {
		error_message(ELF_GETDATA_02_ERROR,
		LIBELF_ERROR, elf_errmsg(-1));
		return;
	}
	if (data->d_size == 0) {
		error_message(SBROW_02_ERROR,
		PLAIN_ERROR, (char *)0);
		return;
	}
	stabstrtab = (char *)data->d_buf;
	data = 0;
	data = elf_getdata(stab_scn, data);
	if (data == NULL) {
		error_message(ELF_GETDATA_02_ERROR,
		LIBELF_ERROR, elf_errmsg(-1));
		return;
	}
	if (data->d_size == 0) {
		error_message(SBROW_03_ERROR,
		PLAIN_ERROR, (char *)0);
		return;
	}
	stabtab = (struct nlist *)data->d_buf;
	stabstroff = stabstrtab;
	prevstabstrsz = 0;
	for (np = stabtab; np < &stabtab[nstab]; np++) {
		if (np->n_type == 0) {
			stabstroff += prevstabstrsz;
			prevstabstrsz = np->n_value;
		}
		symname = stabstroff + np->n_un.n_strx;
		if (np->n_type == 0x48) {
			sbfocus_symbol(&sb_data, arnam, "-a", symname);
		}
	}
}
#endif



static int
search_sym_tab(Elf *elf, Elf_Scn *scn, long mem_offset,
	long *nsyms, long **symlist, char *fname, int *num_errs)
{
	Elf_Data *str_data, *sym_data; /* string table, symbol table */
	Elf_Scn *str_scn;
	GElf_Sxword no_of_symbols;
	GElf_Shdr shdr;
	int counter;
	int str_shtype;
	char *symname;
	static long *sym_ptr = 0;
	static long *nextsym = NULL;
	static int syms_left = 0;

	(void) gelf_getshdr(scn, &shdr);
	str_scn = elf_getscn(elf, shdr.sh_link); /* index for string table */
	if (str_scn == NULL) {
		if (fname != NULL)
			error_message(ELF_GETDATA_01_ERROR,
			LIBELF_ERROR, elf_errmsg(-1),
			fname);
		else
			error_message(ELF_GETDATA_02_ERROR,
			LIBELF_ERROR, elf_errmsg(-1));
		(*num_errs)++;
		return (-1);
	}

	no_of_symbols = shdr.sh_size / shdr.sh_entsize;
	if (no_of_symbols == -1) {
		error_message(SYMTAB_01_ERROR,
		PLAIN_ERROR, (char *)0);
		return (-1);
	}

	(void) gelf_getshdr(str_scn, &shdr);
	str_shtype = shdr.sh_type;
	if (str_shtype == -1) {
		if (fname != NULL)
			error_message(ELF_GETDATA_01_ERROR,
			LIBELF_ERROR, elf_errmsg(-1), fname);
		else
			error_message(ELF_GETDATA_02_ERROR,
			LIBELF_ERROR, elf_errmsg(-1));
		(*num_errs)++;
		return (-1);
	}

	/* This test must happen before testing the string table. */
	if (no_of_symbols == 1)
		return (0);	/* no symbols; 0th symbol is the non-symbol */

	if (str_shtype != SHT_STRTAB) {
		if (fname != NULL)
			error_message(SYMTAB_02_ERROR,
			PLAIN_ERROR, (char *)0,
			fname);
		else
			error_message(SYMTAB_03_ERROR,
			PLAIN_ERROR, (char *)0);
		return (0);
	}
	str_data = 0;
	if ((str_data = elf_getdata(str_scn, str_data)) == 0) {
		if (fname != NULL)
			error_message(SYMTAB_04_ERROR,
			PLAIN_ERROR, (char *)0,
			fname);
		else
			error_message(SYMTAB_05_ERROR,
			PLAIN_ERROR, (char *)0);
		return (0);
	}
	if (str_data->d_size == 0) {
		if (fname != NULL)
			error_message(SYMTAB_06_ERROR,
			PLAIN_ERROR, (char *)0,
			fname);
		else
			error_message(SYMTAB_07_ERROR,
			PLAIN_ERROR, (char *)0);
		return (0);
	}
	sym_data = 0;
	if ((sym_data = elf_getdata(scn, sym_data)) == NULL) {
		if (fname != NULL)
			error_message(ELF_01_ERROR,
			LIBELF_ERROR, elf_errmsg(-1),
			fname, elf_errmsg(-1));
		else
			error_message(ELF_02_ERROR,
			LIBELF_ERROR, elf_errmsg(-1),
			elf_errmsg(-1));
		return (0);
	}

	/* start at 1, first symbol entry is ignored */
	for (counter = 1; counter < no_of_symbols; counter++) {
		GElf_Sym sym;
		(void) gelf_getsym(sym_data, counter, &sym);

		symname = (char *)(str_data->d_buf) + sym.st_name;

		if (((GELF_ST_BIND(sym.st_info) == STB_GLOBAL) ||
			(GELF_ST_BIND(sym.st_info) == STB_WEAK)) &&
			(sym.st_shndx != SHN_UNDEF)) {
			if (!syms_left) {
				sym_ptr = (long *)malloc((SYMCHUNK+1)
							*sizeof (long));
				if (sym_ptr == NULL) {
					error_message(MALLOC_ERROR,
					PLAIN_ERROR, (char *)0);
					exit(1);
				}
				syms_left = SYMCHUNK;
				if (nextsym)
					*nextsym = (long)sym_ptr;
				else
					*symlist = sym_ptr;
				nextsym = sym_ptr;
			}
			sym_ptr = nextsym;
			nextsym++;
			syms_left--;
			(*nsyms)++;
			*sym_ptr = mem_offset;
			savename(symname);	/* put name in the archiver's */
						/* symbol table string table */
		}
	}
	return (0);
}

/*
 * Get the output file size
 */
static int
sizeofmembers()
{
	int sum = 0;
	ARFILE *fptr;
	int hdrsize = sizeof (struct ar_hdr);

	for (fptr = listhead; fptr; fptr = fptr->ar_next) {
		sum += fptr->ar_size;
		if (fptr->ar_size & 01)
			sum++;
		sum += hdrsize;
	}
	return (sum);
}

static int
sizeofnewarchive(int nsyms, int longnames)
{
	int sum = 0;

	sum += SARMAG;

	if (nsyms) {
		char *top = (char *)str_top;
		char *base = (char *)str_base;

		while ((top - base) & 03)
			top++;
		sum += sizeof (struct ar_hdr);
		sum += (nsyms + 1) * 4;
		sum += top - base;
	}

	if (longnames) {
		sum += sizeof (struct ar_hdr);
		sum += str_top1 - str_base1;
	}

	sum += sizeofmembers();

	return (sum);
}
