/******************************************************************************

        			THIS IS Terminal_irix.c
        			IRIX Version of Terminal.C

Copyright 1988-1994, Polhemus Incorporated, all rights reserved.
IRIX port 2024

This program interfaces a PC/IRIX user to a 3SPACE instrument. In addition to
a dumb terminal emulation this program supports logging capability.
This program also supports ASCII and binary data collection formats
and additionally supports the 3SPACE continuous mode of operation.
The comm port used for serial interface communication is selectable.
The RS232 communications protocol is configurable.

This IRIX version replaces:
- DOS graphics library with curses
- MID serial driver with termios
- DOS keyboard handling with UNIX terminal I/O
- DOS file operations with UNIX equivalents

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <curses.h>
#include <signal.h>
#include <ctype.h>

/* File handle for log file */
FILE *logfile;
#define MAXRECORDSIZE 300 /* Max buffer size RS232 local buffering */

/* Command line buffer */
char CommandLine[MAXRECORDSIZE];
/* The displayed command line, char sizes */
char DisplaySymbolSize[MAXRECORDSIZE]; 
int position = 0;	/* Command line size */
int serial_fd = -1;	/* Serial port file descriptor */

/* Record received from instrument */
char DataRecord[MAXRECORDSIZE];
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
#define QUIET 'K'
#define NOTQUIET 'm'
#define ERROR '*'
#define ESC 27
#define PRINTABLE 128
#define TRANSMIT 129
#define CTRL_KEY 130

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

int dly = 200;			/* Response time from system */
volatile int running = 1;	/* Program running flag */

/* Default serial device */
char serial_device[64] = "/dev/ttyd1";

/* Function specifications */

/* Main functions */
int main(int argc, char *argv[]);
void doitall(void);
void cleanup_and_exit(int sig);

/* Command line operations */
void processCommandLine(void);
void buildCommandLine(int ch);
void displayCommandLine(char whatChar);
void editCommandLine(void);
void resetCommandLine(void);
int isCommand(int whatCommand);

/* 3SPACE data record operations */
int fetch(char *datarecord);
void ContinuousReporting(void);
int selectedNonContinuous(void);
int operatorBreak(void);
void NormalReporting(void);
void StatusReporting(void);
void FastrakStatus(void);
void ThreeSpaceStatus(void);
char *DigitizerModeName(int Mode);
void Log(char *Record);
void LogBinary(char *Record, int length);
int isRecord(char whatType);

/* Utility and mode support */
void SelectLogFile(void);
FILE *accessLogFile(char *);
int getFilename(char *);
void BinaryToggle(void);
void LoggingToggle(void);
void displayToggle(void);
int setupSerial(const char *device);
void configureSerial(void);

/* Batch mode functions */
void displayBatchMenu(void);
int OptionMenu(void);
void processBatchMode(void);
int selectBatchFile(void);
int processBatchFile(void);

/* Display utilities */
void scrnbot(void);
void displayFunctionBar(void);
void displayHelp(void);
void Banner(void);

/* Keyboard char operations */
int isFunctionKey(int ch);
int isKey(int whatkey, int ch);

/* Serial communication functions */
int serial_write(const char *data);
int serial_read_char(void);
int serial_available(void);
void serial_flush(void);

/* Curses utility functions */
void init_screen(void);
void cleanup_screen(void);
void move_cursor(int row, int col);
void clear_screen(void);
void clear_lines(int start, int end);
void center_text(int row, const char *text);

/****************************************************************
* 	function	main()
*
*	purpose		Gets the program going with various initializations.
*
*	inputs		command line arguments
*
*	outputs		3SPACE commands
*
*	process		Initialize serial interface
*			Put 3SPACE in ASCII Non-Continuous
*			Call interactive loop
*               
*	remarks		none
****************************************************************/

int main(int argc, char *argv[])
{
	/* Handle command line arguments */
	if (argc > 1) {
		strncpy(serial_device, argv[1], sizeof(serial_device) - 1);
		serial_device[sizeof(serial_device) - 1] = '\0';
	}

	/* Set up signal handler for clean exit */
	signal(SIGINT, cleanup_and_exit);
	signal(SIGTERM, cleanup_and_exit);

	Banner();				/* Lead in screen - before curses */

	/* System initializations */
	if (setupSerial(serial_device) < 0) {
		printf("Failed to open serial device %s\n", serial_device);
		printf("Press ENTER to continue anyway...");
		getchar();
	}
	
	serial_write(toASCIICommand);		/* Put 3SPACE in ASCII */
	serial_write(toNonContinuousCommand);	/* noncontinuous mode */

	init_screen();				/* Initialize curses */
	doitall();				/* interactive control loop  */
	
	cleanup_and_exit(0);
	return 0;
}

/****************************************************************
* 	function	cleanup_and_exit()
*
*	purpose		Clean up resources and exit
****************************************************************/

void cleanup_and_exit(int sig)
{
	(void)sig; /* Suppress unused parameter warning */
	running = 0;
	cleanup_screen();
	if (serial_fd >= 0) {
		close(serial_fd);
	}
	if (isLogFileOpen && logfile) {
		fclose(logfile);
	}
	exit(0);
}

/****************************************************************
* 	function	doitall()
*
*	purpose		overall control loop.
****************************************************************/

void doitall()
{
	int ch;
	int length;

	/* Local initializations */
	clear_screen();
	scrnbot();		/* Display command legend */
	resetCommandLine();

	while (running)
	{
		/* Check for keyboard input */
		ch = getch();
		if (ch != ERR) {
			/* Process function keys */
			int func_key = isFunctionKey(ch);
			
			switch(func_key) {
			case 0: /* Any key but a function key */
				if (ch == '\n' || ch == '\r') {  /* Enter key */
					processCommandLine();
					clear_lines(20, 20);
					move_cursor(20, 0);
					printw("COMMAND ");
					refresh();
				}
				else if (ch == '\b' || ch == 127) { /* Backspace */
					editCommandLine();
				}
				else if (ch >= 32 && ch < 127) { /* Printable character */
					buildCommandLine(ch);
					displayCommandLine(ch);
				}
				break;

			case 1:	/* F1 - Continuous data transmit mode */
				serial_write(toContinuousCommand);
				ContinuousReporting();
				break;

			case 2:	/* F2 - Toggle binary/ASCII output */
				BinaryToggle();
				scrnbot();
				break;

			case 3: /* F3 - Enable/disable data logging */
				LoggingToggle();
				scrnbot();
				break;

			case 4: /* F4 - Choose log file */ 
				SelectLogFile();
				scrnbot();
				break;

			case 5: /* F5 - Display formatted status info */
				StatusReporting();
				scrnbot();
				break;

			case 6: /* F6 - Reconfigure serial interface */
				configureSerial();
				scrnbot();
				break;

			case 7: /* F7 - Request single record */
				serial_write(PCommand);
				NormalReporting();
				move_cursor(20, 8);
				break;

			case 8: /* F8 - Exit program */
				cleanup_and_exit(0);
				break;

			case 9: /* F9 - Display help */
				displayHelp();
				scrnbot();
				break;

			case 10: /* F10 - Batch mode */
				processBatchMode();
				scrnbot();
				break;

			case 11: /* F11 - Toggle display */
				displayToggle();
				scrnbot();
				break;
			}
		}

		/* Check for unsolicited data from instrument */
		if (serial_available()) {
			if ((length = fetch(DataRecord))) {
				/* Display formatting */
				if (isBinary) {
					LogBinary(DataRecord, length);
				}
				else {
					move_cursor(19, 0);
					if (displayOn) {
						printw("%s", DataRecord);
					}
					/* Send it to the log */
					Log(DataRecord);
					move_cursor(20, 8);
				}
				refresh();
			}
		}

		/* Small delay to prevent busy waiting */
		usleep(1000);
	}
}

/****************************************************************
* 	Command Line operations
****************************************************************/

void resetCommandLine(void)
{
	CommandLine[0] = '\0';
	position = 0;
}

void processCommandLine(void)
{
	serial_write(CommandLine);	/* Send command to system */
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

void buildCommandLine(int ch)
{
	if (position < MAXRECORDSIZE - 1) {
		CommandLine[position] = ch;
		position++;
		CommandLine[position] = '\0'; 	/* tentative string end */
	}
}

void displayCommandLine(char whatChar)
{
	if (whatChar >= 32 && whatChar < 127) { /* Printable */
		printw("%c", whatChar);
		DisplaySymbolSize[position-1] = 1;
	}
	else if (whatChar == ESC) {
		printw("<esc>");
		DisplaySymbolSize[position-1] = 5;
	}
	else if (whatChar < 32) { /* Control character */
		printw("^%c", whatChar + 64);
		DisplaySymbolSize[position-1] = 2;
	}
	else if (whatChar == CR) {
		printw("<cr>");
		DisplaySymbolSize[position-1] = 4;
	}
	refresh();
}

void editCommandLine()
{
	int row, col;
	int i;
	
	if (position > 0) {
		position--;
		getyx(stdscr, row, col);
		
		/* Move cursor back by the size of the last symbol */
		col -= DisplaySymbolSize[position];
		if (col < 0) col = 0;
		
		move(row, col);
		/* Clear the characters */
		for (i = 0; i < DisplaySymbolSize[position]; i++) {
			printw(" ");
		}
		move(row, col);
		CommandLine[position] = '\0';
		refresh();
	}
}

int isCommand(int whatCommand)
{
	return (CommandLine[0] == whatCommand);
}

/****************************************************************
* 	3SPACE data record operations
****************************************************************/

void ContinuousReporting(void)
{
	int length;
	char buf[256];

	clear_screen();
	move_cursor(22, 0);
	printw("F1= Non-Continuous");
	move_cursor(19, 0);
	if (isBinary) {
		center_text(10, "Collecting Binary Data");
	}
	refresh();
	
	buf[0] = '\0';
	while (running) {
		if ((length = fetch(buf))) {
			if (selectedNonContinuous())
				break;
			
			/* Display formatting */
			if (isBinary) {
				LogBinary(buf, length);
			}
			else {
				if (displayOn) {
					printw("%s", buf);
				}
				/* Send it to the log */
				Log(buf);
			}
			refresh();
		}
		if (selectedNonContinuous())
			break;
		usleep(1000);
	}

	if (isBinary) {
		clear_lines(10, 10);
	}
	scrnbot();
	serial_flush();
	usleep(2 * dly * 1000);
	serial_flush();
}

int selectedNonContinuous(void)
{
	int ch = getch();
	if (ch != ERR) {
		if (ch == ESC || ch == NONCONTINUOUS || isFunctionKey(ch) == 1) {
			serial_write(toNonContinuousCommand);
			return 1;
		}
	}
	return 0;
}

void NormalReporting(void)
{
	int length;

	usleep(dly * 1000);		/* Await response from the system */
	if (!serial_available())	/* Anything in receive buffer? */
		return;			/* System did not respond */
		
	if ((length = fetch(DataRecord))) {
		move_cursor(19, 0);
		if (isBinary) {
			if (!isRecord(ERROR)) {
				printw("Collecting Binary Data\n");
				LogBinary(DataRecord, length);
			}
			else {		/* Display error messages */
				printw("%s", DataRecord);
				Log(DataRecord);
			}
		}
		else {
			if (displayOn) {
				printw("%s", DataRecord);
			}
			Log(DataRecord);
		}
		move_cursor(20, 8);
		refresh();
	}
}

void StatusReporting()
{
	serial_write(retrieveStatusCommand); /* Send command to system */
	usleep(dly * 1000);		/* Wait for a response from the system*/
	
	if (!serial_available()) {	/* Anything in receive buffer? */
		clear_screen();
		center_text(10, "No Status Received");
		refresh();
		getch();
		return;			/* System did not respond */
	}
	
	if (fetch(DataRecord)) {	/* true when complete record is available */
		if (isRecord(STATUS)) {
			clear_screen();
			move_cursor(3, 30);
			printw("  INSTRUMENT STATUS");

			if (ISFastrak)
				FastrakStatus();
			else
				ThreeSpaceStatus();
		}
		else {
			/* If a non-status record was received output it */
			move_cursor(19, 0);
			printw("%s", DataRecord);
		}
		refresh();
		getch();  /* Wait for key press */
	}
}

void FastrakStatus()
{
	move_cursor(5, 30);
	printw("Station Number  %c", DataRecord[1]);

	strncpy(I3, &DataRecord[3], 3);
	sscanf(I3, "%x", &SystemFlags);

	move_cursor(6, 30);
	if (ISSYSTEMFLAG(BINARYFORMAT))
		printw("Binary Data");
	else
		printw("ASCII Data ");

	move_cursor(7, 30);
	if (ISSYSTEMFLAG(METRICUNITS))
		printw("Metric Units ");
	else
		printw("English Units");

	move_cursor(8, 30);
	if (ISSYSTEMFLAG(COMPENSATIONON))
		printw("Compensation On");
	else
		printw("Compensation Off");

	move_cursor(9, 30);
	if (ISSYSTEMFLAG(CONTINUOUSTRANSMIT))
		printw("Continuous Transmission");
	else
		printw("Non-Continuous Transmission");
	
	move_cursor(10, 30);
	if (ISSYSTEMFLAG(TRACKERON))
		printw("Tracker Configuration On ");
	else
		printw("Tracker Configuration Off");
	
	move_cursor(11, 30);
	if (ISSYSTEMFLAG(TRACKERON))
		printw("FASTRAK");
 	else
		printw("3DRAW");
	
	move_cursor(12, 30);
	printw("Digitizer Mode , Sensor 1 %s", DigitizerModeName(CURRENTDIGITIZERMODE));
	
	move_cursor(13, 30);
	printw("Digitizer Mode , Sensor 2 %s", DigitizerModeName(CURRENTDIGITIZERMODE1));
	
	strncpy(BITERR, &DataRecord[6], 3);
	move_cursor(14, 30);
	printw("BITERR  %s", BITERR);

	strncpy(I3, &DataRecord[15], 3);
	sscanf(I3, "%s", Version);
	strncpy(I3, &DataRecord[18], 3);
	sscanf(I3, "%s", Revision);
	move_cursor(15, 30);
	printw("Software Revision  %s%s", Version, Revision);
}

void ThreeSpaceStatus()
{
	move_cursor(5, 30);
	printw("Station Number  %c", DataRecord[1]);

	strncpy(I3, &DataRecord[3], 3);
	sscanf(I3, "%d", &SystemFlags);

	move_cursor(6, 30);
	if (ISSYSTEMFLAG(BINARYFORMAT))
		printw("Binary Data");
	else
		printw("ASCII Data ");

	move_cursor(7, 30);
	if (ISSYSTEMFLAG(METRICUNITS))
		printw("Metric Units ");
	else
		printw("English Units");

	move_cursor(8, 30);
	if (ISSYSTEMFLAG(COMPENSATIONON))
		printw("Compensation On");
	else
		printw("Compensation Off");

	move_cursor(9, 30);
	if (ISSYSTEMFLAG(CONTINUOUSTRANSMIT))
		printw("Continuous Transmission");
	else
		printw("Non-Continuous Transmission");
	
	move_cursor(10, 30);
	if (ISSYSTEMFLAG(TRACKERON))
		printw("Tracker Configuration On ");
	else
		printw("Tracker Configuration Off");
	
	move_cursor(11, 30);
	if (ISSYSTEMFLAG(EXTENDEDON))
		printw("Extended Configuration On ");
	else
		printw("Extended Configuration Off");
	
	move_cursor(12, 30);
	printw("Digitizer Mode %s", DigitizerModeName(CURRENTDIGITIZERMODE));
	
	strncpy(BITERR, &DataRecord[6], 3);
	move_cursor(13, 30);
	printw("BITERR  %s", BITERR);

	strncpy(I3, &DataRecord[15], 3);
	sscanf(I3, "%s", Version);
	strncpy(I3, &DataRecord[18], 3);
	sscanf(I3, "%s", Revision);
	move_cursor(14, 30);
	printw("Software Revision  %s.%s", Version, Revision);

	if (isQuiet) {
		move_cursor(15, 30);
		printw("Quiet Mode");
	}
}

char *DigitizerModeName(int mode)
{
	static char *name[] = { "Point", "Run", "Track", "Off"};
	if (mode >= 0 && mode <= 3)
		return name[mode];
	return "Unknown";
}

void Log(char *Record)
{
	if (LoggingOn && logfile) {
		fputs(Record, logfile);
		fflush(logfile);
	}
}

void LogBinary(char *Record, int length)
{
	if (LoggingOn && logfile) {
		fwrite(Record, 1, length, logfile);
		fflush(logfile);
	}
}

int fetch(char *DataRecord)
{
	int ch;
	int length = 0;
	struct timeval start_time, current_time;
	long timeout_ms = 250;  /* 250ms timeout */
	long elapsed_ms;

	DataRecord[0] = '\0';
	gettimeofday(&start_time, NULL);
	
	while (1) {
		ch = serial_read_char();
		if (ch >= 0) {
			/* Reset timeout on receiving data */
			gettimeofday(&start_time, NULL);
			
			/* Assemble into the record buffer */
			DataRecord[length++] = (char)(0xFF & ch);
			
			/* Check for CR LF record terminator pair */
			if (length > 1) {
				if (DataRecord[length - 2] == '\r' && 
				    DataRecord[length - 1] == '\n') {
					DataRecord[length] = '\0';
					return length;
				}
			}

			/* Prevent buffer overflow */
			if (length >= MAXRECORDSIZE - 1) {
				center_text(10, "A port read error has occurred");
				refresh();
				return 0;
			}
		}
		else {
			/* Check for timeout */
			gettimeofday(&current_time, NULL);
			elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000 +
					 (current_time.tv_usec - start_time.tv_usec) / 1000;
			
			if (elapsed_ms > timeout_ms) {
				if (length > 0) {
					center_text(10, "A port read error has occurred");
					refresh();
				}
				DataRecord[length] = '\0';
				return length;
			}
		}
		usleep(1000);  /* Small delay */
	}
}

int isRecord(char whatType)
{
	return DataRecord[2] == whatType;
}

/****************************************************************
* 	Utility and mode support
****************************************************************/

void BinaryToggle(void)
{
	clear_screen();
	if (isBinary) {
		center_text(10, "Switched to ASCII Mode");
		serial_write(toASCIICommand);
		isBinary = 0;
	}
	else {
		center_text(10, "Switched to Binary Mode");
		serial_write(toBinaryCommand);
		isBinary = 1;
	}
	refresh();
	usleep(1000000);  /* 1 second delay */
}

void LoggingToggle(void)
{
	clear_screen();
	if (LoggingOn) {
		center_text(10, "Disabled Data Logging");
		LoggingOn = 0;
	}
	else {
		if (isLogFileOpen) {
			center_text(10, "Enabled Data Logging");
			LoggingOn = 1;
		}
		else {
			center_text(10, "Must select a log file");
		}
	}
	refresh();
	usleep(1000000);  /* 1 second delay */
}

void displayToggle(void)
{
	clear_screen();
	if (displayOn) {
		center_text(10, "Displaying of Collected Data Disabled");
		displayOn = 0;
	}
	else {
		center_text(10, "Displaying of Collected Data Enabled");
		displayOn = 1;
	}
	refresh();
	usleep(1000000);  /* 1 second delay */
}

void configureSerial(void)
{
	char device[64];
	
	clear_screen();
	move_cursor(10, 0);
	printw("Enter serial device [%s]: ", serial_device);
	refresh();
	
	echo();
	if (fgets(device, sizeof(device), stdin) != NULL) {
		/* Remove newline if present */
		char *newline = strchr(device, '\n');
		if (newline) *newline = '\0';
	} else {
		device[0] = '\0';
	}
	noecho();
	
	if (strlen(device) > 0) {
		strcpy(serial_device, device);
		
		if (setupSerial(serial_device) < 0) {
			center_text(12, "Failed to configure serial port");
			refresh();
			getch();
		}
		else {
			center_text(12, "Serial port configured successfully");
			refresh();
			usleep(1000000);
		}
	}
}

void SelectLogFile()
{
	static char filename[64];
	int ch;

	/* Option to continue to use a previously opened file */
	if (isLogFileOpen) {
		clear_screen();
		move_cursor(10, 0);
		printw("Log file %s is currently open.", filename);
		move_cursor(11, 0);
		printw("Do you want to use this file? (y/n): ");
		refresh();
		
		ch = getch();
		if (ch == ESC) {
			clear_screen();
			return;
		}
		if (tolower(ch) == 'y') {
			clear_screen();
			center_text(10, "Enabled Data Logging");
			LoggingOn = 1;
			refresh();
			usleep(1000000);
			return;
		}
		else {
			fclose(logfile);
			isLogFileOpen = 0;
		}
	}

	while (1) {
		clear_screen();
		move_cursor(10, 0);
		printw("Messages to/from the System on RS-232 will be logged");
		move_cursor(11, 0);
		printw("Do you want to use the default log file name: SYSDATA.LOG");
		move_cursor(12, 0);
		printw("(y/n): ");
		refresh();
		
		ch = getch();
		if (ch == ESC) {
			clear_screen();
			return;
		}
		
		if (tolower(ch) == 'n') {
			clear_screen();
			move_cursor(10, 0);
			printw("Please enter a log file name: ");
			refresh();
			
			echo();
			if (fgets(filename, sizeof(filename), stdin) != NULL) {
				/* Remove newline if present */
				char *newline = strchr(filename, '\n');
				if (newline) *newline = '\0';
			} else {
				filename[0] = '\0';
			}
			noecho();
			
			if (strlen(filename) == 0) {
				return;
			}
		}
		else {
			strcpy(filename, "sysdata.log");
		}

		if ((logfile = accessLogFile(filename))) {
			clear_screen();
			center_text(10, "Enabled Data Logging");
			isLogFileOpen = 1;
			LoggingOn = 1;
			refresh();
			usleep(1000000);
			return;
		}
	}
}

FILE *accessLogFile(char *filename)
{
	int ch;
	FILE *fp;
	
	/* If file does not exist, attempt to open it */
	if (access(filename, F_OK) != 0) {
		if ((fp = fopen(filename, "wb"))) {
			return fp;
		}
		else {
			clear_screen();
			move_cursor(10, 0);
			printw("I can't open %s", filename);
			move_cursor(11, 0);
			printw("Press any key to continue.");
			refresh();
			getch();
			return NULL;
		}
	}
	else {
		/* If the file exists, check if it is write protected */
		if (access(filename, W_OK) != 0) {
			return NULL;  /* Write protected */
		}
		
		clear_screen();
		move_cursor(10, 0);
		printw("A file named \"%s\" already exists.", filename);
		move_cursor(11, 0);
		printw("Choose an option:");
		move_cursor(12, 0);
		printw("0 - Over-write the named file");
		move_cursor(13, 0);
		printw("1 - Append to this log file");
		move_cursor(14, 0);
		printw("2 - Choose another file name");
		move_cursor(15, 0);
		printw("OPTION: ");
		refresh();

		ch = getch();
		switch (ch) {
		case '0':	/* Want to overwrite the existing file */
			return fopen(filename, "wb");
		case '1':	/* Want to append to the existing file */
			return fopen(filename, "ab");
		default:     /* Anything else assumes you want choose another */
			return NULL;
		}
	}
	return NULL;
}

void scrnbot()
{
	clear_lines(20, 24);
	displayFunctionBar();
	move_cursor(20, 0);
	printw("COMMAND ");
	refresh();
}

void displayFunctionBar()
{
	move_cursor(22, 0);
	printw("TOGGLES  ");
	move_cursor(22, 12);
	printw("F1= Continuous ");
	
	if (isBinary)
		printw("F2= ASCII   ");
	else
		printw("F2= Binary  ");
		
	if (LoggingOn)
		printw("F3= Log Off "); 
	else
		printw("F3= Log On  "); 
		
	if (displayOn)
		printw("F11= Display Off "); 
	else
		printw("F11= Display On  "); 
		
	move_cursor(23, 0);
	printw("UTILITIES ");
	move_cursor(23, 12);
	printw("F4= Select Log   F5= Status  F6= Setup     F7=  Request Data"); 
	move_cursor(24, 12);
	printw("F8= Exit         F9= Help    F10=Com File  ENTER= Send Command");
}

void displayHelp(void)
{
	clear_screen();
	move_cursor(5, 0);
	printw("3SPACE INSTRUMENT COMMANDS HELP\n\n");
	printw("Basic Commands:\n");
	printw("  P     - Request a single data record\n");
	printw("  C     - Enter continuous mode\n");
	printw("  c     - Exit continuous mode\n");
	printw("  F     - ASCII data format\n");
	printw("  f     - Binary data format\n");
	printw("  S     - Status request\n");
	printw("  R     - Reset to factory defaults\n");
	printw("  W     - Save current settings\n\n");
	printw("Output Control:\n");
	printw("  O1,2,3,4,5    - Output format (position & angles)\n");
	printw("  U1            - Metric units\n");
	printw("  u1            - English units\n\n");
	printw("Press any key to continue...");
	refresh();
	getch();
}

void Banner(void)
{
	printf("\n");
	printf("3SPACE C LANGUAGE INTERFACE TO IRIX - Terminal, Ver 2.04 IRIX\n");
	printf("Copyright 1988-1994, Polhemus Incorporated, all rights reserved.\n");
	printf("IRIX Port 2024\n");
	printf("Default serial device: %s\n", serial_device);
	printf("Press ENTER to continue.");
	getchar();
}

/****************************************************************
* 	Keyboard character operations
****************************************************************/

int isFunctionKey(int ch)
{
	switch (ch) {
		case KEY_F(1): return 1;
		case KEY_F(2): return 2;
		case KEY_F(3): return 3;
		case KEY_F(4): return 4;
		case KEY_F(5): return 5;
		case KEY_F(6): return 6;
		case KEY_F(7): return 7;
		case KEY_F(8): return 8;
		case KEY_F(9): return 9;
		case KEY_F(10): return 10;
		case KEY_F(11): return 11;
		default: return 0;
	}
}

int isKey(int whatKey, int ch)
{
	if (whatKey == PRINTABLE)
		return (ch > 31 && ch < 127);
	if (whatKey == TRANSMIT)
		return (ch == '\n' || ch == '\r');
	if (whatKey == CTRL_KEY)
		return (ch > 0 && ch < 27 && ch != '\r' && ch != '\b');
	return (ch == whatKey);
}

/****************************************************************
* 	Batch Mode operations
****************************************************************/

void processBatchMode(void)
{
	int ExitLoop = 0;
	char choice;

	while (!ExitLoop) {
		choice = OptionMenu();
		switch (choice) {
		case '0':	/* Select a new input file */
			if (selectBatchFile())
				ExitLoop = processBatchFile();
			break;

		case '1':	/* Reload the current input file */
			if ((BatchFile = fopen(BatchFileName, "r")))
				ExitLoop = processBatchFile();
			else {
				BEEP;
				strcpy(StatusMessage, "Invalid option selected");
			}
			break;

		case '2': case ESC: /* Exit the batch mode */
			ExitLoop = 1;
			break;

		default:	/* Invalid option selected */
			BEEP;
			strcpy(StatusMessage, "Invalid option selected");
			break;
		}
	}
	clear_screen();
}

int OptionMenu(void)
{
	char choice;

	clear_screen();
	displayBatchMenu();
	choice = getch();
	return choice;
}

void displayBatchMenu()
{
	move_cursor(3, 0);
	printw("        Choose an option.");
	move_cursor(5, 0);
	printw("    OPTION #         ACTION ");

	move_cursor(7, 0);
	printw("           0         Select and send a command file");
	
	if (BatchFileName[0]) {
		move_cursor(8, 0);
		printw("           1         Send current command file %s", BatchFileName);
	}
	else {
		move_cursor(8, 0);
		printw("                                                   ");
	}

	move_cursor(9, 0);
	printw("           2         Return to main menu ");

	move_cursor(15, 8);
	printw("%s", StatusMessage);
	StatusMessage[0] = '\0';

	move_cursor(11, 4);
	printw("    OPTION: ");
	refresh();
}

int selectBatchFile(void)
{
	char SelectedFileName[32];
	FILE *FileHandle;

	clear_screen();
	move_cursor(3, 8);
	printw("Please enter a filename.");
	move_cursor(5, 8);
	printw("filename: ");
	refresh();

	echo();
	if (fgets(SelectedFileName, sizeof(SelectedFileName), stdin) != NULL) {
		/* Remove newline if present */
		char *newline = strchr(SelectedFileName, '\n');
		if (newline) *newline = '\0';
	} else {
		SelectedFileName[0] = '\0';
	}
	noecho();

	if (strlen(SelectedFileName) == 0)
		return 0;

	if ((FileHandle = fopen(SelectedFileName, "r")) == NULL) {
		strcpy(StatusMessage, "Could not open file ");
		strcat(StatusMessage, SelectedFileName);
		return 0;
	}
	else {
		strcpy(BatchFileName, SelectedFileName);
		BatchFile = FileHandle;
		return 1;
	}
}

int processBatchFile(void)
{
	int len;
	
	/* Read a line at a time until the end of the batch file */
	while (fgets(CommandLine, MAXRECORDSIZE, BatchFile) != NULL) {
		len = strlen(CommandLine);
		CommandLine[len-1] = '\0';	/* convert the newline to null */
		
		/* Look for CR symbology and convert if necessary */
		if (len >= 3 && CommandLine[len-3] == '<' && CommandLine[len-2] == '>') {
			CommandLine[len-3] = CR;
			CommandLine[len-2] = '\0';
		}
		
		serial_write(CommandLine);	/* Send to instrument */
		NormalReporting(); 		/* Report any responses */
	}
	fclose(BatchFile);
	return 1;				/* Successfully loaded file */
}

/****************************************************************
* 	Serial communication functions
****************************************************************/

int setupSerial(const char *device)
{
	struct termios tty;

	if (serial_fd >= 0) {
		close(serial_fd);
	}

	serial_fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (serial_fd < 0) {
		perror("Error opening serial port");
		return -1;
	}

	/* Get current serial port settings */
	if (tcgetattr(serial_fd, &tty) != 0) {
		perror("Error getting serial attributes");
		close(serial_fd);
		serial_fd = -1;
		return -1;
	}

	/* Set baud rates */
	cfsetispeed(&tty, B9600);
	cfsetospeed(&tty, B9600);

	/* 8 bits, no parity, 1 stop bit */
	tty.c_cflag &= ~PARENB;		/* Clear parity bit */
	tty.c_cflag &= ~CSTOPB;		/* Clear stop field, only one stop bit */
	tty.c_cflag &= ~CSIZE;		/* Clear all size bits */
	tty.c_cflag |= CS8;		/* 8 bits per byte */
	/* tty.c_cflag &= ~CRTSCTS; */	/* Disable RTS/CTS hardware flow control - not available on all systems */
	tty.c_cflag |= CREAD | CLOCAL;	/* Turn on READ & ignore ctrl lines */

	/* Make raw */
	tty.c_lflag &= ~ICANON;
	tty.c_lflag &= ~ECHO;		/* Disable echo */
	tty.c_lflag &= ~ECHOE;		/* Disable erasure */
	tty.c_lflag &= ~ECHONL;		/* Disable new-line echo */
	tty.c_lflag &= ~ISIG;		/* Disable interpretation of INTR, QUIT and SUSP */
	tty.c_iflag &= ~(IXON | IXOFF | IXANY); /* Turn off s/w flow ctrl */
	tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); /* Disable any special handling of received bytes */

	tty.c_oflag &= ~OPOST;		/* Prevent special interpretation of output bytes */
	tty.c_oflag &= ~ONLCR;		/* Prevent conversion of newline to carriage return/line feed */

	/* Set timeouts */
	tty.c_cc[VTIME] = 1;		/* Wait for up to 0.1s, returning as soon as any data is received */
	tty.c_cc[VMIN] = 0;

	/* Save tty settings */
	if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
		perror("Error setting serial attributes");
		close(serial_fd);
		serial_fd = -1;
		return -1;
	}

	return 0;
}

int serial_write(const char *data)
{
	int len;
	if (serial_fd < 0) return -1;
	
	len = strlen(data);
	return write(serial_fd, data, len);
}

int serial_read_char(void)
{
	unsigned char ch;
	int result;
	
	if (serial_fd < 0) return -1;
	
	result = read(serial_fd, &ch, 1);
	if (result == 1) {
		return ch;
	}
	return -1;
}

int serial_available(void)
{
	fd_set readfs;
	struct timeval timeout;
	
	if (serial_fd < 0) return 0;
	
	FD_ZERO(&readfs);
	FD_SET(serial_fd, &readfs);
	
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	
	return select(serial_fd + 1, &readfs, NULL, NULL, &timeout) > 0;
}

void serial_flush(void)
{
	if (serial_fd >= 0) {
		tcflush(serial_fd, TCIOFLUSH);
	}
}

/****************************************************************
* 	Curses utility functions
****************************************************************/

void init_screen(void)
{
	if (initscr() == NULL) {
		fprintf(stderr, "Error initializing curses\n");
		exit(1);
	}
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	scrollok(stdscr, TRUE);
	clear();
	refresh();
}

void cleanup_screen(void)
{
	if (stdscr != NULL) {
		endwin();
	}
}

void move_cursor(int row, int col)
{
	move(row, col);
}

void clear_screen(void)
{
	clear();
	refresh();
}

void clear_lines(int start, int end)
{
	int i;
	for (i = start; i <= end; i++) {
		move(i, 0);
		clrtoeol();
	}
	refresh();
}

void center_text(int row, const char *text)
{
	int cols = getmaxx(stdscr);
	int start_col = (cols - strlen(text)) / 2;
	if (start_col < 0) start_col = 0;
	
	move(row, start_col);
	printw("%s", text);
}
