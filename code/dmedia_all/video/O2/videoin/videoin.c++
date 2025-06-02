#include <VideoKit.h>

int main(
    )
{
    Global::init();
    
    //
    // Pick an image format.
    //

    int width;
    int height;
    VideoIn::getDefaultFrameSize( VL_ANY, VL_ANY, &width, &height );
    Ref<Params> format = Params::imageFormat(
	width, height, 
	DM_IMAGE_LAYOUT_LINEAR,
	DM_IMAGE_TOP_TO_BOTTOM,
	DM_IMAGE_PACKING_RGBA
	);
    
    //
    // Set up the video input path.
    //

    Ref<VideoIn> video = VideoIn::create( VL_ANY, VL_ANY, format );
    
    //
    // Set up the window.
    //

    GLXFBConfigSGIX fbconfig = Global::chooseFBConfig( format, 0, 0, False );
    Ref<GLWindow> window = GLWindow::create( "Video In", fbconfig, format );
    Ref<Context> context = Context::create( fbconfig, format );
    context->makeCurrent( window->drawable() );
    
    //
    // Create the pool
    //

    Ref<Pool> pool = Pool::create( 1, DM_FALSE );
    pool->addUser( video );
    pool->allocatePool();
    
    //
    // Draw the video to the window.
    //

    video->start();
    while ( DM_TRUE )
    {
	Ref<Buffer> frame = video->getFrame();
	glRasterPos2f( 0.1, height - 0.9 );
	glPixelZoom( 1, -1 );
	glDrawPixels( width, height, GL_RGBA, 
		      GL_UNSIGNED_BYTE, frame->data());
    }
    
    // Global::cleanup();
    // return 0;
}
