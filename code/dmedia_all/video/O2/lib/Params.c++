////////////////////////////////////////////////////////////////////////
//
// Params.c++
//	
// Copyright 1996, Silicon Graphics, Inc.
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

#include "VideoKit.h"

////////
//
// enumName
//
////////

struct enumEntry
{
    const char*	param;
    int		value;
    const char*	name;
};

#define EE(param,value)    { param, value, # value }

static enumEntry enumNames[] = 
{
    EE( DM_POOL_SHARE, DM_POOL_INTERPROCESS ),

    EE( DM_POOL_CACHEABLE, DM_TRUE ),
    EE( DM_POOL_CACHEABLE, DM_FALSE ),

    EE( DM_POOL_MAPPED, DM_TRUE ),
    EE( DM_POOL_MAPPED, DM_FALSE ),

    EE( DM_POOL_NO_ECC, DM_TRUE ),
    EE( DM_POOL_NO_ECC, DM_FALSE ),

    EE( DM_BUFFER_TYPE,	DM_BUFFER_TYPE_PAGE ),
    EE( DM_BUFFER_TYPE,	DM_BUFFER_TYPE_TILE ),
};

#define ARRAY_SIZE(a)  ( sizeof(a) / sizeof(a[0]) )

const char* enumName(
    const char* param,
    int value
    )
{
    for ( int i = 0;  i < ARRAY_SIZE(enumNames);  i++ )
    {
	if ( value == enumNames[i].value &&
	     strcmp( param, enumNames[i].param ) == 0 )
	{
	    return enumNames[i].name;
	}
    }
    
    static char number[100];
    sprintf( number, "??? - %d", value );
    return number;
}
  
////////
//
// PrintParams
//
////////

void PrintParams( const DMparams* p, int indent )
{
    int len = dmParamsGetNumElems( p );
    int i;
    int j;
    
    for ( i = 0;  i < len;  i++ ) {
	const char* name = dmParamsGetElem    ( p, i );
	DMparamtype type = dmParamsGetElemType( p, i );
	
	for ( j = 0;  j < indent;  j++ ) {
	    printf( " " );
	}

	printf( "%8s: ", name );
	switch( type ) 
	    {
	    case DM_TYPE_ENUM:
		printf( "%s", enumName( name, dmParamsGetEnum( p, name ) ) );
		break;
	    case DM_TYPE_INT:
		printf( "%d", dmParamsGetInt( p, name ) );
		break;
	    case DM_TYPE_STRING:
		printf( "%s", dmParamsGetString( p, name ) );
		break;
	    case DM_TYPE_FLOAT:
		printf( "%f", dmParamsGetFloat( p, name ) );
		break;
	    case DM_TYPE_FRACTION:
		{
		    DMfraction f;
		    f = dmParamsGetFract( p, name );
		    printf( "%d/%d", f.numerator, f.denominator );
		}
		break;
	    case DM_TYPE_PARAMS:
		PrintParams( dmParamsGetParams( p, name ), indent + 4 );
		break;
	    case DM_TYPE_ENUM_ARRAY:
		{
		    int i;
		    const DMenumarray* array = dmParamsGetEnumArray( p, name );
		    for ( i = 0;  i < array->elemCount;  i++ ) {
			printf( "%d ", array->elems[i] );
		    }
		}
		break;
	    case DM_TYPE_INT_ARRAY:
		{
		    int i;
		    const DMintarray* array = dmParamsGetIntArray( p, name );
		    for ( i = 0;  i < array->elemCount;  i++ ) {
			printf( "%d ", array->elems[i] );
		    }
		}
		break;
	    case DM_TYPE_STRING_ARRAY:
		{
		    int i;
		    const DMstringarray* array = 
			dmParamsGetStringArray( p, name );
		    for ( i = 0;  i < array->elemCount;  i++ ) {
			printf( "%s ", array->elems[i] );
		    }
		}
		break;
	    case DM_TYPE_FLOAT_ARRAY:
		{
		    int i;
		    const DMfloatarray* array = 
			dmParamsGetFloatArray( p, name );
		    for ( i = 0;  i < array->elemCount;  i++ ) {
			printf( "%f ", array->elems[i] );
		    }
		}
		break;
	    case DM_TYPE_FRACTION_ARRAY:
		{
		    int i;
		    const DMfractionarray* array = 
			dmParamsGetFractArray( p, name );
		    for ( i = 0;  i < array->elemCount;  i++ ) {
			printf( "%d/%d ", 
			        array->elems[i].numerator,
			        array->elems[i].denominator );
		    }
		}
		break;
	    case DM_TYPE_INT_RANGE:
		{
		    const DMintrange* range = dmParamsGetIntRange( p, name );
		    printf( "%d ... %d", range->low, range->high );
		}
		break;
	    case DM_TYPE_FLOAT_RANGE:
		{
		    const DMfloatrange* range = 
			dmParamsGetFloatRange( p, name );
		    printf( "%f ... %f", range->low, range->high );
		}
		break;
	    case DM_TYPE_FRACTION_RANGE:
		{
		    const DMfractionrange* range = 
			dmParamsGetFractRange( p, name );
		    printf( "%d/%d ... %d/%d", 
			    range->low.numerator, 
			    range->low.denominator, 
			    range->high.numerator,
			    range->high.denominator );
		}
		break;
	    default:
		printf( "UNKNOWN TYPE" );
	    }
	printf( "\n" );
    }
}

////////
//
// Implementation class
//
////////

class ParamsImpl : public Params
{
public:
    
    //
    // Constructor
    //

    ParamsImpl()
    {
	DC( dmParamsCreate( &myParams ) );
    }
    
    //
    // Destructor
    //

    ~ParamsImpl()
    {
	dmParamsDestroy( myParams );
    }
    
    //
    // dmparams
    //

    DMparams* dmparams()
    {
	return myParams;
    }
    
    //
    // width
    //

    int width()
    {
	return dmParamsGetInt( myParams, DM_IMAGE_WIDTH );
    }
    
    //
    // height
    //

    int height()
    {
	return dmParamsGetInt( myParams, DM_IMAGE_HEIGHT );
    }
    
    //
    // vlFormat
    //

    int vlFormat()
    {
	DMimagepacking packing = (DMimagepacking)
	    dmParamsGetEnum( myParams, DM_IMAGE_PACKING );
	switch ( packing )
	{
	    case DM_IMAGE_PACKING_RGBA:
	    case DM_IMAGE_PACKING_ABGR:
	    case DM_IMAGE_PACKING_XRGB1555:
		return VL_FORMAT_RGB;
		
	    case DM_IMAGE_PACKING_CbYCrY:
	    case DM_IMAGE_PACKING_CbYCrA:
		return VL_FORMAT_SMPTE_YUV;
		
	    default:
		ErrorHandler::error( "unknown packing in Params::vlFormat" );
		return 0;
	}
    }
    
    //
    // vlLayout
    //

    int vlLayout()
    {
	DMimagelayout layout = (DMimagelayout)
	    dmParamsGetEnum( myParams, DM_IMAGE_LAYOUT );
	switch ( layout )
	{
	    case DM_IMAGE_LAYOUT_LINEAR:
		return VL_LAYOUT_LINEAR;
		
	    case DM_IMAGE_LAYOUT_GRAPHICS:
		return VL_LAYOUT_GRAPHICS;
		
	    case DM_IMAGE_LAYOUT_MIPMAP:
		return VL_LAYOUT_MIPMAP;
		
	    default:
		ErrorHandler::error( "unknown layout in Params::vlLayout" );
		return 0;
	}
    }
    
    //
    // vlPacking
    //

    int vlPacking()
    {
	DMimagepacking packing = (DMimagepacking)
	    dmParamsGetEnum( myParams, DM_IMAGE_PACKING );
	switch ( packing )
	{
	    case DM_IMAGE_PACKING_RGBA:
		return VL_PACKING_ABGR_8;
		
	    case DM_IMAGE_PACKING_ABGR:
		return VL_PACKING_RGBA_8;
		
	    case DM_IMAGE_PACKING_XRGB1555:
		return VL_PACKING_ARGB_1555;
		
	    case DM_IMAGE_PACKING_CbYCrY:
		return VL_PACKING_YVYU_422_8;
		
	    case DM_IMAGE_PACKING_CbYCrA:
		return VL_PACKING_AUYV_4444_8;
		
	    default:
		ErrorHandler::error( "unknown packing in Params::vlPacking" );
		return 0;
	}
    }
    
    //
    // print
    //

    void print()
    {
	PrintParams( myParams, 4 );
    }
    
private:
    DMparams* myParams;
};

////////////////////////////////////////////////////////////////////////
//
// Static Methods
//
////////////////////////////////////////////////////////////////////////

////////
//
// create
//
////////

Ref<Params> Params::create(
    )
{
    return new ParamsImpl();
}

////////
//
// copy
//
////////

Ref<Params> Params::copy(
    Params* original
    )
{
    Ref<Params> p = new ParamsImpl();
    DC( dmParamsCopyAllElems( original->dmparams(), p->dmparams() ) );
    return p;
}

////////
//
// imageFormat
//
////////

Ref<Params> Params::imageFormat(
    int			width,
    int 		height,
    DMimagelayout	layout,
    DMimageorientation	orientation,
    DMimagepacking	packing
    )
{
    Ref<Params> p = new ParamsImpl();
    DC( dmSetImageDefaults( p->dmparams(), width, height, packing ) );
    DC( dmParamsSetEnum( p->dmparams(), DM_IMAGE_LAYOUT, layout ) );
    DC( dmParamsSetEnum( p->dmparams(), DM_IMAGE_ORIENTATION, orientation ) );
    return p;
}

////////
//
// poolSpec
//
////////

Ref<Params> Params::poolSpec(
    int		bufferCount,
    DMboolean	cacheable
    )
{
    Ref<Params> p = new ParamsImpl();
    DC( dmBufferSetPoolDefaults( p->dmparams(), bufferCount, 0,
				 cacheable, DM_TRUE ) );
    return p;
}
