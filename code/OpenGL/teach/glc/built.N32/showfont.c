#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glc.h>
#include <GL/glut.h>


GLuint contextID;
GLuint fontIDs[100];
GLboolean dl = GL_FALSE, needBitmapRecal = GL_TRUE;
GLenum drawStyle = GLC_TRIANGLE;
GLint faceIndex, faceTotal, facesIndex[100], facesTotal[100];
GLfloat rotX = 0.0, rotY = 0.0, rotZ = 0.0, scale = 24.0;
GLint fontIndex, totalFont;
GLint charIndex;


void Init(void)
{
    int i, index;
    char buf[80];
    const char *ptr;

    if ((contextID = glcGenContext()) == 0) {
	printf("Died at glcGenContext\n");
	exit(1);
    }

    if (glcIsContext(contextID) == GL_FALSE) {
	printf("Died at glcIsContext\n");
	exit(1);
    }

    glcContext(contextID);
    if (glcGetError() != GL_NO_ERROR) {
	printf("Died at glcContext\n");
	exit(1);
    }

    if (glcGetCurrentContext() != contextID) {
	printf("Died at glcGetCurrentContext\n");
	exit(1);
    }

    glcPrependCatalog("/usr/lib/X11/fonts/100dpi");
    if (glcGetError() != GL_NO_ERROR) {
	printf("Died at glcPrependCatalog\n");
	exit(1);
    }

    glcAppendCatalog("/usr/lib/X11/fonts/75dpi");
    if (glcGetError() != GL_NO_ERROR) {
	printf("Died at glcAppendCatalog\n");
	exit(1);
    }

    glcRemoveCatalog(1);
    if (glcGetError() != GL_NO_ERROR) {
	printf("Died at glcRemoveCatalog\n");
	exit(1);
    }

    glcRemoveCatalog(0);
    if (glcGetError() != GL_NO_ERROR) {
	printf("Died at glcRemoveCatalog\n");
	exit(1);
    }

    glcPrependCatalog("/usr/lib/X11/fonts/misc");
    if (glcGetError() != GL_NO_ERROR) {
	printf("Died at glcPrependCatalog\n");
	exit(1);
    }

    glcRemoveCatalog(0);
    if (glcGetError() != GL_NO_ERROR) {
	printf("Died at glcRemoveCatalog\n");
	exit(1);
    }

    totalFont = glcGeti(GLC_MASTER_COUNT);
    if (glcGetError() != GL_NO_ERROR) {
	printf("Died at glcGeti\n");
	exit(1);
    }
    if (totalFont > 100) {
	totalFont = 100;
    }

    index = 0;
    for (i = 0; i < totalFont; i++) {
	printf("%s\n", glcGetMasterc(i, GLC_FAMILY));
	fontIDs[index] = glcGenFontID();
	if (glcNewFontFromMaster(fontIDs[index], i) != fontIDs[index]) {
	    printf("Died at glcNewFontFromMaster %d\n", glcGetError());
	    exit(1);
	}
	facesIndex[index] = 0;
	facesTotal[index] = glcGetFonti(fontIDs[index], GLC_FACE_COUNT);
	index++;
    }

    totalFont = index;

    fontIndex = 0;
    charIndex = 0;

    glcRenderStyle(drawStyle);

    glClearColor(0.2, 0.2, 0.2, 0.0);
}

void Reshape(int width, int height)
{

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)width, 0.0, (double)height, -10000.0, 10000.0);
    glMatrixMode(GL_MODELVIEW);
}

void Key(unsigned char key, int x, int y)
{
    int i, j;
    const char *face;

    switch (key) {
	case 'a':
	    rotY -= 1.0;
	    needBitmapRecal = GL_TRUE;
	    glutPostRedisplay();
	    break;
	case 's':
	    rotY += 1.0;
	    needBitmapRecal = GL_TRUE;
	    glutPostRedisplay();
	    break;
	case 'w':
	    rotX -= 1.0;
	    needBitmapRecal = GL_TRUE;
	    glutPostRedisplay();
	    break;
	case 'x':
	    rotX += 1.0;
	    needBitmapRecal = GL_TRUE;
	    glutPostRedisplay();
	    break;
	case 'q':
	    rotZ += 10.0;
	    needBitmapRecal = GL_TRUE;
	    glutPostRedisplay();
	    break;
	case 'z':
	    rotZ -= 10.0;
	    needBitmapRecal = GL_TRUE;
	    glutPostRedisplay();
	    break;
	case '=':
	    scale += 1.0;
	    needBitmapRecal = GL_TRUE;
	    glutPostRedisplay();
	    break;
	case '-':
	    scale -= 1.0;
	    needBitmapRecal = GL_TRUE;
	    glutPostRedisplay();
	    break;
	case 'd':
	    charIndex++;
	    glutPostRedisplay();
	    break;
	case '1':
	    dl = (dl == GL_FALSE) ? GL_TRUE : GL_FALSE;
	    if (dl == GL_TRUE) {
		glcEnable(GLC_GL_OBJECTS);
	    } else {
		glcDisable(GLC_GL_OBJECTS);
	    }
	    glutPostRedisplay();
	    break;
	case '2':
	    if (drawStyle == GLC_BITMAP) {
		drawStyle = GLC_LINE;
	    } else if (drawStyle == GLC_LINE) {
		drawStyle = GLC_TRIANGLE;
	    } else if (drawStyle == GLC_TRIANGLE) {
		drawStyle = GLC_BITMAP;
	    }
	    glcRenderStyle(drawStyle);
	    glutPostRedisplay();
	    break;
	case '3':
	    if (++facesIndex[fontIndex] == facesTotal[fontIndex]) {
		facesIndex[fontIndex] = 0;
	    }
	    face = glcGetFontListc(fontIDs[fontIndex], GLC_FACE_LIST,
			           facesIndex[fontIndex]);
	    glcFontFace(0, face);
	    glutPostRedisplay();
	    break;
	case '4':
	    if (++fontIndex == totalFont) {
		fontIndex = 0;
	    }
	    glutPostRedisplay();
	    break;
	case '5':
	    if (--fontIndex < 0) {
		fontIndex = totalFont - 1;
	    }
	    glutPostRedisplay();
	    break;
	case 27:
	    glcDeleteContext(contextID);
	    exit(1);
    }
}

void SetCharIndex(char *ptr) 
{
    int x, len;

    len = strlen(ptr);
    charIndex = charIndex % len;
}

void Display(void)
{
    GLfloat v[8];
    char *ptr, buf[128];

    glClear(GL_COLOR_BUFFER_BIT);

    glcFont(fontIDs[fontIndex]);
    if (drawStyle == GLC_BITMAP) {
	glPushMatrix();
	glLoadIdentity();

        if (needBitmapRecal == GL_TRUE) {
	    glcLoadIdentity();
	    glcRotate(rotZ);
	    glcScale(scale, scale);
	    needBitmapRecal = GL_FALSE;
	}

	glColor3f(1.0, 0.0, 0.0);
	glRasterPos3f(10.0, 100.0, 0.0);
	ptr = (char *)glcGetFontc(fontIDs[fontIndex], GLC_FAMILY);
	glcRenderString(ptr);

	glColor3f(1.0, 0.0, 0.0);
	glRasterPos3f(10.0, 200.0, 0.0);
	sprintf(buf, "%s (%d/%d)", glcGetFontFace(fontIDs[fontIndex]),
		facesIndex[fontIndex]+1, facesTotal[fontIndex]);
	glcRenderString(buf);
	glPushMatrix();
	glTranslatef(10.0, 200.0, 0.0);
	SetCharIndex(buf);
	if (glcMeasureString(GL_TRUE, buf) != 0) {
	    glColor3f(1.0, 0.0, 1.0);
	    glcGetStringCharMetric(charIndex, GLC_BASELINE, v);
	    glBegin(GL_LINES);
	    glVertex3f(v[0], v[1], 0.0);
	    glVertex3f(v[2], v[3], 0.0);
	    glEnd();
	    glColor3f(0.0, 1.0, 1.0);
	    glcGetStringCharMetric(charIndex, GLC_BOUNDS, v);
	    glBegin(GL_LINE_STRIP);
	    glVertex3f(v[0], v[1], 0.0);
	    glVertex3f(v[2], v[3], 0.0);
	    glVertex3f(v[4], v[5], 0.0);
	    glVertex3f(v[6], v[7], 0.0);
	    glVertex3f(v[0], v[1], 0.0);
	    glEnd();
	}
	glPopMatrix();

	glColor3f(1.0, 0.0, 0.0);
	glRasterPos3f(10.0, 300.0, 0.0);
	if (dl == GL_TRUE) {
	    strcpy(buf, "Display list, ");
	} else {
	    strcpy(buf, "Immediate, ");
	}
	if (drawStyle == GLC_BITMAP) {
	    strcat(buf, "Bitmap");
	} else if (drawStyle == GLC_LINE) {
	    strcat(buf, "Outline");
	} else if (drawStyle == GLC_TRIANGLE) {
	    strcat(buf, "Filled");
	}
	glcRenderString(buf);
	glPushMatrix();
	glTranslatef(10.0, 300.0, 0.0);
	if (glcMeasureString(GL_TRUE, buf) != 0) {
	    glColor3f(1.0, 0.0, 1.0);
	    glcGetStringMetric(GLC_BASELINE, v);
	    glBegin(GL_LINES);
	    glVertex3f(v[0], v[1], 0.0);
	    glVertex3f(v[2], v[3], 0.0);
	    glEnd();
	    glColor3f(0.0, 1.0, 1.0);
	    glcGetStringMetric(GLC_BOUNDS, v);
	    glBegin(GL_LINE_STRIP);
	    glVertex3f(v[0], v[1], 0.0);
	    glVertex3f(v[2], v[3], 0.0);
	    glVertex3f(v[4], v[5], 0.0);
	    glVertex3f(v[6], v[7], 0.0);
	    glVertex3f(v[0], v[1], 0.0);
	    glEnd();
	}
	glPopMatrix();

	glColor3f(1.0, 0.0, 0.0);
	glRasterPos3f(10.0, 400.0, 0.0);
	glcRenderChar((GLint)'A');
	glPushMatrix();
	glTranslatef(10.0, 400.0, 0.0);
	glColor3f(1.0, 0.0, 1.0);
	glcGetCharMetric((GLint)'A', GLC_BASELINE, v);
	glBegin(GL_LINES);
	glVertex3f(v[0], v[1], 0.0);
	glVertex3f(v[2], v[3], 0.0);
	glEnd();
	glColor3f(0.0, 1.0, 1.0);
	glcGetCharMetric((GLint)'A', GLC_BOUNDS, v);
	glBegin(GL_LINE_STRIP);
	glVertex3f(v[0], v[1], 0.0);
	glVertex3f(v[2], v[3], 0.0);
	glVertex3f(v[4], v[5], 0.0);
	glVertex3f(v[6], v[7], 0.0);
	glVertex3f(v[0], v[1], 0.0);
	glEnd();
	glPopMatrix();

	glPushMatrix();
	glTranslatef(200.0, 400.0, 0.0);
	glColor3f(1.0, 0.0, 1.0);
	glcGetMaxCharMetric(GLC_BASELINE, v);
	glBegin(GL_LINES);
	glVertex3f(v[0], v[1], 0.0);
	glVertex3f(v[2], v[3], 0.0);
	glEnd();
	glColor3f(0.0, 1.0, 1.0);
	glcGetMaxCharMetric(GLC_BOUNDS, v);
	glBegin(GL_LINE_STRIP);
	glVertex3f(v[0], v[1], 0.0);
	glVertex3f(v[2], v[3], 0.0);
	glVertex3f(v[4], v[5], 0.0);
	glVertex3f(v[6], v[7], 0.0);
	glVertex3f(v[0], v[1], 0.0);
	glEnd();
	glPopMatrix();
	glPopMatrix();
    } else {
	glPushMatrix();

	glRotatef(rotX, 1.0, 0.0, 0.0);
	glRotatef(rotY, 0.0, 1.0, 0.0);
	glRotatef(rotZ, 0.0, 0.0, 1.0);

	glPushMatrix();
	glTranslatef(10.0, 100.0, 0.0);
	glScalef(scale, scale, 1.0);
	if (drawStyle == GLC_LINE) {
	    glColor3f(0.0, 1.0, 0.0);
	} else if (drawStyle == GLC_TRIANGLE) {
	    glColor3f(0.0, 0.0, 1.0);
	}
	ptr = (char *)glcGetFontc(fontIDs[fontIndex], GLC_FAMILY);
	glcRenderString(ptr);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(10.0, 200.0, 0.0);
	glScalef(scale, scale, 1.0);
	if (drawStyle == GLC_LINE) {
	    glColor3f(0.0, 1.0, 0.0);
	} else if (drawStyle == GLC_TRIANGLE) {
	    glColor3f(0.0, 0.0, 1.0);
	}
	sprintf(buf, "%s (%d/%d)", glcGetFontFace(fontIDs[fontIndex]),
		facesIndex[fontIndex]+1, facesTotal[fontIndex]);
	glcRenderString(buf);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(10.0, 200.0, 0.0);
	glScalef(scale, scale, 1.0);
	SetCharIndex(buf);
	if (glcMeasureString(GL_TRUE, buf) != 0) {
	    glColor3f(1.0, 0.0, 1.0);
	    glcGetStringCharMetric(charIndex, GLC_BASELINE, v);
	    glBegin(GL_LINES);
	    glVertex3f(v[0], v[1], 0.0);
	    glVertex3f(v[2], v[3], 0.0);
	    glEnd();
	    glcGetStringCharMetric(charIndex, GLC_BOUNDS, v);
	    glColor3f(0.0, 1.0, 1.0);
	    glBegin(GL_LINE_STRIP);
	    glVertex3f(v[0], v[1], 0.0);
	    glVertex3f(v[2], v[3], 0.0);
	    glVertex3f(v[4], v[5], 0.0);
	    glVertex3f(v[6], v[7], 0.0);
	    glVertex3f(v[0], v[1], 0.0);
	    glEnd();
	}
	glPopMatrix();

	glPushMatrix();
	glTranslatef(10.0, 300.0, 0.0);
	glScalef(scale, scale, 1.0);
	if (drawStyle == GLC_LINE) {
	    glColor3f(0.0, 1.0, 0.0);
	} else if (drawStyle == GLC_TRIANGLE) {
	    glColor3f(0.0, 0.0, 1.0);
	}
	if (dl == GL_TRUE) {
	    strcpy(buf, "Display list, ");
	} else {
	    strcpy(buf, "Immediate, ");
	}
	if (drawStyle == GLC_BITMAP) {
	    strcat(buf, "Bitmap");
	} else if (drawStyle == GLC_LINE) {
	    strcat(buf, "Outline");
	} else if (drawStyle == GLC_TRIANGLE) {
	    strcat(buf, "Filled");
	}
	glcRenderString(buf);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(10.0, 300.0, 0.0);
	glScalef(scale, scale, 1.0);
	if (glcMeasureString(GL_TRUE, buf) != 0) {
	    glColor3f(1.0, 0.0, 1.0);
	    glcGetStringMetric(GLC_BASELINE, v);
	    glBegin(GL_LINES);
	    glVertex3f(v[0], v[1], 0.0);
	    glVertex3f(v[2], v[3], 0.0);
	    glEnd();
	    glcGetStringMetric(GLC_BOUNDS, v);
	    glColor3f(0.0, 1.0, 1.0);
	    glBegin(GL_LINE_STRIP);
	    glVertex3f(v[0], v[1], 0.0);
	    glVertex3f(v[2], v[3], 0.0);
	    glVertex3f(v[4], v[5], 0.0);
	    glVertex3f(v[6], v[7], 0.0);
	    glVertex3f(v[0], v[1], 0.0);
	    glEnd();
	}
	glPopMatrix();

	glPushMatrix();
	glTranslatef(10.0, 400.0, 0.0);
	glScalef(scale, scale, 1.0);
	if (drawStyle == GLC_LINE) {
	    glColor3f(0.0, 1.0, 0.0);
	} else if (drawStyle == GLC_TRIANGLE) {
	    glColor3f(0.0, 0.0, 1.0);
	}
	glcRenderChar((GLint)'A');
	glPopMatrix();
	glPushMatrix();
	glTranslatef(10.0, 400.0, 0.0);
	glScalef(scale, scale, 1.0);
	glColor3f(1.0, 0.0, 1.0);
	glcGetCharMetric((GLint)'A', GLC_BASELINE, v);
	glBegin(GL_LINES);
	glVertex3f(v[0], v[1], 0.0);
	glVertex3f(v[2], v[3], 0.0);
	glEnd();
	glcGetCharMetric((GLint)'A', GLC_BOUNDS, v);
	glColor3f(0.0, 1.0, 1.0);
	glBegin(GL_LINE_STRIP);
	glVertex3f(v[0], v[1], 0.0);
	glVertex3f(v[2], v[3], 0.0);
	glVertex3f(v[4], v[5], 0.0);
	glVertex3f(v[6], v[7], 0.0);
	glVertex3f(v[0], v[1], 0.0);
	glEnd();
	glPopMatrix();

	if (drawStyle == GLC_LINE) {
	    glColor3f(0.0, 1.0, 0.0);
	} else if (drawStyle == GLC_TRIANGLE) {
	    glColor3f(0.0, 0.0, 1.0);
	}
	glPushMatrix();
	glTranslatef(200.0, 400.0, 0.0);
	glScalef(scale, scale, 1.0);
	glColor3f(1.0, 0.0, 1.0);
	glcGetMaxCharMetric(GLC_BASELINE, v);
	glBegin(GL_LINES);
	glVertex3f(v[0], v[1], 0.0);
	glVertex3f(v[2], v[3], 0.0);
	glEnd();
	glcGetMaxCharMetric(GLC_BOUNDS, v);
	glColor3f(0.0, 1.0, 1.0);
	glBegin(GL_LINE_STRIP);
	glVertex3f(v[0], v[1], 0.0);
	glVertex3f(v[2], v[3], 0.0);
	glVertex3f(v[4], v[5], 0.0);
	glVertex3f(v[6], v[7], 0.0);
	glVertex3f(v[0], v[1], 0.0);
	glEnd();
	glPopMatrix();
	glPopMatrix();
    }

    if (glcGetError() != GL_NO_ERROR) {
	printf("Got a GLC error\n");
    }

    glFlush();
}

void main(int argc, char **argv)
{

    glutInitWindowSize(700, 500);
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB);
    glutCreateWindow("glc test");
    Init();
    glutDisplayFunc(Display);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(Key);
    glutMainLoop();
}
