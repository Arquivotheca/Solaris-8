
/*
 * Copyright (c) 1986 - 1991 by Sun Microsystems, Inc.
 */

#ident	"@(#)rpc_tblout.c	1.13	97/04/22 SMI"

#ifndef lint
static char sccsid[] = "@(#)rpc_tblout.c 1.4 89/02/22 (C) 1988 SMI";
#endif

/*
 * rpc_tblout.c, Dispatch table outputter for the RPC protocol compiler
 */
#include <stdio.h>
#include <string.h>
#include "rpc_parse.h"
#include "rpc_util.h"

#define	TABSIZE		8
#define	TABCOUNT	5
#define	TABSTOP		(TABSIZE*TABCOUNT)

static char tabstr[TABCOUNT+1] = "\t\t\t\t\t";

static char tbl_hdr[] = "struct rpcgen_table %s_table[] = {\n";
static char tbl_end[] = "};\n";

static char null_entry_b[] = "\n\t(char *(*)())0,\n"
			" \t(xdrproc_t) xdr_void,\t\t\t0,\n"
			" \t(xdrproc_t) xdr_void,\t\t\t0,\n";

static char null_entry[] = "\n\t(void *(*)())0,\n"
			" \t(xdrproc_t) xdr_void,\t\t\t0,\n"
			" \t(xdrproc_t) xdr_void,\t\t\t0,\n";


static char tbl_nproc[] = "int %s_nproc =\n\tsizeof(%s_table)"
				"/sizeof(%s_table[0]);\n\n";

void
write_tables()
{
	list *l;
	definition *def;

	f_print(fout, "\n");
	for (l = defined; l != NULL; l = l->next) {
		def = (definition *) l->val;
		if (def->def_kind == DEF_PROGRAM) {
			write_table(def);
		}
	}
}

static
write_table(def)
	definition *def;
{
	version_list *vp;
	proc_list *proc;
	int current;
	int expected;
	char progvers[100];
	int warning;

	for (vp = def->def.pr.versions; vp != NULL; vp = vp->next) {
		warning = 0;
		s_print(progvers, "%s_%s",
		    locase(def->def_name), vp->vers_num);
		/* print the table header */
		f_print(fout, tbl_hdr, progvers);

		if (nullproc(vp->procs)) {
			expected = 0;
		} else {
			expected = 1;
			if (tirpcflag)
				f_print(fout, null_entry);
			else
				f_print(fout, null_entry_b);
		}
		for (proc = vp->procs; proc != NULL; proc = proc->next) {
			current = atoi(proc->proc_num);
			if (current != expected++) {
				f_print(fout,
			"\n/*\n * WARNING: table out of order\n */\n");
				if (warning == 0) {
					f_print(stderr,
				    "WARNING %s table is out of order\n",
					    progvers);
					warning = 1;
					nonfatalerrors = 1;
				}
				expected = current + 1;
			}
			if (tirpcflag)
				f_print(fout,
					"\n\t(void *(*)())RPCGEN_ACTION(");
			else
				f_print(fout,
					"\n\t(char *(*)())RPCGEN_ACTION(");

			/* routine to invoke */
			if (Cflag && !newstyle)
				pvname_svc(proc->proc_name, vp->vers_num);
			else {
				if (newstyle) /* calls internal func */
					f_print(fout, "_");
				pvname(proc->proc_name, vp->vers_num);
			}
			f_print(fout, "),\n");

			/* argument info */
			if (proc->arg_num > 1)
				printit((char *) NULL, proc->args.argname);
			else
			/* do we have to do something special for newstyle */
				printit(proc->args.decls->decl.prefix,
					proc->args.decls->decl.type);
			/* result info */
			printit(proc->res_prefix, proc->res_type);
		}

		/* print the table trailer */
		f_print(fout, tbl_end);
		f_print(fout, tbl_nproc, progvers, progvers, progvers);
	}
}

static
printit(prefix, type)
	char *prefix;
	char *type;
{
	int len;
	int tabs;


	len = fprintf(fout, "\t(xdrproc_t) xdr_%s,", stringfix(type));
	/* account for leading tab expansion */
	len += TABSIZE - 1;
	if (len >= TABSTOP) {
		f_print(fout, "\n");
		len = 0;
	}
	/* round up to tabs required */
	tabs = (TABSTOP - len + TABSIZE - 1)/TABSIZE;
	f_print(fout, "%s", &tabstr[TABCOUNT-tabs]);

	if (streq(type, "void")) {
		f_print(fout, "0");
	} else {
		f_print(fout, "sizeof ( ");
		/* XXX: should "follow" be 1 ??? */
		ptype(prefix, type, 0);
		f_print(fout, ")");
	}
	f_print(fout, ",\n");
}
