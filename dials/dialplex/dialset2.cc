#include "dialhandler.h"
#include <Inventor/nodes/SoMaterial.h>
#include <math.h>

// Physics simulation state variables
static float gravity = 9.8f;
static float springConstant = 2.0f;
static float damping = 0.95f;
static float mass = 1.0f;
static float airResistance = 0.02f;
static float pendulumLength = 1.0f;
static float magneticField = 0.0f;
static float timeStep = 0.016f;

// Physics state arrays
static SbVec3f velocities[8];
static SbVec3f forces[8];
static float pendulumAngles[8];
static float pendulumVelocities[8];
static bool isColliding[8];

class DialSet2 : public DialHandler {
public:
    void init(SceneObjects* scene, DialState* state);
    void handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state);
    void cleanup(SceneObjects* scene, DialState* state);
    const char* getDescription();
    const char* getThemeDescription();
    const char* getDialDescription(int dialIndex);

private:
    void handleGravity(float angle, SceneObjects* scene);
    void handleSpringConstant(float angle, SceneObjects* scene);
    void handleDamping(float angle, SceneObjects* scene);
    void handleMass(float angle, SceneObjects* scene);
    void handleAirResistance(float angle, SceneObjects* scene);
    void handlePendulumLength(float angle, SceneObjects* scene);
    void handleMagneticField(float angle, SceneObjects* scene);
    void handleTimeStep(float angle, SceneObjects* scene);
    void updatePhysics(SceneObjects* scene);
    void updateVisualFeedback(int index, SceneObjects* scene);
};

const char* DialSet2::getDescription() {
    return "PHYSICS SIMULATION - Real-time gravity, springs, and particle dynamics";
}

const char* DialSet2::getThemeDescription() {
    return "PHYSICS SIMULATION - Real-time gravity, springs, and particle dynamics";
}

const char* DialSet2::getDialDescription(int dialIndex) {
    static const char* descriptions[8] = {
        "DIAL 0: Gravity Strength - Force pulling objects downward",
        "DIAL 1: Spring Constant - Tension of connecting springs",
        "DIAL 2: Damping Factor - Energy loss over time",
        "DIAL 3: Object Mass - Weight affecting motion dynamics",
        "DIAL 4: Air Resistance - Friction slowing movement",
        "DIAL 5: Pendulum Length - Swing radius and period",
        "DIAL 6: Magnetic Field - Circular force effects",
        "DIAL 7: Time Step - Simulation speed and precision"
    };
    if (dialIndex >= 0 && dialIndex < 8) {
        return descriptions[dialIndex];
    }
    return "Unknown dial";
}

void DialSet2::init(SceneObjects* scene, DialState* state) {
    for (int i = 0; i < 8; i++) {
        velocities[i] = SbVec3f(0, 0, 0);
        forces[i] = SbVec3f(0, 0, 0);
        pendulumAngles[i] = 0.0f;
        pendulumVelocities[i] = 0.0f;
        isColliding[i] = false;
        
        if (scene->dialMaterials[i]) {
            scene->dialMaterials[i]->diffuseColor.setValue(0.3f, 0.4f, 0.2f);
            scene->dialMaterials[i]->emissiveColor.setValue(0.05f, 0.08f, 0.03f);
            scene->dialMaterials[i]->specularColor.setValue(0.5f, 0.6f, 0.4f);
            scene->dialMaterials[i]->shininess.setValue(0.7f);
        }
    }
    
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(0.05f, 0.08f, 0.05f));
    }
}

void DialSet2::cleanup(SceneObjects* scene, DialState* state) {
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

void DialSet2::handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state) {
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
            handleGravity(state->dialAngles[0], scene);
            break;
        case 1:
            handleSpringConstant(state->dialAngles[1], scene);
            break;
        case 2:
            handleDamping(state->dialAngles[2], scene);
            break;
        case 3:
            handleMass(state->dialAngles[3], scene);
            break;
        case 4:
            handleAirResistance(state->dialAngles[4], scene);
            break;
        case 5:
            handlePendulumLength(state->dialAngles[5], scene);
            break;
        case 6:
            handleMagneticField(state->dialAngles[6], scene);
            break;
        case 7:
            handleTimeStep(state->dialAngles[7], scene);
            break;
    }

    updatePhysics(scene);
    
    if (scene->dialTransforms[dialIndex]) {
        scene->dialTransforms[dialIndex]->rotation.setValue(SbVec3f(0, 1, 0), state->dialAngles[dialIndex]);
    }
}

void DialSet2::handleGravity(float angle, SceneObjects* scene) {
    gravity = 1.0f + (angle + M_PI) * 10.0f;
}

void DialSet2::handleSpringConstant(float angle, SceneObjects* scene) {
    springConstant = 0.1f + (angle + M_PI) * 3.0f;
}

void DialSet2::handleDamping(float angle, SceneObjects* scene) {
    damping = 0.5f + (angle + M_PI) / (4.0f * M_PI);
}

void DialSet2::handleMass(float angle, SceneObjects* scene) {
    mass = 0.1f + (angle + M_PI) * 2.0f;
}

void DialSet2::handleAirResistance(float angle, SceneObjects* scene) {
    airResistance = (angle + M_PI) / (10.0f * M_PI);
}

void DialSet2::handlePendulumLength(float angle, SceneObjects* scene) {
    pendulumLength = 0.5f + (angle + M_PI) * 2.0f;
}

void DialSet2::handleMagneticField(float angle, SceneObjects* scene) {
    magneticField = (angle + M_PI) / (2.0f * M_PI);
}

void DialSet2::handleTimeStep(float angle, SceneObjects* scene) {
    timeStep = 0.001f + (angle + M_PI) / (100.0f * M_PI);
}

void DialSet2::updatePhysics(SceneObjects* scene) {
    for (int i = 0; i < 8; i++) {
        if (scene->scatterOffsets[i]) {
            SbVec3f currentPos = scene->scatterOffsets[i]->translation.getValue();
            
            forces[i] = SbVec3f(0, -gravity * mass, 0);
            forces[i] += currentPos * (-springConstant);
            forces[i] -= velocities[i] * airResistance;
            
            if (magneticField > 0.01f) {
                float mag = magneticField * 5.0f;
                forces[i] += SbVec3f(-currentPos[1] * mag, currentPos[0] * mag, 0);
            }
            
            velocities[i] += forces[i] * (timeStep / mass);
            velocities[i] *= damping;
            
            SbVec3f newPos = currentPos + velocities[i] * timeStep;
            scene->scatterOffsets[i]->translation.setValue(newPos);
            
            float pendulumForce = -sinf(pendulumAngles[i]) * gravity / pendulumLength;
            pendulumVelocities[i] += pendulumForce * timeStep;
            pendulumVelocities[i] *= 0.999f;
            pendulumAngles[i] += pendulumVelocities[i] * timeStep;
            
            updateVisualFeedback(i, scene);
        }
    }
}

void DialSet2::updateVisualFeedback(int index, SceneObjects* scene) {
    if (scene->dialMaterials[index]) {
        float kineticEnergy = velocities[index].length();
        float normalizedEnergy = kineticEnergy / 10.0f;
        if (normalizedEnergy > 1.0f) normalizedEnergy = 1.0f;
        
        float r = 0.2f + normalizedEnergy * 0.8f;
        float g = 0.25f + normalizedEnergy * 0.3f;
        float b = 0.35f - normalizedEnergy * 0.2f;
        
        scene->dialMaterials[index]->diffuseColor.setValue(r, g, b);
        scene->dialMaterials[index]->emissiveColor.setValue(
            normalizedEnergy * 0.1f,
            normalizedEnergy * 0.05f,
            0.0f
        );
    }
    
    if (scene->dialTransforms[index]) {
        scene->dialTransforms[index]->rotation.setValue(
            SbVec3f(0, 0, 1), pendulumAngles[index] * 0.3f
        );
    }
}

// Factory function
DialHandler* createDialSet2() {
    return new DialSet2();
} 
