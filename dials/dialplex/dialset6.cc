#include "dialhandler.h"
#include <Inventor/nodes/SoMaterial.h>
#include <math.h>

// All variables used in the organic growth system
static float growthRate = 1.0f;
static float branchingAngle = 25.0f;
static float branchingProbability = 0.3f;
static float leafDensity = 0.5f;
static float seasonalCycle = 0.0f;
static float evolutionPressure = 0.0f;
static float nutrientLevel = 1.0f;
static float mutationRate = 0.1f;
static float timeAccum = 0.0f;
static float growthSpeed = 0.05f;
static float evolutionRate = 0.02f;
static float mutationFactor = 0.1f;
static float pruningIntensity = 0.0f;
static float regenerationRate = 0.05f;
static float organicTime = 0.0f;

// Growth state arrays
static float organismAge[8];
static float branchLengths[8];
static float branchAngles[8];
static int generationCount[8];
static bool isFlowering[8];
static float healthLevel[8];
static SbVec3f growthDirection[8];
static float growthStates[8];
static float branchAges[8];

class DialSet6 : public DialHandler {
public:
    void init(SceneObjects* scene, DialState* state);
    void handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state);
    void cleanup(SceneObjects* scene, DialState* state);
    const char* getDescription();
    const char* getThemeDescription();
    const char* getDialDescription(int dialIndex);

private:
    void handleBranchingProbability(float angle, SceneObjects* scene);
    void handleGrowthSpeed(float angle, SceneObjects* scene);
    void handleEvolutionRate(float angle, SceneObjects* scene);
    void handleMutationFactor(float angle, SceneObjects* scene);
    void handleSeasonalCycle(float angle, SceneObjects* scene);
    void handleNutrientLevel(float angle, SceneObjects* scene);
    void handlePruningIntensity(float angle, SceneObjects* scene);
    void handleRegenerationRate(float angle, SceneObjects* scene);
    void updateOrganicGrowth(SceneObjects* scene);
    void updateSeasonalEffects(SceneObjects* scene);
};

const char* DialSet6::getDescription() {
    return "ORGANIC GROWTH - Living L-systems with evolution, seasons, and ecosystems";
}

const char* DialSet6::getThemeDescription() {
    return "ORGANIC GROWTH - Living L-systems with evolution, seasons, and ecosystems";
}

const char* DialSet6::getDialDescription(int dialIndex) {
    static const char* descriptions[8] = {
        "DIAL 0: Branching - Probability of new growth branches",
        "DIAL 1: Growth Speed - Rate of organic development",
        "DIAL 2: Evolution Rate - Speed of genetic changes",
        "DIAL 3: Mutation Factor - Randomness in growth patterns",
        "DIAL 4: Seasonal Cycle - Environmental rhythm effects",
        "DIAL 5: Nutrient Level - Ecosystem health and vigor",
        "DIAL 6: Pruning Intensity - Natural selection pressure",
        "DIAL 7: Regeneration Rate - Healing and regrowth speed"
    };
    if (dialIndex >= 0 && dialIndex < 8) {
        return descriptions[dialIndex];
    }
    return "Unknown dial";
}

void DialSet6::init(SceneObjects* scene, DialState* state) {
    for (int i = 0; i < 8; i++) {
        growthStates[i] = 0.0f;
        branchAges[i] = 0.0f;
        organismAge[i] = 0.0f;
        branchLengths[i] = 1.0f;
        branchAngles[i] = 0.0f;
        generationCount[i] = 0;
        isFlowering[i] = false;
        healthLevel[i] = 1.0f;
        growthDirection[i] = SbVec3f(0, 1, 0);
        
        if (scene->dialMaterials[i]) {
            float greenness = 0.4f + (float)i / 16.0f;
            scene->dialMaterials[i]->diffuseColor.setValue(0.3f, greenness, 0.2f);
            scene->dialMaterials[i]->emissiveColor.setValue(0.1f, greenness * 0.3f, 0.05f);
            scene->dialMaterials[i]->specularColor.setValue(0.6f, 0.8f, 0.6f);
            scene->dialMaterials[i]->shininess.setValue(0.7f);
        }
    }
    
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(0.05f, 0.08f, 0.03f));
    }
}

void DialSet6::cleanup(SceneObjects* scene, DialState* state) {
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

void DialSet6::handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state) {
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
            handleBranchingProbability(state->dialAngles[0], scene);
            break;
        case 1:
            handleGrowthSpeed(state->dialAngles[1], scene);
            break;
        case 2:
            handleEvolutionRate(state->dialAngles[2], scene);
            break;
        case 3:
            handleMutationFactor(state->dialAngles[3], scene);
            break;
        case 4:
            handleSeasonalCycle(state->dialAngles[4], scene);
            break;
        case 5:
            handleNutrientLevel(state->dialAngles[5], scene);
            break;
        case 6:
            handlePruningIntensity(state->dialAngles[6], scene);
            break;
        case 7:
            handleRegenerationRate(state->dialAngles[7], scene);
            break;
    }

    updateOrganicGrowth(scene);
    
    if (scene->dialTransforms[dialIndex]) {
        scene->dialTransforms[dialIndex]->rotation.setValue(SbVec3f(0, 1, 0), state->dialAngles[dialIndex]);
    }
}

void DialSet6::handleBranchingProbability(float angle, SceneObjects* scene) {
    branchingProbability = 0.1f + (angle + M_PI) / (2.0f * M_PI) * 0.8f;
}

void DialSet6::handleGrowthSpeed(float angle, SceneObjects* scene) {
    growthSpeed = 0.01f + (angle + M_PI) * 0.1f;
}

void DialSet6::handleEvolutionRate(float angle, SceneObjects* scene) {
    evolutionRate = 0.01f + (angle + M_PI) * 0.05f;
}

void DialSet6::handleMutationFactor(float angle, SceneObjects* scene) {
    mutationFactor = (angle + M_PI) / (4.0f * M_PI);
}

void DialSet6::handleSeasonalCycle(float angle, SceneObjects* scene) {
    seasonalCycle = 0.1f + (angle + M_PI) * 2.0f;
}

void DialSet6::handleNutrientLevel(float angle, SceneObjects* scene) {
    nutrientLevel = 0.2f + (angle + M_PI) / (2.0f * M_PI) * 0.8f;
}

void DialSet6::handlePruningIntensity(float angle, SceneObjects* scene) {
    pruningIntensity = (angle + M_PI) / (2.0f * M_PI);
}

void DialSet6::handleRegenerationRate(float angle, SceneObjects* scene) {
    regenerationRate = 0.01f + (angle + M_PI) * 0.1f;
}

void DialSet6::updateOrganicGrowth(SceneObjects* scene) {
    organicTime += 0.02f;
    
    for (int i = 0; i < 8; i++) {
        if (scene->scatterOffsets[i]) {
            growthStates[i] += growthSpeed * nutrientLevel;
            branchAges[i] += 0.02f;
            
            float growthHeight = sinf(growthStates[i]) * 2.0f + 1.0f;
            float branchWidth = cosf(branchAges[i] * seasonalCycle) * 0.5f + 0.5f;
            
            scene->scatterOffsets[i]->translation.setValue(
                branchWidth * cosf(organicTime + i),
                growthHeight * sinf(organicTime * 0.7f + i),
                0.3f * sinf(organicTime * 1.2f + i)
            );
        }
    }
    
    updateSeasonalEffects(scene);
}

void DialSet6::updateSeasonalEffects(SceneObjects* scene) {
    float seasonPhase = sinf(organicTime * seasonalCycle);
    
    for (int i = 0; i < 8; i++) {
        if (scene->dialMaterials[i]) {
            float healthiness = nutrientLevel * (1.0f - pruningIntensity);
            float seasonal = 0.5f + 0.5f * seasonPhase;
            
            float r = 0.2f + healthiness * 0.3f;
            float g = 0.3f + healthiness * 0.6f * seasonal;
            float b = 0.2f + healthiness * 0.2f;
            
            scene->dialMaterials[i]->diffuseColor.setValue(r, g, b);
            scene->dialMaterials[i]->emissiveColor.setValue(r * 0.3f, g * 0.4f, b * 0.2f);
        }
    }
}

// Factory function
DialHandler* createDialSet6() {
    return new DialSet6();
}
