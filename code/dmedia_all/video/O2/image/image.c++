#include "VideoKit.h"

#include <GL/glu.h>

#include <il/ilABGRImg.h>
#include <il/ilConfigure.h>
#include <il/ilDisplay.h>
#include <il/ilFileImg.h>

#include <unistd.h> // sginap()

Bool lumakey = False;

#define YUV

////////
//
// readFile
//
////////

void readImage(
    const char* fileName,
    int* returnWidth,
    int* returnHeight,
    void** returnPixels
    )
{
    ilFileImg*		myOrigImage = NULL;
    ilABGRImg*		myAbgrImage = NULL;

    //
    // open the file
    //
    
    myOrigImage = new ilFileImg( fileName );
    if ( myOrigImage->getStatus() != ilOKAY ) {
	fprintf( stderr, "Could not open image file: %s\n", fileName );
	exit( 1 );
    }
    
    iflSize size;
    myOrigImage->getSize( size );

    //
    // create the ABGR format image.
    //
    
    myAbgrImage = new ilABGRImg( myOrigImage );
    if ( myAbgrImage->getStatus() != ilOKAY )
    {
	fprintf( stderr, "Could not create ABGR image\n" );
	exit( 1 );
    }
    myAbgrImage->setOrientation(iflLowerLeftOrigin);

    //
    // Get the image
    //

    void* buffer = malloc( size.x * size.y * 4 );
    if ( buffer == NULL )
    {
	fprintf( stderr, "malloc failed\n" );
	exit( 1 );
    }
    
    myAbgrImage->getTile( 0, 0, size.x, size.y, buffer );
    
    //
    // All done.
    //

    delete myAbgrImage;
    delete myOrigImage;
    *returnWidth  = size.x;
    *returnHeight = size.y;
    *returnPixels = buffer;
}

////////
//
// chooseTextureSize
//
////////

static int chooseTextureSize(
    int	    originalSize
    )
{
    //
    // Round up to a power of 2.
    //

    int size = 1;
    while ( size < originalSize ) {
	size *= 2;
    }
    
    return size;
}

////////
//
// createTexture
//
////////

void createTexture(
    int imageWidth,
    int imageHeight,
    void* image,
    int* returnTexWidth,
    int* returnTexHeight,
    void** returnTexture
    )
{
    //
    // Create the buffer to hold the texture.
    //

    int texWidth  = chooseTextureSize( imageWidth  );
    int texHeight = chooseTextureSize( imageHeight );
    void* texture = calloc( texWidth * texHeight, 4 );
    if ( texture == NULL )
    {
	fprintf( stderr, "malloc failed\n" );
	exit( 1 );
    }
    
    //
    // Copy the pixel in, converting from RGBA to YCrCbA, and adding a
    // luma key.
    //

    uint8_t* src = (uint8_t*) image;
    uint8_t* dst = (uint8_t*) texture;
    for ( int y = 0;  y < imageHeight;  y++ )
    {
	for ( int x = 0;  x < imageWidth;  x++ )
	{
	    uint8_t A = *src++;
	    uint8_t B = *src++;
	    uint8_t G = *src++;
	    uint8_t R = *src++;
	    
#ifdef YUV
	    uint8_t Y  =  0.257 * R + 0.504 * G + 0.098 * B;
	    uint8_t Cr = -0.148 * R - 0.291 * G + 0.439 * B + 128;
	    uint8_t Cb =  0.439 * R - 0.368 * G - 0.071 * B + 128;
	    
	    if ( lumakey )
	    {
		A = Y;
	    }
	    
	    *dst++ = Cb;
	    *dst++ = Y;
	    *dst++ = Cr;
	    *dst++ = A;
#else
	    *dst++ = R;
	    *dst++ = G;
	    *dst++ = B;
	    *dst++ = A;
#endif
	}
	
	dst += ( texWidth - imageWidth ) * 4;
    }

    *returnTexWidth  = texWidth;
    *returnTexHeight = texHeight;
    *returnTexture   = texture;
}

////////
//
// checkInput
//
////////

int state = 0;   // which button is down
int lastX = 0;
int lastY = 0;

float x;
float y;
float r;
float s;
float f = 1.0;

float dx;
float dy;
float dr;
float ds;

void checkInput(
    int width
    )
{
    int xfd = XConnectionNumber( Global::xDisplay );
    
  LOOP:
    fd_set fds;
    FD_ZERO( &fds );
    FD_SET( xfd, &fds );
	
    struct timeval noWait;
    noWait.tv_sec = 0;
    noWait.tv_usec = 0;

    if ( select( xfd + 1, &fds, NULL, NULL, &noWait ) != 1 )
    {
	return;
    }
	
    XEvent event;
    XNextEvent( Global::xDisplay, &event );
	
    switch ( event.type )
    {
	case MotionNotify:
	    switch ( state )
	    {
		case 1:
		    dx = ( event.xmotion.x - x );
		    dy = ( event.xmotion.y - y );
		    break;
			
		case 2:
		    f = float(event.xmotion.x) / width;
		    break;
		    
		case 3:
		    dr += (event.xmotion.x - lastX);
		    ds -= (event.xmotion.y - lastY);
		    lastX = event.xmotion.x;
		    lastY = event.xmotion.y;
		    break;
	    }
	    break;

	case ButtonPress:
	    state = event.xbutton.button;
	    lastX = event.xbutton.x;
	    lastY = event.xbutton.y;
	    break;
		
	case ButtonRelease:
	    state = 0;
	    break;
    }
    
    goto LOOP;
}

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
* renderField
*
********/

void renderField(
    int width,
    int height
    )
{
    glMatrixMode( GL_MODELVIEW );
    glPushMatrix();

    glTranslatef( x, height - y, 0 );
    glRotatef( -r / 2.0, 0, 0, 1 );
    float scale = ( s + 200 ) / 200.0;
    glScalef( scale, scale, scale );

    glBegin( GL_QUADS );
    {
	glTexCoord2f( 0.0, 0.0 ); glVertex2i( -width/4, -height/4 );
	glTexCoord2f( 0.0, 1.0 ); glVertex2i( -width/4,  height/4 );
	glTexCoord2f( 1.0, 1.0 ); glVertex2i(  width/4,  height/4 );
	glTexCoord2f( 1.0, 0.0 ); glVertex2i(  width/4, -height/4 );
    }
    glEnd();

    glEnable( GL_LINE_SMOOTH );
    glBegin( GL_LINE_STRIP );
    {
	/****
	glTexCoord2f( 0.0, 0.0 ); glVertex2i( -width/4, -height/4 );
	glTexCoord2f( 0.0, 1.0 ); glVertex2i( -width/4,  height/4 );
	glTexCoord2f( 1.0, 1.0 ); glVertex2i(  width/4,  height/4 );
	glTexCoord2f( 1.0, 0.0 ); glVertex2i(  width/4, -height/4 );
	glTexCoord2f( 0.0, 0.0 ); glVertex2i( -width/4, -height/4 );
	****/
	glTexCoord2f( 0.01, 0.01 ); glVertex2i( -width/4, -height/4 );
	glTexCoord2f( 0.01, 0.99 ); glVertex2i( -width/4,  height/4 );
	glTexCoord2f( 0.99, 0.99 ); glVertex2i(  width/4,  height/4 );
	glTexCoord2f( 0.99, 0.01 ); glVertex2i(  width/4, -height/4 );
	glTexCoord2f( 0.01, 0.01 ); glVertex2i( -width/4, -height/4 );
    }
    glEnd();
    glDisable( GL_LINE_SMOOTH );
    
    glPopMatrix();
}

/********
*
* renderFrame
*
********/

void renderFrame(
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
     * Render both fields.
     */

    {
	glColor4f( 0.0, 0.0, 0.0, f );  // fade
	
	glEnable( GL_STENCIL_TEST );

	x += dx/2.0;   y += dy/2.0;
	r += dr/2.0;   s += ds/2.0;

	glStencilFunc( GL_EQUAL, 0x1, 0x1 );
	glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
	renderField( width, height );
	
	x += dx/2.0;   y += dy/2.0;
	r += dr/2.0;   s += ds/2.0;

	glStencilFunc( GL_NOTEQUAL, 0x1, 0x1 );
	glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
	renderField( width, height );
	
	dx = 0;   dy = 0;
	dr = 0;   ds = 0;

	glDisable( GL_STENCIL_TEST );
    }
    
    glFinish();
}

////////
//
// usage
//
////////

void usage(
    )
{
    fprintf( stderr, "usage:  image imagefile\n" );

    exit( 1 );
}

////////
//
// main
//
////////

main(
    int argc,
    char** argv
    )
{
    Global::init();
    
    if ( argc != 2 )
    {
	usage();
    }

    //
    // Open the image file
    //

    int imageWidth;
    int imageHeight;
    void* image;
    readImage( argv[1], &imageWidth, &imageHeight, &image );
    
    int textureWidth;
    int textureHeight;
    void* texture;
    createTexture( imageWidth, imageHeight, image,
		   &textureWidth, &textureHeight, &texture );
    
    //
    // Pick an image format.  We actually need two because we're going
    // to lie to GL.
    //

    int width;
    int height;
    VideoIn::getDefaultFrameSize( VL_ANY, VL_ANY, &width, &height );
    Ref<Params> format = Params::imageFormat(
	width, height,
	DM_IMAGE_LAYOUT_GRAPHICS,
	DM_IMAGE_TOP_TO_BOTTOM,
#ifdef YUV
	DM_IMAGE_PACKING_CbYCrA
#else
	DM_IMAGE_PACKING_RGBA
#endif
	);
    
    Ref<Params> glformat = Params::copy( format );
    dmParamsSetEnum( 
	glformat->dmparams(),
	DM_IMAGE_PACKING,
	DM_IMAGE_PACKING_RGBA
	);
    
    //
    // Set up the video paths.
    //

    Ref<VideoIn>  videoin  = VideoIn ::create( VL_ANY, VL_ANY, format );
    Ref<VideoOut> videoout = VideoOut::create( VL_ANY, VL_ANY, format );
    
    //
    // Create a pbuffer that can render on top of the video.
    //

    GLXFBConfigSGIX fbconfig = Global::chooseFBConfig( glformat, 0, 1, False );
    Ref<DMPbuffer> pbuffer = DMPbuffer::create( fbconfig, glformat );
    
    //
    // Create the pool
    //

    Ref<Pool> pool = Pool::create( 1, DM_FALSE );
    pool->addUser( videoin );
    pool->addUser( pbuffer );
    pool->addUser( videoout );
    pool->allocatePool();
    
    //
    // Create and initialize the context.
    //

    Ref<Context> context = Context::create( fbconfig, glformat );
    {
	Ref<Buffer> buffer = pool->allocateBuffer( glformat );
	pbuffer->associate( buffer );
	context->makeCurrent( pbuffer->drawable() );
	
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	
	gluBuild2DMipmaps( GL_TEXTURE_2D, GL_RGBA, 
			   textureWidth, textureHeight,
			   GL_RGBA, GL_UNSIGNED_BYTE,
			   texture );
	
	glEnable( GL_TEXTURE_2D );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			 GL_LINEAR_MIPMAP_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	glColor4f( 0.0, 0.0, 0.0, 1.0 );
	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
	
	glMatrixMode( GL_TEXTURE );
	glScalef( 
	    float(imageWidth)  / float(textureWidth),
	    float(imageHeight) / float(textureHeight),
	    1.0
	    );
	glMatrixMode( GL_MODELVIEW );
    }

    //
    // Create a window.  It's just used to gather mouse events.
    //

    Ref<GLWindow> win = GLWindow::create( 
	"events",
	fbconfig,
	format 
	);
    XSelectInput(
	Global::xDisplay, 
	win->xwindow(), 
	PointerMotionMask | ButtonPressMask | ButtonReleaseMask 
	);
    
    //
    // Go
    //

    x = width / 2;
    y = height / 2;
    videoin ->start();
    videoout->start();
    while ( 1 )
    {
	Ref<Buffer> buffer = videoin->getFrame();
	
	pbuffer->associate( buffer );
	renderFrame( width, height );
	videoout->putFrame( buffer );
	
	checkInput( width );
    }
    
}
