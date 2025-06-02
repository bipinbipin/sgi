/*
   Doom Editor Utility, by Brendon Wyber and Rapha‰l Quinet.

   You are allowed to use any parts of this code in another program, as
   long as you give credits to the authors in the documentation and in
   the program itself.  Read the file README.1ST for more information.

   This program comes with absolutely no warranty.

   DEU.H - Main doom defines.
*/

/* the includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <malloc.h>


/* Byte swapping macros -- thanks Wave */

#define ACI_swap_short(x) (((x<<8)|((x>>8)&0xff))&0xffff)
#define ACI_swap_int(x)  ((ACI_swap_short(x)<<16)|ACI_swap_short(x>>16))

/*
   the version information
*/

#define DEU_VERSION	"5.21"	/* the version number */



/*
   the directory structure is the structre used by DOOM to order the
   data in it's WAD files
*/

typedef struct Directory *DirPtr;
struct Directory
{
   unsigned long start;			/* offset to start of data */
   unsigned long size;			/* byte size of data */
   char name[ 8];		/* name of data block */
};



/*
   The wad file pointer structure is used for holding the information
   on the wad files in a linked list.

   The first wad file is the main wad file. The rest are patches.
*/

typedef struct WadFileInfo *WadPtr;
struct WadFileInfo
{
   WadPtr next;			/* next file in linked list */
   char *filename;		/* name of the wad file */
   FILE *fileinfo;		/* C file stream information */
   char type[ 4];		/* type of wad file (IWAD or PWAD) */
   unsigned long dirsize;		/* directory size of WAD */
   unsigned long dirstart;		/* offset to start of directory */
   DirPtr directory;		/* array of directory information */
};



/*
   the master directory structure is used to build a complete directory
   of all the data blocks from all the various wad files
*/

typedef struct MasterDirectory *MDirPtr;
struct MasterDirectory
{
   MDirPtr next;		/* next in list */
   WadPtr wadfile;		/* file of origin */
   struct Directory dir;	/* directory data */
};



/*
   syntactic sugar
*/
typedef int Bool;               /* Boolean data: true or false */


/* convert screen coordinates to map coordinates */
#define MAPX(x)			(OrigX + (int) (((x) - ScrCenterX) / Scale))
#define MAPY(y)			(OrigY + (int) ((ScrCenterY - (y)) / Scale))

/* convert map coordinates to screen coordinates */
#define SCREENX(x)		(ScrCenterX + (int) (((x) - OrigX) * Scale))
#define SCREENY(y)		(ScrCenterY + (int) ((OrigY - (y)) * Scale))

/* object types */
#define OBJ_THINGS		1
#define OBJ_LINEDEFS		2
#define OBJ_SIDEDEFS		3
#define OBJ_VERTEXES		4
#define OBJ_SEGS		5
#define OBJ_SSECTORS		6
#define OBJ_NODES		7
#define OBJ_SECTORS		8
#define OBJ_REJECT		9
#define OBJ_BLOCKMAP		10

/* boolean constants */
#define TRUE			1
#define FALSE			0

/* half the size of an object (Thing or Vertex) in map coords */
#define OBJSIZE			7


/*
   the interfile global variables
*/

/* from deu.c */
extern char *MainWad;		/* name of the main wad file */

/* from wads.c */
extern WadPtr  WadFileList;	/* list of wad files */
extern MDirPtr MasterDir;	/* the master directory */

/*
   the function prototypes
*/

/* from deu.c */
void ProgError( char *, ...);

/* from memory.c */
void *GetMemory( size_t);
void FreeMemory( void *);

/* from wads.c */
void OpenMainWad( char *);
void OpenPatchWad( char *, int);
void CloseWadFiles( void);
WadPtr BasicWadOpen( char *);
void BasicWadRead( WadPtr, void *, long);
void BasicWadSeek( WadPtr, long);
MDirPtr FindMasterDir( MDirPtr, char *);

/* from levels.c */
void ReadLevelData( int, int, int); /* SWAP! */
void ForgetLevelData( void); /* SWAP! */

/* end of file */
