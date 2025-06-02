////////////////////////////////////////////////////////////////////////
//
// beamMeUp.c++
//
//  A video transition
//
////////////////////////////////////////////////////////////////////////

#include <dmedia/PremiereXAPI.h>
#include <assert.h>
#include <stdio.h>   //XXX just for debugging
#include <inttypes.h>
#include <stdlib.h>   //rand()

#define FALSE 0

////////
//
// transition
//
////////

int transition(
    PRX_Transition theData
    )
{
    PRX_ScanlineBuffer srcA = theData->srcA;
    PRX_ScanlineBuffer srcB = theData->srcB;
    PRX_ScanlineBuffer dst = theData->dst;

    //
    // Paranoia
    //

    if(srcA->data == NULL || srcB->data == NULL)
    {
	//assert(FALSE);
	return dcMemErr;     //XXX
    }
    
    assert( srcA->bounds.height == srcB->bounds.height );
    assert( srcA->bounds.height == dst ->bounds.height );
    assert( srcA->bounds.width  == srcB->bounds.width  );
    assert( srcA->bounds.width  == dst ->bounds.width  );
    
    assert( srcA->rowBytes % 4 == 0 );
    assert( srcB->rowBytes % 4 == 0 );
    assert( dst ->rowBytes % 4 == 0 );
    

    //
    // Process the pixels
    //
    
    float percent= (float)theData->part/(float)theData->total;
    int   width  = srcA->bounds.width;
    int   height = srcA->bounds.height;

    uint32_t* srcA_p = (uint32_t*)srcA->data +
	srcA->bounds.x + (srcA->bounds.y * srcA->rowBytes / sizeof(uint32_t));
    uint32_t* srcB_p = (uint32_t*)srcB->data +
	srcB->bounds.x + (srcB->bounds.y * srcB->rowBytes / sizeof(uint32_t));
    uint32_t* dst_p  = (uint32_t*)dst->data +
	dst->bounds.x + (dst->bounds.y * dst->rowBytes / sizeof(uint32_t));
    
    int srcABump = srcA->rowBytes/4 - width;
    int srcBBump = srcB->rowBytes/4 - width;
    int dstBump  = dst->rowBytes/4 - width;
    
    assert( 0 <= srcABump && 0 <= srcBBump && 0 <= dstBump );
   
    for ( int y = 0;  y < height;  y++ )
    {
	for ( int x = 0;  x < width;  x++ )
	{
	    uint32_t pixA = *(srcA_p);
	    uint32_t pixB = *(srcB_p);
	    if ((float(rand())/32767.0) > percent)
		*dst_p = pixA;
	    else *dst_p = pixB;
       
	    srcA_p++;
	    srcB_p++;
	    dst_p++;
	}
	
	srcA_p += srcABump;
	srcB_p += srcBBump;
	dst_p  += dstBump;
    }
    
    return dcNoErr;
}

////////
//
// transitionDispatch
//    returns 0 (dcNoErr) if completed without error
//    returns "dc"error code if there was an error
//
//
////////

extern "C" int transitionDispatch(
    int  selector, /* esCleanup = -1, esExecute = 0, esSetup = 1 */
    PRX_Transition theData
    )
{
    switch(selector)
	{
	case esCleanup:
	    // printf("BeamMeUp: Cleanup not implemented\n");
	    return dcNoErr;
	    
	case esExecute:
	    return transition(theData);
	    
	case esSetup:
	    // printf("BeamMeUp: Setup not implemented\n");
	    return dcNoErr;
	    
	default:
	    assert(FALSE);
	    return dcUnsupported;
	}
}

////////
//
// PRX_PluginProperties
//
////////

static PRX_SPFXOptionsRec options =
{
    0,			/* enable: which corners are enabled */
    0,			/* state: which corners are initially set */
    optNoFirstSetup,	/* flags */
    0,			/* exclusive: do corners act like radio buttons */
    0,			/* hasReverse */
    0,			/* hasEdges */
    0,			/* hasStart */
    0			/* hasEnd */
};

extern "C" PRX_PropertyList PRX_PluginProperties(
    unsigned /* apiVersion */
    )
{
    static PRX_KeyValueRec pluginProperties[] = {
        //Required properties for all plug-in types
	{PK_APIVersion,		(void*) PRX_MAJOR_MINOR(1,0)},
	{PK_DispatchSym,	(void*) "transitionDispatch"},
	{PK_Name,		(void*) "BeamMeUp"},
	{PK_PluginType,		(void*) PRX_TYPE_TRANSITION},
	{PK_PluginVersion,	(void*) PRX_MAJOR_MINOR(1,0)},
	{PK_UniquePrefix,	(void*) "SGI"},

        //Required properties for transitions
	{PK_Description,	(void*) "Probabilistic replacement of A's pixels by B's"},
	{PK_SPFXOptions,	(void*) &options},
	{PK_EndOfList,		(void*) 0},
    };
    
    return(PRX_PropertyList) & pluginProperties;
}

