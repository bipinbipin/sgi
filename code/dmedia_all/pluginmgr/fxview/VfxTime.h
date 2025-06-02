////////////////////////////////////////////////////////////////////////
//
// VfxTime.h
//	
//	A simple time object.
//
// Copyright 1995, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
//
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
//
////////////////////////////////////////////////////////////////////////

#ifndef _VfxTime_H
#define _VfxTime_H

#include <X11/Intrinsic.h>	// Boolean
#include <inttypes.h>
#include <dmedia/moviefile.h>	// MVtime, MVtimescale
typedef int64_t VfxTimeType;

#define BILLION ((VfxTimeType) 1000000000)

////////////////////////////////////////////////////////////////////////
//
// VfxTime class
//
// This is the basic representation of time in fxbuilder.  It stores
// time as an integral number of nanoseconds, and can convert to and
// from a number of different formats:
//
//	- floating point seconds
//      - nanoseconds (VfxTimeType)
//	- timecode
//
////////////////////////////////////////////////////////////////////////

class VfxTime
{
public:
    //
    // Constructors
    //

    VfxTime() 
    {
	nanoseconds = 0;
    }

    static VfxTime fromNanoseconds( VfxTimeType nanos ) 
    {
	return VfxTime( nanos );
    }
    
    static VfxTime fromSeconds(
	double seconds
	)
    {
	return VfxTime( VfxTimeType( double(BILLION) * seconds ) );
    }
    
    static VfxTime fromSecondsFraction( 
	long int numerator,
	long int denominator
	)
    {
	return VfxTime::fromSeconds( double(numerator) / double(denominator) );
    }
    
    static VfxTime fromTimecodeString( const char*, int tcType, Boolean* returnOK );
    
    static VfxTime fromMovieTime( MVtime time, MVtimescale timeScale );

    //
    // Inventor can't store 64-bit integers, so they are converted to
    // and from pairs of 32-bit integers.
    //
    static VfxTime fromUInt32( uint32_t low, uint32_t high )
    {
	int64_t  nanos = (int64_t) ( ( ((uint64_t)high) << 32 ) |
				     ((uint64_t)low) );
	return VfxTime::fromNanoseconds( nanos );
    }
    

    //
    // From a regular string like "1:2" into "00:00:01:02"
    //
    static char *expandTimecodeString(const char *str);

    //
    // Conversions to other types
    //

    double toSeconds()
    {
	return double(nanoseconds) / double(BILLION);
    }
    
    VfxTimeType toNanoseconds()
    {
	return nanoseconds;
    }
    
    //
    // Inventor can't store 64-bit integers, so they are converted to
    // and from pairs of 32-bit integers.
    //
    uint32_t toUInt32Low()
    {
	return (uint32_t) ( nanoseconds & 0xFFFFFFFF );
    }
    uint32_t toUInt32High()
    {
	return (uint32_t) ( ( nanoseconds >> 32 ) & 0xFFFFFFFF );
    }
    

    // toTimecodeString returns a pointer to a static buffer.  the
    // next call to it will overwrite the buffer.  The tcTypes are
    // defined in dmedia/dm_timecode.h.

    const char* toTimecodeString( int tcType );

    int toFrameNumber( double frameRate ) const;
    
    VfxTime frameBack( int tcType, VfxTimeType startTime );
    VfxTime frameForward( int tcType, VfxTimeType endTime );
    

    //
    // now -- returns the current UST.
    //

    static VfxTime now();

    //
    // Arithmetic
    //

    VfxTime operator + ( const VfxTime& other ) const
    {
	return VfxTime( nanoseconds + other.nanoseconds );
    }
    
    VfxTime operator - ( const VfxTime& other ) const
    {
	return VfxTime( nanoseconds - other.nanoseconds );
    }
    
    //
    // Comparisons
    //

    Boolean operator == ( const VfxTime& other ) const
    {
	return nanoseconds == other.nanoseconds;
    }

    Boolean operator != ( const VfxTime& other ) const
    {
	return nanoseconds != other.nanoseconds;
    }

    Boolean operator > ( const VfxTime& other ) const
    {
	return nanoseconds > other.nanoseconds;
    }

    Boolean operator >= ( const VfxTime& other ) const
    {
	return nanoseconds >= other.nanoseconds;
    }
    
    Boolean operator < ( const VfxTime& other ) const
    {
	return nanoseconds < other.nanoseconds;
    }
    
    Boolean operator <= ( const VfxTime& other ) const
    {
	return nanoseconds <= other.nanoseconds;
    }
    
    
private:
    // This constructor is private to avoid inadvertand conversion
    // from float.
    VfxTime( VfxTimeType nanos )
    {
	nanoseconds = nanos;
    }
    
    VfxTimeType nanoseconds;
};

#endif // _VfxTime_H
