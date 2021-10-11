/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 */

#ident	"@(#)kvm_stubs.c	1.1	97/03/31 SMI"

/*
 * XX64: Stub out the kvm_* functions until we have libkvm working...
 */

int
kvm_nlist()
{
	return (-1);
}

void *
kvm_open()
{
	return (0);
}

int
kvm_close()
{
	return (0);
}

int
kvm_read()
{
	return (-1);
}

int
kvm_write()
{
	return (-1);
}
