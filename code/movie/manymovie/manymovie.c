/*****************************************************************************
 * File:        manymovie.c - Also see manymovieArgs.c, manymovieWin.c,
 *              manymovieEvents.c, and the header files with the same names.
 *
 * Usage:       manymovie [-one] <moviefile1> [moviefile2 ...]
 *
 * Description: Sample program which plays several movies at once with
 *              the SGI movie library. If the -one command line option
 *              is used, all movies are played in the same window. Otherwise,
 *              each movie is played in its own window. Manymovie uses 
 *              a keyboard ui which has the following controls (see
 *              manymovieEvents.c):
 *              
 *                 Key       Action              Functions Invoked
 *
 *              p or P     play the movie     mvSetPrimaryAudio(), mvPlay()
 *
 *              s or S     stop the movie            mvStop()
 *
 *              r or R     rewind to the beginning   mvSetCurrentFrame()
 *
 *              m or M     toggle audio muting       mvGetEnableAudio(),
 *                                                   mvSetEnableAudio
 *
 *              l or L     change looping state      mvGetPlayLoopMode(),
 *                                                   mvSetPlayLoopMode()
 *
 *              q or Q     quit manymovie            mvStop(), mvClose()
 *
 * Functions:   Other SGI Movie Library functions used (see other modules):
 *
 *              mvOpenFile()
 *              mvGetEvenFD()
 *              mvSetSelectEvents()
 *              mvPendingEvents()
 *              mvNextEvent()
 *              mvFindTrackByMedium()
 *              mvGetImageWidth()
 *              mvGetImageHeight()
 *              mvBindWindow()
 *              mvShowCurrentFrame()
 *              mvResizeWindow()
 *              mvQueryViewSize()
 *              mvSetViewSize()
 *		mvSetNumMoviesHint()
 *		mvGetErrorStr()
 *		mvGetErrno()
 *
 ****************************************************************************/

#include "manymovieArgs.h"
#include "manymovieWin.h"
#include "manymovieEvents.h"

/***************************************************************************
 *
 * main
 *
 ***************************************************************************/

main ( int argc, char**argv )
{
    /*
     * Verify that valid movie files have been entered, and build the
     * movie name list. See manymovieArgs.c.
     */

    processCmdArgs( argc, argv );

    /*
     * Open each movie file, create a window for it, and display the window.
     * See manymovieWin.c.
     */

    openAllMovies();

    /*
     * Receive and act on appropriate X and Movie events, including user
     * commands entered via the keyboard interface. See manymovieEvents.c.
     */

    handleEvents();

    exit( EXIT_SUCCESS );			    /* good bye. */
}
