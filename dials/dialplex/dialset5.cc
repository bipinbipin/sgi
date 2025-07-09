#include "dialhandler.h"
#include <Inventor/nodes/SoMaterial.h>
#include <math.h>

// Space simulation parameters
static float centralMass = 1000.0f;
static float orbitalSpeed = 1.0f;
static float gravityStrength = 1.0f;
static float stellarWind = 0.0f;
static float cosmicTime = 0.0f;
static float blackHoleStrength = 0.0f;
static float supernovaForce = 0.0f;
static float warpFactor = 0.0f;
static SbVec3f centralBody;

// Orbital state for each celestial body
static float orbitalRadii[8];
static float orbitalAngles[8];
static float orbitalVelocities[8];
static float planetMasses[8];
static SbVec3f planetVelocities[8];
static bool isInAccretion[8];

class DialSet5 : public DialHandler {
public:
    void init(SceneObjects* scene, DialState* state);
    void handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state);
    void cleanup(SceneObjects* scene, DialState* state);
    const char* getDescription();
    const char* getThemeDescription();
    const char* getDialDescription(int dialIndex);

private:
    void handleGravityWells(float angle, SceneObjects* scene);
    void handleOrbitalSpeed(float angle, SceneObjects* scene);
    void handleBlackHoleStrength(float angle, SceneObjects* scene);
    void handleSupernovaForce(float angle, SceneObjects* scene);
    void handleWarpField(float angle, SceneObjects* scene);
    void handleStarDensity(float angle, SceneObjects* scene);
    void handleCosmicTime(float angle, SceneObjects* scene);
    void handleDarkMatter(float angle, SceneObjects* scene);
    void updateSpaceSimulation(SceneObjects* scene);
    void updateCosmicEffects(SceneObjects* scene);
};

const char* DialSet5::getDescription() {
    return "SPACE SIMULATION - Orbital mechanics, black holes, and cosmic phenomena";
}

const char* DialSet5::getThemeDescription() {
    return "SPACE SIMULATION - Orbital mechanics, black holes, and cosmic phenomena";
}

const char* DialSet5::getDialDescription(int dialIndex) {
    static const char* descriptions[8] = {
        "DIAL 0: Gravity Wells - Strength of gravitational attraction",
        "DIAL 1: Orbital Speed - Velocity of celestial bodies",
        "DIAL 2: Black Hole Strength - Event horizon intensity",
        "DIAL 3: Supernova Force - Explosive stellar death effects",
        "DIAL 4: Warp Field - Space-time distortion effects",
        "DIAL 5: Star Density - Number of stellar objects",
        "DIAL 6: Cosmic Time - Rate of universal expansion",
        "DIAL 7: Dark Matter - Invisible matter influence"
    };
    if (dialIndex >= 0 && dialIndex < 8) {
        return descriptions[dialIndex];
    }
    return "Unknown dial";
}

void DialSet5::init(SceneObjects* scene, DialState* state) {
    for (int i = 0; i < 8; i++) {
        orbitalRadii[i] = 1.0f + i * 0.5f;
        orbitalAngles[i] = i * (2.0f * M_PI / 8.0f);
        orbitalVelocities[i] = 1.0f;
        planetMasses[i] = 10.0f + i * 5.0f;
        planetVelocities[i] = SbVec3f(0, 0, 0);
        isInAccretion[i] = false;
        
        if (scene->dialMaterials[i]) {
            switch (i % 4) {
                case 0:
                    scene->dialMaterials[i]->diffuseColor.setValue(0.7f, 0.5f, 0.3f);
                    break;
                case 1:
                    scene->dialMaterials[i]->diffuseColor.setValue(0.8f, 0.7f, 0.4f);
                    break;
                case 2:
                    scene->dialMaterials[i]->diffuseColor.setValue(0.6f, 0.8f, 1.0f);
                    break;
                case 3:
                    scene->dialMaterials[i]->diffuseColor.setValue(1.0f, 0.4f, 0.2f);
                    break;
            }
            scene->dialMaterials[i]->specularColor.setValue(0.8f, 0.8f, 0.8f);
            scene->dialMaterials[i]->shininess.setValue(0.7f);
            scene->dialMaterials[i]->emissiveColor.setValue(0.05f, 0.05f, 0.05f);
        }
    }
    
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(0.01f, 0.01f, 0.03f));
    }
}

void DialSet5::cleanup(SceneObjects* scene, DialState* state) {
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

void DialSet5::handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state) {
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
            handleGravityWells(state->dialAngles[0], scene);
            break;
        case 1:
            handleOrbitalSpeed(state->dialAngles[1], scene);
            break;
        case 2:
            handleBlackHoleStrength(state->dialAngles[2], scene);
            break;
        case 3:
            handleSupernovaForce(state->dialAngles[3], scene);
            break;
        case 4:
            handleWarpField(state->dialAngles[4], scene);
            break;
        case 5:
            handleStarDensity(state->dialAngles[5], scene);
            break;
        case 6:
            handleCosmicTime(state->dialAngles[6], scene);
            break;
        case 7:
            handleDarkMatter(state->dialAngles[7], scene);
            break;
    }

    updateSpaceSimulation(scene);
    
    if (scene->dialTransforms[dialIndex]) {
        float cosmicRotation = state->dialAngles[dialIndex] + orbitalAngles[dialIndex];
        scene->dialTransforms[dialIndex]->rotation.setValue(SbVec3f(0, 1, 0), cosmicRotation);
    }
}

void DialSet5::handleGravityWells(float angle, SceneObjects* scene) {
    gravityStrength = 0.5f + (angle + M_PI) * 5.0f;
}

void DialSet5::handleOrbitalSpeed(float angle, SceneObjects* scene) {
    orbitalSpeed = 0.1f + (angle + M_PI) * 2.0f;
}

void DialSet5::handleBlackHoleStrength(float angle, SceneObjects* scene) {
    blackHoleStrength = (angle + M_PI) * 10.0f;
}

void DialSet5::handleSupernovaForce(float angle, SceneObjects* scene) {
    supernovaForce = 1.0f + (angle + M_PI) * 15.0f;
}

void DialSet5::handleWarpField(float angle, SceneObjects* scene) {
    warpFactor = (angle + M_PI) / (2.0f * M_PI);
}

void DialSet5::handleStarDensity(float angle, SceneObjects* scene) {
    stellarWind = angle * 3.0f;
}

void DialSet5::handleCosmicTime(float angle, SceneObjects* scene) {
    cosmicTime = 0.1f + (angle + M_PI) * 3.0f;
}

void DialSet5::handleDarkMatter(float angle, SceneObjects* scene) {
    centralMass = 500.0f + (angle + M_PI) * 2000.0f;
}

void DialSet5::updateSpaceSimulation(SceneObjects* scene) {
    cosmicTime += 0.02f;
    
    for (int i = 0; i < 8; i++) {
        if (scene->scatterOffsets[i]) {
            float orbitRadius = 2.0f + (float)i * 0.5f;
            float orbitAngle = cosmicTime * orbitalSpeed + i * 0.785f;
            
            SbVec3f orbitalPos;
            orbitalPos.setValue(
                cosf(orbitAngle) * orbitRadius,
                sinf(orbitAngle) * orbitRadius * 0.7f,
                sinf(orbitAngle * 0.5f) * 0.3f
            );
            
            SbVec3f blackHoleEffect(0, 0, 0);
            if (blackHoleStrength > 0.1f) {
                float dist = orbitalPos.length();
                if (dist > 0.1f) {
                    blackHoleEffect = -orbitalPos * (blackHoleStrength / (dist * dist));
                }
            }
            
            SbVec3f finalPos = orbitalPos + blackHoleEffect;
            scene->scatterOffsets[i]->translation.setValue(finalPos);
        }
    }
    
    updateCosmicEffects(scene);
}

void DialSet5::updateCosmicEffects(SceneObjects* scene) {
    for (int i = 0; i < 8; i++) {
        if (scene->dialMaterials[i]) {
            float intensity = 0.5f + 0.5f * sinf(cosmicTime * 2.0f + i * 0.8f);
            float r = 0.8f + 0.2f * sinf(cosmicTime + i);
            float g = 0.6f + 0.4f * sinf(cosmicTime * 1.3f + i);
            float b = 0.4f + 0.6f * sinf(cosmicTime * 0.7f + i);
            
            scene->dialMaterials[i]->diffuseColor.setValue(r * intensity, g * intensity, b * intensity);
            scene->dialMaterials[i]->emissiveColor.setValue(r * 0.3f, g * 0.2f, b * 0.1f);
        }
    }
}

// Factory function
DialHandler* createDialSet5() {
    return new DialSet5();
} 
