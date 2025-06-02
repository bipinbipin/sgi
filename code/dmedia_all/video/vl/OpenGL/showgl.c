/*
 * File:          showgl.c
 *
 * Usage:         showgl imagefile
 *
 * Description:   showgl displays an image using OpenGL
 *
 */

#include <stdio.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <X11/keysym.h>
#include <GL/GLwMDrawA.h>
#include <GL/gl.h>
#include <gl/image.h>

/* Callbacks */
static void drawCB();
static void resizeCB();
static void initCB();

static XtAppContext app_context;
static GLXContext glx_context;

static String fallback_resources[] = {
    "*frame*shadowType: SHADOW_IN",
    "*glwidget*rgba: TRUE",
    "*glwidget*allocateBackground: TRUE",
    NULL
    };

/*#define TESTALPHA*/

typedef struct SGIImage {
	unsigned long *image;
	int x, y, z;
} SGIImage;

static SGIImage *image;

SGIImage *ReadImage(char *filename)
{
  static unsigned short rbuf[8192];
  static unsigned short gbuf[8192];
  static unsigned short bbuf[8192];
  static unsigned short abuf[8192];

  int sizey, sizez;
  unsigned long *sptr;
  IMAGE *image;
  SGIImage *sgiImage;
  long i, j;

  image = iopen(filename, "r");

  if (!image) {
        fprintf(stderr, "Cannot open image file \"%s\"\n", filename);
	return NULL;
  }

  if (image->zsize < 3) {
	fprintf(stderr, "Sorry, this program only reads RGB images\n");
	iclose(image);
	return NULL;
  }

  sgiImage = (SGIImage *) malloc (sizeof(SGIImage));
  sgiImage->x = image->xsize;
  sgiImage->y = image->ysize;
  sgiImage->z = image->zsize; /* depth of image, in bytes */
  sgiImage->image = (unsigned long *) malloc (sgiImage->x*(sgiImage->y+1)*sizeof(long));

  for (j=0; j < sgiImage->y; j++) {
        getrow(image, rbuf, j, 0);
        getrow(image, gbuf, j, 1);
        getrow(image, bbuf, j, 2);
	if (sgiImage->z > 3)
		getrow(image, abuf, j, 3);
        sptr = sgiImage->image + j*sgiImage->x;  /* offset into row */
        for (i=0; i < sgiImage->x; i++) {
		unsigned long pix;
		if (sgiImage->z > 3)
			pix = (unsigned long )rbuf[i] + (gbuf[i]<<8) + (bbuf[i]<<16) + (abuf[i] <<24);
		else
			pix = (unsigned long )rbuf[i] + (gbuf[i]<<8) + (bbuf[i]<<16);
                sptr[i] = pix;
        }
  }
  iclose(image);
  return(sgiImage);
}

void Usage(char *progname)
{
    fprintf(stderr, "Usage: %s image.rgb\n", progname);
}

main(argc, argv)
int argc;
char *argv[];
{
    Arg args[20];
    int n;
    int params[10];
    Widget glw, toplevel, form, frame;

    if (argc < 2) {
	Usage(argv[0]);
	exit(0);
    }

    if (!(image = ReadImage(argv[1])))  {
	Usage(argv[0]);
	exit(0);
    }
 
    toplevel = XtAppInitialize(&app_context, "4DgiftsGLw",
                               (XrmOptionDescList) NULL,
                               (Cardinal)0,
                               (int *)&argc,
                               (String*)argv,
                               fallback_resources,
                               (ArgList)NULL, 0);

    n = 0;
    form = XmCreateForm(toplevel, "form", args, n);
    XtManageChild (form);

    /*
    n = 0;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;

    frame = XmCreateFrame (toplevel, "frame", args, n);
    XtManageChild (frame);
    */

    n = 0;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNwidth, image->x); n++;
    XtSetArg(args[n], XmNheight, image->y); n++;
    XtSetArg(args[n], GLwNredSize, 1); n++;
    XtSetArg(args[n], GLwNgreenSize, 1); n++;
    XtSetArg(args[n], GLwNblueSize, 1); n++;
    glGetIntegerv(GL_ALPHA_BITS, params);
    if (params[0] > 0){
    	XtSetArg(args[n], GLwNalphaSize, 1); n++;
    }
    glw = GLwCreateMDrawingArea(form, "glwidget", args, n);

    XtManageChild (glw);
    XtAddCallback(glw, GLwNexposeCallback, drawCB, 0);
    XtAddCallback(glw, GLwNresizeCallback, resizeCB, 0);
    XtAddCallback(glw, GLwNginitCallback, initCB, 0);

    XtRealizeWidget(toplevel);

    XtAppMainLoop(app_context);

}


static void
initCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    Arg args[1];
    XVisualInfo *vi;

    XtSetArg(args[0], GLwNvisualInfo, &vi);
    XtGetValues(w, args, 1);

    glx_context = glXCreateContext(XtDisplay(w), vi, 0, GL_TRUE);

    GLwDrawingAreaMakeCurrent(w, glx_context);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-0.5, -0.5,image->x, image->y,0,0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}


static void
drawCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    GLfloat val;
    GLwDrawingAreaMakeCurrent(w, glx_context);
    glRasterPos3i(-1,-1,0);

#ifdef TESTALPHA
    {
    	FILE *fp = fopen("/usr/tmp/garbage", "w");
    	fprintf(stderr, "drawCB\n");
    	/*glAlphaFunc(GL_GREATER, 0.5);
    	glEnable(GL_ALPHA_TEST);
	*/
    	glDrawPixels(image->x, image->y, GL_ABGR_EXT, GL_UNSIGNED_BYTE,
                 image->image);
	glClearColor(0.5,0.5,0.5,0.5);
	glClear(GL_COLOR_BUFFER_BIT);
    	glReadPixels(0,0,image->x, image->y, GL_ABGR_EXT, GL_UNSIGNED_BYTE,
                 image->image);
	fwrite(image->image, 4, image->x*image->y, fp);
	fclose(fp);
    }
#else
    glDrawPixels(image->x, image->y, GL_ABGR_EXT, GL_UNSIGNED_BYTE,
                 	image->image);
#endif
    glFlush();
}

static void
resizeCB(Widget w, XtPointer client_data,
          GLwDrawingAreaCallbackStruct *call_data)
{
    GLint viewx = (GLint) call_data->width;
    GLint viewy = (GLint) call_data->height;
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glViewport(0, 0, viewx-1, viewy-1);
    glOrtho(-0.5, -0.5, viewx, viewy, 0, 1);
}

