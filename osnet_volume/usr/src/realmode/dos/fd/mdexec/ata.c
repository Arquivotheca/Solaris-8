/*
 * Copyright 1995 Sun Microsystems, Inc. All Rights Reserved
 */

/* PRAGMA_PLACEHOLDER */

#include "ata.h"

/*
 * return a bitmask of ATA drives that are present and for which
 * the BIOS and CMOS are both enabled. We have to check for all these
 * because a SCSI controller may respond for drives 0x80 and
 * 0x81.
 */

static int ata_findbiosdrives(void);
static int ata_findcmosdrives(void);
static int ata_findctlrs(unsigned ioaddr1, unsigned ioaddr2);
static int ata_check_for_dpt(unsigned ioaddr1);
static int ata_wait1(unsigned short port,
	unsigned char onbits1, unsigned char offbits1, int interval);
static int ata_wait3(unsigned short port,
	unsigned char onbits1, unsigned char offbits1,
	unsigned char fail_onbits2, unsigned char fail_offbits2,
	unsigned char fail_onbits3, unsigned char fail_offbits3,
	int interval);
static unsigned char inb(unsigned short port);
static void outb(unsigned short port, unsigned char val);
static char Buf[2][512];

char	atanames[4][40] = { 0 };
short	atacount = 0;

/*
 * Doing 4 inb()'s from the alternate status register is guaranteed
 * to take at least 400 nsecs (it may take as long as 4 usecs).
 */

#define	ATA_DELAY_400NSEC(port) \
	((void)(inb(port), inb(port), inb(port), inb(port)))



char *
ata_getname(int drive)
{
	return atanames[drive];
}

int ata_find(void)
{
	unsigned char numbios, numcmos, numdrives;

	/* first check BIOS */
	numbios = ata_findbiosdrives();
	if (numbios == 0)
		return 0;

	/* next, CMOS */
	numcmos = ata_findcmosdrives();
	if (numcmos == 0)
		return 0;

	/* finally, check for controllers */
	numdrives = ata_findctlrs(ATA_PRIMARY_IO1, ATA_PRIMARY_IO2);
	if (numdrives & 1)
		set_ataname(Buf[0], atanames[atacount++]);
	if (numdrives & 2)
		set_ataname(Buf[1], atanames[atacount++]);
	numdrives <<= 2;
	numdrives |= ata_findctlrs(ATA_SECONDARY_IO1, ATA_SECONDARY_IO2);
	if (numdrives & 1)
		set_ataname(Buf[0], atanames[atacount++]);
	if (numdrives & 2)
		set_ataname(Buf[1], atanames[atacount++]);
	numdrives = compress(numdrives);
	return (numbios & numdrives);
}

compress(numdrives)
{
	int     bitseen = 0, i;

	for (i = 0; i < 4; i++)
		if (numdrives & (1 << i))
			bitseen++;
	return ((1 << bitseen) - 1);
}

#define	ATA_ID_NM_OSET	27
#define	ATA_ID_REV_OSET	23
#define	ATA_ID_REV_LEN	8
#define	ATA_ID_NM_LEN	40
#define	MENU_VENDOR_LEN	22

static int
set_ataname(char *buf, char *name)
{
	int	i;
	char	*sp, c;

	sp = &buf[ATA_ID_REV_OSET << 1];

	for (i = 0; i < ATA_ID_REV_LEN+ATA_ID_NM_LEN; i += 2) {
		c = sp[i];
		sp[i] = sp[i+1];
		sp[i+1] = c;
	}

	i = ATA_ID_REV_LEN+MENU_VENDOR_LEN;
	sp[i++] = ' ';

	for (i = 0; i < 8; i++)
		sp[i+ATA_ID_REV_LEN+MENU_VENDOR_LEN+1] = sp[i];

	sp[i+ATA_ID_REV_LEN+MENU_VENDOR_LEN+1] = '\0';
	sp = &buf[ATA_ID_NM_OSET << 1];
	strcpy(name, sp);
}

static int ata_findbiosdrives(void)
{
	unsigned char _far *bios_numdrives = (char _far *)0x00400075;

	/* remember, we return a bitmask */
	return (1 << (*bios_numdrives)) - 1;
}

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71
#define CMOS_ADDR_HD_INFO 0x12

static int ata_findcmosdrives(void)
{
	unsigned int hd_data;
	unsigned char numcmos;

	numcmos = 0;
	outb(CMOS_ADDR, CMOS_ADDR_HD_INFO);
	hd_data = inb(CMOS_DATA);
	if (hd_data & 0xF0)
		numcmos |= 1;
	if (hd_data & 0x0F)
		numcmos |= 2;
	return numcmos;
}

static int ata_findctlrs(unsigned ioaddr1, unsigned ioaddr2)
{
	int	drive;
	int	drives_found = 0;
	int	i;

	/*
	 * Check for a conflict with a DPT adapter which can also
	 * occupy these locations.
	 * The DPT identifies itself.
	 */
	if (ata_check_for_dpt(ioaddr1))
		return(0);

	for (drive = 0; drive < 2; drive++) {
		/*
		 * load up with the drive number
		 */
		DriveSelect(ioaddr1, drive);
		ATA_DELAY_400NSEC(ioaddr2);

		if (ata_wait3(ioaddr1 + AT_STATUS,
			      ATS_DRDY, ATS_BSY, 0xff, 0, 0, 0xff, 50))
			continue;

		if (ata_identify(ioaddr1, ioaddr2, Buf[drive]) == 0) {
			/*
			 * got it..
			 * set a bit in the return code
			 */
			drives_found |= (1 << drive);
		}
	}
	return (drives_found);
}

#define	HA_AUX_STATUS	0x08

/*
 * Check for a DPT adapter at our address.  if there is such a beast,
 * return 1.  If not, return 0.
 */
static int
ata_check_for_dpt(unsigned ioaddr1)
{
	unsigned char	v;

	v = inb(ioaddr1 + HA_AUX_STATUS);
	if (v == 0xfc || v == 0xfd || v == 0xfe)
		return(1);
	return(0);
}

static int
ata_wait1(unsigned short port, unsigned char onbits, unsigned char offbits,
	       int interval)
{
	int i;
	unsigned char val;

	for (i = interval; i; i--) {
		val = inb(port);
		if (((val & onbits) == onbits) 
		&&  ((val & offbits) == 0))
			return (0);
		pause_ms(1);
	}
	return (1);
}


static int
ata_wait3(unsigned short port, unsigned char onbits1, unsigned char offbits1,
	       unsigned char fail_onbits2, unsigned char fail_offbits2,
	       unsigned char fail_onbits3, unsigned char fail_offbits3,
	       int interval)
{
	int i;
	unsigned char val;

	for (i = interval; i; i--) {
		val = inb(port);
		if (((val & onbits1) == onbits1) 
		&&  ((val & offbits1) == 0))
			return (0);
		if (((val & fail_onbits2) == fail_onbits2) 
		&&  ((val & fail_offbits2) == 0))
			return (1);
		if (((val & fail_onbits3) == fail_onbits3) 
		&&  ((val & fail_offbits3) == 0))
			return (1);
		pause_ms(1);
	}
	return (1);
}

static unsigned char inb(unsigned short port)
{
	unsigned char retval;

	_asm {
		mov dx, port
		in al, dx
		mov retval, al
	}
	return retval;
}

static unsigned short inw(unsigned short port)
{
	unsigned short retval;

	_asm {
		mov dx, port
		in ax, dx
		mov retval, ax
	}
	return retval;
}


static void outb(unsigned short port, unsigned char val)
{
	_asm {
		mov dx, port
		mov al, val
		out dx, al
	}
}

static ata_identify(int Base, int Base2, short *p);
static DataIn(int Base, short *p);
static Cmd(int Base, int cmd);
static DriveSelect(int Base, int drive);
static Status(int Base);

static
ata_identify(int Base, int Base2, short *p)
{
	long	wait = 0;
	int	status;

	Cmd(Base, ATC_READPARMS);
	ATA_DELAY_400NSEC(Base2);

	ata_wait3(Base2, 0, ATS_BSY, ATS_ERR, ATS_BSY, 0x7f, 0, 4000);

	status = Status(Base);

	if (status == 0xff || status == 0x7f)
		return (-1);

	if (status & ATS_BSY)
		return (-1);

	if (!(status & ATS_DRQ)) {
		/* don't wait if the command aborted */
		if (status & (ATS_ERR | ATS_DF))
			return (-1);
		if (ata_wait1(Base2, ATS_DRQ, ATS_BSY, 1000))
			return (-1);
	}
		
	DataIn(Base, p);
	ATA_DELAY_400NSEC(Base2);

	ata_wait1(Base2, ATS_DRDY, (ATS_DRQ | ATS_BSY), 5);

	if (status & (ATS_ERR | ATS_DF))
		return (-1);

	/* check the config word in the response for the "disk" type */
	if ((p[0] & 0x8001) != 0)
		return (-1);

	return (0);
}

static
DataIn(int Base, short *p)
{
	int	n;

	for (n = 0; n < 256; n++)
		p[n] = inw(Base);
}


static
Cmd(int Base, int cmd)
{
	outb(Base+AT_CMD, (unsigned char)cmd);
}


static
DriveSelect(int Base, int drive)
{
	unsigned char reg = ATDH_DRIVE0;

	drive <<= 4;
	reg |= drive;
	outb(Base+AT_DRVHD, reg);
}

static
Status(int Base)
{
	return inb(Base + AT_CMD);
}
