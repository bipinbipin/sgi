%{
#include "y.tab.h"
extern int yylval;
char last_string[81];
%}
%%
quit		return(QUIT);
leadin		return(LEADIN);
leadout		return(LEADOUT);
bot		return(BOT);
eot		return(EOT);
rate		return(RATE);
prohibit	return(COPY);
program		return(PROGRAM);
index		return(INDEX);
pause		return(PAUSE);
smart		return(SMART);
silence		return(SILENCE);
testpat		return(TESTPAT);
write		return(FWRITE);
read		return(FREAD);
where		return(WHERE);
readpos		return(WHERENOT);
readframe	return(READFRAME);
seek		return(SEEK);
[0-9]+		{ yylval = atoi(yytext); return(INTEGER); }
y		{ yylval = 1; return(BOOLEAN); }
yes		{ yylval = 1; return(BOOLEAN); }
n		{ yylval = 0; return(BOOLEAN); }
no		{ yylval = 0; return(BOOLEAN); }
pn		return(PROGNUM);
at		return(ATIME);
pt		return(PTIME);
:		return(COLON);
[a-zA-Z0-9_.!@#$%^&\*()+=~/<>]+	{ 
		strncpy(last_string, yytext, 80);
		last_string[80] = 0;
		return(STRING);
	}
#[a-zA-Z0-9_.,`|:;\?{}!@#$%^&\*()+=~/<> '"\-]*$	return(COMMENT);
