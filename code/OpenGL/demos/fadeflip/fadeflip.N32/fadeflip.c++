#include "oglwindow.h"
#include "unitsquare.h"
#include "texture.h"

#include <X11/keysym.h>

#include <sys/time.h>
#include <malloc.h>
#include <math.h>
#include <stdlib.h>

#define FADE_RED	0.0
#define FADE_GREEN	0.0
#define FADE_BLUE	0.0

static char def_image0[] = 
  DATADIR "opengl1.128.rgb";
static char def_image1[] = 
  DATADIR "opengl1.128.rgb";
static char def_image2[] = 
  DATADIR "re_coke.rgb";
static char *def_images[3];

static float fov = 40.0;
static float zdist = 5.0;		// Distance eye -> origin 

static int frame = 0;			// Have square fill entire fov?
static int fix_size = 0;		// Fix size of window to size
  					// of first image?

static int rainbow = 0;			// Use rainbow colors for mipmaps?

static int fade = 0;			// Fill smaller mipmaps with
 					// black?


static int use_texture = 1;		// Use textures at all?

static int auto_mode = 0;		// operate automatically?

static float rotx = 0.0, roty = 0.0;	// Rotations about x and y per second

const float mouse_poll = 100000;        // How often to poll mouse

static float fadecolors[][3] = {
  {1, 0, 0},
  {1, 1, 0},
  {0, 1, 0},
  {0, 1, 1},
  {0, 0, 1},
  {1, 0, 1}};
static int n_colors = 6;

static int paused = 0;

GLfloat matrix[16];

int n_textures;

oglwindow *window;
unitsquare *square;

int win_height, win_width;

int image = 0;

/* Note - odd number display lists are the images; the even number that
 * directly follows is the fade for that image */

inline float radians(float degrees)
{
  return ((degrees / 180.0) * M_PI);
}

inline unsigned long current_time()
{
  struct timeval time;
  gettimeofday(&time, NULL);
  return (time.tv_sec * 1000000 + time.tv_usec);
}

inline float transform_z(float *m) 
{
  return (m[10] + m[11]);
}

void do_rotation(unsigned long last_time, unsigned long time)
{
  float elapsed_time;
  float z;

  elapsed_time = (time - last_time) / 1000000.0;

  z = transform_z(matrix);

  glPushMatrix(); 
  glLoadIdentity(); 
  if (!paused) {
    glRotatef(elapsed_time * rotx, 1, 0, 0);
    glRotatef(elapsed_time * roty, 0, 1, 0);
  }
  glMultMatrixf(matrix);
  glGetFloatv(GL_MODELVIEW_MATRIX, matrix);
  glPopMatrix(); 
  glMultMatrixf(matrix);

  /* If the square has flipped around so that a different face is 
   * is visible, a different image should be used */
  if (n_textures && z * transform_z(matrix) <= 0.0) 
    image = (image + 1) % n_textures;
}

unsigned long draw(unsigned long last_draw)
{
  unsigned long time = current_time();

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (fov, 1.0, 0.01, 20.0); 

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt(0, 0, zdist, 0, 0, 0, 0, 1, 0);

  glClear(GL_COLOR_BUFFER_BIT);

  glColor4ub(255, 255, 255, 255);

  glCallList(image * 2 + 1);
  glCallList(image * 2 + 2);

  do_rotation(last_draw, time);

  square->draw();
  
  window->swapbuffers();

  return time;
}

int mouse_motion(int oldmx, int oldmy, unsigned long last_time,
                 int mx, int my, unsigned long time)
{
  float dx, dy, dt;

  dt = (time - last_time) / 1000000.0;

  // Find the mouse motion vector in screen space
  dx = (float)(mx - oldmx) / (float)win_width;
  dy = (float)(my - oldmy) / (float)win_height;

  rotx = dy * 180.0 / dt;
  roty = dx * 180.0 / dt;

  if (rotx != 0.0 || roty != 0.0) return 1;
  else return 0;
}

void show_usage() 
{
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "fadeflip [-f<field of view>] [-s<speed>] file1.rgb \
[file2.rgb...]\n");
}

void main(int argc, char **argv)
{
  Display *dpy;
  XEvent event;
  char buffer[5];
  int bufsize = 5;
  KeySym key;
  XComposeStatus compose;
  Window root_return, child_return;
  int root_x, root_y;
  unsigned int mask_return;
  Window winid;

  int fname;
  int width=0, height=0, x=0, y=0;
  texture *texmap;
  int tex_width;
  unsigned long time, last_mouse_time, last_draw = 0.0;
  int oldmx, oldmy, mx, my;
  int button_down = 0;
  int moving = 0;
  int i, j;
  
  square = new unitsquare();
  square->open();

  texmap = new texture();
  texmap->open();
  texmap->set_min_filter(GL_LINEAR_MIPMAP_LINEAR);
  texmap->set_environment(GL_DECAL);

  fname = argc;
  for (i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      fname = i;
      break;
    }
    switch(argv[i][1]) {
    case 'd':				// Fade
      fade = atoi(&argv[i][2]);
      break;
    case 'f':				// Frame
      frame = 1;
      break;
    case 'r':
      rainbow = 1;
      break;
    case 't':				// Use textures?
      use_texture = atoi(&argv[i][2]);
      break;
    case 'v':				// Field of view
      fov = atof(&argv[i][2]);
      break;
    case 'x':
      fix_size = 1;
      break;
    case 'a':				// Auto mode
      auto_mode = 1;
      break;
    case 'X':
      x = atoi(&argv[i][2]);
      break;
    case 'Y':
      y = atoi(&argv[i][2]);
      break;
    case 'W':
      width = atoi(&argv[i][2]);
      break;
    case 'H':
      height = atoi(&argv[i][2]);
      break;
    case 'h':
    default:
      show_usage();
      exit(1);
      break;
    }
  }

  window = new oglwindow();
  window->set_doublebuffer(1);
  window->set_title("fadeflip");

  if (use_texture) {
    n_textures = argc - fname;
    if (n_textures == 0) {
      n_textures = 3;
      argc = 3;
      fname = 0;
      def_images[0] = def_image0;
      def_images[1] = def_image1;
      def_images[2] = def_image2;
      argv = def_images;
    }

    texmap->create_from_file(argv[fname]);
    
    if (fix_size) {
      width = texmap->get_width();
      height = texmap->get_height();
      window->set_minsize(width, height);
      window->set_maxsize(width, height);
    } 
  } else n_textures = 0;

  // set position and size after processing the -x (fix_size) flag
  if (x && y)
      window->set_position(x, y);
  if (width && height)
      window->set_size(width, height);

  window->add_event_mask(ButtonReleaseMask);
  window->open();
  window->map();
  window->winset();
  glClear(GL_COLOR_BUFFER_BIT);

  dpy = window->get_display();
  win_width = window->get_width();
  win_height = window->get_height();
  winid = window->get_window();

  /* Tex_width = size of unit square in pixels at given distance when 
   * parallel to viewing plane */
  tex_width = (int)((float)window->get_width() / 
		    (zdist * tan(radians(fov / 2.0))));

  for (i = 0; i < n_textures; i++) {
    texmap->create_from_file(argv[fname++]);

    glNewList(i*2 + 1, GL_COMPILE);
    texmap->specify_texture();
    glEndList();

    glNewList(i*2 + 2, GL_COMPILE);
    if (fade) {
      width = texmap->get_width();
      height = texmap->get_height();
      height = width < height ? width : height;
      for (width = 1; width < height; width *= 2);
      j = 1;
      width /= 2;
      while (width >= tex_width) {
	width /= 2;
	j++;
      }
      while (1) {
	if (rainbow) 
	  texmap->create_color(width, fadecolors[j%n_colors][0],
			       fadecolors[j%n_colors][1], 
			       fadecolors[j%n_colors][2], 1, 1);
	else texmap->create_color(width, 0, 0, 0, 1, 1);
	texmap->set_level(j++);
	texmap->setup_texture();
	texmap->specify_mipmap();
	if (width == 1) break;
	width /= 2;
      }
    }
    glEndList();
    texmap->pixels_free();
  }

  if (frame) zdist = 1.0 / tan(radians(fov) / 2.0);    

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  if (use_texture) glEnable(GL_TEXTURE_2D); 

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glGetFloatv(GL_MODELVIEW_MATRIX, matrix);
  
  if (auto_mode) {
    moving = 1;
    rotx = 20.0;
    roty = 45.0;
  }

  while (1) {
    while(((moving || button_down) && XPending(dpy)) ||
	(moving == 0 && button_down == 0)) {
      XNextEvent(dpy, &event);
      switch(event.type) {
      case Expose:
        last_draw = draw(last_draw);
        break;
      case ConfigureNotify:
        window->resize();
	win_width = window->get_width();
	win_height = window->get_height();
        break;
      case ButtonPress:
	button_down = 1;
	oldmx = event.xbutton.x;
	oldmy = event.xbutton.y;
	last_mouse_time = current_time();
	break;
      case ButtonRelease:
	button_down = 0;
	break;
      case KeyPress:
        XLookupString(&event.xkey, buffer, bufsize, &key, &compose);
        if (key == XK_Escape) exit(0);
	        else if (key == XK_h || key == XK_H) {
          rotx = roty = 0.0;
          moving = 0;
	  paused = 0;
          last_draw = draw(last_draw);
        }
        else if (key == XK_w || key == XK_W) {
          rotx = roty = 0.0;
          glPushMatrix();
          glLoadIdentity();
          glGetFloatv(GL_MODELVIEW_MATRIX, matrix);
          glPopMatrix();
          moving = 0;
	  paused = 0;
          last_draw = draw(last_draw);
        }
	else if (key == XK_space) paused = ~paused;
	break;
      }
    }
    if (moving) time = last_draw = draw(last_draw);
    else time = current_time();
    if (button_down && time - last_mouse_time > mouse_poll) {
      XQueryPointer(dpy, winid, &root_return, &child_return, &root_x,
                    &root_y, &mx, &my, &mask_return);
      moving = mouse_motion(oldmx, oldmy, last_mouse_time, mx, my, time);
      oldmx = mx;
      oldmy = my;
      last_mouse_time = time;
    }
  }

}

