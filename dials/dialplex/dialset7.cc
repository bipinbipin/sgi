#include "dialhandler.h"
#include <Inventor/nodes/SoMaterial.h>
#include <math.h>

// Synthesizer state variables
static float bassFrequency = 60.0f;
static float trebleFrequency = 4000.0f;
static int waveformType = 0;
static float filterCutoff = 1000.0f;
static float resonance = 0.5f;
static float distortion = 0.0f;
static float delayTime = 0.5f;
static float chorusDepth = 0.0f;
static float synthTime = 0.0f;

// Visual state arrays
static float vuLevels[8];
static float waveformPhases[8];

class DialSet7 : public DialHandler {
public:
    void init(SceneObjects* scene, DialState* state);
    void handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state);
    void cleanup(SceneObjects* scene, DialState* state);
    const char* getDescription();
    const char* getThemeDescription();
    const char* getDialDescription(int dialIndex);

private:
    void handleBassFrequency(float angle, SceneObjects* scene);
    void handleTrebleFrequency(float angle, SceneObjects* scene);
    void handleWaveformType(float angle, SceneObjects* scene);
    void handleFilterCutoff(float angle, SceneObjects* scene);
    void handleResonance(float angle, SceneObjects* scene);
    void handleDistortion(float angle, SceneObjects* scene);
    void handleDelayTime(float angle, SceneObjects* scene);
    void handleChorusDepth(float angle, SceneObjects* scene);
    void updateSynthesizerVisualization(SceneObjects* scene);
    void updateNeonEffects(SceneObjects* scene);
};

const char* DialSet7::getDescription() {
    return "RETRO SYNTHESIZER - 80s synthwave visuals with neon colors and VU meters";
}

const char* DialSet7::getThemeDescription() {
    return "RETRO SYNTHESIZER - 80s synthwave visuals with neon colors and VU meters";
}

const char* DialSet7::getDialDescription(int dialIndex) {
    static const char* descriptions[8] = {
        "DIAL 0: Bass Frequency - Low-end waveform generation",
        "DIAL 1: Treble Frequency - High-end harmonic content",
        "DIAL 2: Waveform Type - Sine, saw, square, triangle waves",
        "DIAL 3: Filter Cutoff - Frequency filtering threshold",
        "DIAL 4: Resonance - Filter feedback and emphasis",
        "DIAL 5: Distortion - Harmonic saturation and overdrive",
        "DIAL 6: Delay Time - Echo and temporal effects",
        "DIAL 7: Chorus Depth - Modulation width and richness"
    };
    if (dialIndex >= 0 && dialIndex < 8) {
        return descriptions[dialIndex];
    }
    return "Unknown dial";
}

void DialSet7::init(SceneObjects* scene, DialState* state) {
    for (int i = 0; i < 8; i++) {
        vuLevels[i] = 0.0f;
        waveformPhases[i] = 0.0f;
        
        if (scene->dialMaterials[i]) {
            float neonIntensity = 0.6f + (float)i / 16.0f;
            scene->dialMaterials[i]->diffuseColor.setValue(neonIntensity, 0.3f, neonIntensity);
            scene->dialMaterials[i]->emissiveColor.setValue(neonIntensity * 0.8f, 0.2f, neonIntensity * 0.6f);
            scene->dialMaterials[i]->specularColor.setValue(1.0f, 1.0f, 1.0f);
            scene->dialMaterials[i]->shininess.setValue(1.0f);
        }
    }
    
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(0.05f, 0.0f, 0.1f));
    }
}

void DialSet7::cleanup(SceneObjects* scene, DialState* state) {
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

void DialSet7::handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state) {
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
            handleBassFrequency(state->dialAngles[0], scene);
            break;
        case 1:
            handleTrebleFrequency(state->dialAngles[1], scene);
            break;
        case 2:
            handleWaveformType(state->dialAngles[2], scene);
            break;
        case 3:
            handleFilterCutoff(state->dialAngles[3], scene);
            break;
        case 4:
            handleResonance(state->dialAngles[4], scene);
            break;
        case 5:
            handleDistortion(state->dialAngles[5], scene);
            break;
        case 6:
            handleDelayTime(state->dialAngles[6], scene);
            break;
        case 7:
            handleChorusDepth(state->dialAngles[7], scene);
            break;
    }

    updateSynthesizerVisualization(scene);
    
    if (scene->dialTransforms[dialIndex]) {
        scene->dialTransforms[dialIndex]->rotation.setValue(SbVec3f(0, 1, 0), state->dialAngles[dialIndex]);
    }
}

void DialSet7::handleBassFrequency(float angle, SceneObjects* scene) {
    bassFrequency = 20.0f + (angle + M_PI) * 200.0f;
}

void DialSet7::handleTrebleFrequency(float angle, SceneObjects* scene) {
    trebleFrequency = 1000.0f + (angle + M_PI) * 10000.0f;
}

void DialSet7::handleWaveformType(float angle, SceneObjects* scene) {
    waveformType = (int)((angle + M_PI) / (2.0f * M_PI) * 4.0f) % 4;
}

void DialSet7::handleFilterCutoff(float angle, SceneObjects* scene) {
    filterCutoff = 100.0f + (angle + M_PI) * 5000.0f;
}

void DialSet7::handleResonance(float angle, SceneObjects* scene) {
    resonance = (angle + M_PI) / (2.0f * M_PI);
}

void DialSet7::handleDistortion(float angle, SceneObjects* scene) {
    distortion = (angle + M_PI) / (2.0f * M_PI);
}

void DialSet7::handleDelayTime(float angle, SceneObjects* scene) {
    delayTime = 0.1f + (angle + M_PI) * 2.0f;
}

void DialSet7::handleChorusDepth(float angle, SceneObjects* scene) {
    chorusDepth = (angle + M_PI) / (2.0f * M_PI);
}

void DialSet7::updateSynthesizerVisualization(SceneObjects* scene) {
    synthTime += 0.02f;
    
    for (int i = 0; i < 8; i++) {
        if (scene->scatterOffsets[i]) {
            waveformPhases[i] += 0.1f;
            
            float waveHeight = 0.0f;
            switch (waveformType) {
                case 0: // Sine
                    waveHeight = sinf(waveformPhases[i]);
                    break;
                case 1: // Saw
                    waveHeight = 2.0f * (waveformPhases[i] / (2.0f * M_PI) - floorf(waveformPhases[i] / (2.0f * M_PI) + 0.5f));
                    break;
                case 2: // Square
                    waveHeight = (sinf(waveformPhases[i]) > 0.0f) ? 1.0f : -1.0f;
                    break;
                case 3: // Triangle
                    waveHeight = (2.0f / M_PI) * asinf(sinf(waveformPhases[i]));
                    break;
            }
            
            vuLevels[i] = 0.5f + waveHeight * 0.5f;
            
            scene->scatterOffsets[i]->translation.setValue(
                cosf(synthTime + i) * 2.0f,
                waveHeight * 2.0f,
                sinf(synthTime * 0.7f + i) * 0.5f
            );
        }
    }
    
    updateNeonEffects(scene);
}

void DialSet7::updateNeonEffects(SceneObjects* scene) {
    for (int i = 0; i < 8; i++) {
        if (scene->dialMaterials[i]) {
            float vuLevel = vuLevels[i];
            float r = 0.8f + 0.2f * vuLevel;
            float g = 0.2f + 0.3f * vuLevel;
            float b = 0.9f + 0.1f * vuLevel;
            
            scene->dialMaterials[i]->diffuseColor.setValue(r, g, b);
            scene->dialMaterials[i]->emissiveColor.setValue(r * 0.8f, g * 0.6f, b * 0.9f);
        }
    }
}

// Factory function
DialHandler* createDialSet7() {
    return new DialSet7();
} 
