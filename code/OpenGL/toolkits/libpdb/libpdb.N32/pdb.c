/*****************************************************************************
 * pdb - routines for maintaining a database of performance information
 *
 * Porting Notes:
 *
 *	ANSI C (including library routines specified by the standard) is
 *	used throughout.
 *
 *	The routines GetDBFileName() and GetDefaultMachineName() should be
 *	modified when porting to non-UNIX systems.
 *
 *	There are a few ``X-isms'' in this code; for example, the use of
 *	the DISPLAY environment variable to fetch the default machine
 *	name.  These would have to be changed when porting to OS/2 or
 *	Windows.
 *
 *	GetClock() should be modified when porting to non-SVR4 systems, or
 *	to systems whose "double" type has fewer bits of mantissa than
 *	specified by IEEE 754.
 *
 * History:
 *
 *	1.0	9/93	akin	Written.  See accompanying README for
 *				rationale and examples.
 *	2.0	3/95	akin	Added support for version strings
 *				(necessitating a new database file and
 *				format) and made calibration optional in
 *				pdbMeasureRate().  Allowed more special
 *				characters in strings.  Cleaned up some
 *				nits.
 *****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>	/* for gettimeofday, used by GetClock */
#include "pdb.h"


typedef struct HashNodeS {
	struct HashNodeS*	next;
	char*			machineName;
	char*			applicationName;
	char*			benchmarkName;
	char*			versionString;
	double			rate;
	char			storage[1];
	} HashNodeT;
static HashNodeT** HashTable = NULL;
#define HASH_STEP(hash,c) (hash)+=(c)
#define HASH_TABLE_SIZE 64
#define HASH_MODULUS(hash) (hash)&=(HASH_TABLE_SIZE-1)


static int	Dirty = 0;
static char*	Hex = "0123456789ABCDEF";
static double	Overhead = 0.0;


#define LINE_MAX 1024


static double	  ChooseRunTime		(void);
static pdbStatusT DumpHashTable		(FILE*		dbFile);
static void	  FinalizeHashTable	(void);
static double	  GetClock		(void);
static void	  GetDBFileName		(char*		name);
static void	  GetDefaultMachineName	(char*		name);
static pdbStatusT InitializeHashTable	(void);
static pdbStatusT InsertHashNode	(const char*	machineName,
					 int		machineNameLength,
					 const char*	applicationName,
					 int		applicationNameLength,
					 const char*	benchmarkName,
					 int		benchmarkNameLength,
					 const char*	versionString,
					 int		versionStringLength,
					 double		rate,
					 unsigned	hash);
static pdbStatusT LoadHashTable		(FILE*		dbFile);
static pdbStatusT LookupHashNode	(HashNodeT**	nodeP,
					 const char*	machineName,
					 const char*	applicationName,
					 const char*	benchmarkName,
					 const char*	versionString,
					 int		createWhenMissing);
static void	  StringToToken		(char*		dst,
					 char*		src);
static char*	  TokenToString		(char*		dst,
					 char*		src);
static double	  WaitForTick		(void);


/*****************************************************************************
 * ChooseRunTime - select an appropriate runtime for benchmarks
 *****************************************************************************/

static double
ChooseRunTime(void) {
	double start;
	double finish;
	double		runTime;

	start = GetClock();

	/* Wait for next tick: */
	while ((finish = GetClock()) == start)
		;
	
	/* Run for 100 ticks, clamped to [1.0 sec, 5.0 sec]: */
	runTime = 100.0 * (finish - start);
	if (runTime < 1.0)
		runTime = 1.0;
	else if (runTime > 5.0)
		runTime = 5.0;

	return runTime;
	}


/*****************************************************************************
 * DumpHashTable - write contents of hash table to database file
 *****************************************************************************/

static pdbStatusT
DumpHashTable(FILE* dbFile) {
	int		i;
	HashNodeT*	n;
	char machineName[LINE_MAX];
	char applicationName[LINE_MAX];
	char benchmarkName[LINE_MAX];
	char versionString[LINE_MAX];

	if (!HashTable)
		return PDB_NOT_OPEN;

	for (i = 0; i < HASH_TABLE_SIZE; ++i)
		for (n = HashTable[i]; n; n = n->next) {
			StringToToken(machineName, n->machineName);
			StringToToken(applicationName, n->applicationName);
			StringToToken(benchmarkName, n->benchmarkName);
			StringToToken(versionString, n->versionString);
			fprintf(dbFile, "%s\t%s\t%s\t%s\t%g\n",
			    machineName,
			    applicationName,
			    benchmarkName,
			    versionString,
			    n->rate);
			}

	return PDB_NO_ERROR;
	}


/*****************************************************************************
 * FinalizeHashTable - deallocate all storage used by the hash table
 *****************************************************************************/

static void
FinalizeHashTable(void) {
	int		i;
	HashNodeT*	h;
	HashNodeT*	next;

	if (!HashTable)
		return;

	for (i = HASH_TABLE_SIZE - 1; i >= 0; --i)
		for (h = HashTable[i]; h; h = next) {
			next = h->next;
			free(h);
			}
	
	free(HashTable);
	HashTable = NULL;
	}


/*****************************************************************************
 * GetClock - get current time (expressed in seconds)
 *****************************************************************************/

static double
GetClock(void) {
	struct timeval t;

	gettimeofday(&t);

	return (double) t.tv_sec + (double) t.tv_usec * 1E-6;
	}


/*****************************************************************************
 * GetDBFileName - get full pathname of performance database file
 *****************************************************************************/

static void
GetDBFileName(char* name) {
	char* home;

	if (home = getenv("HOME"))
		strcpy(name, home);
	else
		name[0] = '\0';
	strcat(name, "/.pdb2");
	}


/*****************************************************************************
 * GetDefaultMachineName - return name of "current" machine
 *****************************************************************************/

static void
GetDefaultMachineName(char* name) {
	char* display;

	if (display = getenv("DISPLAY"))
		strcpy(name, display);
	else
		strcpy(name, ":0");
	}


/*****************************************************************************
 * InitializeHashTable - allocate memory for hash table and initialize it
 *****************************************************************************/

static pdbStatusT
InitializeHashTable(void) {
	int i;

	HashTable = (HashNodeT**)
	    malloc(HASH_TABLE_SIZE * sizeof(HashNodeT*));
	if (!HashTable)
		return PDB_OUT_OF_MEMORY;

	for (i = HASH_TABLE_SIZE - 1; i >= 0; --i)
		HashTable[i] = NULL;
	
	return PDB_NO_ERROR;
	}


/*****************************************************************************
 * InsertHashNode - place key and data in a selected bucket of the hash table
 *
 * Note:  Does not check for duplicates.
 *	  String length arguments *include* the zero byte at the end of the
 *		string; e.g. "abc" would have length 4.
 *****************************************************************************/

static pdbStatusT
InsertHashNode (
    const char*	machineName,
    int		machineNameLength,
    const char*	applicationName,
    int		applicationNameLength,
    const char*	benchmarkName,
    int		benchmarkNameLength,
    const char*	versionString,
    int		versionStringLength,
    double	rate,
    unsigned	hash
    ) {
	HashNodeT*	n;

	if (!HashTable)
		return PDB_NOT_OPEN;

	n = (HashNodeT*) malloc(sizeof(HashNodeT) + machineNameLength
	    + applicationNameLength + benchmarkNameLength
	    + versionStringLength);
	if (!n)
		return PDB_OUT_OF_MEMORY;

	n->machineName = n->storage;
	memcpy(n->machineName, machineName, machineNameLength);
	n->applicationName = n->machineName + machineNameLength;
	memcpy(n->applicationName, applicationName, applicationNameLength);
	n->benchmarkName = n->applicationName + applicationNameLength;
	memcpy(n->benchmarkName, benchmarkName, benchmarkNameLength);
	n->versionString = n->benchmarkName + benchmarkNameLength;
	memcpy(n->versionString, versionString, versionStringLength);
	n->rate = rate;

	n->next = HashTable[hash];
	HashTable[hash] = n;

	return PDB_NO_ERROR;
	}


/*****************************************************************************
 * LoadHashTable - load hash table with contents of performance database file
 *****************************************************************************/

static pdbStatusT
LoadHashTable(FILE* dbFile) {
	char		line[LINE_MAX];
	pdbStatusT	error = PDB_NO_ERROR;

	if (!HashTable)
		return PDB_NOT_OPEN;

	while (fgets(line, sizeof(line), dbFile)) {
		char		machineName[LINE_MAX];
		char		applicationName[LINE_MAX];
		char		benchmarkName[LINE_MAX];
		char		versionString[LINE_MAX];
		int		machineNameLength;
		int		applicationNameLength;
		int		benchmarkNameLength;
		int		versionStringLength;
		double		rate;
		char*		p;
		char*		q;
		int		c;
		unsigned	hash;


		p = line;
		hash = 0;

		/* Skip whitespace before machine name: */
		while (isspace(*p))
			++p;
		if (!*p) {
			error |= PDB_SYNTAX_ERROR;
			continue;
			}

		/* Scan machine name, get length and hash value: */
		p = TokenToString(machineName, p);
		for (q = machineName; c = *q++;)
			HASH_STEP(hash, c);
		machineNameLength = q - machineName;	/* includes '\0' */

		/* Skip whitespace before application name: */
		while (isspace(*p))
			++p;
		if (!*p) {
			error |= PDB_SYNTAX_ERROR;
			continue;
			}

		/* Scan application name, get length and hash: */
		p = TokenToString(applicationName, p);
		for (q = applicationName; c = *q++;)
			HASH_STEP(hash, c);
		applicationNameLength = q - applicationName; /* includes '\0' */
		
		/* Skip whitespace before benchmark name: */
		while (isspace(*p))
			++p;
		if (!*p) {
			error |= PDB_SYNTAX_ERROR;
			continue;
			}

		/* Scan benchmark name, get length and hash: */
		p = TokenToString(benchmarkName, p);
		for (q = benchmarkName; c = *q++;)
			HASH_STEP(hash, c);
		benchmarkNameLength = q - benchmarkName; /* includes '\0' */
		
		/* Skip whitespace before version string: */
		while (isspace(*p))
			++p;
		if (!*p) {
			error |= PDB_SYNTAX_ERROR;
			continue;
			}

		/* Scan version string, get length and hash: */
		p = TokenToString(versionString, p);
		for (q = versionString; c = *q++;)
			HASH_STEP(hash, c);
		versionStringLength = q - versionString; /* includes '\0' */

		/* Finally, get the rate: */
		rate = strtod(p, NULL);
		if (rate <= 0.0)
			error |= PDB_SYNTAX_ERROR;	/* probably */


		HASH_MODULUS(hash);


		/* Note that we don't weed out any duplicates here... */

		error |= InsertHashNode(
		    machineName, machineNameLength,
		    applicationName, applicationNameLength,
		    benchmarkName, benchmarkNameLength,
		    versionString, versionStringLength,
		    rate, hash);
		}

	return error;
	}

	
/*****************************************************************************
 * LookupHashNode - find key/data node in hash table, or insert it if needed
 *****************************************************************************/

static pdbStatusT
LookupHashNode (
    HashNodeT**	nodeP,
    const char*	machineName,
    const char*	applicationName,
    const char*	benchmarkName,
    const char* versionString,
    int		createWhenMissing
    ) {
	char		defaultMachineName[LINE_MAX];
	const char*	p;
	int		c;
	unsigned	hash;
	HashNodeT*	node;
	int		machineNameLength;
	int		applicationNameLength;
	int		benchmarkNameLength;
	int		versionStringLength;
	pdbStatusT	error;

	if (!HashTable)
		return PDB_NOT_OPEN;

	if (!machineName) {
		GetDefaultMachineName(defaultMachineName);
		machineName = defaultMachineName;
		}

	hash = 0;

	for (p = machineName; c = *p++;)
		HASH_STEP(hash, c);
	machineNameLength = p - machineName;	/* includes '\0' at end */
	for (p = applicationName; c = *p++;)
		HASH_STEP(hash, c);
	applicationNameLength = p - applicationName;
	for (p = benchmarkName; c = *p++;)
		HASH_STEP(hash, c);
	benchmarkNameLength = p - benchmarkName;
	for (p = versionString; c = *p++;)
		HASH_STEP(hash, c);
	versionStringLength = p - versionString;
	
	HASH_MODULUS(hash);

	for (node = HashTable[hash]; node; node = node->next)
		if (!strcmp(node->machineName, machineName)
		 && !strcmp(node->applicationName, applicationName)
		 && !strcmp(node->benchmarkName, benchmarkName)
		 && !strcmp(node->versionString, versionString)) {
			*nodeP = node;
			return PDB_NO_ERROR;
			}

	if (createWhenMissing) {
		error = InsertHashNode(
		    machineName, machineNameLength,
		    applicationName, applicationNameLength,
		    benchmarkName, benchmarkNameLength,
		    versionString, versionStringLength,
		    0.0, hash);
		*nodeP = HashTable[hash];
		return error;
		}
	else
		return PDB_NOT_FOUND;
	}


/*****************************************************************************
 * StringToToken - convert string to a token with escape sequences
 *****************************************************************************/

static void
StringToToken(char* dst, char* src) {
	char c;

	while (c = *src++)
		if (c == '\\') {
			*dst++ = c;
			*dst++ = c;
			}
		else if (isgraph(c))
			*dst++ = c;
		else
			switch (c) {
				case ' ':
					*dst++ = '\\';
					*dst++ = ' ';
					break;
				case '\t':
					*dst++ = '\\';
					*dst++ = 't';
					break;
				case '\r':
					*dst++ = '\\';
					*dst++ = 'r';
					break;
				case '\n':
					*dst++ = '\\';
					*dst++ = 'n';
					break;
				case '\v':
					*dst++ = '\\';
					*dst++ = 'v';
					break;
				case '\f':
					*dst++ = '\\';
					*dst++ = 'f';
					break;
				case '\b':
					*dst++ = '\\';
					*dst++ = 'b';
					break;
				case '\a':
					*dst++ = '\\';
					*dst++ = 'a';
					break;
				default:
					*dst++ = '\\';
					*dst++ = 'x';
					*dst++ = Hex[(c >> 4) & 0xF];
					*dst++ = Hex[c & 0xF];
					break;
				}
	*dst++ = 0;
	}


/*****************************************************************************
 * TokenToString - convert space-delimited token to string, interpreting
 *	escape sequences
 *****************************************************************************/

static char*
TokenToString(char* dst, char* src) {
	char c;

	while (c = *src++)
		if (c == '\\')
			switch (c = *src++) {
				case '\\':
				case ' ':
					*dst++ = c;
					break;
				case 't':
					*dst++ = '\t';
					break;
				case 'r':
					*dst++ = '\r';
					break;
				case 'n':
					*dst++ = '\n';
					break;
				case 'v':
					*dst++ = '\v';
					break;
				case 'f':
					*dst++ = '\f';
					break;
				case 'b':
					*dst++ = '\b';
					break;
				case 'a':
					*dst++ = '\a';
					break;
				case 'x':
					c = *src++;
					*dst = (strchr(Hex, c) - Hex) << 4;
					c = *src++;
					*dst++ |= (strchr(Hex, c) - Hex);
					break;
				default:
					*dst++ = '\\';
					*dst++ = c;
					break;
				}
		else if (isspace(c))
			break;
		else
			*dst++ = c;
	*dst++ = 0;
	return src;
	}


/*****************************************************************************
 * WaitForTick - wait for beginning of next system clock tick; return the time
 *****************************************************************************/

static double
WaitForTick(void) {
	double start;
	double current;

	start = GetClock();

	/* Wait for next tick: */
	while ((current = GetClock()) == start)
		;

	/* Start timing: */
	return current;
	}


/*****************************************************************************
 * pdbClose - write perf data to database file if necessary, then clean up
 *****************************************************************************/

pdbStatusT
pdbClose(void) {
	pdbStatusT	error = PDB_NO_ERROR;
	char		dbFileName[FILENAME_MAX];
	FILE*		dbFile;

	if (!HashTable)
		return PDB_NOT_OPEN;

	if (Dirty) {
		GetDBFileName(dbFileName);

		if (dbFile = fopen(dbFileName, "w")) {
			error = DumpHashTable(dbFile);
			fclose(dbFile);
			}
		else
			error = PDB_CANT_WRITE;

		Dirty = 0;
		}

	FinalizeHashTable();

	return error;
	}


/*****************************************************************************
 * pdbMeasureRate - measure number of caller's operations performed per second
 *****************************************************************************/

pdbStatusT
pdbMeasureRate (
    pdbCallbackT	initialize,
    pdbCallbackT	operation,
    pdbCallbackT	finalize,
    int			calibrate,
    double*		rate
    ) {
	double		runTime;
	long		reps;
	long		newReps;
	long		i;
	double		start;
	double		current;


	if (!operation) {
		*rate = 0.0;
		return PDB_NO_ERROR;
		}


	/* Select a run time that's appropriate for our timer resolution: */
	runTime = ChooseRunTime();


	/* Measure approximate overhead for finalization and timing routines: */
	if (calibrate) {
		if (initialize)
			(*initialize)();
		reps = 0;
		start = WaitForTick();
		do {
			if (finalize)
				(*finalize)();
			++reps;
			} while ((current = GetClock()) < start + runTime);
		Overhead = (current - start) / (double) reps;
		}


	/*
	 * Measure successively larger batches of operations until we find
	 * one that's long enough to meet our runtime target:
	 */
	reps = 1;
	for (;;) {
		if (initialize)
			(*initialize)();

		start = WaitForTick();

		for (i = reps; i > 0; --i)
			(*operation)();

		if (finalize)
			(*finalize)();

		current = GetClock();
		if (current >= start + runTime + Overhead)
			break;

		/* Try to reach runtime target in one fell swoop: */
		if (current > start)
			newReps = reps *
				(0.5 + runTime / (current - start - Overhead));
		else
			newReps = reps * 2;
		if (newReps == reps)
			reps += 1;
		else
			reps = newReps;
		}

	/* Subtract overhead to determine the final operation rate: */
	*rate = (double) reps / (current - start - Overhead);
	return PDB_NO_ERROR;
	}


/*****************************************************************************
 * pdbOpen - open perf database file, load contents into memory
 *****************************************************************************/

pdbStatusT
pdbOpen(void) {
	pdbStatusT	error;
	char		dbFileName[FILENAME_MAX];
	FILE*		dbFile;

	if (HashTable)
		return PDB_ALREADY_OPEN;

	if (error = InitializeHashTable())
		return error;

	/* If the database file can be read, load its contents: */
	GetDBFileName(dbFileName);
	if (dbFile = fopen(dbFileName, "r")) {
		error = LoadHashTable(dbFile);
		fclose(dbFile);
		}

	/* The database is "clean" unless pdbWriteRate() is called: */
	Dirty = 0;

	return error;
	}


/*****************************************************************************
 * pdbReadRate - return performance for a given machine, app, benchmark,
 *	and version
 *****************************************************************************/

pdbStatusT
pdbReadRate (
    const char*		machineName,
    const char*		applicationName,
    const char*		benchmarkName,
    const char*		versionString,
    double*		rate
    ) {
	HashNodeT*	node;
	pdbStatusT	error;

	error = LookupHashNode(&node, machineName, applicationName,
	    benchmarkName, versionString, 0);
	    /* don't create node if it's not present */
	if (!error)
		*rate = node->rate;

	return error;
	}


/*****************************************************************************
 * pdbWriteRate - save performance data for a given machine, app, benchmark,
 *	and version
 *****************************************************************************/

pdbStatusT
pdbWriteRate (
    const char*		machineName,
    const char*		applicationName,
    const char*		benchmarkName,
    const char*		versionString,
    const double	rate
    ) {
	HashNodeT*	node;
	pdbStatusT	error;

	Dirty = 1;

	error = LookupHashNode(&node, machineName, applicationName,
	   benchmarkName, versionString, 1);
	   /* create node if it's not already in table */
	if (!error)
		node->rate = rate;

	return error;
	}
