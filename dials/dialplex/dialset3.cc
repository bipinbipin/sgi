#include "dialhandler.h"
#include <Inventor/nodes/SoMaterial.h>
#include <math.h>

// Particle system parameters
static float emissionRate = 50.0f;
static float particleLifetime = 3.0f;
static float windStrength = 0.0f;
static float attractorStrength = 1.0f;
static float flockingRadius = 2.0f;
static float explosionForce = 5.0f;
static float turbulence = 0.5f;
static float colorSpeed = 1.0f;
static float timeAccum = 0.0f;

// Particle state arrays (simulating multiple particles per dial)
static SbVec3f particleVels[8];
static SbVec3f particleTargets[8];
static float particleAges[8];
static float burstTimers[8];
static bool isBursting[8];
static SbVec3f attractorPos;

class DialSet3 : public DialHandler {
public:
    void init(SceneObjects* scene, DialState* state);
    void handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state);
    void cleanup(SceneObjects* scene, DialState* state);
    const char* getDescription();
    const char* getThemeDescription();
    const char* getDialDescription(int dialIndex);

private:
    void handleEmissionRate(float angle, SceneObjects* scene);
    void handleParticleLifetime(float angle, SceneObjects* scene);
    void handleWindStrength(float angle, SceneObjects* scene);
    void handleAttractorForce(float angle, SceneObjects* scene);
    void handleFlockingRadius(float angle, SceneObjects* scene);
    void handleExplosionForce(float angle, SceneObjects* scene);
    void handleTurbulence(float angle, SceneObjects* scene);
    void handleColorSpeed(float angle, SceneObjects* scene);
    void updateParticleSystem(SceneObjects* scene);
    void updateColorCycling(SceneObjects* scene);
};

const char* DialSet3::getDescription() {
    return "PARTICLE SYSTEM - Fireworks, swarms, flocking, and fluid dynamics";
}

const char* DialSet3::getThemeDescription() {
    return "PARTICLE SYSTEM - Fireworks, swarms, flocking, and fluid dynamics";
}

const char* DialSet3::getDialDescription(int dialIndex) {
    static const char* descriptions[8] = {
        "DIAL 0: Emission Rate - Particles spawned per second",
        "DIAL 1: Particle Lifetime - How long particles exist",
        "DIAL 2: Wind Strength - External force direction",
        "DIAL 3: Attractor Force - Central gravitational pull",
        "DIAL 4: Flocking Radius - Neighbor influence distance",
        "DIAL 5: Explosion Force - Burst intensity on rapid changes",
        "DIAL 6: Turbulence - Random chaotic movement",
        "DIAL 7: Color Speed - Rate of hue cycling animation"
    };
    if (dialIndex >= 0 && dialIndex < 8) {
        return descriptions[dialIndex];
    }
    return "Unknown dial";
}

void DialSet3::init(SceneObjects* scene, DialState* state) {
    attractorPos = SbVec3f(0, 0, 0);
    timeAccum = 0.0f;
    
    for (int i = 0; i < 8; i++) {
        particleVels[i] = SbVec3f(0, 0, 0);
        particleAges[i] = (float)i * 0.5f;
        burstTimers[i] = 0.0f;
        isBursting[i] = false;
        
        float angle = i * (2.0f * M_PI / 8.0f);
        particleTargets[i] = SbVec3f(cosf(angle) * 2.0f, sinf(angle) * 2.0f, 0);
        
        if (scene->dialMaterials[i]) {
            float hue = (float)i / 8.0f;
            scene->dialMaterials[i]->diffuseColor.setValue(
                0.5f + 0.5f * sinf(hue * 2 * M_PI),
                0.5f + 0.5f * sinf(hue * 2 * M_PI + 2.094f),
                0.5f + 0.5f * sinf(hue * 2 * M_PI + 4.188f)
            );
            scene->dialMaterials[i]->emissiveColor.setValue(0.3f, 0.3f, 0.3f);
            scene->dialMaterials[i]->specularColor.setValue(1.0f, 1.0f, 1.0f);
            scene->dialMaterials[i]->shininess.setValue(0.9f);
        }
    }
    
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(0.02f, 0.02f, 0.08f));
    }
}

void DialSet3::cleanup(SceneObjects* scene, DialState* state) {
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

void DialSet3::handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state) {
    if (state->firstDialEvent[dialIndex]) {
        state->lastDialValues[dialIndex] = rawValue;
        state->firstDialEvent[dialIndex] = false;
        return;
    }

    int delta = rawValue - state->lastDialValues[dialIndex];
    state->lastDialValues[dialIndex] = rawValue;
    float deltaAngle = delta * (M_PI / 128.0f);
    state->dialAngles[dialIndex] += deltaAngle;
    
    timeAccum += 0.02f;

    switch (dialIndex) {
        case 0:
            handleEmissionRate(state->dialAngles[0], scene);
            break;
        case 1:
            handleParticleLifetime(state->dialAngles[1], scene);
            break;
        case 2:
            handleWindStrength(state->dialAngles[2], scene);
            break;
        case 3:
            handleAttractorForce(state->dialAngles[3], scene);
            break;
        case 4:
            handleFlockingRadius(state->dialAngles[4], scene);
            break;
        case 5:
            handleExplosionForce(state->dialAngles[5], scene);
            break;
        case 6:
            handleTurbulence(state->dialAngles[6], scene);
            break;
        case 7:
            handleColorSpeed(state->dialAngles[7], scene);
            break;
    }

    updateParticleSystem(scene);
    
    if (scene->dialTransforms[dialIndex]) {
        float swirl = state->dialAngles[dialIndex] + sinf(timeAccum + dialIndex) * turbulence;
        scene->dialTransforms[dialIndex]->rotation.setValue(SbVec3f(0, 1, 0), swirl);
    }
}

void DialSet3::handleEmissionRate(float angle, SceneObjects* scene) {
    emissionRate = 10.0f + (angle + M_PI) * 40.0f;
}

void DialSet3::handleParticleLifetime(float angle, SceneObjects* scene) {
    particleLifetime = 1.0f + (angle + M_PI) * 3.0f;
}

void DialSet3::handleWindStrength(float angle, SceneObjects* scene) {
    windStrength = angle * 2.0f;
}

void DialSet3::handleAttractorForce(float angle, SceneObjects* scene) {
    attractorStrength = (angle + M_PI) * 2.0f;
    attractorPos.setValue(
        sinf(timeAccum * 0.5f) * 3.0f,
        sinf(timeAccum) * 2.0f,
        cosf(timeAccum * 0.3f) * 1.0f
    );
}

void DialSet3::handleFlockingRadius(float angle, SceneObjects* scene) {
    flockingRadius = 0.5f + (angle + M_PI) * 2.0f;
}

void DialSet3::handleExplosionForce(float angle, SceneObjects* scene) {
    explosionForce = 1.0f + (angle + M_PI) * 8.0f;
}

void DialSet3::handleTurbulence(float angle, SceneObjects* scene) {
    turbulence = (angle + M_PI) * 0.5f;
}

void DialSet3::handleColorSpeed(float angle, SceneObjects* scene) {
    colorSpeed = 0.1f + (angle + M_PI) * 2.0f;
}

void DialSet3::updateParticleSystem(SceneObjects* scene) {
    for (int i = 0; i < 8; i++) {
        if (scene->scatterOffsets[i]) {
            SbVec3f currentPos = scene->scatterOffsets[i]->translation.getValue();
            
            particleAges[i] += 0.02f;
            if (particleAges[i] > particleLifetime) {
                particleAges[i] = 0.0f;
            }
            
            SbVec3f totalForce(0, 0, 0);
            
            SbVec3f toAttractor = attractorPos - currentPos;
            float attractorDist = toAttractor.length();
            if (attractorDist > 0.1f) {
                totalForce += toAttractor * (attractorStrength / (attractorDist * attractorDist));
            }
            
            SbVec3f windForce(windStrength, sinf(timeAccum + i) * windStrength * 0.5f, 0);
            totalForce += windForce;
            
            SbVec3f turbForce(
                sinf(timeAccum * 3.0f + i * 0.7f) * turbulence,
                cosf(timeAccum * 2.5f + i * 1.1f) * turbulence,
                sinf(timeAccum * 1.8f + i * 1.3f) * turbulence * 0.5f
            );
            totalForce += turbForce;
            
            particleVels[i] += totalForce * 0.01f;
            particleVels[i] *= 0.98f;
            
            SbVec3f newPos = currentPos + particleVels[i] * 0.02f;
            scene->scatterOffsets[i]->translation.setValue(newPos);
            
            updateColorCycling(scene);
        }
    }
}

void DialSet3::updateColorCycling(SceneObjects* scene) {
    for (int i = 0; i < 8; i++) {
        if (scene->dialMaterials[i]) {
            float hue = fmodf(timeAccum * colorSpeed + i * 0.125f, 1.0f);
            float r = 0.5f + 0.5f * sinf(hue * 2 * M_PI);
            float g = 0.5f + 0.5f * sinf(hue * 2 * M_PI + 2.094f);
            float b = 0.5f + 0.5f * sinf(hue * 2 * M_PI + 4.188f);
            
            scene->dialMaterials[i]->diffuseColor.setValue(r, g, b);
            scene->dialMaterials[i]->emissiveColor.setValue(r * 0.3f, g * 0.3f, b * 0.3f);
        }
    }
}

// Factory function
DialHandler* createDialSet3() {
    return new DialSet3();
} 
