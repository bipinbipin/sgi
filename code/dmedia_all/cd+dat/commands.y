%token LEADIN LEADOUT BOT EOT RATE COPY PROGRAM INDEX PAUSE SILENCE FREAD FWRITE SEEK INTEGER BOOLEAN PROGNUM ATIME PTIME COLON WHERE WHERENOT READFRAME QUIT STRING SMART TESTPAT COMMENT 
%{
extern int dat;
extern int smart;
static int h,m,s,f;
static int p;
extern char last_string[];
%}
%%
script:	command |
	script command 
	;
command:	leadin | leadout | rate | copy | program | index | pause | silence | seek | where | wherenot | atime | ptime | readframe | quit | write | read | smart | testpat | comment ;

quit: QUIT
	{
		quit();
	}
	;
where:	WHERE
	{
		get_tape_pos(dat,1);
	}
	;
wherenot:	WHERENOT
	{
		get_wrongDATtime(dat);
	}
	;
readframe:	READFRAME
	{
		get_tape_pos(dat,0);
	}
	;
leadin:	LEADIN
	{
		write_bot(dat);
	}
	;
leadout: LEADOUT
	{
		write_eot(dat);
	}
	;
rate:	RATE INTEGER
	{
		set_rate(yylval);
	}
	;
copy:	COPY BOOLEAN
	{
		copy_prohibit(yylval);
	}
	;

read:	FREAD STRING INTEGER
	{
		read_file(dat,last_string,yylval,0,0,0,0);
	}
	| FREAD STRING timecode
	{
		read_file(dat,last_string,0,h,m,s,f);
	}

write:	FWRITE STRING
	{
		write_file(dat,last_string);
	}

program: PROGRAM
	{
		inc_program();
	}
	| PROGRAM INTEGER 
	{
		set_program(yylval);
	}
	;

index: INDEX
	{
		inc_index();
	}
	| INDEX INTEGER
	{
		set_index(yylval);
	}
	;

pause: PAUSE
	{
		pause();
	}
	;

smart: SMART BOOLEAN
	{
		smart = yylval;
		if (yylval == 0) printf("playing stupid...\n");
	}
	;

testpat: TESTPAT INTEGER
	{
		write_testpat(dat,yylval);
	}
	;

silence: SILENCE INTEGER
	{
		write_silence(dat,yylval);
	}
	;

atime: ATIME timecode 
	{
		set_atime(h,m,s,f);
	}
	;
ptime: PTIME timecode
	{
		set_ptime(h,m,s,f);
	}
	;
prog: PROGNUM INTEGER { p = yylval; };

seek: SEEK ATIME timecode 
	{ 
		seek_time(dat,1,h,m,s,f);
	}
      | SEEK PTIME timecode
	{
		seek_time(dat,0,h,m,s,f);
	}
      | SEEK prog 
	{
		seek_prog(dat,p);
	}
      | SEEK BOT
	{
		seek_bot(dat);
	}
      | SEEK EOT
	{
		seek_eot(dat);
	}
      ;

hours: INTEGER { h = yylval; };
minutes: INTEGER { m = yylval; };
seconds: INTEGER { s = yylval; };
frames: INTEGER { f = yylval; };

timecode: hours COLON minutes COLON seconds COLON frames;

comment: COMMENT ;
