#include "dialhandler.h"
#include <Inventor/nodes/SoMaterial.h>
#include <math.h>

// Fluid dynamics simulation parameters
static float viscosity = 0.5f;           // Fluid thickness/resistance to flow
static float flowVelocity = 1.0f;        // Base fluid flow speed
static float pressure = 1.0f;            // Pressure field strength
static float turbulenceIntensity = 0.3f; // Chaotic flow patterns
static float vorticityStrength = 0.7f;   // Rotational flow (vortices)
static float surfaceTension = 0.8f;      // Cohesive force at boundaries
static float density = 1.0f;             // Fluid density affecting movement
static float temperatureGradient = 0.5f; // Heat-driven convection

// Fluid state tracking
static SbVec3f fluidVelocities[8];       // Velocity field at each dial
static SbVec3f pressureGradients[8];     // Pressure gradients
static float vortices[8];                // Vorticity values
static float temperatures[8];            // Temperature field
static float timeStep = 0.016f;          // Simulation time step
static float globalTime = 0.0f;

// Fluid flow patterns
static SbVec3f streamlines[8];
static float waveAmplitudes[8];
static bool isLaminarFlow = true;

class DialSet8 : public DialHandler {
public:
    void init(SceneObjects* scene, DialState* state);
    void handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state);
    void cleanup(SceneObjects* scene, DialState* state);
    const char* getDescription();
    const char* getThemeDescription();
    const char* getDialDescription(int dialIndex);

private:
    void handleViscosity(float angle, SceneObjects* scene);
    void handleFlowVelocity(float angle, SceneObjects* scene);
    void handlePressure(float angle, SceneObjects* scene);
    void handleTurbulence(float angle, SceneObjects* scene);
    void handleVorticity(float angle, SceneObjects* scene);
    void handleSurfaceTension(float angle, SceneObjects* scene);
    void handleDensity(float angle, SceneObjects* scene);
    void handleTemperatureGradient(float angle, SceneObjects* scene);
    void updateFluidSimulation(SceneObjects* scene);
    void calculateFluidForces(int dialIndex, SceneObjects* scene);
    void applyNavierStokes(SceneObjects* scene);
    void updateFluidVisualization(SceneObjects* scene);
};

const char* DialSet8::getDescription() {
    return "FLUID DYNAMICS - Advanced fluid simulation with viscosity, flow, pressure, and vortices";
}

const char* DialSet8::getThemeDescription() {
    return "FLUID DYNAMICS - Advanced computational fluid dynamics simulation";
}

const char* DialSet8::getDialDescription(int dialIndex) {
    static const char* descriptions[8] = {
        "DIAL 0: Viscosity - Fluid thickness and resistance to flow",
        "DIAL 1: Flow Velocity - Base fluid flow speed and direction",
        "DIAL 2: Pressure - Pressure field strength and gradients",
        "DIAL 3: Turbulence - Chaotic flow patterns and Reynolds number",
        "DIAL 4: Vorticity - Rotational flow strength and vortex formation",
        "DIAL 5: Surface Tension - Cohesive forces at fluid boundaries",
        "DIAL 6: Density - Fluid density affecting buoyancy and motion",
        "DIAL 7: Temperature - Heat-driven convection and thermal gradients"
    };
    if (dialIndex >= 0 && dialIndex < 8) {
        return descriptions[dialIndex];
    }
    return "Unknown dial";
}

void DialSet8::init(SceneObjects* scene, DialState* state) {
    globalTime = 0.0f;
    
    // Initialize fluid state
    for (int i = 0; i < 8; i++) {
        fluidVelocities[i] = SbVec3f(0, 0, 0);
        pressureGradients[i] = SbVec3f(0, 0, 0);
        vortices[i] = 0.0f;
        temperatures[i] = 20.0f; // Room temperature baseline
        streamlines[i] = SbVec3f(0, 0, 0);
        waveAmplitudes[i] = 0.0f;
        
        if (scene->dialMaterials[i]) {
            // Initialize with fluid-like blue-to-white gradient
            float temp = temperatures[i] / 100.0f; // Normalize temperature
            scene->dialMaterials[i]->diffuseColor.setValue(
                0.1f + temp * 0.3f,      // Red component (heat)
                0.3f + temp * 0.2f,      // Green component
                0.8f - temp * 0.3f       // Blue component (cool)
            );
            scene->dialMaterials[i]->emissiveColor.setValue(0.1f, 0.2f, 0.4f);
            scene->dialMaterials[i]->specularColor.setValue(0.9f, 0.9f, 1.0f);
            scene->dialMaterials[i]->shininess.setValue(0.8f);
            scene->dialMaterials[i]->transparency.setValue(0.2f); // Fluid-like transparency
        }
    }
    
    // Set oceanic background
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(0.05f, 0.15f, 0.3f));
    }
}

void DialSet8::cleanup(SceneObjects* scene, DialState* state) {
    // Reset to standard material properties
    for (int i = 0; i < 8; i++) {
        if (scene->dialMaterials[i]) {
            scene->dialMaterials[i]->transparency.setValue(0.0f);
            scene->dialMaterials[i]->emissiveColor.setValue(0.0f, 0.0f, 0.0f);
        }
    }
    
    if (scene->globalViewer) {
        scene->globalViewer->setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));
    }
}

void DialSet8::handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state) {
    // Convert raw value to angle
    float angle = (float)rawValue / 100.0f * 2.0f * M_PI;
    state->dialAngles[dialIndex] = angle;
    
    // Update the global time for fluid simulation
    globalTime += timeStep;
    
    switch (dialIndex) {
        case 0: handleViscosity(angle, scene); break;
        case 1: handleFlowVelocity(angle, scene); break;
        case 2: handlePressure(angle, scene); break;
        case 3: handleTurbulence(angle, scene); break;
        case 4: handleVorticity(angle, scene); break;
        case 5: handleSurfaceTension(angle, scene); break;
        case 6: handleDensity(angle, scene); break;
        case 7: handleTemperatureGradient(angle, scene); break;
    }
    
    // Update fluid simulation
    updateFluidSimulation(scene);
    calculateFluidForces(dialIndex, scene);
    updateFluidVisualization(scene);
}

void DialSet8::handleViscosity(float angle, SceneObjects* scene) {
    viscosity = 0.1f + (sinf(angle) * 0.5f + 0.5f) * 2.0f; // 0.1 to 2.1
    
    // Higher viscosity means slower, more damped movement
    float dampingFactor = 1.0f / (1.0f + viscosity);
    
    for (int i = 0; i < 8; i++) {
        fluidVelocities[i] *= dampingFactor;
        
        if (scene->scatterOffsets[i]) {
            SbVec3f current = scene->scatterOffsets[i]->translation.getValue();
            float viscousDrag = viscosity * 0.1f;
            scene->scatterOffsets[i]->translation.setValue(
                current.getValue()[0] * (1.0f - viscousDrag),
                current.getValue()[1] * (1.0f - viscousDrag),
                current.getValue()[2] * (1.0f - viscousDrag)
            );
        }
    }
}

void DialSet8::handleFlowVelocity(float angle, SceneObjects* scene) {
    flowVelocity = (sinf(angle) * 0.5f + 0.5f) * 3.0f; // 0 to 3
    
    // Create directional flow pattern
    SbVec3f flowDirection(cosf(angle), sinf(angle), sinf(angle * 0.5f));
    
    for (int i = 0; i < 8; i++) {
        SbVec3f velocityContribution = flowDirection;
        velocityContribution *= (flowVelocity * timeStep);
        fluidVelocities[i] += velocityContribution;
        
        // Create streamline effect
        SbVec3f streamlineContribution = flowDirection;
        streamlineContribution *= (flowVelocity * 0.02f);
        streamlines[i] += streamlineContribution;
        
        if (scene->dialTransforms[i]) {
            float flowRotation = flowVelocity * globalTime * 0.5f;
            scene->dialTransforms[i]->rotation.setValue(
                SbVec3f(0, 1, 0), flowRotation + i * M_PI / 4.0f
            );
        }
    }
}

void DialSet8::handlePressure(float angle, SceneObjects* scene) {
    pressure = 0.5f + (sinf(angle) * 0.5f + 0.5f) * 2.0f; // 0.5 to 2.5
    
    // High pressure creates outward forces, low pressure creates inward forces
    SbVec3f pressureCenter(0, 0, 0);
    
    for (int i = 0; i < 8; i++) {
        float dialAngle = i * 2.0f * M_PI / 8.0f;
        SbVec3f dialPosition(cosf(dialAngle) * 2.0f, sinf(dialAngle) * 2.0f, 0);
        
        SbVec3f pressureForce = dialPosition * (pressure - 1.0f) * 0.1f;
        pressureGradients[i] = pressureForce;
        
        if (scene->scatterOffsets[i]) {
            SbVec3f current = scene->scatterOffsets[i]->translation.getValue();
            scene->scatterOffsets[i]->translation.setValue(
                current.getValue()[0] + pressureForce.getValue()[0],
                current.getValue()[1] + pressureForce.getValue()[1],
                current.getValue()[2] + pressureForce.getValue()[2]
            );
        }
    }
}

void DialSet8::handleTurbulence(float angle, SceneObjects* scene) {
    turbulenceIntensity = (sinf(angle) * 0.5f + 0.5f); // 0 to 1
    
    // Turbulence creates chaotic, unpredictable flow patterns
    isLaminarFlow = turbulenceIntensity < 0.3f;
    
    for (int i = 0; i < 8; i++) {
        if (turbulenceIntensity > 0.1f) {
            // Add random turbulent forces
            float turbX = (sinf(globalTime * 5.0f + i) * 2.0f - 1.0f) * turbulenceIntensity;
            float turbY = (cosf(globalTime * 7.0f + i) * 2.0f - 1.0f) * turbulenceIntensity;
            float turbZ = (sinf(globalTime * 3.0f + i) * 2.0f - 1.0f) * turbulenceIntensity;
            
            SbVec3f turbulence(turbX, turbY, turbZ);
            SbVec3f turbulenceContribution = turbulence;
            turbulenceContribution *= 0.05f;
            fluidVelocities[i] += turbulenceContribution;
            
            if (scene->cylinderOffsets[i]) {
                SbVec3f current = scene->cylinderOffsets[i]->translation.getValue();
                scene->cylinderOffsets[i]->translation.setValue(
                    current.getValue()[0] + turbX * 0.02f,
                    current.getValue()[1] + turbY * 0.02f,
                    current.getValue()[2] + turbZ * 0.02f
                );
            }
        }
    }
}

void DialSet8::handleVorticity(float angle, SceneObjects* scene) {
    vorticityStrength = (sinf(angle) * 0.5f + 0.5f); // 0 to 1
    
    // Create vortex patterns
    for (int i = 0; i < 8; i++) {
        float dialAngle = i * 2.0f * M_PI / 8.0f;
        vortices[i] = vorticityStrength * sinf(globalTime * 2.0f + dialAngle);
        
        // Apply rotational force based on vorticity
        float vortexRadius = 2.0f;
        float rotationalForce = vortices[i] * 0.1f;
        
        SbVec3f vortexCenter(0, 0, 0);
        SbVec3f dialPos(cosf(dialAngle) * vortexRadius, sinf(dialAngle) * vortexRadius, 0);
        SbVec3f tangential(-sinf(dialAngle), cosf(dialAngle), 0);
        
        fluidVelocities[i] += tangential * rotationalForce;
        
        if (scene->dialTransforms[i]) {
            float vortexRotation = vortices[i] * globalTime;
            scene->dialTransforms[i]->rotation.setValue(
                SbVec3f(0, 0, 1), vortexRotation
            );
        }
    }
}

void DialSet8::handleSurfaceTension(float angle, SceneObjects* scene) {
    surfaceTension = (sinf(angle) * 0.5f + 0.5f); // 0 to 1
    
    // Surface tension tries to minimize surface area (creates cohesive forces)
    for (int i = 0; i < 8; i++) {
        SbVec3f cohesiveForce(0, 0, 0);
        
        // Calculate forces toward neighboring dials
        for (int j = 0; j < 8; j++) {
            if (i != j) {
                float iAngle = i * 2.0f * M_PI / 8.0f;
                float jAngle = j * 2.0f * M_PI / 8.0f;
                
                SbVec3f iPos(cosf(iAngle), sinf(iAngle), 0);
                SbVec3f jPos(cosf(jAngle), sinf(jAngle), 0);
                
                SbVec3f diff = jPos - iPos;
                float distance = diff.length();
                
                if (distance < 3.0f) { // Only nearby neighbors
                    SbVec3f normalizedDiff = diff;
                    normalizedDiff.normalize();
                    SbVec3f forceContribution = normalizedDiff;
                    forceContribution *= (surfaceTension * 0.02f);
                    cohesiveForce += forceContribution;
                }
            }
        }
        
        if (scene->scatterOffsets[i]) {
            SbVec3f current = scene->scatterOffsets[i]->translation.getValue();
            scene->scatterOffsets[i]->translation.setValue(
                current.getValue()[0] + cohesiveForce.getValue()[0],
                current.getValue()[1] + cohesiveForce.getValue()[1],
                current.getValue()[2] + cohesiveForce.getValue()[2]
            );
        }
    }
}

void DialSet8::handleDensity(float angle, SceneObjects* scene) {
    density = 0.5f + (sinf(angle) * 0.5f + 0.5f) * 2.0f; // 0.5 to 2.5
    
    // Density affects buoyancy and inertial forces
    float buoyancyForce = (1.0f - density) * 0.1f; // Lighter fluids rise
    
    for (int i = 0; i < 8; i++) {
        SbVec3f buoyancy(0, buoyancyForce, 0);
        fluidVelocities[i] += buoyancy;
        
        // Adjust dial material to reflect density
        if (scene->dialMaterials[i]) {
            float opacity = 0.3f + density * 0.4f; // Denser = more opaque
            scene->dialMaterials[i]->transparency.setValue(1.0f - opacity);
        }
        
        if (scene->cylinderOffsets[i]) {
            SbVec3f current = scene->cylinderOffsets[i]->translation.getValue();
            scene->cylinderOffsets[i]->translation.setValue(
                current.getValue()[0],
                current.getValue()[1] + buoyancyForce,
                current.getValue()[2]
            );
        }
    }
}

void DialSet8::handleTemperatureGradient(float angle, SceneObjects* scene) {
    temperatureGradient = (sinf(angle) * 0.5f + 0.5f); // 0 to 1
    
    // Temperature creates convection currents
    for (int i = 0; i < 8; i++) {
        // Create temperature field based on position and gradient
        float baseTemp = 20.0f + temperatureGradient * 80.0f; // 20°C to 100°C
        float dialAngle = i * 2.0f * M_PI / 8.0f;
        temperatures[i] = baseTemp + sinf(globalTime + dialAngle) * 20.0f;
        
        // Convection: hot fluid rises, cold fluid sinks
        float convectionForce = (temperatures[i] - 50.0f) * 0.002f; // Normalize around 50°C
        SbVec3f convection(0, convectionForce, 0);
        fluidVelocities[i] += convection;
        
        // Update material color based on temperature
        if (scene->dialMaterials[i]) {
            float tempNorm = (temperatures[i] - 20.0f) / 80.0f; // Normalize 0-1
            scene->dialMaterials[i]->diffuseColor.setValue(
                0.2f + tempNorm * 0.8f,      // Red increases with heat
                0.3f + (1.0f - tempNorm) * 0.3f, // Green decreases with heat
                0.8f - tempNorm * 0.6f       // Blue decreases with heat
            );
            scene->dialMaterials[i]->emissiveColor.setValue(
                tempNorm * 0.3f, tempNorm * 0.1f, 0.0f
            );
        }
    }
}

void DialSet8::updateFluidSimulation(SceneObjects* scene) {
    // Apply simplified Navier-Stokes equation
    applyNavierStokes(scene);
    
    // Update wave patterns
    for (int i = 0; i < 8; i++) {
        waveAmplitudes[i] = sinf(globalTime * 2.0f + i * M_PI / 4.0f) * flowVelocity * 0.1f;
        
        if (scene->scatterOffsets[i]) {
            SbVec3f current = scene->scatterOffsets[i]->translation.getValue();
            scene->scatterOffsets[i]->translation.setValue(
                current.getValue()[0],
                current.getValue()[1] + waveAmplitudes[i],
                current.getValue()[2]
            );
        }
    }
}

void DialSet8::calculateFluidForces(int dialIndex, SceneObjects* scene) {
    // Integrate all forces affecting the specific dial
    SbVec3f totalForce = fluidVelocities[dialIndex] + pressureGradients[dialIndex];
    
    // Apply the force to dial position
    if (scene->dialTransforms[dialIndex]) {
        SbVec3f current = scene->dialTransforms[dialIndex]->translation.getValue();
        SbVec3f newPos = current + totalForce * timeStep;
        
        // Clamp to reasonable bounds
        float maxDisplacement = 1.0f;
        if (newPos.length() > maxDisplacement) {
            newPos.normalize();
            newPos *= maxDisplacement;
        }
        
        scene->dialTransforms[dialIndex]->translation.setValue(newPos);
    }
}

void DialSet8::applyNavierStokes(SceneObjects* scene) {
    // Simplified Navier-Stokes: ∂v/∂t = -v·∇v - ∇p/ρ + ν∇²v + f
    for (int i = 0; i < 8; i++) {
        SbVec3f& velocity = fluidVelocities[i];
        
        // Advection term: -v·∇v (simplified)
        SbVec3f advection = velocity * (-0.1f);
        
        // Pressure term: -∇p/ρ
        SbVec3f pressureTerm = pressureGradients[i] * (1.0f / density);
        
        // Viscosity term: ν∇²v (simplified as damping)
        SbVec3f viscosityTerm = velocity * (-viscosity * 0.1f);
        
        // External forces (buoyancy, etc.)
        SbVec3f externalForces(0, 0, 0);
        
        // Update velocity
        velocity += (advection + pressureTerm + viscosityTerm + externalForces) * timeStep;
    }
}

void DialSet8::updateFluidVisualization(SceneObjects* scene) {
    // Update background to reflect fluid state
    if (scene->globalViewer) {
        float avgTemp = 0.0f;
        for (int i = 0; i < 8; i++) {
            avgTemp += temperatures[i];
        }
        avgTemp /= 8.0f;
        
        float tempNorm = (avgTemp - 20.0f) / 80.0f;
        float r = 0.05f + tempNorm * 0.2f;
        float g = 0.15f + (1.0f - tempNorm) * 0.1f;
        float b = 0.3f - tempNorm * 0.15f;
        
        scene->globalViewer->setBackgroundColor(SbColor(r, g, b));
    }
}

// Factory function
DialHandler* createDialSet8() {
    return new DialSet8();
} 