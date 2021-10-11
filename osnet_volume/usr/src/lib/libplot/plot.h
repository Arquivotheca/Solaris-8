/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#ifndef	_PLOT_H
#define	_PLOT_H

#pragma ident	"@(#)plot.h	1.2	97/11/05 SMI"	/* SVr4.0 1.2	*/

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	__STDC__

extern	void arc(short, short, short, short, short, short);
extern	void box(short, short, short, short);
extern	void circle(short, short, short);
extern	void closepl(void);
extern	void closevt(void);
extern	void cont(short, short);
extern	void erase(void);
extern	void label(char *);
extern	void line(short, short, short, short);
extern	void linmod(char *);
extern	void move(short, short);
extern	void openpl(void);
extern	void openvt(void);
extern	void point(short, short);
extern	void space(short, short, short, short);

#else

extern	void arc();
extern	void box();
extern	void circle();
extern	void closepl();
extern	void closevt();
extern	void cont();
extern	void erase();
extern	void label();
extern	void line();
extern	void linmod();
extern	void move();
extern	void openpl();
extern	void openvt();
extern	void point();
extern	void space();

#endif /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* _PLOT_H */
