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
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>

// The control points for this curve
float pts[13][3] = {
   { 6.0,  0.0,  6.0},
   {-5.5,  0.5,  5.5},
   {-5.0,  1.0, -5.0},
   { 4.5,  1.5, -4.5},
   { 4.0,  2.0,  4.0},
   {-3.5,  2.5,  3.5},
   {-3.0,  3.0, -3.0},
   { 2.5,  3.5, -2.5},
   { 2.0,  4.0,  2.0},
   {-1.5,  4.5,  1.5},
   {-1.0,  5.0, -1.0},
   { 0.5,  5.5, -0.5},
   { 0.0,  6.0,  0.0}};

// The knot vector
float knots[17] = {
   0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10};

// Create the nodes needed for the B-Spline curve.
SoSeparator *
makeCurve()
{
   SoSeparator *curveSep = new SoSeparator();
   curveSep->ref();

   // Set the draw style of the curve.
   SoDrawStyle *drawStyle  = new SoDrawStyle;
   drawStyle->lineWidth = 4;
   curveSep->addChild(drawStyle);

   // Define the NURBS curve including the control points
   // and a complexity.
   SoComplexity  *complexity = new SoComplexity;
   SoCoordinate3 *controlPts = new SoCoordinate3;
   SoNurbsCurve  *curve      = new SoNurbsCurve;
   complexity->value = 0.8;
   controlPts->point.setValues(0, 13, pts);
   curve->numControlPoints = 13;
   curve->knotVector.setValues(0, 17, knots);
   curveSep->addChild(complexity);
   curveSep->addChild(controlPts);
   curveSep->addChild(curve);

   curveSep->unrefNoDelete();
   return curveSep;
}

void
main(int, char **argv)
{
   // Initialize Inventor and Xt
   Widget myWindow = SoXt::init(argv[0]);
   if (myWindow == NULL) exit(1);

   SoSeparator *root = makeCurve();
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
