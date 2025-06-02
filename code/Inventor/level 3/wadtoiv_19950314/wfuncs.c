/*
   Doom Editor Utility, by Brendon Wyber and Rapha‰l Quinet.

   You are allowed to use any parts of this code in another program, as
   long as you give credits to the authors in the documentation and in
   the program itself.  Read the file README.1ST for more information.

   This program comes with absolutely no warranty.

   WAD.C - Wad files routines.
*/

/* the includes */
#include "wfuncs.h"
#include "wstructs.h"
#include "levels.h"

/* global variables */
WadPtr WadFileList = NULL;       /* linked list of wad files */
MDirPtr MasterDir = NULL;        /* the master directory */

/* the global data (from levels.c) */
MDirPtr Level = NULL;		/* master dictionary entry for the level */
short int NumThings = 0;		/* number of things */
TPtr Things;			/* things data */
short int NumLineDefs = 0;		/* number of line defs */
struct LineDef *LineDefs;			/* line defs data */
short int NumSideDefs = 0;		/* number of side defs */
SDPtr SideDefs;			/* side defs data */
short int NumVertexes = 0;		/* number of vertexes */
VPtr Vertexes;			/* vertex data */
short int NumSectors = 0;		/* number of sectors */
SPtr Sectors;			/* sectors data */
short int NumSegs = 0;		/* number of segments */
SEPtr Segs = NULL;		/* list of segments */
SEPtr LastSeg = NULL;		/* last segment in the list */
short int NumSSectors = 0;		/* number of subsectors */
SSPtr SSectors = NULL;		/* list of subsectors */
SSPtr LastSSector = NULL;	/* last subsector in the list */
short int NumNodes = 0;		/* number of Nodes */
NPtr Nodes = NULL;		/* nodes tree */
short int NumWTexture = 0;		/* number of wall textures */
char **WTexture;		/* array of wall texture names */
short int NumFTexture = 0;		/* number of floor/ceiling textures */
char **FTexture;		/* array of texture names */
short int MapMaxX = -32767;		/* maximum X value of map */
short int MapMaxY = -32767;		/* maximum Y value of map */
short int MapMinX = 32767;		/* minimum X value of map */
short int MapMinY = 32767;		/* minimum Y value of map */
Bool MadeChanges = FALSE;	/* made changes? */
Bool MadeMapChanges = FALSE;	/* made changes that need rebuilding? */

/*
   terminate the program reporting an error
*/

void ProgError( char *errstr, ...)
{
   va_list args;

   va_start( args, errstr);
   printf( "\nProgram Error: *** ");
   vprintf( errstr, args);
   printf( " ***\n");
   va_end( args);
   /* clean up things and free swap space */
   ForgetLevelData();
   CloseWadFiles();
   exit( 5);
}

/*
   allocate memory with error checking
*/

void *GetMemory( size_t size)
{
   void *ret;

   ret = malloc( size);
   if (!ret)
      ProgError( "out of memory (cannot allocate %u bytes)", size);
   return ret;
}

/*
   free memory
*/

void FreeMemory( void *ptr)
{
   /* just a wrapper around free(), but provide an entry point */
   /* for memory debugging routines... */
   free( ptr);
}

/*
   open the main wad file, read in its directory and create the
   master directory
*/

void OpenMainWad( char *filename)
{
   MDirPtr lastp, newp;
   long n;
   WadPtr wad;

   /* open the wad file */
   printf( "\nLoading main WAD file: %s...\n", filename);
   wad = BasicWadOpen( filename);
   if (strncmp( wad->type, "IWAD", 4))
      ProgError( "\"%s\" is not the main WAD file", filename);

   /* create the master directory */
   lastp = NULL;
   for (n = 0; n < wad->dirsize; n++)
   {
      newp = GetMemory( sizeof( struct MasterDirectory));
      newp->next = NULL;
      newp->wadfile = wad;
      memcpy( &(newp->dir), &(wad->directory[ n]), sizeof( struct Directory));
      newp->dir.start = ACI_swap_int(newp->dir.start);
      newp->dir.size = ACI_swap_int(newp->dir.size);

      if (MasterDir)
	 lastp->next = newp;
      else
	 MasterDir = newp;
      lastp = newp;
   }
}

/*
   open a patch wad file, read in its directory and alter the master
   directory
*/

void OpenPatchWad( char *filename, int doom2)
{
   WadPtr wad;
   MDirPtr mdir;
   int n, l;
   char entryname[9];

   /* open the wad file */
   printf( "Loading patch WAD file: %s...\n", filename);
   wad = BasicWadOpen( filename);
   if (strncmp( wad->type, "PWAD", 4))
      ProgError( "\"%s\" is not a patch WAD file", filename);

   /* alter the master directory */
   l = 0;
   for (n = 0; n < wad->dirsize; n++)
   {
      strncpy( entryname, wad->directory[ n].name, 8);
      entryname[8] = '\0';
      if (l == 0)
      {
	 mdir = FindMasterDir( MasterDir, wad->directory[ n].name);
	 /* if this entry is not in the master directory, then add it */
	 if (mdir == NULL)
	 {
	    printf( "   [Adding new entry %s]\n", entryname);
	    mdir = MasterDir;
	    while (mdir->next)
	       mdir = mdir->next;
	    mdir->next = GetMemory( sizeof( struct MasterDirectory));
	    mdir = mdir->next;
	    mdir->next = NULL;
	 }
	 /* if this is a level, then copy this entry and the next 10 */
	 else
	   if (((!doom2) &&
		(wad->directory[ n].name[ 0] == 'E' && wad->directory[ n].name[ 2] == 'M' &&
		 wad->directory[ n].name[ 4] == '\0')) ||
	       (doom2 &&
		(wad->directory[n].name[0] == 'M' && wad->directory[n].name[1] == 'A' &&
		 wad->directory[n].name[2] == 'P' && wad->directory[n].name[5] == '\0')))
	 {
	    printf( "   [Updating level %s]\n", entryname);
	    l = 10;
	 }
	 else
	    printf( "   [Updating entry %s]\n", entryname);
      }
      else
      {
	 mdir = mdir->next;
	 /* the level data should replace an existing level */
	 if (mdir == NULL || strncmp(mdir->dir.name, wad->directory[ n].name, 8))
	    ProgError( "\%s\" is not an understandable PWAD file (error with %s)", filename, entryname);
	 l--;
      }
      mdir->wadfile = wad;
      memcpy( &(mdir->dir), &(wad->directory[ n]), sizeof( struct Directory));
      mdir->dir.start = ACI_swap_int(mdir->dir.start);
      mdir->dir.size = ACI_swap_int(mdir->dir.size);
   }
}

void CloseWadFiles()
{
   WadPtr curw, nextw;
   MDirPtr curd, nextd;

   /* close the wad files */
   curw = WadFileList;
   WadFileList = NULL;
   while (curw)
   {
      nextw = curw->next;
      fclose( curw->fileinfo);
      FreeMemory( curw->directory);
      FreeMemory( curw);
      curw = nextw;
   }

   /* delete the master directory */
   curd = MasterDir;
   MasterDir = NULL;
   while (curd)
   {
      nextd = curd->next;
      FreeMemory( curd);
      curd = nextd;
   }
}

/*
   basic opening of WAD file and creation of node in Wad linked list
*/

WadPtr BasicWadOpen( char *filename)
{
   WadPtr curw, prevw;

   /* find the wad file in the wad file list */
   prevw = WadFileList;
   if (prevw)
   {
      curw = prevw->next;
      while (curw && strcmp( filename, curw->filename))
      {
	 prevw = curw;
	 curw = prevw->next;
      }
   }
   else
      curw = NULL;

   /* if this entry doesn't exist, add it to the WadFileList */
   if (curw == NULL)
   {
      curw = GetMemory( sizeof( struct WadFileInfo));
      if (prevw == NULL)
	 WadFileList = curw;
      else
	 prevw->next = curw;
      curw->next = NULL;
      curw->filename = filename;
   }

   /* open the file */
   if ((curw->fileinfo = fopen( filename, "rb")) == NULL)
      ProgError( "error opening \"%s\"", filename);

   /* read in the WAD directory info */
   BasicWadRead( curw, curw->type, 4);
   if (strncmp( curw->type, "IWAD", 4) && strncmp( curw->type, "PWAD", 4))
      ProgError( "\"%s\" is not a valid WAD file", filename);
   BasicWadRead( curw, &curw->dirsize, sizeof( curw->dirsize));
   BasicWadRead( curw, &curw->dirstart, sizeof( curw->dirstart));

   /* read in the WAD directory itself */
   curw->dirsize = ACI_swap_int(curw->dirsize); 
   curw->dirstart = ACI_swap_int(curw->dirstart); 
   curw->directory = GetMemory( sizeof( struct Directory) * curw->dirsize);
   BasicWadSeek( curw, curw->dirstart);
   BasicWadRead( curw, curw->directory, sizeof( struct Directory) * curw->dirsize);

   /* all done */
   return curw;
}

/*
   read bytes from a file and store it into an address with error checking
*/

void BasicWadRead( WadPtr wadfile, void *addr, long size)
{
   if (fread( addr, 1, size, wadfile->fileinfo) != size)
      ProgError( "error reading from \"%s\"", wadfile->filename);
}

/*
   go to offset of wad file with error checking
*/

void BasicWadSeek( WadPtr wadfile, long offset)
{
   if (fseek( wadfile->fileinfo, offset, 0))
      ProgError( "error reading from \"%s\"", wadfile->filename);
}

/*
   find an entry in the master directory
*/

MDirPtr FindMasterDir( MDirPtr from, char *name)
{
   while (from)
   {
      if (!strncmp( from->dir.name, name, 8))
	 break;
      from = from->next;
   }
   return from;
}

/*
   read in the level data
*/

void ReadLevelData( int episode, int mission, int doom2) /* SWAP! */
{
   MDirPtr dir;
   char name[ 7];
   int n, m;
   short int val;
   int OldNumVertexes;
   int *VertexUsed;
   long tempsize, tempstart;


   /* find the various level information from the master directory */
   if (doom2)
     sprintf(name, "MAP%.2d", mission);
   else
     sprintf( name, "E%dM%d", episode, mission);
   printf("Reading data for level %s...", name);
   Level = FindMasterDir( MasterDir, name);
   if (!Level)
      ProgError( "level data not found");

   /* get the number of Vertices */
   dir = FindMasterDir( Level, "VERTEXES");
   if (dir != NULL)
      OldNumVertexes = (int) (dir->dir.size / 4L);
   else
      OldNumVertexes = 0;
   if (OldNumVertexes > 0)
   {
      VertexUsed = GetMemory( OldNumVertexes * sizeof( int));
      for (n = 0; n < OldNumVertexes; n++)
	 VertexUsed[ n] = FALSE;
   }

   /* read in the Things data */
   dir = FindMasterDir( Level, "THINGS");
   tempsize = dir->dir.size;
   tempstart = dir->dir.start;
   if (dir != NULL)
      NumThings = (short int) (dir->dir.size / 10L);
   else
      NumThings = 0;
   if (NumThings > 0)
   {
      Things = GetMemory( (unsigned long) NumThings * sizeof( struct Thing));
      BasicWadSeek( dir->wadfile, dir->dir.start);
      for (n = 0; n < NumThings; n++)
      {
	 BasicWadRead( dir->wadfile, &(Things[ n].xpos), 2);
	 BasicWadRead( dir->wadfile, &(Things[ n].ypos), 2);
	 Things[n].xpos = ACI_swap_short(Things[n].xpos);
	 Things[n].ypos = ACI_swap_short(Things[n].ypos);
	 BasicWadRead( dir->wadfile, &(Things[ n].angle), 2);
	 Things[n].angle = ACI_swap_short(Things[n].angle);
	 BasicWadRead( dir->wadfile, &(Things[ n].type), 2);
	 Things[n].type = ACI_swap_short(Things[n].type);
	 BasicWadRead( dir->wadfile, &(Things[ n].when), 2);
	 Things[n].when = ACI_swap_short(Things[n].when);
      }
   }

   /* read in the LineDef information */
   dir = FindMasterDir( Level, "LINEDEFS");
   tempsize = dir->dir.size;
   tempstart = dir->dir.start;
   if (dir != NULL)
      NumLineDefs = (short int) (dir->dir.size / 14L);
   else
      NumLineDefs = 0;
   if (NumLineDefs > 0)
   {
      LineDefs = (struct LineDef *) GetMemory( (unsigned long) NumLineDefs * sizeof( struct LineDef));
      BasicWadSeek( dir->wadfile, dir->dir.start);
      for (n = 0; n < NumLineDefs; n++)
      {
	 BasicWadRead( dir->wadfile, &(LineDefs[ n].start), 2);
	 LineDefs[n].start = ACI_swap_short(LineDefs[n].start);
	 VertexUsed[ LineDefs[ n].start] = TRUE;
	 BasicWadRead( dir->wadfile, &(LineDefs[ n].end), 2);
	 LineDefs[n].end = ACI_swap_short(LineDefs[n].end);
	 VertexUsed[ LineDefs[ n].end] = TRUE;
	 BasicWadRead( dir->wadfile, &(LineDefs[ n].flags), 2);
	 LineDefs[n].flags = ACI_swap_short(LineDefs[n].flags);
	 BasicWadRead( dir->wadfile, &(LineDefs[ n].type), 2);
	 LineDefs[n].type = ACI_swap_short(LineDefs[n].type);
	 BasicWadRead( dir->wadfile, &(LineDefs[ n].tag), 2);
	 LineDefs[n].tag = ACI_swap_short(LineDefs[n].tag);
	 BasicWadRead( dir->wadfile, &(LineDefs[ n].sidedef1), 2);
	 LineDefs[n].sidedef1 = ACI_swap_short(LineDefs[n].sidedef1);
	 BasicWadRead( dir->wadfile, &(LineDefs[ n].sidedef2), 2);
	 LineDefs[n].sidedef2 = ACI_swap_short(LineDefs[n].sidedef2);
      }
   }

   /* read in the SideDef information */
   dir = FindMasterDir( Level, "SIDEDEFS");
   if (dir != NULL)
      NumSideDefs = (short int) (dir->dir.size / 30L);
   else
      NumSideDefs = 0;
   if (NumSideDefs > 0)
   {
      SideDefs = GetMemory( (unsigned long) NumSideDefs * sizeof( struct SideDef));
      BasicWadSeek( dir->wadfile, dir->dir.start);
      for (n = 0; n < NumSideDefs; n++)
      {
	 BasicWadRead( dir->wadfile, &(SideDefs[ n].xoff), 2);
	 SideDefs[n].xoff = ACI_swap_short(SideDefs[n].xoff);
	 BasicWadRead( dir->wadfile, &(SideDefs[ n].yoff), 2);
	 SideDefs[n].yoff = ACI_swap_short(SideDefs[n].yoff);
	 BasicWadRead( dir->wadfile, &(SideDefs[ n].tex1), 8);
	 BasicWadRead( dir->wadfile, &(SideDefs[ n].tex2), 8);
	 BasicWadRead( dir->wadfile, &(SideDefs[ n].tex3), 8);
	 BasicWadRead( dir->wadfile, &(SideDefs[ n].sector), 2);
	 SideDefs[n].sector = ACI_swap_short(SideDefs[n].sector);
      }
   }

   /* read in the Vertices which are all the corners of the level, but ignore the */
   /* Vertices not used in any LineDef (they usually are at the end of the list). */
   NumVertexes = 0;
   for (n = 0; n < OldNumVertexes; n++)
      if (VertexUsed[ n])
	 NumVertexes++;
   if (NumVertexes > 0)
   {
      Vertexes = GetMemory( (unsigned long) NumVertexes * sizeof( struct Vertex));
      dir = FindMasterDir( Level, "VERTEXES");
      BasicWadSeek( dir->wadfile, dir->dir.start);
      MapMaxX = -32767;
      MapMaxY = -32767;
      MapMinX = 32767;
      MapMinY = 32767;
      m = 0;
      for (n = 0; n < OldNumVertexes; n++)
      {
	 BasicWadRead( dir->wadfile, &val, 2);
	 val = ACI_swap_short(val);
	 if (VertexUsed[ n])
	 {
	    if (val < MapMinX)
	       MapMinX = val;
	    if (val > MapMaxX)
	       MapMaxX = val;
	    Vertexes[ m].x = val;
	 }
	 BasicWadRead( dir->wadfile, &val, 2);
	 val = ACI_swap_short(val);
	 if (VertexUsed[ n])
	 {
	    if (val < MapMinY)
	       MapMinY = val;
	    if (val > MapMaxY)
	       MapMaxY = val;
	    Vertexes[ m].y = val;
	    m++;
	 }
      }
      if (m != NumVertexes)
	 ProgError("inconsistency in the Vertexes data\n");
   }

   if (OldNumVertexes > 0)
   {
      /* update the Vertex numbers in the LineDefs (not really necessary, but...) */
      m = 0;
      for (n = 0; n < OldNumVertexes; n++)
	 if (VertexUsed[ n])
	    VertexUsed[ n] = m++;
      for (n = 0; n < NumLineDefs; n++)
      {
	 LineDefs[ n].start = VertexUsed[ LineDefs[ n].start];
	 LineDefs[ n].end = VertexUsed[ LineDefs[ n].end];
      }
      FreeMemory( VertexUsed);
   }

   /* ignore the Segs, SSectors and Nodes */

   /* read in the Sectors information */
   dir = FindMasterDir( Level, "SECTORS");
   if (dir != NULL)
      NumSectors = (int) (dir->dir.size / 26L);
   else
      NumSectors = 0;
   if (NumSectors > 0)
   {
      Sectors = GetMemory( (unsigned long) NumSectors * sizeof( struct Sector));
      BasicWadSeek( dir->wadfile, dir->dir.start);
      for (n = 0; n < NumSectors; n++)
      {
	 BasicWadRead( dir->wadfile, &(Sectors[ n].floorh), 2);
	 Sectors[n].floorh = ACI_swap_short(Sectors[n].floorh);
	 BasicWadRead( dir->wadfile, &(Sectors[ n].ceilh), 2);
	 Sectors[n].ceilh = ACI_swap_short(Sectors[n].ceilh);
	 BasicWadRead( dir->wadfile, &(Sectors[ n].floort), 8);
	 BasicWadRead( dir->wadfile, &(Sectors[ n].ceilt), 8);
	 BasicWadRead( dir->wadfile, &(Sectors[ n].light), 2);
	 Sectors[n].light = ACI_swap_short(Sectors[n].light);
	 BasicWadRead( dir->wadfile, &(Sectors[ n].special), 2);
	 Sectors[n].special = ACI_swap_short(Sectors[n].special);
	 BasicWadRead( dir->wadfile, &(Sectors[ n].tag), 2);
	 Sectors[n].tag = ACI_swap_short(Sectors[n].tag);
      }
   }

   /* ignore the last entries (Reject & BlockMap) */
}

/*
   forget the level data
*/

void ForgetLevelData() /* SWAP! */
{
   /* forget the Things */
   NumThings = 0;
   if (Things)
      FreeMemory( Things);
   Things = NULL;

   /* forget the Vertices */
   NumVertexes = 0;
   if (Vertexes)
      FreeMemory( Vertexes);
   Vertexes = NULL;

   /* forget the LineDefs */
   NumLineDefs = 0;
   if (LineDefs)
      FreeMemory( LineDefs);
   LineDefs = NULL;

   /* forget the SideDefs */
   NumSideDefs = 0;
   if (SideDefs)
      FreeMemory( SideDefs);
   SideDefs = NULL;

   /* forget the Sectors */
   NumSectors = 0;
   if (Sectors)
      FreeMemory( Sectors);
   Sectors = NULL;
}
