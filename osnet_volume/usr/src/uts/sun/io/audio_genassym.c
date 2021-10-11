/*
 * Copyright (c) 1990, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)audio_genassym.c	1.3	97/06/05 SMI"

#ifndef _GENASSYM
#define	_GENASSYM
#endif

#define	SIZES	1

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/dditypes.h>

#include <sys/audioio.h>
#include <sys/audiovar.h>
#include <sys/audio_79C30.h>

#define	OFFSET(type, field)	((int)(&((type *)0)->field))


main()
{
	register aud_cmd_t *cmdp = (struct aud_cmd *)0;
	register amd_unit_t *unitp = (amd_unit_t *)0;
	register struct aud_79C30_chip *chip = (struct aud_79C30_chip *)0;

	printf("#define\tAUD_CMD_DATA 0x%p\n", &cmdp->data);
	printf("#define\tAUD_CMD_ENDDATA 0x%p\n", &cmdp->enddata);
	printf("#define\tAUD_CMD_NEXT 0x%p\n", &cmdp->next);
	printf("#define\tAUD_CMD_SKIP 0x%p\n", &cmdp->skip);
	printf("#define\tAUD_CMD_DONE 0x%p\n", &cmdp->done);
	printf("#define\tAUD_DEV_CHIP 0x%p\n", &unitp->chip);
	printf("#define\tAUD_PLAY_CMDPTR 0x%p\n", &unitp->output.cmdptr);
	printf("#define\tAUD_PLAY_SAMPLES 0x%p\n", &unitp->output.samples);
	printf("#define\tAUD_PLAY_ACTIVE 0x%p\n", &unitp->output.active);
	printf("#define\tAUD_PLAY_ERROR 0x%p\n", &unitp->output.error);
	printf("#define\tAUD_REC_CMDPTR 0x%p\n", &unitp->input.cmdptr);
	printf("#define\tAUD_REC_SAMPLES 0x%p\n", &unitp->input.samples);
	printf("#define\tAUD_REC_ACTIVE 0x%p\n", &unitp->input.active);
	printf("#define\tAUD_REC_ERROR 0x%p\n", &unitp->input.error);
	printf("#define\tAUD_CHIP_IR 0x%p\n", &chip->ir);
	printf("#define\tAUD_CHIP_CR 0x%p\n", &chip->cr);
	printf("#define\tAUD_CHIP_DR 0x%p\n", &chip->dr);
	printf("#define\tAUD_CHIP_BBRB 0x%p\n", &chip->bbrb);
	printf("#define\tAUD_CHIP_INIT_REG 0x%x\n",
	    AUDIO_UNPACK_REG(AUDIO_INIT_INIT));
	printf("#define\tAUD_CHIP_DISABLE 0x%x\n",
	    AUDIO_INIT_BITS_ACTIVE | AUDIO_INIT_BITS_INT_DISABLED);

	/*
	 * Gross hack... Although genassym is a user program and hence
	 * exit has one parameter, it is compiled with the kernel headers
	 * and the _KERNEL define so ANSI-C thinks it should have two!
	 */
	exit(0, 0);
}
