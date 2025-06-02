
//
// snapmovie.c
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define XK_MISCELLANY
#include <X11/keysymdef.h>
#include <Xm/Xm.h>
#include <X11/extensions/readdisplay.h>
#include <X11/Xmd.h>
#include <Xm/Form.h>
#include <Xm/FileSB.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/Label.h>
#include <Xm/TextF.h>
#include <movie.h>
#include <il2.5/ilImage.h>
#include <il2.5/ilConfigure.h>
#include <il2.5/ilABGRImg.h>
#include <il2.5/ilXImage.h>
#include <gl/glws.h>
#include "XLayerUtil.h"
#include "debug.h"

//
// globals
//

XtAppContext	AppContext;
String DefaultResources[] = {
    "*sgiMode: true",
    "*useSchemes: all",
    NULL,
};
Widget		wTopLevel;
Widget		wMainWindow;
Widget		wShowHideButton;	// ugly ugly

#define DEFAULT_FRAME_RATE 15.0
#define DEFAULT_SNAP_TIME  0.5

//
// file scope
//
static MVid		SnapMovie = 0;
static MVid		ImageId = 0;
static MVfileformat	FileFormat = MV_FORMAT_SGI_3;
static char		*ImageCompScheme = DM_IMAGE_UNCOMPRESSED;
static float		fFrameRate = DEFAULT_FRAME_RATE;
static float		fSnapTime = DEFAULT_SNAP_TIME;
static char		*pBuf = NULL;
static int		nBufSize = 0;
static int		nAccFrame = 0;
static Cursor		GrabCursor;
static int		nSnapStartX = -1, nSnapStartY = -1, nSnapWidth = -1, nSnapHeight = -1;
static Window		wOverlayWin = 0;
static GC		gcOverlay;
static Boolean		bShowing = False;

//
// internal function prototypes
//

void ShowCB( Widget widget, XtPointer pClientData, XtPointer pCallData );
void HideCB( Widget widget, XtPointer pClientData, XtPointer pCallData );

//
// SaveMovieFrame
//
void SaveMovieFrame( ilImage *pImg )
{
    if (!pImg || !SnapMovie)
    {
	Debugf( "Invalid Movie or ilImage" );
	return;
    }
    if (!ImageId)
    {
	DMparams	*pImgParams;
	if  (mvFindTrackByMedium( SnapMovie, DM_IMAGE, &ImageId) == DM_SUCCESS)
	{
	    Debugf( "Image Track Already Exist, recreate Movie" );
	    return;
	}
	if (dmParamsCreate( &pImgParams ) != DM_SUCCESS)
	{
	    Debugf( "cannot create dmparams" );
	    return;
	}
	if (dmSetImageDefaults( pImgParams, nSnapWidth, nSnapHeight, DM_PACKING_RGBX )
	    != DM_SUCCESS)
	{
	    Debugf( "cannot set Image Defaults" );
	    return;
	}
	if (dmParamsSetString( pImgParams, DM_IMAGE_COMPRESSION, ImageCompScheme )
	    != DM_SUCCESS)
	{
	    Debugf( "cannot set image compression scheme" );
	    return;
	}
	if (dmParamsSetFloat( pImgParams, DM_IMAGE_RATE, fFrameRate ) != DM_SUCCESS)
	{
	    Debugf( "cannot set image rate" );
	    return;
	}
	
	if (mvAddTrack( SnapMovie, DM_IMAGE, pImgParams, pImgParams, &ImageId ) != DM_SUCCESS)
	{
	    Debugf( "cannot Add Image Track" );
	    return;
	}
	if (pBuf) free( pBuf );
	pBuf = (char *)malloc( nBufSize = nSnapWidth * nSnapHeight * 4 );
	if (!pBuf)
	{
	    Debugf( "Out of memory.  Nothing will work...Quit Now!" );
	    return;
	}
	#if defined( REDUNDANT_DEBUG )
	Debugf( "Track Added Successfully" );
	#endif
	dmParamsDestroy( pImgParams );
    }

    pImg->getTile( 0, 0, pImg->getXsize(), pImg->getYsize(), pBuf );
    ilFlushCache();

    {
	DMparams *pImgParams;

	dmParamsCreate( &pImgParams );
	dmSetImageDefaults( pImgParams, 
			    pImg->getXsize(), pImg->getYsize(), 
			    DM_IMAGE_PACKING_XBGR );
	mvInsertFramesAtTime( ImageId, 
			      mvGetTrackDuration( ImageId, 600 ), 
			      (long long)(fSnapTime * 600), 600,
			      pBuf, nBufSize, pImgParams, 0 );
	dmParamsDestroy( pImgParams );
    }
			  

    #if defined( REDUNDANT_DEBUG )
    Debugf( "Saved Movie Frame <%d>", nAccFrame-1 );
    #endif
}


//
// CloseMovie
//
Boolean CloseMovie( void )
{
    if (wOverlayWin)
	XUnmapWindow( XtDisplay( wTopLevel ), wOverlayWin );
    if (mvWrite( SnapMovie ) != DM_SUCCESS)
	goto Abort;
    if (mvClose( SnapMovie ) != DM_SUCCESS)
	goto Abort;
    if (pBuf) free( pBuf ); pBuf = NULL;
    nBufSize = 0;
    nAccFrame = 0;
    ImageId = 0;
    SnapMovie = 0;
    return( True );
  Abort:
    Debugf( "CloseMovie Failed" );
    return( False );
}

//
// DrawRubberband
//
void DrawRubberband( int nStartX, int nStartY, int nEndX, int nEndY )
{
    static int	nLastX = -1, nLastY = -1, nLastW = -1, nLastH = -1;
    Display	*pDisplay = XtDisplay( wTopLevel );
    int		x, y, w, h;

    if (nStartX < 0)
    {
	nLastX = nLastY = nLastW = nLastH = -1;
	return;
    }
    x = (nStartX <= nEndX) ? nStartX : nEndX;
    y = (nStartY <= nEndY) ? nStartY : nEndY;
    w = nStartX - nEndX;
    h = nStartY - nEndY;
    w = (w > 0) ? w : -w;
    h = (h > 0) ? h : -h;

    if (wOverlayWin)
    {	
	if (nLastX >= 0 && nLastY >= 0 && nLastW >= 0 && nLastH >= 0)
	    XDrawRectangle( pDisplay, wOverlayWin, gcOverlay,
			    nLastX, nLastY, nLastW, nLastH );
	nLastX = (x-1 > 0) ? (x-1) : 0;
	nLastY = (y-1 > 0) ? (y-1) : 0;
	XDrawRectangle( pDisplay, wOverlayWin, gcOverlay,
		        nLastX, nLastY, nLastW = w+2, nLastH = h+2 );
    }
}


//
// ShowRubberband - show rubberband
//
void ShowRubberband( Widget, XtPointer pClientData, XtPointer )
{
    if (bShowing) return;
    ShowCB( Widget(pClientData), NULL, NULL );
}

//
// HideRubberband - show rubberband
//
void HideRubberband( Widget, XtPointer pClientData, XtPointer )
{
    if (!bShowing) return;
    HideCB( Widget( pClientData ), NULL, NULL );
}


//
// DoneRubberband - done when button is released
//
Boolean DoneRubberband( Widget widget )
{
    static int		nGrabStartX = -1, nGrabStartY = -1;
    static int		nGrabCurrentX = -1, nGrabCurrentY = -1;
    int		nGrabEndX = -1, nGrabEndY = -1;
    Display	*pDisplay = XtDisplay( wMainWindow );
    Window	win = XtWindow( widget );
    XEvent	event;

    XFlush( pDisplay );
    XmUpdateDisplay( wTopLevel );
    
    while (XCheckMaskEvent( pDisplay, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, &event ))
    {
	if (event.type == ButtonPress)
	{
//	    Debugf( "Button Down! at <%d,%d>", event.xbutton.x_root, event.xbutton.y_root );
	    nGrabStartX = nGrabCurrentX = event.xbutton.x_root;
	    nGrabStartY = nGrabCurrentY = event.xbutton.y_root;

	    if (wOverlayWin) 
	    {
		XMapWindow( pDisplay, wOverlayWin );
		XClearWindow( pDisplay, wOverlayWin );
	    }
	    return( False );
	}
	else if (event.type == ButtonRelease)
	{
//	    Debugf( "Button Up! at <%d,%d>", event.xbutton.x_root, event.xbutton.y_root );
	    nGrabEndX = nGrabCurrentX = event.xbutton.x_root;
	    nGrabEndY = nGrabCurrentY = event.xbutton.y_root;

	    // get parameters for reading display
	    nSnapStartX = (nGrabStartX <= nGrabEndX) ? nGrabStartX : nGrabEndX;
	    nSnapStartY = (nGrabStartY <= nGrabEndY) ? nGrabStartY : nGrabEndY;
	    nSnapWidth = nGrabStartX - nGrabEndX;
	    nSnapHeight = nGrabStartY - nGrabEndY;
	    if (nSnapWidth < 0) nSnapWidth = -nSnapWidth;
	    if (nSnapHeight < 0) nSnapHeight = -nSnapHeight;
	    DrawRubberband( nGrabStartX, nGrabStartY, nGrabEndX, nGrabEndY );
	    nGrabCurrentX = nGrabCurrentY = -1;
	    
	    if (wOverlayWin) 
	    {
		DrawRubberband( -1, -1, -1, -1 );
		XUnmapWindow( pDisplay, wOverlayWin );
	    }
	    return True;
	}
	else if (event.type == MotionNotify)
	{
	    // Debugf( "Button Moved! at <%d,%d>", event.xbutton.x_root, event.xbutton.y_root );
	    nGrabCurrentX = event.xbutton.x_root;
	    nGrabCurrentY = event.xbutton.y_root;
	    DrawRubberband( nGrabStartX, nGrabStartY, nGrabCurrentX, nGrabCurrentY );
	    return( False );
	}
	else if (event.type == Expose && nGrabCurrentX >=0 && nGrabCurrentY >= 0 )
	{
	    DrawRubberband( nGrabStartX, nGrabStartY, nGrabCurrentX, nGrabCurrentY );
	}
    }
    
    return ( False );
}

//
// ChangeRubberband
//
Boolean ChangeRubberband( XKeyEvent keyEvent )
{
    Boolean	bAugment =  (keyEvent.state & ShiftMask) && (!ImageId);
    Boolean	bChanged = False;
    char	pcBuf[20];
    KeySym	key;
    XComposeStatus compose;

    // undraw the rect
    XDrawRectangle( XtDisplay( wTopLevel ), wOverlayWin, gcOverlay,
		    nSnapStartX-1, nSnapStartY-1, nSnapWidth+2, nSnapHeight+2 );
    XLookupString( &keyEvent, pcBuf, 20, &key, &compose );

    if (key == XK_Escape) 
    {
	HideCB( wShowHideButton, NULL, NULL );
	return( False );
    }

    switch( key )
    {
      case XK_Up:
	if (bAugment)
	    --nSnapHeight;
	else --nSnapStartY;
	bChanged = True;
	break;
      case XK_Down:
	if (bAugment)
	    ++nSnapHeight;
	else ++nSnapStartY;
	bChanged = True;
	break;
      case XK_Right:
	if (bAugment)
	    ++nSnapWidth;
	else ++nSnapStartX;
	bChanged = True;
	break;
      case XK_Left:
	if (bAugment)
	    --nSnapWidth;
	else --nSnapStartX;
	bChanged = True;
	break;
      default:
	break;
    }

    // draw the new/unmodified rect
    XDrawRectangle( XtDisplay( wTopLevel ), wOverlayWin, gcOverlay,
		    nSnapStartX-1, nSnapStartY-1, nSnapWidth+2, nSnapHeight+2 );

    return( bChanged );
}

//
// SetCB
//
void SetCB( Widget widget, XtPointer pClientData, XtPointer )
{
    Display	*pDisplay = XtDisplay( widget );
    if (!SnapMovie)
    {
	Debugf( "Create Movie File First!" );
	return;
    }
    if (ImageId)
    {
	Debugf( "Cannot reset size, Image track already created" );
	return;
    }
    if (XGrabPointer( pDisplay,
		      XtWindow( widget ),
		      False,
		      ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
		      GrabModeAsync, GrabModeAsync,
		      None,
		      GrabCursor,
		      CurrentTime ) == GrabSuccess)
    {	
	//Debugf( "Grab Pointer Successful" );
    }
    while( 1 )
    {
	sginap( 1 );
	if (DoneRubberband( widget ))
	    break;
    }
    XUngrabPointer( pDisplay, CurrentTime );
    ShowCB( (Widget)pClientData, NULL, NULL );
    HideCB( (Widget)pClientData, NULL, NULL );
//    Debugf( "Ungrabbed Pointer" );
}


//
// ShowCB
//
void ShowCB( Widget widget, XtPointer, XtPointer )
{
    if (!wOverlayWin || nSnapStartX < 0 || nSnapStartY < 0) return;
    XMapWindow( XtDisplay( wTopLevel ), wOverlayWin );
    XDrawRectangle( XtDisplay( wTopLevel ), wOverlayWin, gcOverlay,
		    nSnapStartX-1, nSnapStartY-1, nSnapWidth+2, nSnapHeight+2 );

    XtRemoveCallback( widget, XmNactivateCallback, ShowCB, NULL);
    XtAddCallback( widget, XmNactivateCallback, HideCB, NULL);

    XmString	tmpStr = XmStringCreateLocalized( "Hide" );
    XtVaSetValues( widget, XmNlabelString, tmpStr, NULL );
    XmStringFree( tmpStr );

    bShowing = True;
}

//
// HideCB
//
void HideCB( Widget widget, XtPointer, XtPointer )
{
    if (!wOverlayWin || nSnapStartX < 0 || nSnapStartY < 0) return;
    XUnmapWindow( XtDisplay( wTopLevel ), wOverlayWin );

    XtRemoveCallback( widget, XmNactivateCallback, HideCB, NULL);
    XtAddCallback( widget, XmNactivateCallback, ShowCB, NULL);

    XmString	tmpStr = XmStringCreateLocalized( "Show" );
    XtVaSetValues( widget, XmNlabelString, tmpStr, NULL );
    XmStringFree( tmpStr );

    bShowing = False;
}

//
// SnapCB
//
void SnapCB( Widget, XtPointer pClientData, XtPointer )
{
    if (nSnapStartX < 0 || nSnapStartY < 0 || nSnapWidth < 0 || nSnapHeight < 0)
    {
	Debugf( "Snap parameters not set" );
	return;
    }

    if (strcmp( ImageCompScheme, DM_IMAGE_QT_ANIM ) == 0
	|| strcmp( ImageCompScheme, DM_IMAGE_QT_VIDEO ) == 0
	|| strcmp( ImageCompScheme, DM_IMAGE_QT_CVID ) == 0)
    {
	nSnapWidth = nSnapWidth / 4 * 4;
	nSnapHeight = nSnapHeight / 4 * 4;
    }
    #if 0
    Debugf( "Snapping from <%d,%d>, for <%d,%d>", 
	    nSnapStartX, nSnapStartY, nSnapWidth, nSnapHeight );
    #endif

    XtManageChild( (Widget)pClientData );
}

//
// SnapTimeCB
//
void SnapTimeCB( Widget widget, XtPointer, XtPointer )
{
    Widget	wForm = XtParent( widget );
    XtUnmanageChild( wForm );
    
    WidgetList	pwChildren;
    int		nChildren;
    XtVaGetValues( wForm, XmNchildren, &pwChildren, XmNnumChildren, &nChildren, NULL );

    if (nChildren >= 3)
    {
	Widget	wTextField = pwChildren[1];
	String	sSnapTime;
	XtVaGetValues( wTextField, XmNvalue, &sSnapTime, NULL );

	if (sSnapTime != NULL)
	    fSnapTime = strtod( sSnapTime, NULL );
	if (fSnapTime <= 0)
	    fSnapTime = DEFAULT_SNAP_TIME;
    }

    
    XImage	*pXImage;
    Display	*pDisplay = XtDisplay( widget );
    Window	winRoot = RootWindow( pDisplay, DefaultScreen( pDisplay ) );
    unsigned long		nReturnHints;

    pXImage = XReadDisplay( pDisplay, winRoot, 
			    nSnapStartX, nSnapStartY, nSnapWidth, nSnapHeight,
			    XRD_READ_POINTER, 
			    &nReturnHints );
    if (pXImage)
    {
	//Debugf( "XReadDisplay successful" );
	ilXImage	*pilXImg = new ilXImage( pXImage, pDisplay );
	if (!pilXImg)
	{
	    Debugf( "cannot create ilXImage" );
	    return;
	}
	ilImage	*pilABGR = new ilABGRImg( pilXImg );
	if (!pilABGR)
	{
	    Debugf( "cannot create abgr image file" );
	    delete pilXImg;
	    return;
	}
	pilABGR->copyTile( 0, 0, pilABGR->getXsize(), pilABGR->getYsize(),
			   pilXImg, 0, 0 );
	ilFlushCache();

	SaveMovieFrame( pilABGR );

	delete pilABGR;
	delete pilXImg;
	XDestroyImage( pXImage );
    }
    else
	Debugf( "XReadDisplay failed" );

}

//
// CloseCB
//
void CloseCB( Widget, XtPointer, XtPointer )
{
    CloseMovie();
//    Debugf( "CloseMovie" );
}


//
// OpenCB
//
void OpenCB( Widget, XtPointer pClientData, XtPointer pCallData )
{
    XmFileSelectionBoxCallbackStruct    *pCbs = (XmFileSelectionBoxCallbackStruct *)pCallData;
    char        *pFileName = NULL;

    XtUnmanageChild( (Widget)pClientData );

    if (!XmStringGetLtoR (pCbs->value, XmFONTLIST_DEFAULT_TAG, &pFileName ))
	return;

    if (SnapMovie)
	if (!CloseMovie()) goto Abort;

    DMparams	*pDmParams;
    dmParamsCreate( &pDmParams );
    if (mvSetMovieDefaults( pDmParams, FileFormat ) != DM_SUCCESS)
	goto Abort;
    if (mvCreateFile( pFileName, pDmParams, NULL, &SnapMovie ) != DM_SUCCESS)
	goto Abort;
    dmParamsDestroy( pDmParams );

    return;

  Abort:
    if (pDmParams) dmParamsDestroy( pDmParams );
    Debugf( "Cannot Create Movie File: %s", mvGetErrorStr( mvGetErrno() ) );
    return;
}

//
// CancelCB
//
void CancelCB( Widget, XtPointer pClientData, XtPointer)
{
    XtUnmanageChild( (Widget)pClientData );
}

//
// CreateMovieCB
//
void CreateMovieCB( Widget, XtPointer pClientData, XtPointer )
{
    XtManageChild( (Widget)pClientData );
}

//
// QuitCB
//
void QuitCB( Widget, XtPointer, XtPointer )
{
    if (SnapMovie) CloseMovie();
    exit( 0 );
}

//
// FormatOptionsCB
//
void FormatOptionsCB( Widget, XtPointer pClientData, XtPointer pCallData )
{
    XmToggleButtonCallbackStruct *pState = (XmToggleButtonCallbackStruct *)pCallData;

    if (pState->set == FALSE) return;

    switch ((int)pClientData)
    {
      case 0:
	FileFormat = MV_FORMAT_SGI_3;
	return;
      case 1:
      default:
	FileFormat = MV_FORMAT_QT;
	return;
    }
}

//
// FormatOptionsPopDownCB
//
void FormatOptionsPopDownCB( Widget widget, XtPointer pClientData, XtPointer )
{
    Widget	*pList = (Widget *)pClientData;
    XtUnmanageChild( XtParent( widget ) );

    // figure out which option panel to start
    if (FileFormat == MV_FORMAT_SGI_3)
	XtManageChild( pList[0] );
    else XtManageChild( pList[1] );
}

//
// SGICompOptionsCB
//
void SGICompOptionsCB( Widget, XtPointer pClientData, XtPointer pCallData )
{
    XmToggleButtonCallbackStruct *pState = (XmToggleButtonCallbackStruct *)pCallData;

    if (pState->set == FALSE) return;

    switch ((int)pClientData)
    {
      case 1:
	ImageCompScheme = DM_IMAGE_RLE;
	return;
      case 2:
	ImageCompScheme = DM_IMAGE_RLE24;
	return;
      case 3:
	ImageCompScheme = DM_IMAGE_JPEG;
	return;
      case 4:
	ImageCompScheme = DM_IMAGE_MVC1;
	return;
      case 5:
	ImageCompScheme = DM_IMAGE_MVC2;
	return;
      default:
	ImageCompScheme = DM_IMAGE_UNCOMPRESSED;
	return;
    }
}

//
// SGICompOptionsPopDownCB
//
void SGICompOptionsPopDownCB( Widget widget, XtPointer pClientData, XtPointer )
{
    Widget	wForm = XtParent( widget );
    XtUnmanageChild( wForm );
    
    WidgetList	pwChildren;
    int		nChildren;
    XtVaGetValues( wForm, XmNchildren, &pwChildren, XmNnumChildren, &nChildren, NULL );

    if (nChildren >= 4)
    {
	Widget	wTextField = pwChildren[3];
	String	sFrameRate;
	XtVaGetValues( wTextField, XmNvalue, &sFrameRate, NULL );

	if (sFrameRate != NULL)
	    fFrameRate = strtod( sFrameRate, NULL );
	if (fFrameRate <= 0)
	    fFrameRate = DEFAULT_FRAME_RATE;
    }

    XtManageChild( (Widget)pClientData );
}

//
// QTCompOptionsCB
//
void QTCompOptionsCB( Widget, XtPointer pClientData, XtPointer pCallData )
{
    XmToggleButtonCallbackStruct *pState = (XmToggleButtonCallbackStruct *)pCallData;

    if (pState->set == FALSE) return;

    switch ((int)pClientData)
    {
      case 1:
	ImageCompScheme = DM_IMAGE_QT_ANIM;
	return;
      case 2:
	ImageCompScheme = DM_IMAGE_QT_VIDEO;
	return;
      case 3:
	ImageCompScheme = DM_IMAGE_JPEG;
	return;
      case 4:
	ImageCompScheme = DM_IMAGE_QT_CVID;
	return;
      default:
	ImageCompScheme = DM_IMAGE_UNCOMPRESSED;
	return;
    }
}

//
// QTCompOptionsPopDownCB
//
void QTCompOptionsPopDownCB( Widget widget, XtPointer pClientData, XtPointer )
{
    Widget	wForm = XtParent( widget );
    XtUnmanageChild( wForm );
    
    WidgetList	pwChildren;
    int		nChildren;
    XtVaGetValues( wForm, XmNchildren, &pwChildren, XmNnumChildren, &nChildren, NULL );

    if (nChildren >= 4)
    {
	Widget	wTextField = pwChildren[3];
	String	sFrameRate;
	XtVaGetValues( wTextField, XmNvalue, &sFrameRate, NULL );

	if (sFrameRate != NULL)
	    fFrameRate = strtod( sFrameRate, NULL );
	if (fFrameRate <= 0)
	    fFrameRate = DEFAULT_FRAME_RATE;
    }

    XtManageChild( (Widget)pClientData );
}

//
// SetPosInForm
//
void SetPosInForm( Widget widget, int nTop, int nBottom, int nLeft, int nRight )
{
    XtVaSetValues( widget,
                   XmNtopAttachment, XmATTACH_POSITION,
		   XmNtopPosition, nTop,
                   XmNbottomAttachment, XmATTACH_POSITION,
		   XmNbottomPosition, nBottom,
                   XmNleftAttachment, XmATTACH_POSITION,
		   XmNleftPosition, nLeft,
                   XmNrightAttachment, XmATTACH_POSITION,
		   XmNrightPosition, nRight,
		   NULL );
}


//
// CreateFileSel
//
Widget CreateFileSel( Widget wParent )
{
    Widget	wTmp;
    wTmp = XmCreateFileSelectionDialog( wParent, "Create Movie File", NULL, 0 );
    XtAddCallback( wTmp, XmNokCallback, OpenCB, wTmp );
    XtAddCallback( wTmp, XmNcancelCallback, CancelCB, wTmp );
    return( wTmp );
}

#define NUM_ROWS 4
//
// CreateFormatOptionPanel
//
Widget CreateFormatOptionPanel( Widget wParent, Widget *pPopup )
{
    Widget	wTmp;
    wTmp = XmCreateFormDialog( wParent, "Movie Format Options", NULL, 0 );
    XtVaSetValues( wTmp, 
		   XmNfractionBase, NUM_ROWS,
		   XmNlabelString, "Movie Format Options", 
		   NULL );

    Widget	wCompLabel = XmCreateLabel( wTmp, "Movie Format", NULL, 0 );
    SetPosInForm( wCompLabel, 0, 1, 0, NUM_ROWS );
    XtManageChild( wCompLabel );
    
    XmString	xstrSGI = XmStringCreateLocalized( "SGI format 3" );
    XmString	xstrQT = XmStringCreateLocalized( "QuickTime" );

    Widget	wFormatOptions 
	= XmVaCreateSimpleRadioBox( wTmp, "Movie Format", 0, FormatOptionsCB,
				    XmVaRADIOBUTTON, xstrSGI, 'S', NULL, NULL,
				    XmVaRADIOBUTTON, xstrQT, 'Q', NULL, NULL,
				    NULL );
    SetPosInForm( wFormatOptions, 1, 3, 0, NUM_ROWS );
    XtManageChild( wFormatOptions );
    XmStringFree( xstrSGI );
    XmStringFree( xstrQT );

    Widget	wOk = XtVaCreateManagedWidget( "OK!", xmPushButtonWidgetClass, wTmp, NULL );
    SetPosInForm( wOk, NUM_ROWS-1, NUM_ROWS, 0, NUM_ROWS );
    XtAddCallback( wOk, XmNactivateCallback, FormatOptionsPopDownCB, pPopup );

    return( wTmp );
}
#undef NUM_ROWS

#define NUM_ROWS 10
//
// CreateSGIOptionPanel
//
Widget CreateSGIOptionPanel( Widget wParent, Widget wNextPopup )
{
    Widget	wTmp;
    wTmp = XmCreateFormDialog( wParent, "Options", NULL, 0 );
    XtVaSetValues( wTmp,
		   XmNfractionBase, NUM_ROWS,
		   XmNlabelString, "Format Options",
		   NULL );

    Widget	wCompLabel = XmCreateLabel( wTmp, "Compression Format", NULL, 0 );
    SetPosInForm( wCompLabel, 0, 1, 0, NUM_ROWS );
    XtManageChild( wCompLabel );
    
    // compression schemes are found in /usr/include/dmedia/dm_image.h
    XmString	xstrUN = XmStringCreateLocalized( "Uncompressed" );
    XmString	xstrRLE = XmStringCreateLocalized( "RLE - Run Length Encode 8-bit" );
    XmString	xstrRLE24 = XmStringCreateLocalized( "RLE - Run Length Encode 24-bit" );
    XmString	xstrJPEG = XmStringCreateLocalized( "JPEG" );
    XmString	xstrMVC1 = XmStringCreateLocalized( "SGI Motion Video 1" );
    XmString	xstrMVC2 = XmStringCreateLocalized( "SGI Motion Video 2" );
    Widget	wSGICompOptions 
	= XmVaCreateSimpleRadioBox( wTmp, "Movie Format", 0, SGICompOptionsCB,
				    XmVaRADIOBUTTON, xstrUN, 'U', NULL, NULL,
				    XmVaRADIOBUTTON, xstrRLE, 'R', NULL, NULL,
				    XmVaRADIOBUTTON, xstrRLE24, 'L', NULL, NULL,
				    XmVaRADIOBUTTON, xstrJPEG, 'J', NULL, NULL,
				    XmVaRADIOBUTTON, xstrMVC1, '1', NULL, NULL,
				    XmVaRADIOBUTTON, xstrMVC2, '2', NULL, NULL,
				    NULL );
    SetPosInForm( wSGICompOptions, 1, 8, 0, NUM_ROWS );
    XtManageChild( wSGICompOptions );
    XmStringFree( xstrUN );
    XmStringFree( xstrRLE );
    XmStringFree( xstrRLE24 );
    XmStringFree( xstrJPEG );
    XmStringFree( xstrMVC1 );
    XmStringFree( xstrMVC2 );

    Widget	wFrameRateLabel = XmCreateLabel( wTmp, "Frame Rate:", NULL, 0 );
    SetPosInForm( wFrameRateLabel, 8, 9, 0, NUM_ROWS/2 );
    XtManageChild( wFrameRateLabel );
    
    Widget	wFrameRateText = XmCreateTextField( wTmp, "Rate", NULL, 0 );
    SetPosInForm( wFrameRateText, 8, 9, NUM_ROWS/2, NUM_ROWS-1 );
    XtVaSetValues( wFrameRateText, XmNvalue, "15", NULL );
    XtManageChild( wFrameRateText );

    Widget	wOk = XtVaCreateManagedWidget( "OK!", xmPushButtonWidgetClass, wTmp, NULL );
    SetPosInForm( wOk, NUM_ROWS-1, NUM_ROWS, 0, NUM_ROWS );
    XtAddCallback( wOk, XmNactivateCallback, SGICompOptionsPopDownCB, wNextPopup );

    return( wTmp );
}
#undef NUM_ROWS


#define NUM_ROWS 10
//
// CreateQTOptionPanel
//
Widget CreateQTOptionPanel( Widget wParent, Widget wNextPopup )
{
    Widget	wTmp;
    wTmp = XmCreateFormDialog( wParent, "Options", NULL, 0 );
    XtVaSetValues( wTmp,
		   XmNfractionBase, NUM_ROWS,
		   XmNlabelString, "Format Options",
		   NULL );

    Widget	wCompLabel = XmCreateLabel( wTmp, "Compression Format", NULL, 0 );
    SetPosInForm( wCompLabel, 0, 1, 0, NUM_ROWS );
    XtManageChild( wCompLabel );
    
    // compression schemes are found in /usr/include/dmedia/dm_image.h
    XmString	xstrUN = XmStringCreateLocalized( "Uncompressed" );
    XmString	xstrANIM = XmStringCreateLocalized( "Animation" );
    XmString	xstrRPZA = XmStringCreateLocalized( "Road Pizza" );
    XmString	xstrJPEG = XmStringCreateLocalized( "JPEG" );
    XmString	xstrCVID = XmStringCreateLocalized( "Cinepak" );
    Widget	wQTCompOptions 
	= XmVaCreateSimpleRadioBox( wTmp, "Movie Format", 0, QTCompOptionsCB,
				    XmVaRADIOBUTTON, xstrUN, 'U', NULL, NULL,
				    XmVaRADIOBUTTON, xstrANIM, 'A', NULL, NULL,
				    XmVaRADIOBUTTON, xstrRPZA, 'R', NULL, NULL,
				    XmVaRADIOBUTTON, xstrJPEG, 'J', NULL, NULL,
				    XmVaRADIOBUTTON, xstrCVID, 'C', NULL, NULL,
				    NULL );
    SetPosInForm( wQTCompOptions, 1, 8, 0, NUM_ROWS );
    XtManageChild( wQTCompOptions );
    XmStringFree( xstrUN );
    XmStringFree( xstrANIM );
    XmStringFree( xstrRPZA );
    XmStringFree( xstrJPEG );
    XmStringFree( xstrCVID );

    Widget	wFrameRateLabel = XmCreateLabel( wTmp, "Frame Rate:", NULL, 0 );
    SetPosInForm( wFrameRateLabel, 8, 9, 0, NUM_ROWS/2 );
    XtManageChild( wFrameRateLabel );
    
    Widget	wFrameRateText = XmCreateTextField( wTmp, "Rate", NULL, 0 );
    SetPosInForm( wFrameRateText, 8, 9, NUM_ROWS/2, NUM_ROWS-1 );
    XtVaSetValues( wFrameRateText, XmNvalue, "15", NULL );
    XtManageChild( wFrameRateText );

    Widget	wOk = XtVaCreateManagedWidget( "OK!", xmPushButtonWidgetClass, wTmp, NULL );
    SetPosInForm( wOk, NUM_ROWS-1, NUM_ROWS, 0, NUM_ROWS );
    XtAddCallback( wOk, XmNactivateCallback, QTCompOptionsPopDownCB, wNextPopup );

    return( wTmp );
}
#undef NUM_ROWS

#define NUM_ROWS 2
//
// CreateSnapTimePanel
//
Widget CreateSnapTimePanel( Widget wParent )
{
    Widget	wTmp;
    wTmp = XmCreateFormDialog( wParent, "Snap Time", NULL, 0 );
    XtVaSetValues( wTmp,
		   XmNfractionBase, NUM_ROWS,
		   XmNlabelString, "SnapTime Panel",
		   NULL );

    Widget	wSnapTimeLabel = XmCreateLabel( wTmp, "Enter Snap Time:", NULL, 0 );
    SetPosInForm( wSnapTimeLabel, 0, 1, 0, 1 );
    XtManageChild( wSnapTimeLabel );
    
    Widget	wSnapTimeText = XmCreateTextField( wTmp, "Snap Time", NULL, 0 );
    SetPosInForm( wSnapTimeText, 0, 1, 1, 2 );
    XtVaSetValues( wSnapTimeText, XmNvalue, "0.5", NULL );
    XtManageChild( wSnapTimeText );

    Widget	wOk = XtVaCreateManagedWidget( "OK!", xmPushButtonWidgetClass, wTmp, NULL );
    SetPosInForm( wOk, 1, 2, 0, 2 );
    XtAddCallback( wOk, XmNactivateCallback, SnapTimeCB, NULL );

    return( wTmp );
}
#undef NUM_ROWS

//
// CreateMainWindow
//
Widget CreateMainWindow( Widget wParent )
{
    Widget	wForm = XtVaCreateManagedWidget( "Form", xmFormWidgetClass, wParent, 
						 XmNfractionBase, 6,
						 NULL );
    // create the 6 buttons
    Widget	wCreate = XtVaCreateManagedWidget( "Create", xmPushButtonWidgetClass, wForm, NULL );
    Widget	wClose = XtVaCreateManagedWidget( "Close", xmPushButtonWidgetClass, wForm, NULL );
    Widget	wSnap = XtVaCreateManagedWidget( "Snap", xmPushButtonWidgetClass, wForm, NULL );
    Widget	wSet = XtVaCreateManagedWidget( "Set", xmPushButtonWidgetClass, wForm, NULL );
    Widget	wShowHide = XtVaCreateManagedWidget( "Show", xmPushButtonWidgetClass, wForm, NULL );
    Widget	wQuit = XtVaCreateManagedWidget( "Quit", xmPushButtonWidgetClass, wForm, NULL );

    // create the "create movie file dialogs"
    Widget	wFileSel = CreateFileSel( wParent );
    Widget	wSGIOptions = CreateSGIOptionPanel( wParent, wFileSel );
    Widget	wQTOptions = CreateQTOptionPanel( wParent, wFileSel );
    Widget	*pwList = (Widget *)malloc( 2 * sizeof( Widget ) );
                pwList[0] = wSGIOptions;
                pwList[1] = wQTOptions;
    Widget	wFormatOptions = CreateFormatOptionPanel( wParent, pwList );

    // create the dialog to get snap time
    Widget	wSnapTime = CreateSnapTimePanel( wParent );

    SetPosInForm( wCreate, 0, 2, 0, 3 );
    XtAddCallback( wCreate, XmNactivateCallback, CreateMovieCB, wFormatOptions );

    SetPosInForm( wClose, 0, 2, 3, 6 );
    XtAddCallback( wClose, XmNactivateCallback, CloseCB, NULL );

    SetPosInForm( wSnap, 2, 4, 0, 3 );
    XtAddCallback( wSnap, XmNactivateCallback, SnapCB, wSnapTime );
    XtAddCallback( wSnap, XmNarmCallback, ShowRubberband, wShowHide );
    XtAddCallback( wSnap, XmNdisarmCallback, HideRubberband, wShowHide );
    // uh...ugly, but works...unfortunately
    wShowHideButton = wShowHide;

    SetPosInForm( wShowHide, 4, 6, 0, 3 );
    XtAddCallback( wShowHide, XmNactivateCallback, ShowCB, NULL );

    SetPosInForm( wSet, 2, 4, 3, 6 );
    XtAddCallback( wSet, XmNactivateCallback, SetCB, wShowHide );

    SetPosInForm( wQuit, 4, 6, 3, 6 );
    XtAddCallback( wQuit, XmNactivateCallback, QuitCB, NULL );

    return( wForm );
}

#if defined( __cplusplus )
extern "C" {
#endif
//
// SetUpTransparent
//
void SetUpTransparent( void )
{
    XSetWindowAttributes	winattrs;
    Colormap			cmap;
    int				status, nVisuals;
    Display			*pDisplay = XtDisplay( wTopLevel );
    Window			wRoot = DefaultRootWindow( pDisplay );
    XColor			color, exact;
    long			red;
    XLayerVisualInfo		vTemplate, *otherLayerInfo, *defaultLayerInfo;
    Visual			*defaultVisual;
    XGCValues			gcvals;
    XWindowAttributes		AttribRoot;
    
    /* find layer of default visual */
    defaultVisual = DefaultVisual(pDisplay, DefaultScreen( pDisplay ));
    vTemplate.vinfo.visualid = defaultVisual->visualid; 
    /* look for visual in layer "above" default visual with transparent pixel */
    defaultLayerInfo = XGetLayerVisualInfo(pDisplay, VisualIDMask, &vTemplate, &nVisuals);
    vTemplate.layer = defaultLayerInfo->layer + 1;
    vTemplate.vinfo.screen = DefaultScreen( pDisplay );
    vTemplate.type = TransparentPixel;
    otherLayerInfo = XGetLayerVisualInfo(pDisplay,
					 VisualScreenMask|VisualLayerMask|VisualTransparentType, 
					 &vTemplate, &nVisuals);
    if (otherLayerInfo == NULL)
    {
	Debugf( "finding layer below..." );
	if (defaultLayerInfo->type == None)
	{
	    Debugf( "unable to find expected layer visuals" );
	    return;
	}
	vTemplate.layer = defaultLayerInfo->layer - 1;
	vTemplate.vinfo.screen = DefaultScreen( pDisplay );
	otherLayerInfo = XGetLayerVisualInfo( pDisplay,
					      VisualScreenMask | VisualLayerMask, 
					      &vTemplate, &nVisuals );
	if (otherLayerInfo == NULL)
	{
	    Debugf( "cannot find  layer above or below default" );
	    return;
	}
    }

    cmap = XCreateColormap(pDisplay, wRoot, otherLayerInfo->vinfo.visual, AllocNone);
    status = XAllocNamedColor(pDisplay, cmap, "black", &color, &exact);
//    status = XAllocNamedColor(pDisplay, cmap, "white", &color, &exact);
    status = XAllocNamedColor(pDisplay, cmap, "red", &color, &exact);
    if(status == 0) 
    {
	Debugf("could not allocate red");
	return;
    }
    red = color.pixel;
    winattrs.background_pixel = otherLayerInfo->value; /* use transparent pixel */
    winattrs.border_pixel = 0; /* no border but still necessary to avoid BadMatch */
    winattrs.colormap = cmap;
    winattrs.override_redirect = True;
    winattrs.do_not_propagate_mask = True;

    /* get root window attributes */
    XGetWindowAttributes( pDisplay, wRoot, &AttribRoot );
    wOverlayWin = XCreateWindow(pDisplay, wRoot, 0, 0, AttribRoot.width, AttribRoot.height,
				0, otherLayerInfo->vinfo.depth,
				InputOutput, otherLayerInfo->vinfo.visual,
				CWBackPixel|CWBorderPixel|CWColormap|CWOverrideRedirect|CWDontPropagate, 
				&winattrs);
    gcvals.foreground = red;
    gcvals.function = GXxor;
    gcOverlay = XCreateGC(pDisplay, wOverlayWin, GCForeground|GCFunction, &gcvals);

//    XMapWindow( pDisplay, wOverlayWin ); 
}
#if defined( __cplusplus )
}
#endif


//
// CoolMainLoop - yohoo!!!  I can have my own main loop
//
void CoolMainLoop( XtAppContext AppContext )
{
    XEvent event;
    GrabCursor = XCreateFontCursor( XtDisplay( wTopLevel ), 30 );
    SetUpTransparent();
    while( 1 )
    {
	XtAppNextEvent( AppContext, &event );
	if (!bShowing || event.type != KeyPress)
	    XtDispatchEvent( &event );
	else ChangeRubberband( event.xkey );
    }
}

//
// main - 
//
int main( int argc, char *argv[] )
{
    // create app and top level window
    wTopLevel = XtVaAppInitialize( &AppContext, "MoviEdit",
				 NULL, 0,
				 &argc, argv, DefaultResources,
				 XmNinput, False,
				 NULL );

    // create main window
    wMainWindow = CreateMainWindow( wTopLevel );


    // run app
    XtRealizeWidget( wTopLevel );
    CoolMainLoop( AppContext );
    
    return( 0 );
}
