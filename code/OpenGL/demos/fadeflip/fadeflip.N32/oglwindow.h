#include <GL/glu.h>
#include <GL/glx.h>

const int max_titlelength = 80;

class oglwindow {
 public:
  oglwindow();
  ~oglwindow();

  // Call these before calling open;
  void set_doublebuffer(int new_doublebuffer);
  void set_position(int new_x, int new_y);
  void set_size(int new_w, int new_h);
  void set_minsize(int new_minx, int new_miny);
  void set_maxsize(int new_maxx, int new_maxy);
  void set_title(char *new_title);
  void set_event_mask(unsigned long new_event_mask);
  void add_event_mask(unsigned long  new_flag);

  int get_width();
  int get_height();

  char *get_title();

  unsigned long get_event_mask();

  void open();
  void map();
  void winset();
  void swapbuffers();
  void resize();

  Display *get_display();
  Window get_window();

  int get_doublebuffer();

 private:
  int doublebuffer;
  int window_x, window_y, window_w, window_h;
  int minx, miny, maxx, maxy;

  char title[max_titlelength];

  unsigned long event_mask;

  Display *dpy;
  Window window;
  GLXContext ctx;
  XVisualInfo *vi;

};
