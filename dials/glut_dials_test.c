#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>

/* Display callback (minimal) */
void display(void)
{
    glClear(GL_COLOR_BUFFER_BIT);
    glutSwapBuffers();
}

/* Dial callback */
void dials(int dial, int value)
{
    printf("Dial %d rotated to %d\n", dial, value);
}

/* Button callback */
void buttons(int button, int state)
{
    printf("Button %d %s\n", button, state == GLUT_DOWN ? "pressed" : "released");
}

/* Main function */
int main(int argc, char *argv[])
{
    /* Initialize GLUT */
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowSize(200, 200);
    glutCreateWindow("Dialbox GLUT Test");

    /* Register callbacks */
    glutDisplayFunc(display);
    glutDialsFunc(dials);
    glutButtonBoxFunc(buttons);

    /* Initialize OpenGL */
    glClearColor(0.0, 0.0, 0.0, 1.0);

    /* Print instructions */
    printf("Dialbox GLUT Test: Rotate dials or press buttons (Ctrl+C to exit).\n");

    /* Run GLUT event loop */
    glutMainLoop();

    return 0;
}
