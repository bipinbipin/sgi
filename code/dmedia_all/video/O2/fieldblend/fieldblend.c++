#include <VideoKit.h>

#define MILLION 1000000

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
    // Open the display.
    //

    Global::init();
    
    
    //
    // Pick an image format.
    //

    int width;
    int height;
    VideoIn::getDefaultFieldSize( VL_ANY, VL_ANY, &width, &height );
    Ref<Params> videoFormat = Params::imageFormat(
	width, height, 
	DM_IMAGE_LAYOUT_GRAPHICS,
	DM_IMAGE_TOP_TO_BOTTOM,
	DM_IMAGE_PACKING_RGBA
	);
    dmParamsSetEnum( 
	videoFormat->dmparams(),
	DM_CAPTURE_TYPE,
	DM_CAPTURE_TYPE_FIELDS
	);
    
    Ref<Params> windowFormat = Params::imageFormat(
	width, height * 2, 
	DM_IMAGE_LAYOUT_GRAPHICS,
	DM_IMAGE_TOP_TO_BOTTOM,
	DM_IMAGE_PACKING_RGBA
	);
    
    //
    // Set up the video paths.
    //

    Ref<VideoIn>  videoin  = VideoIn ::create( VL_ANY, VL_ANY, videoFormat );
    Ref<VideoOut> videoout = VideoOut::create( VL_ANY, VL_ANY, videoFormat );
    
    //
    // Create an fbconfig and a dmbuffer.
    //

    GLXFBConfigSGIX pb_config = Global::chooseFBConfig(
	videoFormat,
	0,		// depth
	0,		// stencil
	DM_FALSE	// double buffered
	);
    Ref<DMPbuffer> pbuffer = DMPbuffer::create( pb_config, videoFormat );
    Ref<Context> pb_context = Context::create( pb_config, videoFormat );
    
    //
    // Create a window (and another fbconfig).
    //
    
    GLXFBConfigSGIX w_config = Global::chooseFBConfig(
	windowFormat,
	0,		// depth
	0,		// stencil
	DM_TRUE 	// double buffered
	);

    Ref<GLWindow> window = GLWindow::create( "Video", w_config, windowFormat );
    Ref<Context> w_context = Context::create( w_config,  windowFormat );
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
    // Pass the video through, blending in the grahpics.
    //

    videoin->start();
    videoout->start();
    while ( DM_TRUE )
    {
	Ref<Buffer> frame = videoin->getFrame();
	pbuffer->associate( frame );
	printf( "frame\n" );

	static int y = 1;
	y = 1 - y;

	w_context->makeCurrent( window->drawable() );
	glClearColor( 0.0, y, 0.0, 0.0 );
	glClear( GL_COLOR_BUFFER_BIT );
	glColor4f( 1, 0, 0, 1 );
	glEnable( GL_LINE_SMOOTH );
	glBegin( GL_LINES );
	{
	    glVertex2i( 0, 0 );
	    glVertex2i( width/2, height*2 );
	    glVertex2i( width, 0 );
	    glVertex2i( width/2, height*2 );
	}
	glEnd();
	
	pb_context->makeCurrentRead( pbuffer->drawable(), window->drawable() );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glPixelZoom( 1.0, 0.5 );
	glRasterPos2f( 0.1, 0.1 );
	glCopyPixels( 0, y, width, height*2, GL_COLOR );
        glDisable( GL_BLEND );

	window->swapBuffers();
	
	videoout->putFrame( frame );
    }
    
    // Global::cleanup();
    // return 0;
}
