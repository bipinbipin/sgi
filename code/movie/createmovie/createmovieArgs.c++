/*****************************************************************************
 *
 * File:	createmovieqtArgs.c++
 *
 * Description: Part of createmovieqt. Command line processing functions,
 *		access to user preferences, and creation and access to
 *		the file names entered via the command line.
 *
 * Functions:   SGI Movie Library functions used:
 *
 *		mvOpenFile()
 *		mvIsMovieFile()
 *              mvFindTrackByMedium()
 *              mvGetErrorStr()
 *		mvGetErrno()
 *
 *
 *****************************************************************************/

#include "createmovieArgs.h"
#include <dmedia/moviefile.h>
#include <dmedia/audiofile.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
#include <il2.5/ilImage.h>
#include <il2.5/ilGenericImgFile.h>

typedef enum _compScheme
{
    unknownComp,
    none,
    mvc1,
    mvc2,
    rle,
    rle24,
    jpeg,
    rgb8,
    qtvideo,
    qtanim,
    qtcvid,
    indeo    
} compScheme;

typedef struct _namelist {
    char                *name;
    int                 position;
    struct _namelist    *next;
} namelist;

/*
 * Variables set in processCmdArgs() which are restricted to this module.
 */

static char 	    *programName      = NULL;       /* Name of the program. */
static char 	    *outMovieFile     = NULL;       /* New movie file. */
static char 	    *userParamName    = NULL;       /* Optional user param. */ 
static char 	    *userParamString  = NULL;       /* Value of user param. */
static char	    *userParamType    = NULL;       /* movie, image, or audio */
static double 	    frameRate 	      = OPTION_NOT_SET;
static double 	    spatialQuality    = OPTION_NOT_SET;
static int 	    keyFrameRate      = OPTION_NOT_SET;
static DMimageorientation orientation = DM_IMAGE_BOTTOM_TO_TOP;
static DMimageinterlacing interlacing = DM_IMAGE_NONINTERLACED;
static compScheme   compressionScheme = qtvideo;
static MVloopmode   loopMode 	      = MV_LOOP_NONE;
static MVfileformat movieFormat       = MV_FORMAT_QT;
static int 	    frameWidth 	      = OPTION_NOT_SET;
static int 	    frameHeight       = OPTION_NOT_SET;
static namelist     *imageSrcFiles    = NULL;
static namelist     *audioSrcFiles    = NULL;
static namelist     *movieSrcFiles    = NULL;
static int 	    numFiles  	      = 0;

/*
 * Forward declarations of functions that appear below.
 */

static void 	 usage( void );

static void 	 setMovieFormat( char *formatArg );

static void 	 setLoopMode( char *strLoopMode );

static void 	 setInterlacing( char *strInterlacing );

static void 	 setCompressionScheme( char *compressArg );

static void 	 setUserParamType( char *userPType );

static void 	 putFileInList( char *fileName, int position );

static void 	 initNameList( namelist **theList );

static DMboolean badCompressionScheme( void );

static DMboolean isImageFile( char *fileName );

static DMboolean isAudioFile( char *fileName );

static namelist  *getFileListEnd( namelist *fileList );

static namelist  *getFileNames( srcFileType theType );

static void 	 appendNameList( char *fileName, int position,
		 		namelist *fileList );

static namelist  *getFileByTypeAndPos( srcFileType theType, int position );

static char 	 *getFirstArgStr( char *argList );

static char 	 *getStartOfArg( char *argList );

static int 	 getArgLength( char *argList );

static void 	 getArg( char *argList, char *arg );

/*********
 *
 * processCmdArgs - Process the command line arguments.
 *
 *********/

extern void processCmdArgs( int argc, char **argv )
{
    programName = argv[0];

    if ( argc < 4) {
        usage();
    }

    int ch;
    while ( ( ch = getopt( argc, argv, "f:c:l:r:q:k:s:p:o:i:t" ) ) != -1 ) {
        switch ( ch ) {
	    case 'f':
		setMovieFormat( optarg );
            	break;
            case 'c':
	 	setCompressionScheme( optarg );
            	break;
            case 'l':
	   	setLoopMode( optarg );
            	break;
            case 'r':
            	frameRate = atof( optarg );
            	break;
	    case 'q':
            	spatialQuality = atof( optarg );
            	break;
	    case 'k':
            	keyFrameRate = atoi( optarg );
            	break;
	    case 't':
            	orientation = DM_IMAGE_TOP_TO_BOTTOM;
            	break;
	    case 'i':
            	setInterlacing( optarg );
            	break;
            case 's':
	    	{
	            char *argList = optarg;
                    char *strFrameWidth;
    		    char *strFrameHeight;

		    strFrameWidth = getFirstArgStr( argList );
                    frameWidth = atoi( strFrameWidth );
	            free( strFrameWidth );

                    argList = getStartOfArg( argList );
                    strFrameHeight = getFirstArgStr( argList );
                    frameHeight = atoi( strFrameHeight );
                    free( strFrameHeight );
	    	}
            	break;
            case 'p':
	    	{
	            char *argList = optarg;
		    setUserParamType( getFirstArgStr( argList ) );
                    argList = getStartOfArg( argList );
		    userParamName = getFirstArgStr( argList );
                    argList = getStartOfArg( argList );
                    userParamString = getFirstArgStr( argList );
	    	}
            	break;
            case 'o':
	   	outMovieFile = getFirstArgStr( optarg );
            	break;
            case '?':
            	usage();
        }
    }

    /*
     * Check if the compression scheme selected is compatible with the
     * file format.
     */

    if ( badCompressionScheme() ) {
	fprintf( stderr, "%s: Compression %s unavailable for file format.\n",
		programName, getCompressionScheme() );
	exit( EXIT_FAILURE );
    }

    /*
     * Process the file names.
     */

    for( int i = optind; i < argc; i++ ) {
	putFileInList( argv[i], ( i - optind ) );
    }

    if ( !( haveSrcFileType( movieType ) )
        && !( haveSrcFileType( imageType ) ) ) {
        fprintf( stderr,
                "%s: At least one image file or movie file is required.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }
}

/*********
 *
 * Print usage message and exit the program.
 *
 *********/

static void usage( void )
{
    fprintf( stderr, "usage: %s ", programName);
    fprintf( stderr, "[-c compression] [-l loopMode] [ -r frameRate]\n" );
    fprintf( stderr, "       [-f format] [-s xsize,ysize] [-o outMovie] [-k keyFrameRate] [-t] \n" );
    fprintf( stderr, "       [-p paramType,userParam,userParamVal] file . . .\n" ); 
    fprintf( stderr, "\n");
    fprintf( stderr, "\"compression\" = none, mvc1, mvc2, rle, rle24, jpeg, 8rgb, " );
    fprintf( stderr, "qtvideo, qtanim, qtcvid, or indeo.\n" );
    fprintf( stderr, "The default compression scheme is qtvideo.\n" );
    fprintf( stderr, "\"format\"  = sgi, qt, or avi the format of the new movie. " );
    fprintf( stderr, "The default is qt.\n" );
    fprintf( stderr, "\"loopMode\" = once, loop, or swing. The default is once.\n");
    fprintf( stderr, "\"paramType\" = movie, image, or audio.\n");
    fprintf( stderr, "\"file\" may include one or more image, audio, and " );
    fprintf( stderr, " movie files.\n"); 
    exit( EXIT_FAILURE );
}

/*********
 *
 * Make lists of the input file names, according to whether they are image,
 * audio, or movie files.
 *
 *********/

static void putFileInList( char *fileName, int position )
{
    if ( mvIsMovieFile( fileName ) ) {
      	if ( movieSrcFiles == NULL ) {
	    initNameList( &movieSrcFiles );
        }
	appendNameList( fileName, position, movieSrcFiles );
    } 
    else if ( isImageFile( fileName ) ) {
      	if ( imageSrcFiles == NULL ) {
	    initNameList( &imageSrcFiles );
        }
	appendNameList( fileName, position, imageSrcFiles );
    }
    else if ( isAudioFile( fileName ) ) {
      	if ( audioSrcFiles == NULL ) {
	    initNameList( &audioSrcFiles );
        }
	appendNameList( fileName, position, audioSrcFiles );
    } 
    else {
	fprintf( stderr, "%s: %s is not a movie, image, or audio file.\n",
		programName, fileName);
	exit( EXIT_FAILURE );
    }
}

/*********
 *
 * Initialize a name list.
 *
 *********/

static void initNameList( namelist **theList )
{
    *theList = ( namelist *)malloc( sizeof( namelist ) );
    if ( theList == NULL ) {
	fprintf( stderr, "%s: Couldn't malloc new file name.\n", programName );
	exit( EXIT_FAILURE );
    }
    ( *theList )->name = NULL;
    ( *theList )->next = NULL;
}

/*********
 *
 * Find out if a file is an image file.
 *
 *********/

static DMboolean isImageFile( char *fileName )
{
    ilImage* image = ilOpenImgFile( fileName, "r" );
    if ( ( image == NULL ) || ( image->getStatus() != ilOKAY ) )
	return DM_FALSE;
    delete( image );
    return DM_TRUE;
}

/*********
 *
 * Find out if a file is an audio file.
 *
 *********/

static DMboolean isAudioFile( char *fileName )
{
    AFfilehandle audioFile = AFopenfile( fileName, "r", AF_NULL_FILESETUP );
    if ( audioFile == AF_NULL_FILEHANDLE)
	return DM_FALSE;
    AFclosefile( audioFile );
    return DM_TRUE;
}

/*********
 *
 * Find out if this file type has been entered.
 *
 *********/

DMboolean haveSrcFileType( srcFileType theType )
{
    switch( theType ) {
	case imageType:
	    if ( imageSrcFiles == NULL ) {
		return DM_FALSE;
	    }
	    break;
	case audioType:
	    if ( audioSrcFiles == NULL ) {
		return DM_FALSE;
	    }
	    break;
	case movieType:
	    if ( movieSrcFiles == NULL ) {
		return DM_FALSE;
	    }
	    break;
    }
    return DM_TRUE;
}

/*********
 *
 * Put the file name at the end of the list.
 *
 *********/

static void appendNameList( char *fileName, int position, namelist *fileList )
{
    namelist *current = getFileListEnd( fileList );

    if ( current->name == NULL ) {
        current->name = fileName;
	current->position = position;
    }
    else {
    	current->next = ( namelist *)malloc( sizeof( namelist ) );
	if ( current->next == NULL ) {
	    fprintf( stderr, "%s: Malloc failed.\n", programName );
	    exit( EXIT_FAILURE);
	}
    	current = current->next;
    	current->next = NULL;
    	current->name = fileName;
	current->position = position;
    }
    numFiles++;	
}

/*********
 *
 * Return a pointer to the last entry in the fileList.
 *
 *********/

static namelist *getFileListEnd( namelist *fileList )
{
    namelist *current;

    current = fileList;
    if ( current == NULL ) {
	return NULL;
    }
    while( current->next != NULL)
	current = current->next;
    return( current );
}
 

/*********
 *
 * Return a pointer to malloced memory containing the first argument
 * string in argList, which is a string with commas dividing the arguments.
 *
 *********/

static char *getFirstArgStr( char *argList )
{
    int argLength = getArgLength( argList );
    char *argString = ( char * )calloc( argLength + 1, sizeof( char ) );
    getArg( argList, argString );
    return( argString );
}

/*********
 *
 * Return a pointer to the start of the next argument, defined as the position
 * of the next argument delimiter, a comma, plus one. If there are no more
 * argument delimiters, return NULL.
 *
 *********/

static char *getStartOfArg( char *argList )
{
    char *argMarker = ( ( argList == NULL ) ? argList : strchr( argList, ',' ) );

    if ( argMarker == NULL )
        return( NULL );
    argMarker++;
    return( argMarker );
}

/*********
 *
 * Return the length of the first argument in argList.
 *
 *********/

static int getArgLength( char *argList )
{
    char *endArg = getStartOfArg( argList );

    /*
     * If argList points to the last argument, call strlen to find the length,
     * otherwise, calculate it.
     */

    if ( endArg == NULL )
        return( ( ( argList == NULL ) ? 0 : strlen( argList ) ) );
    return( endArg - argList - 1);
}

/*********
 *
 * Copy the first argument in argList to arg.
 *
 *********/

static void getArg( char *argList, char *arg )
{
    int argLength = getArgLength ( argList );
    strncpy( arg, argList, argLength );
    arg[ argLength ] = '\0';
}

/*********
 *
 * Set the value of movieFormat.
 *
 *********/

static void setMovieFormat( char *formatArg )
{
    if ( ( strcmp( formatArg, "sgi" ) == 0 ) || 
	( strcmp( formatArg, "SGI" ) == 0 ) ) {
	movieFormat = MV_FORMAT_SGI_3;
    }
    else if ( ( strcmp( formatArg, "qt" ) == 0 ) ||
	     ( strcmp( formatArg, "QT" ) == 0 ) ) {
	movieFormat = MV_FORMAT_QT;
    }
    else if ( ( strcmp( formatArg, "avi" ) == 0 ) ||
	      ( strcmp( formatArg, "AVI" ) == 0 ) ) {
	movieFormat = MV_FORMAT_AVI;
    }
    else {
        fprintf( stderr, "%s: Unknown movie format %s.\n",
		programName, formatArg );
	usage();
    }
}

/*********
 *
 * Return the value of movieFormat.
 *
 *********/

MVfileformat getMovieFormat( void )
{
    return movieFormat;
}

/*********
 *
 * Check to see if the compression scheme is compatible with the file format.
 *
 *********/

static DMboolean badCompressionScheme( void )
{
    if ( ( getMovieFormat() == MV_FORMAT_QT ) &&
	( ( compressionScheme != none ) && 
	 ( compressionScheme != qtanim ) &&
	  ( compressionScheme != qtcvid ) &&
	  ( compressionScheme != indeo ) &&
	 ( compressionScheme != jpeg ) && 
	 ( compressionScheme != qtvideo ) ) ) {
	return( DM_TRUE );
    }
    if ( ( getMovieFormat() == MV_FORMAT_AVI ) &&
	 ( ( compressionScheme != none ) && 
	   ( compressionScheme != qtcvid ) &&
	   ( compressionScheme != jpeg ) &&
	   ( compressionScheme != indeo ) ) ){
	return ( DM_TRUE );
    }
    
    return( DM_FALSE );
}

/*********
 *
 * Record the requested loopMode.
 *
 *********/

static void setLoopMode( char *strLoopMode )
{
 
    if ( strcmp( strLoopMode, "once" ) == 0 ) {
	loopMode = MV_LOOP_NONE;
    }
    if ( strcmp( strLoopMode, "loop" ) == 0 ) {
	loopMode = MV_LOOP_CONTINUOUSLY;
    }
    if ( strcmp( strLoopMode, "swing" ) == 0 ) {
	loopMode = MV_LOOP_SWINGING;
    }
}

/*********
 *
 * Record the requested interlacing.
 *
 *********/

static void setInterlacing( char *strInterlacing )
{
 
    if ( strcmp( strInterlacing, "none" ) == 0 ) {
	interlacing = DM_IMAGE_NONINTERLACED;
    }
    if ( strcmp( strInterlacing, "odd" ) == 0 ) {
	interlacing = DM_IMAGE_INTERLACED_ODD;
    }
    if ( strcmp( strInterlacing, "even" ) == 0 ) {
	interlacing = DM_IMAGE_INTERLACED_EVEN;
    }
}

/*********
 *
 * Return the value of loopMode.
 *
 *********/

MVloopmode getLoopMode()
{
    return loopMode;
}

/*********
 *
 * Record the requested compression scheme.
 *
 *********/

static void setCompressionScheme( char *compressArg )
{
    if ( strcmp( compressArg, "none" ) == 0 ) {
	compressionScheme = none;
    }
    else if ( strcmp( compressArg, "mvc1" ) == 0 ) {
	compressionScheme = mvc1;
    }
    else if ( strcmp( compressArg, "mvc2" ) == 0 ) {
	compressionScheme = mvc2;
    }
    else if ( strcmp( compressArg, "rle" ) == 0 ) {
	compressionScheme = rle;
    }
    else if ( strcmp( compressArg, "rle24" ) == 0 ) {
	compressionScheme = rle24;
    }
    else if ( strcmp( compressArg, "jpeg" ) == 0 ) {
	compressionScheme = jpeg;
    }
    else if ( strcmp( compressArg, "8rgb" ) == 0 ) {
	compressionScheme = rgb8;
    }
    else if ( strcmp( compressArg, "qtvideo" ) == 0 ) {
	compressionScheme = qtvideo;
    }
    else if ( strcmp( compressArg, "qtanim" ) == 0 ) {
	compressionScheme = qtanim;
    }
    else if ( strcmp( compressArg, "qtcvid" ) == 0 ) {
	compressionScheme = qtcvid;
    }
    else if ( strcmp( compressArg, "indeo" ) == 0 ) {
	compressionScheme = indeo;
    }
    else {
        fprintf( stderr, "%s: Unknown compression scheme %s.\n",
		programName, compressArg );
	usage();
    }
}

/*********
 *
 * Return the string corresponding to the compression scheme.
 *
 *********/

char *getCompressionScheme( void )
{
    switch( compressionScheme ) {
	case none:
	    return( DM_IMAGE_UNCOMPRESSED );
	case mvc1:
	    return( DM_IMAGE_MVC1 );
	case mvc2:
	    return( DM_IMAGE_MVC2 );
	case jpeg:
	    return( DM_IMAGE_JPEG );
	case rle:
	    return( DM_IMAGE_RLE );
	case rle24:
	    return( DM_IMAGE_RLE24 );
	case rgb8:
	    return( DM_IMAGE_UNCOMPRESSED );
	case qtvideo:
	    return( DM_IMAGE_QT_VIDEO );
	case qtanim:
	    return( DM_IMAGE_QT_ANIM );
	case qtcvid:
	    return( DM_IMAGE_QT_CVID );
	case indeo:
	    return( DM_IMAGE_INDEO );
	case unknownComp:
	    assert( DM_FALSE );
	    break;
    }
}

/*********
 *
 * Return the frame width set on the command line.
 *
 *********/

int getFrameWidth( void )
{
    return frameWidth;
}

/*********
 *
 * Return the frame height set on the command line.
 *
 *********/

int getFrameHeight( void )
{
    return frameHeight;
}

/*********
 *
 * Return the frame rate set on the command line.
 *
 *********/

double getFrameRate( void )
{
    return frameRate;
}

/*********
 *
 * Return the quality set on the command line.
 *
 *********/

double getSpatialQuality( void )
{
    return spatialQuality;
}

/*********
 *
 * Return the key frame rate set on the command line.
 *
 *********/

int getKeyFrameRate( void )
{
    return keyFrameRate;
}

/*********
 *
 * Return the orientation set on the command line
 *
 *********/

DMimageorientation getOrientation( void )
{
    return orientation;
}

/*********
 *
 * Return the interlacing set on the command line
 *
 *********/

DMimageinterlacing getInterlacing( void )
{
    return interlacing;
}

/*********
 *
 * Return the name of the output file set on the command line.
 *
 *********/

char *getOutMovieName( void )
{
    return outMovieFile;
}

/*********
 *
 * Return the name of the program.
 *
 *********/

char *getProgramName( void )
{
    return programName;
}

/*********
 *
 * Set the type of the user param.
 *
 *********/

static void setUserParamType( char *userPType )
{
    if ( ( strcmp( userPType, "movie" ) != 0 ) &&
	( strcmp ( userPType, "image" ) != 0 ) &&
   	( strcmp ( userPType, "audio" ) != 0 ) ) {
	fprintf( stderr, "%s: User param must be type movie, image, or audio.\n",
		getProgramName() );
	exit( EXIT_FAILURE );
    }
    userParamType = userPType;
}

/*********
 *
 * Return the type of the user param.
 *
 *********/

char *getUserParamType( void )
{
    return userParamType;
}

/*********
 *
 * Return the name of the user param.
 *
 *********/

char *getUserParamName( void )
{
    return userParamName;
}

/*********
 *
 * Return the value of the user param.
 *
 *********/

char *getUserParamValue( void )
{
    return userParamString;
}

/*********
 *
 * Return the number of image and movie files.
 *
 *********/

int getNumFiles( void )
{
    return numFiles;
}

/*********
 *
 * Get a pointer to the namelist entry of the specified position, returning
 * the file type in theType.
 *
 *********/

void getFileNameByPosition( int pos, namelist **theFile, srcFileType *theType )
{
    namelist *current;

    if ( ( current = getFileByTypeAndPos( imageType, pos) ) != NULL ) {
	*theType = imageType;
	*theFile = current;
    } 
    else if ( ( current = getFileByTypeAndPos( movieType, pos) ) != NULL ) {
	*theType = movieType;
	*theFile = current;
    } 
    else if ( ( current = getFileByTypeAndPos( audioType, pos) ) != NULL ) {
	*theType = audioType;
	*theFile = current;
    } 

}

/*********
 *
 * Return a pointer to the namelist entry of the specified type and position.
 * Return a NULL pointer if not found.
 *
 *********/

static namelist *getFileByTypeAndPos( srcFileType nameType, int position )
{
    namelist *current = getFileNames( nameType );

    while( current != NULL ) {
	if ( current->position == position ) {
	    return current;
	}
	current = current->next;
    }
    return NULL;
}

/*********
 *
 * Return a pointer to the head of the namelist of the specified type.
 *
 *********/

static namelist *getFileNames( srcFileType nameType )
{
    switch( nameType ) {
	case movieType:
	    return( movieSrcFiles );
	case imageType:
	    return( imageSrcFiles );
	case audioType:
	    return( audioSrcFiles );
    }
}

/*********
 *
 * Return the name of the first file of the specified type.
 *
 *********/

char *getFirstFileName( srcFileType theType )
{
    switch( theType ) {
	case movieType:
	    return( movieSrcFiles->name );
	case imageType:
	    return( imageSrcFiles->name );
	case audioType:
	    return( audioSrcFiles->name );
    }
}

/*********
 *
 * Get the position of the file as it was entered on the command line.
 *
 *********/

int getFilePosition( char *theFile )
{
    namelist    *current;
    DMboolean   notFound = DM_TRUE; 
    int		searchType = ( int )movieType;

    while( notFound && ( searchType <= ( int )audioType ) ) {
	current = getFileNames( ( srcFileType )searchType );

	while( ( current != NULL ) 
	      && ( strcmp( current->name, theFile ) != 0 ) ) {
	    current = current->next;
	}
	if ( current == NULL ) {
	    searchType++;
	}
	else {
	    notFound = DM_FALSE;
	}
    }
    assert( searchType <=  ( int )audioType );

    return current->position;
}

/*********
 *
 * Get the file name and type according to its position as entered.
 *
 *********/

void getFileAndTypeByPos( int pos, char **theFile, srcFileType *theType )
{

    namelist    *current;
    DMboolean   notFound = DM_TRUE; 
    int		searchType = ( int )movieType;
    while( notFound && ( searchType <= ( int )audioType ) ) {
	current = getFileByTypeAndPos( ( srcFileType )searchType, pos );
        if ( current == NULL ) {
            searchType++;
	}
	else {
	    notFound = DM_FALSE;
	}
    }
    assert( searchType <= ( int )audioType );

    *theFile = current->name;
    *theType = ( srcFileType )searchType;
}                                     

/*********
 *
 * Get the name of the first movie with audio.
 *
 *********/

char *getFirstAudioMovie( void )
{
    namelist *current = getFileNames( movieType );

    MVid theMovie;
    MVid audioTrack;
    while( current != NULL ) {

        if ( mvOpenFile( current->name, O_RDONLY, &theMovie )
            == DM_FAILURE ) {
            fprintf( stderr, "%s: Unable to open movie file %s.\n",
                    getProgramName(), current->name );
            fprintf( stderr,"    error = %s.\n",
                    mvGetErrorStr( mvGetErrno() ) );
            exit( EXIT_FAILURE );
        }

        if ( mvFindTrackByMedium( theMovie, DM_AUDIO, &audioTrack )
            == DM_SUCCESS ) {
	    mvClose( theMovie );
            return current->name;
        }
	mvClose( theMovie );
	current = current->next;
    }
    return NULL;
}

/*********
 *
 * Find out if frameWidth and frameHeight were set from the command line.
 *
 *********/

DMboolean haveWidthAndHeight( void )
{
    if ( ( frameWidth == OPTION_NOT_SET )
	|| ( frameHeight == OPTION_NOT_SET ) ) {

	return DM_FALSE;
    }
    return DM_TRUE;
}
