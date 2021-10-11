/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kobj_stubs.c	1.16	99/05/04 SMI"

#include <sys/kobj.h>

/*
 * Stubs for entry points into
 * the stand-alone linker/loader.
 */

/*ARGSUSED*/
void
kobj_load_module(struct modctl *modp, int use_path)
{}

/*ARGSUSED*/
void
kobj_unload_module(struct modctl *modp)
{}

/*ARGSUSED*/
struct _buf *
kobj_open_path(char *name, int use_path, int use_moddir_suffix)
{
	return (NULL);
}

/*ARGSUSED*/
struct _buf *
kobj_open_file(char *name)
{
	return (NULL);
}

/*ARGSUSED*/
int
kobj_read_file(struct _buf *file, char *buf, unsigned size, unsigned off)
{
	return (-1);
}

/*ARGSUSED*/
void
kobj_close_file(struct _buf *file)
{}

/*ARGSUSED*/
intptr_t
kobj_open(char *filename)
{
	return (-1L);
}

/*ARGSUSED*/
int
kobj_read(intptr_t descr, char *buf, unsigned size, unsigned offset)
{
	return (-1);
}

/*ARGSUSED*/
void
kobj_close(intptr_t descr)
{}

/*ARGSUSED*/
int
kobj_filbuf(struct _buf *f)
{
	return (-1);
}

/*ARGSUSED*/
int
kobj_addrcheck(void *xmp, caddr_t adr)
{
	return (1);
}

/*ARGSUSED*/
uintptr_t
kobj_getelfsym(char *name, void *mp, int *size)
{
	return (0);
}

/*ARGSUSED*/
void
kobj_getmodinfo(void *xmp, struct modinfo *modinfo)
{}

void
kobj_getpagesize()
{}

/*ARGSUSED*/
char *
kobj_getsymname(uintptr_t value, ulong_t *offset)
{
	return (NULL);
}

/*ARGSUSED*/
uintptr_t
kobj_getsymvalue(char *name, int kernelonly)
{
	return (0);
}

/*ARGSUSED*/
char *
kobj_searchsym(struct module *mp, uintptr_t value, ulong_t *offset)
{
	return (NULL);
}

/*ARGSUSED*/
uintptr_t
kobj_lookup(void *mod, char *name)
{
	return (0);
}

/*ARGSUSED*/
Sym *
kobj_lookup_all(struct module *mp, char *name, int include_self)
{
	return (NULL);
}

/*ARGSUSED*/
void *
kobj_alloc(size_t size, int flag)
{
	return (NULL);
}

/*ARGSUSED*/
void *
kobj_zalloc(size_t size, int flag)
{
	return (NULL);
}

/*ARGSUSED*/
void
kobj_free(void *address, size_t size)
{}

/*ARGSUSED*/
void
kobj_sync(void)
{}

/*ARGSUSED*/
void
kobj_stat_get(kobj_stat_t *kp)
{}

/*ARGSUSED*/
void
kobj_sync_instruction_memory(caddr_t addr, size_t size)
{
}

/*
 * Dummy declarations for variables in
 * the stand-alone linker/loader.
 */
void *__tnf_probe_list_head;
void *__tnf_tag_list_head;
