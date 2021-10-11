#include <dos.h>
#include <stdio.h>

union REGS r;

main()
{
	int	cyls;

	r.h.ah = 0x41;
	r.x.bx = 0x55AA;
	r.h.dl = 0;
	int86(0x13, &r, &r);

	if (r.x.bx != 0xAA55 || r.x.cflag) {
		printf("INT 13 BIOS extensions not present\n"); 
	} else {
		printf("INT 13 BIOS extensions, version 0x%x.\n", r.h.ah);
		if (r.x.cx & 1) {
			printf("Extended disk access support\n");
		}
		if (r.x.cx & 2) {
			printf("Removable media support\n");
		}
	}
	checkdrive(0x80);
	checkdrive(0x81);
	checkdrive(0x82);
	checkdrive(0x83);
}

checkdrive(driveid)
{
	int cyls;

	r.h.ah = 0x8;
	r.h.dl = driveid;
	int86(0x13, &r, &r);

	if (r.x.cflag || r.x.ax) {
		printf("error getting drive info: %d\n", r.x.ax);
		return;
	}
	
	cyls = r.h.cl & ~0x3f;
	cyls = cyls << 2 | r.h.ch;
	printf("cylinders: %d    heads: %d    sectors: %d\n",
			cyls, r.h.dh, r.h.cl & 0x3f);
}
