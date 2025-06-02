
//
// debug.h - debug header
//

#if !defined( DEBUG_H )
#define DEBUG_H

#if !defined( TRUE )
typedef enum {
    FALSE = 0,
    TRUE = 1
} BOOL;
#endif

typedef struct
{
	char			bEnabled;
	char  			*pszLib;
	char			*pszFile;
	int			nLine;
} _DEBUGFINFO;

#define EnableDebugf(b) (_DebugInfo.bEnabled = (b))

#if defined(LIB_NAME)
	#define Debugf \
		if (_DebugfInfo.bEnabled) \
			_DebugfInfo.pszLib = LIB_NAME, \
			_DebugfInfo.pszFile = __FILE__, \
			_DebugfInfo.nLine = __LINE__, \
			_Debugf /* ...followed by args */
#else
	#define Debugf \
		if (_DebugfInfo.bEnabled) \
			_DebugfInfo.pszLib = "?", \
			_DebugfInfo.pszFile = __FILE__, \
			_DebugfInfo.nLine = __LINE__, \
			_Debugf /* ...followed by args */
#endif


//
// globals
//

extern _DEBUGFINFO		_DebugfInfo;
void _Debugf( char *pszFormat, ... );

#endif // DEBUG_H
