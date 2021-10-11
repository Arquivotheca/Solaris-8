/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)bpb.h	1.6	95/03/03 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		bpb.h
 *
 *   Description:	The MDB diskette's BIOS Parameter Block
 *
 */

#ifndef _BPB_H
#define _BPB_H

#pragma pack (1)

struct bios_param_blk {
   char   VDiskName[8];
   ushort VBytesPerSector;                   /* bytes per sector */
   unchar VSectorsPerCluster;             /* sectors per cluster */
   ushort VReservedSectors;          /* number of reserved sectors */
   unchar VNumberOfFATs;
   ushort VRootDirEntries;
   ushort VTotalSectors;
   unchar VMediaDescriptor;
   ushort VSectorsPerFAT;
   ushort VSectorsPerTrack;
   ushort VNumberOfHeads;
   ushort VHiddenSectorsL;
   ushort VHiddenSectorsH;
   ulong  VTotalSectorsBig;
   unchar VPhysicalDriveNum;
   unchar VV4Reserved;
   unchar VExtBootSignature;
   ulong  VVolSerialNumber;
   unchar VVolumeLabel[11];
   unchar VFATType[8];
   ushort VbpbOffsetHigh;
   ushort VbpbOffsetLow;
};


typedef struct bios_param_blk BPB_T;

#endif            /* _BPB_H */
