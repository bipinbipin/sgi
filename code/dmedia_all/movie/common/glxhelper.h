/*
 * glxhelper.h:
 *
 *   List of drawing modes supported by GLXCreateWindow (in glxhelper.c).
 * More than this are possible with mixed model, but this is just an 
 * example.  You can either expand this list (and the corresponding code in 
 * GLXCreateWindow) or call the mixed model calls yourself, using 
 * GLXCreateWindow as an example.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GLXcolorIndexSingleBuffer,
    GLXcolorIndexDoubleBuffer,
    GLXrgbSingleBuffer,
    GLXrgbDoubleBuffer
} GLXWindowType;

extern Window GLXCreateWindow( Display*, Window, 
                               int, int, int, int, int, 
                               unsigned long, XSetWindowAttributes *,
                               GLXWindowType);

#ifdef __cplusplus
}
#endif
