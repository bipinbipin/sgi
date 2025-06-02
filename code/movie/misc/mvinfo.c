/*****************************************************************************
 *
 * File:         mvinfo.c
 *
 * Usage:        mvinfo <moviefile>
 *
 * Description:  Display the parameters of a movie file.
 *
 * Functions:    SGI Digital Media Library functions used:
 *
 *               dmParamsGetNumElems()
 *               dmParamsGetElem()  
 *               dmParamsGetElemType()
 *               dmParamsGetEnum()
 *               dmParamsGetInt()
 *               dmParamsGetFloat()
 *               dmParamsGetFract()
 *               dmParamsGetString() 
 *               dmParamsGetParams()
 *
 *               SGI Movie Library functions used:
 *
 *               mvOpenFile()
 *               mvGetFileFormat()
 *               mvGetParams() 
 *               mvFindTrackByMedium()
 *               mvClose()
 * 
 *****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <dmedia/moviefile.h>

/*
 *
 * Forward declarations for functions used in main() and defined later.
 *
 */

static void PrintFormat( MVfileformat format, char *fileName );
static void PrintParams( const DMparams* params, int indent );
static void PrintString( const char* parameterName, int value, char* string );

/*
 *
 * Data structures used in printing movie descriptions.
 *
 */

struct PrintString {
    const char* paramName;
    int         value;
    const char* printString;
};

struct PrintString PrintTable[] =
{
    { MV_FILE_FORMAT,   MV_FORMAT_SGI_1,        "SGI1"                  },
    { MV_FILE_FORMAT,   MV_FORMAT_SGI_2,        "SGI2"                  },
    { MV_FILE_FORMAT,   MV_FORMAT_SGI_3,        "SGI3"                  },
    { MV_FILE_FORMAT,   MV_FORMAT_QT,           "QuickTime"             },
    { MV_FILE_FORMAT,   MV_FORMAT_OMFI,         "OMFI"                  },
    { MV_FILE_FORMAT,   MV_FORMAT_AVI,          "AVI"                   },
    { MV_FILE_FORMAT,   MV_FORMAT_MPEG1,        "MPEG1"                 },

    { MV_LOOP_MODE,     MV_LOOP_NONE,           "Play Once"             },
    { MV_LOOP_MODE,     MV_LOOP_CONTINUOUSLY,   "Loop"                  },
    { MV_LOOP_MODE,     MV_LOOP_SWINGING,       "Swing"                 },
  
    { DM_MEDIUM,        DM_IMAGE,               "Image"                 },
    { DM_MEDIUM,        DM_AUDIO,               "Audio"                 },

    { DM_IMAGE_INTERLACING,     DM_IMAGE_NONINTERLACED, "Non-interlaced"},
    { DM_IMAGE_INTERLACING,     DM_IMAGE_INTERLACED_EVEN,"Interlaced Even" },
    { DM_IMAGE_INTERLACING,     DM_IMAGE_INTERLACED_ODD, "Interlaced Odd"  },

    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_BGR, "BGR"	},
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_XBGR, "XBGR"	},
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_ABGR, "ABGR"	},
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_RBG323, "8-bit RBG (Indigo)"	},
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_BGR233, "8-bit BGR (Indy)"	},
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_LUMINANCE, "Grayscale"	},
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_RGB, "RGB"	},
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_RGBA, "RGBA"	},
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_RGBX, "RGBX"	},
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_ARGB, "ARGB (Apple)"	},
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_XRGB, "RGBA (Apple)"	},
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_XRGB1555, "16-bit BGR (Apple16)" },
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_CbYCr, "YCbCr 4:4:4"	},
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_CbYCrY, "YCbCr 4:2:2 (CCIR 601)"},
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_CbYCrA, "YCbCrA 4:4:4:4"	},
    { DM_IMAGE_PACKING, DM_IMAGE_PACKING_CbYCrYYY, "YCbCr 4:2:0 (MPEG)"	},

    { DM_IMAGE_ORIENTATION, DM_TOP_TO_BOTTOM,   "Top-to-bottom" },
    { DM_IMAGE_ORIENTATION, DM_BOTTOM_TO_TOP,   "Bottom-to-top" },

    { DM_AUDIO_FORMAT,  DM_AUDIO_TWOS_COMPLEMENT, "Twos-complement" },
    { DM_AUDIO_FORMAT,  DM_AUDIO_UNSIGNED,      "Unsigned" },
    { DM_AUDIO_FORMAT,  DM_AUDIO_FLOAT,         "Float" },
    { DM_AUDIO_FORMAT,  DM_AUDIO_DOUBLE,        "Double" },

    { NULL,             0,                      NULL                    }
};

/**********************************************************************
*
* main
*
**********************************************************************/

main ( int argc, char** argv ) 
{
    MVid         theMovie;       /* The movie instance */
    MVid         track;	 	 /* One of the tracks in the movie */
    char	 *programName;   /* Name of the program */
    char	 *fileName;	 /* Name of the movie file */

    /*
     * Make sure that we have a file name, and that it is a movie file.
     */

    programName = argv[0];

    if ( argc < 2 ) {
	fprintf( stderr, "usage: %s moviefile\n", programName );
	exit( EXIT_FAILURE ); 
    }

    fileName = argv[1];

    if ( !( mvIsMovieFile( fileName ) ) ) {
	fprintf( stderr, "%s: %s is not a movie file.\n",
		programName, fileName );
	exit( EXIT_FAILURE ); 
    }

    /*
     * Open the movie.
     */
    
    if ( mvOpenFile( fileName, O_RDONLY, &theMovie ) != DM_SUCCESS ) { 
	fprintf( stderr, "%s: Could not open movie file %s.\n",
		programName, fileName );
	exit( EXIT_FAILURE );
    }
    
    /*
     * Print the format of the movie.
     */

    PrintFormat( mvGetFileFormat( theMovie ), fileName );
    
    /*
     * Print the parameters for the movie as a whole.
     */
	
    printf( "\nMovie Header Parameters:\n\n" );
    PrintParams( mvGetParams( theMovie ), 4 );
    
    /*
     * Print the parameters for the image track.
     */
    
    if ( mvFindTrackByMedium( theMovie, DM_IMAGE, &track ) == DM_SUCCESS ) {
	printf( "\nImage Track Parameters:\n\n" );
	PrintParams( mvGetParams( track ), 4 );
    }
    else {
	fprintf( stderr, "%s: Couldn't find image track, file %s.\n",
		programName, fileName );
	exit( EXIT_FAILURE );
    }
    
    /*
     * Print the parameters for the audio track.
     */
    
    if ( mvFindTrackByMedium( theMovie, DM_AUDIO, &track ) == DM_SUCCESS ) {
	printf( "\nAudio Track Parameters:\n\n" );
	PrintParams( mvGetParams( track ), 4 );
    }
    
    /*
     * Close the movie and the file.
     */
    
    mvClose( theMovie );
    exit( EXIT_SUCCESS );
}

/********
*
* PrintFormat
*
********/

void PrintFormat( MVfileformat format, char *fileName )
{

    switch ( format ) 
	{
	case MV_FORMAT_QT:
	    printf( "\n%s is a QuickTime file\n", fileName );
	    break;
	case MV_FORMAT_SGI_1:
	    printf( "\n%s is a version 1 SGI movie file.\n", fileName );
	    break;
	case MV_FORMAT_SGI_2:
	    printf( "\n%s is a version 2 SGI movie file.\n", fileName );
	    break;
	case MV_FORMAT_SGI_3:
	    printf( "\n%s is an SGI movie file.\n", fileName );
	    break;
	case MV_FORMAT_OMFI:
	    printf( "\n%s is an OMFI movie file.\n", fileName );
	    break;
	case MV_FORMAT_AVI:
	    printf( "\n%s is an AVI movie file.\n", fileName );
	    break;
	case MV_FORMAT_MPEG1:
	    printf( "\n%s is an MPEG1 movie file.\n", fileName );
	    break;
	default:
	    printf( "\n%s is an unknown type of movie file.\n", fileName );
	    break;
	}
}    

/********
*
* PrintParams
*
********/

static void PrintParams( const DMparams* params, int indent )
{
    int len = dmParamsGetNumElems( params );
    int i;
    int j;
    
    for ( i = 0;  i < len;  i++ ) {
	const char* name = dmParamsGetElem    ( params, i );
	DMparamtype type = dmParamsGetElemType( params, i );
	
	for ( j = 0;  j < indent;  j++ ) {
	    printf( " " );
	}

	printf( "%25s: ", name );
	switch( type ) 
	    {
	    case DM_TYPE_ENUM:
                {
                    char buffer[64];
                    PrintString( name,
                                 dmParamsGetEnum( params, name ),
                                 buffer );
                    printf( "%s", buffer );
                }
		break;
	    case DM_TYPE_INT:
		printf( "%d", dmParamsGetInt( params, name ) );
		break;
	    case DM_TYPE_STRING:
		printf( "%s", dmParamsGetString( params, name ) );
		break;
	    case DM_TYPE_FLOAT:
		printf( "%f", dmParamsGetFloat( params, name ) );
		break;
	    case DM_TYPE_FRACTION:
		{
		    DMfraction f;
		    f = dmParamsGetFract( params, name );
		    printf( "%d/%d", f.numerator, f.denominator );
		}
		break;
	    case DM_TYPE_PARAMS:
		PrintParams( dmParamsGetParams( params, name ), indent + 4 );
		break;
	    default:
		assert( DM_FALSE );
	    }
	printf( "\n" );
    }
}

/********
*
* PrintString
*
********/

static void PrintString( const char* parameterName, int value, char* string )
{
    /*
     * If there is a string in the table that matches this value,
     * return it.
     */

    int i;

    for ( i = 0;  PrintTable[i].paramName != NULL;  i++ ) {
        if ( ( strcmp( PrintTable[i].paramName, parameterName ) == 0 ) &&
              ( PrintTable[i].value == value ) ) {
            strcpy( string, PrintTable[i].printString );
            return;
        }
    }

    /*
     * No matching string, just return the data that we were given.
     */

    sprintf( string, "%d", value );
}

