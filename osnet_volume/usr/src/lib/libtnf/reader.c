/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma	ident	"@(#)reader.c	1.15	95/03/20 SMI"

#include "libtnf.h"

/*
 * Initiate a reader session
 */

tnf_errcode_t
tnf_reader_begin(caddr_t base, size_t size, TNF **tnfret)
{
	tnf_uint32_t 	magic;
	int 		native;
	TNF 		*tnf;
	tnf_ref32_t	*fhdr, *bhdr;
	size_t		tmpsz;
	caddr_t		p, genp, bvp;
	tnf_errcode_t	err;

	/*
	 * Check magic number
	 */

	/* LINTED pointer cast may result in improper alignment */
	if ((magic = *(tnf_uint32_t *)base) == TNF_MAGIC)
		native = 1;	/* same endian */
	else if (magic == TNF_MAGIC_1)
		native = 0;	/* other endian */
	else
		return (TNF_ERR_NOTTNF);

	/*
	 * Allocate TNF struct, initialize members
	 */

	if ((tnf = (TNF*)calloc(1, sizeof (*tnf))) == (TNF*)NULL)
		return (TNF_ERR_ALLOCFAIL);

	tnf->file_magic 	= magic;
	tnf->file_native 	= native;
	tnf->file_start 	= base;
	tnf->file_size 		= size;
	tnf->file_end 		= base + size;

	/*
	 * Examine file header
	 */

	/* LINTED pointer cast may result in improper alignment */
	fhdr = (tnf_ref32_t *)(base + sizeof (magic)); /* second word */
	tnf->file_header = fhdr;

	/* Block size */
	p = _tnf_get_slot_named(tnf, fhdr, TNF_N_BLOCK_SIZE);
	/* LINTED pointer cast may result in improper alignment */
	tnf->block_size	= _GET_UINT32(tnf, (tnf_uint32_t *)p);

	/* Directory size */
	p = _tnf_get_slot_named(tnf, fhdr, TNF_N_DIRECTORY_SIZE);
	/* LINTED pointer cast may result in improper alignment */
	tnf->directory_size = _GET_UINT32(tnf, (tnf_uint32_t *)p);

	/* Block count */
	p = _tnf_get_slot_named(tnf, fhdr, TNF_N_BLOCK_COUNT);
	/* LINTED pointer cast may result in improper alignment */
	tnf->block_count = _GET_UINT32(tnf, (tnf_uint32_t *)p);
	/*
	 * This member tracks data block count, not total block count
	 * (unlike the TNF file header).   Discount directory blocks.
	 */
	tnf->block_count -= tnf->directory_size / tnf->block_size;

	/*
	 * 1196886: Clients may supply file_size information obtained
	 * by fstat() which is incorrect.  Check it now and revise
	 * downwards if we have to.
	 */
	tmpsz = tnf->directory_size + tnf->block_count * tnf->block_size;
	if (tmpsz != size) {
		if (tmpsz > size)
			/* missing data? */
			return (TNF_ERR_BADTNF);
		else {
			tnf->file_size = tmpsz;
			tnf->file_end = base + tmpsz;
		}
	}

	/* Calculate block shift */
	tmpsz = 1;
	while (tmpsz != tnf->block_size) {
		tmpsz <<= 1;
		tnf->block_shift++;
	}

	/* Calculate block mask */
	tnf->block_mask = ~(tnf->block_size - 1);

	/* Generation shift */
	p = _tnf_get_slot_named(tnf, fhdr, TNF_N_FILE_LOGICAL_SIZE);
	/* LINTED pointer cast may result in improper alignment */
	tnf->generation_shift	= _GET_UINT32(tnf, (tnf_uint32_t *)p);

	/* Calculate the address mask */
	/*
	 * Following lint complaint is unwarranted, probably an
	 * uninitialized variable in lint or something ...
	 */
	/* LINTED constant truncated by assignment */
	tnf->address_mask = 0xffffffff;
	tnf->address_mask <<= tnf->generation_shift;
	tnf->address_mask = ~(tnf->address_mask);


	/*
	 * Examine first block header in data area
	 */

	tnf->data_start = tnf->file_start + tnf->directory_size;
	/* LINTED pointer cast may result in improper alignment */
	bhdr = (tnf_ref32_t *)tnf->data_start;

	/* Block generation offset */
	genp = _tnf_get_slot_named(tnf, bhdr, TNF_N_GENERATION);
	tnf->block_generation_offset = genp - (caddr_t)bhdr;

	/* Block bytes valid offset */
	bvp = _tnf_get_slot_named(tnf, bhdr, TNF_N_BYTES_VALID);
	tnf->block_bytes_valid_offset = bvp - (caddr_t)bhdr;

	/*
	 * Bootstrap taginfo system and cache important taginfo
	 */

	if ((err = _tnf_init_tags(tnf)) != TNF_ERR_NONE)
		return (err);

	tnf->file_header_info 	= _tnf_get_info(tnf, _tnf_get_tag(tnf, fhdr));
	tnf->block_header_info 	= _tnf_get_info(tnf, _tnf_get_tag(tnf, bhdr));

	/*
	 * Return TNF handle and error status
	 */

	*tnfret = tnf;
	return (TNF_ERR_NONE);
}

/*
 * Terminate a reader session
 */

tnf_errcode_t
tnf_reader_end(TNF *tnf)
{
	tnf_errcode_t	err;

	/* Deallocate all taginfo */
	if ((err = _tnf_fini_tags(tnf)) != TNF_ERR_NONE)
		return (err);

	/* Deallocate TNF */
	free(tnf);

	return (TNF_ERR_NONE);
}
