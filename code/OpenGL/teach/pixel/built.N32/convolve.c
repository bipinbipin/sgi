#include <GL/glx.h>
#include <GL/gl.h>

#include <gl/image.h>

#include <stdio.h>

#include <Xm/MainW.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/ToggleB.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/Scale.h>
#include <Xm/FileSB.h>
#include <Xm/SelectioB.h>
#include <Xm/MessageB.h>

#include <GL/GLwMDrawA.h>


#include <math.h>

/* size of squares in test image */
#define GRID 16

/* To compile: cc -o convolve convolve.c -lGL -lX11 -limage */

/* Motif Stuff */

static String fallbackResources[] = {
    "*sgiMode: true",
    "*useSchemes: all",
    "*title: Convolution Demo Program",
    "*glxarea*width: 256",
    "*glxarea*height: 256",
    NULL
};

XtWorkProcId workId = 0;
Widget       toplevel, mainw, frame, glxarea, form;
Widget       control, fwidth, fheight;
Widget       actions, load, readit, copy, draw, makeconv2d, makesep2d, texmap;
Widget       makeconv1d;
Widget       enables, enable1d, enable2d, enableSep2d;
Widget       enabletex1d, enabletex2d;
Widget       filter_format, filter_type, filter_contents, filters;
Widget       image_format, image_type, image_contents;
Widget       pzoom, xzoom, yzoom;
Widget       getconv1dfilter, getconv2dfilter, getsepfilter;
Widget       copyfilter1d, copyfilter2d;
Widget       newlist, endlist, calllist;
Arg          args[1];


/*
** Generate a pop-up if an OpenGL error is detected. Keep popping up until
** all errors are shown.
*/
void check_error(char *string)
{
    Widget glerror;
    char message_string[1000];
    XmString error_message;
    GLenum error;
    while((error = glGetError()) != GL_NO_ERROR) {
	glerror = XmCreateErrorDialog(toplevel, "gl Error", NULL, 0);
	sprintf(message_string, "%s: %s", 
		string,
		(char *)gluErrorString(error)); 
	error_message = XmStringCreateLocalized(message_string);
	XtVaSetValues(glerror, XmNmessageString, error_message, NULL);
	XtManageChild(glerror);
	XmStringFree(error_message);
    }	
}

/*
** The following two structures are used for bookkeeping details about
** the current image and the current filter.
** The create function is called to create or resize a filter or image.
** the function is called with a pointer to the appropriate structure,
** with the desired information filled in. If the creation function can't
** meet all the criteria described in the data, it changes the value to
** on it can meet. 
**
** For example, if a filter of a certain size is desired, but the filter
** is defined to be a fixed size, the width and height information are
** changed to match the fixed size filter created. The calling routine
** must check for changed values.
*/


/*
** Current filter
*/

typedef enum {BOX, SOBEL, LAPLACE} FilterContents;

typedef struct filter_info {
    /* used as arguments to OpenGL calls */
    GLsizei width;
    GLsizei height;
    GLenum internalformat;
    GLenum format;
    GLenum type;
    /* Destination format and type for glGet*Filter() calls */
    GLenum get_target;
    GLenum get_format;
    GLenum get_type;
    /* Internal filter information */
    void (*create)(struct filter_info *filt);
    FilterContents contents;
    unsigned char *filter;
    unsigned char *row; /* if filter is separable */
    unsigned char *col; /* if filter is separable */
} Filter;


/*
** Current Image
*/

typedef enum {SQUARES, CHECKERBOARD, FILES} ImageContents;

typedef struct image_info {
    /* used as arguments to OpenGL calls */
    GLenum format;
    GLenum type;
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
    GLfloat rasterposx;
    GLfloat rasterposy;
    /* Internal image information */
    void (*create)(struct image_info *image);
    char filename[100]; /* filename of file read to make image */
    ImageContents contents;
    unsigned int wid; /* actual dimensions and size of filter */
    unsigned int ht;
    unsigned char *image;
} Image;


/*
** Global filter and image
*/

Image image;
Filter filter;



int format_size(GLenum format)
{
    switch(format) {
    case GL_RGB:
	return 3;
    case GL_LUMINANCE_ALPHA:
        return 2;
    case GL_RGBA:
    case GL_ABGR_EXT:
        return 4;
    default:
        return 1;
    }
}

int type_size(GLenum type)
{
   switch(type) {
      case GL_UNSIGNED_SHORT:
      case GL_SHORT:
        return 2;
      case GL_UNSIGNED_BYTE:
      case GL_BYTE:
        return 1;
      case GL_INT:
      case GL_UNSIGNED_INT:
      case GL_FLOAT:
      default:
        return 4;
    }
}


/*
** Convert an element from a float to the destination type
*/
void write_type(float *source, unsigned char *dest, GLenum type)
{
    switch(type) {
    case GL_UNSIGNED_BYTE:
	*(GLubyte *)dest = (GLubyte)(*source * 255.);
	break;
    case GL_BYTE:
	*(GLbyte *)dest = (GLbyte)(*source * 127.);
	break;
    case GL_UNSIGNED_SHORT:
	*(GLushort *)dest = (GLushort)(*source * 65535.);
	break;
    case GL_SHORT:
	*(GLshort *)dest = (GLshort)(*source * 32767.);
	break;
    case GL_UNSIGNED_INT:
	*(GLuint *)dest = (GLuint)(*source * 4292965000);
	break;
    case GL_INT:
	*(GLint *)dest = (GLint)(*source * 2147482000);
	break;
    case GL_FLOAT:
	*(GLfloat *)dest = *source;
	break;
    default:
	fprintf(stderr, "write_type: error, invalid type: %d\n", (int)type);
	break;
    }
}

/*
** Copy an element from an array of float RGBA elements in the proper
** format and type of the destination void * array
*/
void write_element(float *source, unsigned char **dest, 
		   GLenum format, GLenum type)
{

    float *src = source;
    float tmp; /* for luminance, luminance_alpha, intensity */

    switch(format) {
    case GL_RGBA:
	write_type(src, *dest, type);
	src++;
	*dest += type_size(type);
	write_type(src, *dest, type);
	src++;
	*dest += type_size(type);
	write_type(src, *dest, type);
	src++;
	*dest += type_size(type);
	write_type(src, *dest, type);
	src++;
	*dest += type_size(type);
	break;
    case GL_ABGR_EXT:
	write_type(src + 3, *dest, type);
	*dest += type_size(type);
	write_type(src + 2, *dest, type);
	*dest += type_size(type);
	write_type(src + 1, *dest, type);
	*dest += type_size(type);
	write_type(src, *dest, type);
	*dest += type_size(type);
	break;
    case GL_RGB:
	write_type(src, *dest, type);
	src++;
	*dest += type_size(type);
	write_type(src, *dest, type);
	src++;
	*dest += type_size(type);
	write_type(src, *dest, type);
	src++;
	*dest += type_size(type);
	break;
    case GL_LUMINANCE_ALPHA:
	tmp = src[0] + src[1] + src[2];
	write_type(&tmp, *dest, type);
	*dest += type_size(type);
	write_type(src + 3, *dest, type);
	*dest += type_size(type);
	break;
    case GL_LUMINANCE:
	tmp = src[0] + src[1] + src[2];
	if(tmp > 1.0) tmp = 1.0;
	write_type(&tmp, *dest, type);
	*dest += type_size(type);
	break;
    case GL_INTENSITY_EXT:
	tmp = src[0] + src[1] + src[2] + src[3];
	if(tmp > 1.0) tmp = 1.0;
	write_type(&tmp, *dest, type);
	*dest += type_size(type);
	break;
    }
}


void create_sobel(Filter *filter_info)
{
    static float sbl[] = {
	-0.5,-0.5,-0.5,1.0, 0.0,0.0,0.0,1.0,  0.5,0.5,0.5,1.0,
	-1.0,-1.0,-1.0,1.0, 0.0,0.0,0.0,1.0,  1.0,1.0,1.0,1.0,
	-0.5,-0.5,-0.5,1.0, 0.0,0.0,0.0,1.0,  0.5,0.5,0.5,1.0,
    };
    static float sbl_row[] = {-1.,-1.,-1., 1., 0., 0., 0., 1., 1., 1., 1., 1.};
    static float sbl_col[] = { 1., 1., 1., 1., 2., 2., 2., 1., 1., 1., 1., 1.};
    int i, j;    
    unsigned char *f;
    float *s;

    filter_info->create = create_sobel; /* for resizing */
    filter_info->contents = SOBEL;

    filter_info->width = 3;
    filter_info->height = 3;

    if(filter_info->filter) free(filter_info->filter);
    if(filter_info->row) free(filter_info->row);
    if(filter_info->col) free(filter_info->col);

    filter_info->filter = (unsigned char *)malloc(filter_info->width *
					      filter_info->height *
					      format_size(filter_info->format) *
					      type_size(filter_info->type));

    filter_info->row = (unsigned char *)malloc(filter_info->width *
					 format_size(filter_info->format) *
					 type_size(filter_info->type));

    filter_info->col = (unsigned char *)malloc(filter_info->height *
					 format_size(filter_info->format) *
					 type_size(filter_info->type));

    
    s = sbl;
    f = filter_info->filter;
    for(j = 0;j < 3;j++) {
	for(i = 0;i < 3;i++) {
	    write_element(s, &f, filter_info->format, filter_info->type);
	    s += 4;
	}
    }

    s = sbl_row;
    f = filter_info->row;
    for(i = 0;i < 3;i++) {
	write_element(s, &f, filter_info->format, filter_info->type);
	s += 4;
    }

    s = sbl_col;
    f = filter_info->col;
    for(j = 0;j < 3;j++) {
	write_element(s, &f, filter_info->format, filter_info->type);
	s += 4;
    }
}


/*
** Create a laplacian filter
*/
void create_laplace(Filter *filter_info)
{
    float lp[] = {
	0.00,0.00,0.00,1.00,   0.25, 0.25, 0.25,1.00,  0.00,0.00,0.00,1.00,
	0.25,0.25,0.25,1.00,  -1.00,-1.00,-1.00,1.00,  0.25,0.25,0.25,1.00,
	0.00,0.00,0.00,1.00,   0.25, 0.25, 0.25,1.00,  0.00,0.00,0.00,1.00,
    };
    int i, j;    
    unsigned char *f;
    float *s;

    filter_info->create = create_laplace; /* for resizing */
    filter_info->contents = LAPLACE;

    filter_info->width = 3;
    filter_info->height = 3;

    if(filter_info->filter) free(filter_info->filter);
    if(filter_info->row) free(filter_info->row);
    if(filter_info->col) free(filter_info->col);

    filter_info->filter = (unsigned char *)malloc(filter_info->width *
					       filter_info->height *
					      format_size(filter_info->format) *
					      type_size(filter_info->type));

    filter_info->row = NULL;
    filter_info->col = NULL;
    
    s = lp;
    f = filter_info->filter;
    for(j = 0;j < 3;j++) {
	for(i = 0;i < 3;i++) {
	    write_element(s, &f, filter_info->format, filter_info->type);
	    s += 4;
	}
    }
}

/*
** Create a Box filter
*/
void create_box(Filter *filter_info)
{
    int i;
    float value[4];
    unsigned char *f;

    filter_info->create = create_box; /* for resizing */
    filter_info->contents = BOX;

    /* TODO: check for maximum filter width, height? */

    if(filter_info->filter) free(filter_info->filter);
    if(filter_info->row) free(filter_info->row);
    if(filter_info->col) free(filter_info->col);

    filter_info->filter = (unsigned char *)malloc(filter_info->width *
					      filter_info->height *
					      format_size(filter_info->format) *
					      type_size(filter_info->type));

    filter_info->row = (unsigned char *)malloc(filter_info->width *
					 format_size(filter_info->format) *
					 type_size(filter_info->type));

    filter_info->col = (unsigned char *)malloc(filter_info->height *
					 format_size(filter_info->format) *
					 type_size(filter_info->type));


    value[0] = value[1] = value[2] = 1.0/(GLfloat)(filter_info->width * 
						   filter_info->height);
    value[3] = 1.0;

    f = filter_info->filter;
    for(i = 0; i < filter_info->width * filter_info->height; i++) {
	write_element(value, &f, filter_info->format, filter_info->type);
    }

    value[0] = value[1] = value[2] = 1.0/(GLfloat)filter_info->width;
    value[3] = 1.0;
    f = filter_info->row;
    for(i = 0; i < filter_info->width; i++) {
	write_element(value, &f, filter_info->format, filter_info->type);
    }

    value[0] = value[1] = value[2] = 1.0/(GLfloat)filter_info->height;
    value[3] = 1.0;
    f = filter_info->col;
    for(i = 0; i < filter_info->height; i++) {
	write_element(value, &f, filter_info->format, filter_info->type);
    }
}


/*
** Create an image of white squares on a black background
*/
void create_squares(Image *image_info)
{
    unsigned int x, y;
    float zero[4], one[4];
    unsigned char *i;

    zero[0] = 0.; zero[1] = 0.; zero[2] = 0.; zero[3] = 1.;
    one[0] = 1.; one[1] = 1.; one[2] = 1.; one[3] = 1.;

    image_info->create = create_squares; /* for resizing */

    if(image_info->image) free(image_info->image);

    image_info->image = (unsigned char *)malloc(image_info->wid *
					      image_info->ht *
					      format_size(image_info->format) *
					      type_size(image_info->type));

    i = image_info->image;
    for(y = 0; y < image_info->ht; y++) {
	for(x = 0; x < image_info->wid; x++) {
	    if(!(x/GRID & 0x1) & 
	       !(y/GRID & 0x1)) {
		write_element(one, &i, image_info->format, image_info->type);
	    } else {
		write_element(zero, &i, image_info->format, image_info->type);
	    }
	}
    }
}

/*
** Create a checkerboard image
*/

void create_checkerboard(Image *image_info)
{
    unsigned int x, y;
    float zero[4], one[4];
    unsigned char *i;

    zero[0] = 0.; zero[1] = 0.; zero[2] = 0.; zero[3] = 1.;
    one[0] = 1.; one[1] = 1.; one[2] = 1.; one[3] = 1.;

    image_info->create = create_checkerboard; /* for resizing */

    if(image_info->image) free(image_info->image);

    image_info->image = (unsigned char *)malloc(image_info->wid *
					      image_info->ht *
					      format_size(image_info->format) *
					      type_size(image_info->type));

    i = image_info->image;
    for(y = 0; y < image_info->ht; y++) {
	for(x = 0; x < image_info->wid; x++) {
	    if((x/GRID & 0x1) ^ 
	       (y/GRID & 0x1)) {
		write_element(one, &i, image_info->format, image_info->type);
	    } else {
		write_element(zero, &i, image_info->format, image_info->type);
	    }
	}
    }
}

void create_from_file(Image *image_info)
{
    IMAGE *fileimage;
    GLubyte *img, *pixel;
    unsigned short *rbuf, *gbuf, *bbuf;
    int i,j;


    image_info->create = create_from_file; /* for resizing */

    fileimage = iopen(image_info->filename, "r");
    if(!fileimage) { /* open failed */
	fprintf(stderr, "Unable to read rgb image file %s\n", 
		image_info->filename);
	return;
    }    
    
    free(image_info->image);

    /* TODO: fix so format, type can be changed */
    image_info->width = image_info->wid = fileimage->xsize; 
    image_info->height = image_info->ht = fileimage->ysize; 
    
    image_info->type = GL_UNSIGNED_BYTE;
    image_info->format = GL_RGB;
    img = (GLubyte *)malloc(fileimage->xsize * 
			    fileimage->ysize * 
			    3 * 
			    sizeof(GLushort));
    image_info->image = (unsigned char *)img;

    rbuf = (unsigned short *) malloc(fileimage->xsize * sizeof(unsigned short));
    gbuf = (unsigned short *) malloc(fileimage->xsize * sizeof(unsigned short));
    bbuf = (unsigned short *) malloc(fileimage->xsize * sizeof(unsigned short));


    for(j = 0; j < fileimage->ysize; j++) {
	getrow(fileimage, rbuf, j, 0);
	getrow(fileimage, gbuf, j, 1);
	getrow(fileimage, bbuf, j, 2);
	pixel = img + j * fileimage->xsize * 3; /* row pointer */
	for(i = 0; i < fileimage->xsize;i++) {
	    pixel[0] = rbuf[i];
	    pixel[1] = gbuf[i];
	    pixel[2] = bbuf[i];
	    pixel += 3; /* next pixel */
	}
    }

    iclose(fileimage);
    free(rbuf);
    free(gbuf);
    free(bbuf);

/* TODO: update option menus
    XtVaSetValues(filter_format, XmNmenuHistory, button_1, NULL);
    XtVaSetValues(filter_type, XmNmenuHistory, button_3, NULL);
*/
}

void make_image(Image *image_info)
{ 

    switch(image_info->contents) { 
    case SQUARES: /* generate squares on a background */
	create_squares(image_info);
	break;
    case CHECKERBOARD: /* generate a checkerboard pattern */
	create_checkerboard(image_info);
	break;
    case FILES: /* image read in from a file */
	create_from_file(image_info);
	break;
    }
}

void make_filter(Filter *filter_info)
{
    switch(filter_info->contents) {
    case BOX:
	create_box(filter_info);
	break;
    case SOBEL:
	create_sobel(filter_info);
	break;
    case LAPLACE:
	create_laplace(filter_info);
	break;
    }
}

GLfloat pyramid[3 * (3 + 2) * 4]; /* 4 3d Triangles w/ 2d textures */
void make_pyramid()
{
    int i, j, k;
    float angle;

    angle = 120. * M_PI * 2.0 / 360. ;

    k = 0; /* index for loading array */
    for(j = 0; j < 3; j++) { /* number of triangles */
	for(i = j; i < j + 2; i++) { /* finding x & y coordinates */
	    if(i == j) {
		pyramid[k++] = 0.0; /* texture coord */
		pyramid[k++] = 0.0;
	    } else {
		pyramid[k++] = 1.0; /* texture coord */
		pyramid[k++] = 0.0;
	    }
	    pyramid[k++] = fcos(i * angle);
	    pyramid[k++] = fsin(i * angle);
	    pyramid[k++] = -1.0;
	}
	pyramid[k++] = 0.5; /* texture coord */
	pyramid[k++] = 1.0;

	pyramid[k++] = 0.;
	pyramid[k++] = 0.;
	pyramid[k++] = 0.;
    } 
    /* now; last triangle */
    for(i = 0; i < 3; i++) { /* finding x & y coordinates */
	switch(i) {
	case 0:
	    pyramid[k++] = 0.0; /* texture coord */
	    pyramid[k++] = 0.0;
	    break;
	case 1:
	    pyramid[k++] = 1.0; /* texture coord */
	    pyramid[k++] = 0.0;
	    break;
	case 2:
	    pyramid[k++] = 0.5; /* texture coord */
	    pyramid[k++] = 1.0;
	    break;
	}
	pyramid[k++] = fcos(i * angle);
	pyramid[k++] = fsin(i * angle);
	pyramid[k++] = -1.0;
    }
}

void clear_screen(Widget w, XtPointer data, XtPointer callData)
{
    glClear(GL_COLOR_BUFFER_BIT);
}


void draw_tex(Widget w, XtPointer data, XtPointer callData)
{
    GLint zoomx, zoomy;
    int i;

    XmScaleGetValue(xzoom, &zoomx);
    XmScaleGetValue(yzoom, &zoomy);
    
    glPixelZoom(zoomx/100., zoomy/100.);

    if(glIsEnabled(GL_TEXTURE_1D))
	gluBuild1DMipmaps(GL_TEXTURE_1D, 4, image.wid,
			  image.format, image.type, image.image);
    if(glIsEnabled(GL_TEXTURE_2D))
	gluBuild2DMipmaps(GL_TEXTURE_2D, 4, image.wid, image.ht,
			  image.format, image.type, image.image);

    glBegin(GL_TRIANGLES);
    glTexCoord2f(0.,0.);
    glVertex3f(-1.,-1.,0.);
    glTexCoord2f(1.,0.);
    glVertex3f(1.,-1.,0.);
    glTexCoord2f(.5,1.);
    glVertex3f(0.,1.,0.);
    glEnd();

}


void load_image(Widget w, XtPointer data, XtPointer callData)
{
	make_image(&image);
}

void draw_pix(Widget w, XtPointer data, XtPointer callData)
{
    GLint zoomx, zoomy;

    XmScaleGetValue(xzoom, &zoomx);
    XmScaleGetValue(yzoom, &zoomy);
    
    glPixelZoom(zoomx/100., zoomy/100.);

    glRasterPos3f(-1.0, -1.0, 0.0);

    glDrawPixels(image.wid, image.ht, image.format, image.type,
		 image.image);
}

void read_pix(Widget w, XtPointer data, XtPointer callData)
{
    GLint zoomx, zoomy;

    XmScaleGetValue(xzoom, &zoomx);
    XmScaleGetValue(yzoom, &zoomy);
    
    glPixelZoom(zoomx/100., zoomy/100.);

    glReadPixels(0, 0, image.wid, image.ht, image.format, image.type,
		 image.image);
}

void copy_pix(Widget w, XtPointer data, XtPointer callData)
{
    GLint zoomx, zoomy;

    XmScaleGetValue(xzoom, &zoomx);
    XmScaleGetValue(yzoom, &zoomy);
    
    glPixelZoom(zoomx/100., zoomy/100.);

    glRasterPos2f(image.rasterposx, image.rasterposy);
    glCopyPixels(image.x, image.y, image.width, image.height, GL_COLOR);
    glRasterPos2f(-1.0, -1.0); /* restore raster position */
}

void enable_1d(Widget w, XtPointer data, XtPointer call_data)
{
    if(((XmToggleButtonCallbackStruct *)call_data)->set)
	glEnable(GL_CONVOLUTION_1D_EXT);
    else
	glDisable(GL_CONVOLUTION_1D_EXT);
}

void enable_2d(Widget w, XtPointer data, XtPointer call_data)
{
    if(((XmToggleButtonCallbackStruct *)call_data)->set)
	glEnable(GL_CONVOLUTION_2D_EXT);
    else
	glDisable(GL_CONVOLUTION_2D_EXT);
}

void enable_tex_1d(Widget w, XtPointer data, XtPointer call_data)
{
    if(((XmToggleButtonCallbackStruct *)call_data)->set)
	glEnable(GL_TEXTURE_1D);
    else
	glDisable(GL_TEXTURE_1D);
}

void enable_tex_2d(Widget w, XtPointer data, XtPointer call_data)
{
    if(((XmToggleButtonCallbackStruct *)call_data)->set)
	glEnable(GL_TEXTURE_2D);
    else
	glDisable(GL_TEXTURE_2D);
}

void enable_sep_2d(Widget w, XtPointer data, XtPointer call_data)
{
    if(((XmToggleButtonCallbackStruct *)call_data)->set)
	glEnable(GL_SEPARABLE_2D_EXT);
    else
	glDisable(GL_SEPARABLE_2D_EXT);
}

void make_conv_1d(Widget w, XtPointer data, XtPointer callData)
{
    int wid, ht;

    XmScaleGetValue(fwidth, &wid);
    XmScaleGetValue(fheight, &ht);
    filter.width = wid;
    filter.height = ht;
    (*filter.create)(&filter);

    /* load filter */
    glConvolutionFilter1DEXT(GL_CONVOLUTION_1D_EXT, 
			     filter.internalformat,
			     filter.width,
			     filter.format,
			     filter.type,
			     filter.row);
    check_error("glConvolutionFilter1DEXT");
}

void make_conv_2d(Widget w, XtPointer data, XtPointer callData)
{
    int wid, ht;

    XmScaleGetValue(fwidth, &wid);
    XmScaleGetValue(fheight, &ht);
    filter.width = wid;
    filter.height = ht;
    (*filter.create)(&filter);

    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    
    /* load filter */
    glConvolutionFilter2DEXT(GL_CONVOLUTION_2D_EXT, 
			     filter.internalformat,
			     filter.width, filter.height,
			     filter.format,
			     filter.type,
			     filter.filter);
    check_error("glConvolutionFilter2DEXT");
}

void make_sep_2d(Widget w, XtPointer data, XtPointer callData)
{
    int wid, ht;

    XmScaleGetValue(fwidth, &wid);
    XmScaleGetValue(fheight, &ht);
    filter.width = wid;
    filter.height = ht;
    (*filter.create)(&filter);

    /* load filter */
    glSeparableFilter2DEXT(GL_SEPARABLE_2D_EXT, 
			   filter.internalformat,
			   filter.width, filter.height,
			   filter.format,
			   filter.type,
			   filter.row,
			   filter.col);
    check_error("glSeparableFilter2DEXT");
}

void new_list(Widget w, XtPointer data, XtPointer callData)
{
    if(glIsList(1)) {
	check_error("glIsList");
	glDeleteLists(1,1);
	check_error("glDeleteLists");
    }
    glNewList(1, GL_COMPILE);
    check_error("glNewList");
}

void end_list(Widget w, XtPointer data, XtPointer callData)
{
    glEndList();
    check_error("glEndList");
}

void call_list(Widget w, XtPointer data, XtPointer callData)
{
    glCallList(1);
    check_error("glCallList");
}


/*
** Return number of characters needed to print a given type
*/
int print_type_size(GLenum type)
{
    return(4); /* hardwired for GL_FLOAT (X.XX or %.2f) */
}
/*
** Return number of characters needed to print a given format & type
*/
int print_format_size(GLenum format, GLenum type)
{
   return (print_type_size(type) + 2) * 4 + 1; /* hardwired for RGBA */
}

void get_conv_1d_filter(Widget w, XtPointer data, XtPointer callData)
{
    GLfloat *filter_image, *f;
    Widget filter_output;
    XmString filter_elements;
    char *filter_text, *ft;
    GLint wid, ht;
    int i, j;

    glGetConvolutionParameterivEXT(GL_CONVOLUTION_1D_EXT,
				  GL_CONVOLUTION_WIDTH_EXT,
				  &wid);
    check_error("glGetConvolutionParameterivEXT");

    ht = 1;
    filter_image = (GLfloat *)malloc(4 * sizeof(GLfloat) * wid);

    glGetConvolutionFilterEXT(GL_CONVOLUTION_1D_EXT,
			      filter.get_format,
			      filter.get_type,
			      filter_image);
	check_error("glGetConvolutionFilterEXT");

    
/*
**     TODO: make so format and type can vary.
**     4 components/element, 6 chars/component, 1 comma + 3 space/element,
**     2 newline/line
*/
    filter_text = (char *)malloc(26 * wid + 3);

    ft = filter_text;
    for(f = filter_image, i = 0; i < wid; i++) {
	ft += sprintf(ft, "%.2f  ",  *f++);
	ft += sprintf(ft, "%.2f  ",  *f++);
	ft += sprintf(ft, "%.2f  ",  *f++);
	ft += sprintf(ft, "%.2f,   ", *f++);
    }

    filter_output = XmCreateMessageDialog(toplevel, "FilterContents", NULL, 0);
    XtUnmanageChild(XmMessageBoxGetChild(filter_output, 
					 XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(filter_output, 
					 XmDIALOG_HELP_BUTTON));
    filter_elements = XmStringCreateLtoR(filter_text,
					 XmFONTLIST_DEFAULT_TAG);
    XtVaSetValues(filter_output, XmNmessageString, filter_elements, NULL);
    XmStringFree(filter_elements);
    XtManageChild(filter_output);
    free(filter_text);
    free(filter_image);
}

void get_conv_2d_filter(Widget w, XtPointer data, XtPointer callData)
{
    GLfloat *filter_image, *f;
    Widget filter_output;
    XmString filter_elements;
    char *filter_text, *ft;
    GLint wid, ht;
    int i, j;

    glGetConvolutionParameterivEXT(GL_CONVOLUTION_2D_EXT,
				  GL_CONVOLUTION_WIDTH_EXT,
				  &wid);
    check_error("glGetConvolutionParameterivEXT");

    glGetConvolutionParameterivEXT(filter.get_target,
				   GL_CONVOLUTION_HEIGHT_EXT,
				   &ht);
    check_error("glGetConvolutionParameterivEXT");

    filter_image = (GLfloat *)malloc(4 * sizeof(GLfloat) * wid * ht);

    glGetConvolutionFilterEXT(GL_CONVOLUTION_2D_EXT,
			      filter.get_format,
			      filter.get_type,
			      filter_image);
	check_error("glGetConvolutionFilterEXT");

    
/*
**     TODO: make so format and type can vary.
**     4 components/element, 6 chars/component, 1 comma + 3 space/element,
**     2 newline/line
*/
    filter_text = (char *)malloc((26 * wid + 2) * ht + 1);

    ft = filter_text;
    for(j = 0; j < ht; j++) { /* assuming output is RGBA */
	for(f = filter_image, i = 0; i < wid; i++) {
	    ft += sprintf(ft, "%.2f  ",  *f++);
	    ft += sprintf(ft, "%.2f  ",  *f++);
	    ft += sprintf(ft, "%.2f  ",  *f++);
	    ft += sprintf(ft, "%.2f,   ", *f++);
	}
	if(j < ht - 1)
	    ft += sprintf(ft, "\n\n");
    }

    filter_output = XmCreateMessageDialog(toplevel, "FilterContents", NULL, 0);
    XtUnmanageChild(XmMessageBoxGetChild(filter_output, 
					 XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(filter_output, 
					 XmDIALOG_HELP_BUTTON));
    filter_elements = XmStringCreateLtoR(filter_text,
					 XmFONTLIST_DEFAULT_TAG);
    XtVaSetValues(filter_output, XmNmessageString, filter_elements, NULL);
    XmStringFree(filter_elements);
    XtManageChild(filter_output);
    free(filter_text);
    free(filter_image);
}

void get_sep_filter(Widget w, XtPointer data, XtPointer callData)
{
    GLfloat *row, *col, *f;
    Widget filter_output;
    XmString filter_elements;
    char *filter_text, *ft;
    GLint wid, ht;
    int i;

    /* which convolution filter are we using? */

    glGetConvolutionParameterivEXT(GL_SEPARABLE_2D_EXT,
				  GL_CONVOLUTION_WIDTH_EXT,
				  &wid);
    check_error("glGetConvolutionParameterivEXT");
    glGetConvolutionParameterivEXT(GL_SEPARABLE_2D_EXT,
				  GL_CONVOLUTION_HEIGHT_EXT,
				  &ht);
    check_error("glGetConvolutionParameterivEXT");

    row = (GLfloat *)malloc(4 * sizeof(GLfloat) * wid);
    col = (GLfloat *)malloc(4 * sizeof(GLfloat) * ht);

    glGetSeparableFilterEXT(GL_SEPARABLE_2D_EXT,
			    filter.get_format,
			    filter.get_type,
			    row,
			    col,
			    NULL);
    check_error("glGetSeparableFilterEXT");


/*
**     TODO: make so format and type can vary.
**     4 components/element, 6 chars/component, 1 comma + 3 space/element,
**     2 newline/line
*/
    filter_text = (char *)malloc(26 * wid + 26 * ht + 15);

    ft = filter_text;
    ft += sprintf(ft, "Row:  ");
    for(f = row, i = 0; i < wid; i++) { /* assuming output is RGBA */
	ft += sprintf(ft, "%.2f  ",  *f++);
	ft += sprintf(ft, "%.2f  ",  *f++);
	ft += sprintf(ft, "%.2f  ",  *f++);
	ft += sprintf(ft, "%.2f,   ", *f++);
    }

    ft += sprintf(ft, "\n\n Col:  ");
    for(f = col, i = 0; i < ht; i++) { /* assuming output is RGBA */
	ft += sprintf(ft, "%.2f  ",  *f++);
	ft += sprintf(ft, "%.2f  ",  *f++);
	ft += sprintf(ft, "%.2f  ",  *f++);
	ft += sprintf(ft, "%.2f,   ", *f++);
    }

    filter_output = XmCreateMessageDialog(toplevel, "FilterContents", NULL, 0);
    XtUnmanageChild(XmMessageBoxGetChild(filter_output, 
					 XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(filter_output, 
					 XmDIALOG_HELP_BUTTON));
    filter_elements = XmStringCreateLtoR(filter_text,
					 XmFONTLIST_DEFAULT_TAG);
    XtVaSetValues(filter_output, XmNmessageString, filter_elements, NULL);
    XmStringFree(filter_elements);
    XtManageChild(filter_output);
    free(filter_text);
    free(row);
    free(col);
}

void copy_filter_1d(Widget w, XtPointer data, XtPointer callData)
{
    glCopyConvolutionFilter1DEXT(GL_CONVOLUTION_1D_EXT, GL_RGB, 
				 image.x, image.y, 
				 image.width);
    check_error("glCopyConvolutionFilter1DEXT");

    XmScaleSetValue(fwidth, image.width);
}

void copy_filter_2d(Widget w, XtPointer data, XtPointer callData)
{
    glCopyConvolutionFilter2DEXT(GL_CONVOLUTION_2D_EXT, GL_RGB, 
				 image.x, image.y, 
				 image.width, image.height);
    check_error("glCopyConvolutionFilter2DEXT");
    XmScaleSetValue(fwidth, image.width);
    XmScaleSetValue(fheight, image.height);
}


GLboolean made_current = GL_FALSE;

void resize(Widget w, XtPointer data, XtPointer callData)
{
    Dimension wid, ht;

    if(made_current) {
	XtVaGetValues(w, XmNwidth, &wid, XmNheight, &ht, NULL);
	glViewport(0,0, (GLint) wid, (GLint) ht);
	check_error("glViewport");
	image.wid = wid;
	image.ht = ht;
	make_image(&image);
    }
}

void map_state_changed(Widget w, XtPointer data, XEvent *event, Boolean *cont)
{
    switch(event->type) {
    case MapNotify:
	break;
    case UnmapNotify:
	break;
    }
}

void get_filename(Widget w, XtPointer data, XtPointer callData)
{
    char *filename;

    XmFileSelectionBoxCallbackStruct *cbs =
	(XmFileSelectionBoxCallbackStruct *) callData;

    if(cbs->reason == XmCR_OK) {
	if(!XmStringGetLtoR(cbs->value, XmFONTLIST_DEFAULT_TAG, &filename))
	    return; /* internal error */

	strcpy(image.filename, filename);
	XtFree(filename);
	create_from_file(&image);
    }
    XtPopdown(XtParent(w));
}

void cancel_file(Widget w, XtPointer data, XtPointer callData)
{
    XtPopdown(XtParent(w));
    return;
}

/*
** Set the type of image to work on
*/
void set_image_contents(Widget w, XtPointer data, XtPointer callData)
{
    Widget dialog;
    XmString dir;

    image.contents = (ImageContents)data;

    switch((ImageContents)data) {
    case SQUARES:
	create_squares(&image);
	break;
    case CHECKERBOARD:
	create_checkerboard(&image);
	break;
    case FILES:
	dir = XmStringCreateLocalized("/usr/demos/data/images/*.rgb"); /**/
	dialog = XmCreateFileSelectionDialog(toplevel, "imagefile", NULL, 0);
	XtAddCallback(dialog, XmNokCallback, get_filename, NULL);
	XtAddCallback(dialog, XmNcancelCallback, get_filename, NULL);
	XtSetSensitive(XmSelectionBoxGetChild(dialog, XmDIALOG_HELP_BUTTON),
		       False);
	XmFileSelectionDoSearch(dialog, dir);
	XtManageChild(dialog);
	XmStringFree(dir);
	break;
    }
}

void set_image_format(Widget w, XtPointer data, XtPointer callData)
{
    switch((int)data) {
    case 0: /* RGBA */
	image.format = GL_RGBA;
	break;
    case 1: /* RGB */
	image.format = GL_RGB;
	break;
    case 2: /* Luminance */
	image.format = GL_LUMINANCE;
	break;
    case 3: /* Luminance-Alpha */
	image.format = GL_LUMINANCE_ALPHA;
	break;
    case 4: /* Intensity */
	image.format = GL_INTENSITY_EXT;
	break;
    }
    make_image(&image);
}

void set_image_type(Widget w, XtPointer data, XtPointer callData)
{
    switch((int)data) {
    case 0: 
	image.type = GL_BYTE;
	break;
    case 1: 
	image.type = GL_UNSIGNED_BYTE;
	break;
    case 2: 
	image.type = GL_SHORT;
	break;
    case 3: 
	image.type = GL_UNSIGNED_SHORT;
	break;
    case 4: 
	image.type = GL_INT;
	break;
    case 5: 
	image.type = GL_UNSIGNED_INT;
	break;
    case 6: 
	image.type = GL_FLOAT;
	break;
    }
    make_image(&image);
}


void set_filter_format(Widget w, XtPointer data, XtPointer callData)
{
    switch((int)data) {
    case 0: /* RGBA */
	filter.format = GL_RGBA;
	break;
    case 1: /* RGB */
	filter.format = GL_RGB;
	break;
    case 2: /* Luminance */
	filter.format = GL_LUMINANCE;
	break;
    case 3: /* Luminance-Alpha */
	filter.format = GL_LUMINANCE_ALPHA;
	break;
    case 4: /* Intensity */
	filter.format = GL_INTENSITY_EXT;
	break;
    }
    make_filter(&filter);
}

void set_filter_type(Widget w, XtPointer data, XtPointer callData)
{
    switch((int)data) {
    case 0: 
	filter.type = GL_BYTE;
	break;
    case 1: 
	filter.type = GL_UNSIGNED_BYTE;
	break;
    case 2: 
	filter.type = GL_SHORT;
	break;
    case 3: 
	filter.type = GL_UNSIGNED_SHORT;
	break;
    case 4: 
	filter.type = GL_INT;
	break;
    case 5: 
	filter.type = GL_UNSIGNED_INT;
	break;
    case 6: 
	filter.type = GL_FLOAT;
	break;
    }
    make_filter(&filter);
}

void set_filter_contents(Widget w, XtPointer data, XtPointer callData)
{
    switch((int)data) {
    case 0: /* Box Filter */
	create_box(&filter);
	break;
    case 1: /* Laplace Filter */
	create_laplace(&filter);
	break;
    case 2: /* Sobel Filter */
	create_sobel(&filter);
	break;
    default:
	break;
    }
}

/*
** Read mouse (or any other) input events to the drawing window.
** for now, it is used to specify the source and dest of copy pixels
*/
void mouse_input(Widget w, XtPointer client_data, XtPointer call_data)
{
    GLint x,y;
    static int push_state = 0; /* 0 source, 1 dest */
    XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct *)call_data;
    XEvent *event = cbs->event;
    GLfloat sx, sy;

    
    sx = 2.0/(GLfloat)image.wid;
    sy = 2.0/(GLfloat)image.ht;

/*    glBlendEquationEXT(GL_LOGIC_OP); 

    glEnable(GL_BLEND);
*/
    if(cbs->reason == XmCR_INPUT) {
	if(event->xany.type == ButtonPress) {
	    if(push_state) { /* lower left corner of dest */
		x = event->xbutton.x - (GLint)image.wid / 2;
		y =(GLint)image.ht - 1 - event->xbutton.y - (GLint)image.ht / 2;
		image.rasterposx =  2 * x / (GLfloat)image.wid;
		image.rasterposy =  2 * y / (GLfloat)image.ht;
		glBegin(GL_LINES);
		glVertex2f(x * sx - .1, y * sy);
		glVertex2f(x * sx + .1, y * sy);
		glVertex2f(x * sx, y * sy - .1);
		glVertex2f(x * sx, y * sy + .1);
		glEnd();
		check_error("glBegin/glEnd");
	    } else { /* lower left corner of source */
		image.x = event->xbutton.x;
		image.y = (image.height - 1) - event->xbutton.y;
	    }
	} else if (event->xany.type == ButtonRelease) {
	    if(push_state) { /* ignore */
		push_state = ~push_state;
	    } else { /* upper right corner of source */
		image.width = event->xbutton.x - image.x;
		image.height = image.ht - 1 - event->xbutton.y - image.y;
		push_state = ~push_state;
	    }
	}
    }
/*
    glDisable(GL_BLEND);
*/    
}


Display     *dpy;
XtAppContext app;
GLXContext   ctx;

main(int argc, char *argv[])
{
    XVisualInfo *vis;
    XmString rgba, rgb, l, la, intens;
    XmString fubyt, fbyt, fusht, fsht, fuinteg, finteg, fflt;
    XmString iubyt, ibyt, iusht, isht, iuinteg, iinteg, iflt;
    XmString fformat, ftype, fcont; 
    XmString iformat, itype, icont;
    XmString box, laplace, sobel;
    XmString squares, checks, files;
    static int vattribs[] = { GLX_RGBA, 0};
    int screen;

    XtSetLanguageProc(NULL, NULL, NULL);

    toplevel = XtAppInitialize(&app, "SampleCode", 
			       NULL, 0, 
			       &argc, argv,
			       fallbackResources, NULL, 
			       0);

    dpy = XtDisplay(toplevel);
    screen = DefaultScreen(dpy);

   if (!(vis = glXChooseVisual(dpy, screen, vattribs)))  {
       XtAppError(app, "desired visual can't be found");
   }

   /* get context */
   if (!(ctx = glXCreateContext(dpy, vis, 0, GL_TRUE))) {
       XtAppError(app, "couldn't create context");
   }

    XtAddEventHandler(toplevel, StructureNotifyMask, False,
		      map_state_changed, NULL);


    form = XtVaCreateManagedWidget("form", 
				   xmFormWidgetClass, toplevel,
				   NULL);

    frame = XtVaCreateManagedWidget("frame",
				    xmFrameWidgetClass, form,
				    XmNtopAttachment, XmATTACH_FORM,
				    XmNleftAttachment, XmATTACH_FORM,
				    XmNrightAttachment, XmATTACH_FORM,
				    NULL);

    glxarea = XtVaCreateManagedWidget("glxarea", 
				      glwMDrawingAreaWidgetClass, frame, 
				      GLwNvisualInfo, vis, 
				      NULL);
    XtAddCallback(glxarea, XmNexposeCallback, clear_screen, NULL);
    XtAddCallback(glxarea, XmNresizeCallback, resize, NULL);
    XtAddCallback(glxarea, XmNinputCallback, mouse_input, NULL);

    control = XtVaCreateManagedWidget("ControlPanel",
				      xmFormWidgetClass, form,
				      XmNtopAttachment, XmATTACH_WIDGET,
				      XmNtopWidget, frame,
				      XmNleftAttachment, XmATTACH_FORM,
				      XmNrightAttachment, XmATTACH_FORM,
				      XmNbottomAttachment, XmATTACH_FORM,
				      NULL);

    enables = XtVaCreateManagedWidget("Enables",
				      xmRowColumnWidgetClass, control,
				      XmNtopAttachment, XmATTACH_FORM,
				      XmNleftAttachment, XmATTACH_FORM,
				      NULL);

    
    (void)XtVaCreateManagedWidget("title",
				  xmLabelWidgetClass, enables,
				  XtVaTypedArg, XmNlabelString, XmRString,
				  "Enables", sizeof("Enables"),
				  NULL);


    enabletex1d  = XtVaCreateManagedWidget("Tex1d",
				      xmToggleButtonWidgetClass, enables,
				      NULL);
    XtAddCallback(enabletex1d, XmNvalueChangedCallback, enable_tex_1d, NULL);

    enabletex2d  = XtVaCreateManagedWidget("Tex2d",
				      xmToggleButtonWidgetClass, enables,
				      NULL);
    XtAddCallback(enabletex2d, XmNvalueChangedCallback, enable_tex_2d, NULL);


    enable1d  = XtVaCreateManagedWidget("Conv1d",
				      xmToggleButtonWidgetClass, enables,
				      NULL);
    XtAddCallback(enable1d, XmNvalueChangedCallback, enable_1d, NULL);

    enable2d  = XtVaCreateManagedWidget("Conv2d",
				      xmToggleButtonWidgetClass, enables,
				      NULL);
    XtAddCallback(enable2d, XmNvalueChangedCallback, enable_2d, NULL);

    enableSep2d  = XtVaCreateManagedWidget("Sep2d",
				      xmToggleButtonWidgetClass, enables,
				      NULL);
    XtAddCallback(enableSep2d, XmNvalueChangedCallback, enable_sep_2d, NULL);


    actions = XtVaCreateManagedWidget("Actions",
				      xmRowColumnWidgetClass, control,
				      XmNtopAttachment, XmATTACH_FORM,
				      XmNrightAttachment, XmATTACH_FORM,
				      XmNleftWidget, enables,
				      NULL);

    draw = XtVaCreateManagedWidget("DrawPixels",
				      xmPushButtonWidgetClass, actions,
				      NULL);
    XtAddCallback(draw, XmNactivateCallback, draw_pix, NULL);

    readit = XtVaCreateManagedWidget("ReadPixels",
				    xmPushButtonWidgetClass, actions,
				    NULL);
    XtAddCallback(readit, XmNactivateCallback, read_pix, NULL);

    copy = XtVaCreateManagedWidget("CopyPixels",
				    xmPushButtonWidgetClass, actions,
				    NULL);
    XtAddCallback(copy, XmNactivateCallback, copy_pix, NULL);

    texmap = XtVaCreateManagedWidget("TexureMap",
				      xmPushButtonWidgetClass, actions,
				      NULL);
    XtAddCallback(texmap, XmNactivateCallback, draw_tex, NULL);


    load = XtVaCreateManagedWidget("New Image",
				    xmPushButtonWidgetClass, actions,
				    NULL);
    XtAddCallback(load, XmNactivateCallback, load_image, NULL);


    makeconv1d = XtVaCreateManagedWidget("Conv 1d",
				      xmPushButtonWidgetClass, actions,
				      NULL);
    XtAddCallback(makeconv1d, XmNactivateCallback, make_conv_1d, NULL);

    makeconv2d = XtVaCreateManagedWidget("Conv 2d",
				      xmPushButtonWidgetClass, actions,
				      NULL);
    XtAddCallback(makeconv2d, XmNactivateCallback, make_conv_2d, NULL);

    makesep2d  = XtVaCreateManagedWidget("Sep 2d",
				      xmPushButtonWidgetClass, actions,
				      NULL);
    XtAddCallback(makesep2d, XmNactivateCallback, make_sep_2d, NULL);


    getconv1dfilter  = XtVaCreateManagedWidget("Get Conv 1d",
					     xmPushButtonWidgetClass, actions,
					     NULL);
    XtAddCallback(getconv1dfilter, XmNactivateCallback, 
		  get_conv_1d_filter, NULL);

    getconv2dfilter  = XtVaCreateManagedWidget("Get Conv 2d",
					     xmPushButtonWidgetClass, actions,
					     NULL);
    XtAddCallback(getconv2dfilter, XmNactivateCallback, 
		  get_conv_2d_filter, NULL);


    getsepfilter  = XtVaCreateManagedWidget("Get Sep 2d",
					     xmPushButtonWidgetClass, actions,
					     NULL);
    XtAddCallback(getsepfilter, XmNactivateCallback, get_sep_filter, NULL);

    copyfilter1d  = XtVaCreateManagedWidget("Copy 1d",
					     xmPushButtonWidgetClass, actions,
					     NULL);
    XtAddCallback(copyfilter1d, XmNactivateCallback, copy_filter_1d, NULL);

    copyfilter2d  = XtVaCreateManagedWidget("Copy 2d",
					     xmPushButtonWidgetClass, actions,
					     NULL);
    XtAddCallback(copyfilter2d, XmNactivateCallback, copy_filter_2d, NULL);

    newlist  = XtVaCreateManagedWidget("glNewList",
					     xmPushButtonWidgetClass, actions,
					     NULL);
    XtAddCallback(newlist, XmNactivateCallback, new_list, NULL);

    endlist  = XtVaCreateManagedWidget("glEndList",
					     xmPushButtonWidgetClass, actions,
					     NULL);
    XtAddCallback(endlist, XmNactivateCallback, end_list, NULL);

    calllist  = XtVaCreateManagedWidget("glCallList",
					     xmPushButtonWidgetClass, actions,
					     NULL);
    XtAddCallback(calllist, XmNactivateCallback, call_list, NULL);



    filters = XtVaCreateManagedWidget("Filters",
				      xmRowColumnWidgetClass, control,
				      XmNtopAttachment, XmATTACH_WIDGET,
				      XmNleftAttachment, XmATTACH_FORM,
				      XmNtopWidget, enables,
				      NULL);

    (void)XtVaCreateManagedWidget("title",
				  xmLabelWidgetClass, filters,
				  XtVaTypedArg, XmNlabelString, XmRString,
				  "Filter", sizeof("Filter"),
				  NULL);



    fwidth = XtVaCreateManagedWidget("Width",
				     xmScaleWidgetClass, filters,
				     XtVaTypedArg, XmNtitleString, XmRString,
				     "Width", sizeof("Width"),
				     XmNmaximum, 11,
				     XmNminimum, 1,
				     XmNvalue, 3,
				     XmNshowValue, True,
				     XmNorientation, XmHORIZONTAL,
				     NULL);

    fheight = XtVaCreateManagedWidget("Height",
				     xmScaleWidgetClass, filters,
				     XtVaTypedArg, XmNtitleString, XmRString,
				     "Height", sizeof("Height"),
				     XmNmaximum, 11,
				     XmNminimum, 1,
				     XmNvalue, 3,
				     XmNshowValue, True,
				     XmNorientation, XmHORIZONTAL,
				     NULL);


    pzoom = XtVaCreateManagedWidget("Zoom",
				      xmRowColumnWidgetClass, control,
				      XmNtopAttachment, XmATTACH_WIDGET,
				      XmNrightAttachment, XmATTACH_FORM,
				      XmNtopWidget, actions,
				      NULL);


    (void)XtVaCreateManagedWidget("title",
				  xmLabelWidgetClass, pzoom,
				  XtVaTypedArg, XmNlabelString, XmRString,
				  "Zoom", sizeof("Zoom"),
				  NULL);


    xzoom = XtVaCreateManagedWidget("XZoom",
				    xmScaleWidgetClass, pzoom,
				    XtVaTypedArg, XmNtitleString, XmRString,
				    "ZoomX", sizeof("ZoomX"),
				    XmNdecimalPoints, 2,
				    XmNmaximum, 400,
				    XmNminimum, -200,
				    XmNvalue, 100,
				    XmNshowValue, True,
				    XmNorientation, XmHORIZONTAL,
				    NULL);

    yzoom = XtVaCreateManagedWidget("YZoom",
				    xmScaleWidgetClass, pzoom,
				    XtVaTypedArg, XmNtitleString, XmRString,
				    "ZoomY", sizeof("ZoomY"),
				    XmNdecimalPoints, 2,
				    XmNmaximum, 400,
				    XmNminimum, -200,
				    XmNvalue, 100,
				    XmNshowValue, True,
				    XmNorientation, XmHORIZONTAL,
				    NULL);


    fformat = XmStringCreateLocalized("FFormat");
    rgba = XmStringCreateLocalized("RGBA");
    rgb = XmStringCreateLocalized("RGB");
    l = XmStringCreateLocalized("L");
    la = XmStringCreateLocalized("LA");
    intens = XmStringCreateLocalized("I");

    filter_format = 
	XmVaCreateSimpleOptionMenu(filters, "FilterFormat",
				   fformat, 'F', 1, set_filter_format, 
				   XmVaPUSHBUTTON, rgba, 'R', NULL, NULL,
				   XmVaPUSHBUTTON, rgb, 'B', NULL, NULL,
				   XmVaPUSHBUTTON, l, 'L', NULL, NULL,
				   XmVaPUSHBUTTON, la, 'A', NULL, NULL,
				   XmVaPUSHBUTTON, intens, 'I', NULL, NULL,
				   NULL);

    XmStringFree(fformat);
    XmStringFree(rgba);
    XmStringFree(rgb);
    XmStringFree(l);
    XmStringFree(la);
    XmStringFree(intens);

    XtManageChild(filter_format);


    ftype = XmStringCreateLocalized("FType");
    fubyt = XmStringCreateLocalized("UBYTE");
    fbyt = XmStringCreateLocalized("BYTE");
    fusht = XmStringCreateLocalized("USHORT");
    fsht = XmStringCreateLocalized("SHORT");
    fuinteg = XmStringCreateLocalized("UINT");
    finteg = XmStringCreateLocalized("INT");
    fflt = XmStringCreateLocalized("FLOAT");

    filter_type = 
	XmVaCreateSimpleOptionMenu(filters, "FilterType",
				   ftype, 'T', 6, set_filter_type, 
				   XmVaPUSHBUTTON, fbyt, 'b', NULL, NULL,
				   XmVaPUSHBUTTON, fubyt, 'B', NULL, NULL,
				   XmVaPUSHBUTTON, fsht, 's', NULL, NULL,
				   XmVaPUSHBUTTON, fusht, 'S', NULL, NULL,
				   XmVaPUSHBUTTON, finteg, 'i', NULL, NULL,
				   XmVaPUSHBUTTON, fuinteg, 'I', NULL, NULL,
				   XmVaPUSHBUTTON, fflt, 'F', NULL, NULL,
				   NULL);

    XmStringFree(ftype);
    XmStringFree(fubyt);
    XmStringFree(fbyt);
    XmStringFree(fusht);
    XmStringFree(fsht);
    XmStringFree(fuinteg);
    XmStringFree(finteg);
    XmStringFree(fflt);

    XtManageChild(filter_type);



    fcont = XmStringCreateLocalized("FContents");
    box = XmStringCreateLocalized("Box");
    laplace = XmStringCreateLocalized("LaPlace");
    sobel = XmStringCreateLocalized("Sobel");

    filter_contents = 
	XmVaCreateSimpleOptionMenu(filters, "FilterContents",
				   fcont, 'F', 0, set_filter_contents, 
				   XmVaPUSHBUTTON, box, 'B', NULL, NULL,
				   XmVaPUSHBUTTON, laplace, 'L', NULL, NULL,
				   XmVaPUSHBUTTON, sobel, 'S', NULL, NULL,
				   NULL);

    XmStringFree(fcont);
    XmStringFree(laplace);
    XmStringFree(sobel);
    XmStringFree(box);

    XtManageChild(filter_contents);

    icont = XmStringCreateLocalized("IContents");
    squares = XmStringCreateLocalized("Squares");
    checks = XmStringCreateLocalized("CheckerBoard");
    files = XmStringCreateLocalized("File...");

    image_contents = 
	XmVaCreateSimpleOptionMenu(filters, "ImageContents",
				   icont, 'I', 0, set_image_contents, 
				   XmVaPUSHBUTTON, squares, 'S', NULL, NULL,
				   XmVaPUSHBUTTON, checks, 'C', NULL, NULL,
				   XmVaPUSHBUTTON, files, 'F', NULL, NULL,
				   NULL);

    XmStringFree(icont);
    XmStringFree(checks);
    XmStringFree(squares);
    XmStringFree(files);

    XtManageChild(image_contents);

    itype = XmStringCreateLocalized("IType");
    iubyt = XmStringCreateLocalized("UBYTE");
    ibyt = XmStringCreateLocalized("BYTE");
    iusht = XmStringCreateLocalized("USHORT");
    isht = XmStringCreateLocalized("SHORT");
    iuinteg = XmStringCreateLocalized("UINT");
    iinteg = XmStringCreateLocalized("INT");
    iflt = XmStringCreateLocalized("FLOAT");

    image_type = 
	XmVaCreateSimpleOptionMenu(filters, "ImageType",
				   itype, 'I', 6, set_image_type, 
				   XmVaPUSHBUTTON, ibyt, 'b', NULL, NULL,
				   XmVaPUSHBUTTON, iubyt, 'B', NULL, NULL,
				   XmVaPUSHBUTTON, isht, 's', NULL, NULL,
				   XmVaPUSHBUTTON, iusht, 'S', NULL, NULL,
				   XmVaPUSHBUTTON, iinteg, 'i', NULL, NULL,
				   XmVaPUSHBUTTON, iuinteg, 'I', NULL, NULL,
				   XmVaPUSHBUTTON, iflt, 'F', NULL, NULL,
				   NULL);

    XmStringFree(itype);
    XmStringFree(iubyt);
    XmStringFree(ibyt);
    XmStringFree(iusht);
    XmStringFree(isht);
    XmStringFree(iuinteg);
    XmStringFree(iinteg);
    XmStringFree(iflt);

    XtManageChild(image_type);

    
    iformat = XmStringCreateLocalized("IFormat");
    rgba = XmStringCreateLocalized("RGBA");
    rgb = XmStringCreateLocalized("RGB");
    l = XmStringCreateLocalized("L");
    la = XmStringCreateLocalized("LA");
    intens = XmStringCreateLocalized("I");

    image_format = 
	XmVaCreateSimpleOptionMenu(filters, "ImageFormat",
				   iformat, 'I', 1, set_image_format, 
				   XmVaPUSHBUTTON, rgba, 'R', NULL, NULL,
				   XmVaPUSHBUTTON, rgb, 'B', NULL, NULL,
				   XmVaPUSHBUTTON, l, 'L', NULL, NULL,
				   XmVaPUSHBUTTON, la, 'A', NULL, NULL,
				   XmVaPUSHBUTTON, intens, 'I', NULL, NULL,
				   NULL);

    XmStringFree(iformat);
    XmStringFree(rgba);
    XmStringFree(rgb);
    XmStringFree(l);
    XmStringFree(la);
    XmStringFree(intens);

    XtManageChild(image_format);



    XtRealizeWidget(toplevel);

    glXMakeCurrent(dpy, XtWindow(glxarea), ctx);
    made_current = GL_TRUE;
    
    glDisable(GL_DITHER);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

   /* Default Enables */

    check_error("Clear Errors");

    image.wid = 256;
    image.ht = 256;
    image.x = 0;
    image.y = 0;
    image.width = 128;
    image.height = 128;
    image.rasterposx = 0.0;
    image.rasterposy = 0.0;
    image.contents = SQUARES;
    image.type = GL_FLOAT;
    image.format = GL_RGB;
    make_image(&image);

    filter.width = 3;
    filter.height = 3;
    filter.format = GL_RGB;
    filter.type = GL_FLOAT;
    filter.internalformat = GL_RGB;
    filter.get_target = GL_CONVOLUTION_2D_EXT;
    filter.get_format = GL_RGBA;
    filter.get_type = GL_FLOAT;
    filter.contents = BOX;
    make_filter(&filter);

    XtAppMainLoop(app);
}
