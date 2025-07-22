#ifndef DIALHANDLER_H
#define DIALHANDLER_H

#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>

// Forward declarations
enum GeometryMode {
    GEOMETRY_CYLINDERS = 0,
    GEOMETRY_SINGLE_SPHERE = 1
};

struct SceneObjects {
    SoTransform* sceneZoom;
    SoTransform* sceneRotX;
    SoTransform* sceneRotY;
    SoTransform* sceneRotZ;
    SoTransform* dialTransforms[8];
    SoTransform* scatterOffsets[8];
    SoTransform* cylinderOffsets[8];
    SoMaterial* dialMaterials[8];
    SoCylinder* dialCylinders[8];
    SoXtExaminerViewer* globalViewer;
    
    // New sphere-based geometry support
    GeometryMode currentGeometry;
    SoSphere* mainSphere;
    SoTransform* sphereTransform;
    SoMaterial* sphereMaterial;
    SoSwitch* sphereGroup;
    SoSwitch* cylinderGroup;
};

struct DialState {
    float dialAngles[8];
    int lastDialValues[8];
    bool firstDialEvent[8];
    void* handlerData; // For dial set specific state
};

// Base class for dial handlers
class DialHandler {
public:
    virtual ~DialHandler() {}
    
    // Core functionality
    virtual void init(SceneObjects* scene, DialState* state) = 0;
    virtual void handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state) = 0;
    virtual void cleanup(SceneObjects* scene, DialState* state) = 0;
    virtual const char* getDescription() = 0;
    
    // New methods for overlay system
    virtual const char* getThemeDescription() = 0;
    virtual const char* getDialDescription(int dialIndex) = 0;
};

typedef DialHandler* (*DialHandlerFactory)();

#endif // DIALHANDLER_H
