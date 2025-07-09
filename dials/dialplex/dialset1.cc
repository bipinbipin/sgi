#include "dialhandler.h"
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/SbColor.h>
#include <math.h>

class DialSet1 : public DialHandler {
public:
    void init(SceneObjects* scene, DialState* state);
    void handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state);
    void cleanup(SceneObjects* scene, DialState* state);
    const char* getDescription();
    const char* getThemeDescription();
    const char* getDialDescription(int dialIndex);

private:
    void handleOscillationSpeed(float angle, SceneObjects* scene);
    void handleWaveAmplitude(float angle, SceneObjects* scene);
    void handleColorTemperature(float angle, SceneObjects* scene);
    void handleFrequencyModulation(float angle, SceneObjects* scene);
    void handlePulseIntensity(float angle, SceneObjects* scene);
    void handleMetallicShift(float angle, SceneObjects* scene);
    void handleRhythmicPulsing(float angle, SceneObjects* scene);
    void handleHarmonicResonance(float angle, SceneObjects* scene);
    void updateBackground(SceneObjects* scene);
    
    // Musical synthesis parameters
    float oscillationSpeed;
    float waveAmplitude;
    float colorTemperature;
    float frequencyMod;
    float pulseIntensity;
    float metallicShift;
    float rhythmicPhase;
    float harmonicResonance;
    float timeAccumulator;
};

const char* DialSet1::getDescription() {
    return "HARMONIC SYNTHESIS - Musical oscillations and frequency modulation";
}

const char* DialSet1::getThemeDescription() {
    return "HARMONIC SYNTHESIS - Musical oscillations and frequency modulation";
}

const char* DialSet1::getDialDescription(int dialIndex) {
    static const char* descriptions[8] = {
        "DIAL 0: Oscillation Speed - Control harmonic wave frequency",
        "DIAL 1: Wave Amplitude - Height of cylindrical oscillations",
        "DIAL 2: Color Temperature - Shift from cool to warm tones",
        "DIAL 3: Frequency Modulation - Complex wave interference",
        "DIAL 4: Pulse Intensity - Scene zoom and rotation bursts", 
        "DIAL 5: Material Shifts - Plastic to metallic transitions",
        "DIAL 6: Rhythmic Pulsing - Scene breathing and timing",
        "DIAL 7: Harmonic Resonance - Interference pattern complexity"
    };
    if (dialIndex >= 0 && dialIndex < 8) {
        return descriptions[dialIndex];
    }
    return "Unknown dial";
}

void DialSet1::init(SceneObjects* scene, DialState* state) {
    // Reset all musical parameters
    oscillationSpeed = 1.0f;
    waveAmplitude = 1.0f;
    colorTemperature = 0.5f;
    frequencyMod = 1.0f;
    pulseIntensity = 0.0f;
    metallicShift = 0.0f;
    rhythmicPhase = 0.0f;
    harmonicResonance = 0.0f;
    timeAccumulator = 0.0f;
    
    // Set initial warm color scheme
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(0.05f, 0.02f, 0.1f));
    }
}

void DialSet1::cleanup(SceneObjects* scene, DialState* state) {
    // Reset to neutral state
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));
    }
    
    // Reset materials to default
    for (int i = 0; i < 8; i++) {
        if (scene->dialMaterials[i]) {
            scene->dialMaterials[i]->diffuseColor.setValue(0.2f, 0.25f, 0.35f);
            scene->dialMaterials[i]->specularColor.setValue(0.4f, 0.4f, 0.4f);
            scene->dialMaterials[i]->shininess.setValue(0.6f);
        }
    }
}

void DialSet1::handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state) {
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

    // Update time accumulator for oscillations
    timeAccumulator += 0.1f;

    switch (dialIndex) {
        case 0:
            handleOscillationSpeed(state->dialAngles[0], scene);
            break;
        case 1:
            handleWaveAmplitude(state->dialAngles[1], scene);
            break;
        case 2:
            handleColorTemperature(state->dialAngles[2], scene);
            break;
        case 3:
            handleFrequencyModulation(state->dialAngles[3], scene);
            break;
        case 4:
            handlePulseIntensity(state->dialAngles[4], scene);
            break;
        case 5:
            handleMetallicShift(state->dialAngles[5], scene);
            break;
        case 6:
            handleRhythmicPulsing(state->dialAngles[6], scene);
            break;
        case 7:
            handleHarmonicResonance(state->dialAngles[7], scene);
            break;
    }

    // Apply oscillating rotation to each dial based on current settings
    if (scene->dialTransforms[dialIndex]) {
        float oscillation = waveAmplitude * sinf(timeAccumulator * oscillationSpeed + dialIndex * frequencyMod);
        float totalRotation = state->dialAngles[dialIndex] + oscillation * 0.3f;
        scene->dialTransforms[dialIndex]->rotation.setValue(SbVec3f(0, 1, 0), totalRotation);
    }
}

void DialSet1::handleOscillationSpeed(float angle, SceneObjects* scene) {
    oscillationSpeed = 0.1f + (angle + M_PI) * 2.0f; // 0.1 to 4.0 range
    updateBackground(scene);
}

void DialSet1::handleWaveAmplitude(float angle, SceneObjects* scene) {
    waveAmplitude = (angle + M_PI) * 0.5f; // 0 to ~3.14 range
    
    // Apply wave-based cylinder heights
    for (int i = 0; i < 8; i++) {
        if (scene->dialCylinders[i] && scene->cylinderOffsets[i]) {
            float wave = sinf(timeAccumulator * oscillationSpeed + i * 0.8f) * waveAmplitude * 0.1f;
            float newHeight = 0.2f + wave;
            if (newHeight < 0.05f) newHeight = 0.05f;
            
            scene->dialCylinders[i]->height = newHeight;
            scene->cylinderOffsets[i]->translation.setValue(0, newHeight / 2.0f, 0);
        }
    }
}

void DialSet1::handleColorTemperature(float angle, SceneObjects* scene) {
    colorTemperature = (angle + M_PI) / (2.0f * M_PI); // 0 to 1 range
    updateBackground(scene);
}

void DialSet1::handleFrequencyModulation(float angle, SceneObjects* scene) {
    frequencyMod = 0.1f + (angle + M_PI) * 0.5f; // 0.1 to 1.6 range
    
    // Apply frequency-based scatter effects
    for (int i = 0; i < 8; i++) {
        if (scene->scatterOffsets[i]) {
            float freq = frequencyMod + i * 0.2f;
            float x = 0.3f * sinf(timeAccumulator * freq);
            float y = 0.3f * cosf(timeAccumulator * freq * 1.3f);
            float z = 0.2f * sinf(timeAccumulator * freq * 0.7f);
            
            scene->scatterOffsets[i]->translation.setValue(x, y, z);
        }
    }
}

void DialSet1::handlePulseIntensity(float angle, SceneObjects* scene) {
    pulseIntensity = (angle + M_PI) / (2.0f * M_PI); // 0 to 1 range
    
    // Create pulsing scene transformations
    float pulse = pulseIntensity * (1.0f + 0.5f * sinf(timeAccumulator * oscillationSpeed * 2.0f));
    
    if (scene->sceneZoom) {
        float zoomPulse = -5.0f + pulse * 2.0f;
        scene->sceneZoom->translation.setValue(0, 0, zoomPulse);
    }
    
    if (scene->sceneRotY) {
        float rotPulse = pulse * sinf(timeAccumulator * oscillationSpeed * 0.5f) * 0.1f;
        scene->sceneRotY->rotation.setValue(SbVec3f(0, 1, 0), rotPulse);
    }
}

void DialSet1::handleMetallicShift(float angle, SceneObjects* scene) {
    metallicShift = (angle + M_PI) / (2.0f * M_PI); // 0 to 1 range
    
    // Shift materials between plastic and metallic
    for (int i = 0; i < 8; i++) {
        if (scene->dialMaterials[i]) {
            // Base colors shift from warm plastic to cool metal
            float r = 0.6f * (1.0f - metallicShift) + 0.3f * metallicShift;
            float g = 0.4f * (1.0f - metallicShift) + 0.35f * metallicShift;
            float b = 0.2f * (1.0f - metallicShift) + 0.4f * metallicShift;
            
            // Add temperature variation
            r += colorTemperature * 0.2f;
            b += (1.0f - colorTemperature) * 0.2f;
            
            scene->dialMaterials[i]->diffuseColor.setValue(r, g, b);
            scene->dialMaterials[i]->specularColor.setValue(metallicShift, metallicShift, metallicShift);
            scene->dialMaterials[i]->shininess.setValue(0.2f + metallicShift * 0.8f);
        }
    }
}

void DialSet1::handleRhythmicPulsing(float angle, SceneObjects* scene) {
    rhythmicPhase = angle;
    
    // Create rhythmic transformations
    float rhythm = sinf(timeAccumulator * oscillationSpeed + rhythmicPhase) * 0.5f + 0.5f;
    
    if (scene->sceneRotX) {
        scene->sceneRotX->rotation.setValue(SbVec3f(1, 0, 0), rhythm * 0.2f);
    }
    
    if (scene->sceneRotZ) {
        scene->sceneRotZ->rotation.setValue(SbVec3f(0, 0, 1), sinf(timeAccumulator * oscillationSpeed * 0.7f + rhythmicPhase) * 0.1f);
    }
}

void DialSet1::handleHarmonicResonance(float angle, SceneObjects* scene) {
    harmonicResonance = (angle + M_PI) / (2.0f * M_PI); // 0 to 1 range
    
    // Create complex harmonic interference patterns
    for (int i = 0; i < 8; i++) {
        if (scene->scatterOffsets[i]) {
            float harmonics = harmonicResonance * 5.0f; // 0 to 5 harmonics
            float interference = 0.0f;
            
            // Calculate harmonic interference
            for (int h = 1; h <= (int)harmonics + 1; h++) {
                interference += sinf(timeAccumulator * oscillationSpeed * h + i * M_PI / 4.0f) / h;
            }
            
            float intensity = harmonicResonance * 0.4f;
            float x = interference * intensity * cosf(i * M_PI / 4.0f);
            float y = interference * intensity * sinf(i * M_PI / 4.0f);
            float z = interference * intensity * 0.5f;
            
            SbVec3f current = scene->scatterOffsets[i]->translation.getValue();
            scene->scatterOffsets[i]->translation.setValue(current.getValue()[0] + x * 0.1f, current.getValue()[1] + y * 0.1f, current.getValue()[2] + z * 0.1f);
        }
    }
}

void DialSet1::updateBackground(SceneObjects* scene) {
    if (!scene->globalViewer) return;
    
    // Create color based on temperature and oscillation
    float r = 0.05f + colorTemperature * 0.3f + 0.1f * sinf(timeAccumulator * oscillationSpeed);
    float g = 0.02f + (1.0f - colorTemperature) * 0.15f + 0.05f * sinf(timeAccumulator * oscillationSpeed * 1.3f);
    float b = 0.1f + (1.0f - colorTemperature) * 0.4f + 0.2f * sinf(timeAccumulator * oscillationSpeed * 0.7f);
    
    // Clamp colors
    r = (r < 0.0f) ? 0.0f : ((r > 1.0f) ? 1.0f : r);
    g = (g < 0.0f) ? 0.0f : ((g > 1.0f) ? 1.0f : g);
    b = (b < 0.0f) ? 0.0f : ((b > 1.0f) ? 1.0f : b);
    
    scene->globalViewer->setBackgroundColor(SbColor(r, g, b));
    scene->globalViewer->render();
}

// Factory function
DialHandler* createDialSet1() {
    return new DialSet1();
} 
