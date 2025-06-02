#include <GL/glu.h>
#include <GL/glx.h>

extern "C" {
#include "myimage.h"
};

#define DEFAULT_DISPLAY_TYPE		GL_UNSIGNED_BYTE
#define DEFAULT_DISPLAY_FORMAT		GL_RGBA
#define DEFAULT_ALIGNMENT		1
#define DEFAULT_COMPONENTS		4
#define DEFAULT_LEVEL			0

#define DEFAULT_ENVIRONMENT		GL_MODULATE

#define DEFAULT_MIN_FILTER		GL_LINEAR
#define DEFAULT_MAX_FILTER		GL_LINEAR

class texture {
 public:
  texture();
  ~texture();
  
  void open();
 
  void set_display_format(GLenum new_display_format);
  GLenum get_display_format();

  void set_display_type(GLenum new_display_type);
  GLenum get_display_type();

  void set_alignment(int new_alignment);
  int get_alignment();

  void set_components(int new_components);
  int get_components();

  void set_level(int new_level);
  int get_level();

  void set_environment(GLenum new_environment);
  GLenum get_environment();

  void set_min_filter(GLenum new_min_filter);
  GLenum get_min_filter();
  void set_max_filter(GLenum new_max_filter);
  GLenum get_max_filter();

  int get_width();
  int get_height();

  void create_color(int total_size,
		    float r, float g, float b, float a, float l);
  void create_checkerboard(int total_size, int block_size,
			   float r1 = 1.0, float g1 = 0.0, float b1 = 0.0,
			   float alpha1 = 1.0, float lum1 = 0.299,
			   float r2 = 0.0, float g2 = 0.0, float b2 = 1.0,
			   float alpha2 = 1.0, float lum2 = 0.114);
  void create_diag_stripes(int total_size, int stripe_width,
			   float r1 = 1.0, float g1 = 0.0, float b1 = 0.0,
			   float alpha1 = 1.0, float lum1 = 0.299,
			   float r2 = 0.0, float g2 = 0.0, float b2 = 1.0,
			   float alpha2 = 1.0, float lum2 = 0.114);
  void create_from_file(char *fname);

  void map_lum_to_alpha();

  void draw_pixels();
  void draw_pixels(int x1, int y1, int x2, int y2);

  /* setup_texture() sets up the alignment etc */
  void setup_texture();
  void setup_texture(int x1, int y1, int x2, int y2);

  /* specify_texture():
   * 1. Calls setup_texture()
   * 2. Calls glTexImage2D() with a level of level if min_filter is 
   * GL_NEAREST or GL_LINEAR, otherwise calls gluBuild2DMipmaps() */
  void specify_texture();
  void specify_texture(int x1, int y1, int x2, int y2);

  /* specify_mipmap()
   * Calls glTexImage2D() with a level of level */
  void specify_mipmap();
  void specify_mipmap(int x1, int y1, int x2, int y2);

  void pixels_free();

 private:
  void pixels_alloc();
  void pixels_alloc(int size);

  void assign_pixel(int x, int y, float r, float g, float b, float a,
		    float l);
  void assign_pixel(int x, int y, float r, float g, float b, float a,
		    float l, GLenum display_format,
		    GLenum display_type, int alignment);

  inline void assign_pixel_a(int x, int y, float a);

  void reformat(GLenum new_display_format, GLenum new_display_type,
		int new_alignment);

  int row_length();
  int row_length(GLenum display_format, GLenum display_type, 
		 int alignment);

  inline float pixel_r(int x, int y);
  inline float pixel_r(int x, int y, GLenum display_format,
		       GLenum display_type, int alignment);

  inline float pixel_g(int x, int y);
  inline float pixel_g(int x, int y, GLenum display_format,
		       GLenum display_type, int alignment);

  inline float pixel_b(int x, int y);
  inline float pixel_b(int x, int y, GLenum display_format,
		       GLenum display_type, int alignment);

  inline float pixel_alpha(int x, int y);
  inline float pixel_alpha(int x, int y, GLenum display_format,
			   GLenum display_type, int alignment);

  inline float pixel_lum(int x, int y);
  inline float pixel_lum(int x, int y, GLenum display_format,
			 GLenum display_type, int alignment);

  int height, width;
  GLvoid *pixels;
  int pixels_size;
  GLenum display_format;
  GLenum display_type;
  int alignment;
  int components;
  int level;

  GLenum environment;

  GLenum min_filter, max_filter;
};
