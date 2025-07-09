#include "dialhandler.h"
#include <Inventor/nodes/SoMaterial.h>
#include <math.h>

// Fractal parameters
static float fractalZoom = 1.0f;
static float centerX = 0.0f;
static float centerY = 0.0f;
static int maxIterations = 50;
static float juliaReal = -0.7f;
static float juliaImag = 0.27f;
static float colorShift = 0.0f;
static float morphAmount = 0.0f;
static float fractalTime = 0.0f;
static bool isMandelbrot = true;

// Additional fractal state variables
static int fractalIterations = 50;
static float paletteCycle = 1.0f;
static int mandelbrotMode = 1;
static int psychedelicMode = 0;

// Fractal arrays
static SbVec3f fractalPositions[8];
static float fractalIntensities[8];
static float fractalValues[8];
static int iterationCounts[8];

class DialSet4 : public DialHandler {
public:
    void init(SceneObjects* scene, DialState* state);
    void handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state);
    void cleanup(SceneObjects* scene, DialState* state);
    const char* getDescription();
    const char* getThemeDescription();
    const char* getDialDescription(int dialIndex);

private:
    void handleZoom(float angle, SceneObjects* scene);
    void handleIteration(float angle, SceneObjects* scene);
    void handleJuliaReal(float angle, SceneObjects* scene);
    void handleJuliaImag(float angle, SceneObjects* scene);
    void handleColorShift(float angle, SceneObjects* scene);
    void handlePaletteCycle(float angle, SceneObjects* scene);
    void handleMandelbrotMode(float angle, SceneObjects* scene);
    void handlePsychedelicMode(float angle, SceneObjects* scene);
    void updateFractalVisualization(SceneObjects* scene);
    void updateColorPalette(SceneObjects* scene);
};

const char* DialSet4::getDescription() {
    return "FRACTAL EXPLORER - Mandelbrot and Julia sets with infinite zoom and psychedelic colors";
}

const char* DialSet4::getThemeDescription() {
    return "FRACTAL EXPLORER - Mandelbrot and Julia sets with infinite zoom and psychedelic colors";
}

const char* DialSet4::getDialDescription(int dialIndex) {
    static const char* descriptions[8] = {
        "DIAL 0: Zoom Factor - Infinite zoom into fractal depths",
        "DIAL 1: Iteration Count - Detail level and computation depth",
        "DIAL 2: Julia Real - Real component of Julia set constant",
        "DIAL 3: Julia Imaginary - Imaginary component of Julia set constant",
        "DIAL 4: Color Shift - Hue rotation of fractal colors",
        "DIAL 5: Palette Cycle - Speed of color palette animation",
        "DIAL 6: Mandelbrot Mode - Switch between fractal types",
        "DIAL 7: Psychedelic Mode - Trippy color effects and animations"
    };
    if (dialIndex >= 0 && dialIndex < 8) {
        return descriptions[dialIndex];
    }
    return "Unknown dial";
}

void DialSet4::init(SceneObjects* scene, DialState* state) {
    for (int i = 0; i < 8; i++) {
        fractalPositions[i] = SbVec3f(0, 0, 0);
        fractalIntensities[i] = 0.5f;
        fractalValues[i] = 0.0f;
        iterationCounts[i] = 0;
        
        if (scene->dialMaterials[i]) {
            float hue = (float)i / 8.0f;
            scene->dialMaterials[i]->diffuseColor.setValue(
                0.5f + 0.5f * sinf(hue * 6.28f),
                0.5f + 0.5f * sinf(hue * 6.28f + 2.094f),
                0.5f + 0.5f * sinf(hue * 6.28f + 4.188f)
            );
            scene->dialMaterials[i]->emissiveColor.setValue(0.4f, 0.4f, 0.4f);
            scene->dialMaterials[i]->specularColor.setValue(1.0f, 1.0f, 1.0f);
            scene->dialMaterials[i]->shininess.setValue(1.0f);
        }
    }
    
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    }
}

void DialSet4::cleanup(SceneObjects* scene, DialState* state) {
    for (int i = 0; i < 8; i++) {
        if (scene->scatterOffsets[i]) {
            scene->scatterOffsets[i]->translation.setValue(0, 0, 0);
        }
        if (scene->dialMaterials[i]) {
            scene->dialMaterials[i]->diffuseColor.setValue(0.2f, 0.25f, 0.35f);
            scene->dialMaterials[i]->emissiveColor.setValue(0.03f, 0.03f, 0.05f);
            scene->dialMaterials[i]->specularColor.setValue(0.4f, 0.4f, 0.4f);
            scene->dialMaterials[i]->shininess.setValue(0.6f);
        }
    }
    
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));
    }
}

void DialSet4::handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state) {
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
            handleZoom(state->dialAngles[0], scene);
            break;
        case 1:
            handleIteration(state->dialAngles[1], scene);
            break;
        case 2:
            handleJuliaReal(state->dialAngles[2], scene);
            break;
        case 3:
            handleJuliaImag(state->dialAngles[3], scene);
            break;
        case 4:
            handleColorShift(state->dialAngles[4], scene);
            break;
        case 5:
            handlePaletteCycle(state->dialAngles[5], scene);
            break;
        case 6:
            handleMandelbrotMode(state->dialAngles[6], scene);
            break;
        case 7:
            handlePsychedelicMode(state->dialAngles[7], scene);
            break;
    }

    updateFractalVisualization(scene);
    
    if (scene->dialTransforms[dialIndex]) {
        scene->dialTransforms[dialIndex]->rotation.setValue(SbVec3f(0, 1, 0), state->dialAngles[dialIndex]);
    }
}

void DialSet4::handleZoom(float angle, SceneObjects* scene) {
    fractalZoom = 1.0f + (angle + M_PI) * 100.0f;
}

void DialSet4::handleIteration(float angle, SceneObjects* scene) {
    fractalIterations = 10 + (int)((angle + M_PI) * 100.0f);
}

void DialSet4::handleJuliaReal(float angle, SceneObjects* scene) {
    juliaReal = angle;
}

void DialSet4::handleJuliaImag(float angle, SceneObjects* scene) {
    juliaImag = angle;
}

void DialSet4::handleColorShift(float angle, SceneObjects* scene) {
    colorShift = (angle + M_PI) / (2.0f * M_PI);
}

void DialSet4::handlePaletteCycle(float angle, SceneObjects* scene) {
    paletteCycle = 0.1f + (angle + M_PI) * 5.0f;
}

void DialSet4::handleMandelbrotMode(float angle, SceneObjects* scene) {
    mandelbrotMode = (angle > 0.0f) ? 1 : 0;
}

void DialSet4::handlePsychedelicMode(float angle, SceneObjects* scene) {
    psychedelicMode = (angle > 0.0f) ? 1 : 0;
}

void DialSet4::updateFractalVisualization(SceneObjects* scene) {
    fractalTime += 0.02f;
    
    for (int i = 0; i < 8; i++) {
        if (scene->scatterOffsets[i]) {
            float x = (float)(i % 2) * 6.0f - 3.0f;
            float y = (float)(i / 2) * 3.0f - 6.0f;
            
            fractalPositions[i].setValue(
                x + sinf(fractalTime * paletteCycle + i) * 2.0f,
                y + cosf(fractalTime * paletteCycle + i) * 2.0f,
                sinf(fractalTime * 0.3f + i) * 0.5f
            );
            
            scene->scatterOffsets[i]->translation.setValue(fractalPositions[i]);
        }
    }
    
    updateColorPalette(scene);
}

void DialSet4::updateColorPalette(SceneObjects* scene) {
    for (int i = 0; i < 8; i++) {
        if (scene->dialMaterials[i]) {
            float hue = fmodf(fractalTime * paletteCycle + i * 0.125f + colorShift, 1.0f);
            float r = 0.5f + 0.5f * sinf(hue * 6.28f);
            float g = 0.5f + 0.5f * sinf(hue * 6.28f + 2.094f);
            float b = 0.5f + 0.5f * sinf(hue * 6.28f + 4.188f);
            
            if (psychedelicMode) {
                float intensity = 1.0f + sinf(fractalTime * 5.0f + i) * 0.5f;
                r *= intensity;
                g *= intensity;
                b *= intensity;
            }
            
            scene->dialMaterials[i]->diffuseColor.setValue(r, g, b);
            scene->dialMaterials[i]->emissiveColor.setValue(r * 0.4f, g * 0.4f, b * 0.4f);
        }
    }
}

// Factory function
DialHandler* createDialSet4() {
    return new DialSet4();
} 
