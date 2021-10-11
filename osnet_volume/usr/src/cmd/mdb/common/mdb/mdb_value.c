/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_value.c	1.1	99/08/11 SMI"

/*
 * Immediate Value Target
 *
 * The immediate value target is used when the '=' verb is used to format an
 * immediate value.  The target is initialized with a specific uintmax_t, and
 * then simply copies bytes from this integer in its read routine.  Two notes:
 *
 * (1) the value_read function is explicitly declared without an address
 * argument, so that it may be act as any of the *read ops-vector entry points.
 *
 * (2) in order to preserve the semantic that 0x1234=X produces output 0x1234,
 * we cannot actually just bcopy from our uintmax_t, because it (8 bytes) is
 * larger than the destination integer container for 'X' (4 bytes).  The adb
 * model (and mdb_fmt.c follows suit) is for this to act like a cast down to
 * the smaller integer type, so in the big-endian case only we may need to
 * explicitly move the source pointer and copy only the lower bytes.
 */

#include <mdb/mdb_target_impl.h>
#include <mdb/mdb_types.h>
#include <mdb/mdb_conf.h>
#include <mdb/mdb_err.h>

#include <sys/isa_defs.h>
#include <strings.h>

static ssize_t
value_read(mdb_tgt_t *t, void *dst, size_t nbytes)
{
	ssize_t n = MIN(nbytes, sizeof (uintmax_t));
	const void *src = t->t_data;

#ifdef _BIG_ENDIAN
	src = (const char *)src + sizeof (uintmax_t) - n;
#endif
	if (n != 0)
		bcopy(src, dst, n);

	return (n);
}

/*ARGSUSED*/
static ssize_t
value_write(mdb_tgt_t *t, const void *buf, size_t nbytes)
{
	return (nbytes); /* We allow writes to silently fail */
}

static const mdb_tgt_ops_t value_ops = {
	(int (*)()) mdb_tgt_notsup,		/* t_setflags */
	(int (*)()) mdb_tgt_notsup,		/* t_setcontext */
	(void (*)()) mdb_tgt_nop,		/* t_activate */
	(void (*)()) mdb_tgt_nop,		/* t_deactivate */
	(void (*)()) mdb_tgt_nop,		/* t_destroy */
	(const char *(*)()) mdb_tgt_null,	/* t_name */
	(const char *(*)()) mdb_conf_isa,	/* t_isa */
	(const char *(*)()) mdb_conf_platform,	/* t_platform */
	(int (*)()) mdb_tgt_notsup,		/* t_uname */
	(ssize_t (*)()) mdb_tgt_notsup,		/* t_aread */
	(ssize_t (*)()) mdb_tgt_notsup,		/* t_awrite */
	(ssize_t (*)()) value_read,		/* t_vread */
	(ssize_t (*)()) value_write,		/* t_vwrite */
	(ssize_t (*)()) value_read,		/* t_pread */
	(ssize_t (*)()) value_write,		/* t_pwrite */
	(ssize_t (*)()) value_read,		/* t_fread */
	(ssize_t (*)()) value_write,		/* t_fwrite */
	(ssize_t (*)()) value_read,		/* t_ioread */
	(ssize_t (*)()) value_write,		/* t_iowrite */
	(int (*)()) mdb_tgt_notsup,		/* t_vtop */
	(int (*)()) mdb_tgt_notsup,		/* t_lookup_by_name */
	(int (*)()) mdb_tgt_notsup,		/* t_lookup_by_addr */
	(int (*)()) mdb_tgt_notsup,		/* t_symbol_iter */
	(int (*)()) mdb_tgt_notsup,		/* t_mapping_iter */
	(int (*)()) mdb_tgt_notsup,		/* t_object_iter */
	(const mdb_map_t *(*)()) mdb_tgt_null,	/* t_addr_to_map */
	(const mdb_map_t *(*)()) mdb_tgt_null,	/* t_name_to_map */
	(int (*)()) mdb_tgt_notsup,		/* t_thread_iter */
	(int (*)()) mdb_tgt_notsup,		/* t_cpu_iter */
	(int (*)()) mdb_tgt_notsup,		/* t_thr_status */
	(int (*)()) mdb_tgt_notsup,		/* t_cpu_status */
	(int (*)()) mdb_tgt_notsup,		/* t_status */
	(int (*)()) mdb_tgt_notsup,		/* t_run */
	(int (*)()) mdb_tgt_notsup,		/* t_step */
	(int (*)()) mdb_tgt_notsup,		/* t_continue */
	(int (*)()) mdb_tgt_notsup,		/* t_call */
	(int (*)()) mdb_tgt_notsup,		/* t_add_brkpt */
	(int (*)()) mdb_tgt_notsup,		/* t_add_pwapt */
	(int (*)()) mdb_tgt_notsup,		/* t_add_vwapt */
	(int (*)()) mdb_tgt_notsup,		/* t_add_iowapt */
	(int (*)()) mdb_tgt_notsup,		/* t_add_ixwapt */
	(int (*)()) mdb_tgt_notsup,		/* t_add_sysenter */
	(int (*)()) mdb_tgt_notsup,		/* t_add_sysexit */
	(int (*)()) mdb_tgt_notsup,		/* t_add_signal */
	(int (*)()) mdb_tgt_notsup,		/* t_add_object_load */
	(int (*)()) mdb_tgt_notsup,		/* t_add_object_unload */
	(int (*)()) mdb_tgt_notsup,		/* t_getareg */
	(int (*)()) mdb_tgt_notsup,		/* t_putareg */
	(int (*)()) mdb_tgt_nop			/* t_stack_iter */
};

int
mdb_value_tgt_create(mdb_tgt_t *t, int argc, const char *argv[])
{
	if (argc != 1 || argv[0] == NULL)
		return (set_errno(EINVAL));

	t->t_ops = &value_ops;
	t->t_data = (void *)argv[0];

	return (0);
}
