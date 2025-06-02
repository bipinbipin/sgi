#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoNurbsCurve.h>
#include <Inventor/nodes/SoNurbsSurface.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>

// The control points for this surface
float pts[16][3] = {
   {-4.5, -2.0,  8.0},
   {-2.0,  1.0,  8.0},
   { 2.0, -3.0,  6.0},
   { 5.0, -1.0,  8.0},
   {-3.0,  3.0,  4.0},
   { 0.0, -1.0,  4.0},
   { 1.0, -1.0,  4.0},
   { 3.0,  2.0,  4.0},
   {-5.0, -2.0, -2.0},
   {-2.0, -4.0, -2.0},
   { 2.0, -1.0, -2.0},
   { 5.0,  0.0, -2.0},
   {-4.5,  2.0, -6.0},
   {-2.0, -4.0, -5.0},
   { 2.0,  3.0, -5.0},
   { 4.5, -2.0, -6.0}};

// The knot vector
float knots[8] = {
   0, 0, 0, 0, 1, 1, 1, 1};

// Create the nodes needed for the Bezier surface.
SoSeparator *
makeSurface()
{
   SoSeparator *surfSep = new SoSeparator();
   surfSep->ref();

   // Define the Bezier surface including the control
   // points and a complexity.
   SoComplexity  *complexity = new SoComplexity;
   SoCoordinate3 *controlPts = new SoCoordinate3;
   SoNurbsSurface  *surface  = new SoNurbsSurface;
   complexity->value = 0.7;
   controlPts->point.setValues(0, 8, pts);
   surface->numUControlPoints = 4;
   surface->numVControlPoints = 4;
   surface->uKnotVector.setValues(0, 8, knots);
   surface->vKnotVector.setValues(0, 8, knots);
   surfSep->addChild(complexity);
   surfSep->addChild(controlPts);
   surfSep->addChild(surface);

   surfSep->unrefNoDelete();
   return surfSep;
}


void
main(int, char **argv)
{
   // Initialize Inventor and Xt
   Widget myWindow = SoXt::init(argv[0]);
   if (myWindow == NULL) exit(1);

   SoSeparator *root = makeSurface();
   root->ref();

   SoXtExaminerViewer *myViewer = 
            new SoXtExaminerViewer(myWindow);
   myViewer->setSceneGraph(root);
   myViewer->setTitle("b-spline curve");
   myViewer->show();
   myViewer->viewAll();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}
