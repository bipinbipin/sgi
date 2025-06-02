/******************************************************************************
 *
 * File:         createmovie.c++
 *
 * Usage:        createmovie [-c compression] [-l loopMode] [ -r frameRate]
 *			     [-s xsize,ysize] [-o outMovie] file . . .
 *		 file may include one or more image, audio, and movie files.
 *   		 Compression schemes: none mvc1 rle jpeg 8rgb (default is mvc1)
 *  		 Loop modes:          once loop swing         (default is once)
 *
 * Description:  Command line program to make a movie file from image, audio,
 *		 and/or other movie files.
 *
 * Functions:    The following SGI Movie Library functions are used:
 *
 *               mvOpenFile()
 *               mvSetMovieDefaults()
 *		 mvSetLoopMode()
 *               mvCreateFile()
 *               mvGetErrorStr()
 *               mvGetErrno()
 *               mvClose()
 *               mvGetParams()
 *               mvSetParams()
 *		 mvAddUserParam()
 *               mvAddTrack()
 *               mvFindTrackByMedium()
 *               mvInsertFrames()
 *               mvGetAudioRate()
 *               mvGetImageRate()
 *               mvGetAudioWidth()
 *               mvGetTrackLength()
 *
 *               The following SGI Digital Media Library functions are used:
 *
 *               dmParamsCreate()
 *               dmParamsDestroy()
 *               dmSetImageDefaults()
 *               dmSetAudioDefaults()
 *               dmParamsSetInt()
 *               dmParamsSetFloat()
 *               dmParamsSetEnum()
 *               dmParamsSetString()
 *               dmImageFrameSize()
 *
 ******************************************************************************/

#include "createmovieArgs.h"
#include "createmovieInit.h"
#include "createmovieFiles.h"
#include <dmedia/moviefile.h>

/*
 * Forward declarations of functions that appear below.
 */
    
static void makeMovie( MVid *theMovie );

/**********************************************************************
 *
 * main
 *
 **********************************************************************/

main( int argc, char **argv )
{
    MVid theMovie;
   
    processCmdArgs( argc, argv );
    
    makeMovie( &theMovie );

    mvClose( theMovie );

    exit( EXIT_SUCCESS );
}

/*********
 *
 * Make the new movie file.
 *
 *********/

static void makeMovie( MVid *theMovie )
{

    initMovie( theMovie ); 

    putFilesInMovie( *theMovie );

}
