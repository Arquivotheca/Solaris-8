/*
 * rcvbundl.h
 *
 * Author:  Patrick J Luhmann (PJL)
 * Date:    05/26/99
 *
 * This file contains the loadable micro code arrays to implement receive
 * bundling on the 82558 A-step, 82558 B-step, 82559 B-step, and 82559 C-step.
 *
 */

#ifndef	_RCVBUNDL_H
#define	_RCVBUNDL_H

#pragma ident	"@(#)rcvbundl.h	1.1	99/07/19 SMI"


#ifdef __cplusplus
extern "C" {
#endif


/*
 *  CPUSaver parameters
 *
 *  All CPUSaver parameters are 16-bit literals that are part of a
 *  "move immediate value" instruction.  By changing the value of
 *  the literal in the instruction before the code is loaded, the
 *  driver can change algorithm.
 *
 *  CPUSAVER_DWORD - This is the location of the instruction that loads
 *    the dead-man timer with its inital value.  By writing a 16-bit
 *    value to the low word of this instruction, the driver can change
 *    the timer value.  The current default is either x600 or x800;
 *    experiments show that the value probably should stay within the
 *    range of x200 - x1000.
 *
 *  CPUSAVER_BUNDLE_MAX_DWORD - This is the location of the instruction
 *    that sets the maximum number of frames that will be bundled.  In
 *    some situations, such as the TCP windowing algorithm, it may be
 *    better to limit the growth of the bundle size than let it go as
 *    high as it can, because that could cause too much added latency.
 *    The default is six, because this is the number of packets in the
 *    default TCP window size.  A value of 1 would make CPUSaver indicate
 *    an interrupt for every frame received.  If you do not want to put
 *    a limit on the bundle size, set this value to xFFFF.
 */

/*  This value is the same for both A and B step of 558. */
#define	D101_CPUSAVER_DWORD			72


/*  Parameter values for the D101M B-step */
#define	D101M_CPUSAVER_DWORD			78
#define	D101M_CPUSAVER_BUNDLE_MAX_DWORD		65
/*
 * This feature not yet implemented
 *	#define	D101M_CPUSAVER_MIN_SIZE_DWORD       15
 */


/*  Parameter values for the D101S A-step */
#define	D101S_CPUSAVER_DWORD			78
#define	D101S_CPUSAVER_BUNDLE_MAX_DWORD		67
/*
 * This feature not yet implemented
 *	#define	D101S_CPUSAVER_MIN_SIZE_DWORD	133
 */


#define	D101_A_RCVBUNDLE_UCODE \
{\
0x03B301BB, \
0x0046FFFF, \
0xFFFFFFFF, \
0x051DFFFF, \
0xFFFFFFFF, \
0xFFFFFFFF, \
0x000C0001, \
0x00101212, \
0x000C0008, \
0x003801BC, \
0x00000000, \
0x00124818, \
0x000C1000, \
0x00220809, \
0x00010200, \
0x00124818, \
0x000CFFFC, \
0x003803B5, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x0010009C, \
0x0024B81D, \
0x00130836, \
0x000C0001, \
0x0026081C, \
0x0020C81B, \
0x00130824, \
0x00222819, \
0x00101213, \
0x00041000, \
0x003A03B3, \
0x00010200, \
0x00101B13, \
0x00238081, \
0x00213049, \
0x0038003B, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x0010009C, \
0x0024B83E, \
0x00130826, \
0x000C0001, \
0x0026083B, \
0x00010200, \
0x00134824, \
0x000C0001, \
0x00101213, \
0x00041000, \
0x0038051E, \
0x00101313, \
0x00010400, \
0x00380521, \
0x00050600, \
0x00100824, \
0x00101310, \
0x00041000, \
0x00080600, \
0x00101B10, \
0x0038051E, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
}


#define	D101_B0_RCVBUNDLE_UCODE \
{\
0x03B401BC, \
0x0047FFFF, \
0xFFFFFFFF, \
0x051EFFFF, \
0xFFFFFFFF, \
0xFFFFFFFF, \
0x000C0001, \
0x00101B92, \
0x000C0008, \
0x003801BD, \
0x00000000, \
0x00124818, \
0x000C1000, \
0x00220809, \
0x00010200, \
0x00124818, \
0x000CFFFC, \
0x003803B6, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x0010009C, \
0x0024B81D, \
0x0013082F, \
0x000C0001, \
0x0026081C, \
0x0020C81B, \
0x00130837, \
0x00222819, \
0x00101B93, \
0x00041000, \
0x003A03B4, \
0x00010200, \
0x00101793, \
0x00238082, \
0x0021304A, \
0x0038003C, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x0010009C, \
0x0024B83E, \
0x00130826, \
0x000C0001, \
0x0026083B, \
0x00010200, \
0x00134837, \
0x000C0001, \
0x00101B93, \
0x00041000, \
0x0038051F, \
0x00101313, \
0x00010400, \
0x00380522, \
0x00050600, \
0x00100837, \
0x00101310, \
0x00041000, \
0x00080600, \
0x00101790, \
0x0038051F, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
}


#define	D101M_B_RCVBUNDLE_UCODE \
{\
0x00550215, \
0xFFFF0437, \
0xFFFFFFFF, \
0x06A70789, \
0xFFFFFFFF, \
0x0558FFFF, \
0x000C0001, \
0x00101312, \
0x000C0008, \
0x00380216, \
0x0010009C, \
0x00204056, \
0x002380CC, \
0x00380056, \
0x0010009C, \
0x00244C0B, \
0x00000800, \
0x00124818, \
0x00380438, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x0010009C, \
0x00244C2D, \
0x00010004, \
0x00041000, \
0x003A0437, \
0x00044010, \
0x0038078A, \
0x00000000, \
0x00100099, \
0x00206C64, \
0x0010009C, \
0x00244C48, \
0x00130824, \
0x000C0001, \
0x00101213, \
0x002606AB, \
0x00041000, \
0x00010004, \
0x00130826, \
0x000C0006, \
0x002206A8, \
0x0013C926, \
0x00101313, \
0x003806A8, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00080800, \
0x00101B10, \
0x00050004, \
0x00100826, \
0x00101210, \
0x00380C34, \
0x00000000, \
0x00000000, \
0x0021155B, \
0x00100099, \
0x00206559, \
0x0010009C, \
0x00244559, \
0x00130836, \
0x000C0000, \
0x00220C66, \
0x000C0001, \
0x00101B13, \
0x00229C62, \
0x00210C62, \
0x00226C62, \
0x00216C62, \
0x0022FC62, \
0x00215C62, \
0x00214C62, \
0x00380555, \
0x00140000, \
0x00380555, \
0x00041000, \
0x003806A8, \
0x00010004, \
0x00041000, \
0x00278C6B, \
0x00040800, \
0x00018100, \
0x003A0437, \
0x00130826, \
0x000C0001, \
0x00220559, \
0x00101313, \
0x00380559, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
}


#define	D101S_RCVBUNDLE_UCODE \
{\
0x00550242, \
0xFFFF047E, \
0xFFFFFFFF, \
0x06FD0814, \
0xFFFFFFFF, \
0x05A6FFFF, \
0x000C0001, \
0x00101312, \
0x000C0008, \
0x00380243, \
0x0010009C, \
0x00204056, \
0x002380D0, \
0x00380056, \
0x0010009C, \
0x00244F8B, \
0x00000800, \
0x00124818, \
0x0038047F, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x0010009C, \
0x00244FAD, \
0x00010004, \
0x00041000, \
0x003A047E, \
0x00044010, \
0x00380815, \
0x00000000, \
0x00100099, \
0x00206FFB, \
0x0010009A, \
0x0020AFFB, \
0x0010009C, \
0x00244FC8, \
0x00130824, \
0x000C0001, \
0x00101213, \
0x00260701, \
0x00041000, \
0x00010004, \
0x00130826, \
0x000C0006, \
0x002206FE, \
0x0013C926, \
0x00101313, \
0x003806FE, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00080600, \
0x00101B10, \
0x00050004, \
0x00100826, \
0x00101210, \
0x00380FB6, \
0x00000000, \
0x00000000, \
0x002115A9, \
0x00100099, \
0x002065A7, \
0x0010009A, \
0x0020A5A7, \
0x0010009C, \
0x002445A7, \
0x00130836, \
0x000C0000, \
0x00220FE4, \
0x000C0001, \
0x00101B13, \
0x00229FFD, \
0x00210FFD, \
0x00226FFD, \
0x00216FFD, \
0x0022FFFD, \
0x00215FFD, \
0x00214FFD, \
0x003805A3, \
0x00010004, \
0x00041000, \
0x00278FE9, \
0x00040800, \
0x00018100, \
0x003A047E, \
0x00130826, \
0x000C0001, \
0x002205A7, \
0x00101313, \
0x003805A7, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00041000, \
0x003806FE, \
0x00140000, \
0x003805A3, \
0x00000000, \
}

#ifdef __cplusplus
}
#endif

#endif	/* _RCVBUNDL_H */
