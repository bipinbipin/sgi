/* varray.c
 * 
 * This demonstrates rendering vertex arrays via glArrayElement(), 
 * glDrawArrays() and glDrawElements() in immediate mode and display lists.
 * Client states are enabled/disabled via gl{En|Dis}ableClientState() or
 * glInterleavedArrays. Use of gl{Push|Pop}ClientAttrib() is also demonstrated.
 *
 * Commands include:
 * '1' renders vertex arrays via glArrayElement()
 * '2' renders vertex arrays via glDrawArrays()
 * '3' renders vertex arrays via glDrawElements()
 * 'i' toggles glInterleavedArrays()
 * 'd' toggles display lists
 * 'm' toggles between compile and compile and execute mode
 *      (works only if display lists are on)
 * 'c' clears screen
 * 'g' prints gets
 * 'v' verbose mode
 * 'h','?' this help message
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glut.h>

#define COLOR_OFFSET_1 16
#define COLOR_OFFSET_2 32

#define CHECK_ERROR \
	err = glGetError(); \
	if (err) { \
	    printf("Error %d at line %d\n", err, __LINE__); \
	}

static GLubyte texture[768];

static GLint verts[] = {
    10, 10, 0,
    200, 100, 0,
    50, 200, 0
};
static GLfloat colors[] = {
    1.0, 0.0, 0.0,
    0.0, 0.0, 1.0,
    0.5, 0.2, 0.8
};
static GLfloat cv[] = { 
    1.0, 1.0, 0.0,
    430, 30,
    1.0, 0.0, 1.0,
    480, 300,
    1.0, 1.0, 1.0,
    380, 200
};
static GLshort tv[] = {
    0, 0,
    5, 400,
    1, 0,
    100, 400,
    1, 1,
    100, 495,
    0, 1,
    5, 495
};
static GLfloat verts2[] = {
    350.0, 450.0, 1.0, 1.0,
    350.0, 350.0, 1.0, 1.0,
    400.0, 325.0, 1.0, 1.0,
    450.0, 350.0, 1.0, 1.0,
    450.0, 450.0, 1.0, 1.0,
    400.0, 475.0, 1.0, 1.0
};
static GLfloat norms[] = {
    0, 0, 1, 
    0, 0, 1, 
    0, 0, 1, 
    0, 0, 1,
    0, 0, -1, 
    0, 0, -1, 
    0, 0, -1, 
    0, 0, -1
};
static GLint verts3[] = { 
    150, 200, 
    250, 200, 
    250, 300, 
    150, 300, 
    250, 200, 
    350, 200, 
    350, 300, 
    250, 300
};
static GLuint indices[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8 
};
static GLuint dlistIndices[] = {
    1, 2, 3, 4, 5, 6		/* 0 is not allowed */
};
static GLboolean edges[] = {
    GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE, GL_FALSE, GL_TRUE
};

static int window;
static GLenum isRGB, err;
typedef enum { 
   ARRAYELEMENT, DRAWARRAYS, DRAWELEMENTS 
} RenderVertexArrayMode;
static RenderVertexArrayMode renderVertexArrayMode= ARRAYELEMENT;
static int isDList = 0;
static int isGets = 0;		
static int isClear= 0;
static int isInterleavedArrays= 0;
static int isVerbose = 1;
static GLenum dlMode = GL_COMPILE;

/* function declarations */
static void dumpDisplayList(void);
static void setUpColorMap(void);
static void init(void);
static void reshape(int, int);
static void key(unsigned char, int, int);
static void testGets(void);
static void testClientAttributeStack(void);
static void renderLowerLeftTriangle(GLuint, GLfloat *);
static void renderLowerRightTriangle(GLuint);
static void renderUpperLeftQuad(GLuint);
static void renderUpperRightEdge(GLuint);
static void renderMiddleQuad(GLuint);
static void renderDonut(GLuint, GLfloat *);
static void draw(void);
static void args(int, char *[]);

#define MAX_ELEMENTS 2000
/* skip past 3 colors + 3 vertices of 4 bytes each */
#define SKIP ((3+3)*sizeof(GLfloat))

static void dumpDisplayList(void)
{
    if (isVerbose) {
       if (isDList) {
	   if (dlMode == GL_COMPILE) {
	      printf("GL_COMPILE display lists\n");
	   }
	   else {
	      assert(dlMode == GL_COMPILE_AND_EXECUTE);
	      printf("GL_COMPILE_AND_EXECUTE display lists\n");
	   }
       }   
       else printf("Immediate mode on\n");
    }
}

static void setUpColorMap(void)
{
    int ii;

    for (ii = 0; ii < 16; ii++) {
	glutSetColor(ii+COLOR_OFFSET_1, 0.0, 0.0, ii/15.0);
	glutSetColor(ii+COLOR_OFFSET_2, 0.0, ii/15.0, 0.0);
    }
}

static void init(void)
{
    int ii;

    if (!isRGB) {
       setUpColorMap();
    }

    /* create texture image */
    for (ii = 0; ii < 192; ii+=3) {
       texture[ii] = 0xff; texture[ii+1] = 0xff; texture[ii+2] = 0x00;
    }
    for (; ii < 2*192; ii+=3) {
       texture[ii] = 0xff; texture[ii+1] = 0x00; texture[ii+2] = 0xff;
    }
    for (; ii < 3*192; ii+=3) {
       texture[ii] = 0x00; texture[ii+1] = 0xff; texture[ii+2] = 0xff;
    }
    for (; ii < 4*192; ii+=3) {
       texture[ii] = 0x00; texture[ii+1] = 0x00; texture[ii+2] = 0xff;
    }

    printf("Type h or ? for help.\n");
}

static void reshape(int width, int height)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 500.0, 0.0, 500.0, 0.0, -1000.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void key(unsigned char key, int x, int y)
{

    switch (key) {
      case '1':
	  renderVertexArrayMode = ARRAYELEMENT;
	  if (isVerbose) 
	     printf("Rendering vertex arrays via glArrayElement(): ");
	  dumpDisplayList();
	  glutPostRedisplay();
          break;
      case '2':
	  renderVertexArrayMode = DRAWARRAYS;
	  if (isVerbose)
	     printf("Rendering vertex arrays via glDrawArrays(): ");
	  dumpDisplayList();
	  glutPostRedisplay();
          break;
      case '3':
	  renderVertexArrayMode = DRAWELEMENTS;
	  if (isVerbose)
	     printf("Rendering vertex arrays via glDrawElements(): ");
	  dumpDisplayList();
	  glutPostRedisplay();
          break;
      case 'd':
	  isDList = !isDList;
	  dumpDisplayList();
	  glutPostRedisplay();	  
	  break;
      case 'm':
	  if (isDList) {
	     if(dlMode == GL_COMPILE) {
		dlMode = GL_COMPILE_AND_EXECUTE;
	     } else {
	        assert(dlMode == GL_COMPILE_AND_EXECUTE);
	        dlMode = GL_COMPILE;
	     }
	     dumpDisplayList();	     
	     glutPostRedisplay();
	  }
	  else {
	     printf("Only works with display lists.\n");
	  }
	  break;
      case 'g': 
	  isGets= 1;
	  dumpDisplayList();
	  glutPostRedisplay();
	  break;
      case 'c':
	  isClear= 1;
	  if (isVerbose) printf("Clear screen\n");
	  glutPostRedisplay();
          break;
      case 'i':
	  isInterleavedArrays= !isInterleavedArrays;
	  if (isVerbose) {
	     if (isInterleavedArrays) {
		printf("InterleavedArrays on\n"); 
	     }
	     else {
		printf("InterleavedArrays off\n"); 
	     }
	  }
	  glutPostRedisplay();
	  break;
      case 'v':	  
	  isVerbose= !isVerbose;
	  if (isVerbose) 
	     printf("Verbose on\n"); 
	  else printf("Verbose off\n");
          break;
      case 'h': case '?':
	  printf("1 - renders vertex arrays via glArrayElement()\n");
	  printf("2 - renders vertex arrays via glDrawArrays()\n");
	  printf("3 - renders vertex arrays via glDrawElements()\n");
	  printf("i - toggles glInterleavedArrays()\n");
	  printf("d - toggles display list\n");
	  printf("m - toggles between GL_COMPILE & GL_COMPILE_AND_EXECUTE\n");
	  printf("    (works only if display lists are on)\n");
	  printf("c - clears screen\n"); 
	  printf("g - prints gets\n");
	  printf("v - toggle verbose mode\n"); 
	  printf("h, ? - this help message\n");
	  printf("Esc - quit\n");
          break;
      case 27:
	  glutDestroyWindow(window);
	  exit(0);
	  break;
      default:
	  printf("Type h or ? for help.\n");
	  break;
    }
}

static void testGets(void)
{
    GLint size, stride, type, count;
    GLvoid *ptr;
    GLint depth;
    
    glGetIntegerv(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH,&depth);
    assert(glGetError() == GL_NO_ERROR);
    printf("GL_MAX_CLIENT_ATTRIB_STACK_DEPTH = %d\n",depth); 

    glGetIntegerv(GL_INDEX_ARRAY_TYPE, &type);
    glGetIntegerv(GL_INDEX_ARRAY_STRIDE, &stride);
    glGetPointerv(GL_INDEX_ARRAY_POINTER, &ptr);
    printf("INDEX_ARRAY type = 0x%x, stride = %d, ptr = 0x%x \n", 
	   type, stride, ptr);
    CHECK_ERROR

    glGetIntegerv(GL_EDGE_FLAG_ARRAY_STRIDE, &stride);
    glGetPointerv(GL_EDGE_FLAG_ARRAY_POINTER, &ptr);
    printf("EDGE_FLAG_ARRAY stride = %d, ptr = 0x%x\n", stride, ptr);
    CHECK_ERROR

    glGetIntegerv(GL_TEXTURE_COORD_ARRAY_SIZE, &size);
    glGetIntegerv(GL_TEXTURE_COORD_ARRAY_TYPE, &type);
    glGetIntegerv(GL_TEXTURE_COORD_ARRAY_STRIDE, &stride);
    glGetPointerv(GL_TEXTURE_COORD_ARRAY_POINTER, &ptr);
    printf("TEXTURE_COORD_ARRAY size = %d, type = 0x%x, stride = %d, ptr = 0x%x\n",
	   size, type, stride, ptr);
    CHECK_ERROR

    glGetIntegerv(GL_COLOR_ARRAY_SIZE, &size);
    glGetIntegerv(GL_COLOR_ARRAY_TYPE, &type);
    glGetIntegerv(GL_COLOR_ARRAY_STRIDE, &stride);
    glGetPointerv(GL_COLOR_ARRAY_POINTER, &ptr);
    printf("COLOR_ARRAY size = %d, type = 0x%x, stride = %d, ptr = 0x%x\n",
	   size, type, stride, ptr);
    CHECK_ERROR

    glGetIntegerv(GL_NORMAL_ARRAY_TYPE, &type);
    glGetIntegerv(GL_NORMAL_ARRAY_STRIDE, &stride);
    glGetPointerv(GL_NORMAL_ARRAY_POINTER, &ptr);
    printf("NORMAL_ARRAY type = 0x%x, stride = %d, ptr = 0x%x\n", 
	   type, stride, ptr);
    CHECK_ERROR

    glGetIntegerv(GL_VERTEX_ARRAY_SIZE, &size);
    glGetIntegerv(GL_VERTEX_ARRAY_TYPE, &type);
    glGetIntegerv(GL_VERTEX_ARRAY_STRIDE, &stride);
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &ptr);
    printf("VERTEX_ARRAY size = %d, type = 0x%x, stride = %d, ptr = 0x%x\n\n",
	   size, type, stride, ptr);
    CHECK_ERROR
}

static void testClientAttributeStack(void)
{
    int ii;
    GLint depth, currentDepth;

    glGetIntegerv(GL_CLIENT_ATTRIB_STACK_DEPTH,&currentDepth);
    assert(glGetError() == GL_NO_ERROR);
    assert(currentDepth == 0);

    glGetIntegerv(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH,&depth);
    assert(glGetError() == GL_NO_ERROR);

    for (ii= 0; ii< depth; ii++) {
      glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
      assert(glGetError() == GL_NO_ERROR);
    } 

    /* stack is now full */
    glGetIntegerv(GL_CLIENT_ATTRIB_STACK_DEPTH,&currentDepth);
    assert(currentDepth == depth);

    /* test for stack overflow */
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    assert(glGetError() == GL_STACK_OVERFLOW);

    /* empty out stack */
    for (ii= 0; ii< depth; ii++) {
      glPopClientAttrib();
      assert(glGetError() == GL_NO_ERROR);
    } 

    /* now stack is empty */

    /* test for stack underflow */ 
    glPopClientAttrib();
    assert(glGetError() == GL_STACK_UNDERFLOW);

    /* now stack is empty */
}

static void renderLowerLeftTriangle(GLuint id, GLfloat *keepPointer)
{
    {
       /* GL_C3F_V3I doesn't exist so make it GL_C3F_V3F instead */
       int numElementColors= sizeof(colors)/sizeof(GLfloat);/* == 9 */
       int numElementVerts= sizeof(verts)/sizeof(GLint); /* == 9 */
       int numFloats= numElementVerts + numElementColors; /* == 18 */
       int numVerts= numElementVerts/3;	/* == 3 */
       int count= 0; 
       int ii, jj;
       int cc= 0, vv= 0;

       assert(numFloats <= MAX_ELEMENTS);
       assert(keepPointer != NULL);
       /* store interleaved array in color,vertex order for numVerts */
       for (ii= 0; ii< numVerts; ii++) {
	  for (jj= 0; jj< 3; jj++) {
	     keepPointer[count++] = colors[cc++];
	  }
	  for (jj= 0; jj< 3; jj++) {
	     keepPointer[count++] = (GLfloat)verts[vv++];
	  }
       }
       assert(cc == numElementVerts);
       assert(vv == numElementVerts);
       assert(count == numFloats);
    }

    if (!isInterleavedArrays) {
       glColorPointer(3, GL_FLOAT, SKIP, keepPointer);
       glVertexPointer(3, GL_FLOAT, SKIP, &keepPointer[3]);

       glEnableClientState(GL_COLOR_ARRAY);
       glEnableClientState(GL_VERTEX_ARRAY);
       assert(glIsEnabled(GL_NORMAL_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_INDEX_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_TEXTURE_COORD_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_EDGE_FLAG_ARRAY) == GL_FALSE);
    }
    else {
       glInterleavedArrays(GL_C3F_V3F,0,(const GLvoid *)keepPointer);
    }
    /* save vertex array info on stack so we can use it later */
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    CHECK_ERROR

    if (isDList) {
	glNewList(id, dlMode);
    }
    switch (renderVertexArrayMode) {
    int ii;      
    case ARRAYELEMENT:
	glBegin(GL_TRIANGLES);
	    for (ii = 0; ii < 3; ii++) {
		glArrayElement(ii);
	    }
	glEnd();
        break; 
    case DRAWARRAYS:
        glDrawArrays(GL_TRIANGLES, 0, 3);
        break; 
    case DRAWELEMENTS:
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, indices);
        break; 
    default:
        assert(0);
	break;
    }
    if (isDList) {
	glEndList();
    }
    CHECK_ERROR
}

static void renderLowerRightTriangle(GLuint id)
{
    GLfloat *iaf= NULL;

    if (!isInterleavedArrays) {
       glColorPointer(3, GL_FLOAT, 20, cv);
       glVertexPointer(2, GL_FLOAT, 20, &cv[3]);

       /* same arrays still enabled */
       assert(glIsEnabled(GL_COLOR_ARRAY) == GL_TRUE);
       assert(glIsEnabled(GL_VERTEX_ARRAY) == GL_TRUE);
       /* everything else still disabled */ 
       assert(glIsEnabled(GL_INDEX_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_EDGE_FLAG_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_TEXTURE_COORD_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_NORMAL_ARRAY) == GL_FALSE);
    }
    else {
      /* GL_C3F_V2F doesn't exist so make it GL_C3F_V3F instead */
      int numElementCV= sizeof(cv)/sizeof(GLfloat);
      int numVerts= 3;
      /* need numVerts's Zs coords since they are missing */
      int numFloats= numElementCV + numVerts; 
      int count= 0;
      int ii;

      assert(iaf == NULL);
      iaf= (GLfloat *)malloc(sizeof(GLfloat) * numFloats);

      /* copy array into interleaved array with missing Zs properly inserted */
      assert(iaf != NULL);
      for (ii= 0; ii< numElementCV; ii++) {
	 if ( (ii != 0) && ((ii % 5) == 0)) {
	    iaf[count++] = 0.0;	/* put in missing Z coord */
	 }
         iaf[count++] = cv[ii];
      }
      /* don't forget last missing Z */
      if (ii == numElementCV) iaf[count++] = 0.0; 

      assert(count == numFloats);
      glInterleavedArrays(GL_C3F_V3F,0,(const GLvoid *)iaf);
    }

    if (isDList) {
	glNewList(id, dlMode);
    }
    switch (renderVertexArrayMode) {
    int ii;      
    case ARRAYELEMENT:
	glBegin(GL_TRIANGLES);
	    for (ii = 0; ii < 3; ii++) {
		glArrayElement(ii);
	    }
	glEnd();
        break; 
    case DRAWARRAYS:
        glDrawArrays(GL_TRIANGLES, 0, 3);
        break; 
    case DRAWELEMENTS:
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, indices);
        break; 
    default:
        assert(0);
	break;
    }
    if (isDList) {
	glEndList();
    }
    if (isInterleavedArrays) {
        assert(iaf != NULL);
	free((void *)iaf);
	iaf= NULL;
    }
    glDisableClientState(GL_COLOR_ARRAY);
}

static void renderUpperLeftQuad(GLuint id)
{
    GLfloat *iaf= NULL;

    if (!isInterleavedArrays) {
       glTexCoordPointer(2, GL_SHORT, 8, tv);
       glVertexPointer(2, GL_SHORT, 8, &tv[2]);

       glEnableClientState(GL_VERTEX_ARRAY);
       glEnableClientState(GL_TEXTURE_COORD_ARRAY);
       assert(glIsEnabled(GL_NORMAL_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_INDEX_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_COLOR_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_EDGE_FLAG_ARRAY) == GL_FALSE);
    }
    else {
       /* GL_T2S_V2S doesn't exist so make it GL_T2F_V3F instead */ 
       int numElementTV= sizeof(tv)/sizeof(GLshort);
       int numVerts= 4;
       /* need numVerts' Zs coords since they are missing */
       int numFloats= numElementTV + numVerts;
       int count= 0;
       int ii;

       assert(iaf == NULL);
       iaf= (GLfloat*)malloc(sizeof(GLfloat) * numFloats);

       /* copy array into interleaved array with missing Zs properly inserted*/
       assert(iaf != NULL);
       for (ii= 0; ii< numElementTV; ii++) {
	  iaf[count++] = (GLfloat) tv[ii];
	  if ( (ii != 0) && ((ii % 4) == 0)) {
	     iaf[count++] = 0.0; /* put in missing Z coord */
	  }
       }
       /* don't forget last missing Z */
       if (ii == numElementTV) iaf[count++] = 0.0; 

       assert(count == numFloats);
       glInterleavedArrays(GL_T2F_V3F,0,(const GLvoid *)iaf);
    }

    if (isDList) {
	glNewList(id, dlMode);
    }
    glPushAttrib(GL_ENABLE_BIT|GL_TEXTURE_BIT);
    glEnable(GL_TEXTURE_2D);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE,
		 texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    switch (renderVertexArrayMode) {
    int ii;
    case ARRAYELEMENT:
	glBegin(GL_POLYGON);
	    for (ii = 0; ii < 4; ii++) {
		glArrayElement(ii);
	    }
	glEnd();
        break;
    case DRAWARRAYS:
	glDrawArrays(GL_POLYGON, 0, 4);
        break;
    case DRAWELEMENTS:
        glDrawElements(GL_POLYGON, 4, GL_UNSIGNED_INT, indices);
        break;
    default:
        assert(0);
	break;
    }
    glPopAttrib();		/* restore enable, min/mag filters */
    if (isDList) {
	glEndList();
    }
    if (isInterleavedArrays) {
        assert(iaf != NULL);
	free((void *)iaf);
	iaf= NULL;
    }
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

static void renderUpperRightEdge(GLuint id)
{
    if (!isInterleavedArrays) {
       glEdgeFlagPointer(0, edges);
       glVertexPointer(4, GL_FLOAT, 0, verts2);

       glEnableClientState(GL_EDGE_FLAG_ARRAY);
       assert(glIsEnabled(GL_NORMAL_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_INDEX_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_COLOR_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_TEXTURE_COORD_ARRAY) == GL_FALSE);

       assert(glIsEnabled(GL_VERTEX_ARRAY) == GL_TRUE);
    }
    else {
       /* GL_E6F_V4F doesn't exist so make it GL_V3F instead */
       /* edge flag is not supported for interleavedarrays
	* stride is 16 bytes since that the distance between vertices,
	* (the 4th coord is skipped).
	*/
       glInterleavedArrays(GL_V3F,16,(const GLvoid *)verts2);
    }

    if (isDList) {
	glNewList(id, dlMode);
    }
    glPushAttrib(GL_CURRENT_BIT|GL_POLYGON_BIT|GL_LINE_BIT|GL_LIGHTING_BIT);
    glColor3f(1.0, 0.0, 0.0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(4.0);
    glShadeModel(GL_FLAT);
    switch (renderVertexArrayMode) {
    int ii;
    case ARRAYELEMENT:
	glBegin(GL_POLYGON);
	    for (ii = 0; ii < 6; ii++) {
		glArrayElement(ii);
	    }
	glEnd();
        break;
    case DRAWARRAYS:
        glDrawArrays(GL_POLYGON, 0, 6);
        break;
    case DRAWELEMENTS:
        glDrawElements(GL_POLYGON, 6, GL_UNSIGNED_INT, indices);
        break;
    default:
        assert(0);
	break;
    }
    glPopAttrib();		/* restore linewidth & polygonmode & current */
    if (isDList) {
	glEndList();
    }

    glDisableClientState(GL_EDGE_FLAG_ARRAY);
}

static void renderMiddleQuad(GLuint id)
{
    GLfloat *iaf= NULL;

    /* render quad in middle */
    if (!isInterleavedArrays) {
       glNormalPointer(GL_FLOAT, 0, norms);
       glVertexPointer(2, GL_INT, 0, verts3);

       glEnableClientState(GL_NORMAL_ARRAY);
       assert(glIsEnabled(GL_EDGE_FLAG_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_INDEX_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_TEXTURE_COORD_ARRAY) == GL_FALSE);
       assert(glIsEnabled(GL_COLOR_ARRAY) == GL_FALSE);

       assert(glIsEnabled(GL_VERTEX_ARRAY) == GL_TRUE);
    }
    else {
        /* GL_N3F_V2I doesn't exist so make it GL_N3F_V3F instead */
        int numElementNorms= sizeof(norms)/sizeof(GLfloat);/* == 24 */
	int numElementVerts= sizeof(verts3)/sizeof(GLint); /* == 16 */
	int numVerts= numElementNorms/3; /* == 8 */
	int numFloats= numElementNorms + numElementVerts + numVerts; /* == 48*/
	int count= 0;
	int ii, jj;
	int nn= 0, vv= 0;
	
	assert(iaf == NULL);
	iaf= (GLfloat *)malloc(sizeof(GLfloat) * numFloats);
	
	/* store interleaved array in normal/vertex order for numVerts with
	 * missing Zs propertly inserted.
	 */
	assert(iaf != NULL);
	for (ii= 0; ii< numVerts; ii++) {
	    for (jj= 0; jj< 3; jj++) {
	        iaf[count++]= norms[nn++];
	    }
	    for (jj= 0; jj< 2; jj++) {
	        iaf[count++]= (GLfloat) verts3[vv++];
	    }
	    iaf[count++]= 0.0;	/* put in missing Z */
	}
	assert(nn == numElementNorms);
	assert(vv == numElementVerts);
	assert(count == numFloats);
	glInterleavedArrays(GL_N3F_V3F,0,(const GLvoid *)iaf);
    }

    if (isDList) {
	glNewList(id, dlMode);
    }
    glPushAttrib(GL_ENABLE_BIT|GL_LIGHTING_BIT);  
    {
	GLfloat back_light_pos[] = {0.0, 0.0, -1.0, 1.0};
	GLfloat back_light_dir[] = {0.0, 0.0, 1.0, 1.0};
	GLfloat amb[] = {0.5, 0.5, 0.5, 1.0};
	GLfloat white[] = {1.0, 1.0, 1.0, 1.0};
	GLfloat red[] = {1.0, 0.0, 0.0, 1.0};
	GLfloat blue[] = {0.0, 0.0, 1.0, 1.0};

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
	glMaterialfv(GL_FRONT, GL_AMBIENT, white);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, red);
	glEnable(GL_LIGHT1);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, blue);
	glLightfv(GL_LIGHT1, GL_POSITION, back_light_pos);
	glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, back_light_dir);

    }
    switch (renderVertexArrayMode) {
    int ii;
    case ARRAYELEMENT:
	glBegin(GL_POLYGON);
	    for (ii = 0; ii < 4; ii++) { /* [0-3] */
		glArrayElement(ii);
	    }
	glEnd();
	glBegin(GL_POLYGON);
	    for (; ii < 8; ii++) { /* [4-7] */
		glArrayElement(ii);
	    }
	glEnd();
        break;
    case DRAWARRAYS:	
        glDrawArrays(GL_POLYGON,0,4); /* left quad */
        glDrawArrays(GL_POLYGON,4,4); /* right quad */
        break;
    case DRAWELEMENTS:
        glDrawElements(GL_POLYGON, 4, GL_UNSIGNED_INT, indices);
        glDrawElements(GL_POLYGON, 4, GL_UNSIGNED_INT, &indices[4]);
        break;
    default:
        assert(0);
	break;
    }
    glPopAttrib();		/* restore enable, lighting */
    if (isDList) {
	glEndList();
    }
    if (isInterleavedArrays) {
        assert(iaf != NULL);
	free((void *)iaf);
	iaf= NULL;
    }

    glDisableClientState(GL_NORMAL_ARRAY);
}

static void renderDonut(GLuint id, GLfloat *keepPointer)
{
    int ii;   
    {
	static GLfloat stripVertex[128*2][2];
	static GLfloat stripColor[128*2][3];
	int numSteps = 128;
	int numSectors = 3;

	float stepSize = 2 * M_PI / (numSteps - 1);
	float sectorSize = 2 * M_PI / numSectors;
	float innerRadius = 40;
	float outerRadius = 80;
	float xCenter = 225;
	float yCenter = 400;

	GLuint *lotsaIndices= (GLuint*)malloc(sizeof(GLuint) * numSteps * 2);
	assert(lotsaIndices != NULL);

	/* compute colors & vertices */ 
	for (ii = 0; ii < numSteps*2; ii += 2) {
	    float angle = ii/2 * stepSize;
	    float x = cos(angle);
	    float y = sin(angle);
	    float sector = angle / sectorSize;
	    float fade = sector - (int)sector;
	    float r, g, b;
	    stripVertex[ii][0] = innerRadius * x + xCenter;
	    stripVertex[ii][1] = innerRadius * y + yCenter;
	    stripVertex[ii+1][0] = outerRadius * x + xCenter;
	    stripVertex[ii+1][1] = outerRadius * y + yCenter;
	    switch ((int) sector) {
	      case 0: case 3:
		/* red fade to green */
		r = 1.00 * (1 - fade) + 0.25 * fade;
		g = 0.25 * (1 - fade) + 1.00 * fade;
		b = 0.25 * (1 - fade) + 0.25 * fade;
		break;
	      case 1:
		/* green fade to blue */
		r = 0.25 * (1 - fade) + 0.25 * fade;
		g = 1.00 * (1 - fade) + 0.25 * fade;
		b = 0.25 * (1 - fade) + 1.00 * fade;
		break;
	      case 2:
		/* blue fade to red */
		r = 0.25 * (1 - fade) + 1.00 * fade;
		g = 0.25 * (1 - fade) + 0.25 * fade;
		b = 1.00 * (1 - fade) + 0.25 * fade;
		break;
	    }
	    stripColor[ii][0] = stripColor[ii+1][0] = r;
	    stripColor[ii][1] = stripColor[ii+1][1] = g;
	    stripColor[ii][2] = stripColor[ii+1][2] = b;
	    lotsaIndices[ii] = ii;
	    lotsaIndices[ii+1] = ii+1; 
	}

        /* GL_C3F_V2F doesn't exist so make it GL_C3F_V3F instead */
	/* copy colors & vertices into interleaved array with missing Zs 
	 * properly inserted 
	 */
	{
	    int numElementColor= 128*2*3;
	    int numElementVertex= 128*2*2;
	    int numVerts= 128 * 2;
	    int numFloats= numElementColor + numElementVertex + numVerts;
	    int count= 0;
	    int ii;

	    assert(numFloats <= MAX_ELEMENTS); 
	    /* copy arrays into interleaved array with missing Zs properly
	     * inserted
	     */
	    assert(keepPointer != NULL);
	    for (ii = 0; ii < numSteps*2; ii++) {
	       keepPointer[count++] = stripColor[ii][0];
	       keepPointer[count++] = stripColor[ii][1];
	       keepPointer[count++] = stripColor[ii][2];

	       keepPointer[count++] = stripVertex[ii][0];
	       keepPointer[count++] = stripVertex[ii][1];
	       keepPointer[count++] = 0.0; /* insert missing Z coord */
	    }
	    assert(count == numFloats);
	}
	glPopClientAttrib();        
	/* check if values are same as for lower-left triangle */
	{			
	    GLint size, type, stride;
	    GLvoid *ptr;

	    assert(glIsEnabled(GL_COLOR_ARRAY) == GL_TRUE);
	    assert(glIsEnabled(GL_VERTEX_ARRAY) == GL_TRUE);

	    assert(glIsEnabled(GL_INDEX_ARRAY) == GL_FALSE);
	    assert(glIsEnabled(GL_EDGE_FLAG_ARRAY) == GL_FALSE);
	    assert(glIsEnabled(GL_TEXTURE_COORD_ARRAY) == GL_FALSE);
	    assert(glIsEnabled(GL_NORMAL_ARRAY) == GL_FALSE);

	    glGetIntegerv(GL_COLOR_ARRAY_SIZE, &size);
	    assert(size == 3);
	    glGetIntegerv(GL_COLOR_ARRAY_TYPE, &type);
	    assert(type == GL_FLOAT);
	    glGetIntegerv(GL_COLOR_ARRAY_STRIDE, &stride);
	    assert(stride == SKIP);
	    glGetPointerv(GL_COLOR_ARRAY_POINTER, &ptr);
	    assert(ptr == keepPointer);

	    glGetIntegerv(GL_VERTEX_ARRAY_SIZE, &size);
	    assert(size == 3);
	    glGetIntegerv(GL_VERTEX_ARRAY_TYPE, &type);
	    assert(type == GL_FLOAT);
	    glGetIntegerv(GL_VERTEX_ARRAY_STRIDE, &stride);
	    assert(stride == SKIP);
	    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &ptr);
	    assert(ptr == ((char *)keepPointer + (3 * sizeof(GLfloat))));
	}

	if (isDList) {
	    glNewList(id, dlMode);
	}
	switch (renderVertexArrayMode) {
	case ARRAYELEMENT:
	    glBegin(GL_TRIANGLE_STRIP);
	    for (ii = 0; ii < numSteps*2; ii += 2) {
		glArrayElement(ii);
		glArrayElement(ii+1);
	    }
	    glEnd();
	    break;
        case DRAWARRAYS:
	    glDrawArrays(GL_TRIANGLE_STRIP, 0, numSteps*2);
	    break;
	case DRAWELEMENTS:
	    glDrawElements(GL_TRIANGLE_STRIP, numSteps*2, 
			   GL_UNSIGNED_INT,lotsaIndices);
	    break;
        default:
	    assert(0);
	    break;
	}
	if (isDList) {
	    glEndList();
	}

	free((void *)lotsaIndices);
    }

    glDisableClientState(GL_COLOR_ARRAY); 
}

static void draw(void)
{
    GLfloat *keepPointer;

    if (isClear) {
       glClear(GL_COLOR_BUFFER_BIT);
       glFlush();
       isClear= 0; 
       return; 
    }

    keepPointer= (GLfloat *)malloc(sizeof(GLfloat) * MAX_ELEMENTS);
    assert(keepPointer != NULL);

    if (!isDList) {		/* clear for immediate mode */
       glClear(GL_COLOR_BUFFER_BIT);      
    }

    testClientAttributeStack();

    assert(glIsEnabled(GL_INDEX_ARRAY) == GL_FALSE);
    assert(glIsEnabled(GL_EDGE_FLAG_ARRAY) == GL_FALSE);
    assert(glIsEnabled(GL_TEXTURE_COORD_ARRAY) == GL_FALSE);
    assert(glIsEnabled(GL_COLOR_ARRAY) == GL_FALSE);
    assert(glIsEnabled(GL_NORMAL_ARRAY) == GL_FALSE);
    assert(glIsEnabled(GL_VERTEX_ARRAY) == GL_FALSE);

    /* save default client state */
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);

    renderLowerLeftTriangle(dlistIndices[0], keepPointer);
    renderLowerRightTriangle(dlistIndices[1]);
    renderUpperLeftQuad(dlistIndices[2]);
    renderUpperRightEdge(dlistIndices[3]);
    renderMiddleQuad(dlistIndices[4]);
    renderDonut(dlistIndices[5], keepPointer);

    /* restore default client state */
    glPopClientAttrib();
    {				/* check if default state for pointers */
        GLvoid *ptr;
	
	glGetPointerv(GL_INDEX_ARRAY_POINTER, &ptr); assert(ptr == NULL);
	glGetPointerv(GL_EDGE_FLAG_ARRAY_POINTER, &ptr); assert(ptr == NULL); 

	glGetPointerv(GL_TEXTURE_COORD_ARRAY_POINTER, &ptr); assert(ptr == NULL); 
	glGetPointerv(GL_COLOR_ARRAY_POINTER, &ptr); assert(ptr == NULL); 
	glGetPointerv(GL_NORMAL_ARRAY_POINTER, &ptr); assert(ptr == NULL); 
	glGetPointerv(GL_VERTEX_ARRAY_POINTER, &ptr); assert(ptr == NULL); 
    }

    if (isDList) {
        glClear(GL_COLOR_BUFFER_BIT); 
        glCallLists(6,GL_UNSIGNED_INT,dlistIndices);
    }
    if (isDList) {
        glDeleteLists(1,6);
    }

    CHECK_ERROR
    glFlush();

    if (isGets) {
       testGets();
       isGets = 0; 
    }

    free((void *)keepPointer);
}

static void args(int argc, char **argv)
{
    GLint ii;

    isRGB = GL_TRUE;

    for (ii = 1; ii < argc; ii++) {
	if (strcmp(argv[ii], "-ci") == 0) {
	    isRGB = GL_FALSE;
	} else if (strcmp(argv[ii], "-rgb") == 0) {
	    isRGB = GL_TRUE;
	}
    }
}

int main(int argc, char *argv[])
{
    GLenum type;

    glutInit(&argc, argv);
    args(argc, argv);

    type = GLUT_SINGLE;
    type |= (isRGB) ? GLUT_RGB : GLUT_INDEX;
    glutInitDisplayMode(type);
    glutInitWindowSize(500, 500);
    window= glutCreateWindow("Vertex Array demo");

    init();

    glutReshapeFunc(reshape);
    glutKeyboardFunc(key);
    glutDisplayFunc(draw);
    glutMainLoop();
}

