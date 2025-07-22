#include "dialhandler.h"
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <math.h>

// Simple sphere control parameters
static float sphereHue = 0.0f;          // Dial 0: Sphere color hue
static float backgroundHue = 0.0f;      // Dial 1: Background color hue
static float xStretch = 1.0f;           // Dial 2: X-axis stretch
static float yStretch = 1.0f;           // Dial 3: Y-axis stretch
static float zStretch = 1.0f;           // Dial 4: Z-axis stretch
static float warbleAmount = 0.0f;       // Dial 5: Warble intensity
static float sphereSize = 2.0f;         // Dial 6: Overall size
static float sphereComplexity = 0.5f;   // Dial 7: Sphere complexity

static float timeCounter = 0.0f;        // For warble animation

class DialSet9 : public DialHandler {
public:
    void init(SceneObjects* scene, DialState* state);
    void handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state);
    void cleanup(SceneObjects* scene, DialState* state);
    const char* getDescription();
    const char* getThemeDescription();
    const char* getDialDescription(int dialIndex);

private:
    void handleSphereColor(float angle, SceneObjects* scene);
    void handleBackgroundColor(float angle, SceneObjects* scene);
    void handleXStretch(float angle, SceneObjects* scene);
    void handleYStretch(float angle, SceneObjects* scene);
    void handleZStretch(float angle, SceneObjects* scene);
    void handleWarble(float angle, SceneObjects* scene);
    void handleSize(float angle, SceneObjects* scene);
    void handleComplexity(float angle, SceneObjects* scene);
    void updateTransform(SceneObjects* scene);
    void hueToRGB(float hue, float& r, float& g, float& b);
};

const char* DialSet9::getDescription() {
    return "SPHERE STUDIO - Complete sphere control with color, stretch, warble, and complexity";
}

const char* DialSet9::getThemeDescription() {
    return "SPHERE STUDIO - Full featured sphere manipulation";
}

const char* DialSet9::getDialDescription(int dialIndex) {
    static const char* descriptions[8] = {
        "DIAL 0: Sphere Color - Hue cycle for sphere material",
        "DIAL 1: Background Color - Hue cycle for scene background", 
        "DIAL 2: X-Axis Stretch - Horizontal stretching (unlimited)",
        "DIAL 3: Y-Axis Stretch - Vertical stretching (unlimited)",
        "DIAL 4: Z-Axis Stretch - Depth stretching (unlimited)",
        "DIAL 5: Warble - Animated wobbling (unlimited intensity)",
        "DIAL 6: Size - Overall sphere size",
        "DIAL 7: Complexity - Sphere detail/segments"
    };
    if (dialIndex >= 0 && dialIndex < 8) {
        return descriptions[dialIndex];
    }
    return "Unknown dial";
}

void DialSet9::init(SceneObjects* scene, DialState* state) {
    // Reset all parameters
    sphereHue = 0.5f;
    backgroundHue = 0.2f;
    xStretch = 1.0f;
    yStretch = 1.0f;
    zStretch = 1.0f;
    warbleAmount = 0.0f;
    sphereSize = 2.0f;
    sphereComplexity = 0.5f;
    timeCounter = 0.0f;
    
    // Initialize sphere
    if (scene->mainSphere) {
        scene->mainSphere->radius.setValue(sphereSize);
    }
    
    // Set initial sphere color (orange)
    if (scene->sphereMaterial) {
        scene->sphereMaterial->diffuseColor.setValue(0.8f, 0.4f, 0.2f);
        scene->sphereMaterial->specularColor.setValue(0.9f, 0.9f, 0.9f);
        scene->sphereMaterial->shininess.setValue(0.8f);
        scene->sphereMaterial->transparency.setValue(0.0f);
        scene->sphereMaterial->emissiveColor.setValue(0.0f, 0.0f, 0.0f);
    }
    
    // Position sphere lower in scene
    if (scene->sphereTransform) {
        scene->sphereTransform->translation.setValue(0, -4, 0);  // Move down 4 units
        scene->sphereTransform->rotation.setValue(SbVec3f(0, 1, 0), 0.0f);
        scene->sphereTransform->scaleFactor.setValue(1, 1, 1);
    }
    
    // Set initial background color (dark blue)
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(0.05f, 0.1f, 0.2f));
    }
}

void DialSet9::cleanup(SceneObjects* scene, DialState* state) {
    // Reset to defaults
    if (scene->mainSphere) {
        scene->mainSphere->radius.setValue(2.0f);
    }
    
    if (scene->sphereMaterial) {
        scene->sphereMaterial->diffuseColor.setValue(0.8f, 0.4f, 0.2f);
        scene->sphereMaterial->specularColor.setValue(0.9f, 0.9f, 0.9f);
        scene->sphereMaterial->shininess.setValue(0.8f);
        scene->sphereMaterial->transparency.setValue(0.0f);
        scene->sphereMaterial->emissiveColor.setValue(0.0f, 0.0f, 0.0f);
    }
    
    if (scene->sphereTransform) {
        scene->sphereTransform->translation.setValue(0, -4, 0);
        scene->sphereTransform->rotation.setValue(SbVec3f(0, 1, 0), 0.0f);
        scene->sphereTransform->scaleFactor.setValue(1, 1, 1);
    }
    
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(0.02f, 0.05f, 0.15f));
    }
}

void DialSet9::handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state) {
    // Convert raw value to angle
    float angle = (float)rawValue / 100.0f * 2.0f * M_PI;
    state->dialAngles[dialIndex] = angle;
    
    // Update time counter for warble
    timeCounter += 0.02f;
    
    switch (dialIndex) {
        case 0: handleSphereColor(angle, scene); break;
        case 1: handleBackgroundColor(angle, scene); break;
        case 2: handleXStretch(angle, scene); break;
        case 3: handleYStretch(angle, scene); break;
        case 4: handleZStretch(angle, scene); break;
        case 5: handleWarble(angle, scene); break;
        case 6: handleSize(angle, scene); break;
        case 7: handleComplexity(angle, scene); break;
    }
    
    // Update transform
    updateTransform(scene);
}

void DialSet9::handleSphereColor(float angle, SceneObjects* scene) {
    // Map angle to hue (0 to 1)
    sphereHue = (sinf(angle) * 0.5f + 0.5f);
    
    // Convert hue to RGB
    float r, g, b;
    hueToRGB(sphereHue, r, g, b);
    
    if (scene->sphereMaterial) {
        scene->sphereMaterial->diffuseColor.setValue(r, g, b);
    }
    
    printf("Sphere hue: %.2f, RGB: (%.2f, %.2f, %.2f)\n", sphereHue, r, g, b);
}

void DialSet9::handleBackgroundColor(float angle, SceneObjects* scene) {
    // Map angle to hue (0 to 1)
    backgroundHue = (sinf(angle) * 0.5f + 0.5f);
    
    // Convert hue to RGB with darker values for background
    float r, g, b;
    hueToRGB(backgroundHue, r, g, b);
    
    // Make background colors darker
    r *= 0.3f;
    g *= 0.3f;
    b *= 0.3f;
    
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(r, g, b));
    }
    
    printf("Background hue: %.2f, RGB: (%.2f, %.2f, %.2f)\n", backgroundHue, r, g, b);
}

void DialSet9::handleXStretch(float angle, SceneObjects* scene) {
    // Map angle to stretch factor (unlimited range)
    xStretch = 0.1f + (sinf(angle) * 0.5f + 0.5f) * 4.9f;  // 0.1 to 5.0
    
    printf("X stretch: %.2f\n", xStretch);
}

void DialSet9::handleYStretch(float angle, SceneObjects* scene) {
    // Map angle to stretch factor (unlimited range)
    yStretch = 0.1f + (sinf(angle) * 0.5f + 0.5f) * 4.9f;  // 0.1 to 5.0
    
    printf("Y stretch: %.2f\n", yStretch);
}

void DialSet9::handleZStretch(float angle, SceneObjects* scene) {
    // Map angle to stretch factor (unlimited range)
    zStretch = 0.1f + (sinf(angle) * 0.5f + 0.5f) * 4.9f;  // 0.1 to 5.0
    
    printf("Z stretch: %.2f\n", zStretch);
}

void DialSet9::handleWarble(float angle, SceneObjects* scene) {
    // Map angle to warble intensity (unlimited)
    warbleAmount = (sinf(angle) * 0.5f + 0.5f) * 2.0f;  // 0 to 2.0
    
    printf("Warble intensity: %.2f\n", warbleAmount);
}

void DialSet9::handleSize(float angle, SceneObjects* scene) {
    // Map angle to size (0.5 to 5.0)
    sphereSize = 0.5f + (sinf(angle) * 0.5f + 0.5f) * 4.5f;
    
    if (scene->mainSphere) {
        scene->mainSphere->radius.setValue(sphereSize);
    }
    
    printf("Sphere size: %.2f\n", sphereSize);
}

void DialSet9::handleComplexity(float angle, SceneObjects* scene) {
    // Map angle to complexity (0 to 1)
    sphereComplexity = sinf(angle) * 0.5f + 0.5f;
    
    // Note: Open Inventor SoSphere doesn't directly control segments,
    // but this gives visual feedback for complexity setting
    
    printf("Sphere complexity: %.2f\n", sphereComplexity);
}

void DialSet9::updateTransform(SceneObjects* scene) {
    if (!scene->sphereTransform) return;
    
    // Calculate final scale with warble
    float finalXStretch = xStretch;
    float finalYStretch = yStretch;
    float finalZStretch = zStretch;
    
    // Add warble effect if enabled
    if (warbleAmount > 0.01f) {
        finalXStretch += sinf(timeCounter * 2.0f) * warbleAmount * 0.3f;
        finalYStretch += cosf(timeCounter * 1.5f) * warbleAmount * 0.25f;
        finalZStretch += sinf(timeCounter * 2.5f) * warbleAmount * 0.2f;
        
        // Also add position warble
        float wobbleX = sinf(timeCounter * 1.2f) * warbleAmount * 0.5f;
        float wobbleY = cosf(timeCounter * 1.8f) * warbleAmount * 0.3f;
        float wobbleZ = sinf(timeCounter * 1.6f) * warbleAmount * 0.4f;
        
        scene->sphereTransform->translation.setValue(wobbleX, -4 + wobbleY, wobbleZ);
    } else {
        // Keep sphere positioned lower without wobble
        scene->sphereTransform->translation.setValue(0, -4, 0);
    }
    
    // Apply stretching
    scene->sphereTransform->scaleFactor.setValue(finalXStretch, finalYStretch, finalZStretch);
    
    // Optional: slight rotation for visual interest
    if (warbleAmount > 0.01f) {
        float rotX = warbleAmount * sinf(timeCounter * 0.8f) * 0.2f;
        float rotY = timeCounter * 0.3f;
        float rotZ = warbleAmount * cosf(timeCounter * 0.6f) * 0.15f;
        
        SbRotation rotationX(SbVec3f(1, 0, 0), rotX);
        SbRotation rotationY(SbVec3f(0, 1, 0), rotY);
        SbRotation rotationZ(SbVec3f(0, 0, 1), rotZ);
        
        SbRotation finalRotation = rotationX * rotationY * rotationZ;
        scene->sphereTransform->rotation.setValue(finalRotation);
    } else {
        scene->sphereTransform->rotation.setValue(SbVec3f(0, 1, 0), 0.0f);
    }
}

void DialSet9::hueToRGB(float hue, float& r, float& g, float& b) {
    // Convert HSV to RGB (with S=1, V=1)
    float h = hue * 6.0f;  // Hue in 0-6 range
    int i = (int)h;
    float f = h - i;
    
    switch (i % 6) {
        case 0: r = 1.0f; g = f;    b = 0.0f; break;
        case 1: r = 1-f;  g = 1.0f; b = 0.0f; break;
        case 2: r = 0.0f; g = 1.0f; b = f;    break;
        case 3: r = 0.0f; g = 1-f;  b = 1.0f; break;
        case 4: r = f;    g = 0.0f; b = 1.0f; break;
        case 5: r = 1.0f; g = 0.0f; b = 1-f;  break;
    }
}

// Factory function
DialHandler* createDialSet9() {
    return new DialSet9();
} 