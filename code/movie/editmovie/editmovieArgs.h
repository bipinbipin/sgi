/******************************************************************************
 *
 * File:	editmovieArgs.h
 *
 * Description:	Declarations for public functions in editmovieArgs.c.
 *
 *****************************************************************************/

#include <dmedia/moviefile.h>

/*
 * The available editing operations.
 */

typedef enum _editOp
{
    unknownOp,
    copyOp,
    deleteOp,
    insertOp,
    pasteOp
} editOp;

extern void 	 processCmdArgs( int argc, char **argv );

extern DMboolean editInMemory( void );

extern editOp 	 getEditOperation( void );

extern char 	 *getProgramName( void );

extern char 	 *getEditMovieName( void );

extern char 	 *getSourceMovieName( void );

extern char 	 *getOutMovieName( void );

extern char 	 *getCopyFileName( void );

extern char 	 *getCopyFileFormat( void );

extern MVframe 	 getNumEditFrames( void );

extern MVframe 	 getFirstEditFrame( void );

extern MVframe 	 getFirstSourceFrame( void );

extern DMmedium  getEditTrackType( void );
