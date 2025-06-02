

/* $Revision: 1.1 $ */
#include <stdio.h>
#include <stdlib.h>
#include "XLayerUtil.h"

extern "C" {

static Bool layersRead;
static Atom overlayVisualsAtom;
static OverlayInfo **overlayInfoPerScreen;
static int *numOverlaysPerScreen;

XLayerVisualInfo *
XGetLayerVisualInfo(Display *display, long lvinfo_mask, 
		    XLayerVisualInfo *lvinfo_template, 
		    int *nitems_return)
{
   XVisualInfo *vinfo;
   XLayerVisualInfo *layerInfo;
   Window root;
   Status status;
   Atom actualType;
   unsigned long sizeData, bytesLeft;
   int actualFormat, numVisuals, numScreens, count, i, j;

   vinfo = XGetVisualInfo(display, lvinfo_mask&VisualAllMask,
      &lvinfo_template->vinfo, nitems_return);
   if(vinfo == NULL)
      return NULL;
   numVisuals = *nitems_return;
   if(layersRead == False) {
      overlayVisualsAtom = XInternAtom(display, "SERVER_OVERLAY_VISUALS", True);
      if(overlayVisualsAtom != None) {
         numScreens = ScreenCount(display);
         overlayInfoPerScreen = (OverlayInfo**) malloc(numScreens*sizeof(OverlayInfo*));
         numOverlaysPerScreen = (int*) malloc(numScreens*sizeof(int));
         if(overlayInfoPerScreen != NULL && numOverlaysPerScreen != NULL) {
            for(i=0; i<numScreens; i++) {
               root = RootWindow(display, i);
               status = XGetWindowProperty(display, root, overlayVisualsAtom,
                  0L, (long)10000, False, overlayVisualsAtom, &actualType, &actualFormat,
                  &sizeData, &bytesLeft, (u_char**) &overlayInfoPerScreen[i]);
               if(status!=Success || actualType!=overlayVisualsAtom ||
                 actualFormat!=32 || sizeData<4)
                  numOverlaysPerScreen[i] = 0;
               else
                  numOverlaysPerScreen[i] = sizeData / (sizeof(OverlayInfo)/4);
            }
            layersRead = True;
         } else {
            if(overlayInfoPerScreen != NULL) free(overlayInfoPerScreen);
            if(numOverlaysPerScreen != NULL) free(numOverlaysPerScreen);
         }
      }
   }
   layerInfo = (XLayerVisualInfo*) malloc(numVisuals * sizeof(XLayerVisualInfo));
   if(layerInfo == NULL) {
      XFree(vinfo);
      return NULL;
   }
   count = 0;
   for(i=0; i<numVisuals; i++) {
      XVisualInfo *pVinfo;
      int screen;
      OverlayInfo *overlayInfo;

      pVinfo = &vinfo[i];
      screen = pVinfo->screen;
      overlayInfo = NULL;
      if(layersRead) {
         for(j=0; j<numOverlaysPerScreen[screen]; j++)
            if(pVinfo->visualid == overlayInfoPerScreen[screen][j].overlay_visual) {
               overlayInfo = &overlayInfoPerScreen[screen][j];
	       break;
            }
      }
      if(lvinfo_mask & VisualLayerMask)
         if(overlayInfo == NULL) {
            if(lvinfo_template->layer != 0) continue;
         } else
            if(lvinfo_template->layer != overlayInfo->layer) continue;
      if(lvinfo_mask & VisualTransparentType)
         if(overlayInfo == NULL) {
            if(lvinfo_template->type != None) continue;
         } else
            if(lvinfo_template->type != overlayInfo->transparent_type) continue;
      if(lvinfo_mask & VisualTransparentValue)
         if(overlayInfo == NULL)
            /* non-overlay visuals have no sense of TransparentValue */
            continue;
         else
            if(lvinfo_template->value != overlayInfo->value) continue;
      layerInfo[count].vinfo = *pVinfo;
      if(overlayInfo == NULL) {
         layerInfo[count].layer = 0;
         layerInfo[count].type = None;
         layerInfo[count].value = 0; /* meaningless */
      } else {
         layerInfo[count].layer = overlayInfo->layer;
         layerInfo[count].type = overlayInfo->transparent_type;
         layerInfo[count].value = overlayInfo->value;
      }
      count++;
   }
   XFree(vinfo);
   *nitems_return = count;
   if(count == 0) {
      XFree(layerInfo);
      return NULL;
   } else
      return layerInfo;
}

Status
XMatchLayerVisualInfo(Display *display, int screen, int depth, 
		      int nClass, int layer, XLayerVisualInfo *lvinfo_return)
{
   XLayerVisualInfo *lvinfo;
   XLayerVisualInfo lvinfoTemplate;
   int nitems;

   lvinfoTemplate.vinfo.screen = screen;
   lvinfoTemplate.vinfo.depth = depth;
   lvinfoTemplate.vinfo.c_class = nClass;
   lvinfoTemplate.layer = layer;
   lvinfo = XGetLayerVisualInfo(display,
      VisualScreenMask|VisualDepthMask|VisualClassMask|VisualLayerMask,
      &lvinfoTemplate, &nitems);
   if(lvinfo != NULL && nitems > 0) {
      *lvinfo_return = *lvinfo;
      return 1;
   } else
      return 0;
}

}
