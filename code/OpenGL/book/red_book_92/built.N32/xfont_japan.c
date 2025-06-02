#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "aux.h"

GLuint base;

void makeRasterFont(void)
{
    XFontStruct *fontInfo;
    Font id;
    unsigned int first, last;
    Display *xdisplay;

    xdisplay = auxXDisplay();
    fontInfo = XLoadQueryFont(xdisplay, 
	"--mincho-medium-r-normal--14-130-75-75-c-140-jisx0208.1983-0");
    if (fontInfo == NULL) {
        printf("no font found\n");
	exit(0);
    }

    id = fontInfo->fid;
    first = fontInfo->min_char_or_byte2 + fontInfo->min_byte1 * 128;
    last = fontInfo->max_char_or_byte2 + fontInfo->max_byte1 * 128;

    base = glGenLists((GLuint)last+1);
    if (base == 0) {
        printf("out of display lists\n");
	exit (0);
    }
    glXUseXFont(id, first, last-first+1, base+first);
}

void printString(char *s)
{
    glPushAttrib(GL_LIST_BIT);
    glListBase(base);
    glCallLists(strlen(s)/2, GL_UNSIGNED_SHORT, (GLubyte *)s);
    glPopAttrib();
}

void myinit (void) 
{
    makeRasterFont();
    glShadeModel(GL_FLAT);    
}

void display(void)
{
    GLfloat white[3] = { 1.0, 1.0, 1.0 };
    int i, j;
    char teststring[3];
    teststring[0] = 0x24;
    teststring[1] = 0x24;
    teststring[2] = 0;

    glClear(GL_COLOR_BUFFER_BIT);
    glColor3fv(white);
    glRasterPos2i(20, 100);
    printString(teststring);
    glFlush();
}

void myReshape(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (GLfloat)w, 0.0, (GLfloat)h, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int main(int argc, char** argv)
{
    auxInitDisplayMode(AUX_SINGLE|AUX_RGB);
    auxInitPosition(0, 0, 500, 500);
    auxInitWindow(argv[0]);
    auxReshapeFunc(myReshape);
    myinit();
    auxMainLoop(display);
}
