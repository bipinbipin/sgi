//
// debug.c - debug help file
//

#include <stdio.h>
#include <stdarg.h>
#include <strings.h>
#include "debug.h"

//
// globals
//

_DEBUGFINFO		_DebugfInfo = { 1, "", "", -1 };

//
// _Debugf - print debugging information in a useful way
//

void _Debugf( char *pszFormat, ... )
{
	va_list		args;
	static char	szPrev[256];
	char		szBuff[10240];
	char		*p = szBuff;

	sprintf( p, "%s!%s(%d): ", _DebugfInfo.pszLib, _DebugfInfo.pszFile, _DebugfInfo.nLine );
	if (strcmp( szBuff, szPrev ) == 0)
	{
		while (*p) *p++ = ' ';
	}
	else
	{
		strcpy( szPrev, szBuff );
		p += strlen( p );
	}
	va_start( args, pszFormat );
	vsprintf( p, pszFormat, args );
	va_end( args );
	p += strlen( p );
	if (p[-1] != '\n') strcpy( p, "\n" );
	fputs( szBuff, stderr );
}
