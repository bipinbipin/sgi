#include <stdio.h>
#include <GL/glc.h>

int main(void)
{
    GLint i, j, masterCount, faceCount;

    glcContext(glcGenContext());

    glcPrependCatalog("/usr/lib/X11/fonts/misc");

    masterCount = glcGeti(GLC_MASTER_COUNT);
    for (i = 0; i < masterCount; i++) {
        printf("GLC_FAMILY = \"%s\" ", glcGetMasterc(i, GLC_FAMILY));
        printf("GLC_VENDOR = \"%s\"\n", glcGetMasterc(i, GLC_VENDOR));
        faceCount = glcGetMasteri(i, GLC_FACE_COUNT);
        for (j = 0; j < faceCount; j++) {
            printf("  Face = \"%s\"\n", glcGetMasterListc(i, GLC_FACE_LIST, j));
        }
        printf("\n");
    }
    return 0;
}
