#!smake

default: cdtodat dodat verifydat

verifydat: verifydat.o
	cc verifydat.o -o verifydat \
             -ldataudio -lm -laudio -lds -lmediad

cdtodat: cdtodat.o
	cc cdtodat.o -o cdtodat \
             -ldataudio -lcdaudio -lm -laudio -lds -lmediad

dodat: lex.yy.o y.tab.o dodat.o
	cc dodat.o lex.yy.o y.tab.o -o dodat \
             -ldataudio -lm -laudio -lmediad -laudiofile

lex.yy.c: commands.l y.tab.c
	lex commands.l

y.tab.c: commands.y
	yacc -d commands.y

clean:
	rm -rf *.[ou] a.out core lex.yy.[co] y.tab.[cho] _force ar.tmp.* 

clobber: clean
	rm -rf verifydat cdtodat dodat 

