/*****************************************************************************
 *
 * File:	createmovieArgs.h
 *
 * Description: External interface to createmovieArgs.c.
 *
 *****************************************************************************/

#include <dmedia/moviefile.h>
#include <dmedia/movieplay.h>

#define OPTION_NOT_SET 0

typedef enum _srcFileType
{
    movieType,
    imageType,
    audioType
} srcFileType;

extern void processCmdArgs( int argc, char **argv );

extern MVfileformat getMovieFormat( void );

extern MVloopmode getLoopMode( void );

extern char *getCompressionScheme( void );

extern int getFrameWidth( void );

extern int getFrameHeight( void );

extern double getFrameRate( void );

extern double getSpatialQuality( void );

extern int getKeyFrameRate( void );

extern DMimageorientation getOrientation( void );

extern DMinterlacing getInterlacing( void );

extern char *getOutMovieName( void );

extern char *getProgramName( void );

extern char *getUserParamType( void );

extern char *getUserParamName( void );

extern char *getUserParamValue( void );

extern int getNumFiles( void );

extern char *getFirstFileName( srcFileType theType );

extern int getFilePosition( char *theFile );

extern void getFileAndTypeByPos( int pos, char **theFile,
				     srcFileType *theType );

extern char *getFirstAudioMovie( void );

extern DMboolean haveSrcFileType( srcFileType theType );

extern DMboolean haveWidthAndHeight( void );
