/******************************************************************************

        			THIS IS Terminal.C

Copyright 1988-1994, Polhemus Incorporated, all rights reserved.

Revision History

24Jan92	SST
	Added batch mode capability
	Fixed single record request hot key
04Feb92 SST
	Minor formatting associated w/ F7 key
	Command line oriented command file operation
	Single message for collecting binary data in continuous mode
10Aug92 SST
	Created a FASTRAK version of formatted status reporting
	Added compensation On/Off to status reporting output
 8Mar93 HDC
 	Changed delay for system response to delay(msec) instead of a
	counting loop to be more compatible across different PC speeds.
24Jan94 HDC
	Changed per SDRs 94-008 and 94-015 to correct for line wrap on 
	the screen and allowing input from the system when input is not
	requested through the terminal program (ie stylus button caused
	I/O from the Fastrak/3DRAW).  Also corrected an error of not logging
	errors when in binary mode with log on and the logic associated with
	flushing of buffers after a change from continous to non-continous.
13Mar94 SST
	SDR 94-037. Continuous mode does not always work with 386 and 486
	based machines. Rewrote the continuous loop to no longer make calls
	to the Fetch() function, but to implement a continuous loop specific 
	system output fetch sequence.
	Minor change to FastrakStatus function.
	During ECO002.001 validation it was discovered that insufficient time 
	was allowed for a response from the system before timing out on
	the record fetch. The fetch() function used to process records has a 
	delay argument which controls the length of this wait, but this was
	fixed at 200msec. This amount of time allowed complete responses at
	baud rates around 4800 or faster. The length of the wait is now calc-
	ulated to be the approximate length of time required to transfer a 1 
	line record at a given baud rate. This is determined at startup and 
	any time the MID configuration is changed. At higher baud rates a 
	minimum response delay is imposed.
	The delay associated with stylus record polling has been modified
	from 200 msec to 0 as the existence of these records are not a result
	of a request from the terminal program.
21Mar94 SST
	SDR 94-046. Data records generated w/ the stylus pushbutton were
	being dropped by the terminal program. The code was re-written to
	see if unsolicited data was pending in the receive buffer. If so
	delay long enough for a one line record to be completely xmitted by
	the instrument then display and log it once received.
23Jun94 SST
	The following changes were made to correct problems in the
	terminal program as documented in SCO02.003:

	The fetch() function was rewritten so it could again be used in the
	ContinuousReporting and Stylus Button collection routines. This is
	a more correct solution to fixes in SCO002.001 and SCO002.00202.
	
	The continuous reporting loop was rewritten such that the keyboard
	query (selectedNonContinuous) was performed following each system 
	record processed instead of following each character processed.

	The F11 key was assigned to enable and disable display of data. The
	displayOn flag was added to support this feature. This key has no
	effect on host PCs with 83 key keyboards.

	A routine linktoHelpFile was added so the appropriate help file
	can be linked to from information available in the instrument status.

	Rewrote SelectLogFile function to allow limited filename editing
	and ESC exit from functio.

	Added ESC terminate request functionality to batch file processing.

	Removed changing rs232 setup confirmation.

15-Sep-94 SCO 02.004
	Quiet mode and normal mode reversed. Swap definition of QUIET and
	NOTQUIET.

	Update Rev.

This program interfaces a PC user to a 3SPACE instrument. In addition to
a dumb terminal emulation this program supports logging capability.
This program also supports ASCII and binary data collection formats
and additionally supports the 3SPACE continuous mode of operation.
The comm port used for serial interface communication is selectable.
The RS232 communications protocol is configurable, spawning the Para-
soft MCF menu driven program to facilitate this task.
This program invokes calls to the Parasoft MID serial interface driver 
utilities for serial I/O.  These utilities replace the comm port BIOS 
utilities and support such features as buffered input and output.

This program uses both the "printf" and "_outtext" commands for out-
putting text to the display. The _outtext usage is to stay synced with
certain other graphics commands that it was necessary to use in imple-
mentation, for example, _gettextposition and _settextwindow. Although
a limitation of _outtext is that it accepts only string formatted data
for output to the display. printf on the other hand provides support
for conversion of a decimal number (%d) to a textual output, which
is not only convenient but a standard "C" usage. An unsavory side 
effect of this mixed mode of textual output is printf'd text comes 
bordered with a fill color, whereas _outtext'd output does not. Ul-
timeatly all output should use the _outtext function with the to ASCII
conversions handled by the programmer.
******************************************************************************/

#include "stdlib.h"
#include "stdio.h"
#include "utils.h"	/* Header file of screen, keyboard and misc functs*/
#include "graph.h"	/* Specs for graphics/screen formatting calls */
#include "process.h"	/* Specs for launching tasks */
#include "mid/midio.h"	/* Specs for the MID driver utilities */
#include "conio.h"
#include "bios.h"

FILE *logfile;		/* Handle for log file */
#define MAXRECORDSIZE 300 /* Max buffer size RS232 local buffering */
			/* Command line buffer */
char CommandLine[MAXRECORDSIZE];
			/* The displayed command line, char sizes */
char DisplaySymbolSize[MAXRECORDSIZE]; 
int position = 0;	/* Command line size */
int commport = 0;	/* comm port; 0 for at/xt's */
			/* record received from instrument */

char DataRecord[MAXRECORDSIZE];
char *KeyboardInput;	/* Single character input buffer */
char misc[16];		/* miscellaneous buffer */
char BatchFileName[32];
char StatusMessage[64];
FILE *BatchFile;

int LoggingOn = 0;	/* Data logging enabled flag */
int displayOn = 1;	/* Display of data records enabled flag */
int isLogFileOpen = 0;  /* Has a log file been selected? */
int isBinary = 0;	/* are systems in the binary mode? */
int isQuiet = 0;	/* ISOTRAK quiet mode? */

/* 3SPACE commands */
char toBinaryCommand[] = "f";
char toASCIICommand[] = "F";
char toContinuousCommand[] = "C";
char toNonContinuousCommand[] = "c";
char retrieveStatusCommand[] = "S";
char PCommand[] = "P";

/* Special key values */
#define CR '\r'	
#define CONTINUOUS 'C'
#define NONCONTINUOUS 'c'
#define BINARY 'f'
#define ASCII 'F'
#define STATUS 'S'
#define BS '\b'
/* SCO 002.004 */
#define QUIET 'K'
#define NOTQUIET 'm'
#define ERROR '*'
#define ESC 27
#define PRINTABLE 128
#define TRANSMIT 129
#define CTRL 130

/* Status record formatting support */
/* System flag bit field definitions */
#define BINARYFORMAT 1
#define METRICUNITS 2
#define COMPENSATIONON 4
#define CONTINUOUSTRANSMIT 8
#define TRACKERON 16
#define EXTENDEDON 32
#define FASTRAK 32
#define DIGITIZERMODEMASK 192
#define DIGITIZERMODEMASK1 768
#define CURRENTDIGITIZERMODE ( (DIGITIZERMODEMASK & SystemFlags) / 64 )
#define CURRENTDIGITIZERMODE1 ( (DIGITIZERMODEMASK1 & SystemFlags) / 256 )
#define ISSYSTEMFLAG(whichFlag) (SystemFlags & whichFlag)
#define ISFastrak ( (DataRecord[18] == '.') )
#define BEEP putchar('\a')

char I3[] = "\0\0\0"; 		/* Data scan formatter */
int SystemFlags;		/* system flags buffer */
char BITERR[] = "   ";		/* BITERR buffer */
char Version[3];		/* buffer */
char Revision[3];		/* buffer */

int dly;			/* Response time from system */
int Kb101;			/* 101 key keyboard installed? */

/* Function specifications */

/* Main functions */
int main(void );		/* Main program */
void doitall(void );		/* Overall processing loop */

/* Command line operations */
void processCommandLine(void);	/* Send and respond to, an 3SPACE command */
void buildCommandLine(int ch);	/* Assembles characters into the command line */
void displayCommandLine(char whatChar); /* Displays the next char or symbol typed */
void editCommandLine(void);	/* Handle backspaces */
void resetCommandLine(void);	/* Initialize for building next command */
int isCommand(int whatCommand);	/* search out certain commands */

/* 3SPACE data record operations */
int fetch(char *datarecord);	/* Read a data record from the 3SPACE */
void ContinuousReporting(void);	/* Process input data continuously */
int selectedNonContinuous(void); /* Was Noncontinuous mode commanded? */
int operatorBreak(void);	 /* Did user hit ESC to stop a subprocess? */
void NormalReporting(void);	/* Look for any response and process */
void StatusReporting(void);	/* Displays formatted status report */
void FastrakStatus(void);	/* FASTRAK formatted status */
void ThreeSpaceStatus(void);	/* 3SPACE formatted status */
char *DigitizerModeName(int Mode); /* When displaying status */
void Log(char *Record); 	/* Write records when logging enabled */
void LogBinary(char *Record,int length); /* Write records when logging enabled */
int isRecord(char whatType);	/* True if record returned is of type requested */

/* Utility and mode support */
void SelectLogFile(void );	/* Open a file for data logging */
FILE *accessLogFile(char *);    /* validate the logfile selected */
int getFilename(char *);	/* Input a filename from the user */
void BinaryToggle(void);	/* Switch back and forth binary & ASCII */
void LoggingToggle(void);	/* Turn data logging on and off */
void displayToggle(void);	/* Turn displaying of data on and off */
void setupMIDConfig(void);	/* Initialize the serial interface */
void ConfigureMID(void );	/* MID initialization function */

/* Batch mode functions */
void displayBatchMenu (void);
int OptionMenu(void);
void processBatchMode(void);
int selectBatchFile(void);
int processBatchFile(void);

/* Display utilities */
void scrnbot(void );		/* Command legend and prompt */
void displayFunctionBar(void);	/* Function key legend */
void displayHelp(void);		/* Scrolls through file of 3SPACE commands */
char *linktoHelpFile(void);	/* Correct help file for system connected*/
void Banner(void);		/* Polhemus banner */

/* Keyboard char operations */
int isFunctionKey(void);	/* was the key pressed a function key? */
int isKey(int whatkey);		/* Look for certain key inputs */

extern short delay(short msec);

/****************************************************************
* 	function	main()
*
*	purpose		Gets the program going with various initializations.
*
*	inputs		none
*
*	outputs		3SPACE commands
*
*	process		Initialize serial interface
*			Put 3SPACE in ASCII Non-Continuous
*			Call interactive loop
*               
*	remarks		none
****************************************************************/

main()
	{
	Banner();				/* Lead in screen */

	/* System initializations */
	ConfigureMID();				/* Initialize serial interface */
	mid_puts(commport,toASCIICommand);	/* Put 3SPACE in ASCII */
	mid_puts(commport,toNonContinuousCommand); /* noncontinuous mode */
						/* SCO002.001 */
	dly = mid_brd(commport,0) * 10;		/* base response delay on */
	if (dly < 200 ) dly = 200;		/* baud rate or minimum */

	Kb101 = kbtype();			/* Is this a 101 keyboard*/
	doitall();				/* interactive control loop  */
	}


/****************************************************************
* 	function	doitall()
*
*	purpose		overall  control loop.
*
*	process		put up control screen
*			Retrieve inputs from keyboard
*			Handle hot keys
*				Continuous mode control
*				ASCII/binary mode control
*				Data logging switch
*				Log file selection
*				Status reporting
*				I/O setup
*				Single data record processing
*				Exit to DOS
*			Send Commands and Process responses
*			Build Command Lines
*			Display Command Lines
*			Edit Command Lines
*				
*	remarks		
*
****************************************************************/

void doitall()
{
	int length;			/* scratch variable */
	/* Local initializations */
	CLS;			/* Clear Screen */
	scrnbot();		/* Display command legend */
	resetCommandLine();
	_wrapon(_GWRAPON);

	while (1)
	{
	    if(kbhit())
		{
		/* Program waits here for the user to type something */
		KeyboardInput = gtkbd();
		/* If user pressed a function key, see which number it was
		and intiate the appropriate action. If its not a function
		key see if its a special key and take appropriate action,
		or else assemble the type key into the command line */

		switch(isFunctionKey())
		{
		case 0: /* Any key but a function key */

					 	/* transmit command trigger */
			if (isKey(TRANSMIT)) 	/* This is the Keyboard END key */
				{
				processCommandLine();
				CLROWS(21, 21);
				ATSAY(21, 0, "COMMAND ");
				}
			else if (isKey(BS))
				editCommandLine();
			else			/* Part of command string */
				{
				buildCommandLine(KeyboardInput[1]);
				displayCommandLine(KeyboardInput[1]);
				}
			break;

		case 1:	/* Command 3SPACE to continuous data transmit mode */
			/* Report data continuously until user terminates */
			{
			mid_puts(commport,toContinuousCommand);
			ContinuousReporting();
			break;
			}
		case 2:	/* Toggle 3SPACE between binary and ASCII output */
			{
			BinaryToggle();
			scrnbot();
			break;
			}
		case 3: /* Enable of disable writing of data to the log file */
			/* This does not do any file control */
			{
			LoggingToggle();
			scrnbot();
			break;
			}
		case 4: /* Choose a file and create, append or overwrite */ 
			{
			SelectLogFile();
			scrnbot();
			break;
			}
		case 5: /* Display formatted 3SPACE status info */
			{
			StatusReporting();
			scrnbot();
			break;
			}
		case 6: /* Reconfigure the serial interface */
			{
			setupMIDConfig();
						/* SCO002.001 */
			dly = mid_brd(commport,0)*10;/* base response delay */
			if (dly < 200 ) dly = 200; /*on baud rate or minimum */
			scrnbot();
			break;
			}
		case 7: /* Request a single 3SPACE record and display */
			{
			mid_puts(commport,PCommand);
			NormalReporting();
			_settextposition(21,9);
			break;
			}
		case 8: /* Exit program to DOS */
			{
			CLS;
			mid_clean();
			exit(0);
			}
		case 9: /* Display help for 3SPACE commands */
			{
			displayHelp();
			scrnbot();
			break;
			}
		case 10: /* Send 3SPACE command file to instrument*/
			{
			processBatchMode();
			scrnbot();
			break;
			}
/* SCO 002.003 */
		case 11: /* Turn off displaying of data records*/
			{
			if(Kb101)
				displayToggle();
			scrnbot();
			break;
			}
		}	/* end switch */

	    }	/* end key hit true */
	    	/* look for records sent with the stylus button */
							/* SDR 94-046 */
	    /* Check to see if any data is in the receive buffer.
	    If there is, allow enough time for an entire output record 
	    (1 lines worth) to be received by the terminal program.
	    Display and log this record. */
	    if (mid_count(commport,2))	/* Anything in rec buf? */
		{
		if(length=fetch(DataRecord))
			{
					/* Display formatting */
			if (isBinary)
			    LogBinary(DataRecord,length);
			else
			    {
			    _settextwindow(1,1,20,80);
			    _settextposition(20,1);
			    if(displayOn)
			    	_outtext(DataRecord);
				    /* Send it to the log */
			    Log(DataRecord);
			    _settextwindow(1,1,25,80);
			    _settextposition(21,9);
			    }
			}
		    }
	}  /* end while interactive loop*/
}  /* end doitall */


/****************************************************************
* 	Function	Command Line operations
*
*	purpose		Perform operations on the CommandLine
*				resetCommandLine
*				processCommandLine
*				buildCommandLine
*				displayCommandLine
*				editCommandLine
*				
*
*	process		See individual functions
*				
*	remarks		
*
****************************************************************/

/*                                                             */
/* Initializes command line before assembling a command */
/*                                                             */

void resetCommandLine(void)
	{
	CommandLine[0] = '\0';
	position = 0;
	}


/*                                                             */
/* Process command line is called when the user has completed an
entire command line and wants it sent to this system. Additionally
this section will wait to see if any responses have been sent back
by the 3SPACE and process these.
Certain commands sent to the 3SPACE have special meaning to both the
the 3SPACE and the terminal program. These commands are trapped out
and the appropriate action needed by the terminal program is taken */
/*                                                             */

void processCommandLine(void)
	{
	mid_puts(commport, CommandLine); /* Send command to system */
	Log(CommandLine);		/* Send command to the logger */
	Log("\r\n");			/* and a crlf for file formatting */
	if (isCommand(BINARY))		/* Special command traps */
		isBinary = 1; 
	else if (isCommand(ASCII))
		isBinary = 0;
	else if (isCommand(QUIET))
		isQuiet = 1;
	else if (isCommand(NOTQUIET))
		isQuiet = 0;
	else if (isCommand(CONTINUOUS))
		ContinuousReporting();
	else				/* Regular commands */
		NormalReporting();

	resetCommandLine();		/* Housekeeping */
	}

/*                                                             */
/* Assembles a command line keeping track of size etc. */
/*                                                             */

void buildCommandLine(ch)
	{
	CommandLine[position] = ch;
	position++;
	CommandLine[position] = '\0'; 	/* tentative string end */
	}


/*                                                             */
/* Displays the command line being assembled to the user. Non printing
characters can also assembled into command lines and a recognizable 
symbology is shown to the user in these cases. displayCommandLine
does not actually maintain a logical image of the displayed text, but
process each character realtime as a user keys them in. */
/*                                                             */

void displayCommandLine(char whatChar)
	{
	if (isKey(PRINTABLE)) /* if ascii */
		{
		misc[0]=KeyboardInput[1];
		misc[1]='\0';
		_outtext(misc);
		DisplaySymbolSize[position] = 1;
		}

	else if (isKey(ESC))
		{
		_outtext("<esc>");
		DisplaySymbolSize[position] = 5;
		}

	else if (isKey(CTRL))
		{
		_outtext("^");
		misc[0]=KeyboardInput[1] + 64;
		misc[1]='\0';
		_outtext(misc);
		DisplaySymbolSize[position] = 2;
		}

	else if (isKey(CR))
		{
		_outtext("<cr>");
		DisplaySymbolSize[position] = 4;
		}

	}


/*                                                             */
/* Edit the command line on the display, must
know the size of the last symbol to know how many
characters to Back Space over. Backspacing is the
only editting capability supported */
/*                                                             */

void editCommandLine()
	{
	int j;
	struct rccoord screen;

	if (position)
		{
		for (j=0; j<DisplaySymbolSize[position]; j++)
			{
			screen = _gettextposition();		/* Current cursor position */
			screen.col = screen.col-1;		/* backspaced cursor position */
			_settextposition(screen.row,screen.col); /* backspace */
			_outtext(" ");			/* overwrite with blank */
			_settextposition(screen.row,screen.col); /* backspace */
			}
		position--;
		CommandLine[position] = '\0'; /* tentative string end */
		}
	}


/* Accepts a single character command identifier and returns
a true if the assembled command is of this type */

int isCommand( int whatCommand)
{
	return(CommandLine[0] == whatCommand);
}



/****************************************************************
* 	Function	3SPACE data record operations
*
*	purpose		Perform operations on the DataRecord
*				fetch from 3SPACE			
*				Put system in Continuous Reporting mode
*				Perform Normal Reporting
*				Status Reporting on formatted screen
*				Write data to log file when enabled
*				Test record type
*
*	process		See individual functions
*				
*	remarks		
*
****************************************************************/


/*                                                             */
/* Places the instrument in the continuous data transmission mode
of operation by sending the appropriate command. Enters a tight
	fetch data
	display data or message
	log data
	?terminate continuous
loop. These operations are subject to mode conditions as detailed
below */
/*                                                             */

void ContinuousReporting(void)
{
int length;
char buf[256];

CLS;
ATSAY(23, 0, "F1= Non-Continuous");	/* pressing F1 quits loop */
_settextwindow(1,1,20,80);		/* Scroll window formatting */
_settextposition(20,1);
if (isBinary ) 			/* Can't predictably display binary data */
	CTRSAY(10,"Collecting Binary Data");
buf[0] ='\0';
while(1)
	{
	if(length = fetch(buf))
		{
		if (selectedNonContinuous())
			break;
				/* Display formatting */
		if (isBinary)
		    LogBinary(buf,length);
		else
		    {
		    if(displayOn)
			_outtext(buf);
			    /* Send it to the log */
		    Log(buf);
		    }
		}
	if (selectedNonContinuous())
		break;
	}

_settextwindow(1,1,25,80);		/* Restore display format */
if (isBinary)	/* Clear binary data message*/
	CLROWS(10,10);
scrnbot();
mid_flush(0,2);
delay(2*dly);
mid_flush(0,2);
}

/* The fetch,display,log,?exit loop */
/* This entire function has been rewritten as a result of SDR 94-037 */
/* The continuous loop is structured as follows:
	while discontinue has not been selected by the user do the following:
		Sample the MID receive buffer.
		if a character is in the buffer build it into an output
		record image.
		when the record is complete, that is terminated by a CRLF
		pair, display and log the record.
	when discontinue has been selected:
		throw out the record under construction
		wait for the instrument to finish sending the data records
		it has queued up before discontinuing mode.
		Pump these characters from the MID buffer to the bit bucket.
	Continuous loop complete.
*/
/* Looks for user hitting keys. If yes,
check to see if it is a discontinue continuous
command. Either 'c' or F1. Return a yes I
want to discontinue flag if so */

int selectedNonContinuous(void)
	{
	if (kbhit())			 /* Was a key pressed? */
		{
		KeyboardInput = gtkbd(); /* Fetch and see if a discontinue key struck */
		if(isKey(ESC) || isKey(NONCONTINUOUS) || isFunctionKey()==1 )
			/* Send the discontinue command to instrument */ 
			{		 
			mid_puts(commport,toNonContinuousCommand);
			return 1;
			}
		return 0;
		}
	return 0;
	}


/* In this mode it is expected that the instrument is returning a finite
number of records. Normal reporting will continue to process records
until fetch times out. Processing is:
	fetch record
	if record
		display record
		log record */

void NormalReporting(void)
	{
	int length;

						/* SCO002.001 */
	delay(dly);			/* Await response from the system */
	if (!mid_count(commport,2))	/* Anything in rec buf? */
		return;			/* System did not respond */
	if(length=fetch(DataRecord)) 	/* As long as there are data records */
		{
		_settextwindow(1,1,20,80);	/* Display formatting */
		_settextposition(20,1);
		if (isBinary)		/* Can't display binary predictably */
			{
			if (!isRecord(ERROR))
				{
				_outtext("Collecting Binary Data\n");
				LogBinary(DataRecord,length);
				}
			else		/* Display error messages */
				{
				_outtext(DataRecord);
				Log(DataRecord);
				}
			}
		else
			{
			if (displayOn)
				_outtext(DataRecord);
			Log(DataRecord);
			}
		_settextwindow(1,1,25,80);	/* Display formatting */
		}
	}


/*                                                             */
/* Send the command to retrieve a status record from the 3SPACE.
Decode the returned message and display to the user in a formatted
screen. The formatting should be apparent from the print statements.
Consult the 3SPACE manual for actual status record format. */
/*                                                             */

void StatusReporting()
	{
	mid_puts(commport,retrieveStatusCommand); /* Send command to system */
					/* SCO002.001 */
	delay(dly);		        /* Wait for a response from the system*/
	if (!mid_count(commport,2))	/* Anything in rec buf? */
		{
		CLS;
		CTRSAY(10,"No Status Received");
		return;			/* System did not respond */
		}
	if (fetch(DataRecord))		/* true when complete record is available */
		{
		if (isRecord(STATUS))
			{
			CLS;
			_settextposition(4,30);		/* Title */
			_outtext("  INSTRUMENT STATUS"); 	

			if (ISFastrak)
				FastrakStatus();
			else
				ThreeSpaceStatus();
			}
		else
		/* If a non-status record was received output it */
			{
			_settextwindow(1,1,20,80);	/* Display formatting */
			_settextposition(20,1);
			_outtext(DataRecord);
			_settextwindow(1,1,25,80);	/* Display formatting */
			}
		}
	}
	
/* The format of the status message returned by the FASTRAK instrument 
although in most fields is consistent with previous instruments is 
distinct enough so that separate FASTRAK and 3SPACE decoding and
formatting routines have been implemented */

/*FASTRAK version */
void FastrakStatus()
	{
	misc[0] = DataRecord[1];	/* Station number */
	misc[1] = '\0';
	_settextposition(6,30);
	_outtext("Station Number  ");
	_outtext(misc);

	strncpy(I3,&DataRecord[3],3);
	sscanf(I3,"%x",&SystemFlags);

	_settextposition(7,30);		/* Binary or ASCII */
	if (ISSYSTEMFLAG(BINARYFORMAT))
		_outtext("Binary Data");
	else
		_outtext("ASCII Data ");

	_settextposition(8,30);		/* English or Metric */
	if (ISSYSTEMFLAG(METRICUNITS))
		_outtext("Metric Units ");
	else
		_outtext("English Units");

	_settextposition(9,30);		/*Compensation On*/
	if (ISSYSTEMFLAG(COMPENSATIONON))
		_outtext("Compensation On");
	else
		_outtext("Compensation Off");

	_settextposition(10,30);	/* Continuous, has to be here */
	if (ISSYSTEMFLAG(CONTINUOUSTRANSMIT))
		_outtext("Continuous Transmission");
	else
		_outtext("Non-Continuous Transmission");
	
	_settextposition(11,30);
	if (ISSYSTEMFLAG(TRACKERON))
		_outtext("Tracker Configuration On ");
	else
		_outtext("Tracker Configuration Off");
	
	_settextposition(12,30);
	if(ISSYSTEMFLAG(TRACKERON))	/* Picked up w/ SDR 94-037 */
		_outtext("FASTRAK");
 	else
		_outtext("3DRAW");
	
	_settextposition(13,30);	/* uses string constant function */
	_outtext("Digitizer Mode , Sensor 1 ");
	_outtext(DigitizerModeName(CURRENTDIGITIZERMODE));
	
	_settextposition(14,30);	/* uses string constant function */
	_outtext("Digitizer Mode , Sensor 2 ");
	_outtext(DigitizerModeName(CURRENTDIGITIZERMODE1));
	
	strncpy(BITERR,&DataRecord[6],3); /* BIT errors */
	_settextposition(15,30);
	_outtext("BITERR  ");
	_outtext(BITERR);

	strncpy(I3,&DataRecord[15],3);	/* Instrument S/W version */
	sscanf(I3,"%s",Version);
	strncpy(I3,&DataRecord[18],3);
	sscanf(I3,"%s",Revision);
	_settextposition(16,30);
	_outtext("Software Revision  ");
	_outtext(Version);
	_outtext(Revision);
	}

/* 3SPACE version */
void ThreeSpaceStatus()
	{
	misc[0] = DataRecord[1];	/* Station number */
	misc[1] = '\0';
	_settextposition(6,30);
	_outtext("Station Number  ");
	_outtext(misc);

	strncpy(I3,&DataRecord[3],3);
	sscanf(I3,"%d",&SystemFlags);

	_settextposition(7,30);		/* Binary or ASCII */
	if (ISSYSTEMFLAG(BINARYFORMAT))
		_outtext("Binary Data");
	else
		_outtext("ASCII Data ");

	_settextposition(8,30);		/* English or Metric */
	if (ISSYSTEMFLAG(METRICUNITS))
		_outtext("Metric Units ");
	else
		_outtext("English Units");

	_settextposition(9,30);		/*Compensation On*/
	if (ISSYSTEMFLAG(COMPENSATIONON))
		_outtext("Compensation On");
	else
		_outtext("Compensation Off");

	_settextposition(10,30);	/* Continuous, has to be here */
	if (ISSYSTEMFLAG(CONTINUOUSTRANSMIT))
		_outtext("Continuous Transmission");
	else
		_outtext("Non-Continuous Transmission");
	
	_settextposition(11,30);
	if (ISSYSTEMFLAG(TRACKERON))
		_outtext("Tracker Configuration On ");
	else
		_outtext("Tracker Configuration Off");
	
	_settextposition(12,30);
	if (ISSYSTEMFLAG(EXTENDEDON))
		_outtext("Extended Configuration On ");
	else
		_outtext("Extended Configuration Off");
	
	_settextposition(13,30);	/* uses string constant function */
	_outtext("Digitizer Mode ");
	_outtext(DigitizerModeName(CURRENTDIGITIZERMODE));
	
	strncpy(BITERR,&DataRecord[6],3); /* BIT errors */
	_settextposition(14,30);
	_outtext("BITERR  ");
	_outtext(BITERR);

	strncpy(I3,&DataRecord[15],3);	/* Instrument S/W version */
	sscanf(I3,"%s",Version);
	strncpy(I3,&DataRecord[18],3);
	sscanf(I3,"%s",Revision);
	_settextposition(15,30);
	_outtext("Software Revision  ");
	_outtext(Version);
	_outtext(".");
	_outtext(Revision);

	if(isQuiet)			/* ISOTRAK only mode */
		{
		_settextposition(16,30);
		_outtext("Quiet Mode");
		}
	}

/* Returns a pointer to the digitizer mode name string */
char *DigitizerModeName( int mode )
	{
	static char *name[] = { "Point", "Run", "Track", "Off"};
	return name[mode];
	}


/*                                                             */
/* Write the data record to an output file. This checks
to see if logging is enabled or not. */
/*                                                             */

void Log(char *Record)
	{
	if (LoggingOn)
		fputs(Record,logfile);
	}

/*                                                             */
/* Write the data record to an output file. fwrite is used in
this instance so 0's which appear as null terminators to fputs
can be written to the file. This function checks to see if logging 
is enabled or not. */
/*                                                             */

void LogBinary(char *Record, int length)
	{
	if (LoggingOn)
		fwrite(Record,1,length,logfile);
	}


/* Reads a data record from the 3SPACE instrument. This is
based on the instrument terminating all output records with
a CR LF pair. This appears to hold true for binary or ASCII formatted
data. A programmable delay can be used to await the transmission of an 
entire 80 character record is provided in start of the fetch routine. 
If following this delay the receive buffer is empty then it is assumed 
that no record is forthcoming at this time and the routine is exited. */

/* SCO 002.003 */
int fetch(char *DataRecord)
{
int ch;
int j, length = 0;
long begin_tick, end_tick;

#define TIMEOUT 1193180L/65536L*.25

DataRecord[0] = '\0';
_bios_timeofday(_TIME_GETCLOCK, &begin_tick);
while(1)
	{
	if ((ch = mid_comin(commport)) != -1)
		{	
			/* time out reference */
		_bios_timeofday(_TIME_GETCLOCK, &begin_tick);
			/* Something was in the buffer */
			/* Strip out the received character and 
			assemble into the record buffer */
		DataRecord[length++] = (char)(0xFF&ch);
			/* Check for CR LF record terminator pair */
		if (length > 1)
	    		if(DataRecord[length - 2]=='\r' && 
	    		DataRecord[length-1] == '\n')
				{
				DataRecord[length] = '\0';
				return (length);
				}

			/* An endless record was transmitted or 
			interface configuration	problems */
		if (length > 295)
			{
			CTRSAY(10,"A port read error has occured");
			return(0);
			}
		}
/* Look for the condition where a record is being received but quits before 
an entire record is received. This block will detect that condition and quit
the fetch loop .*/
	else
		{
		_bios_timeofday(_TIME_GETCLOCK, &end_tick);
		if((end_tick-begin_tick) > TIMEOUT)
			{
			if(length)
			    CTRSAY(10,"A port read error has occured\n");
			DataRecord[length] = '\0';
			return(length);
			}
		}
	}
}  /* end fetch */


/* Was the record returned from the instrument a Status record 
for example */
int isRecord(char whatType)
{
return DataRecord[2] == whatType;
}

/****************************************************************
* 	Function	Utility and mode support
*
*	purpose		Provide tools for the following functions:
*				Switching between binary and ASCII modes
*				Selecting Log Files
*				Enabling/disable logging control
*				Defining serial interface protocol
*				Initializing the serial interface
*				Displaying the command promt and legend
*				Displaying instrument commands help
*				Display the Polhemus banner
*				
*
*	process		See individual functions
*				
*	remarks		
*
****************************************************************/


/*                                                             */
/* Depending on the current state of the systems, send the
command to the 3 space to transmit either binary or ASCII data.
Announce this mode change to the user and flag the terminal
program */
/*                                                             */
 
void BinaryToggle(void)
	{
	CLS;
	if (isBinary)
		{
		CTRSAY(10,"Switched to ASCII Mode");
		mid_puts(commport,toASCIICommand);
		isBinary=0;
		}
	else
		{
		CTRSAY(10,"Switched to Binary Mode");
		mid_puts(commport,toBinaryCommand);
		isBinary=1;
		}
	}


/*                                                             */
/* Change state on the logging control flag. Announce mode change to user.
If a log file has not yet been opened, inform the user of this */
/*                                                             */

void LoggingToggle(void)
	{
	CLS;
	if (LoggingOn)
		{
		CTRSAY(10,"Disabled Data Logging");
		LoggingOn = 0;
		}
	else
		{
		if (isLogFileOpen)
			{
			CTRSAY(10,"Enabled Data Logging");
			LoggingOn = 1;
			}
		else
			CTRSAY(10,"Must select a log file");
		}
	}


/*                                                             */
/* Change state on the logging control flag. Announce mode change to user.
If a log file has not yet been opened, inform the user of this */
/*                                                             */

void displayToggle(void)	/* SCO 02.003 */
	{
	CLS;
	if (displayOn)
		{
		CTRSAY(10,"Displaying of Collected Data Disabled");
		displayOn = 0;
		}
	else
		{
		CTRSAY(10,"Displaying of Collected Data Enabled");
		displayOn = 1;
		}
	}


/*  Spawn the MID serial interface driver configuration setup program 
mcf.exe. This allows user to define baud rates, stop bits, parity, 
input buffer sizes etc. Perform any error handling for mcf access errors. 
After the interface has been reconfigured reintialize the serial interface 
to use the new configuration. */

void setupMIDConfig(void)
{
struct videoconfig config;

/* The MID configuration program will not run with CGA graphics adaptor
cards. Must setup config file on a PC w/ a EGA, VGA, etc. */
_getvideoconfig(&config);
if (config.adapter == _CGA)
    {
    CLS;
    CTRSAY(10,"Cannot run the MID config program with current video adapter.");
    return;
    }

mid_clean();				/* Housekeeping */
if (spawnl (P_WAIT,"mid/mcf.exe",0) )	/* Run MID config program */
    printf("Could not run the MID configuration program\n");
if (mid_readcf("mid.mcf")==0)		/* Latch in new port config */
    printf("MID config file failed to load properly.");
mid_alloc(0);
if(mid_reset()==0)
    printf("MID interface not configured properly.");
CLS;
}

/*                                                             */
/* Startup MID driver. Flag any problems with MID access here. */
/*                                                             */

void ConfigureMID()
	{
	/* Load the serial interface configuration file */
	/* This file contains baud rate, stop bits, buffer sizes, etc */
	if (!mid_readcf("mid.mcf"))
		{
		CLS;
		CTRSAY (9," Could not load the MID config file.");
		CTRSAY(10, "Pressing ENTER will exit this program.");
		gets(misc);	/* Wait */
		mid_clean();	/* Housekeeping */
		exit(1);	/* Exit program */
		}
	mid_alloc(0);
	/* Empty buffers, clear interrupts, return MID revision  etc. */
	if (!mid_reset())
		{
		CLS;
		CTRSAY(9,"MID did not initialize, see terminal.doc.");
		CTRSAY(10, "Pressing ENTER will exit this program.");
 		gets(misc);	/* Wait */
		mid_clean();	/* Housekeeping */
		exit(1);	/* Exit program */
		}
	}


/* 
	Select a file which to log to.
	The user can select the default log file, specify a new file to use, 
	or continue using the log file currently open.
*/

void SelectLogFile()
{
static char filename[64];
int ch;

/* option to continue to use a previously opened file */
if (isLogFileOpen)
	{
	CLS;
	printf("\n\tLog file %s is currently open.",filename);
	printf("\n\tDo you want to use this file?");
	printf("\n\t(y or n):   ");
	ch = getch();
	if (ch == ESC)
		{
		CLS;
		return;
		}
	if (tolower(ch) == 'y')
		{
		CLS;
		CTRSAY(10,"Enabled Data Logging");
		LoggingOn=1;		/* Will automatically start logging */
		return;
		}
	else
		fclose(logfile);
	}

while(1)		/* Select between default or user spec'd log file */
	{
	CLS;
	printf("\n\tMessages to/from the System on RS-232 will be logged");
	printf("\n\tDo you want to use the default log file name: SYSDATA.LOG");
	printf("\n\t(y or n): ");
	ch = getch();
	if(ch == ESC)		/* abort log file selection process? */
		{
		CLS;
		return;
		}
	if(tolower(ch)=='n')	/* User input logfile name */
		{
		CLS;
		puts("\n\n\n\tPlease enter a log file name.");
		printf("\n\tfilename: ");
		if(!getFilename(filename))
			return;
		}
	else			/* The default logfile selected */
		strcpy(filename, "sysdata.log");

		/* Check the accessability of the requested logfile. 
		0 => file not accessable or invalid name, etc. 	
		1 => logfile successfully opened */ 
	if (logfile = accessLogFile(filename))
		{
		CLS;
		CTRSAY(10,"Enabled Data Logging");
		isLogFileOpen = 1;	/* Flag the log file is open */
		LoggingOn=1;		/* Will automatically start logging */
		return;			/* records to the file */
		}
	}
}



/* Open selected file for data logging. Respond to file access conditions by
giving the user the option to:
	overwrite files
	append to files
	select a new log file
*/

FILE *accessLogFile(char *filename)
{
int ch;
static FILE *fp;
		/* if spec'd file does not exist, attempt to open it
		and return the results of this action */
if(access(filename,0))
	{
	if (fp = fopen(filename, "wb"))
		return fp;
	else
		{
		printf("\n\tI can't open %s ", filename);
		printf("\n\tPress ENTER to continue.");
		getch();
		return 0;
		}
	}
else		/* If the file does exist see if it is write protected*/
	{
	if(access(filename,2))
		return 0;  /* Write protected. Return File Inaccessable */
	CLS;		   /* Offer options of what to do about existing file*/
	printf("\n\n\tA file named \"%s\" already exists.", filename);
	printf("\n\tChoose an option.\n");
	printf("     OPTION #            ACTION                      \n");
	printf("            0            Over-write the named file.  \n");
	printf("            1            Append to this log file.    \n");
	printf("            2            Choose another file name.   \n");
	printf("\n            OPTION : ");

	switch (getche())	/* Await user response to promts */
		{
		case '0':	/* Want to overwrite the existing file */
			return (fopen(filename, "wb"));
		case '1':	/* Want to append to the existing file */
			return (fopen(filename, "ab"));
		default:     /* Anything else assumes you want choose another*/
			return 0;
		}
	}
return 0;
}


/* Build a logfile name 1 character at a time. Showing the name as it is
assembled. The enter key terminates the assembling of the file name, but is
not included in the filename string. This code will associate the CR or LF 
character with the enter key. Use of the BS will back over a character but 
not blank it out. ESC aborts the select log file process */
int getFilename(char *filename)
{
int ch;
int j = 0;
while(1)
    {
    ch = getche();	/* Process 1 char at a time */
    if (ch == ESC)	/* Abort log file selection */
	return 0;
    if (ch == BS)	/* Back over last character enetered*/
	{
	if(j>0)
		j--;
	continue;
	}		/* Finished inputting name */
    if ((ch == '\r')||(ch == '\n'))
	{
	filename[j] = '\0';
	return j;
	}
			/* Assemble filename string */
    filename[j++] = (char)ch;
    }
return j;
}


/*                                                             */
/* Display the command legend at the bottom of the display */
/*                                                             */

void scrnbot()
	{
	CLROWS(21,25);
	displayFunctionBar();
	ATSAY(21, 0, "COMMAND ");
	}

/*                                                             */
/* Display the function key actions. Check Mode conditions for
toggle type function keys */
/*                                                             */

void displayFunctionBar()
	{
	ATSAY(23, 0,"TOGGLES  ");
	ATSAY(23,12, "F1= Continuous ");
	if (isBinary)
		ATSAY(23, 29,"F2= ASCII   ");
	else
		ATSAY(23, 29,"F2= Binary  ");
	if (LoggingOn)
		ATSAY(23, 41,"F3= Log Off "); 
	else
		ATSAY(23, 41,"F3= Log On  "); 
	if (Kb101)
		if (displayOn)
			ATSAY(23, 55,"F11= Display Off "); 
		else
			ATSAY(23, 55,"F11= Display On  "); 
	ATSAY(24, 0,"UTILITIES ");
	ATSAY(24,12,"F4= Select Log   F5= Status  F6= Setup     F7=  Request Data"); 
	ATSAY(25,12,"F8= Exit to DOS  F9= Help    F10=Com File  END= Send Command");
	}


/*                                                             */
/* Scroll through the help file for the 3SPACE instrument commands */
/*                                                             */
/*sco 002.003 */
void displayHelp(void)
{
char *helpcmd;

if(helpcmd = linktoHelpFile())
	{
	CLS;
	system(helpcmd);		/* invoke the DOS more command */
	printf("\nPress ENTER to continue.");
	gets(misc); 			/* wait for user to view display */
	CLS;
	}
else
	{
	CLROWS(10,10);
	CTRSAY(10,"Could not link to a help file!");
	}
return;
}

/* sco 002.003 */
/* Determine which instrument is being interface from the status word.
Return a pointer to the DOS commnad line which displays the appropriate
help file. If cannot get status from the system or the help file does not
exist return a null pointer */
char *linktoHelpFile(void)
{
int ver;

mid_puts(commport,retrieveStatusCommand); /* Send command to system */
delay(dly);			/* Await response from the system */
if (!mid_count(commport,2))	/* Anything in rec buf? */
	return 0;		/* System did not respond */
if (fetch(DataRecord))
	{
	if(ISFastrak)		/* FASTRAK or 3DRAW */
		{
		strncpy(I3,&DataRecord[3],3);
		sscanf(I3,"%x",&SystemFlags);
		if(ISSYSTEMFLAG(TRACKERON)) /*Determine from status word*/
			return ("more <help.cmd");
		else
			return ("more <3dhelp.cmd");
		}
	else			/* ISOTRAK-II or 3SPACE */
		{
		sscanf(&DataRecord[15],"%d",&ver);
		if(ver < 3)
			return ("more <3shelp.cmd");
		else
			return ("more <isohelp.cmd");
		}
	}
return 0;
}


/***************************************************************/
/* Display the Polhemus front page banner */
/***************************************************************/
void Banner(void)
{
CLS;
CTRSAY(10,"3SPACE C LANGUAGE INTERFACE TO PC and compatibles - Terminal, Ver 2.04");
CTRSAY(12,"Copyright 1988-1994, Polhemus Incorporated, all rights reserved.");
CTRSAY(25,"Press ENTER to continue.");
gets(misc);
CLS;
}


/****************************************************************
* 	Function	Keyboard character operations
*
*	purpose		Perform operations on the last key pressed
*				Check to see if it was a function key.
*				Check to see if it was a certain key.
*				
*
*	process		See individual functions
*				
*	remarks		
*
****************************************************************/


/*                                                             */
/* Check to see if the key entered satisfys the conditions of being
function key 1->11. If so return the function key number, otherwise
return a 0. */
/*                                                             */

int isFunctionKey(void)
{
	int KeyNumber;
	if (KeyboardInput[0]==0xff85)
		return 11;
	if (KeyboardInput[1] != 0)
		return 0 ;
	KeyNumber= KeyboardInput[0]-58;
	if ( KeyNumber > 0 && KeyNumber < 11 )
		return KeyNumber;
	return 0 ;
}


/*                                                             */
/* Check if the key just entered was a certain key or class of key */
/*                                                             */

int isKey( int whatKey)
{
	if (whatKey == PRINTABLE)
		return (KeyboardInput[1] > 31 && KeyboardInput[1] < 127);
	if (whatKey == TRANSMIT)
		return ((KeyboardInput[0] == 79) ||
		((KeyboardInput[0] == 0xE0) && (KeyboardInput[1] == 0x4F)));
	if (whatKey == CTRL)
		{
		if (KeyboardInput[1] != CR && KeyboardInput[1] != BS)
			return (KeyboardInput[1] > 0 && KeyboardInput[1] < 27);
		}
	return ((KeyboardInput[1] == whatKey));
}

/**********************************************************************
*	Function	Batch Mode operations
*
*	purpose		Implement a 3SPACE batch or command file 
*			processing capability.
*
*	inputs		none
*
*	Functions	Batch Mode Supervisor
*			Batch mode options menu
*			Batch file selection
*			Transmit batch file to 3SPACE
*
*	remarks
*
**********************************************************************/

/* processBatchMode is the supervisor routine for
the batch mode. An options menus is presented from
which the user can:
	Select a batch file for transmission to the instrument
	Retransmit a previously selected batch file
	Exit batch menu
The user selects an option and the supervisor takes the 
appropriate action. Any data received from the instrument
is displayed and upon completion of the batch file download
the batch mode supervisor is exited.

Some limitations exist in what commands are proper for
usage in a batch file. For the present the instrument 
should not be commanded to the continuous mode of operation
from the batch file. Additionally it is not advisable at
this time to put the instrument in the binary mode of 
operation.
*/

void processBatchMode(void)
{
	int ExitLoop;

	ExitLoop = 0;
	while( !ExitLoop )
	{
		switch(OptionMenu())
		{
		case '0':	/* Select a new input file */
			{
			if(selectBatchFile())
				ExitLoop=processBatchFile();
			break;
			}
		case '1':	/* Reload the current input file */
			{
			if(BatchFile=fopen(BatchFileName,"r"))
				ExitLoop=processBatchFile();
			else
				{
				BEEP;
				strcpy(StatusMessage,"Invalid option selected");
				}
			break;
			}
				     /* SCO 002.003 */
		case '2': case ESC : /* Exit the batch mode */
			{
			ExitLoop = 1;
			break;
			}
		default:	/* Invalid option selected */
			{
			BEEP;
			strcpy(StatusMessage,"Invalid option selected");
			break;
			}
		}
	}
CLS;
}

/**********************************************************************
*	Function	OptionMenu()
*
*	purpose		Offer user a list of batch mode options
*
*	inputs		StatusMessage
*
*	process		Display menu on screen
*			Prompt user to select an option
*			Read users choice
*			Return choice to calling routine
*
*	remarks
*
**********************************************************************/

int OptionMenu(void)
{
	char choice;    	/* Menu item chosen*/

	CLS;
	displayBatchMenu();	/* Displays batch mode menu choices */	
	choice = getch();	/* fetches your choice */
	return choice;
}

void displayBatchMenu()
{
	/* Option menu header */
	_settextposition(4,0);
	_outtext("        Choose an option.");
	_settextposition(6,0);
	_outtext("    OPTION #         ACTION ");

	/* Option 1 description */
	_settextposition(8,0);
	_outtext("           0         Select and send a command file");
	
	/* Option 2 description */
	/* Offered when batch file previously loaded */
	if (BatchFileName[0])
		{
		_settextposition(9,0);
		_outtext("           1         Send current command file ");
		_outtext(BatchFileName);
		}
	else
		{
		_settextposition(9,0);
		_outtext("                                      ");
		_outtext("                                      ");
		}

	/* Don't really want to load a batch file */
	_settextposition(10,0);
	_outtext("           2         Return to main menu ");

	/* Explains why the program beeped at you */
	_settextposition(16,8);
	_outtext(StatusMessage);
	StatusMessage[0]='\0';

	/* Option selection prompt */
	_settextposition(12,4);
	_outtext("    OPTION: ");

}

/**********************************************************************
*	Function	selectBatchFile()
*
*	purpose		Supervise the selection of a new batch
*			file
*
*	inputs		BatchFileName,BatchFile,StatusMessage
*
*	outputs		BatchFileName,BatchFile,StatusMessage
*
*	process		Prompt the user for a file name
*			Read the file name
*			Open requested file
*			If accessable
*				update file handle and current file name
*			else
*				update status message with error message
*
*	remarks
*
**********************************************************************/

int selectBatchFile(void)
{
	char SelectedFileName[32];
	FILE *FileHandle;

	CLS;
	/* Prompt user for file name */
	_settextposition(4,8);
	_outtext("Please enter a filename.");
	_settextposition(6,8);
	_outtext("filename: "); 

	/* Read filename requested */
	/* SCO 002.003 */
	if(!getFilename(SelectedFileName))
		return 0;

	if ( (FileHandle=fopen(SelectedFileName,"r")) == NULL)
	/* File not accessable */
		{
		strcpy(StatusMessage,"Could not open file ");
		strcat(StatusMessage,SelectedFileName);
		return 0;
		}
	else
	/* A good batch file, update current file being used and
	the file handle */
		{
		strcpy(BatchFileName,SelectedFileName);
		BatchFile = FileHandle;
		return 1;
		}
}

/**********************************************************************
*	Function	processBatchFile()
*
*	purpose		Supervisor for processing a batch file
*
*	inputs		BatchFile handle
*
*	process		Get a line from the batch file
*			Last char is a LF/0AH/'\n' change to a NULL character
*			If command string ends with <> append a CR/0DH/'\r' 
*			followed by a NULL char
*			Send to the instrument
*			Process like a normal response from instrument
*			If EOF exit
*
*	remarks		The format of commandlines in a batch file
*			are every command line in the batch file must
*			end with a CRLF pair. This is actually a
*			LF/0AH as generated by the Norton Editor.
*			If the command line requires a CR, use the
*			<> pair followed by the editor LF.
*
**********************************************************************/

int processBatchFile(void)
{
int len;
	/* Read a line at a time until the end of the batch file */
	while ( fgets(CommandLine,MAXRECORDSIZE,BatchFile) != NULL )
		{
		len = strlen(CommandLine);	/* length of command line */
		CommandLine[len-1] = '\0';	/* convert the newline to */
						/* the null character */
						/* Look for CR symbology */
						/* and convert if CR to ASCII */
						/* value if necessary */
		if ( (CommandLine[len-3]=='<')&&(CommandLine[len-2]=='>') )
			{
			CommandLine[len-3]=CR;
			CommandLine[len-2]=NULL;
			}
		mid_puts(commport,CommandLine);	/* Send to instrument */
		NormalReporting(); 		/* Report any responses */
		}				/* from instrument */
	fclose(BatchFile);
	return 1;				/* Successfully loaded file */
}
