/* wadtoiv version 1.2		March 14, 1995
   Kenneth B. Russell		kbrussel@media.mit.edu
   
   Thanks to Raphael Quinet and Brendon Wyber for distributing
   the source for DEU, their Doom Editor Utility, from which
   several functions (and all the wad data structures) came.
*/

#define WADTOIV_VERSION_NUMBER   "1.2"
#define WADTOIV_VERSION_DATE     "3/14/95"

#include <stdio.h>
#include <iostream.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <bstring.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "image.h"
extern "C" {
#include "wfuncs.h"
#include "levels.h"
}

#include <Inventor/SoDB.h>
#include <Inventor/SbBox.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTexture2Transform.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoTextureCoordinateBinding.h>

//#define DEBUG_FILES

struct TextureListStruct {
  SoSeparator *sep;
  SoTexture2 *texture;
  SoTexture2Transform *tXform;
  SoTextureCoordinate2 *texCoord;
  SoTextureCoordinateBinding *tBind;
  SoCoordinate3 *coord;
  SoNormal *norms;
  SoNormalBinding *nBind;
  SoFaceSet *polys;
  int index;
  char name[8];
  struct TextureListStruct *next;
  int xofs;
  int yofs;
  int xsize;
  int ysize;
  int faceidx;
};
typedef struct TextureListStruct TextureList;

struct VertexListStruct {
  short int index;
  short int x;
  short int y;
  struct VertexListStruct *next;
};
typedef struct VertexListStruct VertexList;

struct LineDefListStruct {
  short int index;
  short int start;
  short int end;
  short int sidedefidx;
  struct LineDefListStruct *next;
};
typedef struct LineDefListStruct LineDefList;

struct SectorListStruct {
  int sectorNum;
  LineDefList *linedefs;
  struct SectorListStruct *next;
};
typedef struct SectorListStruct SectorList;

SoSeparator *root=NULL;
int debug_files = 0;
int palettenum = 0;
int doom2 = 0;

TextureList *
ScanTextureList(TextureList *tl_ptr, char cmp_string[8])
{
  while (tl_ptr)
    {
      if (strncmp(cmp_string, tl_ptr->name, 8) == 0)
	return(tl_ptr);
      tl_ptr = tl_ptr->next;
    }
  return(NULL);
}

void
ReadDoomPalette( int playpalnum, unsigned char *dpal)
{
   MDirPtr             dir;
   int                 n;

   if (playpalnum < 0 && playpalnum > 13)
      return;
   dir = FindMasterDir( MasterDir, "PLAYPAL");
   if (dir)
   {
      BasicWadSeek( dir->wadfile, dir->dir.start);
      for (n = 0; n <= playpalnum; n++)
	 BasicWadRead( dir->wadfile, dpal, 768L);
/*      for (n = 0; n < 768; n++)
	 dpal[ n] /= 4; */
    }
}

void
DumpFloorTextureToRGBFile(char *texname)
{
  char texfilename[256];
  IMAGE *tex;
  MDirPtr dir;
  unsigned char *pixels;
  unsigned char *dpal;
  unsigned short buffer[64];
  int c1, c2, c3;

  sprintf(texfilename, "%.8s.rgb", texname);
  if (debug_files)
    {
      printf("would dump %.8s\n", texname);
    }
  else
    {
      fprintf(stderr, "Outputting floor texture %.8s\n", texname);

      tex = iopen(texfilename, "w", RLE(1), 4, 64, 64, 4);
      
      dir = FindMasterDir( MasterDir, texname);
      if (dir == NULL)
	{
	  fprintf(stderr, "ERROR: Texture \'%.8s\' not found\n", texname);
	  exit(1);
	}
      /* Load in the palette */
      
      dpal = (unsigned char *)GetMemory(768 * sizeof(char));
      ReadDoomPalette(palettenum, dpal);
      
      /* Get the pixels of the texture */
      
      pixels = (unsigned char *)GetMemory( 4096 * sizeof( char));
      BasicWadSeek( dir->wadfile, dir->dir.start);
      BasicWadRead( dir->wadfile, pixels, 4096L);
      
      
      for (c1=0; c1<4; c1++)  /* Depth plane: R, G, B */
	{
	  for (c2=0; c2<64; c2++)  /* Which row: 0 - 63 */
	    {
	      for (c3=0; c3<64; c3++)  /* Which column: 0 - 63 */
		{
		  /* The dpal is an array of the form RGBRGBRGB...
		     starting at the upper left of the texture and
		     going across each row, then down a row */
		  
		  if (c1 != 3)
		    buffer[c3] = dpal[(3 * pixels[(64 * c2) + c3]) + c1];
		  else
		    buffer[c3] = 255;
		}
	      putrow(tex, buffer, (63 - c2), c1);
	    }
	}
      
      iclose(tex);

      FreeMemory(dpal);
      FreeMemory(pixels);
    }
}

void
DumpWallTextureToRGBFile(char *texname,
			 int *bb_xsize,   /* size of the texture */
			 int *bb_ysize,
			 int *bb_xofs,
			 int *bb_yofs)
{
  char texfilename[256];
  IMAGE *tex;
  MDirPtr  		dir;		/* main directory pointer to the TEXTURE* entries */
  MDirPtr  		pdir;		/* main directory pointer to the PNAMES entry */
  unsigned long    	*offsets;	/* array of offsets to texture names */
  int      		n;		/* general counter */
  unsigned short int    xsize, ysize;
  short int             xofs, yofs;     /* offset in wall texture space of each component of the texture */
  short int 		temp_xofs, temp_yofs;
  int	 		x, y;
  unsigned short int    fields;		/* number of wall patches used to build this texture */
  unsigned short int    pnameind;	/* patch name index in PNAMES table */
  short int      	junk;       	/* holds useless data */
  unsigned long     	numtex;		/* number of texture names in TEXTURE* list */
  unsigned long     	texofs;		/* offset in the wad file to the texture data */
  char     		tname[9]; 	/* texture name */
  char     		picname[9]; 	/* wall patch name */
  unsigned char		*lpColumnData;
  unsigned char 	*lpColumn;
  unsigned long	   	*lpNeededOffsets;
  int		       	nColumns, nCurrentColumn;
  unsigned long		lCurrentOffset;
  int		       	fColumnInMemory;
  int		       	i, n2;
  unsigned char       	bRowStart, bColored;
  unsigned char *dpal;
  unsigned short *bigbuf;  /* Holds the entire picture in memory */
  int c1, c2;
  int minx = 32767, miny = 32767, maxx = -32767, maxy = -32767;

  sprintf(texfilename, "%.8s.rgb", texname);
  if (debug_files)
    {
      printf("would dump %.8s\n", texname);
    }
  else
    {

      /* offset for texture we want. */

      texofs = 0;

      /* search for texname in texture1 names */

      dir = FindMasterDir( MasterDir, "TEXTURE1");
      BasicWadSeek( dir->wadfile, dir->dir.start);
      BasicWadRead( dir->wadfile, &numtex, 4);
      numtex = ACI_swap_int(numtex);

      /* read in the offsets for texture1 names and info. */

      offsets = (unsigned long *) GetMemory( (size_t) (numtex * sizeof( long)));
      for (n = 0; n < numtex; n++)
	{
	  BasicWadRead( dir->wadfile, &(offsets[ n]), 4L);
	  offsets[n] = ACI_swap_int(offsets[n]);
	}
      for (n = 0; n < numtex && !texofs; n++)
	{
	  BasicWadSeek( dir->wadfile, dir->dir.start + offsets[ n]);
	  BasicWadRead( dir->wadfile, &tname, 8);
	  if (!strncmp(tname, texname, 8))
	    texofs = dir->dir.start + offsets[ n];
	}
      FreeMemory( offsets);
      if (texofs == 0)
	{
	  /* search for texname in texture2 names */

	  dir = FindMasterDir( MasterDir, "TEXTURE2");
	  BasicWadSeek( dir->wadfile, dir->dir.start);
	  BasicWadRead( dir->wadfile, &numtex, 4);
	  numtex = ACI_swap_int(numtex);

	  /* read in the offsets for texture2 names */

	  offsets = (unsigned long *) GetMemory((size_t) (numtex * sizeof( long)));
	  for (n = 0; n < numtex; n++)
	    {
	      BasicWadRead( dir->wadfile, &(offsets[ n]), 4L);
	      offsets[n] = ACI_swap_int(offsets[n]);
	    }
	  for (n = 0; n < numtex && !texofs; n++)
	    {
	      BasicWadSeek( dir->wadfile, dir->dir.start + offsets[ n]);
	      BasicWadRead( dir->wadfile, &tname, 8);
	      if (!strncmp( tname, texname, 8))
		texofs = dir->dir.start + offsets[ n];
	    }
	  FreeMemory( offsets);
	}

      if (texofs == 0)
	{
	  fprintf(stderr, "ERROR: Texture \'%.8s\' not found\n", texname);
	  exit(1);
	}

      /* Load in the palette */
      
      dpal = (unsigned char *)GetMemory((size_t) (768 * sizeof(char)));
      ReadDoomPalette(palettenum, dpal);

      /* read the info for this texture */
      BasicWadSeek( dir->wadfile, texofs + 12L);
      BasicWadRead( dir->wadfile, &xsize, 2L);
      xsize = ACI_swap_short(xsize);
      BasicWadRead( dir->wadfile, &ysize, 2L);
      ysize = ACI_swap_short(ysize);
      BasicWadSeek( dir->wadfile, texofs + 20L);
      BasicWadRead( dir->wadfile, &fields, 2L);
      fields = ACI_swap_short(fields);

      bigbuf = new unsigned short[4 * ysize * xsize];
      bzero((char *)bigbuf, (4 * xsize * ysize * sizeof(unsigned short)));
      *bb_xsize = xsize;
      *bb_ysize = ysize;

      fprintf(stderr, "Outputting wall texture %.8s, fields %d, size %d, %d\n", texname, fields, *bb_xsize, *bb_ysize);

      /* Center all textures for which it is appropriate --
	 the bounding rectangle of the composited pictures
	 should be the same as the texture size (if not,
	 don't center it) */

      for (n = 0; n < fields; n++)
	{
	  BasicWadSeek( dir->wadfile, texofs + 22L + n * 10L);
	  BasicWadRead( dir->wadfile, &xofs, 2L);
	  xofs = ACI_swap_short(xofs);
	  BasicWadRead( dir->wadfile, &yofs, 2L);
	  yofs = ACI_swap_short(yofs);
	  BasicWadRead( dir->wadfile, &pnameind, 2L);
	  pnameind = ACI_swap_short(pnameind);

	  /* OK, now look up the pic's name in the PNAMES entry. */
	  pdir = FindMasterDir( MasterDir, "PNAMES");
	  BasicWadSeek( pdir->wadfile, pdir->dir.start + 4L + pnameind * 8L);
	  BasicWadRead( pdir->wadfile, &picname, 8L);
	  for(c1 = 0; c1 < 8; c1++)
	    {
	      picname[c1] = toupper(picname[c1]);
	    }

	  picname[ 8] = '\0';

	  dir = FindMasterDir( MasterDir, picname);
	  if (dir == NULL)
	    {
	      fprintf(stderr, "ERROR: Picture \'%.8s\' not found\n", picname);
	      exit(1);
	    }

	  BasicWadSeek( dir->wadfile, dir->dir.start);
	  BasicWadRead( dir->wadfile, &xsize, 2L);
	  xsize = ACI_swap_short(xsize);
	  BasicWadRead( dir->wadfile, &ysize, 2L);
	  ysize = ACI_swap_short(ysize);

	  /* We now have the size and offset of the picture element,
	     so improve the total bounding rectangle */

	  if (xofs < minx)
	    minx = xofs;
	  if (yofs < miny)
	    miny = yofs;
	  if ((xofs + xsize) > maxx)
	    maxx = xofs + xsize;
	  if ((yofs + ysize) > maxy)
	    maxy = yofs + ysize;
	}

      if (!((maxx - minx == *bb_xsize) && (maxy - miny == *bb_ysize)))
	{
	  *bb_xofs = 0;
	  *bb_yofs = 0;
	}
      else   /* Center the texture */
	{
	  *bb_xofs = minx;
	  *bb_yofs = miny;
	}

      /* display the texture */
      for (n = 0; n < fields; n++)
	{
	  BasicWadSeek( dir->wadfile, texofs + 22L + n * 10L);

	  /* These two numbers are the offsets of the
	     picture in texture space. */

	  BasicWadRead( dir->wadfile, &xofs, 2L);
	  xofs = ACI_swap_short(xofs);
	  BasicWadRead( dir->wadfile, &yofs, 2L);
	  yofs = ACI_swap_short(yofs);
	  BasicWadRead( dir->wadfile, &pnameind, 2L);
	  pnameind = ACI_swap_short(pnameind);
	  BasicWadRead( dir->wadfile, &junk, 2L);  /* Junk should = 1. */
	  BasicWadRead( dir->wadfile, &junk, 2L);  /* Junk should = 0. */
	  /* OK, now look up the pic's name in the PNAMES entry. */
	  pdir = FindMasterDir( MasterDir, "PNAMES");
	  BasicWadSeek( pdir->wadfile, pdir->dir.start + 4L + pnameind * 8L);
	  BasicWadRead( pdir->wadfile, &picname, 8L);
	  for(c1 = 0; c1 < 8; c1++)
	    {
	      picname[c1] = toupper(picname[c1]);
	    }
	  picname[ 8] = '\0';

	  /* code from DisplayPic() */

	  dir = FindMasterDir( MasterDir, picname);
	  if (dir == NULL)
	    {
	      fprintf(stderr, "ERROR: Picture \'%.8s\' not found\n", picname);
	      exit(1);
	    }

	  BasicWadSeek( dir->wadfile, dir->dir.start);
	  BasicWadRead( dir->wadfile, &xsize, 2L);
	  xsize = ACI_swap_short(xsize);
	  BasicWadRead( dir->wadfile, &ysize, 2L);
	  ysize = ACI_swap_short(ysize);

	  /* The following two offsets should be ignored --
	     we want the texture space offsets */

	  BasicWadRead( dir->wadfile, &temp_xofs, 2L);
	  temp_xofs = ACI_swap_short(temp_xofs);
	  BasicWadRead( dir->wadfile, &temp_yofs, 2L);
	  temp_yofs = ACI_swap_short(temp_yofs);
	  
#define TEX_COLUMNBUFFERSIZE	(60L * 1024L)
#define TEX_COLUMNSIZE		512L

	  nColumns = xsize;

	  lpColumnData    = (unsigned char *) GetMemory((size_t) TEX_COLUMNBUFFERSIZE);
	  lpNeededOffsets = (unsigned long *) GetMemory((size_t) (nColumns * 4L));
	  BasicWadRead( dir->wadfile, lpNeededOffsets, nColumns * 4L);
	  for(nCurrentColumn = 0; nCurrentColumn < nColumns; nCurrentColumn++)
	    {
	      lpNeededOffsets[nCurrentColumn] = ACI_swap_int(lpNeededOffsets[nCurrentColumn]);
	    }

	  /* read first column data, and subsequent column data */
	  BasicWadSeek( dir->wadfile, dir->dir.start + lpNeededOffsets[ 0]);
	  BasicWadRead( dir->wadfile, lpColumnData, TEX_COLUMNBUFFERSIZE);

	  for (nCurrentColumn = 0; nCurrentColumn < nColumns; nCurrentColumn++)
	    {
	      lCurrentOffset  = lpNeededOffsets[ nCurrentColumn];
	      fColumnInMemory = (lCurrentOffset >= lpNeededOffsets[ 0]) && 
		(lCurrentOffset < (unsigned long)(lpNeededOffsets[ 0] + TEX_COLUMNBUFFERSIZE - TEX_COLUMNSIZE));
	      if (fColumnInMemory)
		{
		  lpColumn = &lpColumnData[ lCurrentOffset - lpNeededOffsets[ 0]];
		}
	      else
		{
		  lpColumn = (unsigned char *) GetMemory((size_t) TEX_COLUMNSIZE);
		  BasicWadSeek( dir->wadfile, dir->dir.start + lCurrentOffset);
		  BasicWadRead( dir->wadfile, lpColumn, TEX_COLUMNSIZE);
		}

	      /* we now have the needed column data, one way or another, so write it */
	      n2 = 1;
	      bRowStart = lpColumn[ 0];
	      while (bRowStart != 255 && n2 < TEX_COLUMNSIZE)
		{
		  bColored = lpColumn[ n2];
		  n2 += 2;				/* skip over 'null' pixel in data */
		  for (i = 0; i < bColored; i++)
		    {
		      x = xofs - *bb_xofs + nCurrentColumn;
		      y = yofs - *bb_yofs + bRowStart + i;
		      if ((x >= 0) && (y >= 0) && (x < *bb_xsize) && (y < *bb_ysize))
			{
			  /* Add the pixels to the texture */

			  for (c1 = 0; c1 < 3; c1++)
			    {
			      bigbuf[(*bb_xsize * c1) + (4 * *bb_xsize * y) + x] = 
				dpal[(3 * lpColumn[i + n2]) + c1];
			    }

			  /* Set the alpha channel */

			  bigbuf[(*bb_xsize * 3) + (4 * *bb_xsize * y) + x] = 255;
			}
		    }
		  n2 += bColored + 1;	/* skip over written pixels, and the 'null' one */
		  bRowStart = lpColumn[ n2++];
		}
	      if (bRowStart != 255)
		ProgError( "BUG: bRowStart != 255.");

	      if (!fColumnInMemory)
		FreeMemory( lpColumn);
	    }
	  FreeMemory( lpColumnData);
	  FreeMemory( lpNeededOffsets);
	}
  
      /* Output the completed texture */
      
      tex = iopen(texfilename, "w", RLE(1), 4, *bb_xsize, *bb_ysize, 4);
      
     for(c1 = 0; c1 < 4; c1++)
	{
	  for (c2 = 0; c2 < *bb_ysize; c2++)
	    {
	      putrow(tex, &(bigbuf[(4 * *bb_xsize * c2) + (*bb_xsize * c1)]), (*bb_ysize - c2 - 1), c1);
	    }
	}
      
      iclose(tex);

      FreeMemory(dpal);
      delete[] bigbuf;
    }
}

TextureList *
ConditionalFloorTextureDump(TextureList *tl, TextureList **tl_last, char *texname)
{
  int c1;
  TextureList *tptr;
  char texfilename[256];

  if (texname[0] != '-')
    {
      for(c1 = 0; c1 < 8; c1++)
	{
	  texname[c1] = toupper(texname[c1]);
	}
      
      tptr = ScanTextureList(tl, texname);

      if (!tptr)
	{
	  DumpFloorTextureToRGBFile(texname);
	  strncpy((*tl_last)->name, texname, 8);
	  sprintf(texfilename, "%.8s.rgb", texname);
	  tptr = *tl_last;
	  (*tl_last)->texture->filename.setValue(texfilename);
	  (*tl_last)->next = new TextureList;
	  (*tl_last) = (*tl_last)->next;
	  (*tl_last)->next = NULL;
	  bzero((void *)&((*tl_last)->name), 8);
	  (*tl_last)->index = 0;
	  (*tl_last)->faceidx = 0;
	  (*tl_last)->sep = new SoSeparator;
	  (*tl_last)->sep->ref();	  
	  (*tl_last)->texture = new SoTexture2;
	  (*tl_last)->texture->ref();
	  (*tl_last)->tXform = new SoTexture2Transform;
	  (*tl_last)->tXform->ref();
	  (*tl_last)->texCoord = new SoTextureCoordinate2;
	  (*tl_last)->texCoord->ref();
	  (*tl_last)->tBind = new SoTextureCoordinateBinding;
	  (*tl_last)->tBind->ref();
	  (*tl_last)->coord = new SoCoordinate3;
	  (*tl_last)->coord->ref();
	  (*tl_last)->norms = new SoNormal;
	  (*tl_last)->norms->ref();
	  (*tl_last)->nBind = new SoNormalBinding;
	  (*tl_last)->nBind->ref();
	  (*tl_last)->polys = new SoFaceSet;
	  (*tl_last)->polys->ref();
	}
      return(tptr);
    }
  return(NULL);
}  

TextureList *
ConditionalWallTextureDump(TextureList *tl, TextureList **tl_last, char *texname)
{
  int c1;
  int tex_xsize, tex_ysize, tex_xofs, tex_yofs;
  TextureList *tptr;
  char texfilename[256];

  if (texname[0] != '-')
    {
      for(c1 = 0; c1 < 8; c1++)
	{
	  texname[c1] = toupper(texname[c1]);
	}
      
      tptr = ScanTextureList(tl, texname);

      if (!tptr)
	{
	  DumpWallTextureToRGBFile(texname,
				   &tex_xsize,
				   &tex_ysize,
				   &tex_xofs,
				   &tex_yofs);
	  strncpy((*tl_last)->name, texname, 8);
	  sprintf(texfilename, "%.8s.rgb", texname);
	  (*tl_last)->texture->filename.setValue(texfilename);
	  (*tl_last)->tXform->scaleFactor.setValue(SbVec2f((1. / tex_xsize),
							   (1. / tex_ysize)));
	  (*tl_last)->xsize = tex_xsize;
	  (*tl_last)->ysize = tex_ysize;
	  (*tl_last)->xofs = tex_xofs;
	  (*tl_last)->yofs = tex_yofs;
	  tptr = *tl_last;
	  (*tl_last)->next = new TextureList;
	  (*tl_last) = (*tl_last)->next;
	  (*tl_last)->next = NULL;
	  bzero((void *)((*tl_last)->name), 8);
	  (*tl_last)->index = 0;
	  (*tl_last)->faceidx = 0;
	  (*tl_last)->sep = new SoSeparator;
	  (*tl_last)->sep->ref();
	  (*tl_last)->texture = new SoTexture2;
	  (*tl_last)->texture->ref();
	  (*tl_last)->tXform = new SoTexture2Transform;
	  (*tl_last)->tXform->ref();
	  (*tl_last)->texCoord = new SoTextureCoordinate2;
	  (*tl_last)->texCoord->ref();
	  (*tl_last)->tBind = new SoTextureCoordinateBinding;
	  (*tl_last)->tBind->ref();
	  (*tl_last)->coord = new SoCoordinate3;
	  (*tl_last)->coord->ref();
	  (*tl_last)->norms = new SoNormal;
	  (*tl_last)->norms->ref();
	  (*tl_last)->nBind = new SoNormalBinding;
	  (*tl_last)->nBind->ref();
	  (*tl_last)->polys = new SoFaceSet;
	  (*tl_last)->polys->ref();
	}
      return(tptr);
    }
  return(NULL);
}  

void
AddUpperWallVertices(TextureList *tptr, int ld_idx, int sidedefnum)
/* sidedefnum is the number of the sidedef -- 1 or 2 */
{
  float length, height;
  SbVec3f v1, v2, v3, v4;
  SbVec2f offset;

  /* Add the four vertices of the sidedef to the coordinate lists */
  
  if (tptr)
    {
      switch (sidedefnum)
	{
	case 1:
	  {
	    int miny, maxy;

	    miny = Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].ceilh;
	    maxy = Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].ceilh;

	    if (miny > maxy)
	      {
		miny = Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].ceilh;
		maxy = Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].ceilh;
	      }

	    v1.setValue(-Vertexes[LineDefs[ld_idx].start].x,
			miny,
			Vertexes[LineDefs[ld_idx].start].y);
	    
	    v2.setValue(-Vertexes[LineDefs[ld_idx].start].x,
			maxy,
			Vertexes[LineDefs[ld_idx].start].y);
	    
	    v3.setValue(-Vertexes[LineDefs[ld_idx].end].x,
			maxy,
			Vertexes[LineDefs[ld_idx].end].y);
	    
	    v4.setValue(-Vertexes[LineDefs[ld_idx].end].x,
			miny,
			Vertexes[LineDefs[ld_idx].end].y);

	    offset.setValue(tptr->xofs + SideDefs[LineDefs[ld_idx].sidedef1].xoff,
			    tptr->yofs - SideDefs[LineDefs[ld_idx].sidedef1].yoff);
			    

	    break;
	  }

	case 2:
	  {
	    int miny, maxy;

	    miny = Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].ceilh;
	    maxy = Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].ceilh;

	    if (miny > maxy)
	      {
		miny = Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].ceilh;
		maxy = Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].ceilh;
	      }

	    v1.setValue(-Vertexes[LineDefs[ld_idx].end].x,
			miny,
			Vertexes[LineDefs[ld_idx].end].y);
	    
	    v2.setValue(-Vertexes[LineDefs[ld_idx].end].x,
			maxy,
			Vertexes[LineDefs[ld_idx].end].y);
	    
	    v3.setValue(-Vertexes[LineDefs[ld_idx].start].x,
			maxy,
			Vertexes[LineDefs[ld_idx].start].y);
	    
	    v4.setValue(-Vertexes[LineDefs[ld_idx].start].x,
			miny,
			Vertexes[LineDefs[ld_idx].start].y);

	    offset.setValue(tptr->xofs + SideDefs[LineDefs[ld_idx].sidedef2].xoff,
			    tptr->yofs - SideDefs[LineDefs[ld_idx].sidedef2].yoff);
	    break;
	  }
	default:
	  {
	    fprintf(stderr, "AddUpperWallVertices: sidedefnum must be either 1 or 2\n");
	    exit(1);
	  }
	}
      
      if ((LineDefs[ld_idx].start >= 0) &&
	  (LineDefs[ld_idx].start < NumVertexes) &&
	  (LineDefs[ld_idx].end >= 0) &&
	  (LineDefs[ld_idx].end < NumVertexes))
	{
	  SbVec3f vec1, vec2;
	  
	  vec1 = v2 - v1;
	  vec2 = v3 - v2;
	  tptr->norms->vector.set1Value((tptr->index / 4),
					-(vec1.cross(vec2)));
	  
	  length = sqrt(((Vertexes[LineDefs[ld_idx].end].x - Vertexes[LineDefs[ld_idx].start].x) *
			 (Vertexes[LineDefs[ld_idx].end].x - Vertexes[LineDefs[ld_idx].start].x)) +
			((Vertexes[LineDefs[ld_idx].end].y - Vertexes[LineDefs[ld_idx].start].y) *
			 (Vertexes[LineDefs[ld_idx].end].y - Vertexes[LineDefs[ld_idx].start].y)));
	  height = abs(Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].ceilh - Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].ceilh);
	  
	  if (LineDefs[ld_idx].flags & (1 << 3))
	    {
	      /* Upper texture is unpegged.
		 Align texture with top of polygon. */

	      tptr->coord->point.set1Value(tptr->index, v1);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(0, -height));
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v2);
	      tptr->texCoord->point.set1Value(tptr->index, offset);
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v3);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(length, 0));
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v4);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(length, -height));
	      (tptr->index)++;
	    }
	  else
	    {
	      tptr->coord->point.set1Value(tptr->index, v1);
	      tptr->texCoord->point.set1Value(tptr->index, offset);
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v2);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(0, height));
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v3);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(length, height));
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v4);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(length, 0));
	      (tptr->index)++;
	    }
	}
      else
	{
	  fprintf(stderr, "Vertex error: start %d, end %d\n",
		  LineDefs[ld_idx].start, LineDefs[ld_idx].end);
	}
    }
  else
    {
      //      fprintf(stderr, "Null tptr\n");
    }
}

void
AddLowerWallVertices(TextureList *tptr, int ld_idx, int sidedefnum)
/* sidedefnum is the number of the sidedef -- 1 or 2 */
{
  float length, height;
  SbVec3f v1, v2, v3, v4;
  SbVec2f offset;

  /* Add the four vertices of the sidedef to the coordinate lists */
  
  if (tptr)
    {
      switch (sidedefnum)
	{
	case 1:
	  {
	    int miny, maxy;

	    miny = Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].floorh;
	    maxy = Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].floorh;

	    if (miny > maxy)
	      {
		miny = Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].floorh;
		maxy = Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].floorh;
	      }

	    v1.setValue(-Vertexes[LineDefs[ld_idx].start].x,
			miny,
			Vertexes[LineDefs[ld_idx].start].y);
	    
	    v2.setValue(-Vertexes[LineDefs[ld_idx].start].x,
			maxy,
			Vertexes[LineDefs[ld_idx].start].y);
	    
	    v3.setValue(-Vertexes[LineDefs[ld_idx].end].x,
			maxy,
			Vertexes[LineDefs[ld_idx].end].y);
	    
	    v4.setValue(-Vertexes[LineDefs[ld_idx].end].x,
			miny,
			Vertexes[LineDefs[ld_idx].end].y);

	    offset.setValue(tptr->xofs + SideDefs[LineDefs[ld_idx].sidedef1].xoff,
			    tptr->yofs - SideDefs[LineDefs[ld_idx].sidedef1].yoff);
	    break;
	  }
	  
	case 2:
	  {
	    int miny, maxy;

	    miny = Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].floorh;
	    maxy = Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].floorh;

	    if (miny > maxy)
	      {
		miny = Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].floorh;
		maxy = Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].floorh;
	      }

	    v1.setValue(-Vertexes[LineDefs[ld_idx].end].x,
			miny,
			Vertexes[LineDefs[ld_idx].end].y);
	    
	    v2.setValue(-Vertexes[LineDefs[ld_idx].end].x,
			maxy,
			Vertexes[LineDefs[ld_idx].end].y);
	    
	    v3.setValue(-Vertexes[LineDefs[ld_idx].start].x,
			maxy,
			Vertexes[LineDefs[ld_idx].start].y);
	    
	    v4.setValue(-Vertexes[LineDefs[ld_idx].start].x,
			miny,
			Vertexes[LineDefs[ld_idx].start].y);

	    offset.setValue(tptr->xofs + SideDefs[LineDefs[ld_idx].sidedef2].xoff,
			    tptr->yofs - SideDefs[LineDefs[ld_idx].sidedef2].yoff);
	    break;
	  }

	default:
	  {
	    fprintf(stderr, "AddLowerWallVertices: sidedefnum must be either 1 or 2\n");
	    exit(1);
	  }
	}
      
      if ((LineDefs[ld_idx].start >= 0) &&
	  (LineDefs[ld_idx].start < NumVertexes) &&
	  (LineDefs[ld_idx].end >= 0) &&
	  (LineDefs[ld_idx].end < NumVertexes))
	{
	  SbVec3f vec1, vec2;
	  
	  vec1 = v2 - v1;
	  vec2 = v3 - v2;
	  
	  tptr->norms->vector.set1Value((tptr->index / 4),
					-(vec1.cross(vec2)));
	  
	  length = sqrt(((Vertexes[LineDefs[ld_idx].end].x - Vertexes[LineDefs[ld_idx].start].x) *
			 (Vertexes[LineDefs[ld_idx].end].x - Vertexes[LineDefs[ld_idx].start].x)) +
			((Vertexes[LineDefs[ld_idx].end].y - Vertexes[LineDefs[ld_idx].start].y) *
			 (Vertexes[LineDefs[ld_idx].end].y - Vertexes[LineDefs[ld_idx].start].y)));
	  height = abs(Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].floorh - Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].floorh);
	  
	  if (LineDefs[ld_idx].flags & (1 << 4))
	    {
	      /* Lower texture is unpegged.
		 Align texture with bottom of polygon
		 (with allowances for the height of the room). */

	      int roomHeight;
	      if (sidedefnum == 1)
		{
		  roomHeight = (Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].ceilh -
				Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].floorh);

		  offset.setValue(tptr->xofs + SideDefs[LineDefs[ld_idx].sidedef1].xoff,
				  tptr->yofs - SideDefs[LineDefs[ld_idx].sidedef1].yoff - (roomHeight % tptr->ysize));

		}
	      else
		{
		  roomHeight = (Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].ceilh -
				Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].floorh);

		  offset.setValue(tptr->xofs + SideDefs[LineDefs[ld_idx].sidedef2].xoff,
				  tptr->yofs - SideDefs[LineDefs[ld_idx].sidedef2].yoff - (roomHeight % tptr->ysize));
		}
		  
	      tptr->coord->point.set1Value(tptr->index, v1);
	      tptr->texCoord->point.set1Value(tptr->index, offset);
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v2);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(0, height));
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v3);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(length, height));
	      (tptr->index)++;
	  
	      tptr->coord->point.set1Value(tptr->index, v4);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(length, 0));
	      (tptr->index)++;
	    }
	  else
	    {
	      tptr->coord->point.set1Value(tptr->index, v1);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(0, -height));
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v2);
	      tptr->texCoord->point.set1Value(tptr->index, offset);
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v3);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(length, 0));
	      (tptr->index)++;
	  
	      tptr->coord->point.set1Value(tptr->index, v4);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(length, -height));
	      (tptr->index)++;
	    }
	}
      else
	{
	  fprintf(stderr, "Vertex error: start %d, end %d\n",
		  LineDefs[ld_idx].start, LineDefs[ld_idx].end);
	}
    }
  else
    {
      //      fprintf(stderr, "Null tptr\n");
    }
}

void
AddNormalWallVertices(TextureList *tptr, int ld_idx, int sidedefnum)
{
  float length, height;
  SbVec3f v1, v2, v3, v4;
  SbVec2f offset;

  /* Add the four vertices of the sidedef to the coordinate lists */
  
  if (tptr)
    {
      switch (sidedefnum)
	{
	case 1:
	  {
	    v1.setValue(-Vertexes[LineDefs[ld_idx].start].x,
			Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].floorh,
			Vertexes[LineDefs[ld_idx].start].y);
	    
	    v2.setValue(-Vertexes[LineDefs[ld_idx].start].x,
			Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].ceilh,
			Vertexes[LineDefs[ld_idx].start].y);
	    
	    v3.setValue(-Vertexes[LineDefs[ld_idx].end].x,
			Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].ceilh,
			Vertexes[LineDefs[ld_idx].end].y);
	    
	    v4.setValue(-Vertexes[LineDefs[ld_idx].end].x,
			Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].floorh,
			Vertexes[LineDefs[ld_idx].end].y);

	    offset.setValue(tptr->xofs + SideDefs[LineDefs[ld_idx].sidedef1].xoff,
			    tptr->yofs - SideDefs[LineDefs[ld_idx].sidedef1].yoff);
	    break;
	  }
	  
	case 2:
	  {
	    v1.setValue(-Vertexes[LineDefs[ld_idx].end].x,
			Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].floorh,
			Vertexes[LineDefs[ld_idx].end].y);
	    
	    v2.setValue(-Vertexes[LineDefs[ld_idx].end].x,
			Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].ceilh,
			Vertexes[LineDefs[ld_idx].end].y);
	    
	    v3.setValue(-Vertexes[LineDefs[ld_idx].start].x,
			Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].ceilh,
			Vertexes[LineDefs[ld_idx].start].y);
	    
	    v4.setValue(-Vertexes[LineDefs[ld_idx].start].x,
			Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].floorh,
			Vertexes[LineDefs[ld_idx].start].y);
	    
	    offset.setValue(tptr->xofs + SideDefs[LineDefs[ld_idx].sidedef2].xoff,
			    tptr->yofs - SideDefs[LineDefs[ld_idx].sidedef2].yoff);

	    break;	    
	  }
	  
	default:
	  {
	    fprintf(stderr, "AddNormalWallVertices: sidedefnum must be either 1 or 2\n");
	    exit(1);
	  }
	}
      
      if ((LineDefs[ld_idx].start >= 0) &&
	  (LineDefs[ld_idx].start < NumVertexes) &&
	  (LineDefs[ld_idx].end >= 0) &&
	  (LineDefs[ld_idx].end < NumVertexes))
	{
	  SbVec3f vec1, vec2;
	  
	  vec1 = v2 - v1;
	  vec2 = v3 - v2;
	  
	  tptr->norms->vector.set1Value((tptr->index / 4),
					-(vec1.cross(vec2)));
	  
	  length = sqrt(((Vertexes[LineDefs[ld_idx].end].x - Vertexes[LineDefs[ld_idx].start].x) *
			 (Vertexes[LineDefs[ld_idx].end].x - Vertexes[LineDefs[ld_idx].start].x)) +
			((Vertexes[LineDefs[ld_idx].end].y - Vertexes[LineDefs[ld_idx].start].y) *
			 (Vertexes[LineDefs[ld_idx].end].y - Vertexes[LineDefs[ld_idx].start].y)));
	  height = Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].ceilh - 
	    Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].floorh;
	  
	  if (LineDefs[ld_idx].flags & (1 << 4))
	    {
	      /* Lower texture is unpegged. This also affects the normal texture.
		 Align texture with bottom of polygon, this time with NO allowances
		 for the height of the room. */

	      tptr->coord->point.set1Value(tptr->index, v1);
	      tptr->texCoord->point.set1Value(tptr->index, offset);
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v2);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(0, height));
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v3);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(length, height));
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v4);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(length, 0));
	      (tptr->index)++;
	    }
	  else
	    { 
	      tptr->coord->point.set1Value(tptr->index, v1);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(0, -height));
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v2);
	      tptr->texCoord->point.set1Value(tptr->index, offset);
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v3);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(length, 0));
	      (tptr->index)++;
	      
	      tptr->coord->point.set1Value(tptr->index, v4);
	      tptr->texCoord->point.set1Value(tptr->index, offset + SbVec2f(length, -height));
	      (tptr->index)++;
	    }
	}
      else
	{
	  fprintf(stderr, "Vertex error: start %d, end %d\n",
		  LineDefs[ld_idx].start, LineDefs[ld_idx].end);
	}
    }
  else
    {
      //      fprintf(stderr, "Null tptr\n");
    }
}

VertexList *
ScanVertexList(VertexList *vptr, unsigned short int v_idx)
{
  while (vptr)
    {
      if (vptr->index >= 0)
	{
	  if ((Vertexes[v_idx].x == vptr->x) && (Vertexes[v_idx].y == vptr->y))
	    {
	      return (vptr);
	    }
	}
      vptr = vptr->next;
    }
  return(NULL);
}

LineDefList *
ScanLineDefListByIndex(LineDefList *lptr, int ld_idx)
{
  while (lptr)
    {
      if ((lptr->index >= 0) &&
	  (lptr->index == ld_idx))
	{
	  return (lptr);
	}
      lptr = lptr->next;
    }
  return(NULL);
}

SectorList *
ScanSectorList(SectorList *sptr, int whichSector)
{
  while (sptr)
    {
      if ((sptr->sectorNum >= 0) &&
	  (sptr->sectorNum == whichSector))
	{
	  return(sptr);
	}
      sptr = sptr->next;
    }
  return(NULL);
}

void
AddSectorLineDef(SectorList *sptr,
		 short int index,
		 short int start,
		 short int end,
		 short int sidedefidx)
{
  LineDefList *lptr;

  lptr = sptr->linedefs;

  while (lptr->index >= 0)
    {
      lptr = lptr->next;
    }
  
  if ((index >= 0) && (index < NumLineDefs))
    {
      lptr->index = index;
      lptr->start = start;
      lptr->end = end;
      lptr->sidedefidx = sidedefidx;
      lptr->next = new LineDefList;
      lptr->next->next = NULL;
      lptr->next->index = -1;
    }
  else
    {
      fprintf(stderr, "AddSectorLineDef: invalid linedef number %d\n", index);
    }
}

/*
void
AddFloorVertex(SectorList *sptr, int vertexIndex)
{
  VertexList *vptr;

  vptr = sptr->vertices;
  while (vptr->index >= 0)
    {
      vptr = vptr->next;
    }

  if ((vertexIndex >= 0) && (vertexIndex < NumVertexes))
    {
      vptr->index = vertexIndex;
      vptr->x = Vertexes[vertexIndex].x;
      vptr->y = Vertexes[vertexIndex].y;
      vptr->next = new VertexList;
      vptr->next->index = -1;
      vptr->next->next = NULL;
    }
  else
    {
      fprintf(stderr, "BUG: AddFloorVertex: Invalid Vertex Index\n");
    }
}
*/

/*
 * ChaseLoop
 * This function "chases" around the edge of a sector (enclosed
 * region) to create a polygon (possibly concave) for it. This polygon
 * is then used for both the floor and ceiling for the region.
 */

void
ChaseLoop(SectorList *sptr, int ld_idx, int startVertex, int currentVertex, int sector)
{
  int counter;
  int dieFlag = 0;

//  fprintf(stderr, "StartLoop: ld_idx %d, startVertex %d, currentVertex %d\n",
//	  ld_idx, startVertex, currentVertex);

 restart:

  while ((currentVertex != startVertex) && !dieFlag)
    {
      /* Scan the linedefs following the current one
	 to find the next one sharing this vertex
	 and containing a sidedef pointing to the same sector. */
      
      //      fprintf(stderr, "Added linedef %d\n", lastLineDefAdded);
      counter = ld_idx + 1;
      
      while (counter < NumLineDefs)
	{
	  /* Add the other vertex of that linedef to the vertex list and
	     make that vertex the current vertex. */
	  
	  if ((LineDefs[counter].sidedef1 >= 0) &&
	      (LineDefs[counter].sidedef1 < NumSideDefs))
	    {
	      if ((SideDefs[LineDefs[counter].sidedef1].sector >= 0) &&
		  (SideDefs[LineDefs[counter].sidedef1].sector < NumSectors))
		{
		  if ((sector == SideDefs[LineDefs[counter].sidedef1].sector) &&
		      (currentVertex == LineDefs[counter].start))
		    {
		      if (!ScanLineDefListByIndex(sptr->linedefs, counter))
			{
			  AddSectorLineDef(sptr,
					   counter,
					   LineDefs[counter].start,
					   LineDefs[counter].end,
					   1);
			  currentVertex = LineDefs[counter].end;
			  //			  fprintf(stderr, "Added vertex %d\n", currentVertex);
			  goto restart;
			}
		    }
		  else
		    {
		      if ((sector == SideDefs[LineDefs[counter].sidedef1].sector) &&
			  (currentVertex == LineDefs[counter].end))
			{
			  if (!ScanLineDefListByIndex(sptr->linedefs, counter))
			    {
			      AddSectorLineDef(sptr,
					       counter,
					       LineDefs[counter].end,
					       LineDefs[counter].start,
					       1);
			      currentVertex = LineDefs[counter].start;
			      //			      fprintf(stderr, "Added vertex %d\n", currentVertex);
			      goto restart;
			    }
			}
		    }
		}
	    }
	  
	  if ((LineDefs[counter].sidedef2 >= 0) &&
	      (LineDefs[counter].sidedef2 < NumSideDefs))
	    {
	      if ((SideDefs[LineDefs[counter].sidedef2].sector >= 0) &&
		  (SideDefs[LineDefs[counter].sidedef2].sector < NumSectors))
		{
		  if ((sector == SideDefs[LineDefs[counter].sidedef2].sector) &&
		      (currentVertex == LineDefs[counter].start))
		    {
		      if (!ScanLineDefListByIndex(sptr->linedefs, counter))
			{
			  AddSectorLineDef(sptr,
					   counter,
					   LineDefs[counter].start,
					   LineDefs[counter].end,
					   2);
			  currentVertex = LineDefs[counter].end;
			  //			  fprintf(stderr, "Added vertex %d\n", currentVertex);
			  goto restart;
			}
		    }
		  else
		    {
		      if ((sector == SideDefs[LineDefs[counter].sidedef2].sector) &&
			  (currentVertex == LineDefs[counter].end))
			{
			  if (!ScanLineDefListByIndex(sptr->linedefs, counter))
			    {
			      AddSectorLineDef(sptr,
					       counter,
					       LineDefs[counter].end,
					       LineDefs[counter].start,
					       2);
			      currentVertex = LineDefs[counter].start;
			      //			      fprintf(stderr, "Added vertex %d\n", currentVertex);
			      goto restart;
			    }
			}
		    }
		}
	    }
	  /* Repeat until we've reached the start again. */
	  counter++;
	}
      dieFlag = 1;
    }

  if (dieFlag)  /* Bug in the WAD? */
    {
      fprintf(stderr, "Killed a ChaseLoop\n");
    }
}

/*
 * ChaseExtraLoop
 * I thought this would deal with holes in polygons, but it doesn't seem
 * to...  the rationale is that you trace these extra loops in the
 * opposite direction of the previous loops to create polygons with holes
 * in them.
 * This doesn't work, and is never called.
 */
  
void
  ChaseExtraLoop(SectorList *sptr, int ld_idx, int startVertex, int currentVertex, unsigned short int sector)
{
  int counter;
  int lastLineDefAdded = ld_idx;
  int closeVertex;
  unsigned char dieFlag;
  
  /* Get the start/end vertex of the existing loop --
     we'll need this to close the loop again */
  
  closeVertex = sptr->linedefs->index;
  
 restart:

  dieFlag = 0;
  while ((currentVertex != startVertex) && !dieFlag)
    {
      /* Scan the linedefs following the current one
	 to find the next one sharing this vertex
	 and containing a sidedef pointing to the same sector. */
      
      counter = ld_idx + 1;
      
      while (counter < NumLineDefs)
	{
	  /* Add the other vertex of that linedef to the vertex list and
	     make that vertex the current vertex. */
	  
	  if ((LineDefs[counter].sidedef1 >= 0) &&
	      (LineDefs[counter].sidedef1 < NumSideDefs))
	    {
	      if ((SideDefs[LineDefs[counter].sidedef1].sector >= 0) &&
		  (SideDefs[LineDefs[counter].sidedef1].sector < NumSectors))
		{
		  if ((sector == SideDefs[LineDefs[counter].sidedef1].sector) &&
		      (currentVertex == LineDefs[counter].start))
		    {
		      if (!ScanLineDefListByIndex(sptr->linedefs, counter))
			{
			  AddSectorLineDef(sptr,
					   counter,
					   LineDefs[counter].start,
					   LineDefs[counter].end,
					   1);
			  currentVertex = LineDefs[counter].end;
			  //			  fprintf(stderr, "Added vertex %d\n", currentVertex);
			  goto restart;
			}
		    }
		  else
		    {
		      if ((sector == SideDefs[LineDefs[counter].sidedef1].sector) &&
			  (currentVertex == LineDefs[counter].end))
			{
			  if (!ScanLineDefListByIndex(sptr->linedefs, counter))
			    {
			      AddSectorLineDef(sptr,
					       counter,
					       LineDefs[counter].end,
					       LineDefs[counter].start,
					       1);
			      currentVertex = LineDefs[counter].start;
			      //			      fprintf(stderr, "Added vertex %d\n", currentVertex);
			      goto restart;
			    }
			}
		    }
		}
	    }
	  
	  if ((LineDefs[counter].sidedef2 >= 0) &&
	      (LineDefs[counter].sidedef2 < NumSideDefs))
	    {
	      if ((SideDefs[LineDefs[counter].sidedef2].sector >= 0) &&
		  (SideDefs[LineDefs[counter].sidedef2].sector < NumSectors))
		{
		  if ((sector == SideDefs[LineDefs[counter].sidedef2].sector) &&
		      (currentVertex == LineDefs[counter].start))
		    {
		      if (!ScanLineDefListByIndex(sptr->linedefs, counter))
			{
			  AddSectorLineDef(sptr,
					   counter,
					   LineDefs[counter].start,
					   LineDefs[counter].end,
					   2);
			  currentVertex = LineDefs[counter].end;
			  //			  fprintf(stderr, "Added vertex %d\n", currentVertex);
			  goto restart;
			}
		    }
		  else
		    {
		      if ((sector == SideDefs[LineDefs[counter].sidedef2].sector) &&
			  (currentVertex == LineDefs[counter].end))
			{
			  if (!ScanLineDefListByIndex(sptr->linedefs, counter))
			    {
			      AddSectorLineDef(sptr,
					       counter,
					       LineDefs[counter].end,
					       LineDefs[counter].start,
					       2);
			      currentVertex = LineDefs[counter].start;
			      //			      fprintf(stderr, "Added vertex %d\n", currentVertex);
			      goto restart;
			    }
			}
		    }
		}
	    }
	  /* Repeat until we've reached the start again. */
	  counter++;
	  
	}
      dieFlag = 1;
    }
  
  if (dieFlag)
    fprintf(stderr, "Killed a ChaseExtraLoop\n");
  else
    fprintf(stderr, "ChaseExtraLoop succeeded\n");
  /* In this case we do need to add the start vertex again, since
     it isn't part of the new loop. */
  
  //  AddFloorVertex(sptr, closeVertex);
}

void
PrepareToChaseLoop(SectorList *sptr, int ld_idx, int sd_idx, int newloop)
{
  unsigned short int startVertex;
  unsigned short int currentVertex;
  unsigned short int sector;

/*  fprintf(stderr, "Dumping LineDef %d, SideDef %d, Sector %d\n",
	  ld_idx,
	  (sd_idx == 1)?LineDefs[ld_idx].sidedef1:LineDefs[ld_idx].sidedef2,
	  (sd_idx == 1)?SideDefs[LineDefs[ld_idx].sidedef1].sector:SideDefs[LineDefs[ld_idx].sidedef2].sector); */

  if (!newloop)
    {
      switch (sd_idx)
	{
	case 1:
	  {
	    startVertex = LineDefs[ld_idx].start;
	    currentVertex = LineDefs[ld_idx].end;
	    AddSectorLineDef(sptr,
			     ld_idx,
			     LineDefs[ld_idx].start,
			     LineDefs[ld_idx].end,
			     sd_idx);
	    sector = SideDefs[LineDefs[ld_idx].sidedef1].sector;
	    break;
	  }
	case 2:
	  {
	    startVertex = LineDefs[ld_idx].end;
	    currentVertex = LineDefs[ld_idx].start;
	    AddSectorLineDef(sptr,
			     ld_idx,
			     LineDefs[ld_idx].end,
			     LineDefs[ld_idx].start,
			     sd_idx);
	    sector = SideDefs[LineDefs[ld_idx].sidedef2].sector;
	    break;
	  }
	default:
	  {
	    fprintf(stderr, "PrepareToChaseLoop: sd_idx must be 1 or 2\n");
	    exit(1);
	  }
	}
      ChaseLoop(sptr, ld_idx, startVertex, currentVertex, sector);
    }
  else   /* Need to add loop to end of existing one. */
    {
      /*
      switch (sd_idx)
	{
	case 1:
	  {
	    startVertex = LineDefs[ld_idx].start;
	    currentVertex = LineDefs[ld_idx].end;
	    AddSectorLineDef(sptr,
			     ld_idx,
			     LineDefs[ld_idx].start,
			     LineDefs[ld_idx].end,
			     sd_idx);
	    sector = SideDefs[LineDefs[ld_idx].sidedef1].sector;
	    break;
	  }
	case 2:
	  {
	    startVertex = LineDefs[ld_idx].end;
	    currentVertex = LineDefs[ld_idx].start;
	    AddSectorLineDef(sptr,
			     ld_idx,
			     LineDefs[ld_idx].end,
			     LineDefs[ld_idx].start,
			     sd_idx);
	    sector = SideDefs[LineDefs[ld_idx].sidedef2].sector;
	    break;
	  }
	default:
	  {
	    fprintf(stderr, "PrepareToChaseLoop: sd_idx must be 1 or 2\n");
	    exit(1);
	  }

	}
      ChaseExtraLoop(sptr, ld_idx, startVertex, currentVertex, sector);
      */
    }
}
    
void
AddAllFloorAndCeilingVertices(TextureList *texlist)
{
  SectorList *sectors;
  SectorList *slast;
  SectorList *sptr;
  int ld_idx;
  int sd_idx;
  int vertexCounter;
  TextureList *tptr;
  LineDefList *lptr;
  
  sectors = new SectorList;
  sectors->next = NULL;
  sectors->sectorNum = -1;
  sectors->linedefs = new LineDefList;
  sectors->linedefs->index = -1;
  sectors->linedefs->next = NULL;
  slast = sectors;

  SbBox3f *bbox = new SbBox3f;

  /* Build up the list of sectors and the vertices surrounding them */

  for (ld_idx = 0; ld_idx < NumLineDefs; ld_idx++)
    {
      sd_idx = 1;

      if ((LineDefs[ld_idx].sidedef1 >= 0) &&
	  (LineDefs[ld_idx].sidedef1 < NumSideDefs))
	{
	  if ((SideDefs[LineDefs[ld_idx].sidedef1].sector >= 0) &&
	      (SideDefs[LineDefs[ld_idx].sidedef1].sector < NumSectors))
	    {
	      if (Sectors[SideDefs[LineDefs[ld_idx].sidedef1].sector].floort[0] != '-')
		{
		  sptr = ScanSectorList(sectors, SideDefs[LineDefs[ld_idx].sidedef1].sector);
		  if (!sptr)
		    {
		      PrepareToChaseLoop(slast, ld_idx, sd_idx, 0);
		      slast->sectorNum = SideDefs[LineDefs[ld_idx].sidedef1].sector;
		      slast->next = new SectorList;
		      slast = slast->next;
		      slast->next = NULL;
		      slast->sectorNum = -1;
		      slast->linedefs = new LineDefList;
		      slast->linedefs->index = -1;
		      slast->linedefs->next = NULL;
		    }
		  else
		    {
		      /* Only add the loop if it hasn't been added before */
		      if (!ScanLineDefListByIndex(sptr->linedefs, ld_idx))
			PrepareToChaseLoop(sptr, ld_idx, sd_idx, 1);
		    }
		}
	      else
		fprintf(stderr, "Got a floor with no texture: LineDef %d, SideDef 1, Sector %d\n",
		       ld_idx, SideDefs[LineDefs[ld_idx].sidedef1].sector);
	    }
	}

      sd_idx++;

      if ((LineDefs[ld_idx].sidedef2 >= 0) &&
	  (LineDefs[ld_idx].sidedef2 < NumSideDefs))
	{
	  if ((SideDefs[LineDefs[ld_idx].sidedef2].sector >= 0) &&
	      (SideDefs[LineDefs[ld_idx].sidedef2].sector < NumSectors))
	    {
	      if (Sectors[SideDefs[LineDefs[ld_idx].sidedef2].sector].floort[0] != '-')
		{
		  sptr = ScanSectorList(sectors, SideDefs[LineDefs[ld_idx].sidedef2].sector);
		  if (!sptr)
		    {
		      PrepareToChaseLoop(slast, ld_idx, sd_idx, 0);
		      slast->sectorNum = SideDefs[LineDefs[ld_idx].sidedef2].sector;
		      slast->next = new SectorList;
		      slast = slast->next;
		      slast->next = NULL;
		      slast->sectorNum = -1;
		      slast->linedefs = new LineDefList;
		      slast->linedefs->index = -1;
		      slast->linedefs->next = NULL;
		    }
		  else
		    {
		      /* Only add the loop if it hasn't been added before */
		      if (!ScanLineDefListByIndex(sptr->linedefs, ld_idx))
			PrepareToChaseLoop(sptr, ld_idx, sd_idx, 1);
		    }
		}
	      else
		fprintf(stderr, "Got a floor with no texture: LineDef %d, SideDef 2, Sector %d\n",
		       ld_idx, SideDefs[LineDefs[ld_idx].sidedef2].sector);
	    }
	}
    }

  /* Now add these sectors to the floor texture list.
     Many sectors may share the same texture and we'd
     like them to all show up under the same separator. */

  sptr = sectors;

  while (sptr)
    {
      if (sptr->sectorNum >= 0)
	{
	  vertexCounter = 0;
	  lptr = sptr->linedefs;
	  tptr = ScanTextureList(texlist, Sectors[sptr->sectorNum].floort);
	  if (tptr)
	    {
	      while (lptr)
		{
		  if (lptr->index >= 0)
		    {
		      tptr->coord->point.set1Value(tptr->index,
						   SbVec3f(-Vertexes[lptr->start].x,
							   Sectors[sptr->sectorNum].floorh,
							   Vertexes[lptr->start].y));
		      
		      tptr->texCoord->point.set1Value(tptr->index,
						      SbVec2f(-Vertexes[lptr->start].x,
							      Vertexes[lptr->start].y));
		      
		      (tptr->index)++;
		      vertexCounter++;
		    }
		  lptr = lptr->next;
		}
	      
	      tptr->norms->vector.set1Value(tptr->faceidx,
					    SbVec3f(0, 1, 0));
	      tptr->polys->numVertices.set1Value(tptr->faceidx, vertexCounter);
	      (tptr->faceidx)++;
	    }
	  else
	    {
	      fprintf(stderr, "AddAllFloorAndCeilingVertices: FATAL ERROR: texture %.8s not found\n",
		      Sectors[sptr->sectorNum].floort);
	      exit(1);
	    }


	  vertexCounter = 0;
	  lptr = sptr->linedefs;
	  tptr = ScanTextureList(texlist, Sectors[sptr->sectorNum].ceilt);
	  if (tptr)
	    {
	      while (lptr)
		{
		  if (lptr->index >= 0)
		    {
		      tptr->coord->point.set1Value(tptr->index,
						   SbVec3f(-Vertexes[lptr->start].x,
							   Sectors[sptr->sectorNum].ceilh,
							   Vertexes[lptr->start].y));
		      
		      tptr->texCoord->point.set1Value(tptr->index,
						      SbVec2f(-Vertexes[lptr->start].x,
							      Vertexes[lptr->start].y));
		      
		      (tptr->index)++;
		      vertexCounter++;
		    }
		  lptr = lptr->next;
		}
	      
	      tptr->norms->vector.set1Value(tptr->faceidx,
					    SbVec3f(0, -1, 0));
	      tptr->polys->numVertices.set1Value(tptr->faceidx, vertexCounter);
	      (tptr->faceidx)++;
	    }
	  else
	    {
	      fprintf(stderr, "AddAllFloorAndCeilingVertices (2): FATAL ERROR: texture %.8s not found\n",
		      Sectors[sptr->sectorNum].floort);
	      exit(1);
	    }
	}
      sptr = sptr->next;
    }
}

void
  AddWallTextureListToSceneGraph(TextureList *tl)
{
  int counter;
  
  while (tl)
    {
      if (tl->index)
	{
	  tl->sep->addChild(tl->texture);

	  tl->sep->addChild(tl->tXform);

	  tl->sep->addChild(tl->norms);

	  tl->nBind->value.setValue(SoNormalBinding::PER_FACE);
	  tl->sep->addChild(tl->nBind);

	  tl->sep->addChild(tl->coord);

	  tl->sep->addChild(tl->texCoord);

	  tl->tBind->value.setValue(SoTextureCoordinateBinding::PER_VERTEX);
	  tl->sep->addChild(tl->tBind);

	  for(counter = 0; counter < (tl->index/4); counter++)
	    {
	      tl->polys->numVertices.set1Value(counter, 4);
	    }

	  tl->sep->addChild(tl->polys);

	  root->addChild(tl->sep);
	}
      tl = tl->next;
    }
}

void
  AddFloorTextureListToSceneGraph(TextureList *tl)
{
  SoSeparator *floorRoot = new SoSeparator;
  floorRoot->ref();

  SoShapeHints *floorHints = new SoShapeHints;
  floorHints->faceType.setValue(SoShapeHints::UNKNOWN_FACE_TYPE);

  floorRoot->addChild(floorHints);

  while (tl)
    {
      if (tl->index)
	{
	  tl->sep->addChild(tl->texture);

	  tl->tXform->scaleFactor.setValue(SbVec2f((1. / 64.), (1. / 64.)));
	  tl->sep->addChild(tl->tXform);

	  tl->sep->addChild(tl->norms);

	  tl->nBind->value.setValue(SoNormalBinding::PER_FACE);
	  tl->sep->addChild(tl->nBind);

	  tl->sep->addChild(tl->coord);

	  tl->sep->addChild(tl->texCoord);

	  tl->tBind->value.setValue(SoTextureCoordinateBinding::PER_VERTEX);
	  tl->sep->addChild(tl->tBind);

	  tl->sep->addChild(tl->polys);

	  floorRoot->addChild(tl->sep);
	}
      tl = tl->next;
    }
  root->addChild(floorRoot);
}

void
UpdateBoundingBox(float x, 
		  float y,
		  float z, 
		  SbVec3f &min,
		  SbVec3f &max)
{
  if (min == SbVec3f(0, 0, 0))
    min.setValue(x, y, z);
  else
    {
      if (x <= min[0])
	min.setValue(x, min[1], min[2]);

      if (y <= min[1])
	min.setValue(min[0], y, min[2]);

      if (z <= min[2])
	min.setValue(min[0], min[1], z);
    }

  if (max == SbVec3f(0, 0, 0))
    max.setValue(x, y, z);
  else
    {
      if (x >= max[0])
	max.setValue(x, max[1], max[2]);

      if (y >= max[1])
	max.setValue(max[0], y, max[2]);

      if (z >= max[2])
	max.setValue(max[0], max[1], z);
    }
}  

SbBox3f *
ComputeBoundingBoxOfLevel()
{
  int counter;

  SbVec3f min(0,0,0), max(0,0,0);

  for(counter = 0; counter < NumLineDefs; counter++)
    {
      if ((LineDefs[counter].sidedef1 >= 0) && (LineDefs[counter].sidedef1 < NumSideDefs))
	{
	  float x, y, z;

	  x = -Vertexes[LineDefs[counter].start].x;
	  y = Sectors[SideDefs[LineDefs[counter].sidedef1].sector].floorh;
	  z = Vertexes[LineDefs[counter].start].y;

	  UpdateBoundingBox(x, y, z, min, max);

	  x = -Vertexes[LineDefs[counter].start].x;
	  y = Sectors[SideDefs[LineDefs[counter].sidedef1].sector].ceilh;
	  z = Vertexes[LineDefs[counter].start].y;

	  UpdateBoundingBox(x, y, z, min, max);

	  x = -Vertexes[LineDefs[counter].end].x;
	  y = Sectors[SideDefs[LineDefs[counter].sidedef1].sector].floorh;
	  z = Vertexes[LineDefs[counter].end].y;

	  UpdateBoundingBox(x, y, z, min, max);

	  x = -Vertexes[LineDefs[counter].end].x;
	  y = Sectors[SideDefs[LineDefs[counter].sidedef1].sector].ceilh;
	  z = Vertexes[LineDefs[counter].end].y;

	  UpdateBoundingBox(x, y, z, min, max);
	}

      if ((LineDefs[counter].sidedef2 >= 0) && (LineDefs[counter].sidedef2 < NumSideDefs))
	{
	  float x, y, z;

	  x = -Vertexes[LineDefs[counter].start].x;
	  y = Sectors[SideDefs[LineDefs[counter].sidedef2].sector].floorh;
	  z = Vertexes[LineDefs[counter].start].y;

	  UpdateBoundingBox(x, y, z, min, max);

	  x = -Vertexes[LineDefs[counter].start].x;
	  y = Sectors[SideDefs[LineDefs[counter].sidedef2].sector].ceilh;
	  z = Vertexes[LineDefs[counter].start].y;

	  UpdateBoundingBox(x, y, z, min, max);

	  x = -Vertexes[LineDefs[counter].end].x;
	  y = Sectors[SideDefs[LineDefs[counter].sidedef2].sector].floorh;
	  z = Vertexes[LineDefs[counter].end].y;

	  UpdateBoundingBox(x, y, z, min, max);

	  x = -Vertexes[LineDefs[counter].end].x;
	  y = Sectors[SideDefs[LineDefs[counter].sidedef2].sector].ceilh;
	  z = Vertexes[LineDefs[counter].end].y;

	  UpdateBoundingBox(x, y, z, min, max);
	}
    }

  SbBox3f *bbox = new SbBox3f(min, max);
  return bbox;
}

void
SanityDumpTextureList(TextureList *tl)
{
  TextureList *tmp = tl;

  while(tmp)
    {
      fprintf(stderr, "%s -> ", tmp->name);
      tmp = tmp->next;
    }
  fprintf(stderr, " NULL\n");
}

void
main(int argc, char **argv)
{
  int c;
  char *wadfilename = NULL;
  char *outfilename = NULL;
  char *MainWad = "doom.wad";
  char *MainWad2 = "doom2.wad";
  char levelname[4];
  char levelfilename[10];
  int cv_episode = 1;
  int cv_mission = 1;
  TextureList *ftl, *ftl_last; /* Floor and ceiling texture list */
  TextureList *wtl, *wtl_last; /* Wall texture list */
  TextureList *tptr;
  int counter;

  ftl = new TextureList;
  bzero((void *)(ftl->name), 8);
  ftl->next = NULL;
  ftl->index = 0;
  ftl->faceidx = 0;
  ftl->sep = new SoSeparator;
  ftl->sep->ref();
  ftl->texture = new SoTexture2;
  ftl->texture->ref();
  ftl->tXform = new SoTexture2Transform;
  ftl->tXform->ref();
  ftl->texCoord = new SoTextureCoordinate2;
  ftl->texCoord->ref();
  ftl->tBind = new SoTextureCoordinateBinding;
  ftl->tBind->ref();
  ftl->coord = new SoCoordinate3;
  ftl->coord->ref();
  ftl->norms = new SoNormal;
  ftl->norms->ref();
  ftl->nBind = new SoNormalBinding;
  ftl->nBind->ref();
  ftl->polys = new SoFaceSet;
  ftl->polys->ref();
  ftl_last = ftl;

  wtl = new TextureList;
  bzero((void *)(wtl->name), 8);
  wtl->next = NULL;
  wtl->index = 0;
  wtl->sep = new SoSeparator;
  wtl->sep->ref();
  wtl->texture = new SoTexture2;
  wtl->texture->ref();
  wtl->tXform = new SoTexture2Transform;
  wtl->tXform->ref();
  wtl->texCoord = new SoTextureCoordinate2;
  wtl->texCoord->ref();
  wtl->tBind = new SoTextureCoordinateBinding;
  wtl->tBind->ref();
  wtl->coord = new SoCoordinate3;
  wtl->coord->ref();
  wtl->norms = new SoNormal;
  wtl->norms->ref();
  wtl->nBind = new SoNormalBinding;
  wtl->nBind->ref();
  wtl->polys = new SoFaceSet;
  wtl->polys->ref();
  wtl_last = wtl;

  fprintf(stderr, "wadtoiv version %s\n", WADTOIV_VERSION_NUMBER);
  fprintf(stderr, "Release date: %s\n\n", WADTOIV_VERSION_DATE);

  if (argc == 1)
    {
      cout << "Usage: wadtoiv [-f <WAD filename>] [-e <episode>] [-m <mission>]\n";
      exit(0);
    }

  while ((c = getopt(argc, argv, "2df:e:m:o:p:")) != EOF) 
    {
      switch (c) 
	{
	case '2':
	  doom2 = 1;
	  break;

	case 'd':
	  debug_files = 1;
	  break;

	case 'f':
	  wadfilename = optarg;
	  break;

	case 'e':
	  cv_episode = atoi(optarg);
	  break;

	case 'm':
	  cv_mission = atoi(optarg);
	  break;

	case 'o':
	  outfilename = optarg;
	  break;
	  
	case 'p':
	  palettenum = atoi(optarg);
	  break;
	}
    }

  SoDB::init();

  root = new SoSeparator;
  root->ref();

  // Add a little light to the scene
  SoDirectionalLight *dl1 = new SoDirectionalLight;
  dl1->direction.setValue(0, -1, 0);
  dl1->intensity.setValue(0.6);
  root->addChild(dl1);

  SoDirectionalLight *dl2 = new SoDirectionalLight;
  dl2->direction.setValue(0, 1, 0);
  dl2->intensity.setValue(0.6);
  root->addChild(dl2);

  SoShapeHints *myShapeHints = new SoShapeHints;
  myShapeHints->vertexOrdering.setValue(SoShapeHints::CLOCKWISE);
  myShapeHints->shapeType.setValue(SoShapeHints::SOLID);
  root->addChild(myShapeHints);

  if (doom2)
    {
      /* only boundary check the mission */

      if ((cv_mission < 1) || (cv_mission > 32))
	{
	  cerr << "Error: mission must be between 1 and 32 for Doom 2" << endl;
	  exit(1);
	}
      OpenMainWad(MainWad2);
      sprintf(levelname, "MAP%.2d", cv_mission);
    }
  else
    {
      /* boundary check episode and mission */

      if ((cv_episode < 1) || (cv_episode > 3))
	{
	  cerr << "Error: episode must be between 1 and 3 for Doom 1" << endl;
	  exit(1);
	}

      if ((cv_mission < 1) || (cv_mission > 9))
	{
	  cerr << "Error: mission must be between 1 and 9 for Doom 1" << endl;
	  exit(1);
	}
      OpenMainWad(MainWad);
      sprintf(levelname, "E%dM%d", cv_episode, cv_mission);
    }

  if (wadfilename != NULL)
    OpenPatchWad(wadfilename, doom2);

  ReadLevelData(cv_episode, cv_mission, doom2);

  printf("\n");
  /* Go through all the linedefs */
  
  fprintf(stderr, "NumLineDefs: %d NumSideDefs: %d NumSectors: %d\n",
	  NumLineDefs, NumSideDefs, NumSectors);

  fprintf(stderr, "Outputting textures...\n"); 

  for(counter = 0; counter < NumLineDefs; counter++)
    {
      /* Dump the floor/ceiling textures of all sectors
	 adjoining SideDefs referenced by LineDefs */
      
      if ((LineDefs[counter].sidedef1 >= 0) && (LineDefs[counter].sidedef1 < NumSideDefs))
	{
	  tptr = ConditionalFloorTextureDump(ftl, &ftl_last, Sectors[SideDefs[LineDefs[counter].sidedef1].sector].floort);
	  tptr = ConditionalFloorTextureDump(ftl, &ftl_last, Sectors[SideDefs[LineDefs[counter].sidedef1].sector].ceilt);
	  
	  /* Dump out the vertices of the upper wall surfaces */
	  
	  tptr = ConditionalWallTextureDump(wtl, &wtl_last, SideDefs[LineDefs[counter].sidedef1].tex1);
	  AddUpperWallVertices(tptr, counter, 1);
	  
	  /* Dump out the vertices of the lower wall surfaces */
	  
	  tptr = ConditionalWallTextureDump(wtl, &wtl_last, SideDefs[LineDefs[counter].sidedef1].tex2);
	  AddLowerWallVertices(tptr, counter, 1);
	  
	  /* Dump out the vertices of the normal wall surfaces */
	  
	  tptr = ConditionalWallTextureDump(wtl, &wtl_last, SideDefs[LineDefs[counter].sidedef1].tex3);
	  AddNormalWallVertices(tptr, counter, 1);
	}

      if ((LineDefs[counter].sidedef2 >= 0) && (LineDefs[counter].sidedef2 < NumSideDefs))
	{
	  tptr = ConditionalFloorTextureDump(ftl, &ftl_last, Sectors[SideDefs[LineDefs[counter].sidedef2].sector].floort);
	  tptr = ConditionalFloorTextureDump(ftl, &ftl_last, Sectors[SideDefs[LineDefs[counter].sidedef2].sector].ceilt);
	  
	  tptr = ConditionalWallTextureDump(wtl, &wtl_last, SideDefs[LineDefs[counter].sidedef2].tex1);
	  AddUpperWallVertices(tptr, counter, 2);
	  
	  tptr = ConditionalWallTextureDump(wtl, &wtl_last, SideDefs[LineDefs[counter].sidedef2].tex2);
	  AddLowerWallVertices(tptr, counter, 2);
	  
	  tptr = ConditionalWallTextureDump(wtl, &wtl_last, SideDefs[LineDefs[counter].sidedef2].tex3);
	  AddNormalWallVertices(tptr, counter, 2);
	  
	  /*	  printf("Two-sided sidedef: %d %d\n", counter, LineDefs[counter].sidedef2); */
	}
    }

  if (doom2)
    sprintf(levelfilename, "%.5s.iv", levelname);
  else    
    sprintf(levelfilename, "%.4s.iv", levelname);

  fprintf(stderr, "Building floor vertex lists...\n");

  AddWallTextureListToSceneGraph(wtl);
  AddAllFloorAndCeilingVertices(ftl);
  AddFloorTextureListToSceneGraph(ftl);

  SbBox3f *bbox;
  bbox = ComputeBoundingBoxOfLevel();
  float xmin, ymin, zmin, xmax, ymax, zmax;
  bbox->getBounds(xmin, ymin, zmin, xmax, ymax, zmax);
  fprintf(stderr, "Bounding box of level: (%f, %f, %f) to (%f, %f, %f)\n",
	  xmin, ymin, zmin, xmax, ymax, zmax);	  

  fprintf(stderr, "Outputting scene graph as %s...\n", levelfilename);

  SoOutput *out = new SoOutput;
  if (out->openFile(levelfilename))
    {
      SoWriteAction *wa = new SoWriteAction(out);
      wa->apply(root);
    }
  out->closeFile();
  CloseWadFiles();
}
