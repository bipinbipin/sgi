#include "texture.h"

extern "C" {
#include "myimage.h"
};

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <malloc.h>

#define Malloc(ptr, type, count) \
{ \
  ptr = (type *)malloc(count * sizeof(type)); \
  if (ptr == NULL && count != 0 && sizeof(type) != 0) { \
    fprintf(stderr, "malloc failed.\n"); \
    fprintf(stderr, "%d bytes requested.\n", count * sizeof(type)); \
    exit(1); \
  } \
}

inline int sizeof_format(GLenum format) 
{
  switch(format) {
  case GL_COLOR_INDEX:
  case GL_RED:
  case GL_GREEN:
  case GL_BLUE:
  case GL_ALPHA:
  case GL_LUMINANCE:
    return 1;
  case GL_LUMINANCE_ALPHA:
    return 2;
  case GL_RGB:
    return 3;
  case GL_RGBA:
    return 4;
  default:
    fprintf(stderr, "Unrecognized pixel format %d.\n", format);
    return 0;
  }
}

inline int sizeof_type(GLenum type)
{
  switch(type) {
  case GL_UNSIGNED_BYTE:
    return sizeof(GLubyte);
  case GL_BYTE:
    return sizeof(GLbyte);
  case GL_UNSIGNED_SHORT:
    return sizeof(GLushort);
  case GL_SHORT:
    return sizeof(GLshort);
  case GL_UNSIGNED_INT:
    return sizeof(GLuint);
  case GL_INT:
    return sizeof(GLint);
  default:
    fprintf(stderr, "Unrecognized pixel type %d.\n", type);
    return 0;
  }
}

inline float max_value(GLenum type)
{
  switch(type) {
  case GL_UNSIGNED_BYTE:
    return 255.0;
  case GL_BYTE:
    return 127.0;
  case GL_UNSIGNED_SHORT:
    return 65535.0;
  case GL_SHORT:
    return 32767.0;
  case GL_UNSIGNED_INT:
    return 4294967295.0;
  case GL_INT:
    return 2147483647.0;
  default:
    fprintf(stderr, "Unrecognized pixel type %d.\n", type);
    return 0;
  }
}

inline float max_value_inv(GLenum type)
{
  switch(type) {
  case GL_UNSIGNED_BYTE:
    return 3.9215686e-3;
  case GL_BYTE:
    return 7.8740157e-3;
  case GL_UNSIGNED_SHORT:
    return 1.5259022e-5;
  case GL_SHORT:
    return 3.0518509e-5;
  case GL_UNSIGNED_INT:
    return 2.3283064e-10;
  case GL_INT:
    return 4.46566129e-10;
  default:
    fprintf(stderr, "Unrecognized pixel type %d.\n", type);
    return 0;
  }
}

inline float luminance(float r, float g, float b) 
{
  return(.299*r + .587*g + .114*b);
}

texture::texture() 
{
}

void texture::open()
{
  pixels = NULL;
  pixels_size = 0;
  display_format = DEFAULT_DISPLAY_FORMAT;
  display_type = DEFAULT_DISPLAY_TYPE;
  alignment = DEFAULT_ALIGNMENT;
  components = DEFAULT_COMPONENTS;
  level = DEFAULT_LEVEL;
  
  min_filter = DEFAULT_MIN_FILTER;
  max_filter = DEFAULT_MAX_FILTER;

  environment = DEFAULT_ENVIRONMENT;

}

void texture::set_display_format(GLenum new_display_format)
{
  reformat(new_display_format, display_type, alignment);
}

GLenum texture::get_display_format()
{
  return display_format;
}

void texture::set_display_type(GLenum new_display_type)
{
  reformat(display_format, new_display_type, alignment);
}

GLenum texture::get_display_type()
{
  return display_type;
}

void texture::set_alignment(int new_alignment)
{
  reformat(display_format, display_type, new_alignment);
}

int texture::get_alignment()
{
  return alignment;
}

void texture::set_components(int new_components)
{
  components = new_components;
}

int texture::get_components()
{
  return components;
}

void texture::set_level(int new_level)
{
  level = new_level;
}

int texture::get_level()
{
  return level;
}

void texture::set_min_filter(GLenum new_min_filter) 
{
  min_filter = new_min_filter;
}

GLenum texture::get_min_filter() 
{
  return min_filter;
}

void texture::set_max_filter(GLenum new_max_filter)
{
  max_filter = new_max_filter;
}

GLenum texture::get_max_filter()
{
  return max_filter;
}

int texture::get_width()
{
  return width;
}

int texture::get_height()
{
  return height;
}

void texture::set_environment(GLenum new_environment)
{
  environment = new_environment;
}

GLenum texture::get_environment()
{
  return environment;
}

void texture::create_color(int total_size,
			   float r, float g, float b, float a, float l)
{
  int x, y;
  width = height = total_size;
  pixels_alloc(width * height);
  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      assign_pixel(x, y, r, g, b, a, l);
}

void texture::create_checkerboard(int total_size, int block_size,
				  float r1, float g1, float b1,
				  float alpha1, float lum1,
				  float r2, float g2, float b2,
				  float alpha2, float lum2) 
{
  int x, y;
  width = height = total_size;
  pixels_alloc(width * height);
  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      if ((((x /block_size) % 2) + ((y / block_size) % 2)) % 2)
	assign_pixel(x, y, r1, g1, b1, alpha1, lum1);
      else assign_pixel(x, y, r2, g2, b2, alpha2, lum2);
}

void texture::create_diag_stripes(int total_size, int stripe_width,
				  float r1, float g1, float b1,
				  float alpha1, float lum1,
				  float r2, float g2, float b2,
				  float alpha2, float lum2) 

{
  int x, y;
  width = height = total_size;
  pixels_alloc(width * height);
  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      if (((x + y) / stripe_width) % 2) 
	assign_pixel(x, y, r1, g1, b1, alpha1, lum1);
      else assign_pixel(x, y, r2, g2, b2, alpha2, lum2);
}

void texture::create_from_file(char *fname) 
{
  IMAGE *image;
  unsigned short *rbuf, *gbuf, *bbuf;
  float r, g, b;
  int x, y;

  image = iopen(fname, "r");
  if (image == NULL) {
    fprintf(stderr, "Unable to open file %s.\n", fname);
    exit(1);
  }

  width = image->xsize;
  height = image->ysize;

  Malloc(rbuf, unsigned short, image->xsize);
  Malloc(gbuf, unsigned short, image->xsize);
  Malloc(bbuf, unsigned short, image->xsize);

  pixels_alloc(width * height);

  for (y = 0; y < image->ysize; y++) {
    getrow(image, rbuf, y, 0);
    getrow(image, gbuf, y, 1);
    getrow(image, bbuf, y, 2);
    for (x = 0; x < image->xsize; x++) {
      r = (float)rbuf[x] / 255.0;
      g = (float)gbuf[x] / 255.0;
      b = (float)bbuf[x] / 255.0;
      assign_pixel(x, y, r, g, b, 1.0, luminance(r, g, b));
    }
  }

  if (image->xsize) {
    free(rbuf);
    free(gbuf);
    free(bbuf);
  }

  /* There must be some way to close an image file, but I don't know
   * know what it is, so we'll just leave it open for now. */

}

void texture::map_lum_to_alpha() 
{
  int x, y;
  float lum;

  if (pixels_size == 0) {
    fprintf(stderr, "Cannot map luminance to alpha:  no image in memory.\n");
    return;
  }
  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++) 
      assign_pixel_a(x, y, pixel_lum(x, y));
}

void texture::draw_pixels()
{
  draw_pixels(0, 0, width, height);
}

void texture::draw_pixels(int x1, int y1, int x2, int y2)
{
  glPixelStorei(GL_PACK_ROW_LENGTH, width);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, x1);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, y1); 
  glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
  glDrawPixels(x2-x1, y2-y1, display_format, display_type, pixels);
}

void texture::setup_texture() 
{
  setup_texture(0, 0, width, height);
}

void texture::setup_texture(int x1, int y1, int x2, int y2)
{
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, max_filter);

  if ((components < 3 && environment == GL_DECAL) || 
      (components > 2 && environment == GL_BLEND)) 
    fprintf(stderr, 
	    "Warning:  illegal component / environment combination.\n");
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, environment);

  glPixelStorei(GL_PACK_ROW_LENGTH, width);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, x1);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, y1);
  glPixelStorei(GL_UNPACK_ALIGNMENT, alignment); 
}

void texture::specify_texture()
{
  specify_texture(0, 0, width, height);
}


void texture::specify_texture(int x1, int y1, int x2, int y2)
{
  setup_texture(x1, y1, x2, y2);

  /* Create the mip map if required, otherwise don't */
  if (min_filter == GL_NEAREST || min_filter == GL_LINEAR) 
    glTexImage2D(GL_TEXTURE_2D, level, components,
		 x2-x1, y2-y1, 0, display_format, display_type,
		 pixels);
  else gluBuild2DMipmaps(GL_TEXTURE_2D, components, x2-x1,
			 y2-y1, display_format, display_type,
			 pixels);
}

void texture::specify_mipmap() 
{
  specify_mipmap(0, 0, width, height);
}

void texture::specify_mipmap(int x1, int y1, int x2, int y2)
{
  glTexImage2D(GL_TEXTURE_2D, level, components, 
	       x2-x1, y2-y1, 0, display_format, display_type,
	       pixels);
}

void texture::pixels_alloc() 
{
  pixels_alloc(height * width);
}

void texture::pixels_alloc(int size)
{
  int size_bytes;

  size_bytes = 
    size * sizeof_format(display_format) * sizeof_type(display_type);
  if (pixels_size == 0) {
    pixels = malloc(size_bytes);
    pixels_size = size_bytes;
  }
  else if (size_bytes > pixels_size) {
    pixels = realloc(pixels, size_bytes);
    pixels_size = size_bytes;
  }
  if (pixels == NULL && size_bytes != 0) {
    fprintf(stderr, "Malloc failed - unable to allocate %d bytes.\n",
	    size_bytes);
    exit(1);
  }
  
}

void texture::pixels_free()
{
  if (pixels != NULL) free(pixels);
  pixels = NULL;
  pixels_size = 0;
}

#define ASSIGN_PIXEL2(element, value, el_length, enum, type) \
 case enum: \
  ((type *)((char *)pixels + y*rowlength))[x*el_length + element] = \
  (type)(value * max_value(enum)); \
break; 

#define ASSIGN_PIXEL(element, value, el_length) \
switch(display_type) { \
  ASSIGN_PIXEL2(element, value, el_length, GL_UNSIGNED_BYTE, GLubyte); \
  ASSIGN_PIXEL2(element, value, el_length, GL_BYTE, GLbyte); \
  ASSIGN_PIXEL2(element, value, el_length, GL_UNSIGNED_SHORT, GLushort); \
  ASSIGN_PIXEL2(element, value, el_length, GL_SHORT, GLshort); \
  ASSIGN_PIXEL2(element, value, el_length, GL_UNSIGNED_INT, GLuint); \
  ASSIGN_PIXEL2(element, value, el_length, GL_INT, GLint); \
}

void texture::assign_pixel(int x, int y, float r, float g, float b, float a,
			   float l)
{
  assign_pixel(x, y, r, g, b, a, l, display_format, display_type, alignment);
}

void texture::assign_pixel(int x, int y, float r, float g, float b, float a,
			   float l, GLenum display_format, 
			   GLenum display_type, int alignment)
{
  int rowlength;

  rowlength = row_length();
  switch (display_format) {
  case GL_RED:
    ASSIGN_PIXEL(0, r, 1);
    break;
  case GL_GREEN:
    ASSIGN_PIXEL(0, g, 1);
    break;
  case GL_BLUE:
    ASSIGN_PIXEL(0, b, 1);
    break;
  case GL_ALPHA:
    ASSIGN_PIXEL(0, a, 1);
    break;
  case GL_LUMINANCE:
    ASSIGN_PIXEL(0, l, 1);
    break;
  case GL_LUMINANCE_ALPHA:
    ASSIGN_PIXEL(0, l, 2);
    ASSIGN_PIXEL(1, a, 2);
    break;
  case GL_RGB:
    ASSIGN_PIXEL(0, r, 3);
    ASSIGN_PIXEL(1, g, 3);
    ASSIGN_PIXEL(2, b, 3);
    break;
  case GL_RGBA:
    ASSIGN_PIXEL(0, r, 4);
    ASSIGN_PIXEL(1, g, 4);
    ASSIGN_PIXEL(2, b, 4);
    ASSIGN_PIXEL(3, a, 4);
    break;
  default:
    fprintf(stderr, "Unrecognized display format %d\n", display_format);
    exit(1);
  }
}

inline void texture::assign_pixel_a(int x, int y, float a)
{
  int rowlength = row_length();
  if (display_format == GL_LUMINANCE_ALPHA) {
    ASSIGN_PIXEL(1, a, 2); 
  } else if (display_format == GL_RGBA) ASSIGN_PIXEL(3, a, 4);
  return;
}

void texture::reformat(GLenum new_display_format, GLenum new_display_type,
		       int new_alignment)
{
  GLenum old_display_format = display_format;
  GLenum old_display_type = display_type;
  int old_alignment = alignment;
  int old_pixels_size = pixels_size;
  int direction;
  int old_rowlength, new_rowlength;
  int x, y;
  
  /* Make sure that something acutally has changed. */
  if (new_display_format == old_display_format &&
      new_display_type == old_display_type && 
      new_alignment == old_alignment) return;

  if (old_pixels_size == 0) {
    display_format = new_display_format;
    display_type = new_display_type;
    alignment = new_alignment;
    return;
  }

  old_rowlength = row_length(old_display_format, old_display_type,
			     old_alignment);
  new_rowlength = row_length(new_display_format, new_display_type,
			     new_alignment);
  
  /* Seems like this should cause a core dump since the amount of 
   * space has not changed yet... */
  pixels_alloc();

  if (new_rowlength > old_rowlength) direction = -1;
  else direction = 1;

  for (y = (direction == 1 ? 0 : height - 1);
       y < height && y >= 0; y += direction) 
    for (x = (direction == 1 ? 0 : width - 1);
         x < height && x >= 0; x += direction) 
      {
	assign_pixel(x, y, pixel_r(x, y), pixel_g(x, y), pixel_b(x, y),
		     pixel_alpha(x, y), pixel_lum(x, y), new_display_format,
		     new_display_type, new_alignment);
      }

  display_format = new_display_format;
  display_type = new_display_type;
  alignment = new_alignment;

}

int texture::row_length()
{
  return row_length(display_format, display_type, alignment);
}

int texture::row_length(GLenum display_format, GLenum display_type,
			int alignment)
{
  int row_length;
  row_length = width * sizeof_format(display_format) * 
    sizeof_type(display_type);
  row_length += row_length % alignment;
  return row_length;
}

#define PIXEL_VALUE2(enum, type, index) \
((float) \
((type *)((char *)pixels + y*rowlength)) \
[x*sizeof_format(display_format) + index] * max_value_inv(enum))

#define PIXEL_VALUE(enum, type, index) \
 case enum: \
  return PIXEL_VALUE2(enum, type, index);

#define PIXEL_AVG_VALUE(enum, type) \
 case enum: \
  return (luminance(PIXEL_VALUE2(enum, type, 0), \
		    PIXEL_VALUE2(enum, type, 1), \
		    PIXEL_VALUE2(enum, type, 2)));

inline float texture::pixel_r(int x, int y)
{
  return pixel_r(x, y, display_format, display_type, alignment);
}

inline float texture::pixel_r(int x, int y, GLenum display_format, 
  GLenum display_type, int alignment)
{
  int rowlength;
  
  rowlength = row_length(display_format, display_type, alignment);

  switch(display_format) {
  case GL_RED:
  case GL_RGB:
  case GL_RGBA:
  case GL_LUMINANCE:
  case GL_LUMINANCE_ALPHA:
    switch(display_type) {
      PIXEL_VALUE(GL_UNSIGNED_BYTE, GLubyte, 0);
      PIXEL_VALUE(GL_BYTE, GLbyte, 0);
      PIXEL_VALUE(GL_UNSIGNED_SHORT, GLushort, 0);
      PIXEL_VALUE(GL_SHORT, GLshort, 0);
      PIXEL_VALUE(GL_UNSIGNED_INT, GLuint, 0);
      PIXEL_VALUE(GL_INT, GLint, 0);
    }
    break;
  case GL_GREEN:
  case GL_BLUE:
    return 0.0;
  }
}

inline float texture::pixel_g(int x, int y)
{
  return pixel_g(x, y, display_format, display_type, alignment);
}

inline float texture::pixel_g(int x, int y, GLenum display_format, 
			       GLenum display_type, int alignment)
{
  int rowlength;
  
  rowlength = row_length(display_format, display_type, alignment);

  switch(display_format) {
  case GL_GREEN:
  case GL_LUMINANCE:
  case GL_LUMINANCE_ALPHA:
    switch(display_type) {
      PIXEL_VALUE(GL_UNSIGNED_BYTE, GLubyte, 0);
      PIXEL_VALUE(GL_BYTE, GLbyte, 0);
      PIXEL_VALUE(GL_UNSIGNED_SHORT, GLushort, 0);
      PIXEL_VALUE(GL_SHORT, GLshort, 0);
      PIXEL_VALUE(GL_UNSIGNED_INT, GLuint, 0);
      PIXEL_VALUE(GL_INT, GLint, 0);
    }
    break;
  case GL_RGB:
  case GL_RGBA:
    switch(display_type) {
      PIXEL_VALUE(GL_UNSIGNED_BYTE, GLubyte, 1);
      PIXEL_VALUE(GL_BYTE, GLbyte, 1);
      PIXEL_VALUE(GL_UNSIGNED_SHORT, GLushort, 1);
      PIXEL_VALUE(GL_SHORT, GLshort, 1);
      PIXEL_VALUE(GL_UNSIGNED_INT, GLuint, 1);
      PIXEL_VALUE(GL_INT, GLint, 1);
    }
    break;
  case GL_RED:
  case GL_BLUE:
    return 0.0;
  }
}

inline float texture::pixel_b(int x, int y)
{
  return pixel_b(x, y, display_format, display_type, alignment);
}

inline float texture::pixel_b(int x, int y, GLenum display_format, 
			       GLenum display_type, int alignment)
{
  int rowlength;
  
  rowlength = row_length(display_format, display_type, alignment);

  switch(display_format) {
  case GL_BLUE:
  case GL_LUMINANCE:
  case GL_LUMINANCE_ALPHA:
    switch(display_type) {
      PIXEL_VALUE(GL_UNSIGNED_BYTE, GLubyte, 0);
      PIXEL_VALUE(GL_BYTE, GLbyte, 0);
      PIXEL_VALUE(GL_UNSIGNED_SHORT, GLushort, 0);
      PIXEL_VALUE(GL_SHORT, GLshort, 0);
      PIXEL_VALUE(GL_UNSIGNED_INT, GLuint, 0);
      PIXEL_VALUE(GL_INT, GLint, 0);
    }
    break;
  case GL_RGB:
  case GL_RGBA:
    switch(display_type) {
      PIXEL_VALUE(GL_UNSIGNED_BYTE, GLubyte, 2);
      PIXEL_VALUE(GL_BYTE, GLbyte, 2);
      PIXEL_VALUE(GL_UNSIGNED_SHORT, GLushort, 2);
      PIXEL_VALUE(GL_SHORT, GLshort, 2);
      PIXEL_VALUE(GL_UNSIGNED_INT, GLuint, 2);
      PIXEL_VALUE(GL_INT, GLint, 2);
    }
    break;
  case GL_RED:
  case GL_GREEN:
    return 0.0;
  }
}

inline float texture::pixel_alpha(int x, int y)
{
  return pixel_alpha(x, y, display_format, display_type, alignment);
}

inline float texture::pixel_alpha(int x, int y, GLenum display_format, 
				   GLenum display_type, int alignment)
{
  int rowlength;

  rowlength = row_length(display_format, display_type, alignment);
  switch(display_format) {
  case GL_RGBA:
    switch(display_type) {
      PIXEL_VALUE(GL_UNSIGNED_BYTE, GLubyte, 3);
      PIXEL_VALUE(GL_BYTE, GLbyte, 3);
      PIXEL_VALUE(GL_UNSIGNED_SHORT, GLushort, 3);
      PIXEL_VALUE(GL_SHORT, GLshort, 3);
      PIXEL_VALUE(GL_UNSIGNED_INT, GLuint, 3);
      PIXEL_VALUE(GL_INT, GLint, 3);
    }
  case GL_LUMINANCE_ALPHA:
    switch(display_type) {
      PIXEL_VALUE(GL_UNSIGNED_BYTE, GLubyte, 1);
      PIXEL_VALUE(GL_BYTE, GLbyte, 1);
      PIXEL_VALUE(GL_UNSIGNED_SHORT, GLushort, 1);
      PIXEL_VALUE(GL_SHORT, GLshort, 1);
      PIXEL_VALUE(GL_UNSIGNED_INT, GLuint, 1);
      PIXEL_VALUE(GL_INT, GLint, 1);
    }
  default:
    return 1.0;
  }
}

inline float texture::pixel_lum(int x, int y)
{
  return pixel_lum(x, y, display_format, display_type, alignment);
}

inline float texture::pixel_lum(int x, int y, GLenum display_format, 
				GLenum display_type, int alignment)
{
  int rowlength;

  rowlength = row_length(display_format, display_type, alignment);

  switch(display_format) {
  case GL_RED:
  case GL_GREEN:
  case GL_BLUE:
  case GL_LUMINANCE:
  case GL_LUMINANCE_ALPHA:
    switch(display_type) {
      PIXEL_VALUE(GL_UNSIGNED_BYTE, GLubyte, 0);
      PIXEL_VALUE(GL_BYTE, GLbyte, 0);
      PIXEL_VALUE(GL_UNSIGNED_SHORT, GLushort, 0);
      PIXEL_VALUE(GL_SHORT, GLshort, 0);
      PIXEL_VALUE(GL_UNSIGNED_INT, GLuint, 0);
      PIXEL_VALUE(GL_INT, GLint, 0);
    }
    break;
  case GL_RGB:
  case GL_RGBA:
    switch(display_type) {
      PIXEL_AVG_VALUE(GL_UNSIGNED_BYTE, GLubyte);
      PIXEL_AVG_VALUE(GL_BYTE, GLbyte);
      PIXEL_AVG_VALUE(GL_UNSIGNED_SHORT, GLushort);
      PIXEL_AVG_VALUE(GL_SHORT, GLshort);
      PIXEL_AVG_VALUE(GL_UNSIGNED_INT, GLuint);
      PIXEL_AVG_VALUE(GL_INT, GLint);
    }
  default:
    fprintf(stderr, "Unrecognized display format %d\n", display_format);
    return 0.0;
  }
}
