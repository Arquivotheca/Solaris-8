/*
 * BlastIt!
 * This program replaces IO.SYS on a boot floppy and only a boot
 * floppy, don't want to screw up my hard disk. BlastIt uses the 
 * blocks that are allocated to IO.SYS and copies the data onto
 * the disks.
 */

#include "disk.h"
short BootDev;
#ifdef DEBUG
long debug = 0;
#endif

char block[SECSIZ];
sa_start()
{
        extern struct _fat_controller_ fat_man;
        _dir_entry_t d;
        _file_desc_p fp;
        int c, i, bo = 0;
        int spc;
        long adjust;
        long sect;
        char filename[80];

        fatInit(0x0, TYPE_DOS);

        spc = fat_man.f_bpb.bs_sectors_per_cluster;
        adjust = fat_man.f_filesec + fat_man.f_adjust;

        if (dosStat("io.sys", &d)) {
                Print("Can't find IO.SYS\n");
                myExit(0);
        }

        Print("Enter filename: ");
        myGets(filename, 80);
        if ((fp = dosOpen(filename, FILE_READ)) == (_file_desc_p)0) {
                Print("Can't open %s\n", filename);
                myExit(0);
        }
        
        c = d.d_cluster;
        Print("Starting cluster %d\n", c);
        while (CLUSTER_VALID(c)) {
                sect = (c - 2) * spc + adjust;
                for (i = 0; i < spc; i++) {
                        if (dosRead(fp, &block[0], SECSIZ) <= 0)
                                /* ---- EOF ---- */
                                break;
                        if (biosWriteSect(&block[0], sect + i, 1)) {
                                Print("Failed to write sector %ld\n",
                                        sect + i);
                                myExit(1);
                        }
                        bo++;
                }
                c = fatMap(c);
        }
        if (CLUSTER_VALID(c) == 0)
                Print("You may have a problem. EOF on IO.SYS. Cluster %d\n", c);
        Print("Block written out %d blocks\n", bo);

        myExit(0);
}
