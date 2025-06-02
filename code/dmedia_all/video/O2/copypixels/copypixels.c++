#include <VideoKit.h>

#define MILLION 1000000

void usage(
    )
{
    fprintf( stderr, "usage:  copypixels [-d] -16\n" );
    fprintf( stderr, "usage:  copypixels [-d] -32\n" );
    fprintf( stderr, "    -d   double buffer\n" );
    exit( 1 );
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
    //
    // Parse the command line.
    //

    Bool doubleBuffer = False;
    DMimagepacking windowPacking = (DMimagepacking) 0;
    
    for ( int i = 1;  i < argc;  i++ )
    {
	if ( strcmp( argv[i], "-d" ) == 0 )
	{
	    doubleBuffer = True;
	}
	else if ( strcmp( argv[i], "-16" ) == 0 )
	{
	    windowPacking = DM_IMAGE_PACKING_XRGB1555;
	}
	else if ( strcmp( argv[i], "-32" ) == 0 )
	{
	    windowPacking = DM_IMAGE_PACKING_RGBA;
	}
	else
	{
	    usage();
	}
    }
    
    if ( windowPacking == 0 )
    {
	usage();
    }
    
    //
    // Open the display.
    //

    Global::init();
    
    
    //
    // Pick an image format.
    //

    int width;
    int height;
    VideoIn::getDefaultFrameSize( VL_ANY, VL_ANY, &width, &height );
    Ref<Params> format = Params::imageFormat(
	width, height, 
	DM_IMAGE_LAYOUT_GRAPHICS,
	DM_IMAGE_TOP_TO_BOTTOM,
	DM_IMAGE_PACKING_RGBA
	);
    Ref<Params> windowFormat =Params::imageFormat(
	width, height, 
	DM_IMAGE_LAYOUT_GRAPHICS,
	DM_IMAGE_TOP_TO_BOTTOM,
	windowPacking
	);
    //
    // Set up the video paths.
    //

    Ref<VideoIn>  videoin  = VideoIn ::create( VL_ANY, VL_ANY, format );
    Ref<VideoOut> videoout = VideoOut::create( VL_ANY, VL_ANY, format );
    
    //
    // Create an fbconfig and a dmbuffer.
    //

    GLXFBConfigSGIX pb_config = Global::chooseFBConfig(
	format,
	0,		// depth
	0,		// stencil
	DM_FALSE	// double buffered
	);
    Ref<DMPbuffer> pbuffer = DMPbuffer::create( pb_config, format );
    Ref<Context> pb_context = Context::create( pb_config, format );
    
    //
    // Create a window (and another fbconfig).
    //
    
    GLXFBConfigSGIX w_config = Global::chooseFBConfig(
	windowFormat,
	0,		// depth
	0,		// stencil
	doubleBuffer	// double buffered
	);
    printf( "Window FBconfig:\n" );
    Global::printFBConfig( w_config ); 

    Ref<GLWindow> window = GLWindow::create( "Video", w_config, windowFormat );
    Ref<Context> w_context = Context::create( w_config,  format );
    w_context->makeCurrent( window->drawable() );
    
    //
    // Create the pool
    //

    Ref<Pool> pool = Pool::create( 1, DM_FALSE );
    pool->addUser( videoin );
    pool->addUser( pbuffer );
    pool->addUser( videoout );
    pool->allocatePool();
    
    //
    // Pass the video through, checking timestamps.
    //

    videoin->start();
    videoout->start();
    while ( DM_TRUE )
    {
	Ref<Buffer> frame = videoin->getFrame();
	
	pbuffer->associate( frame );

	w_context->makeCurrentRead( window->drawable(), pbuffer->drawable() );
	glClearColor( 0.0, 0.5, 0.0, 1.0 );
	glClear( GL_COLOR_BUFFER_BIT );
	glRasterPos2f( 0.1, 0.1 );
	glCopyPixels( 0, 0, width, height, GL_COLOR );
	glColor4f( 1, 0, 0, 0 );
	glEnable( GL_LINE_SMOOTH );
	glBegin( GL_LINES );
	{
	    glVertex2i( 0, 0 );
	    glVertex2i( width, height );
	    glVertex2i( width, 0 );
	    glVertex2i( 0, height );
	}
	glEnd();
	
	window->swapBuffers();
	
	pb_context->makeCurrentRead( pbuffer->drawable(), window->drawable() );
	glReadBuffer( GL_FRONT );
	glClearColor( 0, 0.0, 0.5, 1.0 );
        glClear( GL_COLOR_BUFFER_BIT );
	glRasterPos2f( 0.1, 0.1 );
	glCopyPixels( 0, 0, width, height, GL_COLOR );
	
	videoout->putFrame( frame );
    }
    
    // Global::cleanup();
    // return 0;
}
