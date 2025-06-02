/*****************************************************************************
 * isfast - routines for *subjective* tests of OpenGL performance
 *****************************************************************************/



#ifndef __ISFAST_H__
#define __ISFAST_H__
#ifdef __cplusplus
extern "C" {
#endif


int IsFastOpenWindow(const char* attributes, const char* title);
void IsFastCloseWindow(void);

int DepthBufferingIsFast(void);
int ImmediateModeIsFast(void);
int LineAAIsFast(void);
int StencillingIsFast(void);
int TextureMappingIsFast(void);



#ifdef X11
int IsFastXOpenDisplay(const char* displayName);
void IsFastXCloseDisplay(void);
#endif



#ifdef __cplusplus
}
#endif
#endif /* !__ISFAST_H__ */


