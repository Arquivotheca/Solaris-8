/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1995, 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)file.c	1.25	98/10/09 SMI"

#include <errno.h>
#include "mcs.h"
#include "extern.h"
#include "gelf.h"

static int Sect_exists = 0;
static int notesegndx = -1;
static int notesctndx = -1;
static Seg_Table *b_e_seg_table;

static section_info_table *sec_table;
static int64_t *off_table;	/* array maintains section's offset; */
				/* set to retain old offset, else 0 */
static int64_t *nobits_table;  	/* array maintains NOBITS sections */

static char *new_sec_string = NULL;

#define	MMAP_USED	1
#define	MMAP_UNUSED	2
/*
 * Function prototypes.
 */
static void copy_file(int, char *, char *);
static void
copy_non_elf_to_temp_ar(int, Elf *, int, Elf_Arhdr *, char *, Cmd_Info *);
static void copy_elf_file_to_temp_ar_file(int, Elf_Arhdr *, char *);
static int process_file(Elf *, char *, Cmd_Info *);
static void initialize(int shnum);
static int build_segment_table(Elf*, GElf_Ehdr *);
static int traverse_file(Elf *, GElf_Ehdr *, char *, Cmd_Info *);
static uint64_t location(int64_t, int, Elf *);
static uint64_t scn_location(Elf_Scn *, Elf *);
static int build_file(Elf *, GElf_Ehdr *, Cmd_Info *);


int
each_file(char *cur_file, Cmd_Info *cmd_info)
{
	Elf *elf = 0;
	Elf_Cmd cmd;
	Elf *arf = 0;
	Elf_Arhdr *mem_header;
	char *cur_filenm = NULL;
	int code = 0;
	int error = 0, err = 0;
	int ar_file = 0;
	int fdartmp;
	int fd;
	int oflag;

	if (cmd_info->flags & MIGHT_CHG)
		oflag = O_RDWR;
	else
		oflag = O_RDONLY;

	if ((fd = open(cur_file, oflag)) == -1) {
		error_message(OPEN_ERROR,
		SYSTEM_ERROR, strerror(errno), prog, cur_file);
		return (FAILURE);
	}

	cmd = ELF_C_READ;
	if ((arf = elf_begin(fd, cmd, (Elf *)0)) == 0) {
		error_message(LIBELF_ERROR,
		LIBelf_ERROR, elf_errmsg(-1), prog);
		(void) elf_end(arf);
		(void) close(fd);   /* done processing this file */
		return (FAILURE);
	}

	if ((elf_kind(arf) == ELF_K_AR)) {
		ar_file = 1;
		if (CHK_OPT(cmd_info, MIGHT_CHG)) {
			artmpfile = tempnam(TMPDIR, "mcs2");
			if ((fdartmp = open(artmpfile,
			    O_WRONLY | O_APPEND | O_CREAT,
			    (mode_t) 0666)) == NULL) {
				error_message(OPEN_TEMP_ERROR,
				SYSTEM_ERROR, strerror(errno),
				prog, artmpfile);
				(void) elf_end(arf);
				(void) close(fd);
				exit(FAILURE);
			}
			/* write magic string to artmpfile */
			if ((write(fdartmp, ARMAG, SARMAG)) != SARMAG) {
				error_message(WRITE_ERROR,
				SYSTEM_ERROR, strerror(errno),
				prog, artmpfile, cur_file);
				mcs_exit(FAILURE);
			}
		}
	} else {
		ar_file = 0;
		cur_filenm = cur_file;
	}

	/*
	 * Holds temporary file;
	 * if archive, holds the current member file if it has an ehdr,
	 * and there were no errors in
	 * processing the object file.
	 */
	elftmpfile = tempnam(TMPDIR, "mcs1");

	while ((elf = elf_begin(fd, cmd, arf)) != 0) {
		if (ar_file) /* get header info */ {
			if ((mem_header = elf_getarhdr(elf)) == NULL) {
				error_message(GETARHDR_ERROR,
				LIBelf_ERROR, elf_errmsg(-1),
				prog, cur_file, elf_getbase(elf));
				(void) elf_end(elf);
				(void) elf_end(arf);
				(void) close(fd);
				(void) unlink(artmpfile);
				return (FAILURE);
			}

			if (cur_filenm != NULL)
				free(cur_filenm);
			if ((cur_filenm = (char *)malloc((strlen(cur_file) + 3 +
			    strlen(mem_header->ar_name)))) == NULL) {
				error_message(MALLOC_ERROR,
				PLAIN_ERROR, (char *)0,
				prog);
				mcs_exit(FAILURE);
			}

			(void) sprintf(cur_filenm, "%s[%s]",
				cur_file, mem_header->ar_name);
		}

		if (elf_kind(elf) == ELF_K_ELF) {
			if ((code = process_file(elf, cur_filenm, cmd_info)) ==
			    FAILURE) {
				if (!ar_file) {
					(void) elf_end(arf);
					(void) elf_end(elf);
					(void) close(fd);
					return (FAILURE);
				} else {
					copy_non_elf_to_temp_ar(
					fd, elf, fdartmp, mem_header,
					cur_file, cmd_info);
					error++;
				}
			} else if (ar_file && CHK_OPT(cmd_info, MIGHT_CHG)) {
				if (code == DONT_BUILD)
					copy_non_elf_to_temp_ar(
					fd, elf, fdartmp, mem_header,
					cur_file, cmd_info);
				else
					copy_elf_file_to_temp_ar_file(
						fdartmp, mem_header, cur_file);
			}
		} else {
			/*
			 * decide what to do with non-ELF file
			 */
			if (!ar_file) {
				error_message(FILE_TYPE_ERROR,
				PLAIN_ERROR, (char *)0,
				prog, cur_filenm);
				(void) close(fd);
				return (FAILURE);
			} else {
				if (CHK_OPT(cmd_info, MIGHT_CHG))
					copy_non_elf_to_temp_ar(
					fd, elf, fdartmp, mem_header,
					cur_file, cmd_info);
			}
		}
		cmd = elf_next(elf);
		(void) elf_end(elf);
	}

	err = elf_errno();
	if (err != 0) {
		error_message(LIBELF_ERROR,
		LIBelf_ERROR, elf_errmsg(err), prog);
		error_message(NOT_MANIPULATED_ERROR,
		PLAIN_ERROR, (char *)0,
		prog, cur_file);
		return (FAILURE);
	}

	(void) elf_end(arf);

	if (ar_file && CHK_OPT(cmd_info, MIGHT_CHG)) {
		(void) close(fdartmp); /* done writing to ar_temp_file */
		/* copy ar_temp_file to FILE */
		copy_file(fd, cur_file, artmpfile);
	} else if (code != DONT_BUILD && CHK_OPT(cmd_info, MIGHT_CHG))
		copy_file(fd, cur_file, elftmpfile);
	(void) close(fd);   /* done processing this file */
	return (error);
}

static int
process_file(Elf *elf, char *cur_file, Cmd_Info *cmd_info)
{
	int error = SUCCESS;
	int x;
	GElf_Ehdr ehdr;

	/*
	 * Initialize
	 */
	if (gelf_getehdr(elf, &ehdr) != NULL)
		initialize(ehdr.e_shnum);
	else {
		error_message(LIBELF_ERROR,
		LIBelf_ERROR, elf_errmsg(-1), prog);
		return (FAILURE);
	}
	if (ehdr.e_phnum != 0) {
		if (build_segment_table(elf, &ehdr) == FAILURE)
			return (FAILURE);
	}

	if ((x = traverse_file(elf, &ehdr, cur_file, cmd_info)) ==
	    FAILURE) {
		error_message(WRN_MANIPULATED_ERROR,
		PLAIN_ERROR, (char *)0,
		prog, cur_file);
		error = FAILURE;
	} else if (x != DONT_BUILD && x != FAILURE) {
		if (build_file(elf, &ehdr, cmd_info) == FAILURE) {
			error_message(WRN_MANIPULATED_ERROR,
			PLAIN_ERROR, (char *)0,
			prog, cur_file);
			error = FAILURE;
		}
	}

	if (x == DONT_BUILD)
		return (DONT_BUILD);
	else {
		free(off_table);
		free(sec_table);
		free(nobits_table);
		return (error);
	}
}

static int
traverse_file(Elf *elf, GElf_Ehdr * ehdr, char *cur_file, Cmd_Info *cmd_info)
{
	Elf_Scn *scn;
	Elf_Scn *temp_scn;
	Elf_Data *data;
	int ret = 0;
	int SYM = 0;		/* used by strip command */
	GElf_Xword x;
	GElf_Shdr shdr;
	int phnum = ehdr->e_phnum;
	unsigned int ndx, i, scn_index = 1;
	char *temp_name;

	Sect_exists = 0;

	ndx = ehdr->e_shstrndx; /* used to find section's name */
	scn = 0;
	while ((scn = elf_nextscn(elf, scn)) != 0) {
		char *name;

		if (gelf_getshdr(scn, &shdr) == NULL) {
			error_message(NO_SECT_TABLE_ERROR,
			LIBelf_ERROR, elf_errmsg(-1),
			prog, cur_file);
			return (FAILURE);
		} else {
			name = elf_strptr(elf, ndx, (size_t)shdr.sh_name);
			if (name == NULL)
				name = "_@@@###";
		}

		sec_table[scn_index].scn	= scn;
		sec_table[scn_index+1].scn	= (Elf_Scn *) -1;
		sec_table[scn_index].secno	= scn_index;
		sec_table[scn_index].osecno	= scn_index;
		SET_ACTION(sec_table[scn_index].flags, ACT_NOP);
		sec_table[scn_index].name	= name;
		if (ehdr->e_phnum == 0)
			SET_LOC(sec_table[scn_index].flags, NOSEG);
		else
			SET_LOC(sec_table[scn_index].flags, \
				scn_location(scn, elf));

		/*
		 * If the target section is pointed by a section
		 * holding relocation infomation, then the
		 * pointing section would be useless if the
		 * target section is removed.
		 */
		if ((shdr.sh_type == SHT_REL || shdr.sh_type == SHT_RELA) &&
		    (shdr.sh_info != SHN_UNDEF &&
		    (temp_scn = elf_getscn(elf, shdr.sh_info)) != 0)) {
			GElf_Shdr tmp_shdr;
			if (gelf_getshdr(temp_scn, &tmp_shdr) != NULL) {
				temp_name = elf_strptr(elf, ndx,
					(size_t)tmp_shdr.sh_name);
				sec_table[scn_index].rel_name = temp_name;
				sec_table[scn_index].rel_scn_index =
				    shdr.sh_info;
				if (phnum == 0)
					sec_table[scn_index].rel_loc = NOSEG;
				    else
					sec_table[scn_index].rel_loc =
						scn_location(temp_scn, elf);
			}
		}
		data = 0;
		if ((data = elf_getdata(scn, data)) == NULL) {
			error_message(LIBELF_ERROR,
			LIBelf_ERROR, elf_errmsg(-1), prog);
			return (FAILURE);
		}
		sec_table[scn_index].data = data;

		/*
		 * Check if this section is a candidate for
		 * action to be processes.
		 */
		if (sectcmp(name) == 0) {
			SET_CANDIDATE(sec_table[scn_index].flags);

			/*
			 * This flag just shows that there was a
			 * candidate.
			 */
			Sect_exists++;
		}
		x = GET_LOC(sec_table[scn_index].flags);

		/*
		 * Remeber the note sections index so that we can
		 * reset the NOTE segments offset to point to it.
		 *
		 * It may have been assigned a new location in the
		 * resulting output elf image.
		 */
		if (shdr.sh_type == SHT_NOTE)
			notesctndx = scn_index;

		if (x == IN || x == PRIOR)
			off_table[scn_index] = shdr.sh_offset;
		if (shdr.sh_type == SHT_NOBITS)
			nobits_table[scn_index] = 1;

		/*
		 * If this section satisfies the condition,
		 * apply the actions specified.
		 */
		if (ISCANDIDATE(sec_table[scn_index].flags)) {
			ret += apply_action(&sec_table[scn_index],
				cur_file, cmd_info);
		}

		/*
		 * If I am strip command, determine if symtab can go or not.
		 */
		if (CHK_OPT(cmd_info, I_AM_STRIP) &&
		    (CHK_OPT(cmd_info, xFLAG) == 0) &&
		    (CHK_OPT(cmd_info, lFLAG) == 0)) {
			if (shdr.sh_type == SHT_SYMTAB &&
			    GET_LOC(sec_table[scn_index].flags) == AFTER) {
				SYM = scn_index;
			}
		}
		scn_index++;
	}

	/*
	 * If there were any errors traversing the file,
	 * just return error.
	 */
	if (ret != 0)
		return (FAILURE);

	/*
	 * Remove symbol table if possible
	 */
	if (CHK_OPT(cmd_info, I_AM_STRIP) && SYM != 0) {
		GElf_Shdr tmp_shdr;
		section_info_table *i = &sec_table[0];

		(void) gelf_getshdr(sec_table[SYM].scn, &tmp_shdr);
		sec_table[SYM].secno = DELETED;
		++(cmd_info->no_of_nulled);
		if (Sect_exists == 0)
			++Sect_exists;
		SET_ACTION(sec_table[SYM].flags, ACT_DELETE);
		off_table[SYM] = 0;
		/*
		 * Can I remove section header
		 * string table ?
		 */
		if ((tmp_shdr.sh_link != SHN_UNDEF) &&
		    (tmp_shdr.sh_link != ehdr->e_shstrndx) &&
		    (GET_LOC(i[tmp_shdr.sh_link].flags) == AFTER)) {
			i[tmp_shdr.sh_link].secno = DELETED;
			++(cmd_info->no_of_nulled);
			if (Sect_exists == 0)
				++Sect_exists;
			SET_ACTION(i[tmp_shdr.sh_link].flags, ACT_DELETE);
			off_table[tmp_shdr.sh_link] = 0;
		}
	}

	/*
	 * If I only printed the contents, then
	 * just report so.
	 */
	if (CHK_OPT(cmd_info, pFLAG) && !CHK_OPT(cmd_info, MIGHT_CHG))
		return (DONT_BUILD); /* don't bother creating a new file */
				/* since the file has not changed */

	/*
	 * I might need to add a new section. Check it.
	 */
	if (Sect_exists == 0 && CHK_OPT(cmd_info, aFLAG)) {
		int act = 0;
		new_sec_string = calloc(1, cmd_info->str_size + 1);
		if (new_sec_string == NULL)
			return (FAILURE);
		for (act = 0; act < actmax; act++) {
			if (Action[act].a_action == ACT_APPEND) {
				(void) strcat(new_sec_string,
					Action[act].a_string);
				(void) strcat(new_sec_string, "\n");
				cmd_info->no_of_append = 1;
			}
		}
	}

	/*
	 * If I did not append any new sections, and I did not
	 * modify/delete any sections, then just report so.
	 */
	if ((Sect_exists == 0 && cmd_info->no_of_append == 0) ||
	    !CHK_OPT(cmd_info, MIGHT_CHG))
		return (DONT_BUILD);

	/*
	 * Found at least one section which was processed.
	 *	Deleted or Appended or Compressed.
	 */
	if (Sect_exists) {
		/*
		 * First, handle the deleted sections.
		 */
		if (cmd_info->no_of_delete != 0 ||
		    cmd_info->no_of_nulled != 0) {
			int acc = 0;
			int rel_idx;

			/*
			 * Handle relocation/target
			 * sections.
			 */
			for (i = 1; i < ehdr->e_shnum; i++) {
				rel_idx = sec_table[i].rel_scn_index;
				if (rel_idx == 0)
					continue;

				/*
				 * If I am removed, then remove my
				 * target section.
				 */
				if ((sec_table[i].secno == DELETED ||
				    sec_table[i].secno == NULLED) &&
				    sec_table[i].rel_loc != IN) {
					if (GET_LOC(sec_table[rel_idx].flags) ==
					    PRIOR)
						sec_table[rel_idx].secno =
							NULLED;
					else
						sec_table[rel_idx].secno =
							DELETED;
					SET_ACTION(sec_table[rel_idx].flags,\
						ACT_DELETE);
				}

				/*
				 * I am not removed. Check if my target is
				 * removed or nulled. If so, let me try to
				 * remove my self.
				 */
				if ((sec_table[rel_idx].secno == DELETED ||
				    sec_table[rel_idx].secno == NULLED) &&
				    GET_LOC(sec_table[i].flags) != IN) {
					if (GET_LOC(sec_table[i].flags) ==
					    PRIOR)
						sec_table[i].secno =
							NULLED;
					else
						sec_table[i].secno =
							DELETED;
					SET_ACTION(sec_table[i].flags,\
						ACT_DELETE);
				}
			}

			/*
			 * Now, take care of DELETED sections
			 */
			for (i = 1; i < ehdr->e_shnum; i++) {
				if (sec_table[i].secno == DELETED)
					acc++;
				else
					sec_table[i].secno -= acc;
			}
		}
	}

	/*
	 * I know that the file has been modified.
	 * A new file need to be created.
	 */
	return (SUCCESS);
}

static int
build_file(Elf * src_elf, GElf_Ehdr * src_ehdr, Cmd_Info * cmd_info)
{
	Elf_Scn *src_scn;
	Elf_Scn *dst_scn;
	int	new_sh_name = 0;	/* to hold the offset for the new */
					/* section's name */
	Elf *dst_elf = 0;
	Elf_Data *elf_data;
	Elf_Data *data;
	int64_t scn_no, x;
	size_t no_of_symbols = 0;
	section_info_table *info;
	unsigned int    c = 0;
	int fdtmp;
	GElf_Shdr src_shdr;
	GElf_Shdr dst_shdr;
	GElf_Ehdr dst_ehdr;
	GElf_Off  new_offset = 0, r;



	if ((fdtmp = open(elftmpfile, O_RDWR |
		O_TRUNC | O_CREAT, (mode_t) 0666)) == -1) {
		error_message(OPEN_TEMP_ERROR,
		SYSTEM_ERROR, strerror(errno),
		prog, elftmpfile);
		return (FAILURE);
	}

	if ((dst_elf = elf_begin(fdtmp, ELF_C_WRITE, (Elf *) 0)) == NULL) {
		error_message(READ_ERROR,
		LIBelf_ERROR, elf_errmsg(-1),
		prog, elftmpfile);
		(void) close(fdtmp);
		return (FAILURE);
	}

	if (gelf_newehdr(dst_elf, gelf_getclass(src_elf)) == NULL) {
		error_message(LIBELF_ERROR,
		LIBelf_ERROR, elf_errmsg(-1), prog);
		return (FAILURE);
	}

	/* initialize dst_ehdr */
	(void) gelf_getehdr(dst_elf, &dst_ehdr);
	dst_ehdr = *src_ehdr;

	/*
	 * flush the changes to the ehdr so the
	 * ident array is filled in.
	 */
	(void) gelf_update_ehdr(dst_elf, &dst_ehdr);


	if (src_ehdr->e_phnum != 0) {
		(void) elf_flagelf(dst_elf, ELF_C_SET, ELF_F_LAYOUT);

		if (gelf_newphdr(dst_elf, src_ehdr->e_phnum) == NULL) {
			error_message(LIBELF_ERROR,
			LIBelf_ERROR, elf_errmsg(-1), prog);
			return (FAILURE);
		}

		for (x = 0; x < src_ehdr->e_phnum; ++x) {
			GElf_Phdr dst;
			GElf_Phdr src;

			/* LINTED */
			(void) gelf_getphdr(src_elf, (int)x, &src);
			/* LINTED */
			(void) gelf_getphdr(dst_elf, (int)x, &dst);
			(void) memcpy(&dst, &src, sizeof (GElf_Phdr));
			/* LINTED */
			(void) gelf_update_phdr(dst_elf, (int)x, &dst);
		}

		x = location(dst_ehdr.e_phoff, 0, src_elf);
		if (x == AFTER)
			new_offset = (GElf_Off)src_ehdr->e_ehsize;
	}

	scn_no = 1;
	while ((src_scn = sec_table[scn_no].scn) != (Elf_Scn *) -1) {
		info = &sec_table[scn_no];
		/*  If section should be copied to new file NOW */
		if (info->secno != DELETED && info->secno <= scn_no) {
			if ((dst_scn = elf_newscn(dst_elf)) == NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			(void) gelf_getshdr(dst_scn, &dst_shdr);
			(void) gelf_getshdr(info->scn, &src_shdr);
			(void) memcpy(&dst_shdr, &src_shdr, sizeof (GElf_Shdr));

			/* update link and info fields */
			if (sec_table[src_shdr.sh_link].secno < 0)
				dst_shdr.sh_link = 0;
			else
				dst_shdr.sh_link =
				sec_table[src_shdr.sh_link].secno;

			if ((src_shdr.sh_type == SHT_REL) ||
			    (src_shdr.sh_type == SHT_RELA)) {
				if (sec_table[src_shdr.sh_info].secno < 0)
					dst_shdr.sh_info = 0;
				else
					dst_shdr.sh_info =
					    sec_table[src_shdr.sh_info].secno;
			}

			data = sec_table[scn_no].data;
			if ((elf_data = elf_newdata(dst_scn)) == NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			*elf_data = *data;

			/* SYMTAB might need some change */
			if (src_shdr.sh_type == SHT_SYMTAB &&
			    src_shdr.sh_entsize != 0 &&
			    (cmd_info->no_of_delete != 0 ||
			    cmd_info->no_of_nulled != 0)) {
				char * new_sym;

				no_of_symbols = src_shdr.sh_size /
				    src_shdr.sh_entsize;
				new_sym = malloc(no_of_symbols *
						src_shdr.sh_entsize);
				if (new_sym == NULL) {
					error_message(MALLOC_ERROR,
					PLAIN_ERROR, (char *)0, prog);
					mcs_exit(FAILURE);
				}

				/* CSTYLED */
				elf_data->d_buf = (void *) new_sym;
				for (c = 0; c < no_of_symbols; c++) {
					GElf_Sym csym;

					(void) gelf_getsym(data, c, &csym);

					if (csym.st_shndx <= src_ehdr->e_shnum
					    /* CSTYLED */
					    && csym.st_shndx != 0) {
						section_info_table *i;
						i = &sec_table[csym.st_shndx];
						if (i->secno != DELETED &&
							i->secno != NULLED)
							csym.st_shndx =
							    i->secno;
						else {
						/*
						 * The section which this symbol
						 * relates to is removed.
						 * There is no way to specify
						 * this fact, just change the
						 * shndx to 1.
						 */
							csym.st_shndx = 1;
						}
					}

					(void) gelf_update_sym(elf_data, c,
					    &csym);
				}
			}

			/*
			 * If the section is to be updated,
			 * do so.
			 */
			if (ISCANDIDATE(info->flags)) {
				if (GET_LOC(info->flags) == PRIOR &&
				    (info->secno == NULLED ||
				    info->secno == EXPANDED ||
				    info->secno == SHRUNK)) {
					/*
					* The section is updated,
					* but the position is not too
					* good. Need to NULL this out.
					*/
					dst_shdr.sh_name = 0;
					dst_shdr.sh_type = SHT_PROGBITS;
					if (info->secno != NULLED) {
						(cmd_info->no_of_moved)++;
						SET_MOVING(info->flags);
					}
				} else {
					/*
					 * The section is positioned AFTER,
					 * or there are no segments.
					 * It is safe to update this section.
					 */
					data = sec_table[scn_no].mdata;
					*elf_data = *data;
					dst_shdr.sh_size = elf_data->d_size;
				}
			}
			/* add new section name to shstrtab? */
			else if (!Sect_exists &&
			    new_sec_string != NULL &&
			    scn_no == src_ehdr->e_shstrndx &&
			    dst_shdr.sh_type == SHT_STRTAB &&
			    (src_ehdr->e_phnum == 0 ||
			    (x = scn_location(dst_scn, dst_elf)) != IN ||
			    x != PRIOR)) {
				size_t sect_len;

				sect_len = strlen(SECT_NAME);
				if ((elf_data->d_buf = (char *)
				malloc((dst_shdr.sh_size +
				sect_len + 1))) == NULL) {
					error_message(MALLOC_ERROR,
					PLAIN_ERROR, (char *)0, prog);
					mcs_exit(FAILURE);
				}
				/* put original data plus new data in section */
				(void) memcpy(elf_data->d_buf,
					data->d_buf, data->d_size);
				(void) memcpy(&((char *) elf_data->d_buf)
					[data->d_size],
					SECT_NAME,
					sect_len + 1);
				/* LINTED */
				new_sh_name = (int)dst_shdr.sh_size;
				dst_shdr.sh_size += sect_len + 1;
				elf_data->d_size += sect_len + 1;
			}

			/*
			 * Compute offsets.
			 */
			if (src_ehdr->e_phnum != 0) {
				/*
				 * Compute section offset.
				 */
				if (off_table[scn_no] == 0) {
					if (dst_shdr.sh_addralign != 0) {
						r = new_offset %
						    dst_shdr.sh_addralign;
						if (r)
						    new_offset +=
						    dst_shdr.sh_addralign - r;
					}
					dst_shdr.sh_offset = new_offset;
					elf_data->d_off = 0;
				} else {
					if (nobits_table[scn_no] == 0)
						new_offset = off_table[scn_no];
				}
				if (nobits_table[scn_no] == 0)
					new_offset += dst_shdr.sh_size;
			}
		}

		(void) gelf_update_shdr(dst_scn, &dst_shdr); /* flush changes */
		scn_no++;
	}

	/*
	 * This is the real new section.
	 */
	if (!Sect_exists && new_sec_string != NULL) {
		size_t string_size;
		string_size = strlen(new_sec_string) + 1;
		if ((dst_scn = elf_newscn(dst_elf)) == NULL) {
			error_message(LIBELF_ERROR,
			LIBelf_ERROR, elf_errmsg(-1), prog);
			return (FAILURE);
		}
		(void) gelf_getshdr(dst_scn, &dst_shdr);

		dst_shdr.sh_name = new_sh_name;
		dst_shdr.sh_type = SHT_PROGBITS;
		dst_shdr.sh_flags = 0;
		dst_shdr.sh_addr = 0;
		if (src_ehdr->e_phnum != NULL)
			dst_shdr.sh_offset = new_offset;
		else
			dst_shdr.sh_offset = 0;
		dst_shdr.sh_size = string_size + 1;
		dst_shdr.sh_link = 0;
		dst_shdr.sh_info = 0;
		dst_shdr.sh_addralign = 1;
		dst_shdr.sh_entsize = 0;
		(void) gelf_update_shdr(dst_scn, &dst_shdr); /* flush changes */

		if ((elf_data = elf_newdata(dst_scn)) == NULL) {
			error_message(LIBELF_ERROR,
			LIBelf_ERROR, elf_errmsg(-1), prog);
			return (FAILURE);
		}
		elf_data->d_size = string_size + 1;
		if ((elf_data->d_buf = (char *)
		    calloc(1, string_size + 1)) == NULL) {
			error_message(MALLOC_ERROR,
			PLAIN_ERROR, (char *)0,
			prog);
			mcs_exit(FAILURE);
		}
		(void) memcpy(&((char *) elf_data->d_buf)[1],
			new_sec_string, string_size);
		elf_data->d_align = 1;
		new_offset += string_size + 1;
	}

	/*
	 * If there are sections which needed to be moved,
	 * then do it here.
	 */
	if (cmd_info->no_of_moved != 0) {
		int cnt;
		info = &sec_table[0];

		for (cnt = 0; cnt < src_ehdr->e_shnum; cnt++, info++) {
			if ((GET_MOVING(info->flags)) == 0)
				continue;

			if ((src_scn = elf_getscn(src_elf, info->osecno)) ==
			    NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			if (gelf_getshdr(src_scn, &src_shdr) == NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			if ((dst_scn = elf_newscn(dst_elf)) == NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			if (gelf_getshdr(dst_scn, &dst_shdr) == NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			dst_shdr = src_shdr;

			data = info->mdata;

			dst_shdr.sh_offset = new_offset;  /* UPDATE fields */
			dst_shdr.sh_size = data->d_size;
			dst_shdr.sh_link = sec_table[src_shdr.sh_link].osecno;
			dst_shdr.sh_info = sec_table[src_shdr.sh_info].osecno;
			(void) gelf_update_shdr(dst_scn, &dst_shdr);
			if ((elf_data = elf_newdata(dst_scn)) == NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			(void) memcpy(elf_data, data, sizeof (Elf_Data));

			new_offset += data->d_size;
		}
	}

	/*
	 * In the event that the position of the sting table has changed,
	 * as a result of deleted sections, update the ehdr->e_shstrndx.
	 */
	if (src_ehdr->e_shstrndx > 0 && src_ehdr->e_shnum > 0 &&
	    (sec_table[src_ehdr->e_shstrndx].secno < src_ehdr->e_shnum)) {
		dst_ehdr.e_shstrndx =
			sec_table[dst_ehdr.e_shstrndx].secno;
	}


	if (src_ehdr->e_phnum != 0) {
		size_t align = gelf_fsize(dst_elf, ELF_T_ADDR, 1, EV_CURRENT);

		/* UPDATE location of program header table */
		if (location(dst_ehdr.e_phoff, 0, dst_elf) == AFTER) {
			r = new_offset % align;
			if (r)
				new_offset += align - r;

			dst_ehdr.e_phoff = new_offset;
			new_offset += dst_ehdr.e_phnum
					* dst_ehdr.e_phentsize;
		}
		/* UPDATE location of section header table */
		if ((location(dst_ehdr.e_shoff, 0, src_elf) == AFTER) ||
		    ((location(dst_ehdr.e_shoff, 0, src_elf) == PRIOR) &&
		    (!Sect_exists && new_sec_string != NULL))) {
			r = new_offset % align;
			if (r)
				new_offset += align - r;

			dst_ehdr.e_shoff = new_offset;
		}
		free(b_e_seg_table);

		/*
		 * The NOTE segment is the one segment whos
		 * sections might get moved by mcs processing.
		 * Make sure that the NOTE segments offset points
		 * to the .note section.
		 */
		if ((notesegndx != -1) && (notesctndx != -1) &&
		    (sec_table[notesctndx].secno)) {
			Elf_Scn *	notescn;
			GElf_Shdr	nshdr;

			notescn = elf_getscn(dst_elf,
				sec_table[notesctndx].secno);
			(void) gelf_getshdr(notescn, &nshdr);

			if (gelf_getclass(dst_elf) == ELFCLASS32) {
				Elf32_Phdr * ph	= elf32_getphdr(dst_elf) +
				    notesegndx;
				/* LINTED */
				ph->p_offset	= (Elf32_Off)nshdr.sh_offset;
			} else {
				Elf64_Phdr * ph	= elf64_getphdr(dst_elf) +
				    notesegndx;
				ph->p_offset	= (Elf64_Off)nshdr.sh_offset;
			}
		}
	}

	/* copy ehdr changes back into real ehdr */
	(void) gelf_update_ehdr(dst_elf, &dst_ehdr);
	if (elf_update(dst_elf, ELF_C_WRITE) < 0) {
		error_message(LIBELF_ERROR,
		LIBelf_ERROR, elf_errmsg(-1), prog);
		return (FAILURE);
	}

	(void) elf_end(dst_elf);
	(void) close(fdtmp);
	return (SUCCESS);
}

/*
 * Search through PHT saving the beginning and ending segment offsets
 */
static int
build_segment_table(Elf * elf, GElf_Ehdr * ehdr)
{
	unsigned int i;

	if ((b_e_seg_table = (Seg_Table *)
		calloc(ehdr->e_phnum, sizeof (Seg_Table))) == NULL) {
		error_message(MALLOC_ERROR,
		PLAIN_ERROR, (char *)0,
		prog);
		mcs_exit(FAILURE);
	}

	for (i = 0; i < ehdr->e_phnum; i++) {
		GElf_Phdr ph;

		(void) gelf_getphdr(elf, i, &ph);

		/*
		 * remember the note SEGMENTS index so that we can
		 * re-set it's p_offset later if needed.
		 */
		if (ph.p_type & PT_NOTE)
			notesegndx = i;

		b_e_seg_table[i].p_offset = ph.p_offset;
		b_e_seg_table[i].p_memsz  = ph.p_offset + ph.p_memsz;
		b_e_seg_table[i].p_filesz = ph.p_offset + ph.p_filesz;
	}
	return (SUCCESS);
}


static void
copy_elf_file_to_temp_ar_file(
	int fdartmp,
	Elf_Arhdr *mem_header,
	char *cur_file)
{
	char *buf;
	char mem_header_buf[sizeof (struct ar_hdr) + 1];
	int fdtmp3;
	struct stat stbuf;

	if ((fdtmp3 = open(elftmpfile, O_RDONLY)) == -1) {
		error_message(OPEN_TEMP_ERROR,
		SYSTEM_ERROR, strerror(errno),
		prog, elftmpfile);
		mcs_exit(FAILURE);
	}

	(void) stat(elftmpfile, &stbuf); /* for size of file */

	if ((buf = (char *)
	    malloc(ROUNDUP(stbuf.st_size))) == NULL) {
		error_message(MALLOC_ERROR,
		PLAIN_ERROR, (char *)0,
		prog);
		mcs_exit(FAILURE);
	}

	if (read(fdtmp3, buf, stbuf.st_size) != stbuf.st_size) {
		error_message(READ_MANI_ERROR,
		SYSTEM_ERROR, strerror(errno),
		prog, elftmpfile, cur_file);
		mcs_exit(FAILURE);
	}

	(void) sprintf(mem_header_buf, FORMAT,
		mem_header->ar_rawname,
		mem_header->ar_date,
		(unsigned) mem_header->ar_uid,
		(unsigned) mem_header->ar_gid,
		(unsigned) mem_header->ar_mode,
		stbuf.st_size, ARFMAG);

	if (write(fdartmp, mem_header_buf,
	    (unsigned) sizeof (struct ar_hdr)) !=
	    (unsigned) sizeof (struct ar_hdr)) {
		error_message(WRITE_MANI_ERROR,
		SYSTEM_ERROR, strerror(errno),
		prog, elftmpfile, cur_file);
		mcs_exit(FAILURE);
	}

	if (stbuf.st_size & 0x1) {
		buf[stbuf.st_size] = '\n';
		if (write(fdartmp, buf, (size_t)ROUNDUP(stbuf.st_size)) !=
		    (size_t) ROUNDUP(stbuf.st_size)) {
			error_message(WRITE_MANI_ERROR,
			SYSTEM_ERROR, strerror(errno),
			prog, elftmpfile, cur_file);
			mcs_exit(FAILURE);
		}
	} else if (write(fdartmp, buf, stbuf.st_size) != stbuf.st_size) {
			error_message(WRITE_MANI_ERROR,
			SYSTEM_ERROR, strerror(errno),
			prog, elftmpfile, cur_file);
			mcs_exit(FAILURE);
	}
	free(buf);
	(void) close(fdtmp3);
}

static void
copy_non_elf_to_temp_ar(
	int fd,
	Elf *elf,
	int fdartmp,
	Elf_Arhdr *mem_header,
	char *cur_file,
	Cmd_Info *cmd_info)
{
	char    mem_header_buf[sizeof (struct ar_hdr) + 1];
	char *file_buf;

	if (strcmp(mem_header->ar_name, "/") != 0) {
		(void) sprintf(mem_header_buf, FORMAT,
			mem_header->ar_rawname,
			mem_header->ar_date,
			(unsigned) mem_header->ar_uid,
			(unsigned) mem_header->ar_gid,
			(unsigned) mem_header->ar_mode,
			mem_header->ar_size, ARFMAG);

		if (write(fdartmp, mem_header_buf, sizeof (struct ar_hdr)) !=
		    sizeof (struct ar_hdr)) {
			error_message(WRITE_MANI_ERROR,
			SYSTEM_ERROR, strerror(errno),
			prog, cur_file);
			mcs_exit(FAILURE);
		}
		if ((file_buf = (char *)
		    malloc(ROUNDUP(mem_header->ar_size))) == NULL) {
			error_message(MALLOC_ERROR,
			PLAIN_ERROR, (char *)0,
			prog);
			mcs_exit(FAILURE);
		}

		if (lseek(fd, elf_getbase(elf), 0) != elf_getbase(elf)) {
			error_message(WRITE_MANI_ERROR,
			prog, cur_file);
			mcs_exit(FAILURE);
		}

		if (read(fd, file_buf,
		    (size_t) ROUNDUP(mem_header->ar_size)) !=
		    (size_t) ROUNDUP(mem_header->ar_size)) {
			error_message(READ_MANI_ERROR,
			SYSTEM_ERROR, strerror(errno),
			prog, cur_file);
			mcs_exit(FAILURE);
		}
		if (write(fdartmp,
		    file_buf,
		    (size_t) ROUNDUP(mem_header->ar_size)) !=
		    (size_t) ROUNDUP(mem_header->ar_size)) {
			error_message(WRITE_MANI_ERROR,
			SYSTEM_ERROR, strerror(errno),
			prog, cur_file);
			mcs_exit(FAILURE);
		}
		free(file_buf);
	} else if (CHK_OPT(cmd_info, MIGHT_CHG)) {
		error_message(SYM_TAB_AR_ERROR,
		PLAIN_ERROR, (char *)0,
		prog, cur_file);
		error_message(EXEC_AR_ERROR,
		PLAIN_ERROR, (char *)0,
		cur_file);
	}
}

static void
copy_file(int ofd, char *fname, char *temp_file_name)
{
	int i;
	int fdtmp2;
	struct stat stbuf;
	char *buf;

	for (i = 0; signum[i]; i++) /* started writing, cannot interrupt */
		(void) signal(signum[i], SIG_IGN);

	if ((fdtmp2 = open(temp_file_name, O_RDONLY)) == -1) {
		error_message(OPEN_TEMP_ERROR,
		SYSTEM_ERROR, strerror(errno),
		prog, temp_file_name);
		mcs_exit(FAILURE);
	}

	(void) stat(temp_file_name, &stbuf); /* for size of file */

	/*
	 * Get the contents of the updated file.
	 * First try mmap()'ing. If mmap() fails,
	 * then use the malloc() and read().
	 */
	i = MMAP_USED;
	if ((buf = (char *) mmap(0, stbuf.st_size,
		PROT_READ, MAP_SHARED, fdtmp2, 0)) == (caddr_t) -1) {
		if ((buf = (char *)
		    malloc(stbuf.st_size * sizeof (char))) == NULL) {
			error_message(MALLOC_ERROR,
			PLAIN_ERROR, (char *)0,
			prog);
			mcs_exit(FAILURE);
		}

		if (read(fdtmp2, buf, stbuf.st_size) != stbuf.st_size) {
			error_message(READ_SYS_ERROR,
			SYSTEM_ERROR, strerror(errno),
			prog, temp_file_name);
			mcs_exit(FAILURE);
		}
		i = MMAP_UNUSED;
	}

	if (ftruncate(ofd, 0) == -1) {
		error_message(WRITE_MANI_ERROR2,
		SYSTEM_ERROR, strerror(errno),
		prog, fname);
		mcs_exit(FAILURE);
	}
	if (lseek(ofd, 0, SEEK_SET) == -1) {
		error_message(WRITE_MANI_ERROR2,
		SYSTEM_ERROR, strerror(errno),
		prog, fname);
		mcs_exit(FAILURE);
	}
	if ((write(ofd, buf, stbuf.st_size)) != stbuf.st_size) {
		error_message(WRITE_MANI_ERROR2,
		SYSTEM_ERROR, strerror(errno),
		prog, fname);
		mcs_exit(FAILURE);
	}

	/*
	 * clean
	 */
	if (i == MMAP_USED)
		(void) munmap(buf, stbuf.st_size);
	else
		free(buf);
	(void) close(fdtmp2);
	(void) unlink(temp_file_name); 	/* temp file */
}

static uint64_t
location(int64_t offset, int mem_search, Elf * elf)
{
	int i;
	uint64_t upper;
	GElf_Ehdr ehdr;

	(void) gelf_getehdr(elf, &ehdr);

	for (i = 0; i < ehdr.e_phnum; i++) {
		if (mem_search)
			upper = b_e_seg_table[i].p_memsz;
		else
			upper = b_e_seg_table[i].p_filesz;
		if ((offset >= b_e_seg_table[i].p_offset) &&
		    (offset <= upper))
			return (IN);
		else if (offset < b_e_seg_table[i].p_offset)
			return (PRIOR);
	}
	return (AFTER);
}

static uint64_t
scn_location(Elf_Scn * scn, Elf * elf)
{
	GElf_Shdr shdr;

	(void) gelf_getshdr(scn, &shdr);

	/*
	 * If the section is not a NOTE section and it has no
	 * virtual address then it is not part of a mapped segment.
	 */
	if (shdr.sh_addr == 0)
		return (location(shdr.sh_offset + shdr.sh_size, 0, elf));

	return (location(shdr.sh_offset + shdr.sh_size, 1, elf));
}

static void
initialize(int shnum)
{
	if ((sec_table = (section_info_table *)
		calloc(shnum + 1,
		sizeof (section_info_table))) == NULL) {
		error_message(MALLOC_ERROR,
		PLAIN_ERROR, (char *)0,
		prog);
		exit(FAILURE);
	}

	if ((off_table = (int64_t *)
		calloc(shnum,
		sizeof (int64_t))) == NULL) {
		error_message(MALLOC_ERROR,
		PLAIN_ERROR, (char *)0,
		prog);
		exit(FAILURE);
	}

	if ((nobits_table = (int64_t *)
		calloc(shnum, sizeof (int64_t))) == NULL) {
		error_message(MALLOC_ERROR,
		PLAIN_ERROR, (char *)0,
		prog);
		exit(FAILURE);
	}
}
