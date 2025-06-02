/*
 * vertarray - simple demo of the vertex array extension.
 */
/* compile:  cc -o vertarray vertarray.c -lGLw -lGLU -lGL -lXm -lXt -lX11 */

#include <GL/glx.h>
#include <GL/glu.h>
#include <Xm/Frame.h>
#include <Xm/RowColumn.h>
#include <X11/GLw/GLwMDrawA.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int attributeList[] = {GLX_RGBA, GLX_RED_SIZE, 1, GLX_DEPTH_SIZE, 4, None};

static String fallbackResources[] = {
    "*sgiMode: True",
    "*useSchemes: all",
    "*glxwidget.width: 300", "*glxwidget.height: 300",
    "*frame.shadowType: SHADOW_IN",
    NULL,
};


/* number of strips into which the grid should be subdivided */
#define SIZE 21

GLfloat vertexes[SIZE][SIZE][2][3];
GLubyte colors[SIZE][SIZE][2][3];
GLfloat normals[SIZE][SIZE][2][3];

int lighting = 0, faces = 1;

static void 
reshape(Widget w, XtPointer client_data, XtPointer call)
{
    GLwDrawingAreaCallbackStruct *call_data;
    call_data = (GLwDrawingAreaCallbackStruct *) call;

    glViewport(0, 0, call_data->width, call_data->height);
}

static void 
draw_scene(Widget w, XtPointer client_data, XtPointer call)
{
    int i;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (faces) {
	for (i = 0; i < SIZE; i++) {
	    glDrawArraysEXT(GL_QUAD_STRIP, i * 2 * SIZE, 2*SIZE);
	}
    } else {
	for (i = 0; i < SIZE; i++) {
	    glDrawArraysEXT(GL_LINES, i * 2 * SIZE, 2*SIZE);
	}
    }
    
}

static void 
initialize(void)
{
    float x0, y0, z0, x1, y1, z1;
    GLfloat height[SIZE + 2][SIZE + 2];
    int i, j;

    /*
     * set up an array from which to read the heights.  THIS IS NOT
     * THE VERTEX ARRAY.  this is a lookup table from which we will
     * make the vertex array.  it is one wider and one higher than the
     * actual grid because of how it is used to compute normals. */
    for (i = 0; i < SIZE + 2; i++) 
       for (j = 0; j < SIZE + 2; j++) {
	   height[j][i] = sin(i) + cos(j); 
       }

    /* 
     * set up the vertex arrays using the lookup table.  we will draw
     * the array as SIZE independent quad strips.  
     */
    for (i = 0; i < SIZE; i++) {
	for (j = 0; j < SIZE; j++) {
	    vertexes[i][j][0][0] = i;
	    vertexes[i][j][0][1] = j;
	    vertexes[i][j][0][2] = height[i][j];

	    vertexes[i][j][1][0] = i + 1;
	    vertexes[i][j][1][1] = j;
	    vertexes[i][j][1][2] = height[i + 1][j];
	}
    }
    glVertexPointerEXT(3, GL_FLOAT, 0, 
		       sizeof(vertexes) / sizeof(vertexes[0][0][0]),
		       (const GLvoid *)&vertexes[0][0][0][0]);
    glEnable(GL_VERTEX_ARRAY_EXT);



    /* 
     * set up the normal at vertex (x, y) as the cross product of (x,
     * y) -- (x + 1, y) and (x, y) -- (x, y + 1).  
     */
    for (i = 0; i < SIZE; i++) {
	for (j = 0; j < SIZE; j++) {
	    normals[i][j][0][0] = -height[i+1][j];
	    normals[i][j][0][1] = -height[i][j+1];
	    normals[i][j][0][2] = 1;

	    normals[i][j][1][0] = -height[i+2][j];
	    normals[i][j][1][1] = -height[i+1][j+1];
	    normals[i][j][1][2] = 1;
	       
	}
    }
    glNormalPointerEXT(GL_FLOAT, 0, 
		       sizeof(normals) / sizeof(normals[0][0][0]),
		       (const GLvoid *)&normals[0][0][0][0]);
    if (lighting) glEnable(GL_NORMAL_ARRAY_EXT);
    /* normals are not normalized so turn on autonormalization */
    glEnable(GL_NORMALIZE);



    /* 
     * set up the colors as a simple ramp of the form:
     * r = x / SIZE
     * g = y / SIZE
     * b = ((x + y) / 2) / SIZE
     */
    for (i = 0; i < SIZE; i++) {
	for (j = 0; j < SIZE; j++) {
	    colors[j][i][0][0] = (float)i / (float)SIZE * 255.0;
	    colors[j][i][0][1] = (float)j / (float)SIZE * 255.0;
	    colors[j][i][0][2] = ((float)(i + j) / 2.0) / (float)SIZE * 255.0;

	    colors[j][i][1][0] = (float)i / (float)SIZE * 255.0;
	    colors[j][i][1][1] = (float)(j + 1) / (float)SIZE * 255.0;
	    colors[j][i][1][2] = 
	       ((float)(i + 1 + j)/2.0) / (float)SIZE * 255.0;
	}
    }
    glColorPointerEXT(3, GL_UNSIGNED_BYTE, 0, 
		      sizeof(colors) / sizeof(colors[0][0][0]),
		      (const GLvoid *)&colors[0][0][0][0]);
    if (!lighting) glEnable(GL_COLOR_ARRAY_EXT);


    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHT0);
    if (lighting) {
	glEnable(GL_LIGHTING);
    }


    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45, 1, 1, SIZE * 4);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(-SIZE, -SIZE, 12,
	      SIZE, SIZE, 0, 
	      0, 0, 1);
}

static void
process_input(Widget w, XtPointer client_data, XtPointer call)
{
    int redraw = 0;
    XEvent *event = ((GLwDrawingAreaCallbackStruct *) call)->event;
    char buf[31];
    KeySym keysym;


    switch(event->type) {
      case KeyRelease:
	(void) XLookupString(&event->xkey, buf, sizeof(buf), &keysym, NULL);
	if (keysym == XK_Escape) {
	    exit(EXIT_SUCCESS);
	}
	    
	/* simple state machine to cycle through all combinations of
	 * lighting & drawing faces.  although we do not have to
	 * en/disable the normal array, it will result in passing less
	 * stuff down the pipe.  */
	if (!lighting && !faces) {
	    lighting = 1;
	    glEnable(GL_LIGHTING);
	    glEnable(GL_NORMAL_ARRAY_EXT);
	    glDisable(GL_COLOR_ARRAY_EXT);
	} else if (lighting && !faces) {
	    faces = 1;
	} else if (lighting && faces) {
	    lighting = 0;
	    glDisable(GL_LIGHTING);
	    glDisable(GL_NORMAL_ARRAY_EXT);
	    glEnable(GL_COLOR_ARRAY_EXT);
	} else if (!lighting && faces) {
	    faces = 0;
	}
	redraw = 1;
	break;
      default:
	break;
    }

    if (redraw) draw_scene(w, 0, 0);
}
    

main(int argc, char *argv[])
{
    Display *dpy;
    XVisualInfo *visinfo;
    GLXContext glxcontext;
    Widget toplevel, frame, glxwidget;
    XtAppContext appctx;
    
    toplevel = XtOpenApplication(&appctx, "vertarray", NULL, 0, 
				 &argc, argv, fallbackResources, 
				 applicationShellWidgetClass, NULL, 0);
    dpy = XtDisplay(toplevel);
    
    frame = XmCreateFrame(toplevel, "frame", NULL, 0);
    XtManageChild(frame);
    
    /* specify visual directly */
    if (!(visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), attributeList)))
       XtAppError(appctx, "no suitable RGB visual");
    
    glxwidget = XtVaCreateManagedWidget("glxwidget", 
					glwMDrawingAreaWidgetClass,
					frame, GLwNvisualInfo, 
					visinfo, NULL);
    XtAddCallback(glxwidget, GLwNexposeCallback, draw_scene, NULL);
    XtAddCallback(glxwidget, GLwNresizeCallback, reshape, NULL);
    XtAddCallback(glxwidget, GLwNinputCallback, process_input, NULL);

    XtRealizeWidget(toplevel);

    glxcontext = glXCreateContext(dpy, visinfo, 0, GL_TRUE);
    GLwDrawingAreaMakeCurrent(glxwidget, glxcontext);

    initialize();

    printf("hit any key to toggle drawing mode, ESC to exit.\n");

    XtAppMainLoop(appctx);
}

