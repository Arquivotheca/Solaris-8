#
#ident "@(#)Makefile 1.6 93/11/10"
#
SUFFIXES = .tex .dvi .ps

.SUFFIXES: $(SUFFIXES)

.KEEP_STATE:

.tex.ps:
	latex $<
	dvips -f $*.dvi > $*.ps

.tex.dvi:
	latex $<

.dvi.ps:
	dvips -f $< > $@

all:	$(OBJS)

clean:
	rm *.aux *.dvi *.log
