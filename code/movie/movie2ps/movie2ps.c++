#include <assert.h>
#include <stdio.h>
#include <time.h>

typedef int Boolean;
#define FALSE 0
#define TRUE  1

////////////////////////////////////////////////////////////////////////
//
// ContactSheet class
//
// Knows how to print pages full of images with captions.
//
////////////////////////////////////////////////////////////////////////

class ContactSheet
{
public:
    //
    // Constructor -- sets up the page size etc.  Outputs the
    // PostScript prolog.
    //

    ContactSheet(
	FILE*	outputFile,	// where the PostScript is sent
        int	imageCount,	// the number of images that will be
				// printed.  PostScript needs a page
				// count at the beginning.
        float	imageAspect,	// The aspect ratio of the images to
				// display.  If the images are
				// actually a different shape, they
				// will be padded with white.
        int	pageWidth,	// in points
        int	pageHeight,	// in points
    	int	margin,		// in points
        int	labelFontSize,	// in points, the size of the labels
				// under the images.
        int	pageFontSize,	// in points, the size of the page
				// numbers. 
        int	gap,		// gap between images in a row, in points
    	int	columns,	// how many images to put across the page
	Boolean	grayscale	// translate color to grayscale?
        );
    
    //
    // Destructor -- outputs the closing PostScript.  Must be called
    // to produce a valid PostScript file.
    //

    ~ContactSheet();
    
    //
    // printImage() -- output one image.  If imageCount was specified
    // in the constructor, this must not be called more times than
    // imageCount.
    //

    void printImage(
	const char*	label,	// The string that will appear under
				// the image.  Can be NULL.
        int		width,  // Width of the image, in pixels
        int		height,	// Height of the image, in pixels
        const void*	buffer	// the pixels, left-to-right,
				// bottom-to-top, XBGR.
	);
    
private:
    //
    // Settings specified directly by the client.
    //

    FILE*	myOutputFile;
    int		myImageCount;
    float	myImageAspect;
    int		myPageWidth;
    int		myPageHeight;
    int		myMargin;
    int		myLabelSize;
    int		myPagenumSize;
    int		myGap;
    int		myColumnCount;
    Boolean	myGrayscale;
    
    //
    // Numbers that we compute from the above settings.
    //

    int		myPrintableWidth;	// size of printable area
					// (inside margins) 
    int		myPrintableHeight;
    
    int		myCellWidth;		// Area allocated to image+label+gap 
    int		myCellHeight;

    int		myImageWidth;		// size of the area that each
    int		myImageHeight;		// image occupies 

    int		myRowCount;		// number of rows of images on a page
    int		myExtraGap;		// the rows may not fit
					// exactly, in which case
					// extra space is inserted
					// between each row.

    int		myPageLabelHeight;

    int		myCellsPerPage;		// how many images per page
    int		myPageCount;		// number of pages that will
					// be printed 

    //
    // Printing state.
    //

    Boolean	myPageStarted;	// Is there a page in progress.
    int		myImageIndex;	// Counts the images that are printed:
				// the index of the next image to print.
    //
    // Private methods.
    //

    void	computeSizes();
    void	dumpByte( unsigned char );
    void	dumpImage( int width, int height, const void* buffer );
    void	finishPage();
    void	prolog();
    void	startPage( int pageNum );

    void	D( int );
    void	N();
    void	S( const char* );
    void	SN( const char* );
};

////////
//
// Printing "macros"
//
////////

inline void ContactSheet::S(
    const char* string
    )
{
    fprintf( myOutputFile, "%s ", string );
}

inline void ContactSheet::D(
    int n
    )
{
    fprintf( myOutputFile, "%d ", n );
}

inline void ContactSheet::N(
    )
{
    fprintf( myOutputFile, "\n" );
}

inline void ContactSheet::SN(
    const char* string
    )
{
    S( string );
    N();
}

////////
//
// Constructor
//
////////

ContactSheet::ContactSheet(
    FILE*	outputFile,	
    int		imageCount,	
    float	imageAspect,
    int		pageWidth,	
    int		pageHeight,	
    int		margin,		
    int		labelFontSize,	
    int		pageFontSize,	
    int		gap,
    int		columnCount,
    Boolean	grayscale
    )
{
    //
    // Store the settings from the client.
    //

    myOutputFile	= outputFile;
    myImageCount	= imageCount;
    myImageAspect	= imageAspect;
    myPageWidth		= pageWidth;
    myPageHeight	= pageHeight;
    myMargin		= margin;
    myLabelSize		= labelFontSize;
    myPagenumSize	= pageFontSize;
    myGap		= gap;
    myColumnCount	= columnCount;
    myGrayscale		= grayscale;
    
    //
    // Compute the derived values.
    //

    this->computeSizes();
    
    //
    // Initialize printing state.
    //
    
    myPageStarted	= FALSE;
    myImageIndex	= 0;

    //
    // Send the prolog.  (Function definitions, etc.)
    //

    this->prolog();
}

////////
//
// Destructor
//
////////

ContactSheet::~ContactSheet(
    )
{
    //
    // Sanity check.
    //

    assert( myImageIndex <= myImageCount );
    
    //
    // If we're in the middle of a page, finish it.
    //

    if ( myPageStarted ) {
	this->finishPage();
    }
    
    //
    // Send the stuff that goes at the end of the file.
    //

    SN( "%%EOF" );
}

////////
//
// computeSizes
//
////////

void ContactSheet::computeSizes(
    )
{
    //
    // The printable area on the page is the area inside the margins.
    //

    myPrintableWidth	= myPageWidth - myMargin * 2;
    myPrintableHeight	= myPageHeight - myMargin * 2;
    
    //
    // The printable part is evenly divided among the columns of
    // "cells".  Each cell includes an image, its label, and some
    // whitespace around them.
    //

    myCellWidth		= myPrintableWidth / myColumnCount;

    //
    // There is a fixed gap between the images.  The remaining space
    // is available for images.
    //

    myImageWidth	= myCellWidth - myGap;

    //
    // The image height is derived from the width and the aspect ratio
    // of the images to be displayed.
    //

    myImageHeight	= int( float(myImageWidth) / myImageAspect );

    //
    // Each cell must be tall enough to hold the image and the label.
    //

    myCellHeight	= myImageHeight + int( 1.5 * myGap ) + myLabelSize;

    //
    // The label at the bottom includes a gap.
    //

    myPageLabelHeight	= myPagenumSize + myGap;

    //
    // We use as many rows as will fit.
    //

    myRowCount		= (myPrintableHeight - myPageLabelHeight) / 
	                  myCellHeight;

    //
    // Space is the added so that the rows will fill the page.
    //

    myExtraGap		= ( myPrintableHeight - myPageLabelHeight - 
			    myRowCount * myCellHeight ) / myRowCount;

    //
    // Now we know how many images there will be per page, and how
    // many pages will be printed.
    //

    myCellsPerPage	= myRowCount * myColumnCount;
    myPageCount		= ( myImageCount + myCellsPerPage - 1 ) / 
	                  myCellsPerPage;
}

////////
//
// dumpByte
//
////////

static const char* hex = "0123456789abcdef";

inline void ContactSheet::dumpByte(
    unsigned char byte
    )
{
    fprintf( myOutputFile, "%c%c", hex[ byte >> 4 ], hex[ byte & 0x0f ] );
}


////////
//
// dumpImage
//
////////

void ContactSheet::dumpImage(
    int		width,
    int		height,
    const void* buffer
    )
{
    //
    // Dump out the pixels in hex: R, G, B.  Limit lines to about 60 chars.
    //
    
    int pixelCount = width * height;
    int pos = 0;
    unsigned char* ptr = (unsigned char*) buffer;
    for ( int i = 0;  i < pixelCount;  i++ ) {
	unsigned char x = *(ptr++);
	unsigned char b = *(ptr++);
	unsigned char g = *(ptr++);
	unsigned char r = *(ptr++);
 	if ( myGrayscale ) {
 	    unsigned char avg = 
 		(unsigned char) ( ( int(r) + int(g) + int(b) ) / 3 );
 	    dumpByte( avg );
 	    pos += 2;
 	}
 	else {
 	    dumpByte( r );
 	    dumpByte( g );
 	    dumpByte( b );
 	    pos += 6;
 	}
	if ( 60 < pos ) {
	    fprintf( myOutputFile, "\n" );
	    pos = 0;
	}
    }
    if ( pos != 0 ) {
	fprintf( myOutputFile, "\n" );
    }
}

////////
//
// finishPage
//
////////

void ContactSheet::finishPage(
    )
{
    SN( "showpage" );
    N();
    
    myPageStarted = FALSE;
}

////////
//
// prolog
//
////////

void ContactSheet::prolog(
    )
{
    //
    // Get the current time.
    //
    
    time_t now = time( NULL );
    const char* date = ctime( &now );
    
    SN( "%!PS-Adobe-2.0" );
    S ( "%%BoundingBox:" );
    	D(myMargin);  D(myMargin);
    	D(myPageWidth-myMargin);  D(myPageHeight-myMargin); N();
    SN( "%%Title: Movie Scenes" );
    fprintf( myOutputFile, "%%%%CreationDate: %s", date );
    S ( "%%Pages:" );  D( myPageCount );  N();
    SN( "%%EndComments" );
    SN( "%%BeginProlog" );
    SN( "%%BeginResource ShowcaseResource" );
    SN( "" );
    SN( "/bwproc {" );
    SN( "	rgbproc" );
    SN( "	dup length 3 idiv string 0 3 0" );
    SN( "	5 -1 roll {" );
    SN( "	add 2 1 roll 1 sub dup 0 eq" );
    SN( "	{ pop 3 idiv 3 -1 roll dup 4 -1 roll dup" );
    SN( "	  3 1 roll 5 -1 roll put 1 add 3 0 }" );
    SN( "	{ 2 1 roll } ifelse" );
    SN( "	} forall" );
    SN( "	pop pop pop" );
    SN( "} def" );
    SN( "" );
    SN( "systemdict /colorimage known not {" );
    SN( "	/colorimage {" );
    SN( "		pop" );
    SN( "		pop" );
    SN( "		/rgbproc exch def" );
    SN( "		{ bwproc } image" );
    SN( "	} def" );
    SN( "} if" );
    SN( "" );
    SN( "/hexgrayimage {" );
    SN( "  /ny exch def" );
    SN( "  /nx exch def" );
    SN( "  gsave" );
    SN( "  nx ny 8" );
    SN( "  % [nx 0 0 ny neg 0 ny]  % This inverts the image" );
    SN( "  [nx 0 0 ny 0 0]" );
    SN( "  /istr nx string def" );
    SN( "  {currentfile istr readhexstring pop}" );
    SN( "  image" );
    SN( "  grestore" );
    SN( "} bind def" );
    SN( "" );
    SN( "/hexrgbimage {" );
    SN( "  /ny exch def" );
    SN( "  /nx exch def" );
    SN( "  gsave" );
    SN( "  nx ny 8" );
    SN( "  % [nx 0 0 ny neg 0 ny]  % This inverts the image" );
    SN( "  [nx 0 0 ny 0 0]" );
    SN( "  /istr nx 3 mul string def" );
    SN( "  {currentfile istr readhexstring pop}" );
    SN( "  false 3" );
    SN( "  colorimage" );
    SN( "  grestore" );
    SN( "} bind def" );
    SN( "" );
    SN( "%" );
    SN( "% This document assumes 8.5 x 11 paper." );
    SN( "% Rescale the entire document to fit on" );
    SN( "% whatever paper we are printing on." );
    SN( "%" );
    SN( "" );
    SN( "/scalepage {" );
    SN( "newpath clippath pathbbox" );
    SN( "/URy exch def" );
    SN( "/URx exch def" );
    SN( "/LLy exch def" );
    SN( "/LLx exch def" );
    SN( "/Width  URx LLx sub 0.005 sub def" );
    SN( "/Height URy LLy sub 0.005 sub def" );
    SN( "LLx LLy translate" );
    SN( "Width 612 div Height 792 div gt" );
    SN( "    { /Y_size Height def" );
    SN( "      /X_size 612 792 div Y_size mul def" );
    SN( "      /Scale Height 792 div def }" );
    SN( "    { /X_size Width def" );
    SN( "      /Y_size 792 612 div X_size mul def" );
    SN( "      /Scale Width 612 div def } ifelse" );
    SN( "Width  X_size sub 2 div" );
    SN( "Height Y_size sub 2 div translate" );
    SN( "Scale Scale scale" );
    SN( "} bind def" );
    SN( "" );
    SN( "/centertext {" );
    SN( "    dup stringwidth pop 2 div neg 0 rmoveto" );
    SN( "    show" );
    SN( "} bind def" );
    SN( "" );
    SN( "/pagenumfont " );
    S("    /Times-Roman findfont" ); D( myPagenumSize ); SN( "scalefont" );
    SN( "    def" );
    SN( "" );
    SN( "/timecodefont" );
    S ( "    /Courier findfont" );  D( myLabelSize );  SN( "scalefont" );
    SN( "    def" );
    SN( "" );
    SN( "%%EndResource" );
    SN( "%%EndProlog" );
    SN( "%%BeginSetup" );
    SN( "%%EndSetup" );
}

////////
//
// printImage
//
////////

void ContactSheet::printImage(
    const char*	label,	// The string that will appear under
			// the image.  Can be NULL.
    int		width,  // Width of the image, in pixels
    int		height,	// Height of the image, in pixels
    const void*	buffer	// the pixels, left-to-right,
			// bottom-to-top, XBGR.
    )
{
    //
    // Figure out which page and cell we are printing.
    //

    int pageNum   = myImageIndex / myCellsPerPage;
    int cellIndex = myImageIndex % myCellsPerPage;
    int ix        = cellIndex % myColumnCount;
    int iy        = cellIndex / myColumnCount;
    
    //
    // Start a new page, if needed.
    //

    if ( ! myPageStarted ) {
	this->startPage( pageNum );
    }
    
    //
    // Find the location of this cell on the page.
    //

    int cellX = myCellWidth * ix;
    int cellY = myPageLabelHeight + (myRowCount - iy - 1) * myCellHeight +
	        myExtraGap * ( myRowCount - iy - 1 ) +
		myExtraGap / 2;
    
    //
    // Decide which command to use for printing
    //

    const char* imageCommand;
    if ( myGrayscale ) {
 	imageCommand = "hexgrayimage";
    }
    else {
 	imageCommand = "hexrgbimage";
    }
    
    //
    // Dump out the PostScript.
    //

    SN( "gsave" );
    D( cellX );  D( cellY );  SN( "translate" );
    SN( "gsave" );
    D( myGap / 2 );  D( myGap + myLabelSize ); SN( "translate" );
    D( myImageWidth );  D( myImageHeight );  SN( "scale" );
    D( width );  D( height );  SN( imageCommand );
    this->dumpImage( width, height, buffer );
    SN( "grestore" );
    SN( "timecodefont setfont" );
    D( myCellWidth / 2 );  D( myGap / 2 );  SN( "moveto" );
    fprintf( myOutputFile, "(%s) centertext\n", label );
    SN( "grestore" );
    
    //
    // If we just did the last cell on the page, finish off the page.
    //

    if ( cellIndex == myCellsPerPage - 1 ) {
	this->finishPage();
    }

    //
    // Bump the counter for the next image.
    //

    myImageIndex++;

}

////////
//
// startPage
//
////////

void ContactSheet::startPage(
    int pageNumber
    )
{
    S( "%%Page: label" );  D( pageNumber + 1 );  N();
    SN( "scalepage" );
    D( myMargin );  D( myMargin );  SN( "translate" );
    SN( "pagenumfont setfont" );
    D( myPrintableWidth / 2 );  D( myGap / 2 );  SN( "moveto" );
    fprintf( myOutputFile, 
	     "(Page %d of %d) centertext\n",
	     pageNumber + 1, myPageCount );

    myPageStarted = TRUE;
}
    
////////
//
// Global variables (yecch).
//
// All size measurements are in points, except where noted.
//
////////

#include <dmedia/moviefile.h>
#include <string.h>
#include <dmedia/dm_timecode.h>

int		verbose;
int		columns;
int		grayscale;

const char*	pname;
MVid		movie;
MVid		imageTrack;
int		movieWidth;	// in pixels
int		movieHeight;	// in pixels
int		frames;		// number of frames in the movie.

int		bufferSize;	// size of movie frames
void*		buffer;		// buffer used to hold movie frames


////////
//
// openMovie
//
////////

void openMovie(
    const char* fileName
    )
{
    //
    // Open the movie.
    //

    DMstatus status;
    status = mvOpenFile( fileName, O_RDONLY, &movie );
    if ( status != DM_SUCCESS ) {
	fprintf( stderr, "%s: Could not open movie file: %s\n", 
		 pname, fileName );
	exit( 1 );
    }
    
    //
    // Find the image track.
    //

    status = mvFindTrackByMedium( movie, DM_IMAGE, &imageTrack );
    if ( status != DM_SUCCESS ) {
	fprintf( stderr, "%s: Movie does not have an image track.\n", pname );
	exit( 1 );
    }
	
    if ( mvGetImagePacking( imageTrack ) != DM_PACKING_RGBX ) {
	fprintf( stderr, "%s: Only RGBX movies are supported.\n", pname );
	exit( 1 );
    }
    
    if ( mvGetImageOrientation( imageTrack ) != DM_BOTTOM_TO_TOP ) {
	fprintf( stderr, "%s: Only bottom-to-top movies are supported.\n", 
		 pname );
	exit( 1 );
    }
    
    //
    // Get the image size
    //

    frames	= mvGetTrackLength( imageTrack );
    movieWidth  = mvGetImageWidth ( imageTrack );
    movieHeight = mvGetImageHeight( imageTrack );
    bufferSize  = dmImageFrameSize( mvGetParams( imageTrack ) );
    buffer      = malloc( bufferSize );
    
    if ( buffer == NULL ) {
	fprintf( stderr, "%s: Could not allocate memory for buffer.\n",
		 pname );
	exit( 1 );
    }
    
    if ( bufferSize != movieWidth * movieHeight * 4 ) {
	fprintf( stderr, "%s: Buffer size confusion.\n", pname );
	exit( 1 );
    }
}

////////
//
// usage
//
////////

void usage(
    )
{
    fprintf( stderr,
	     "Usage: %s [-v] [-gray] [-columns n] movieFile > postscriptFile\n", 
	     pname );
    fprintf( stderr, "\n" );
    fprintf( stderr, "    -v           verbose output to stderr\n" );
    fprintf( stderr, "    -gray        print in grayscale (default is color)\n" );
    fprintf( stderr, "    -columns n   specify the number of columns of\n" );
    fprintf( stderr, "                 images on each page.\n" );
    fprintf( stderr, "\n" );
    
    exit( 1 );
}

////////
//
// printMovie
//
////////

void printMovie(
    ContactSheet*	printer
    )
{
    MVtime 	cursor 		= 0;
    MVtimescale timescale 	= 2997;
    MVtime	frameEnd;
    
    //
    // Set up the parameters that describe the image format we want from
    // the movie library.
    //

    DMparams* imageParams;
    dmParamsCreate( &imageParams );
    dmSetImageDefaults( imageParams, movieWidth, movieHeight,
    			DM_IMAGE_PACKING_XBGR );

    while ( cursor < mvGetTrackDuration( imageTrack, timescale ) )
    {
	DMstatus status =
	    mvGetTrackBoundary(
		imageTrack,
		cursor,
		timescale,
		MV_FORWARD,
		MV_FRAME,
		&frameEnd
		);
	assert( status == DM_SUCCESS );

	DMtimecode timecode;
	status = dmTCFromSeconds( 
	    &timecode, 
	    DM_TC_2997_4FIELD_DROP, 
	    double(cursor) / double(timescale)
	    );
	char timestring[64];
	assert( status == DM_SUCCESS );
	dmTCToString( timestring, &timecode );
	fprintf( stderr, "Timecode: %s\n", timestring );

	status = mvRenderMovieToImageBuffer(
	    movie,
	    (cursor + frameEnd) / 2,
	    timescale,
	    buffer,
	    imageParams
	    );
	assert( status == DM_SUCCESS );
	
	printer->printImage( timestring, movieWidth, movieHeight, buffer );

	cursor = frameEnd + 1;
    }
}

////////
//
// main
//
////////

int main(
    int		 argc,
    const char** argv
    )
{
    //
    // Save the program name
    //

    pname = argv[0];
    
    //
    // Default settings.
    //
    
    verbose		= 0;
    columns		= 5;
    
    //
    // Process arguments.
    //

    if ( argc < 2 ) {
	usage();
    }
    {
	for ( int i = 1;  i < argc - 1;  i++ ) {
	    if ( strcmp( argv[i], "-columns" ) == 0 ) {
		if ( argc - 2 <= i ) {
		    usage();
		}
		columns = atoi( argv[++i] );
	    }
	    else if ( strcmp( argv[i], "-gray" ) == 0 ) {
		grayscale = 1;
	    }
	    else if ( strcmp( argv[i], "-v" ) == 0 ) {
		verbose = 1;
	    }
	    else {
		usage();
	    }
	}
    }
    if ( argv[argc-1][0] == '-' ) {
	usage();
    }

    //
    // Open the input movie.
    //

    openMovie( argv[argc-1] );
    
    //
    // Generate PostScript
    //
    
    {
	float aspect = float(movieWidth) / float(movieHeight);
	
	ContactSheet printer(
	    stdout,		// where the PostScript goes
	    frames,		// how many images will be printed
	    aspect,
	    8.5 * 72,		// page width
	    11 * 72,		// page height
	    0.5 * 72,		// margin
	    7,			// font size for labels
	    12,			// font size for page numbers
	    0.2 * 72,		// gap between images
	    columns,            // number of columns
	    grayscale
	    );
	
	printMovie( &printer );
    }
}


