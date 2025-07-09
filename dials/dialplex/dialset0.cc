#include "dialhandler.h"
#include <Inventor/nodes/SoMaterial.h>
#include <math.h>

// Helper arrays for scatter and bloom effects
static SbVec3f lastScatter6[8];
static SbVec3f lastScatter7[8];
static SbVec3f bloomVectors[8];

class DialSet0 : public DialHandler {
public:
    void init(SceneObjects* scene, DialState* state);
    void handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state);
    void cleanup(SceneObjects* scene, DialState* state);
    const char* getDescription();
    const char* getThemeDescription();
    const char* getDialDescription(int dialIndex);

private:
    void setBackgroundFromDial(float angle, SceneObjects* scene);
    void handleCylinderHeight(float angle, SceneObjects* scene);
    void handleScatterChaos(float angle, SceneObjects* scene);
    void handleBloomEffect(float angle, SceneObjects* scene);
};

void DialSet0::init(SceneObjects* scene, DialState* state) {
    // Initialize bloom vectors for dial 7 effect
    for (int x = 0; x < 8; x++) {
        float angle = x * (M_PI / 4.0f);  // 8 directions evenly spaced
        bloomVectors[x] = SbVec3f(cosf(angle), sinf(angle), ((x % 2 == 0) ? 0.5f : -0.5f));
        lastScatter6[x] = SbVec3f(0, 0, 0);
        lastScatter7[x] = SbVec3f(0, 0, 0);
    }
}

void DialSet0::cleanup(SceneObjects* scene, DialState* state) {
    // Reset any visual effects to neutral state
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));
    }
}

const char* DialSet0::getDescription() {
    return "CLASSIC SCENE CONTROL - Scene rotation, camera, effects";
}

const char* DialSet0::getThemeDescription() {
    return "CLASSIC SCENE CONTROL - Original 3D manipulation and visual effects";
}

const char* DialSet0::getDialDescription(int dialIndex) {
    static const char* descriptions[8] = {
        "DIAL 0: Background Hue - Change background color temperature",
        "DIAL 1: Scene Y-Rotation - Rotate entire scene around Y-axis", 
        "DIAL 2: Scene Zoom - Move camera closer/farther from scene",
        "DIAL 3: Scene X-Rotation - Rotate entire scene around X-axis",
        "DIAL 4: Cylinder Height - Extend/retract dial cylinders",
        "DIAL 5: Scene Z-Rotation - Rotate entire scene around Z-axis",
        "DIAL 6: Scatter Chaos - Spread dials apart with chaotic motion",
        "DIAL 7: Bloom Effect - Add radial glow and material morphing"
    };
    if (dialIndex >= 0 && dialIndex < 8) {
        return descriptions[dialIndex];
    }
    return "Unknown dial";
}

void DialSet0::handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state) {
    // Handle first event skip
    if (state->firstDialEvent[dialIndex]) {
        state->lastDialValues[dialIndex] = rawValue;
        state->firstDialEvent[dialIndex] = false;
        return;
    }

    int delta = rawValue - state->lastDialValues[dialIndex];
    state->lastDialValues[dialIndex] = rawValue;

    float deltaAngle = delta * (M_PI / 128.0f);
    state->dialAngles[dialIndex] += deltaAngle;

    switch (dialIndex) {
        case 0:
            setBackgroundFromDial(state->dialAngles[0], scene);
            break;

        case 1:
            if (scene->sceneRotY)
                scene->sceneRotY->rotation.setValue(SbVec3f(0, 1, 0), state->dialAngles[1]);
            break;

        case 2:
            if (scene->sceneZoom) {
               float zoomZ = -5.0f + state->dialAngles[2] * 2.0f;
               scene->sceneZoom->translation.setValue(0, 0, zoomZ);
            }
            break;

        case 3:
            if (scene->sceneRotX)
                scene->sceneRotX->rotation.setValue(SbVec3f(1, 0, 0), state->dialAngles[3]);
            break;
         
        case 4:
            handleCylinderHeight(state->dialAngles[4], scene);
            break;

        case 5:
            if (scene->sceneRotZ)
                scene->sceneRotZ->rotation.setValue(SbVec3f(0, 0, 1), state->dialAngles[5]);
            break;

        case 6:
            handleScatterChaos(state->dialAngles[6], scene);
            break;
            
        case 7:
            handleBloomEffect(state->dialAngles[7], scene);
            break;
    }

    // Default: rotate own object
    if (scene->dialTransforms[dialIndex]) {
        scene->dialTransforms[dialIndex]->rotation.setValue(SbVec3f(0, 1, 0), state->dialAngles[dialIndex]);
    }
}

void DialSet0::setBackgroundFromDial(float angle, SceneObjects* scene) {
    if (!scene->globalViewer) return;

    float phase = fmodf(angle / (4.0f * M_PI), 1.0f);
    if (phase < 0) phase += 1.0f;

    float r = 0.2f + 0.1f * sinf(2 * M_PI * phase + M_PI / 3);   // low red
    float g = 0.2f + 0.1f * sinf(2 * M_PI * phase + M_PI);       // dark green
    float b = 0.3f + 0.3f * sinf(2 * M_PI * phase);              // deep blue/purple

    // Clamp to [0,1] manually
    r = (r < 0.0f) ? 0.0f : ((r > 1.0f) ? 1.0f : r);
    g = (g < 0.0f) ? 0.0f : ((g > 1.0f) ? 1.0f : g);
    b = (b < 0.0f) ? 0.0f : ((b > 1.0f) ? 1.0f : b);

    scene->globalViewer->setBackgroundColor(SbColor(r, g, b));
    scene->globalViewer->render();
}

void DialSet0::handleCylinderHeight(float angle, SceneObjects* scene) {
    for (int i = 0; i < 8; i++) {
       if (scene->dialCylinders[i] && scene->cylinderOffsets[i]) {
             float baseHeight = 0.2f;
             float adjustedAngle = (angle > 0.0f) ? angle : 0.0f;
             float scale = 1.0f + adjustedAngle * 0.2f;

             // Set cylinder height
             float newHeight = baseHeight * scale;
             scene->dialCylinders[i]->height = newHeight;

             // Offset cylinder upward by half height to grow downward
             scene->cylinderOffsets[i]->translation.setValue(0, newHeight / 2.0f, 0);
       }
    }
}

void DialSet0::handleScatterChaos(float angle, SceneObjects* scene) {
    for (int i = 0; i < 8; i++) {
       if (scene->scatterOffsets[i]) {
             float strength = 0.6f * sinf(angle);

             // Pseudo-random direction
             float dx = strength * sinf(i * 1.7f + angle);
             float dy = strength * cosf(i * 2.3f + angle);
             float dz = strength * sinf(i * 3.1f - angle);

             SbVec3f newOffset(dx, dy, dz);

             // Apply delta
             SbVec3f delta = newOffset - lastScatter6[i];
             lastScatter6[i] = newOffset;

             // Apply to current transform
             SbVec3f current = scene->scatterOffsets[i]->translation.getValue();
             scene->scatterOffsets[i]->translation.setValue(current + delta);
       }
    }
}

void DialSet0::handleBloomEffect(float angle, SceneObjects* scene) {
    for (int i = 0; i < 8; i++) {
       if (scene->scatterOffsets[i]) {
             float t = angle;

             float radius = t * 0.5f;
             SbVec3f newOffset = bloomVectors[i] * radius;
             SbVec3f delta = newOffset - lastScatter7[i];
             lastScatter7[i] = newOffset;

             SbVec3f current = scene->scatterOffsets[i]->translation.getValue();
             scene->scatterOffsets[i]->translation.setValue(current + delta);

             if (scene->dialMaterials[i]) {
                float absRadius = (radius < 0.0f) ? -radius : radius;
                float norm = (absRadius < 3.5f) ? (absRadius / 3.5f) : 1.0f;
                float fade = norm * norm;

                // Strong transition from plastic → electric blue steel
                float r = 0.6f * (1.0f - fade) + 0.1f * fade;
                float g = 0.6f * (1.0f - fade) + 0.2f * fade;
                float b = 0.6f * (1.0f - fade) + 1.0f * fade;

                // Specular color morphs to white highlight
                float spec = 0.0f * (1.0f - fade) + 1.0f * fade;

                // Increase shininess rapidly — gives tight reflection
                float shine = 0.0f * (1.0f - fade) + 1.0f * fade;

                scene->dialMaterials[i]->diffuseColor.setValue(r, g, b);
                scene->dialMaterials[i]->specularColor.setValue(spec, spec, spec);
                scene->dialMaterials[i]->shininess.setValue(shine);
             }
       }
    }
}

// Factory function
DialHandler* createDialSet0() {
    return new DialSet0();
} 
