/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)example1.c	1.1	99/11/19 SMI"

#include <sys/mdb_modapi.h>
#include <sys/sysinfo.h>

/*
 * simple_echo dcmd - Demonstrate some simple argument processing by iterating
 * through the argument array and printing back each argument.
 */
static int
simple_echo(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (flags & DCMD_ADDRSPEC)
		mdb_printf("You specified address %p\n", addr);
	else
		mdb_printf("Current address is %p\n", addr);

	while (argc-- != 0) {
		if (argv->a_type == MDB_TYPE_STRING)
			mdb_printf("%s ", argv->a_un.a_str);
		else
			mdb_printf("%llr ", argv->a_un.a_val);
		argv++;
	}

	mdb_printf("\n");
	return (DCMD_OK);
}

/*
 * vminfo dcmd - Print out the global vminfo structure, nicely formatted.
 * This function illustrates one of the functions for reading data from
 * the target program (or core file): mdb_readvar().  The vminfo_t
 * structure contains cumulative counters for various system virtual
 * memory statistics.  Each second, these are incremented by the current
 * values of freemem and the other corresponding statistical counters.
 */
/*ARGSUSED*/
static int
vminfo(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	vminfo_t vm;

	if ((flags & DCMD_ADDRSPEC) || argc != 0)
		return (DCMD_USAGE);

	if (mdb_readvar(&vm, "vminfo") == -1) {
		/*
		 * If the mdb_warn string does not end in a \n, mdb will
		 * automatically append the reason for the failure.
		 */
		mdb_warn("failed to read vminfo structure");
		return (DCMD_ERR);
	}

	mdb_printf("Cumulative memory statistics:\n");
	mdb_printf("%8llu pages of free memory\n", vm.freemem);
	mdb_printf("%8llu pages of reserved swap\n", vm.swap_resv);
	mdb_printf("%8llu pages of allocated swap\n", vm.swap_alloc);
	mdb_printf("%8llu pages of unreserved swap\n", vm.swap_avail);
	mdb_printf("%8llu pages of unallocated swap\n", vm.swap_free);

	return (DCMD_OK);
}

/*
 * MDB module linkage information:
 *
 * We declare a list of structures describing our dcmds, and a function
 * named _mdb_init to return a pointer to our module information.
 */

static const mdb_dcmd_t dcmds[] = {
	{ "simple_echo", "[ args ... ]", "echo back arguments", simple_echo },
	{ "vminfo", NULL, "print vm information", vminfo },
	{ NULL }
};

static const mdb_modinfo_t modinfo = {
	MDB_API_VERSION, dcmds, NULL
};

const mdb_modinfo_t *
_mdb_init(void)
{
	return (&modinfo);
}
