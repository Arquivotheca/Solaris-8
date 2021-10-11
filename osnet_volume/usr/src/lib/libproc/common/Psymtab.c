/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Psymtab.c	1.5	99/09/06 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <limits.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/systeminfo.h>

#include "Pcontrol.h"
#include "Putil.h"

static file_info_t *build_map_symtab(struct ps_prochandle *, map_info_t *);
static map_info_t *exec_map(struct ps_prochandle *);
static map_info_t *object_to_map(struct ps_prochandle *, const char *);
static map_info_t *object_name_to_map(struct ps_prochandle *, const char *);
static GElf_Sym *sym_by_name(sym_tbl_t *, const char *, GElf_Sym *);

/*
 * Allocation function for a new file_info_t
 */
static file_info_t *
file_info_new(struct ps_prochandle *P, map_info_t *mptr)
{
	file_info_t *fptr = malloc(sizeof (file_info_t));
	map_info_t *mp;
	map_info_t *mp_prev;
	int i;

	if (fptr == NULL)
		return (NULL);

	(void) memset(fptr, 0, sizeof (file_info_t));
	list_link(fptr, &P->file_head);
	(void) strcpy(fptr->file_pname, mptr->map_pmap.pr_mapname);
	mptr->map_file = fptr;
	fptr->file_ref = 1;
	fptr->file_fd = -1;
	P->num_files++;

	/*
	 * Attach the new file info struct to all matching maps.
	 */
	for (i = 0, mp = list_next(&P->map_head), mp_prev = NULL;
	    i < P->num_mappings; i++, mp = list_next(mp)) {
		/*
		 * If we have a match, attach the file_info to the mapping and
		 * increment the reference count.  If this is a data mapping,
		 * save a pointer to it in mp_prev for the next iteration.
		 */
		if (mp->map_pmap.pr_mapname[0] != '\0' &&
		    mp->map_file == NULL &&
		    strcmp(fptr->file_pname, mp->map_pmap.pr_mapname) == 0) {
			mp->map_file = fptr;
			fptr->file_ref++;

			if ((mp->map_pmap.pr_mflags & (MA_READ | MA_WRITE)) ==
			    (MA_READ | MA_WRITE))
				mp_prev = mp;
		}

		/*
		 * An anon mapping immediately adjacent to the previously noted
		 * object data mapping is also part of the data space (the BSS).
		 */
		if (mp->map_file == NULL && mp_prev != NULL &&
		    mp->map_pmap.pr_mapname[0] == '\0' &&
		    (mp->map_pmap.pr_mflags & (MA_ANON | MA_READ | MA_WRITE)) ==
		    (MA_ANON | MA_READ | MA_WRITE) && mp->map_pmap.pr_vaddr ==
		    mp_prev->map_pmap.pr_vaddr + mp_prev->map_pmap.pr_size) {
			mp->map_file = fptr;
			fptr->file_ref++;
		}
	}

	return (fptr);
}

/*
 * Deallocation function for a file_info_t
 */
static void
file_info_free(struct ps_prochandle *P, file_info_t *fptr)
{
	if (--fptr->file_ref == 0) {
		list_unlink(fptr);
		if (fptr->file_lo)
			free(fptr->file_lo);
		if (fptr->file_lname)
			free(fptr->file_lname);
		if (fptr->file_elf)
			(void) elf_end(fptr->file_elf);
		if (fptr->file_fd >= 0)
			(void) close(fptr->file_fd);
		free(fptr);
		P->num_files--;
	}
}

/*
 * Allocation function for a new map_info_t
 */
static map_info_t *
map_info_new(struct ps_prochandle *P, prmap_t *pmap)
{
	map_info_t *mptr = malloc(sizeof (map_info_t));

	if (mptr == NULL)
		return (NULL);

	(void) memset(mptr, 0, sizeof (map_info_t));
	mptr->map_pmap = *pmap;
	P->num_mappings++;
	return (mptr);
}

/*
 * Deallocation function for a map_info_t
 */
static void
map_info_free(struct ps_prochandle *P, map_info_t *mptr)
{
	file_info_t *fptr;

	list_unlink(mptr);
	if ((fptr = mptr->map_file) != NULL) {
		if (fptr->file_map == mptr)
			fptr->file_map = NULL;
		file_info_free(P, fptr);
	}
	if (P->execname && mptr == P->map_exec) {
		free(P->execname);
		P->execname = NULL;
	}
	if (P->auxv && (mptr == P->map_exec || mptr == P->map_ldso)) {
		free(P->auxv);
		P->auxv = NULL;
	}
	if (mptr == P->map_exec)
		P->map_exec = NULL;
	if (mptr == P->map_ldso)
		P->map_ldso = NULL;
	free(mptr);
	P->num_mappings--;
}

/*
 * Call-back function for librtld_db to iterate through all of its shared
 * libraries.  We use this to get the load object names for the mappings.
 */
static int
map_iter(const rd_loadobj_t *lop, void *cd)
{
	char buf[PATH_MAX];
	struct ps_prochandle *P = cd;
	map_info_t *mptr;
	file_info_t *fptr;

	if ((mptr = Paddr2mptr(P, lop->rl_base)) == NULL)
		return (1); /* Base address does not match any mapping */

	if ((fptr = mptr->map_file) == NULL &&
	    (fptr = file_info_new(P, mptr)) == NULL)
		return (1); /* Failed to allocate a new file_info_t */

	if ((fptr->file_lo == NULL) &&
	    (fptr->file_lo = malloc(sizeof (rd_loadobj_t))) == NULL) {
		file_info_free(P, fptr);
		return (1); /* Failed to allocate rd_loadobj_t */
	}

	fptr->file_map = mptr;
	*fptr->file_lo = *lop;

	if (fptr->file_lname) {
		free(fptr->file_lname);
		fptr->file_lname = NULL;
	}

	if (Pread_string(P, buf, sizeof (buf), lop->rl_nameaddr) > 0) {
		if ((fptr->file_lname = strdup(buf)) != NULL)
			fptr->file_lbase = basename(fptr->file_lname);
	}

	return (1);
}

static void
map_set(struct ps_prochandle *P, map_info_t *mptr, const char *lname)
{
	file_info_t *fptr;

	if ((fptr = mptr->map_file) == NULL &&
	    (fptr = file_info_new(P, mptr)) == NULL)
		return; /* Failed to allocate a new file_info_t */

	fptr->file_map = mptr;

	if ((fptr->file_lo == NULL) &&
	    (fptr->file_lo = malloc(sizeof (rd_loadobj_t))) == NULL) {
		file_info_free(P, fptr);
		return; /* Failed to allocate rd_loadobj_t */
	}

	(void) memset(fptr->file_lo, 0, sizeof (rd_loadobj_t));
	fptr->file_lo->rl_base = mptr->map_pmap.pr_vaddr;
	fptr->file_lo->rl_bend =
		mptr->map_pmap.pr_vaddr + mptr->map_pmap.pr_size;

	if (fptr->file_lname) {
		free(fptr->file_lname);
		fptr->file_lname = NULL;
	}

	if ((fptr->file_lname = strdup(lname)) != NULL)
		fptr->file_lbase = basename(fptr->file_lname);
}

static void
load_static_maps(struct ps_prochandle *P)
{
	map_info_t *mptr;

	if (Pgetauxval(P, AT_BASE) == -1L) {
		/*
		 * The dynamic linker does not exist for this process.
		 * Just construct the map for the a.out.
		 */
		if ((mptr = object_name_to_map(P, PR_OBJ_EXEC)) != NULL)
			map_set(P, mptr, "a.out");
	} else {
		/*
		 * The dynamic linker does exist for this process.
		 * Construct the map for it now; we will construct
		 * the a.out (and other) maps when ld.so.1 is finished.
		 */
		if ((mptr = object_name_to_map(P, PR_OBJ_LDSO)) != NULL)
			map_set(P, mptr, "ld.so.1");
	}
}

/*
 * Go through all the address space mappings, validating or updating
 * the information already gathered, or gathering new information.
 */
void
Pupdate_maps(struct ps_prochandle *P)
{
	char mapfile[64];
	int mapfd;
	struct stat statb;
	prmap_t *Pmap = NULL;
	prmap_t *pmap;
	ssize_t nmap;
	int i;
	map_info_t *mptr;
	map_info_t *nextptr;
	map_info_t *newmptr;
	uint_t oldmapcnt;
	int anychange = FALSE;

	if (P->info_valid)
		return;

	Preadauxvec(P);

	(void) sprintf(mapfile, "/proc/%d/map", (int)P->pid);
	if ((mapfd = open(mapfile, O_RDONLY)) < 0 ||
	    fstat(mapfd, &statb) != 0 ||
	    statb.st_size < sizeof (prmap_t) ||
	    (Pmap = malloc(statb.st_size)) == NULL ||
	    (nmap = pread(mapfd, Pmap, statb.st_size, 0L)) <= 0 ||
	    (nmap /= sizeof (prmap_t)) == 0) {
		if (Pmap != NULL)
			free(Pmap);
		if (mapfd >= 0)
			(void) close(mapfd);
		Preset_maps(P);	/* utter failure; destroy tables */
		return;
	}
	(void) close(mapfd);

	/*
	 * Merge the new mappings with the old mapping list.
	 * The map elements are maintained in order, sorted by address.
	 * /proc/<pid>/map returns the maps sorted by address.
	 */
	mptr = list_next(&P->map_head);
	oldmapcnt = P->num_mappings;
	for (i = 0, pmap = Pmap; i < nmap; i++, pmap++) {
		if (oldmapcnt == 0) {	/* we ran out of old mappings */
			if (pmap->pr_mapname[0] != '\0')
				anychange = TRUE;
			if ((newmptr = map_info_new(P, pmap)) != NULL)
				list_link(newmptr, &P->map_head);
			continue;
		}
		if (pmap->pr_vaddr == mptr->map_pmap.pr_vaddr &&
		    pmap->pr_size == mptr->map_pmap.pr_size &&
		    pmap->pr_offset == mptr->map_pmap.pr_offset &&
		    (pmap->pr_mflags & ~(MA_BREAK | MA_STACK)) ==
		    (mptr->map_pmap.pr_mflags & ~(MA_BREAK | MA_STACK)) &&
		    pmap->pr_pagesize == mptr->map_pmap.pr_pagesize &&
		    pmap->pr_shmid == mptr->map_pmap.pr_shmid &&
		    strcmp(pmap->pr_mapname, mptr->map_pmap.pr_mapname) == 0) {
			/*
			 * If the map elements match, no change needed.  Note
			 * that we do not consider the unreliable MA_BREAK and
			 * MA_STACK flags when we make our comparison, but we
			 * must always copy the latest flags into our private
			 * copy of the prmap_t structure.
			 */
			mptr->map_pmap.pr_mflags = pmap->pr_mflags;
			mptr = list_next(mptr);
			oldmapcnt--;
			continue;
		}
		if (pmap->pr_vaddr + pmap->pr_size > mptr->map_pmap.pr_vaddr) {
			if (mptr->map_pmap.pr_mapname[0] != '\0')
				anychange = TRUE;
			nextptr = list_next(mptr);
			map_info_free(P, mptr);
			mptr = nextptr;
			oldmapcnt--;
			i--;
			pmap--;
			continue;
		}
		if (pmap->pr_mapname[0] != '\0')
			anychange = TRUE;
		if ((newmptr = map_info_new(P, pmap)) != NULL)
			list_link(newmptr, mptr);
	}

	while (oldmapcnt) {
		if (mptr->map_pmap.pr_mapname[0] != '\0')
			anychange = TRUE;
		nextptr = list_next(mptr);
		map_info_free(P, mptr);
		mptr = nextptr;
		oldmapcnt--;
	}

	free(Pmap);
	P->info_valid = 1;

	/*
	 * Consult librtld_db to get the load object
	 * names for all of the shared libraries.
	 */
	if (anychange && P->rap != NULL)
		(void) rd_loadobj_iter(P->rap, map_iter, P);
}

/*
 * Return the librtld_db agent handle for the victim process.
 * The handle will become invalid at the next successful exec() and the
 * client (caller of proc_rd_agent()) must not use it beyond that point.
 * If the process is already dead, we've already tried our best to
 * create the agent during core file initialization.
 */
rd_agent_t *
Prd_agent(struct ps_prochandle *P)
{
	if (P->rap == NULL && P->state != PS_DEAD) {
		Pupdate_maps(P);
		if (P->num_files == 0)
			load_static_maps(P);
		if ((P->rap = rd_new(P)) != NULL)
			(void) rd_loadobj_iter(P->rap, map_iter, P);
	}
	return (P->rap);
}

/*
 * Return the prmap_t structure containing 'addr', but only if it
 * is in the dynamic linker's link map and is the text section.
 */
const prmap_t *
Paddr_to_text_map(struct ps_prochandle *P, uintptr_t addr)
{
	map_info_t *mptr;

	if (P->num_mappings == 0)
		Pupdate_maps(P);

	if ((mptr = Paddr2mptr(P, addr)) != NULL) {
		file_info_t *fptr = build_map_symtab(P, mptr);
		const prmap_t *pmp = &mptr->map_pmap;

		if (fptr != NULL && fptr->file_lo != NULL &&
		    fptr->file_lo->rl_base >= pmp->pr_vaddr &&
		    fptr->file_lo->rl_base < pmp->pr_vaddr + pmp->pr_size)
			return (pmp);
	}

	return (NULL);
}

/*
 * Return the prmap_t structure containing 'addr' (no restrictions on
 * the type of mapping).
 */
const prmap_t *
Paddr_to_map(struct ps_prochandle *P, uintptr_t addr)
{
	map_info_t *mptr;

	if (P->num_mappings == 0)
		Pupdate_maps(P);

	if ((mptr = Paddr2mptr(P, addr)) != NULL)
		return (&mptr->map_pmap);

	return (NULL);
}

/*
 * Convert a full or partial load object name to the prmap_t for its
 * corresponding primary text mapping.
 */
const prmap_t *
Pname_to_map(struct ps_prochandle *P, const char *name)
{
	map_info_t *mptr;

	if (name == PR_OBJ_EVERY)
		return (NULL); /* A reasonable mistake */

	if ((mptr = object_name_to_map(P, name)) != NULL)
		return (&mptr->map_pmap);

	return (NULL);
}

/*
 * If we're not a core file, re-read the /proc/<pid>/auxv file and store
 * its contents in P->auxv.  In the case of a core file, we either
 * initialized P->auxv in Pcore() from the NT_AUXV, or we don't have an
 * auxv because the note was missing.
 */
void
Preadauxvec(struct ps_prochandle *P)
{
	char auxfile[64];
	struct stat statb;
	ssize_t naux;
	int fd;

	if (P->state == PS_DEAD)
		return; /* Already read during Pgrab_core() */

	if (P->auxv != NULL) {
		free(P->auxv);
		P->auxv = NULL;
	}

	(void) sprintf(auxfile, "/proc/%d/auxv", (int)P->pid);

	if ((fd = open(auxfile, O_RDONLY)) < 0 ||
	    fstat(fd, &statb) != 0 ||
	    statb.st_size < sizeof (auxv_t) ||
	    ((P->auxv = malloc(statb.st_size + sizeof (auxv_t))) == NULL) ||
	    (naux = read(fd, P->auxv, statb.st_size)) < 0 ||
	    (naux /= sizeof (auxv_t)) < 1) {
		if (P->auxv)
			free(P->auxv);
		P->auxv = malloc(sizeof (auxv_t));
		naux = 0;
	}

	if (fd >= 0)
		(void) close(fd);
	if (P->auxv != NULL)
		(void) memset(&P->auxv[naux], 0, sizeof (auxv_t));
}

/*
 * Return a requested element from the process's aux vector.
 * Return -1 on failure (this is adequate for our purposes).
 */
long
Pgetauxval(struct ps_prochandle *P, int type)
{
	auxv_t *auxv;

	if (P->auxv == NULL)
		Preadauxvec(P);

	if (P->auxv == NULL)
		return (-1);

	for (auxv = P->auxv; auxv->a_type != AT_NULL; auxv++) {
		if (auxv->a_type == type)
			return (auxv->a_un.a_val);
	}

	return (-1);
}

/*
 * Find or build the symbol table for the given mapping.
 */
static file_info_t *
build_map_symtab(struct ps_prochandle *P, map_info_t *mptr)
{
	file_info_t *fptr;
	uint_t i;

	if ((fptr = mptr->map_file) != NULL) {
		if (!fptr->file_init)
			Pbuild_file_symtab(P, fptr);
		return (fptr);
	}

	if (mptr->map_pmap.pr_mapname[0] == '\0')
		return (NULL);

	/*
	 * Attempt to find a matching file.
	 * (A file can be mapped at several different addresses.)
	 */
	for (i = 0, fptr = list_next(&P->file_head); i < P->num_files;
	    i++, fptr = list_next(fptr)) {
		if (strcmp(fptr->file_pname, mptr->map_pmap.pr_mapname) == 0) {
			mptr->map_file = fptr;
			fptr->file_ref++;
			return (fptr);
		}
	}

	/*
	 * If we need to create a new file_info structure, iterate
	 * through the load objects in order to attempt to connect
	 * this new file with its primary text mapping.  We again
	 * need to handle ld.so as a special case because we need
	 * to be able to bootstrap librtld_db.
	 */
	if ((fptr = file_info_new(P, mptr)) == NULL)
		return (NULL);

	if (P->map_ldso != mptr) {
		if (P->rap != NULL)
			(void) rd_loadobj_iter(P->rap, map_iter, P);
		else
			(void) Prd_agent(P);
	} else {
		fptr->file_map = mptr;
	}

	/*
	 * If librtld_db wasn't able to help us connect the file to a primary
	 * text mapping, set file_map to the current mapping because we require
	 * fptr->file_map to be set in Pbuild_file_symtab.  librtld_db may be
	 * unaware of what's going on in the rare case that a legitimate ELF
	 * file has been mmap(2)ed into the process address space *without*
	 * the use of dlopen(3x).  Why would this happen?  See pwdx ... :)
	 */
	if (fptr->file_map == NULL)
		fptr->file_map = mptr;

	if (!fptr->file_init)
		Pbuild_file_symtab(P, fptr);

	return (fptr);
}

/*
 * Build the symbol table for the given mapped file.
 */
void
Pbuild_file_symtab(struct ps_prochandle *P, file_info_t *fptr)
{
	char objectfile[PATH_MAX];
	uint_t i;

	GElf_Ehdr ehdr;
	GElf_Sym s;

	Elf_Data *shdata;
	Elf_Scn *scn;
	Elf *elf;

	struct {
		GElf_Shdr c_shdr;
		Elf_Data *c_data;
		const char *c_name;
	} *cp, *cache = NULL, *dyn = NULL, *plt = NULL;

	if (fptr->file_init)
		return;	/* We've already processed this file */

	/*
	 * Mark the file_info struct as having the symbol table initialized
	 * even if we fail below.  We tried once; we don't try again.
	 */
	fptr->file_init = 1;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		dprintf("libproc ELF version is more recent than libelf\n");
		return;
	}

	if (P->state == PS_DEAD) {
		/*
		 * If we're a core file, we can't open files from the /proc
		 * object directory; we have only the mapping and file names
		 * to guide us.  We prefer the file_lname, but need to handle
		 * the case of it being NULL in order to bootstrap: we first
		 * come here during rd_new() when the only information we have
		 * is interpreter name associated with the AT_BASE mapping.
		 */
		(void) snprintf(objectfile, sizeof (objectfile), "%s",
		    fptr->file_lname ? fptr->file_lname : fptr->file_pname);
	} else {
		(void) snprintf(objectfile, sizeof (objectfile),
		    "/proc/%d/object/%s", (int)P->pid, fptr->file_pname);
	}

	if ((fptr->file_fd = open(objectfile, O_RDONLY)) < 0) {
		dprintf("Pbuild_file_symtab: failed to open %s: %s\n",
		    objectfile, strerror(errno));
		return;
	}

	/*
	 * Open the elf file, and then get the ehdr and .shstrtab data buffer
	 * so we can process sections by name.
	 */
	if ((elf = elf_begin(fptr->file_fd, ELF_C_READ, NULL)) == NULL ||
	    (elf_kind(elf) != ELF_K_ELF || gelf_getehdr(elf, &ehdr) == NULL) ||
	    (scn = elf_getscn(elf, ehdr.e_shstrndx)) == NULL ||
	    (shdata = elf_getdata(scn, NULL)) == NULL) {
		dprintf("failed to process ELF file %s: %s\n",
		    objectfile, elf_errmsg(elf_errno()));
		goto bad;
	}

	if ((cache = malloc(ehdr.e_shnum * sizeof (*cache))) == NULL) {
		dprintf("failed to malloc section cache for %s\n", objectfile);
		goto bad;
	}

	dprintf("processing ELF file %s\n", objectfile);
	fptr->file_class = ehdr.e_ident[EI_CLASS];
	fptr->file_etype = ehdr.e_type;
	fptr->file_elf = elf;

	/*
	 * Iterate through each section, caching its section header, data
	 * pointer, and name.  We use this for handling sh_link values below.
	 */
	for (cp = cache + 1, scn = NULL; scn = elf_nextscn(elf, scn); cp++) {
		if (gelf_getshdr(scn, &cp->c_shdr) == NULL)
			goto bad; /* Failed to get section header */

		if ((cp->c_data = elf_getdata(scn, NULL)) == NULL)
			goto bad; /* Failed to get section data */

		cp->c_name = (const char *)shdata->d_buf + cp->c_shdr.sh_name;
	}

	/*
	 * Now iterate through the section cache in order to locate info
	 * for the .symtab, .dynsym, .dynamic, and .plt sections:
	 */
	for (i = 1, cp = cache + 1; i < ehdr.e_shnum; i++, cp++) {
		GElf_Shdr *shp = &cp->c_shdr;

		if (shp->sh_type == SHT_SYMTAB || shp->sh_type == SHT_DYNSYM) {
			sym_tbl_t *symp = shp->sh_type == SHT_SYMTAB ?
			    &fptr->file_symtab : &fptr->file_dynsym;

			symp->sym_data = cp->c_data;
			symp->sym_symn = shp->sh_size / shp->sh_entsize;
			symp->sym_strs = cache[shp->sh_link].c_data->d_buf;

		} else if (shp->sh_type == SHT_DYNAMIC) {
			dyn = cp;

		} else if (strcmp(cp->c_name, ".plt") == 0)
			plt = cp;
	}

	/*
	 * Fill in the base address of the text mapping for shared libraries.
	 * This allows us to translate symbols before librtld_db is ready.
	 */
	if (fptr->file_etype == ET_DYN) {
		fptr->file_dyn_base = fptr->file_map->map_pmap.pr_vaddr -
		    fptr->file_map->map_pmap.pr_offset;
		dprintf("setting file_dyn_base for %s to %p\n",
		    objectfile, (void *)fptr->file_dyn_base);
	}

	if (fptr->file_lo == NULL)
		goto done; /* Nothing else to do if no load object info */

	/*
	 * If the object is a shared library and we have a different rl_base
	 * value, reset file_dyn_base according to librtld_db's information.
	 */
	if (fptr->file_etype == ET_DYN &&
	    fptr->file_lo->rl_base != fptr->file_dyn_base) {
		dprintf("resetting file_dyn_base for %s to %p\n",
		    objectfile, (void *)fptr->file_lo->rl_base);
		fptr->file_dyn_base = fptr->file_lo->rl_base;
	}

	/*
	 * Fill in the PLT information for this file if a PLT symbol is found.
	 */
	if (sym_by_name(&fptr->file_dynsym, "_PROCEDURE_LINKAGE_TABLE_", &s)) {
		fptr->file_plt_base = s.st_value + fptr->file_dyn_base;
		fptr->file_plt_size = (plt != NULL) ? plt->c_shdr.sh_size : 0;

		dprintf("PLT found at %p, size = %lu\n",
		    (void *)fptr->file_plt_base, (ulong_t)fptr->file_plt_size);
	}

	/*
	 * Fill in the _DYNAMIC information if a _DYNAMIC symbol is found.
	 */
	if (dyn != NULL && sym_by_name(&fptr->file_dynsym, "_DYNAMIC", &s)) {
		uintptr_t dynaddr = s.st_value + fptr->file_dyn_base;
		size_t ndyn = dyn->c_shdr.sh_size / dyn->c_shdr.sh_entsize;
		GElf_Dyn d;

		for (i = 0; i < ndyn; i++) {
			if (gelf_getdyn(dyn->c_data, i, &d) != NULL &&
			    d.d_tag == DT_JMPREL) {
				fptr->file_jmp_rel =
				    d.d_un.d_ptr + fptr->file_dyn_base;
				break;
			}
		}

		dprintf("_DYNAMIC found at %p, %lu entries, DT_JMPREL = %p\n",
		    (void *)dynaddr, (ulong_t)ndyn, (void *)fptr->file_jmp_rel);
	}

done:
	free(cache);
	return;

bad:
	if (cache != NULL)
		free(cache);

	(void) elf_end(elf);
	fptr->file_elf = NULL;
	(void) close(fptr->file_fd);
	fptr->file_fd = -1;
}

/*
 * Given a process virtual address, return the map_info_t containing it.
 * If none found, return NULL.
 */
map_info_t *
Paddr2mptr(struct ps_prochandle *P, uintptr_t addr)
{
	map_info_t *mptr = list_next(&P->map_head);
	uint_t i;

	for (i = 0; i < P->num_mappings; i++, mptr = list_next(mptr)) {
		if (addr >= mptr->map_pmap.pr_vaddr &&
		    addr < mptr->map_pmap.pr_vaddr + mptr->map_pmap.pr_size)
			return (mptr);
	}

	return (NULL);
}

/*
 * Return the map_info_t for the executable file.
 * If not found, return NULL.
 */
static map_info_t *
exec_map(struct ps_prochandle *P)
{
	uint_t i;
	map_info_t *mptr;
	map_info_t *mold = NULL;
	file_info_t *fptr;
	uintptr_t base;

	for (i = 0, mptr = list_next(&P->map_head); i < P->num_mappings;
	    i++, mptr = list_next(mptr)) {
		if (mptr->map_pmap.pr_mapname[0] == '\0')
			continue;
		if (strcmp(mptr->map_pmap.pr_mapname, "a.out") == 0) {
			if ((fptr = mptr->map_file) != NULL &&
			    fptr->file_lo != NULL) {
				base = fptr->file_lo->rl_base;
				if (base >= mptr->map_pmap.pr_vaddr &&
				    base < mptr->map_pmap.pr_vaddr +
				    mptr->map_pmap.pr_size)	/* text space */
					return (mptr);
				mold = mptr;	/* must be the data */
				continue;
			}
			/* This is a poor way to test for text space */
			if (!(mptr->map_pmap.pr_mflags & MA_EXEC) ||
			    (mptr->map_pmap.pr_mflags & MA_WRITE)) {
				mold = mptr;
				continue;
			}
			return (mptr);
		}
	}

	return (mold);
}

/*
 * Given a shared object name, return the map_info_t for it.  If no matching
 * object is found, return NULL.  Normally, the link maps contain the full
 * object pathname, e.g. /usr/lib/libc.so.1.  We allow the object name to
 * take one of the following forms:
 *
 * 1. An exact match (i.e. a full pathname): "/usr/lib/libc.so.1"
 * 2. An exact basename match: "libc.so.1"
 * 3. An initial basename match up to a '.' suffix: "libc.so" or "libc"
 * 4. The literal string "a.out" is an alias for the executable mapping
 *
 * The third case is convenient for callers, and also necessary in order to
 * support interaction with libthread_db, which calls into us with
 * TD_LIBTHREAD_NAME (defined to be the literal string "libthread.so").
 */
static map_info_t *
object_to_map(struct ps_prochandle *P, const char *objname)
{
	map_info_t *mp;
	file_info_t *fp;
	size_t objlen;
	uint_t i;

	/*
	 * First pass: look for exact matches of the entire pathname or
	 * basename (cases 1 and 2 above):
	 */
	for (i = 0, mp = list_next(&P->map_head); i < P->num_mappings;
	    i++, mp = list_next(mp)) {

		if (mp->map_pmap.pr_mapname[0] == '\0' ||
		    (fp = mp->map_file) == NULL ||
		    fp->file_lo == NULL || fp->file_lname == NULL)
			continue;

		/*
		 * If we match, return the primary text mapping; otherwise
		 * just return the mapping we matched.
		 */
		if (strcmp(fp->file_lname, objname) == 0 ||
		    strcmp(fp->file_lbase, objname) == 0)
			return (fp->file_map ? fp->file_map : mp);
	}

	objlen = strlen(objname);

	/*
	 * Second pass: look for partial matches (case 3 above):
	 */
	for (i = 0, mp = list_next(&P->map_head); i < P->num_mappings;
	    i++, mp = list_next(mp)) {

		if (mp->map_pmap.pr_mapname[0] == '\0' ||
		    (fp = mp->map_file) == NULL ||
		    fp->file_lo == NULL || fp->file_lname == NULL)
			continue;

		/*
		 * If we match, return the primary text mapping; otherwise
		 * just return the mapping we matched.
		 */
		if (strncmp(fp->file_lbase, objname, objlen) == 0 &&
		    fp->file_lbase[objlen] == '.')
			return (fp->file_map ? fp->file_map : mp);
	}

	/*
	 * One last check: we allow "a.out" to always alias the executable,
	 * assuming this name was not in use for something else.
	 */
	if (strcmp(objname, "a.out") == 0)
		return (P->map_exec);

	return (NULL);
}

static map_info_t *
object_name_to_map(struct ps_prochandle *P, const char *name)
{
	map_info_t *mptr;

	if (P->num_mappings == 0)
		Pupdate_maps(P);

	if (P->map_exec == NULL && ((mptr = Paddr2mptr(P,
	    Pgetauxval(P, AT_ENTRY))) != NULL || (mptr = exec_map(P)) != NULL))
		P->map_exec = mptr;

	if (P->map_ldso == NULL && (mptr = Paddr2mptr(P,
	    Pgetauxval(P, AT_BASE))) != NULL)
		P->map_ldso = mptr;

	if (name == PR_OBJ_EXEC)
		mptr = P->map_exec;
	else if (name == PR_OBJ_LDSO)
		mptr = P->map_ldso;
	else
		mptr = (Prd_agent(P) == NULL) ? NULL : object_to_map(P, name);

	return (mptr);
}

/*
 * When two symbols are found by address, decide which one is to be preferred.
 */
static GElf_Sym *
sym_prefer(GElf_Sym *sym1, char *name1, GElf_Sym *sym2, char *name2)
{
	/*
	 * Prefer the non-NULL symbol.
	 */
	if (sym1 == NULL)
		return (sym2);
	if (sym2 == NULL)
		return (sym1);

	/*
	 * Prefer a function to an object.
	 */
	if (GELF_ST_TYPE(sym1->st_info) != GELF_ST_TYPE(sym2->st_info)) {
		if (GELF_ST_TYPE(sym1->st_info) == STT_FUNC)
			return (sym1);
		if (GELF_ST_TYPE(sym2->st_info) == STT_FUNC)
			return (sym2);
	}

	/*
	 * Prefer the one that has a value closer to the requested addr.
	 */
	if (sym1->st_value > sym2->st_value)
		return (sym1);
	if (sym1->st_value < sym2->st_value)
		return (sym2);

	/*
	 * Prefer the weak or strong global symbol to the local symbol.
	 */
	if (GELF_ST_BIND(sym1->st_info) != GELF_ST_BIND(sym2->st_info)) {
		if (GELF_ST_BIND(sym1->st_info) == STB_LOCAL)
			return (sym2);
		if (GELF_ST_BIND(sym2->st_info) == STB_LOCAL)
			return (sym1);
	}

	/*
	 * Prefer the symbol with fewer leading underscores in the name.
	 */
	while (*name1 == '_' && *name2 == '_')
		name1++, name2++;
	if (*name1 == '_')
		return (sym2);
	if (*name2 == '_')
		return (sym1);

	/*
	 * Prefer the smaller sized symbol.
	 */
	if (sym1->st_size < sym2->st_size)
		return (sym1);
	if (sym1->st_size > sym2->st_size)
		return (sym2);

	/*
	 * There is no reason to prefer one to the other.
	 * Arbitrarily prefer the first one.
	 */
	return (sym1);
}

/*
 * Look up a symbol by address in the specified symbol table.
 * Adjustment to 'addr' must already have been made for the
 * offset of the symbol if this is a dynamic library symbol table.
 */
static GElf_Sym *
sym_by_addr(sym_tbl_t *symtab, GElf_Addr addr, GElf_Sym *symbolp)
{
	Elf_Data *data = symtab->sym_data;
	size_t symn = symtab->sym_symn;
	char *strs = symtab->sym_strs;
	GElf_Sym sym, *symp = NULL;
	GElf_Sym osym, *osymp = NULL;
	int type;
	int i;

	if (data == NULL || symn == 0 || strs == NULL)
		return (NULL);

	for (i = 0; i < symn; i++) {
		if ((symp = gelf_getsym(data, i, &sym)) != NULL) {
			type = GELF_ST_TYPE(sym.st_info);
			if ((type == STT_OBJECT || type == STT_FUNC) &&
			    addr >= sym.st_value &&
			    addr < sym.st_value + sym.st_size) {
				if (osymp)
					symp = sym_prefer(
					    symp, strs + symp->st_name,
					    osymp, strs + osymp->st_name);
				if (symp != osymp) {
					osym = sym;
					osymp = &osym;
				}
			}
		}
	}
	if (osymp) {
		*symbolp = osym;
		return (symbolp);
	}
	return (NULL);
}

/*
 * Look up a symbol by name in the specified symbol table.
 */
static GElf_Sym *
sym_by_name(sym_tbl_t *symtab, const char *name, GElf_Sym *sym)
{
	Elf_Data *data = symtab->sym_data;
	size_t symn = symtab->sym_symn;
	char *strs = symtab->sym_strs;
	int type;
	int i;

	if (data == NULL || symn == 0 || strs == NULL)
		return (NULL);

	for (i = 0; i < symn; i++) {
		if (gelf_getsym(data, i, sym) != NULL) {
			type = GELF_ST_TYPE(sym->st_info);
			if ((type == STT_OBJECT || type == STT_FUNC) &&
			    strcmp(name, strs + sym->st_name) == 0) {
				return (sym);
			}
		}
	}

	return (NULL);
}

/*
 * Search the process symbol tables looking for a symbol whose
 * value to value+size contain the address specified by addr.
 * Return values are:
 *	sym_name_buffer containing the symbol name
 *	GElf_Sym symbol table entry
 * Returns 0 on success, -1 on failure.
 */
int
Plookup_by_addr(
	struct ps_prochandle *P,
	uintptr_t addr,			/* process address being sought */
	char *sym_name_buffer,		/* buffer for the symbol name */
	size_t bufsize,			/* size of sym_name_buffer */
	GElf_Sym *symbolp)		/* returned symbol table entry */
{
	GElf_Sym	*symp;
	char		*name;
	GElf_Sym	sym1, *sym1p = NULL;
	GElf_Sym	sym2, *sym2p = NULL;
	char		*name1 = NULL;
	char		*name2 = NULL;
	map_info_t	*mptr;
	file_info_t	*fptr;

	if (P->num_mappings == 0)
		Pupdate_maps(P);

	if ((mptr = Paddr2mptr(P, addr)) == NULL ||	/* no such address */
	    (fptr = build_map_symtab(P, mptr)) == NULL || /* no mapped file */
	    fptr->file_elf == NULL)			/* not an ELF file */
		return (-1);

	/*
	 * Adjust the address by the load object base address in
	 * case the address turns out to be in a shared library.
	 */
	addr -= fptr->file_dyn_base;

	/*
	 * Search both symbol tables, symtab first, then dynsym.
	 */
	if ((sym1p = sym_by_addr(&fptr->file_symtab, addr, &sym1)) != NULL)
		name1 = fptr->file_symtab.sym_strs + sym1.st_name;
	if ((sym2p = sym_by_addr(&fptr->file_dynsym, addr, &sym2)) != NULL)
		name2 = fptr->file_dynsym.sym_strs + sym2.st_name;

	if ((symp = sym_prefer(sym1p, name1, sym2p, name2)) == NULL)
		return (-1);

	name = (symp == sym1p) ? name1 : name2;
	(void) strncpy(sym_name_buffer, name, bufsize);
	sym_name_buffer[bufsize - 1] = '\0';

	*symbolp = *symp;
	symbolp->st_value += fptr->file_dyn_base;

	return (0);
}

/*
 * Search the process symbol tables looking for a symbol
 * whose name matches the specified name.
 * Return values are:
 *	GElf_Sym symbol table entry
 * Returns 0 on success, -1 on failure.
 */
int
Plookup_by_name(
	struct ps_prochandle *P,
	const char *object_name,	/* load object name */
	const char *symbol_name,	/* symbol name */
	GElf_Sym *symp)			/* returned symbol table entry */
{
	map_info_t *mptr;
	file_info_t *fptr;
	int cnt;

	GElf_Sym sym;
	int rv = -1;

	if (object_name == PR_OBJ_EVERY) {
		/* create all the file_info_t's for all the mappings */
		(void) Prd_agent(P);
		cnt = P->num_files;
		fptr = list_next(&P->file_head);
	} else {
		cnt = 1;
		if ((mptr = object_name_to_map(P, object_name)) == NULL ||
		    (fptr = build_map_symtab(P, mptr)) == NULL)
			return (-1);
	}

	/*
	 * Iterate through the loaded object files and look for the symbol
	 * name in the .symtab and .dynsym of each.  If we encounter a match
	 * with SHN_UNDEF, keep looking in hopes of finding a better match.
	 * This means that a name such as "puts" will match the puts function
	 * in libc instead of matching the puts PLT entry in the a.out file.
	 */
	for (; cnt > 0; cnt--, fptr = list_next(fptr)) {
		if (!fptr->file_init)
			Pbuild_file_symtab(P, fptr);

		if (fptr->file_elf == NULL)
			continue;

		if (!sym_by_name(&fptr->file_symtab, symbol_name, symp) &&
		    !sym_by_name(&fptr->file_dynsym, symbol_name, symp))
			continue;

		symp->st_value += fptr->file_dyn_base;

		if (symp->st_shndx != SHN_UNDEF)
			return (0);

		if (rv != 0) {
			sym = *symp;
			rv = 0;
		}
	}

	if (rv == 0)
		*symp = sym;

	return (rv);
}

/*
 * Iterate over the process's address space mappings.
 */
int
Pmapping_iter(struct ps_prochandle *P, proc_map_f *func, void *cd)
{
	map_info_t *mptr;
	file_info_t *fptr;
	uint_t cnt;
	char *object_name;
	int rc = 0;

	/* create all the file_info_t's for all the mappings */
	(void) Prd_agent(P);

	for (cnt = P->num_mappings, mptr = list_next(&P->map_head);
	    cnt; cnt--, mptr = list_next(mptr)) {
		if ((fptr = mptr->map_file) == NULL)
			object_name = NULL;
		else
			object_name = fptr->file_lname;
		if ((rc = func(cd, &mptr->map_pmap, object_name)) != 0)
			return (rc);
	}
	return (0);
}

/*
 * Iterate over the process's mapped objects.
 */
int
Pobject_iter(struct ps_prochandle *P, proc_map_f *func, void *cd)
{
	map_info_t *mptr;
	file_info_t *fptr;
	uint_t cnt;
	int rc = 0;

	(void) Prd_agent(P); /* create file_info_t's for all the mappings */

	for (cnt = P->num_files, fptr = list_next(&P->file_head);
	    cnt; cnt--, fptr = list_next(fptr)) {

		const char *lname = fptr->file_lname ? fptr->file_lname : "";

		if ((mptr = fptr->file_map) == NULL)
			continue;

		if ((rc = func(cd, &mptr->map_pmap, lname)) != 0)
			return (rc);
	}
	return (0);
}

/*
 * Given a virtual address, return the name of the underlying
 * mapped object (file), as provided by the dynamic linker.
 * Return NULL on failure (no underlying shared library).
 */
char *
Pobjname(struct ps_prochandle *P, uintptr_t addr,
	char *buffer, size_t bufsize)
{
	map_info_t *mptr;
	file_info_t *fptr;

	/* create all the file_info_t's for all the mappings */
	(void) Prd_agent(P);

	if ((mptr = Paddr2mptr(P, addr)) != NULL &&
	    (fptr = mptr->map_file) != NULL &&
	    fptr->file_lname != NULL) {
		(void) strncpy(buffer, fptr->file_lname, bufsize);
		if (strlen(fptr->file_lname) >= bufsize)
			buffer[bufsize-1] = '\0';
		return (buffer);
	}
	return (NULL);
}

/*
 * Given an object name, iterate over the object's symbols.
 * If which == PR_SYMTAB, search the normal symbol table.
 * If which == PR_DYNSYM, search the dynamic symbol table.
 */
int
Psymbol_iter(struct ps_prochandle *P, const char *object_name,
	int which, int mask, proc_sym_f *func, void *cd)
{
	GElf_Sym sym;
	map_info_t *mptr;
	file_info_t *fptr;
	sym_tbl_t *symtab;
	Elf_Data *data;
	size_t symn;
	const char *strs;
	size_t offset;
	int i;
	int rv;

	if ((mptr = object_name_to_map(P, object_name)) == NULL)
		return (-1);

	if ((fptr = build_map_symtab(P, mptr)) == NULL || /* no mapped file */
	    fptr->file_elf == NULL)			/* not an ELF file */
		return (-1);

	/*
	 * Search the specified symbol table.
	 */
	switch (which) {
	case PR_SYMTAB:
		symtab = &fptr->file_symtab;
		break;
	case PR_DYNSYM:
		symtab = &fptr->file_dynsym;
		break;
	default:
		return (-1);
	}

	data = symtab->sym_data;
	symn = symtab->sym_symn;
	strs = symtab->sym_strs;

	if (data == NULL || strs == NULL)
		return (-1);

	offset = fptr->file_dyn_base;
	rv = 0;

	for (i = 0; i < symn; i++) {
		if (gelf_getsym(data, i, &sym) != NULL) {
			uint_t s_bind, s_type, type;

			if (sym.st_shndx == SHN_UNDEF)	/* symbol reference */
				continue;

			s_bind = GELF_ST_BIND(sym.st_info);
			s_type = GELF_ST_TYPE(sym.st_info);

			/*
			 * In case you haven't already guessed, this relies on
			 * the bitmask used in <libproc.h> for encoding symbol
			 * type and binding matching the order of STB and STT
			 * constants in <sys/elf.h>.  ELF can't change without
			 * breaking binary compatibility, so I think this is
			 * reasonably fair game.
			 */
			if (s_bind < STB_NUM && s_type < STT_NUM) {
				type = (1 << (s_type + 8)) | (1 << s_bind);
				if ((type & ~mask) != 0)
					continue;
			} else
				continue; /* Invalid type or binding */

			sym.st_value += offset;
			if ((rv = func(cd, &sym, strs + sym.st_name)) != 0)
				break;
		}
	}

	return (rv);
}

/* number of argument or environment pointers to read all at once */
#define	NARG	100

/*
 * Like getenv(char *name), but applied to the target proces.
 * The caller must provide a buffer for the resulting string.
 */
char *
Pgetenv(struct ps_prochandle *P, const char *name,
	char *buffer, size_t bufsize)
{
	const psinfo_t *psp;
	uintptr_t envpoff;
	GElf_Sym sym;

	int nenv = NARG;
	long envp[NARG];

	/*
	 * Attempt to find the "_environ" variable in the process.
	 * Failing that, use the original value provided by Ppsinfo().
	 */
	if ((psp = Ppsinfo(P)) == NULL)
		return (NULL);

	envpoff = psp->pr_envp; /* Default if no _environ found */

	if (Plookup_by_name(P, PR_OBJ_EXEC, "_environ", &sym) == 0) {
		if (P->status.pr_dmodel == PR_MODEL_NATIVE) {
			if (Pread(P, &envpoff, sizeof (envpoff),
			    sym.st_value) != sizeof (envpoff))
				envpoff = psp->pr_envp;
		} else if (P->status.pr_dmodel == PR_MODEL_ILP32) {
			uint32_t envpoff32;

			if (Pread(P, &envpoff32, sizeof (envpoff32),
			    sym.st_value) != sizeof (envpoff32))
				envpoff = psp->pr_envp;
			else
				envpoff = envpoff32;
		}
	}

	for (;;) {
		char string[PATH_MAX], *s;
		uintptr_t envoff;

		if (nenv == NARG) {
			(void) memset(envp, 0, sizeof (envp));
			if (P->status.pr_dmodel == PR_MODEL_NATIVE) {
				if (Pread(P, envp, sizeof (envp), envpoff) <= 0)
					break;
			} else if (P->status.pr_dmodel == PR_MODEL_ILP32) {
				uint32_t e32[NARG];
				int i;

				(void) memset(e32, 0, sizeof (e32));
				if (Pread(P, e32, sizeof (e32), envpoff) <= 0)
					break;
				for (i = 0; i < NARG; i++)
					envp[i] = e32[i];
			}
			nenv = 0;
		}

		if ((envoff = envp[nenv++]) == NULL)
			break;

		if (Pread_string(P, string, sizeof (string), envoff) > 0 &&
		    (s = strchr(string, '=')) != NULL) {
			*s++ = '\0';
			if (strcmp(name, string) == 0) {
				(void) strncpy(buffer, s, bufsize);
				return (buffer);
			}
		}

		envpoff += (P->status.pr_dmodel == PR_MODEL_LP64)? 8 : 4;
	}

	return (NULL);
}

/*
 * Get the platform string from the core file if we have it;
 * just perform the system call for the caller if this is a live process.
 */
char *
Pplatform(struct ps_prochandle *P, char *s, size_t n)
{
	if (P->state == PS_DEAD) {
		if (P->core->core_platform == NULL) {
			errno = ENODATA;
			return (NULL);
		}
		(void) strncpy(s, P->core->core_platform, n - 1);
		s[n - 1] = '\0';

	} else if (sysinfo(SI_PLATFORM, s, n) == -1)
		return (NULL);

	return (s);
}

/*
 * Get the uname(2) information from the core file if we have it;
 * just perform the system call for the caller if this is a live process.
 */
int
Puname(struct ps_prochandle *P, struct utsname *u)
{
	if (P->state == PS_DEAD) {
		if (P->core->core_uts == NULL) {
			errno = ENODATA;
			return (-1);
		}
		(void) memcpy(u, P->core->core_uts, sizeof (struct utsname));
		return (0);
	}
	return (uname(u));
}

/*
 * Called from Pcreate(), Pgrab(), and Pfgrab_core() to initialize
 * the symbol table heads in the new ps_prochandle.
 */
void
Pinitsym(struct ps_prochandle *P)
{
	P->num_mappings = 0;
	P->num_files = 0;
	list_link(&P->map_head, NULL);
	list_link(&P->file_head, NULL);
}

/*
 * Called from Prelease() to destroy the symbol tables.
 * Must be called by the client after an exec() in the victim process.
 */
void
Preset_maps(struct ps_prochandle *P)
{
	if (P->rap != NULL) {
		rd_delete(P->rap);
		P->rap = NULL;
	}

	if (P->execname != NULL) {
		free(P->execname);
		P->execname = NULL;
	}

	if (P->auxv != NULL) {
		free(P->auxv);
		P->auxv = NULL;
	}

	while (P->num_mappings > 0)
		map_info_free(P, list_next(&P->map_head));

	P->info_valid = 0;
}
