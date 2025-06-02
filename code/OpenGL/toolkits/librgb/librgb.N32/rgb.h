/*
** RGB Image Structure
*/

typedef struct _RGBImageRec {
    GLint sizeX, sizeY;
    unsigned char *data;
} RGBImageRec;

extern RGBImageRec *rgbImageLoad(char *);
