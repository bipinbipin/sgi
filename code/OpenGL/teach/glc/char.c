#include <GL/glc.h>
#include <stdio.h>

int main(void)
{
    GLint i, j, masterCount, charCount, fontID;

    glcContext(glcGenContext());
    masterCount = glcGeti(GLC_MASTER_COUNT);
    for (i = 0 ; i < masterCount ; ++i) {
        printf("GLC_FAMILY = \"%s\" ", glcGetMasterc(i, GLC_FAMILY));
        printf("GLC_VENDOR = \"%s\"\n", glcGetMasterc(i, GLC_VENDOR));
        j = glcGetMasteri(i, GLC_MIN_MAPPED_CODE);
        printf("  master min mapped code = %d\n", j);
        j = glcGetMasteri(i, GLC_MAX_MAPPED_CODE);
        printf("  master max mapped code = %d\n", j);
        charCount = glcGetMasteri(i, GLC_CHAR_COUNT);
        printf("  master char count = %d\n", charCount);
        for (j = 0; j < charCount; j++) {
            printf("    %d, \"%s\"\n", j,
		   glcGetMasterListc(i, GLC_CHAR_LIST, j));
        }
        glcNewFontFromMaster(1, i);
        j = glcGetFonti(1, GLC_MIN_MAPPED_CODE);
        printf("  font min mapped code = %d\n", j);
        j = glcGetFonti(1, GLC_MAX_MAPPED_CODE);
        printf("  font max mapped code = %d\n", j);
        charCount = glcGetFonti(1, GLC_CHAR_COUNT);
        printf("  font char count = %d\n", charCount);
        for (j = 0 ; j < charCount ; ++j) {
            printf("    %d, %s\n", j, glcGetFontListc(1, GLC_CHAR_LIST, j));
        }
        printf("\n");
        glcDeleteFont(1);
    }
    printf("Error = %d\n", glcGetError());
    return 0;
}
