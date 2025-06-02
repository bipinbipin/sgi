/*
** blendxor.c - Demonstrates the use of the blend_logic_op
**    extension to draw hilights. Using XOR to draw the same
**    image twice restores the background to its original value.
*/

#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <GL/glut.h>

#if defined(GL_EXT_blend_color) && defined(GL_EXT_blend_logic_op)

GLenum doubleBuffer;
GLenum dithering = GL_FALSE;


static void Init(void)
{

    glDisable(GL_DITHER);
    glShadeModel(GL_FLAT);
}

static void Reshape(int width, int height)
{

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, 400, 0, 400);
    glMatrixMode(GL_MODELVIEW);
}

static void Key(unsigned char key, int x, int y)
{

    switch (key) {
      case 'd':
	dithering = !dithering;
	glutPostRedisplay();
	break;
      case 27:
	exit(0);
    }
}

static void Draw(void)
{
    float xscale, yscale;
    GLfloat x, y;
    int i;

    glDisable(GL_BLEND);

    (dithering) ? glEnable(GL_DITHER) : glDisable(GL_DITHER);

    glClearColor(0.5, 0.6, 0.1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    /* Draw background prims */
    glColor3f(0.1, 0.1, 1.0);
    glBegin(GL_TRIANGLES);
        glVertex2i(5, 5);
        glVertex2i(130, 50);
        glVertex2i(100,  300);
    glEnd();
    glColor3f(0.5, 0.2, 0.9);
    glBegin(GL_TRIANGLES);
        glVertex2i(200, 100);
        glVertex2i(330, 50);
        glVertex2i(340,  400);
    glEnd();

    glEnable(GL_BLEND);
    glBlendEquationEXT(GL_LOGIC_OP);
    glLogicOp(GL_XOR);

    /* Draw a set of rectangles across the window */
    glColor3f(0.9, 0.2, 0.8);
    for(i = 0; i < 400; i+=60) {
        glBegin(GL_POLYGON);
            glVertex2i(i, 100);
            glVertex2i(i+50, 100);
            glVertex2i(i+50, 200);
            glVertex2i(i, 200);
        glEnd();
    }
    if (doubleBuffer) {
	glutSwapBuffers();
	glDrawBuffer(GL_FRONT);	/* draw next prims in visible buffer */
    } else {
	glFlush();
    }
    sleep(2);

    /* Redraw  the rectangles, which should erase them */
    for(i = 0; i < 400; i+=60) {
        glBegin(GL_POLYGON);
            glVertex2i(i, 100);
            glVertex2i(i+50, 100);
            glVertex2i(i+50, 200);
            glVertex2i(i, 200);
        glEnd();
    }

    glFlush();
    if (doubleBuffer) {
        glDrawBuffer(GL_BACK);	/* draw next frame in invisible buffer */
    }
    assert(glGetError() == GL_NO_ERROR);
}

static void Args(int argc, char **argv)
{
    GLint i;

    doubleBuffer = GL_FALSE;

    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-sb") == 0) {
	    doubleBuffer = GL_FALSE;
	} else if (strcmp(argv[i], "-db") == 0) {
	    doubleBuffer = GL_TRUE;
	}
    }
}

static GLboolean QueryExtension(char *extName)
{
    /*
    ** Search for extName in the extensions string. Use of strstr()
    ** is not sufficient because extension names can be prefixes of
    ** other extension names. Could use strtok() but the constant
    ** string returned by glGetString can be in read-only memory.
    */
    char *p = (char *) glGetString(GL_EXTENSIONS);
    char *end = p + strlen(p);
    while (p < end) {
	int n = strcspn(p, " ");
	if ((strlen(extName) == n) && (strncmp(extName, p, n) == 0)) {
	    return GL_TRUE;
	}
	p += (n + 1);
    }
    return GL_FALSE;
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    Args(argc, argv);

#if (GLUT_API_VERSION >= 4 || GLUT_XLIB_IMPLEMENTATION >= 9)
    if (doubleBuffer) glutInitDisplayString("red<=4 green<=4 blue<=4 double");
    else glutInitDisplayString("red<=4 green<=4 blue<=4");    
#else
    {
       GLenum type = GLUT_RGB;
       type |= (doubleBuffer) ? GLUT_DOUBLE : GLUT_SINGLE;

       glutInitDisplayMode(type);
    }
#endif

    glutInitWindowSize(400, 400);
    glutCreateWindow("Blend XOR");

    if (!QueryExtension("GL_EXT_blend_logic_op")) {
	printf("Blend_logic_op extension is not present.\n");
	exit(0);
    }

    Init();

    glutReshapeFunc(Reshape);
    glutKeyboardFunc(Key);
    glutDisplayFunc(Draw);
    glutMainLoop();
}

#else

int main(int argc, char **argv)
{

    printf("GL_EXT_blend_logic_op extension is not present.\n");
}

#endif
