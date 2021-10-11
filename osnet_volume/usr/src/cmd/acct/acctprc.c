/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)acctprc.c	1.7	96/08/16 SMI"	/* SVr4.0 1.3	*/
/*
 *      acctprc
 *      reads std. input (acct.h format), 
 *      writes std. output (tacct format)
 *      sorted by uid
 *      adds login names
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include "acctdef.h"
#include <sys/acct.h>
#include <string.h>
#include <search.h>

struct  acct    ab;
struct  ptmp    pb;
struct  tacct   tb;

struct  utab    {
        uid_t   ut_uid;
        char    ut_name[NSZ];
        float   ut_cpu[2];      /* cpu time (mins) */
        float   ut_kcore[2];    /* kcore-mins */
        long    ut_pc;          /* # processes */
} * ub; 
static  usize;
char	*strncpy();
void **root = NULL;

main()
{
        long    elaps[2];
        long    etime, stime, mem;
#ifdef uts
	float   expand();
#else
	time_t  expand();
#endif

        while (fread(&ab, sizeof(ab), 1, stdin) == 1) {
                if (!MYKIND(ab.ac_flag))
                        continue;
                pb.pt_uid = ab.ac_uid;
                CPYN(pb.pt_name, NULL);
                /*
                 * approximate cpu P/NP split as same as elapsed time
                 */
                if ((etime = SECS(expand(ab.ac_etime))) == 0)
                        etime = 1;
                stime = expand(ab.ac_stime) + expand(ab.ac_utime);
                mem = expand(ab.ac_mem);
                pnpsplit(ab.ac_btime, etime, elaps);
                pb.pt_cpu[0] = (double)stime * (double)elaps[0] / etime;
                pb.pt_cpu[1] = (stime > pb.pt_cpu[0])? stime - pb.pt_cpu[0] : 0;
                pb.pt_cpu[1] = stime - pb.pt_cpu[0];
                if (stime)
                        pb.pt_mem = (mem + stime - 1) / stime;
                else
                        pb.pt_mem = 0;  /* unlikely */
                enter(&pb);
        }
        output();
	exit(0);
}

int node_compare(const void *node1, const void *node2)
{
	if (((const struct utab *)node1)->ut_uid > \
		((const struct utab *)node2)->ut_uid)
		return(1); 
	else if (((const struct utab *)node1)->ut_uid < \
		((const struct utab *)node2)->ut_uid)
		return(-1);
	else	return(0);
}

enter(p) 
register struct ptmp *p; 
{
        double memk;
        struct utab **pt;
         
	if ((ub = (struct utab *)malloc(sizeof (struct utab))) == NULL) {
		fprintf(stderr, "acctprc: malloc fail!\n");
		exit(2);
	}

        ub->ut_uid = p->pt_uid;
        CPYN(ub->ut_name, p->pt_name);
        ub->ut_cpu[0] = MINT(p->pt_cpu[0]);
        ub->ut_cpu[1] = MINT(p->pt_cpu[1]);
        memk = KCORE(pb.pt_mem);  
        ub->ut_kcore[0] = memk * MINT(p->pt_cpu[0]);
        ub->ut_kcore[1] = memk * MINT(p->pt_cpu[1]);
        ub->ut_pc = 1;
         
        if (*(pt = (struct utab **)tsearch((void *)ub, (void **)&root,  \
                node_compare)) == NULL) {
                fprintf(stderr, "Not enough space available to build tree\n");
                exit(1);
	}

	if (*pt != ub) {
        	(*pt)->ut_cpu[0] += MINT(p->pt_cpu[0]);
        	(*pt)->ut_cpu[1] += MINT(p->pt_cpu[1]);
        	(*pt)->ut_kcore[0] += memk * MINT(p->pt_cpu[0]);
        	(*pt)->ut_kcore[1] += memk * MINT(p->pt_cpu[1]);
		(*pt)->ut_pc++;
		free(ub);
        }
}

void print_node(const void *node, VISIT order, int level) {

	if (order == postorder || order == leaf) {
		tb.ta_uid = (*(struct utab **)node)->ut_uid;
		CPYN(tb.ta_name, (char *)uidtonam((*(struct utab **)node)->ut_uid));
		tb.ta_cpu[0] = (*(struct utab **)node)->ut_cpu[0];
		tb.ta_cpu[1] = (*(struct utab **)node)->ut_cpu[1];
                tb.ta_kcore[0] = (*(struct utab **)node)->ut_kcore[0];
                tb.ta_kcore[1] = (*(struct utab **)node)->ut_kcore[1];
                tb.ta_pc = (*(struct utab **)node)->ut_pc;
                fwrite(&tb, sizeof(tb), 1, stdout);
	}
}
 
output()
{
                twalk((struct utab *)root, print_node);
}
