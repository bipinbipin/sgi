#ifndef handy_h
#define handy_h

/*	#define DEBUG	*/

#ifdef DEBUG
#define DEBUGONLY(x) x
#else
#define DEBUGONLY(x)
#endif 


/**********************************************************************
*
* Handy error macros
*
**********************************************************************/

#ifdef __ANSI_CPP__
#define PRINT_CALL(call)   fprintf( stderr, "%s failed in file %s, line %d\n", # call, __FILE__, __LINE__ );
#else
#define PRINT_CALL(call)   fprintf( stderr, "%s failed in file %s, line %d\n", "call", __FILE__, __LINE__ );
#endif

#define ERROR(message)\
        {\
            fprintf( stderr, "%s\n", message );\
            fprintf( stderr, "in file %s, line %d\n", __FILE__, __LINE__);\
	    exit(EXIT_FAILURE);\
	}

/*
** CC - "Compression Check".  Wraps a CL call that returns <0 on error.
*/

#define CC(call)							      \
    {									      \
	if ( (call) < 0 ) {						      \
	    PRINT_CALL(call);						      \
            exit( EXIT_FAILURE );					      \
	}								      \
    }									      \

/*
** CNC -- "Compression NULL Check".  Wraps a CL call that returns NULL
** on error.
*/

#define CNC(call)							      \
    {									      \
	if ( (call) == NULL ) {						      \
	    PRINT_CALL(call);						      \
            exit( EXIT_FAILURE );					      \
	}								      \
    }									      \

/*
** VC - "Video Check".  Wraps a VL call that returns <0 on error.
*/

#define VC(call)							      \
    {									      \
	if ( (call) < 0 ) {						      \
	    fprintf( stderr, "%s\n", vlStrError(vlGetErrno()) );\
	    PRINT_CALL(call);						      \
            exit( EXIT_FAILURE );					      \
	}								      \
    }									      \

/*
** VNC -- "Video NULL Check".  Wraps a VL call that returns NULL
** on error.
*/

#define VNC(call)							      \
    {									      \
	if ( (call) == NULL ) {						      \
	    fprintf( stderr, "%s\n", vlStrError(vlGetErrno()) );\
	    PRINT_CALL(call);						      \
            exit( EXIT_FAILURE );					      \
	}								      \
    }									      \

/*
** AC - "Audio Check".  Wraps an AL call that returns <0 on error.
*/

#define AC(call)							      \
    {									      \
	if ( (call) < 0 ) {						      \
	    PRINT_CALL(call);						      \
            exit( EXIT_FAILURE );					      \
	}								      \
    }									      \

/*
** ANC -- "Audio NULL Check".  Wraps an AL call that returns NULL
** on error.
*/

#define ANC(call)							      \
    {									      \
	if ( (call) == NULL ) {						      \
	    PRINT_CALL(call);						      \
            exit( EXIT_FAILURE );					      \
	}								      \
    }									      \

/*
** DC -- "DM Params Check".  Wraps a call to a dmParams function that
** returns DM_FAILURE on error.
*/

#define DC(call)							      \
    {									      \
	if ( (call) != DM_SUCCESS ) {					      \
            char   detail[DM_MAX_ERROR_DETAIL], *s;\
            int    err;\
	    PRINT_CALL(call);						      \
            s = dmGetError(&err, detail);\
            fprintf( stderr, "DM error %s: %s\n", s, detail);\
            exit( EXIT_FAILURE );					      \
	}								      \
    }									      \

/*
** MC -- "Movie Check".  Wraps a movie library call that returns
** DM_FAILURE on error.
*/

#define MC(call)							      \
    {									      \
	if ( (call) != DM_SUCCESS ) {					      \
	    fprintf( stderr, "%s\n", mvGetErrorStr(mvGetErrno()) );\
	    PRINT_CALL(call);						      \
            exit( EXIT_FAILURE );					      \
	}								      \
    }									      \

/*
** OC -- "OS Check".  Wraps a Unix call that returns negative on error.
*/

#define OC(call)							      \
    {									      \
	if ( (call) < 0 ) {						      \
	    perror( "" );					      \
	    PRINT_CALL(call);						      \
            exit( EXIT_FAILURE );					      \
	}								      \
    }									      \

/*
** OCW -- "OS Check Warn".  	Like OC, but prints warning and continues 
**				instead of exiting.
*/

#define OCW(call)							      \
    {									      \
	if ( (call) < 0 ) {						      \
	    fprintf( stderr, " \nWarning: " );				      \
	    perror( "" );					      \
	    PRINT_CALL(call);						      \
	}								      \
    }									      \

#define EVEN(foo) 					\
	( ( (foo/2)*2 == foo )? 1 : 0 )


#endif
