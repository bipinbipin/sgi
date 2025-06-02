/******************************************************************************
 *
 * File:        manymovieWin.h
 *
 * Description: External interface to manymovieWin.c. Creates X windows
 *              suitable for GL rendering for all the movies, opens the
 *              movies and the windows. Provides access to the X Display.
 *
 ******************************************************************************/

#include <X11/Xlib.h>

extern void openAllMovies( void );

extern Display *getXDisplay( void );
