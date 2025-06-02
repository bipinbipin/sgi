#ifndef __WSTRUCTS
#define __WSTRUCTS

/*
   Doom Editor Utility, by Brendon Wyber and Rapha‰l Quinet.

   You are allowed to use any parts of this code in another program, as
   long as you give credits to the authors in the documentation and in
   the program itself.  Read the file README.1ST for more information.

   This program comes with absolutely no warranty.

   WSTRUCTS.H - WAD files data structures.
*/


/*
   this data structure contains the information about the THINGS
*/

struct Thing
{
   short int xpos;      /* x position */
   short int ypos;      /* y position */
   short int angle;     /* facing angle */
   short int type;      /* thing type */
   short int when;      /* appears when? */
};
typedef struct Thing *TPtr;



/*
   this data structure contains the information about the LINEDEFS
*/
struct LineDef
{
   short int start;     /* from this vertex ... */
   short int end;       /* ... to this vertex */
   short int flags;     /* see NAMES.C for more info */
   short int type;      /* see NAMES.C for more info */
   short int tag;       /* crossing this linedef activates the sector with the same tag */
   short int sidedef1;  /* sidedef */
   short int sidedef2;  /* only if this line adjoins 2 sectors */
};
typedef struct LineDef *LDPtr;



/*
   this data structure contains the information about the SIDEDEFS
*/
struct SideDef
{
   short int xoff;      /* X offset for texture */
   short int yoff;      /* Y offset for texture */
   char tex1[8];  /* texture name for the part above */
   char tex2[8];  /* texture name for the part below */
   char tex3[8];  /* texture name for the regular part */
   short int sector;    /* adjacent sector */
};
typedef struct SideDef *SDPtr;



/*
   this data structure contains the information about the VERTEXES
*/
struct Vertex
{
   short int x;         /* X coordinate */
   short int y;         /* Y coordinate */
};
typedef struct Vertex *VPtr;



/*
   this data structure contains the information about the SEGS
*/
typedef struct Seg *SEPtr;
struct Seg
{
   SEPtr next;    /* next Seg in list */
   short int start;     /* from this vertex ... */
   short int end;       /* ... to this vertex */
   unsigned angle;/* angle (0 = east, 16384 = north, ...) */
   short int linedef;   /* linedef that this seg goes along*/
   short int flip;      /* true if not the same direction as linedef */
   unsigned dist; /* distance from starting point */
};



/*
   this data structure contains the information about the SSECTORS
*/
typedef struct SSector *SSPtr;
struct SSector
{
   SSPtr next;	  /* next Sub-Sector in list */
   short int num;       /* number of Segs in this Sub-Sector */
   short int first;     /* first Seg */
};



/*
   this data structure contains the information about the NODES
*/
typedef struct Node *NPtr;
struct Node
{
   short int x, y;                         /* starting point */
   short int dx, dy;                       /* offset to ending point */
   short int miny1, maxy1, minx1, maxx1;   /* bounding rectangle 1 */
   short int miny2, maxy2, minx2, maxx2;   /* bounding rectangle 2 */
   short int child1, child2;               /* Node or SSector (if high bit is set) */
   NPtr node1, node2;                /* pointer if the child is a Node */
   short int num;                          /* number given to this Node */
};



/*
   this data structure contains the information about the SECTORS
*/
struct Sector
{
   short int floorh;    /* floor height */
   short int ceilh;     /* ceiling height */
   char floort[8];/* floor texture */
   char ceilt[8]; /* ceiling texture */
   short int light;     /* light level (0-255) */
   short int special;   /* special behaviour (0 = normal, 9 = secret, ...) */
   short int tag;       /* sector activated by a linedef with the same tag */
};
typedef struct Sector *SPtr;

#endif

/* end of file */

