#include <GL/glc.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    int i;
    GLfloat v[8];

    glcContext(glcGenContext());
    glcNewFontFromFamily(1, argv[1]);
    if (glcMeasureString(GL_TRUE, argv[2]) != 0) {
	for (i = 0; i < strlen(argv[2]); i++) {
	    printf("'%c'\n", argv[2][i]);
	    glcGetStringCharMetric(i, GLC_BASELINE, v);
	    printf("baseline - %f, %f, %f, %f\n", v[0], v[1], v[2], v[3]);
	    glcGetStringCharMetric(i, GLC_BOUNDS, v);
	    printf("bounds - %f, %f, %f, %f\n", v[6], v[7], v[4], v[5]);
	    printf("         %f, %f, %f, %f\n", v[0], v[1], v[2], v[3]);
	    printf("\n");
	}
    }
    glcDeleteFont(1);
    printf("Error = %d\n", glcGetError());
    return 0;
}
