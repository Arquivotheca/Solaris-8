/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)snoop_smb.c 1.1	99/10/21 SMI"

/*
 * References used throughout this code:
 *
 * [CIFS/1.0] :  A Common Internet File System (CIFS/1.0) Protocol
 *                 Internet Engineering Task Force (IETF) draft
 *                 Paul J. Leach, Microsoft, Dec. 1997
 *
 * [X/Open-SMB] :  X/Open CAE Specification;
 *                 Protocols for X/Open PC Interworking: SMB, Version 2
 *                 X/Open Document Number: C209
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "snoop.h"

extern char *dlc_header;
char *show_type();

/* some macros just for compactness */
#define	GETLINE get_line(0, 0)
#define	DECARGS int flags, uchar_t *data, int len, char *extrainfo

/*
 * SMB Format (header)
 * [X/Open-SMB, Sec. 5.1]
 */
struct smb {
	uchar_t idf[4]; /*  identifier, contains 0xff, 'SMB'  */
	uchar_t com;    /*  command code  */
	uchar_t rcls;   /*  error class  */
	uchar_t res;
	uchar_t err[2]; /*  error code  */
	uchar_t flags;
	uchar_t flags2[2];
	uchar_t re[12];
	uchar_t tid[2];
	uchar_t pid[2];
	uchar_t uid[2];
	uchar_t mid[2];
	/*
	 * immediately after the above 32 byte header:
	 *   unsigned char  WordCount;
	 *   unsigned short ParameterWords[ WordCount ];
	 *   unsigned short ByteCount;
	 *   unsigned char  ParameterBytes[ ByteCount ];
	 */
};

/* smb flags */
#define	SERVER_RESPONSE	0x80

static void interpret_sesssetupX(DECARGS);
static void interpret_tconX(DECARGS);
static void interpret_trans(DECARGS);
static void interpret_trans2(DECARGS);
static void interpret_negprot(DECARGS);
static void interpret_default(DECARGS);

/*
 * Trans2 subcommand codes
 * [X/Open-SMB, Sec. 16.1.7]
 */
#define	TRANS2_OPEN 0x00
#define	TRANS2_FIND_FIRST 0x01
#define	TRANS2_FIND_NEXT2 0x02
#define	TRANS2_QUERY_FS_INFORMATION 0x03
#define	TRANS2_QUERY_PATH_INFORMATION 0x05
#define	TRANS2_SET_PATH_INFORMATION 0x06
#define	TRANS2_QUERY_FILE_INFORMATION 0x07
#define	TRANS2_SET_FILE_INFORMATION 0x08
#define	TRANS2_CREATE_DIRECTORY 0x0D


struct decode {
	char *name;
	void (*func)(DECARGS);
	char *callfmt;
	char *replyfmt;
};

/*
 * SMB command codes (function names)
 * [X/Open-SMB, Sec. 5.2]
 */
static struct decode SMBtable[256] = {
	/* 0x00 */
	{ "mkdir", 0, 0, 0 },
	{ "rmdir", 0, 0, 0 },
	{ "open", 0, 0, 0 },
	{ "create", 0, 0, 0 },

	{
		"close", 0,
		/* [X/Open-SMB, Sec. 7.10] */
		"WFileID\0lLastModTime\0wByteCount\0\0",
		"wByteCount\0\0"
	},

	{ "flush", 0, 0, 0 },
	{ "unlink", 0, 0, 0 },

	{
		"mv", 0,
		/* [X/Open-SMB, Sec. 7.11] */
		"wFileAttributes\0wByteCount\0"
		"r\0UFileName\0r\0UNewPath\0\0",
		"wByteCount\0\0"
	},

	{
		"getatr", 0,
		/* [X/Open-SMB, Sec. 8.4] */
		"dBytecount\0r\0UFileName\0\0",
		"wFileAttributes\0lTime\0lSize\0R\0R\0R\0"
		"R\0R\0wByteCount\0\0"
	},

	{ "setatr", 0, 0, 0 },

	{
		"read", 0,
		/* [X/Open-SMB, Sec. 7.4] */
		"WFileID\0wI/0 Bytes\0LFileOffset\0"
		"WBytesLeft\0wByteCount\0\0",
		"WDataLength\0R\0R\0R\0R\0wByteCount\0\0"
	},

	{
		"write", 0,
		/* [X/Open-SMB, Sec. 7.5] */
		"WFileID\0wI/0 Bytes\0LFileOffset\0WBytesLeft\0"
		"wByteCount\0\0",
		"WDataLength\0wByteCount\0\0"
	},

	{ "lock", 0, 0, 0 },
	{ "unlock", 0, 0, 0 },
	{ "ctemp", 0, 0, 0 },
	{ "mknew", 0, 0, 0 },

	/* 0x10 */
	{
		"chkpth", 0,
		/* [X/Open-SMB, Sec. 8.7] */
		"wByteCount\0r\0UFile\0\0",
		"wByteCount\0\0"
	},

	{ "exit", 0, 0, 0 },
	{ "lseek", 0, 0, 0 },
	{ "lockread", 0, 0, 0 },
	{ "writeunlock", 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },

	{
		"readbraw", 0,
		/* [X/Open-SMB, Sec. 10.1] */
		"WFileID\0LFileOffset\0wMaxCount\0"
		"wMinCount\0lTimeout\0R\0wByteCount\0\0", 0
	},

	{ "readbmpx", 0, 0, 0 },
	{ "readbs", 0, 0, 0 },
	{ "writebraw", 0, 0, 0 },
	{ "writebmpx", 0, 0, 0 },
	{ "writebs", 0, 0, 0 },

	/* 0x20 */
	{ "writec", 0, 0, 0 },
	{ "qrysrv", 0, 0, 0 },
	{ "setattrE", 0, 0, 0 },
	{ "getattrE", 0, 0, 0 },

	{
		"lockingX", 0,
		/* [X/Open-SMB, Sec. 12.2] */
		"wChainedCommand\0wNextOffset\0WFileID\0"
		"wLockType\0lOpenTimeout\0"
		"W#Unlocks\0W#Locks\0wByteCount\0\0", 0
	},

	{ "trans", interpret_trans, 0, 0 },
	{ "transs", 0, 0, 0 },
	{ "ioctl", 0, 0, 0 },
	{ "ioctls", 0, 0, 0 },
	{ "copy", 0, 0, 0 },
	{ "move", 0, 0, 0 },
	{ "echo", 0, 0, 0 },
	{ "writeclose", 0, 0, 0 },

	{
		"openX", 0,
		/* [X/Open-SMB, Sec. 12.1] */
		"wChainedCommand\0wNextOffset\0wFlags\0"
		"wMode\0wSearchAttributes\0wFileAttributes\0"
		"lTime\0wOpenFunction\0lFileSize\0lOpenTimeout\0"
		"R\0R\0wByteCount\0r\0UFileName\0\0",
		"wChainedCommand\0wNextOffset\0WFileID\0"
		"wAttributes\0lTime\0LSize\0wOpenMode\0"
		"wFileType\0wDeviceState\0wActionTaken\0"
		"lUniqueFileID\0R\0wBytecount\0\0"
	},

	{ "readX", 0, 0, 0 },
	{ "writeX", 0, 0, 0 },

	/* 0x30 */
	{ 0, 0, 0, 0 },
	{ "closeTD", 0, 0, 0 },
	{ "trans2", interpret_trans2, 0, 0 },
	{ "trans2s", 0, 0, 0 },
	{
		"findclose", 0,
		/* [X/Open-SMB, Sec. 15.4 ] */
		"WFileID\0wByteCount\0\0",
		"wByteCount\0\0"
	},
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },

	/* 0x40 */
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },

	/* 0x50 */
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },

	/* 0x60 */
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },

	/* 0x70 */
	{ "tcon", 0, 0, 0 },
	{
		"tdis", 0,
		/* [X/Open-SMB, Sec. 6.3] */
		"wByteCount\0\0",
		"wByteCount\0\0"
	},
	{ "negprot", interpret_negprot, 0, 0 },
	{ "sesssetupX", interpret_sesssetupX, 0, 0 },
	{
		"uloggoffX", 0,
		/* [X/Open-SMB, Sec. 15.5] */
		"wChainedCommand\0wNextOffset\0\0",
		"wChainedCommnad\0wNextOffset\0\0" },
	{ "tconX", interpret_tconX, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },

	/* 0x80 */
	{ "dskattr", 0, 0, 0 },
	{ "search", 0, 0, 0 },
	{ "ffirst", 0, 0, 0 },
	{ "funique", 0, 0, 0 },
	{ "fclose", 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },

	/* 0x90 */
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },

	/* 0xa0 */
	/*
	 * Command codes 0xa0 to 0xa7 are from
	 * [CIFS/1.0, Sec. 5.1]
	 */
	{ " NT_Trans", 0, 0, 0 },
	{ " NT_Trans2", 0, 0, 0 },
	{
		" NT_CreateX", 0,
		/* [CIFS/1.0, Sec. 4.2.1] */
		"wChainedCommand\0wNextOffset\0r\0"
		"wNameLength\0lCreateFlags\0lRootDirFID\0"
		"lDesiredAccess\0R\0R\0R\0R\0"
		"lNTFileAttributes\0lFileShareAccess\0"
		"R\0R\0lCreateOption\0lImpersonationLevel\0"
		"bSecurityFlags\0wByteCount\0r\0"
		"UFileName\0\0",
		"wChainedCommand\0wNextOffset\0"
		"bOplockLevel\0WFileID\0lCreateAction\0\0"
	},
	{ 0, 0, 0, 0 },
	{
		" NT_Cancel", 0,
		/* [CIFS/1.0, Sec. 4.1.8] */
		"wByteCount\0", 0
	},
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },

	/* 0xb0 */
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },

	/* 0xc0 */
	{ "splopen", 0, 0, 0 },
	{ "splwr", 0, 0, 0 },
	{ "splclose", 0, 0, 0 },
	{ "splretq", 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },

	/* 0xd0 */
	{ "sends", 0, 0, 0 },
	{ "sendb", 0, 0, 0 },
	{ "fwdname", 0, 0, 0 },
	{ "cancelf", 0, 0, 0 },
	{ "getmac", 0, 0, 0 },
	{ "sendstrt", 0, 0, 0 },
	{ "sendend", 0, 0, 0 },
	{ "sendtxt", 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },

	/* 0xe0 */
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },

	/* 0xf0 */
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 }
};

/* Helpers to get short and int values in Intel order. */
static ushort_t
get2(uchar_t *p) {
	return (p[0] + (p[1]<<8));
}
static uint_t
get4(uchar_t *p) {
	return (p[0] + (p[1]<<8) + (p[2]<<16) + (p[3]<<24));
}

/*
 * This is called by snoop_netbios.c.
 * This is the external entry point.
 */
void
interpret_smb(int flags, uchar_t *data, int len)
{
	struct smb *smb;
	char *call_reply_detail, *call_reply_sum;
	struct decode *decoder;
	char xtra[300];
	char *line;

	smb = (struct smb *)data;
	decoder = &SMBtable[smb->com & 255];
	if (smb->flags & SERVER_RESPONSE) {
		call_reply_detail = "SERVER RESPONSE";
		call_reply_sum = "R";
	} else {
		call_reply_detail =	"CLIENT REQUEST";
		call_reply_sum = "C";
	}
	xtra[0] = '\0';

	/*
	 * SMB Header description
	 * [X/Open-SMB, Sec. 5.1]
	 */
	if (flags & F_DTAIL) {
		show_header("SMB:  ", "SMB Header", len);
		show_space();
		sprintf(GETLINE, "%s", call_reply_detail);

		(void) sprintf(GETLINE, "Command code = 0x%x",
				smb->com);
		if (decoder->name)
			(void) sprintf(GETLINE,
				"Command name =  SMB%s", decoder->name);

		show_space();
		sprintf(GETLINE, "SMB Status:");

		/* Error classes [X/Open-SMB, Sec. 5.6] */
		switch (smb->rcls) {
		case 0x00:
			sprintf(GETLINE,
				"   - Error class = No error");
			break;
		case 0x01:
			sprintf(GETLINE,
				"   - Error class = Operating System");
			break;
		case 0x02:
			sprintf(GETLINE,
				"   - Error class = LMX server");
			break;
		case 0x03:
			sprintf(GETLINE,
				"   - Error class = Hardware");
			break;
		case 0xff:
			sprintf(GETLINE,
				"   - Error class = Incorrect format.");
			break;
		}

		if (smb->err[0] != 0x00) {
			sprintf(GETLINE,
				"   - Error code = %x", smb->err[0]);
		} else
			sprintf(GETLINE, "   - Error code = No error");

		show_space();

		sprintf(GETLINE, "Header:");
		sprintf(GETLINE, "   - Tree ID      (TID) = 0x%.4x",
			get2(smb->tid));
		sprintf(GETLINE, "   - Process ID   (PID) = 0x%.4x",
			get2(smb->pid));
		sprintf(GETLINE, "   - User ID      (UID) = 0x%.4x",
			get2(smb->uid));
		sprintf(GETLINE, "   - Multiplex ID (MID) = 0x%.4x",
			get2(smb->mid));
		sprintf(GETLINE, "   - Flags summary = 0x%.2x",
					smb->flags);
		sprintf(GETLINE, "   - Flags2 summary = 0x%.4x",
					get2(smb->flags2));
		show_space();
	}

	if (decoder->func)
		(decoder->func)(flags, (uchar_t *)data, len, xtra);
	else
		interpret_default(flags, (uchar_t *)data, len, xtra);

	if (flags & F_SUM) {
		line = get_sum_line();
		if (decoder->name)
			sprintf(line,
			"SMB %s Code=0x%x Name=SMB%s %sError=%x ",
			call_reply_sum, smb->com, decoder->name, xtra,
			smb->err[0]);

		else sprintf(line, "SMB %s Code=0x%x Error=%x ",
					call_reply_sum, smb->com, smb->err[0]);

		line += strlen(line);
	}

	if (flags & F_DTAIL)
		show_trailer();
}

static void
output_bytes(uchar_t *data, int bytecount)
{
	int i;
	char buff[80];
	char word[10];

	buff[0] = word[0] = '\0';
	sprintf(GETLINE, "Byte values (in hex):");
	for (i = 0; i < bytecount; i++) {
		sprintf(word, "%.2x ", data[i]);
		strcat(buff, word);
		if ((i+1)%16 == 0 || i == (bytecount-1)) {
			sprintf(GETLINE, "%s", buff);
			strcpy(buff, "");
		}
	}
}

/*
 * Based on the Unicode Standard,  http://www.unicode.org/
 * "The Unicode Standard: A Technical Introduction", June 1998
 */
static int
unicode2ascii(char *outstr, int outlen, uchar_t *instr, int inlen)
{
	int i = 0, j = 0;
	char c;

	while (i < inlen && j < (outlen-1)) {
		/* Show unicode chars >= 256 as '?' */
		if (instr[i+1])
			c = '?';
		else
			c = instr[i];
		if (c == '\0')
			break;
		outstr[j] = c;
		i += 2;
		j++;
	}
	outstr[j] = '\0';
	return (j);
}

/*
 * TRANS2 information levels
 * [X/Open-SMB, Sec. 16.1.6]
 */
static void
get_info_level(char *outstr, int value)
{

	switch (value) {
	case 1:
		sprintf(outstr, "Standard"); break;
	case 2:
		sprintf(outstr, "Query EA Size"); break;
	case 3:
		sprintf(outstr, "Query EAS from List"); break;
	case 0x101:
		sprintf(outstr, "Directory Info"); break;
	case 0x102:
		sprintf(outstr, "Full Directory Info"); break;
	case 0x103:
		sprintf(outstr, "Names Info"); break;
	case 0x104:
		sprintf(outstr, "Both Directory Info"); break;
	default:
		sprintf(outstr, "Unknown"); break;
	}
}


/*
 * Interpret TRANS2_OPEN subcommand
 * [X/Open-SMB, Sec. 16.2]
 */
/* ARGSUSED */
static void
output_trans2_open(int flags, uchar_t *data, char *xtra)
{
	int length;

	if (flags & F_SUM) {
		length = sprintf(xtra, "Open ");
		xtra += length;
	}

	if (flags & F_DTAIL) {
		show_space();
		sprintf(GETLINE, "Transact2Function = Open");
	}
}

/*
 * Interpret TRANS2_QUERY_FS subcommand
 * [X/Open-SMB, Sec. 16.5]
 */
/* ARGSUSED */
static void
output_trans2_queryfs(int flags, uchar_t *data, char *xtra)
{
	int length;

	if (flags & F_SUM) {
		length = sprintf(xtra, "QueryFSInfo ");
		xtra += length;
	}

	if (flags & F_DTAIL) {
		show_space();
		sprintf(GETLINE, "Transact2Function = QueryFSInfo");
	}
}

/*
 * Interpret TRANS2_QUERY_PATH subcommand
 * [X/Open-SMB, Sec. 16.7]
 */
/* ARGSUSED */
static void
output_trans2_querypath(int flags, uchar_t *data, char *xtra)
{
	int length;
	char filename[256];

	if (flags & F_SUM) {
		length = sprintf(xtra, "QueryPathInfo ");
		xtra += length;
		data += 6;
		(void) unicode2ascii(filename, 256, data, 512);
		sprintf(xtra, "File=%s ", filename);
	}

	if (flags & F_DTAIL) {
		show_space();
		sprintf(GETLINE, "Transact2Function = QueryPathInfo");
		sprintf(GETLINE, "InfoLevel = 0x%.4x",
			get2(data));
		data += 6;
		(void) unicode2ascii(filename, 256, data, 512);
		sprintf(GETLINE, "FileName = %s",
			filename);
	}
}

/*
 * Interpret TRANS2_SET_PATH subcommand
 * [X/Open-SMB, Sec. 16.8]
 */
/* ARGSUSED */
static void
output_trans2_setpath(int flags, uchar_t *data, char *xtra)
{
	int length;

	if (flags & F_SUM) {
		length = sprintf(xtra, "SetPathInfo ");
		xtra += length;
	}

	if (flags & F_DTAIL) {
		show_space();
		sprintf(GETLINE, "Transact2Function = SetPathInfo");
	}
}

/*
 * Interpret TRANS2_QUERY_FILE subcommand
 * [X/Open-SMB, Sec. 16.9]
 */
/* ARGSUSED */
static void
output_trans2_queryfile(int flags, uchar_t *data, char *xtra)
{
	int length;

	if (flags & F_SUM) {
		length = sprintf(xtra, "QueryFileInfo ");
		xtra += length;
		sprintf(xtra, "FileID=0x%x ", get2(data));
	}

	if (flags & F_DTAIL) {
		show_space();
		sprintf(GETLINE, "Transact2Function = QueryFileInfo");
		sprintf(GETLINE, "FileID = 0x%.4x",
			get2(data));
		data += 2;
		sprintf(GETLINE, "InfoLevel = 0x%.4x",
			get2(data));
	}
}

/*
 * Interpret TRANS2_SET_FILE subcommand
 * [X/Open-SMB, Sec. 16.10]
 */
/* ARGSUSED */
static void
output_trans2_setfile(int flags, uchar_t *data, char *xtra)
{
	int length;

	if (flags & F_SUM) {
		length = sprintf(xtra, "SetFileInfo ");
		xtra += length;
		sprintf(xtra, "FileID=0x%x ", get2(data));
	}

	if (flags & F_DTAIL) {
		show_space();
		sprintf(GETLINE, "Transact2Function = SetFileInfo");
		sprintf(GETLINE, "FileID = 0x%.4x",
			get2(data));
		data += 2;
		sprintf(GETLINE, "InfoLevel = 0x%.4x",
			get2(data));
	}
}

/*
 * Interpret TRANS2_CREATE_DIR subcommand
 * [X/Open-SMB, Sec. 16.13]
 */
/* ARGSUSED */
static void
output_trans2_createdir(int flags, uchar_t *data, char *xtra)
{
	int length;

	if (flags & F_SUM) {
		length = sprintf(xtra, "CreateDir ");
		xtra += length;
	}

	if (flags & F_DTAIL) {
		show_space();
		data += 2;
		sprintf(GETLINE, "Transact2Function = CreateDir");
	}
}

/*
 * Interpret TRANS2_FIND_FIRST subcommand
 * [X/Open-SMB, Sec. 16.3]
 */
/* ARGSUSED */
static void
output_trans2_findfirst(int flags, uchar_t *data, char *xtra)
{
	int length;
	char filename[256];
	char infolevel[100];

	if (flags & F_SUM) {
		length = sprintf(xtra, "Findfirst ");
		xtra += length;
		data += 12;
		(void) unicode2ascii(filename, 256, data, 512);
		sprintf(xtra, "File=%s ", filename);
	}

	if (flags & F_DTAIL) {
		show_space();
		sprintf(GETLINE, "Transact2Function = Findfirst");
		sprintf(GETLINE, "SearchAttributes = 0x%.4x",
			get2(data));
		data += 2;
		sprintf(GETLINE, "FindCount = 0x%.4x",
			get2(data));
		data += 2;
		sprintf(GETLINE, "FindFlags = 0x%.4x",
			get2(data));
		data += 2;
		get_info_level(infolevel, get2(data));
		sprintf(GETLINE, "InfoLevel = %s",
			infolevel);
		data += 6;
		(void) unicode2ascii(filename, 256, data, 512);
		sprintf(GETLINE, "FileName = %s",
			filename);
	}
}


/*
 * Interpret TRANS2_FIND_NEXT subcommand
 * [X/Open-SMB, Sec. 16.4]
 */
/* ARGSUSED */
static void
output_trans2_findnext(int flags, uchar_t *data, char *xtra)
{
	int length;
	char filename[256];
	char infolevel[100];

	if (flags & F_SUM) {
		length = sprintf(xtra, "Findnext ");
		xtra += length;
		data += 12;
		(void) unicode2ascii(filename, 256, data, 512);
		sprintf(xtra, "File=%s ", filename);
	}

	if (flags & F_DTAIL) {
		show_space();
		sprintf(GETLINE, "Transact2Function = Findnext");
		sprintf(GETLINE, "FileID = 0x%.4x",
			get2(data));
		data += 2;
		sprintf(GETLINE, "FindCount = 0x%.4x",
			get2(data));
		data += 2;
		get_info_level(infolevel, get2(data));
		sprintf(GETLINE, "InfoLevel = %s",
			infolevel);
		data += 2;
		sprintf(GETLINE, "FindKey = 0x%.8x",
			get4(data));
		data += 4;
		sprintf(GETLINE, "FindFlags = 0x%.4x",
			get2(data));
		data += 2;
		(void) unicode2ascii(filename, 256, data, 512);
		sprintf(GETLINE, "FileName = %s",
			filename);
	}
}

/*
 * Interpret a "Negprot" SMB
 * [X/Open-SMB, Sec. 6.1]
 */
/* ARGSUSED */
static void
interpret_negprot(int flags, uchar_t *data, int len, char *xtra)
{
	int length;
	int bytecount;
	char dialect[256];
	struct smb *smbdata;
	uchar_t *protodata;

	smbdata  = (struct smb *)data;
	protodata = (uchar_t *)data + sizeof (struct smb);
	protodata++;			/* skip wordcount */

	if (smbdata->flags & SERVER_RESPONSE) {
		if (flags & F_SUM) {
			sprintf(xtra, "Dialect#=%d ", protodata[0]);
		}
		if (flags & F_DTAIL) {
			sprintf(GETLINE, "Protocol Index = %d",
					protodata[0]);
		}
	} else {
		/*
		 * request packet:
		 * short bytecount;
		 * struct { char fmt; char name[]; } dialects
		 */
		bytecount = get2(protodata);
		protodata += 2;
		if (flags & F_SUM) {
			while (bytecount > 1) {
				length = sprintf(dialect, (char *)protodata+1);
				protodata += (length+2);
				bytecount -= (length+2);
			}
			sprintf(xtra, "LastDialect=%s ", dialect);
		}
		if (flags & F_DTAIL) {
			sprintf(GETLINE, "ByteCount = %d", bytecount);
			while (bytecount > 1) {
				length = sprintf(dialect, (char *)protodata+1);
				sprintf(GETLINE, "Dialect String = %s",
						dialect);
				protodata += (length+2);
				bytecount -= (length+2);
			}
		}
	}
}


/*
 * Interpret a "trans" SMB
 * [X/Open-SMB, Appendix B]
 */
/* ARGSUSED */
static void
interpret_trans(int flags, uchar_t *data, int len, char *xtra)
{
	int bytecount;
	int wordcount;
	int isunicode;
	char filename[256];
	struct smb *smb;
	uchar_t *transdata; /* word parameters */
	uchar_t *byteparms; /* byte parameters */

	smb  = (struct smb *)data;
	transdata = (uchar_t *)data + sizeof (struct smb);
	wordcount = *transdata++;

	byteparms = transdata + (2 * wordcount);
	bytecount = get2(byteparms);
	byteparms += 2;

	isunicode = smb->flags2[1] & 0x80;

	if (flags & F_DTAIL && !(smb->flags & SERVER_RESPONSE)) {
		/* print the word parameters */
		sprintf(GETLINE, "WordCount = %d", wordcount);
		sprintf(GETLINE, "TotalParamBytes = %d",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "TotalDataBytes = %d",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "MaxParamBytes = %d",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "MaxDataBytes = %d",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "MaxSetupWords = 0x%.2x",
			transdata[0]);
		transdata += 2;
		sprintf(GETLINE, "TransactFlagsSummary = 0x%.4x",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "TransactionTimeout = 0x%.8x",
			get4(transdata));
		transdata += 6;
		sprintf(GETLINE, "ParameterBytes = 0x%.4x",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "ParameterOffset = 0x%.4x",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "DataBytes = 0x%.4x",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "DataOffset = 0x%.4x",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "MaxSetupWords = 0x%.4x",
			get2(transdata));
		transdata += 2;

		/* print byte parameters */
		sprintf(GETLINE, "ByteCount = %d", bytecount);
		if (isunicode) {
			byteparms += 1;  /* alignment padding */
			(void) unicode2ascii(
				filename, 256, byteparms, bytecount);
		} else {
			strcpy(filename, (char *)byteparms);
		}
		sprintf(GETLINE, "FileName = %s", filename);
	}

	if (flags & F_DTAIL && smb->flags & SERVER_RESPONSE) {
		sprintf(GETLINE, "WordCount = %d", wordcount);
		sprintf(GETLINE, "TotalParamBytes = %d",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "TotalDataBytes = %d",
			get2(transdata));
		transdata += 4;
		sprintf(GETLINE, "ParameterBytes = 0x%.4x",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "ParameterOffset = 0x%.4x",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "ParameterDisplacement = 0x%.4x",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "DataBytes = 0x%.4x",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "DataOffset = 0x%.4x",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "DataDisplacement = 0x%.4x",
			get2(transdata));
		transdata += 2;
		sprintf(GETLINE, "MaxSetupWords = 0x%.4x",
			get2(transdata));
		transdata += 2;

		sprintf(GETLINE, "ByteCount = %d", bytecount);
	}
}

/*
 * Interpret a "TconX" SMB
 * [X/Open-SMB, Sec. 11.4]
 */
/* ARGSUSED */
static void
interpret_tconX(int flags, uchar_t *data, int len, char *xtra)
{
	int length;
	int bytecount;
	int passwordlength;
	int wordcount;
	char tempstring[256];
	struct smb *smbdata;
	uchar_t *tcondata;

	smbdata  = (struct smb *)data;
	tcondata = (uchar_t *)data + sizeof (struct smb);
	wordcount = *tcondata++;

	if (flags & F_SUM && !(smbdata->flags & SERVER_RESPONSE)) {
		tcondata += 6;
		passwordlength = get2(tcondata);
		tcondata = tcondata + 4 + passwordlength;
		length = sprintf(tempstring, (char *)tcondata);
		sprintf(xtra, "Share=%s ", tempstring);
	}

	if (flags & F_SUM && smbdata->flags & SERVER_RESPONSE) {
		tcondata += 8;
		length = sprintf(tempstring, (char *)tcondata);
		sprintf(xtra, "Type=%s ", tempstring);
	}

	if (flags & F_DTAIL && !(smbdata->flags & SERVER_RESPONSE)) {
		sprintf(GETLINE, "WordCount = %d", wordcount);
		sprintf(GETLINE, "ChainedCommand = 0x%.2x",
			tcondata[0]);
		tcondata += 2;
		sprintf(GETLINE, "NextOffset = 0x%.4x",
			get2(tcondata));
		tcondata += 2;
		sprintf(GETLINE, "DisconnectFlag = 0x%.4x",
			get2(tcondata));
		tcondata += 2;
		passwordlength = get2(tcondata);
		sprintf(GETLINE, "PasswordLength = 0x%.4x",
			passwordlength);
		tcondata += 2;
		bytecount = get2(tcondata);
		sprintf(GETLINE, "ByteCount = %d", bytecount);
		tcondata = tcondata + 2 + passwordlength;
		length = sprintf(tempstring, (char *)tcondata);
		tcondata += (length+1);
		sprintf(GETLINE, "FileName = %s", tempstring);
		length = sprintf(tempstring, (char *)tcondata);
		tcondata += (length+1);
		sprintf(GETLINE, "ServiceName = %s", tempstring);
	}

	if (flags & F_DTAIL && smbdata->flags & SERVER_RESPONSE) {
		sprintf(GETLINE, "WordCount = %d", wordcount);
		sprintf(GETLINE, "ChainedCommand = 0x%.2x",
			tcondata[0]);
		tcondata += 2;
		sprintf(GETLINE, "NextOffset = 0x%.4x",
			get2(tcondata));
		tcondata += 2;
		sprintf(GETLINE, "OptionalSupport = 0x%.4x",
			get2(tcondata));
		tcondata += 2;
		bytecount = get2(tcondata);
		sprintf(GETLINE, "ByteCount = %d", bytecount);
		tcondata += 2;
		length = sprintf(tempstring, (char *)tcondata);
		tcondata += (length+1);
		sprintf(GETLINE, "ServiceName = %s", tempstring);
		length = sprintf(tempstring, (char *)tcondata);
		tcondata += (length+1);
		sprintf(GETLINE, "NativeFS = %s", tempstring);
	}
}

/*
 * Interpret a "SesssetupX" SMB
 * [X/Open-SMB, Sec. 11.3]
 */
/* ARGSUSED */
static void
interpret_sesssetupX(int flags, uchar_t *data, int len, char *xtra)
{
	int length;
	int bytecount;
	int passwordlength;
	int isunicode;
	int upasswordlength;
	int wordcount;
	int cap;
	char tempstring[256];
	struct smb *smbdata;
	uchar_t *setupdata;

	smbdata  = (struct smb *)data;
	setupdata = (uchar_t *)data + sizeof (struct smb);
	wordcount = *setupdata++;

	isunicode = smbdata->flags2[1] & 0x80;

	if (flags & F_SUM && !(smbdata->flags & SERVER_RESPONSE)) {
		if (wordcount != 13)
			return;
		setupdata += 14;
		passwordlength = get2(setupdata);
		setupdata += 2;
		upasswordlength = get2(setupdata);
		setupdata += 6;
		cap = get4(setupdata);
		setupdata = setupdata + 6 + passwordlength + upasswordlength;
		if (isunicode) {
			setupdata += 1;
			(void) unicode2ascii(tempstring, 256, setupdata, 256);
			sprintf(xtra, "Username=%s ", tempstring);
		} else {
			length = sprintf(tempstring, (char *)setupdata);
			sprintf(xtra, "Username=%s ", tempstring);
		}
	}

	if (flags & F_DTAIL && !(smbdata->flags & SERVER_RESPONSE)) {
		if (wordcount != 13)
			return;
		sprintf(GETLINE, "ChainedCommand = 0x%.2x",
			setupdata[0]);
		setupdata += 2;
		sprintf(GETLINE, "NextOffset = 0x%.4x",
			get2(setupdata));
		setupdata += 2;
		sprintf(GETLINE, "MaxBufferSize = 0x%.4x",
			get2(setupdata));
		setupdata += 2;
		sprintf(GETLINE, "MaxMPXRequests = %d",
			get2(setupdata));
		setupdata += 2;
		sprintf(GETLINE, "VCNumber = %d",
			get2(setupdata));
		setupdata += 2;
		sprintf(GETLINE, "SessionKey = %d",
			get4(setupdata));
		setupdata += 4;
		passwordlength = get2(setupdata);
		sprintf(GETLINE, "PasswordLength = 0x%.4x",
			passwordlength);
		setupdata += 2;
		upasswordlength = get2(setupdata);
		sprintf(GETLINE, "UnicodePasswordLength = 0x%.4x",
			upasswordlength);
		setupdata += 6;
		cap = get4(setupdata);
		sprintf(GETLINE, "Capabilities = 0x%0.8x", cap);
		setupdata += 4;
		bytecount = get2(setupdata);
		sprintf(GETLINE, "ByteCount = %d", bytecount);
		setupdata = setupdata + 2 + passwordlength + upasswordlength;
		if (isunicode) {
			setupdata++;
			length = 2*unicode2ascii(
				tempstring, 256, setupdata, 256);
			if (length == 2) {
				sprintf(GETLINE,
						"AccountName = %s", tempstring);
				sprintf(GETLINE,
						"DomainName = %s", tempstring);
				setupdata += 3;
			} else {
				setupdata += length;
				sprintf(GETLINE,
						"AccountName = %s", tempstring);
				length = 2*unicode2ascii(
					tempstring, 256, setupdata, 256);
				setupdata += length;
				sprintf(GETLINE,
						"DomainName = %s", tempstring);
			}
			length = 2*unicode2ascii(
				tempstring, 256, setupdata, 256);
			setupdata += (length+2);
			sprintf(GETLINE,
					"NativeOS = %s", tempstring);
			length = 2*unicode2ascii(
				tempstring, 256, setupdata, 256);
			sprintf(GETLINE,
					"NativeLanman = %s", tempstring);
		} else {
			length = sprintf(tempstring, (char *)setupdata);
			setupdata += (length+1);
			sprintf(GETLINE, "AccountName = %s", tempstring);
			length = sprintf(tempstring, (char *)setupdata);
			setupdata += (length+1);
			sprintf(GETLINE, "DomainName = %s", tempstring);
			length = sprintf(tempstring, (char *)setupdata);
			setupdata += (length+1);
			sprintf(GETLINE, "NativeOS = %s", tempstring);
			sprintf(tempstring, (char *)setupdata);
			sprintf(GETLINE, "NativeLanman = %s", tempstring);
		}
	}

	if (flags & F_DTAIL && smbdata->flags & SERVER_RESPONSE) {
		if (wordcount != 3)
			return;
		sprintf(GETLINE, "ChainedCommand = 0x%.2x",
			setupdata[0]);
		setupdata += 2;
		sprintf(GETLINE, "NextOffset = 0x%.4x",
			get2(setupdata));
		setupdata += 2;
		sprintf(GETLINE, "SetupAction = 0x%.4x",
			get2(setupdata));
		setupdata += 2;
		bytecount = get2(setupdata);
		sprintf(GETLINE, "ByteCount = %d", bytecount);
		setupdata += 2;
		length = sprintf(tempstring, (char *)setupdata);
		setupdata += (length+1);
		sprintf(GETLINE, "NativeOS = %s", tempstring);
		length = sprintf(tempstring, (char *)setupdata);
		setupdata += (length+1);
		sprintf(GETLINE, "NativeLanman = %s", tempstring);
		length = sprintf(tempstring, (char *)setupdata);
		sprintf(GETLINE, "DomainName = %s", tempstring);
	}
}

/*
 * Interpret "Trans2" SMB
 * [X/Open-SMB, Sec. 16]
 */
/* ARGSUSED */
static void
interpret_trans2(int flags, uchar_t *data, int len, char *xtra)
{
	int paramoffset;
	int bytecount;
	int trans2code;
	int wordcount;
	struct smb *smbdata;
	uchar_t *trans2data;
	uchar_t *trans2datasum;

	smbdata  = (struct smb *)data;
	trans2data = (uchar_t *)data + sizeof (struct smb);
	wordcount = *trans2data++;

	trans2datasum = trans2data + 28;

	if (flags & F_SUM) {
		trans2datasum = trans2data+20;
		paramoffset = get2(trans2datasum);
		trans2datasum += 8;
		trans2code = get2(trans2datasum);
		trans2datasum += 2;
		trans2data = data+paramoffset;
	}

	if (flags & F_DTAIL && !(smbdata->flags & SERVER_RESPONSE)) {
		sprintf(GETLINE, "WordCount = %d", wordcount);
		sprintf(GETLINE, "TotalParamBytes = %d",
			get2(trans2data));
		trans2data += 2;
		sprintf(GETLINE, "TotalDataBytes = %d",
			get2(trans2data));
		trans2data += 2;
		sprintf(GETLINE, "MaxParamBytes = %d",
			get2(trans2data));
		trans2data += 2;
		sprintf(GETLINE, "MaxDataBytes = %d",
			get2(trans2data));
		trans2data += 2;
		sprintf(GETLINE, "MaxSetupWords = 0x%.2x",
			trans2data[0]);
		trans2data += 2;
		sprintf(GETLINE, "TransactFlagsSummary = 0x%.4x",
			get2(trans2data));
		trans2data += 2;
		sprintf(GETLINE, "TransactTimeout = 0x%.8x",
			get4(trans2data));
		trans2data += 6;
		sprintf(GETLINE, "ParameterBytes = 0x%.4x",
			get2(trans2data));
		trans2data += 2;
		paramoffset = get2(trans2data);
		sprintf(GETLINE, "ParameterOffset = 0x%.4x",
			paramoffset);
		trans2data += 2;
		sprintf(GETLINE, "DataBytes = 0x%.4x",
			get2(trans2data));
		trans2data += 2;
		sprintf(GETLINE, "DataOffset = 0x%.4x",
			get2(trans2data));
		trans2data += 2;
		sprintf(GETLINE, "MaxSetupWords = %d",
			trans2data[0]);
		trans2data += 2;
		trans2code = get2(trans2data);
		trans2data += 2;
		bytecount = get2(trans2data);
		trans2data += 2;

		sprintf(GETLINE, "Transact2FunctionCode = 0x%.4x",
			trans2code);
		sprintf(GETLINE, "ByteCount = %d", bytecount);
		trans2data = data+paramoffset;
	}

	if (!(smbdata->flags & SERVER_RESPONSE)) {
		switch (trans2code) {
		case TRANS2_OPEN:
			output_trans2_open(flags,
						trans2data, xtra);
			break;
		case TRANS2_FIND_FIRST:
			output_trans2_findfirst(flags,
						trans2data, xtra);
			break;
		case TRANS2_FIND_NEXT2:
			output_trans2_findnext(flags,
						trans2data, xtra);
			break;
		case TRANS2_QUERY_FS_INFORMATION:
			output_trans2_queryfs(flags,
						trans2data, xtra);
			break;
		case TRANS2_QUERY_PATH_INFORMATION:
			output_trans2_querypath(flags,
						trans2data, xtra);
			break;
		case TRANS2_SET_PATH_INFORMATION:
			output_trans2_setpath(flags,
						trans2data, xtra);
			break;
		case TRANS2_QUERY_FILE_INFORMATION:
			output_trans2_queryfile(flags,
						trans2data, xtra);
			break;
		case TRANS2_SET_FILE_INFORMATION:
			output_trans2_setfile(flags,
						trans2data, xtra);
			break;
		case TRANS2_CREATE_DIRECTORY:
			output_trans2_createdir(flags,
						trans2data, xtra);
			break;
		}
	}

	if (flags & F_DTAIL && smbdata->flags & SERVER_RESPONSE) {
		sprintf(GETLINE, "WordCount = %d", wordcount);
		sprintf(GETLINE, "TotalParamBytes = %d",
			get2(trans2data));
		trans2data += 2;
		sprintf(GETLINE, "TotalDataBytes = %d",
			get2(trans2data));
		trans2data += 4;
		sprintf(GETLINE, "ParameterBytes = 0x%.4x",
			get2(trans2data));
		trans2data += 2;
		sprintf(GETLINE, "ParameterOffset = 0x%.4x",
			get2(trans2data));
		trans2data += 2;
		sprintf(GETLINE, "ParameterDisplacement = 0x%.4x",
			get2(trans2data));
		trans2data += 2;
		sprintf(GETLINE, "DataBytes = 0x%.4x",
			get2(trans2data));
		trans2data += 2;
		sprintf(GETLINE, "DataOffset = 0x%.4x",
			get2(trans2data));
		trans2data += 2;
		sprintf(GETLINE, "DataDisplacement = 0x%.4x",
			get2(trans2data));
		trans2data += 2;
		sprintf(GETLINE, "MaxSetupWords = 0x%.2x",
			trans2data[0]);
		trans2data += 2;
		bytecount = get2(trans2data);
		sprintf(GETLINE, "ByteCount = %d", bytecount);
		trans2data += 2;
		output_bytes(data, bytecount);
	}
}


static void
interpret_default(int flags, uchar_t *data, int len, char *xtra)
{
	int slength;
	int i;
	int printit;
	int wordcount;
	char *outstr;
	char *prfmt;
	char *format;
	char valuetype;
	char word[10];
	char *label;
	char tempstring[256];
	uchar_t *comdata, *limit;
	char buff[80];
	struct smb *smbdata;
	struct decode *decoder;

	smbdata  = (struct smb *)data;
	comdata = (uchar_t *)data + sizeof (struct smb);
	wordcount = *comdata++;
	limit = data + len;

	decoder = &SMBtable[smbdata->com & 255];

	if (smbdata->flags & SERVER_RESPONSE)
		format = decoder->replyfmt;
	else
		format = decoder->callfmt;

	if (!format || strlen(format) == 0) {
		if (wordcount == 0 || flags & F_SUM)
			return;
		sprintf(GETLINE, "WordCount = %d", wordcount);
		sprintf(GETLINE, "Word values (in hex):");
		for (i = 0; i < wordcount; i++) {
			sprintf(word, "%.4x ", get2(comdata));
			comdata += 2;
			if (comdata >= limit)
				wordcount = i+1; /* terminate */
			strcat(buff, word);
			if (((i+1) & 7) == 0 || i == (wordcount-1)) {
				sprintf(GETLINE, "%s", buff);
				strcpy(buff, "");
			}
		}
		return;
	}


	valuetype = format[0];
	while (valuetype != '\0') {
		if (comdata >= limit)
			break;
		if ((flags & F_DTAIL) && valuetype != 'r' && valuetype != 'R')
			outstr = GETLINE;
		else
			outstr = xtra + strlen(xtra);
		label = format+1;
		printit = (flags & F_DTAIL) || (valuetype <= 'Z');

		switch (valuetype) {
		case 'W':
		case 'w':
			prfmt = (flags & F_DTAIL) ? "%s = 0x%.4x" : "%s=0x%x ";
			if (printit)
				sprintf(outstr, prfmt, label, get2(comdata));
			comdata += 2;
			break;
		case 'D':
		case 'd':
			prfmt = (flags & F_DTAIL) ? "%s = %d" : "%s=%d ";
			if (printit)
				sprintf(outstr, prfmt, label, get2(comdata));
			comdata += 2;
			break;
		case 'L':
		case 'l':
			prfmt = (flags & F_DTAIL) ? "%s = 0x%.8x" : "%s=0x%x ";
			if (printit)
				sprintf(outstr, prfmt, label, get4(comdata));
			comdata += 4;
			break;
		case 'B':
		case 'b':
			prfmt = (flags & F_DTAIL) ? "%s = 0x%.2x" : "%s=0x%x ";
			if (printit)
				sprintf(outstr, prfmt, label, comdata[0]);
			comdata += 1;
			break;
		case 'r':
			comdata++;
			break;
		case 'R':
			comdata += 2;
			break;
		case 'U':
		case 'u':
			prfmt = (flags & F_DTAIL) ? "%s = %s" : "%s=%s ";
			slength = unicode2ascii(tempstring, 256, comdata, 256);
			if (printit)
				sprintf(outstr, prfmt, label, tempstring);
			comdata +=  (slength*2 + 1);
			break;
		case 'S':
		case 's':
			prfmt = (flags & F_DTAIL) ? "%s = %s" : "%s=%s ";
			slength = sprintf(tempstring, (char *)comdata);
			if (printit)
				sprintf(outstr, prfmt, label, tempstring);
			comdata += (slength+1);
			break;
		}
		format += (strlen(format) + 1);
		valuetype = format[0];
	}
}
