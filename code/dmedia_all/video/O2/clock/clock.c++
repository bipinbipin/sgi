#include <VideoKit.h>

#define BILLION ((stamp_t)1000000000)

DMboolean frameFlag = DM_FALSE;

/********
*
* setupStencil
*
********/

void setupStencil(
    int width,
    int height
    )
{
    glClearStencil( 0x0 );
    glClear( GL_STENCIL_BUFFER_BIT );

    glEnable( GL_STENCIL_TEST );

    glStencilFunc ( GL_ALWAYS, 0x1, 0x1 );
    glStencilOp( GL_REPLACE, GL_REPLACE, GL_REPLACE );

    glColor3f( 1.0, 1.0, 1.0 );

    glBegin( GL_LINES );
    {
	int line;
        for ( line = 0; line < height; line += 2 ) {
            glVertex2f( 0.0, line + 0.5 );
            glVertex2f( width, line + 0.5 );
        }
    }
    glEnd();

    glDisable( GL_STENCIL_TEST );
    
    glClearColor( 0, 0, 0, 0 );
    glClear( GL_COLOR_BUFFER_BIT );
    
}

/********
*
* rgbaColor
*
********/

static void rgbaColor(
    float r,
    float g,
    float b,
    float a
    )
{
    /* This is from Video Demystified, 2nd edition, page 43 */
    float y  = ( 255.0 * ( 0.257 * r + 0.504 * g + 0.098 * b ) +  16 ) / 255.0;
    float cr = ( 255.0 * (-0.148 * r - 0.291 * g + 0.439 * b ) + 128 ) / 255.0;
    float cb = ( 255.0 * ( 0.439 * r - 0.368 * g - 0.071 * b ) + 128 ) / 255.0;

    glColor4f( cb, y, cr, a );
}

/********
*
* drawHand
*
********/

static void drawHand(
    float rotation,
    int   width,
    int   height,
    float r,
    float g,
    float b
    )
{
    glPushMatrix();
    glRotatef( -rotation, 0, 0, 1 );
    rgbaColor( r, g, b, 0.3 );
    glBegin( GL_QUADS );
    {
	glVertex2i( -width,      0 );
	glVertex2i(      0, height );
	glVertex2i(  width,      0 );
	glVertex2i(      0,    -20 );
    }
    glEnd();
    rgbaColor( r, g, b, 0.8 );
    glBegin( GL_LINE_STRIP );
    {
	glVertex2i( -width,      0 );
	glVertex2i(      0, height );
	glVertex2i(  width,      0 );
	glVertex2i(      0,    -20 );
	glVertex2i( -width,      0 );
    }
    glEnd();
    glPopMatrix();
}


/********
*
* renderField
*
********/

void renderField(
    stamp_t outputUST,
    int width,
    int height
    )
{
    struct timeval currentTV;
    stamp_t currentUST;
    stamp_t currentSixtieths;
    stamp_t outputSixtieths;
    time_t seconds;
    struct tm* local;
    double outputSeconds;
    double outputMinutes;
    double outputHours;
    
    /*
     * Get a reference point for correlating UST and clock time.
     */

    dmGetUSTCurrentTimePair(
	(unsigned long long*) &currentUST,
	&currentTV 
	);
    currentSixtieths = 
	((stamp_t)currentTV.tv_sec) * 60 + 
	((stamp_t)currentTV.tv_usec) * 60 / 1000000;
    
    /*
     * Extropolate the time at the outputUST.  We use time in
     * sixtieths of a second.
     */

    outputSixtieths =
	currentSixtieths + ( outputUST - currentUST ) / (BILLION/60);
    
    /*
     * Get the seconds, minutes, and hours for the current time zone.
     */

    seconds = (time_t) ( outputSixtieths / 60 );
    local = localtime( &seconds );
    
    /*
     * Render the clock
     */

    glEnable( GL_LINE_SMOOTH );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    glMatrixMode( GL_MODELVIEW );
    glPushMatrix();

    glTranslatef( width / 2.0,  height / 2.0, 0.0 );

#if 0
    glPushMatrix();
    glRotatef( - ( outputSixtieths % 60 ) * 6.0, 0, 0, 1 );
    glBegin( GL_QUADS );
    {
	glColor4f( 1, 0, 0, 0.8 ); glVertex2i( -10, 200 );
	glColor4f( 1, 0, 0, 1.0 ); glVertex2i(   0, 210 );
	glColor4f( 1, 0, 0, 0.8 ); glVertex2i(  10, 200 );
	glColor4f( 0, 0, 0, 0.6 ); glVertex2i(   0, 190 );
    }
    glEnd();
    glPopMatrix();
#endif
    
    outputSeconds = local->tm_sec + ( outputSixtieths % 60 ) / 60.0;
    drawHand( outputSeconds * 6.0, 5, 200, 1.0, 1.0, 0.0 );
    
    outputMinutes = local->tm_min + outputSeconds / 60.0;
    drawHand( outputMinutes * 6.0, 10, 150, 0.8, 0.8, 0.8 );
    
    outputHours = local->tm_hour + outputMinutes / 60.0;
    drawHand( outputHours * 30.0, 10, 100, 0.8, 0.8, 0.8 );
    
    glPopMatrix();
    glDisable( GL_LINE_SMOOTH );

    {
	static int x = 0;
	x = ( x + 5 ) % 770;
	
	rgbaColor( 0.4, 0.8, 0.8, 0.7 );

	glBegin( GL_QUADS );
	{
	    glVertex2f( x - 50, 400 );
	    glVertex2f( x - 50, 450 );
	    glVertex2f( x,      450 );
	    glVertex2f( x,      400 );
	}
	glEnd();
    }
    
    glDisable( GL_BLEND );
}

/********
*
* renderOneFrame
*
********/

void renderOneFrame(
    stamp_t ust,
    int width,
    int height
    )
{
    /*
     * Set up the stencil.
     */
    
    {
	static Bool firstTime = True;
	if ( firstTime )
	{
	    firstTime = False;
	    printf( "drawing stencil\n" );
	    setupStencil( width, height );
	}
    }
    
    /*
     * Render either the whole frame, or the two fields.
     */

    if ( frameFlag )
    {
	renderField( ust, width, height );
	renderField( ust + BILLION / 60, width, height );
    }
    else
    {
	glEnable( GL_STENCIL_TEST );

	glStencilFunc( GL_EQUAL, 0x1, 0x1 );
	glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
	renderField( ust, width, height );
	
	glStencilFunc( GL_NOTEQUAL, 0x1, 0x1 );
	glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
	renderField( ust + BILLION / 60, width, height );
	
	glDisable( GL_STENCIL_TEST );
    }
}

////////
//
// main
//
////////

int main(
    int argc,
    char** argv
    )
{
    Global::init();
    
    //
    // Parse arguments
    //
    
    {
	int c;
	
	while ( ( c = getopt( argc, argv, "f" ) ) != -1 )
	{
	    switch ( c )
	    {
		case 'f':
		    frameFlag = True;
		    break;
		    
		default:
		    assert( False );
	    }
	}
    }
    
	
    //
    // Pick an image format.  A second one is used to lie to OpenGL.
    //

    int width;
    int height;
    VideoIn::getDefaultFrameSize( VL_ANY, VL_ANY, &width, &height );
    Ref<Params> videoFormat = Params::imageFormat(
	width, height, 
	DM_IMAGE_LAYOUT_GRAPHICS,
	DM_IMAGE_TOP_TO_BOTTOM,
	DM_IMAGE_PACKING_CbYCrA
	);
    Ref<Params> gfxFormat = Params::copy( videoFormat );
    dmParamsSetEnum(
	gfxFormat->dmparams(),
	DM_IMAGE_PACKING,
	DM_IMAGE_PACKING_RGBA
	);
    dmParamsSetEnum( 
	gfxFormat->dmparams(),
	DM_IMAGE_LAYOUT,
	DM_IMAGE_LAYOUT_GRAPHICS
	);
    
    //
    // Set up the video input and output paths.
    //

    Ref<VideoIn>  videoIn  = VideoIn ::create( VL_ANY, VL_ANY, videoFormat );
    Ref<VideoOut> videoOut = VideoOut::create( VL_ANY, VL_ANY, videoFormat );
    
    //
    // Set up the pbuffer where the rendering will happen.
    //

    GLXFBConfigSGIX fbconfig = Global::chooseFBConfig( gfxFormat, 0, 1, False);
    Ref<DMPbuffer> pbuffer = DMPbuffer::create( fbconfig, gfxFormat );
    Ref<Context> context = Context::create( fbconfig, gfxFormat );
    
    //
    // Create the pool
    //

    Ref<Pool> pool = Pool::create( 1, DM_FALSE );
    pool->addUser( videoIn );
    pool->addUser( pbuffer );
    pool->addUser( videoOut );
    pool->allocatePool();
    
    //
    // Make the graphics context current.  There must be a dmbuffer
    // associated with the pbuffer before it can be made current.
    //
    
    {
	Ref<Buffer> dummy = pool->allocateBuffer( gfxFormat );
	pbuffer->associate( dummy );
	context->makeCurrent( pbuffer->drawable() );
	printf( "first makeCurrent is done\n" );
    }
    
    //
    // Copy video from input to output, rendering onto it.
    //

    videoIn->start();
    videoOut->start();
    while ( DM_TRUE )
    {
	Ref<Buffer> frame = videoIn->getFrame();
	
	frame->setFormat( gfxFormat );
	pbuffer->associate( frame );
	context->makeCurrent( pbuffer->drawable() );

	USTMSCpair stamp = frame->timestamp();
	renderOneFrame( stamp.ust, width, height );
	
	videoOut->putFrame( frame );
    }
    
    // Global::cleanup();
    // return 0;
}
