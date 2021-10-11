/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)outfile.c	1.54	99/06/09 SMI"

/*
 * This file contains the functions responsible for opening the output file
 * image, associating the appropriate input elf structures with the new image,
 * and obtaining new elf structures to define the new image.
 */
#include	<stdio.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<link.h>
#include	<errno.h>
#include	<string.h>
#include	<limits.h>
#include	"msg.h"
#include	"_libld.h"

/*
 * Open the output file and insure the correct access modes.
 */
uintptr_t
open_outfile(Ofl_desc * ofl)
{
	mode_t		mask, mode;
	struct stat	status;
	int		exists = 0;

	/*
	 * Determine the required file mode from the type of output file we
	 * are creating.
	 */
	if (ofl->ofl_flags & (FLG_OF_EXEC | FLG_OF_SHAROBJ))
		mode = 0777;
	else
		mode = 0666;

	/*
	 * Determine if the output file already exists.
	 */
	if (stat(ofl->ofl_name, &status) == 0)
		exists++;

	/*
	 * Open (or create) the output file name (ofl_fd acts as a global
	 * flag to ldexit() signifying whether the output file should be
	 * removed or not on error).
	 */
	if ((ofl->ofl_fd = open(ofl->ofl_name, O_RDWR | O_CREAT | O_TRUNC,
	    mode)) < 0) {
		int	err = errno;

		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN), ofl->ofl_name,
		    strerror(err));
		return (S_ERROR);
	}

	/*
	 * If we've just created this file the modes will be fine, however if
	 * the file had already existed make sure the modes are correct.
	 */
	if (exists) {
		mask = umask(0);
		(void) umask(mask);
		(void) chmod(ofl->ofl_name, mode & ~mask);
	}

	return (1);
}


/*
 * If we are creating a memory model we need to update the present memory image.
 * First we need to call elf_update(ELF_C_NULL) which will calculate the offsets
 * of each section and its associated data buffers.  From this information we
 * can then determine what padding is required.
 * Two actions are necessary to convert the present disc image into a memory
 * image:
 *
 *  o	Loadable segments must be padded so that the next segments virtual
 *	address and file offset are the same.
 *
 *  o	NOBITS sections must be converted into allocated, null filled sections.
 */
uintptr_t
pad_outfile(Ofl_desc * ofl)
{
	Listnode *	lnp1, * lnp2;
	off_t		offset;
	Elf_Scn *	oscn = 0;
	Sg_desc *	sgp;
	Os_desc *	osp;
	Ehdr *		ehdr;

	/*
	 * Update all the elf structures.  This will assign offsets to the
	 * section headers and data buffers as they relate to the new image.
	 */
	if (elf_update(ofl->ofl_welf, ELF_C_NULL) == -1) {
		eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_UPDATE), ofl->ofl_name);
		return (S_ERROR);
	}
	if ((ehdr = elf_getehdr(ofl->ofl_welf)) == NULL) {
		eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_GETEHDR), ofl->ofl_name);
		return (S_ERROR);
	}

	/*
	 * Initialize the offset by skipping the Elf header and program
	 * headers.
	 */
	offset = ehdr->e_phoff + (ehdr->e_phnum * ehdr->e_phentsize);

	/*
	 * Traverse the segment list looking for loadable segments.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp)) {
		Phdr *	phdr = &(sgp->sg_phdr);

		/*
		 * If we've already processed a loadable segment, the `scn'
		 * variable will be initialized to the last section that was
		 * part of that segment.  Add sufficient padding to this section
		 * to cause the next segments virtual address and file offset to
		 * be the same.
		 */
		if (oscn && (phdr->p_type == PT_LOAD)) {
			Elf_Data *	data;
			size_t 		size;

			size = (size_t)(S_ROUND(offset, phdr->p_align) -
			    offset);

			if ((data = elf_newdata(oscn)) == NULL) {
				eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_NEWDATA),
				    ofl->ofl_name);
				return (S_ERROR);
			}
			if ((data->d_buf = (void *)libld_calloc(size, 1)) == 0)
				return (S_ERROR);

			data->d_type = ELF_T_BYTE;
			data->d_size = size;
			data->d_align = 1;
			data->d_version = ofl->ofl_libver;
		}

		/*
		 * Traverse the output sections for this segment calculating the
		 * offset of each section. Retain the final section descriptor
		 * as this will be where any padding buffer will be added.
		 */
		for (LIST_TRAVERSE(&(sgp->sg_osdescs), lnp2, osp)) {
			Shdr *	shdr = osp->os_shdr;

			offset = (off_t)S_ROUND(offset, shdr->sh_addralign);
			offset += shdr->sh_size;

			/*
			 * If this is a NOBITS output section convert all of
			 * its associated input sections into real, null filled,
			 * data buffers, and change the section to PROGBITS.
			 */
			if (shdr->sh_type == SHT_NOBITS)
				shdr->sh_type = SHT_PROGBITS;
		}

		/*
		 * If this is a loadable segment retain the last output section
		 * descriptor.  This acts both as a flag that a loadable
		 * segment has been seen, and as the segment to which a padding
		 * buffer will be added.
		 */
		if (phdr->p_type == PT_LOAD)
			oscn =	osp->os_scn;
	}
	return (1);
}


/*
 * Create the elf structures that allow the input data to be associated with the
 * new image:
 *
 *	o	define the new elf image using elf_begin(),
 *
 *	o	obtain an elf header for the image,
 *
 *	o	traverse the input segments and create a program header array
 *		to define the required segments,
 *
 *	o 	traverse the output sections for each segment assigning a new
 *		section descriptor and section header for each,
 *
 *	o	traverse the input sections associated with each output section
 *		and assign a new data descriptor to each (each output section
 *		becomes a linked list of input data buffers).
 */
uintptr_t
create_outfile(Ofl_desc * ofl)
{
	Listnode *	lnp1, * lnp2, * lnp3;
	Sg_desc *	sgp;
	Os_desc *	osp;
	Is_desc *	isp;
	Elf_Scn	*	scn;
	Shdr *		shdr;
	int		nseg = 0;
	Word		ptype;
	Word		flags = ofl->ofl_flags;
	size_t		ndx = 0;
	Elf_Cmd		cmd;
	int		fd;

	if (flags & FLG_OF_MEMORY) {
		cmd = ELF_C_IMAGE;
		fd = 0;
	} else {
		fd = ofl->ofl_fd;
		cmd = ELF_C_WRITE;
	}

	/*
	 * If there are any ordered section,
	 * handle them here.
	 */
	if (ofl->ofl_ordered.head != NULL) {
		sort_ordered(ofl);
	}

	/*
	 * Tell the access library about our new temporary file.
	 */
	if ((ofl->ofl_welf = elf_begin(fd, cmd, 0)) == NULL) {
		eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_BEGIN), ofl->ofl_name);
		return (S_ERROR);
	}

	/*
	 * Obtain a new Elf header.
	 */
	if (elf_newehdr(ofl->ofl_welf) == NULL) {
		eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_NEWEHDR), ofl->ofl_name);
		return (S_ERROR);
	}

	for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp)) {
		/*
		 * Count the number of segments that will go in the program
		 * header table. If a segment is empty, ignore it.
		 */
		int	frst = 0;
		Phdr *	phdr = &(sgp->sg_phdr);

		ptype = phdr->p_type;

		if (!(flags & FLG_OF_RELOBJ)) {
			if (ptype == PT_PHDR) {
				/*
				 * If we are generating an interp section
				 * (and thus an associated PT_INTERP program
				 * header entry) also generate a PT_PHDR
				 * program header entry.  This allows
				 * the kernel to generate the appropriate
				 * aux vector entries to pass to the
				 * interpreter (refer to exec/elf/elf.c;
				 * Note that if an image was generated
				 * with an interp section but no associated
				 * PT_PHDR program header entry the kernel
				 * will simply pass the interpreter an
				 * open file descriptor when the image is
				 * executed).
				 */
				if (ofl->ofl_osinterp)
					nseg++;
			} else if (ptype == PT_INTERP) {
				if (ofl->ofl_osinterp)
					nseg++;
			} else if (ptype == PT_DYNAMIC) {
				if (flags & FLG_OF_DYNAMIC)
					nseg++;
#if	defined(ELF_TARGET_IA64)
			} else if (ptype == PT_IA_64_UNWIND) {
				if (ofl->ofl_osunwind &&
				    !(flags & FLG_OF_RELOBJ))
					nseg++;
#endif	/* ELF_TARGET_IA64 */
			} else if (ptype == PT_SUNWBSS) {
				if (ofl->ofl_issunwbss)
					nseg++;
			} else if ((sgp->sg_osdescs.head) ||
			    (sgp->sg_flags & FLG_SG_EMPTY)) {
				if ((ptype != PT_NULL) &&
				    !(flags & FLG_OF_RELOBJ))
					nseg++;
			}
		}

		for (LIST_TRAVERSE(&(sgp->sg_osdescs), lnp2, osp)) {
			/*
			 * Get a section descriptor for the section.
			 */
			if ((scn = elf_newscn(ofl->ofl_welf)) == NULL) {
				eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_NEWSCN),
				    ofl->ofl_name);
				return (S_ERROR);
			}
			osp->os_scn = scn;

			/*
			 * Get a new section header table entry and copy the
			 * pertinent information from the in-core descriptor.
			 * As we had originally allocated the section header
			 * (refer place_section()) we might as well free it up.
			 */
			if ((shdr = elf_getshdr(scn)) == NULL) {
				eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_GETSHDR),
				    ofl->ofl_name);
				return (S_ERROR);
			}
			*shdr = *(osp->os_shdr);
			osp->os_shdr = shdr;

			/*
			 * Knock off the SHF_ORDERED flag.
			 */
			osp->os_shdr->sh_flags &= ~SHF_ORDERED;

			for (LIST_TRAVERSE(&(osp->os_isdescs), lnp3, isp)) {
				Elf_Data *	data;

				/*
				 * Create new output data buffers for each of
				 * the input data buffers, thus linking the new
				 * buffers to the new elf output structures.
				 * Simply make the new data buffers point to
				 * the old data.
				 */
				if ((data = elf_newdata(scn)) == NULL) {
					eprintf(ERR_ELF,
					    MSG_ORIG(MSG_ELF_NEWDATA),
					    ofl->ofl_name);
					return (S_ERROR);
				}
				*data = *(isp->is_indata);
				isp->is_indata = data;

#if	defined(_ELF64) && defined(_ILP32)
				/*
				 * 4106312, the 32-bit ELF64 version of ld
				 * needs to be able to create large .bss
				 * sections.  The d_size member of Elf_Data
				 * only allows 32-bits in _ILP32, so we build
				 * multiple data-items that each fit into 32-
				 * bits.  libelf (4106398) can summ these up
				 * into a 64-bit quantity.  This only works
				 * for NOBITS sections which don't have any
				 * real data to maintain and don't require
				 * large file support.
				 */
				if (isp->is_shdr->sh_type == SHT_NOBITS) {
					Xword sz = isp->is_shdr->sh_size;

					while (sz >> 32) {
						data->d_size = SIZE_MAX;
						sz -= (Xword)SIZE_MAX;
						data = elf_newdata(scn);
					}
					data->d_size = (size_t)sz;
				}
#endif

				/*
				 * If this segment requires rounding realign the
				 * first data buffer associated with the first
				 * section.
				 */
				if ((frst++ == 0) &&
				    (sgp->sg_flags & FLG_SG_ROUND)) {
					Xword    align;

					if (data->d_align)
						align = (Xword)
						    S_ROUND(data->d_align,
						    sgp->sg_round);
					else
						align = sgp->sg_round;

					data->d_align = (size_t)align;
				}
			}
			/*
			 * clear out the szoutrels counter so that
			 * it can be used again in the building of relocs.
			 *
			 * see machrel.c
			 */
			osp->os_szoutrels = 0;
		}
	}

	/*
	 * Build an empty PHDR.
	 */
	if (nseg) {
		if ((ofl->ofl_phdr = elf_newphdr(ofl->ofl_welf,
		    nseg)) == NULL) {
			eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_NEWPHDR),
			    ofl->ofl_name);
			return (S_ERROR);
		}
	}

	/*
	 * If we need to generate a memory model, pad the image.
	 */
	if (flags & FLG_OF_MEMORY) {
		if (pad_outfile(ofl) == S_ERROR)
			return (S_ERROR);
	}

	/*
	 * After all the basic input file processing, all data pointers are
	 * referencing two types of memory:
	 *
	 *	o	allocated memory, ie. elf structures, internal link
	 *		editor structures, and any new sections that have been
	 *		created.
	 *
	 *	o	original input file mmap'ed memory, ie. the actual data
	 *		sections of the input file images.
	 *
	 * Up until now, the only memory modifications have been carried out on
	 * the allocated memory.  Before carrying out any relocations, write the
	 * new output file image and reassign any necessary data pointers to the
	 * output files memory image.  This insures that any relocation
	 * modifications are made to the output file image and not to the input
	 * file image, thus preventing the creation of dirty pages and reducing
	 * the overall swap space requirement.
	 *
	 * Write out the elf structure so as to create the new file image.
	 */
	if ((ofl->ofl_size = (size_t)elf_update(ofl->ofl_welf,
	    ELF_C_WRIMAGE)) == (size_t)-1) {
		eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_UPDATE), ofl->ofl_name);
		return (S_ERROR);
	}

	/*
	 * Initialize the true `ofl' information with the memory images address
	 * and size.  This will be used to write() out the image once any
	 * relocation processing has been completed.  We also use this image
	 * information to setup a new Elf descriptor, which is used to obtain
	 * all the necessary elf pointers within the new output image.
	 */
	if ((ofl->ofl_elf = elf_begin(0, ELF_C_IMAGE,
	    ofl->ofl_welf)) == NULL) {
		eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_BEGIN), ofl->ofl_name);
		return (S_ERROR);
	}
	if ((ofl->ofl_ehdr = elf_getehdr(ofl->ofl_elf)) == NULL) {
		eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_GETEHDR), ofl->ofl_name);
		return (S_ERROR);
	}
	if (!(flags & FLG_OF_RELOBJ))
		if ((ofl->ofl_phdr = elf_getphdr(ofl->ofl_elf)) == NULL) {
			eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_GETPHDR),
			    ofl->ofl_name);
			return (S_ERROR);
		}

	/*
	 * Reinitialize the section descriptors, section headers and obtain new
	 * output data buffer pointers (these will be used to perform any
	 * relocations).
	 */
	for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp)) {
		Phdr *	_phdr = &(sgp->sg_phdr);

		for (LIST_TRAVERSE(&(sgp->sg_osdescs), lnp2, osp)) {
			if ((osp->os_scn = elf_getscn(ofl->ofl_elf, ++ndx)) ==
			    NULL) {
				eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_GETSCN),
				    ofl->ofl_name);
				return (S_ERROR);
			}
			if ((osp->os_shdr = elf_getshdr(osp->os_scn)) ==
			    NULL) {
				eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_GETSHDR),
				    ofl->ofl_name);
				return (S_ERROR);
			}
			if ((osp->os_outdata =
			    elf_getdata(osp->os_scn, NULL)) == NULL) {
				eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_GETDATA),
				    ofl->ofl_name);
				return (S_ERROR);
			}

			/*
			 * If this section is part of a loadable segment insure
			 * that the segments alignment is appropriate.
			 */
			if (_phdr->p_type == PT_LOAD) {
				_phdr->p_align = (Xword)lcm(_phdr->p_align,
				    osp->os_shdr->sh_addralign);
			}
		}
	}
	return (1);
}
