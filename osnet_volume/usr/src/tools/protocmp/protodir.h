/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)protodir.h	1.2	99/08/25 SMI"

extern int read_in_protodir(const char *, elem_list *, int);
extern int process_dependencies(const char *pkgname, const char *parentdir,
    elem_list *list, int verbose);
extern int process_package_dir(const char *pkgname, const char *protodir,
    elem_list *list, int verbose);
