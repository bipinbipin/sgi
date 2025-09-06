#include "dialhandler.h"

#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoCylinder.h>

#include <math.h>
#include <vector>
#include <stdio.h>

// Simple tree model controls (first pass placeholder before full Space Colonization)
// Note: backgroundHue removed - dial 0 now controls tree X-rotation

// Space Colonization parameters (moving toward 3D)
static float crownRadius = 3.0f;        // Dial 1: horizontal radius of crown ellipsoid
static int   attractorCount = 400;      // Dial 2: number of attractor points (reduced default)
static float influenceRadius = 1.2f;    // Fixed value for colonization algorithm (dial 3 now controls forward/back)
static float killDistance = 0.5f;       // Dial 4: kill distance
static float stepLength = 0.35f;        // Dial 5: branch step length
static float tropismUp = 0.06f;         // Dial 6: upward tropism
static float branchHue = 0.10f;         // Dial 7: Branch color hue
static float crownHeight = 6.0f;        // internal: vertical height of crown
static float treeScale = 1.5f;          // overall scale applied via transform

// Forking and thickness controls
static float forkAngleDeg = 18.0f;      // fork divergence angle
static int   maxForksPerTip = 2;        // limit branches per tip per step
static float thicknessBase = 0.12f;     // base radius near trunk
static float thicknessDecay = 0.82f;    // per-depth decay factor (stronger thinning)
static float segmentTaper = 0.88f;      // extra per-segment taper factor (per segment)

struct TreeScene {
    SoSeparator* root;
    SoTransform* treeTransform;
    SoMaterial* branchMaterial;
    SoDrawStyle* drawStyle;
    SoCoordinate3* coords;
    SoLineSet* lines;
    SoSeparator* branchContainer;        // holds branch cylinders
    // Colonization state
    std::vector<SbVec3f> nodePositions;  // includes root and grown nodes
    std::vector<int>      parentIndex;    // for each node (except root), parent idx
    std::vector<SbVec3f> attractors;      // persistent attractors
    bool needReseed;
    std::vector<int>      frontierIndices; // nodes at the growth front
    std::vector<SbVec3f>  nodeDirs;        // per-node growth direction for smoothing
    std::vector<int>      nodeDepth;       // depth from root for thickness
    bool growthPaused;                     // pause further growth at caps
    // Performance monitoring
    int consecutiveSlowFrames;             // count of slow render cycles
    int totalRenderCalls;                  // total renders since init
};

class DialSet10 : public DialHandler {
public:
    void init(SceneObjects* scene, DialState* state);
    void handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state);
    void cleanup(SceneObjects* scene, DialState* state);
    const char* getDescription();
    const char* getThemeDescription();
    const char* getDialDescription(int dialIndex);

private:
    void rebuildTree(SceneObjects* scene, DialState* state);
    void hueToRGB(float hue, float& r, float& g, float& b);
    void runColonizationSteps(TreeScene* tree, int steps);
    void seedInitial(TreeScene* tree);
    float rand01(unsigned int seed);
};

static int gStepsHint = 1;
static bool gSystemOverloaded = false;  // global safety valve

// (legacy parametric helpers removed; using 3D space colonization)

// Forward declaration for local utility
static float clampFloatToRange(float value, float minValue, float maxValue);

// Rotate vector v around axis k (unit) by angle radians using Rodrigues' formula
static SbVec3f rotateAroundAxis(const SbVec3f &v, const SbVec3f &kUnit, float angleRad) {
    float c = cosf(angleRad);
    float s = sinf(angleRad);
    SbVec3f kxv = kUnit.cross(v);
    float kdotv = kUnit.dot(v);
    return v * c + kxv * s + kUnit * (kdotv * (1.0f - c));
}

// Hard caps to keep UI responsive regardless of dial settings
static int kMaxNodes = 300;             // absolute cap on grown nodes (reduced from 1500)
static int kMaxRenderSegments = 150;    // cap on rendered cylinders (reduced from 500)
static int kMaxAttractors = 800;        // cap on attractors to prevent overwhelming

void DialSet10::init(SceneObjects* scene, DialState* state) {
    crownRadius = 3.0f;
    attractorCount = 400;
    influenceRadius = 1.2f;        // Still used in colonization algorithm
    killDistance = 0.5f;
    stepLength = 0.35f;
    tropismUp = 0.06f;
    treeScale = 1.5f;
    branchHue = 0.10f;
    
    // Reset global overload state on init
    gSystemOverloaded = false;

    // Prepare tree group
    if (scene->treeGroup) {
        scene->treeGroup->whichChild.setValue(-3); // ensure visible when selected
        while (scene->treeGroup->getNumChildren() > 0) {
            scene->treeGroup->removeChild(0);
        }

        SoSeparator* root = new SoSeparator;
        SoTransform* t = new SoTransform;
        t->translation.setValue(0.0f, -6.5f, 0.0f);
        t->scaleFactor.setValue(treeScale, treeScale, treeScale);
        root->addChild(t);

        SoMaterial* material = new SoMaterial;
        float br, bg, bb; hueToRGB(branchHue, br, bg, bb);
        material->diffuseColor.setValue(br, bg, bb);
        material->specularColor.setValue(0.2f, 0.2f, 0.2f);
        material->shininess.setValue(0.2f);
        root->addChild(material);
        SoDrawStyle* style = new SoDrawStyle;
        style->lineWidth.setValue(3.0f);
        root->addChild(style);

        // Container for cylinder branches (3D geometry)
        SoSeparator* branchContainer = new SoSeparator;
        root->addChild(branchContainer);

        // Retain a lightweight line visualization for debugging (hidden by default)
        SoCoordinate3* coords = new SoCoordinate3;
        SoLineSet* lines = new SoLineSet;
        SoDrawStyle* debugStyle = new SoDrawStyle; debugStyle->lineWidth.setValue(1.0f);
        debugStyle->linePattern.setValue(0x0F0F); // dashed
        // Comment these in to see debug lines:
        // root->addChild(debugStyle);
        // root->addChild(coords);
        // root->addChild(lines);

        scene->treeGroup->addChild(root);

        TreeScene* tree = new TreeScene;
        tree->root = root;
        tree->treeTransform = t;
        tree->branchMaterial = material;
        tree->coords = coords;
        tree->lines = lines;
        tree->branchContainer = branchContainer;
        tree->drawStyle = style;
        tree->needReseed = true;
        tree->growthPaused = false;
        tree->consecutiveSlowFrames = 0;
        tree->totalRenderCalls = 0;
        state->handlerData = (void*)tree;
    }

    if (scene->globalViewer) {
        // Set a fixed nice background for tree viewing (dark blue-green)
        scene->globalViewer->setBackgroundColor(SbColor(0.05f, 0.08f, 0.12f));
    }

    rebuildTree(scene, state);
}
//comment
void DialSet10::cleanup(SceneObjects* scene, DialState* state) {
    if (scene->treeGroup) {
        while (scene->treeGroup->getNumChildren() > 0) {
            scene->treeGroup->removeChild(0);
        }
    }
    if (state->handlerData) {
        TreeScene* tree = (TreeScene*)state->handlerData;
        delete tree;
        state->handlerData = NULL;
    }
}

const char* DialSet10::getDescription() {
    return "TREE STUDIO - Simple procedural tree (pre-colonization)";
}

const char* DialSet10::getThemeDescription() {
    return "TREE STUDIO - Branching prototype; next: Space Colonization";
}

const char* DialSet10::getDialDescription(int i) {
    static const char* d[8] = {
        "DIAL 0: Tree X-Rotation - Rotate tree around X-axis",
        "DIAL 1: Crown Radius (XY)",
        "DIAL 2: Attractor Count",
        "DIAL 3: Tree Forward/Back - Move tree closer/farther",
        "DIAL 4: Kill Distance",
        "DIAL 5: Step Length",
        "DIAL 6: Tropism Up",
        "DIAL 7: Branch Hue"
    };
    if (i >= 0 && i < 8) return d[i];
    return "Unknown dial";
}

void DialSet10::handleDial(int dialIndex, int rawValue, SceneObjects* scene, DialState* state) {
    float angle = (float)rawValue / 100.0f * 2.0f * (float)M_PI;
    state->dialAngles[dialIndex] = angle;

    switch (dialIndex) {
        case 0: {
            // Tree X-Rotation control
            if (state->handlerData) {
                TreeScene* tree = (TreeScene*)state->handlerData;
                if (tree->treeTransform) {
                    // Convert dial angle to rotation (full 360 degree range)
                    float rotationAngle = angle;  // Use raw angle for full rotation
                    tree->treeTransform->rotation.setValue(SbVec3f(1, 0, 0), rotationAngle);
                }
            }
            break;
        }
        case 1: {
            float a01 = (sinf(angle) * 0.5f + 0.5f); // 0..1
            crownRadius = 1.0f + a01 * 5.0f;        // 1..6
            crownHeight = 2.0f * crownRadius;       // proportional height
            break;
        }
        case 2: {
            float a01 = (sinf(angle) * 0.5f + 0.5f);
            attractorCount = 100 + (int)(a01 * 700.0f); // 100..800 (capped)
            if (attractorCount > kMaxAttractors) attractorCount = kMaxAttractors;
            if (state->handlerData) ((TreeScene*)state->handlerData)->needReseed = true;
            break;
        }
        case 3: {
            // Tree Forward/Back control (like dialset0 dial 2)
            if (state->handlerData) {
                TreeScene* tree = (TreeScene*)state->handlerData;
                if (tree->treeTransform) {
                    // Use same logic as dialset0: -5.0f + angle * 2.0f
                    float zoomZ = -5.0f + state->dialAngles[3] * 2.0f;
                    // Get current translation and only modify Z component
                    SbVec3f currentTrans = tree->treeTransform->translation.getValue();
                    tree->treeTransform->translation.setValue(currentTrans[0], currentTrans[1], zoomZ);
                }
            }
            break;
        }
        case 4: {
            float a01 = (sinf(angle) * 0.5f + 0.5f);
            killDistance = 0.3f + a01 * 1.0f; // 0.3..1.3 (avoid too-small values)
            break;
        }
        case 5: {
            float a01 = (sinf(angle) * 0.5f + 0.5f);
            stepLength = 0.1f + a01 * 0.7f; // 0.1..0.8
            break;
        }
        case 6: {
            float a01 = (sinf(angle) * 0.5f + 0.5f);
            tropismUp = a01 * 0.3f; // 0..0.3 upward bias
            // If user reduces tropism significantly and we are paused at cap,
            // allow unpausing so a change in direction can reduce active tips
            if (state->handlerData) {
                TreeScene* tree = (TreeScene*)state->handlerData;
                if (tree->growthPaused && tropismUp < 0.05f) tree->growthPaused = false;
            }
            break;
        }
        case 7: {
            // Only update color; do not reseed or rebuild immediately to avoid accidental resets
            branchHue = (sinf(angle) * 0.5f + 0.5f);
            break;
        }
    }

    // Update branch color and transform immediately
    if (state->handlerData) {
        TreeScene* tree = (TreeScene*)state->handlerData;
        // Do not modify transform on color/background changes to avoid unintended shape changes
        if (tree->branchMaterial) {
            float r, g, b; hueToRGB(branchHue, r, g, b);
            tree->branchMaterial->diffuseColor.setValue(r, g, b);
        }
        if (tree->drawStyle) {
            // Adjust line width based on crown radius for visual balance
            float lw = 1.5f + (crownRadius * 0.6f);
            if (lw > 5.0f) lw = 5.0f;
            tree->drawStyle->lineWidth.setValue(lw);
        }
    }

    // For background/model color changes, we avoid heavy rebuilds here
    if (dialIndex != 0 && dialIndex != 7) {
        rebuildTree(scene, state);
    } else {
        // Light render refresh
        if (scene->globalViewer) scene->globalViewer->render();
    }
}

void DialSet10::rebuildTree(SceneObjects* scene, DialState* state) {
    if (!state->handlerData) return;
    TreeScene* tree = (TreeScene*)state->handlerData;
    if (!tree->coords || !tree->lines) return;
    
    // Safety valve - if system is overloaded, only allow color changes
    if (gSystemOverloaded) {
        printf("[TreeStudio] System overloaded - skipping tree rebuild\n");
        return;
    }

    // Optionally reseed attractors when distribution parameters changed
    if (tree->needReseed) {
        // Generate attractors in an ellipsoidal crown region above the root
        tree->attractors.clear();
        tree->attractors.reserve(attractorCount);
        unsigned int seed = (unsigned int)attractorCount + 12345u;
        for (int i = 0; i < attractorCount; ++i) {
            float u1 = rand01(seed + i * 3 + 0);
            float u2 = rand01(seed + i * 3 + 1);
            float u3 = rand01(seed + i * 3 + 2);

            float h01 = 0.05f + 0.95f * u1;
            float y = h01 * crownHeight;
            float rMax = crownRadius * (0.3f + 0.7f * h01);
            float theta = 2.0f * (float)M_PI * u2;
            float r = sqrtf(u3) * rMax;
            float x = r * cosf(theta);
            float z = r * sinf(theta);
            tree->attractors.push_back(SbVec3f(x, y, z));
        }
        // Rather than a full reset, keep existing nodes if present to avoid jarring resets.
        // If empty (first time), seed initial trunk.
        if (tree->nodePositions.empty()) {
            seedInitial(tree);
        }
        tree->needReseed = false;
        tree->growthPaused = false;
    }

    // Perform a tiny number of growth steps per dial event to keep UI responsive
    if (!tree->growthPaused) {
        // Adaptive step hint based on current complexity
        int currentNodes = (int)tree->nodePositions.size();
        int adaptiveSteps = gStepsHint;
        if (currentNodes > 100) adaptiveSteps = 1;      // very conservative past 100 nodes
        if (currentNodes > 200) adaptiveSteps = 0;      // pause at 200 nodes
        
        if (adaptiveSteps > 0) {
            runColonizationSteps(tree, adaptiveSteps);
        } else {
            tree->growthPaused = true;
            printf("[TreeStudio] Growth auto-paused due to complexity\n");
        }
    }

    // Rebuild 3D cylinder geometry for branches
    // Clear previous cylinders
    if (tree->branchContainer) {
        while (tree->branchContainer->getNumChildren() > 0) tree->branchContainer->removeChild(0);
    }

    int totalNodes = (int)tree->nodePositions.size();
    if (totalNodes > 1 && tree->branchContainer) {
        // Compute simple per-node depth from root for thickness
        tree->nodeDepth.assign(totalNodes, 0);
        for (int i = 1; i < totalNodes; ++i) {
            int p = tree->parentIndex[i];
            int d = (p >= 0) ? tree->nodeDepth[p] + 1 : 0;
            tree->nodeDepth[i] = d;
        }

        // If we exceed render cap, pause further growth and keep oldest segments (trunk) visible
        int numSegments = totalNodes - 1;
        const int maxRenderSegments = kMaxRenderSegments; // fewer cylinders than lines
        int startNode = 1;
        if (numSegments > maxRenderSegments) {
            // Freeze growth and render only the first maxRenderSegments from the base
            tree->growthPaused = true;
            numSegments = maxRenderSegments;
            startNode = 1;
        }

        int endNode = startNode + numSegments;
        if (endNode > totalNodes) endNode = totalNodes;
        for (int i = startNode; i < endNode; ++i) {
            int p = tree->parentIndex[i];
            if (p < 0) continue;
            SbVec3f a = tree->nodePositions[p];
            SbVec3f b = tree->nodePositions[i];
            SbVec3f seg = b - a;
            float len = seg.length();
            if (len < 1e-4f) continue;

            // Compute thickness based on parent depth and extra per-segment taper
            int dParent = tree->nodeDepth[p];
            float parentRadius = thicknessBase * powf(thicknessDecay, (float)dParent);
            float radius = parentRadius * segmentTaper;
            if (radius < 0.006f) radius = 0.006f; // allow thinner tips to reduce draw cost

            // Orientation: align cylinder with segment
            SbVec3f dir = seg; dir.normalize();
            // default cylinder axis is Y; compute rotation from Y to dir
            SbVec3f yAxis(0.0f, 1.0f, 0.0f);
            SbVec3f axis = yAxis.cross(dir);
            float dot = yAxis.dot(dir);
            float angle = acosf(clampFloatToRange(dot, -1.0f, 1.0f));

            SoTransform* tSeg = new SoTransform;
            // position at midpoint
            SbVec3f mid = (a + b) * 0.5f;
            tSeg->translation.setValue(mid);
            if (axis.length() > 1e-6f && angle > 1e-6f) {
                axis.normalize();
                tSeg->rotation.setValue(axis, angle);
            }
            tSeg->scaleFactor.setValue(radius, len * 0.5f, radius); // scale Y by half, cylinder height=2*Y

            SoSeparator* segSep = new SoSeparator;
            segSep->addChild(tSeg);
            SoCylinder* cyl = new SoCylinder;
            cyl->radius = 1.0f; // scaled by transform
            cyl->height = 2.0f; // so scaleFactor Y = len/2
            segSep->addChild(cyl);
            tree->branchContainer->addChild(segSep);
        }
    }

    if (scene->globalViewer) scene->globalViewer->render();
    
    // Check for system overload conditions
    tree->totalRenderCalls++;
    int currentNodes = (int)tree->nodePositions.size();
    int totalCylinders = tree->branchContainer ? tree->branchContainer->getNumChildren() : 0;
    
    if (currentNodes > kMaxNodes * 0.9f || totalCylinders > kMaxRenderSegments * 0.9f) {
        tree->consecutiveSlowFrames++;
        if (tree->consecutiveSlowFrames > 3) {
            gSystemOverloaded = true;
            tree->growthPaused = true;
            printf("[TreeStudio] System overload detected - tree growth disabled\n");
        }
    } else {
        tree->consecutiveSlowFrames = 0;  // reset counter
    }
}

void DialSet10::hueToRGB(float hue, float& r, float& g, float& b) {
    float h = hue * 6.0f;
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

// Factory
DialHandler* createDialSet10() { return new DialSet10(); }

// --------------------- Colonization helpers ---------------------

void DialSet10::seedInitial(TreeScene* tree) {
    tree->nodePositions.clear();
    tree->parentIndex.clear();
    tree->frontierIndices.clear();
    tree->nodeDirs.clear();
    // Root at origin
    tree->nodePositions.push_back(SbVec3f(0.0f, 0.0f, 0.0f));
    tree->parentIndex.push_back(-1);

    // Seed a short vertical trunk to reach the crown's lower band
    int trunkSegments = 8;
    float targetHeight = crownHeight * 0.2f; // ~20% of crown height
    float seg = stepLength;
    float y = 0.0f;
    int parent = 0;
    for (int i = 0; i < trunkSegments && y < targetHeight; ++i) {
        y += seg;
        SbVec3f p(0.0f, y, 0.0f);
        tree->nodePositions.push_back(p);
        tree->parentIndex.push_back(parent);
        parent = (int)tree->nodePositions.size() - 1;
    }
    // Initialize frontier with the last trunk node
    tree->frontierIndices.push_back(parent);
    tree->nodeDirs.resize(tree->nodePositions.size(), SbVec3f(0.0f, 1.0f, 0.0f));
}

float DialSet10::rand01(unsigned int seed) {
    unsigned int x = seed * 1664525u + 1013904223u;
    x ^= x >> 16; x *= 2246822519u; x ^= x >> 13; x *= 3266489917u; x ^= x >> 16;
    return (x & 0x00FFFFFF) / (float)0x01000000; // [0,1)
}

static float clampFloatToRange(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

void DialSet10::runColonizationSteps(TreeScene* tree, int steps) {
    const float minNodeSpacing = stepLength * 0.6f;
    const int maxNodes = kMaxNodes;

    for (int iteration = 0; iteration < steps; ++iteration) {
        int numNodes = (int)tree->nodePositions.size();
        if (numNodes >= maxNodes) {
            tree->growthPaused = true;
            break;
        }
        
        // Early termination if too few attractors remain
        if (tree->attractors.size() < 10) {
            printf("[TreeStudio] Growth stopped: insufficient attractors\n");
            break;
        }

        // Accumulators per node
        std::vector<SbVec3f> accum;
        std::vector<int> counts;
        accum.resize(numNodes, SbVec3f(0.0f, 0.0f, 0.0f));
        counts.resize(numNodes, 0);

        // For each attractor, find closest node within influence radius
        int numActiveAttractors = 0;
        for (int a = 0; a < (int)tree->attractors.size(); ++a) {
            SbVec3f pa = tree->attractors[a];
            float bestDist2 = influenceRadius * influenceRadius;
            int bestIdx = -1;
            for (int n = 0; n < numNodes; ++n) {
                SbVec3f d = pa - tree->nodePositions[n];
                float d2 = d.dot(d);
                if (d2 < bestDist2) { bestDist2 = d2; bestIdx = n; }
            }
            if (bestIdx >= 0) {
                SbVec3f dir = pa - tree->nodePositions[bestIdx];
                float len = dir.length();
                if (len > 1e-5f) dir /= len;
                accum[bestIdx] += dir;
                counts[bestIdx] += 1;
                numActiveAttractors++;
            }
        }

        // Grow new nodes (limit to frontier only for efficiency)
        std::vector<int> newNodeIndices;
        SbVec3f trop(0.0f, tropismUp, 0.0f);
        if (tree->frontierIndices.empty()) {
            // If no explicit frontier, consider the last N nodes as frontier
            int window = numNodes > 32 ? 32 : numNodes;
            tree->frontierIndices.reserve(window);
            for (int k = numNodes - window; k < numNodes; ++k) tree->frontierIndices.push_back(k);
        }

        for (int fi = 0; fi < (int)tree->frontierIndices.size(); ++fi) {
            int n = tree->frontierIndices[fi];
            if (counts[n] > 0) {
                SbVec3f g = accum[n];
                if (g.length() > 1e-5f) g.normalize();
                // Bias with previous growth dir and tropism to reduce squiggle
                g = (g * 0.6f) + (tree->nodeDirs[n] * 0.3f) + (trop * 0.1f);
                if (g.length() > 1e-5f) g.normalize();
                
                // Auto-slowdown as we approach caps (more aggressive)
                float fullness = (float)numNodes / (float)maxNodes; // 0..1
                int adaptiveForks = maxForksPerTip;
                if (fullness > 0.3f) adaptiveForks = 2;     // reduce forking earlier
                if (fullness > 0.5f) adaptiveForks = 1;     // single branch only at 50%
                if (fullness > 0.8f) adaptiveForks = 0;     // stop growth at 80%
                
                // More aggressive step reduction
                float adaptiveStep = stepLength;
                if (fullness > 0.4f) adaptiveStep *= 0.8f;  // slow down at 40%
                if (fullness > 0.6f) adaptiveStep *= 0.6f;  // slower at 60%
                if (fullness > 0.8f) adaptiveStep *= 0.3f;  // very slow at 80%

                // Early exit if growth is stopped
                if (adaptiveForks <= 0) continue;
                
                // Compute up to N fork directions around g
                int forks = adaptiveForks;
                if (forks < 1) forks = 1; if (forks > 3) forks = 3;
                float angleRad = forkAngleDeg * (float)M_PI / 180.0f;
                // Build an orthonormal basis (g is forward)
                SbVec3f forward = g;
                SbVec3f helper(0.0f, 1.0f, 0.0f);
                if (fabsf(helper.dot(forward)) > 0.95f) helper = SbVec3f(1.0f, 0.0f, 0.0f);
                SbVec3f right = forward.cross(helper); right.normalize();
                SbVec3f upv = right.cross(forward); upv.normalize();

                // Candidate fork directions
                SbVec3f cand[3]; int candCount = 0;
                if (forks == 1) {
                    cand[candCount++] = forward;
                } else if (forks == 2) {
                    cand[candCount++] = rotateAroundAxis(forward, upv, angleRad);
                    cand[candCount++] = rotateAroundAxis(forward, upv, -angleRad);
                } else {
                    cand[candCount++] = forward;
                    cand[candCount++] = rotateAroundAxis(forward, upv, angleRad);
                    cand[candCount++] = rotateAroundAxis(forward, upv, -angleRad);
                }

                for (int c = 0; c < candCount; ++c) {
                    SbVec3f newPos = tree->nodePositions[n] + cand[c] * adaptiveStep;

                    // Prevent too-close nodes
                    bool tooClose = false;
                    // Only check recent nodes for performance
                    int startIdx = (int)tree->nodePositions.size() - 256;
                    if (startIdx < 0) startIdx = 0;
                    for (int k = startIdx; k < (int)tree->nodePositions.size(); ++k) {
                        SbVec3f dd = newPos - tree->nodePositions[k];
                        if (dd.dot(dd) < minNodeSpacing * minNodeSpacing) { tooClose = true; break; }
                    }
                    if (!tooClose) {
                        tree->nodePositions.push_back(newPos);
                        tree->parentIndex.push_back(n);
                        tree->nodeDirs.push_back(cand[c]);
                        newNodeIndices.push_back((int)tree->nodePositions.size() - 1);
                    }
                }
            }
        }

        if (newNodeIndices.empty()) {
            // Fallback: extend trunk upward a step to reach attractors (only if not too dense)
            int last = (int)tree->nodePositions.size() - 1;
            if (last >= 0 && numNodes < maxNodes * 0.7f) {  // only allow fallback before 70% full
                SbVec3f newPos = tree->nodePositions[last] + SbVec3f(0.0f, stepLength, 0.0f);
                tree->nodePositions.push_back(newPos);
                tree->parentIndex.push_back(last);
                tree->nodeDirs.push_back(SbVec3f(0.0f, 1.0f, 0.0f));
                newNodeIndices.push_back((int)tree->nodePositions.size() - 1);
            } else {
                // No new nodes possible - stop growth
                printf("[TreeStudio] Growth stopped: no viable expansion\n");
                tree->growthPaused = true;
                break;
            }
        }

        // Update frontier to newly created nodes unless paused
        if (!tree->growthPaused) tree->frontierIndices.swap(newNodeIndices);

        // Remove attractors close to any newly created node
        std::vector<SbVec3f> remaining;
        remaining.reserve(tree->attractors.size());
        for (int a = 0; a < (int)tree->attractors.size(); ++a) {
            SbVec3f pa = tree->attractors[a];
            bool consumed = false;
            for (int idx = 0; idx < (int)newNodeIndices.size(); ++idx) {
                int ni = newNodeIndices[idx];
                SbVec3f d = pa - tree->nodePositions[ni];
                if (d.dot(d) <= killDistance * killDistance) { consumed = true; break; }
            }
            if (!consumed) remaining.push_back(pa);
        }
        tree->attractors.swap(remaining);

        if (numActiveAttractors == 0 || tree->attractors.empty()) break;

        // If paused, break out of further steps
        if (tree->growthPaused) break;

        // Debug log when nearing caps (once per iteration step)
        if (numNodes > (int)(0.9f * maxNodes)) {
            printf("[TreeStudio] Near node cap: %d/%d\n", numNodes, maxNodes);
        }
    }
}


