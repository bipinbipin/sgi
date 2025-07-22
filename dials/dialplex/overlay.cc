#include "overlay.h"
#include "dialhandler.h"
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/SbString.h>

// Global overlay state
static SoSeparator *overlayRoot = NULL;

// Button box layout: 6 rows, variable columns
const int rowCols[6] = {4, 6, 6, 6, 6, 4};

// Map each button index (0-31) to its (row, col) in the grid
const ButtonBoxPos buttonBoxMap[32] = {
    // Row 0 (4 cols)
    {0,0},{0,1},{0,2},{0,3},
    // Row 1 (6 cols)
    {1,0},{1,1},{1,2},{1,3},{1,4},{1,5},
    // Row 2 (6 cols)
    {2,0},{2,1},{2,2},{2,3},{2,4},{2,5},
    // Row 3 (6 cols)
    {3,0},{3,1},{3,2},{3,3},{3,4},{3,5},
    // Row 4 (6 cols)
    {4,0},{4,1},{4,2},{4,3},{4,4},{4,5},
    // Row 5 (4 cols)
    {5,0},{5,1},{5,2},{5,3}
};

void initOverlay(SoXtExaminerViewer* viewer)
{
    // Initialize overlay root if needed
    if (!overlayRoot) {
        overlayRoot = new SoSeparator;
        overlayRoot->ref();
    }
}

void updateOverlaySceneGraph(SoXtExaminerViewer* viewer, int currentSetIndex, bool overlayVisible, DialHandler* currentHandler)
{
    if (!viewer) return;
    
    if (overlayRoot) overlayRoot->unref();
    overlayRoot = new SoSeparator;
    overlayRoot->ref();
    
    if (!overlayVisible) {
        viewer->setOverlaySceneGraph(overlayRoot);
        return;
    }
    
    // Get window size (default to 1280x1024 if not available)
    SbVec2s winSize(1280, 1024);
    winSize = viewer->getSize();
    float winW = winSize[0];
    float winH = winSize[1];
    
    // Camera for overlay
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(winW/2, winH/2, 5);
    cam->height.setValue(winH);
    cam->nearDistance.setValue(1);
    cam->farDistance.setValue(10);
    overlayRoot->addChild(cam);

    // Grid parameters - centered on screen with more padding
    float cellSize = 80.0f;
    float gridWidth = 6 * cellSize; // max 6 columns
    float gridHeight = 6 * cellSize; // 6 rows
    float startX = (winW - gridWidth) / 2.0f;
    float startY = (winH - gridHeight) / 2.0f + 80.0f;

    // Draw button grid using line thickness to show state
    for (int btnIdx = 0; btnIdx < 32; ++btnIdx) {
        int row = buttonBoxMap[btnIdx].row;
        int col = buttonBoxMap[btnIdx].col;
        int cols = rowCols[row];
        float rowWidth = cols * cellSize;
        float rowStartX = startX + (gridWidth - rowWidth) / 2.0f;
        float x = rowStartX + col * cellSize;
        float y = startY + (5 - row) * cellSize; // flip Y
        
        // Use line thickness to show button state
        SoDrawStyle *style = new SoDrawStyle;
        if (btnIdx == currentSetIndex) {
            style->lineWidth.setValue(8.0f); // thick when pressed
        } else {
            style->lineWidth.setValue(2.0f); // thin when not pressed
        }
        overlayRoot->addChild(style);
        
        // Draw square outline
        SoCoordinate3 *coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f(x, y, 0));
        coords->point.set1Value(1, SbVec3f(x+cellSize, y, 0));
        coords->point.set1Value(2, SbVec3f(x+cellSize, y+cellSize, 0));
        coords->point.set1Value(3, SbVec3f(x, y+cellSize, 0));
        coords->point.set1Value(4, SbVec3f(x, y, 0));
        overlayRoot->addChild(coords);
        
        SoLineSet *lines = new SoLineSet;
        lines->numVertices.set1Value(0, 5);
        overlayRoot->addChild(lines);
        
        // Add an X pattern if button is pressed
        if (btnIdx == currentSetIndex) {
            SoCoordinate3 *xCoords = new SoCoordinate3;
            xCoords->point.set1Value(0, SbVec3f(x+10, y+10, 0));
            xCoords->point.set1Value(1, SbVec3f(x+cellSize-10, y+cellSize-10, 0));
            xCoords->point.set1Value(2, SbVec3f(x+cellSize-10, y+10, 0));
            xCoords->point.set1Value(3, SbVec3f(x+10, y+cellSize-10, 0));
            overlayRoot->addChild(xCoords);
            
            SoLineSet *xLines = new SoLineSet;
            xLines->numVertices.set1Value(0, 2);
            xLines->numVertices.set1Value(1, 2);
            overlayRoot->addChild(xLines);
        }
    }

    // Left side text overlay - Dynamic dial descriptions from current handler
    SoSeparator *leftSep = new SoSeparator;
    SoFont *leftFont = new SoFont;
    leftFont->name.setValue("Courier");
    leftFont->size.setValue(12.0f);
    leftSep->addChild(leftFont);
    
    SoTransform *leftTextPos = new SoTransform;
    leftTextPos->translation.setValue(30, winH - 50, 0);
    leftSep->addChild(leftTextPos);
    
    // Set text color to white
    SoBaseColor *leftTextColor = new SoBaseColor;
    leftTextColor->rgb.setValue(1.0f, 1.0f, 1.0f); // White text
    leftSep->addChild(leftTextColor);
    
    SoText2 *leftText = new SoText2;
    leftText->string.set1Value(0, "DIAL FUNCTION MATRIX");
    leftText->string.set1Value(1, "====================");
    
    // Show current theme description
    if (currentHandler && currentSetIndex >= 0) {
        SbString setInfo("ACTIVE SET: ");
        setInfo += SbString(currentSetIndex);
        leftText->string.set1Value(2, setInfo.getString());
        leftText->string.set1Value(3, currentHandler->getThemeDescription());
    } else {
        leftText->string.set1Value(2, "ACTIVE SET: NONE");
        leftText->string.set1Value(3, "No handler active");
    }
    leftText->string.set1Value(4, "");
    
    // Show dynamic dial descriptions
    int lineIndex = 5;
    for (int i = 0; i < 8; i++) {
        char dialHeader[32];
        sprintf(dialHeader, " .--.  DIAL %d", i);
        leftText->string.set1Value(lineIndex++, dialHeader);
        
        if (currentHandler) {
            const char* desc = currentHandler->getDialDescription(i);
            // Split the description into lines for better formatting
            char shortDesc[64];
            strncpy(shortDesc, desc, 63);
            shortDesc[63] = '\0';
            
            // Find the colon to split title from description
            char* colonPos = strchr(shortDesc, ':');
            if (colonPos && strlen(colonPos) > 2) {
                *colonPos = '\0'; // terminate at colon
                char line1[64], line2[64];
                sprintf(line1, "|%s|", shortDesc + 7); // skip "DIAL X: "
                sprintf(line2, "|%s|", colonPos + 2); // skip ": "
                leftText->string.set1Value(lineIndex++, line1);
                leftText->string.set1Value(lineIndex++, line2);
            } else {
                char line[64];
                sprintf(line, "|%s|", shortDesc + 7); // skip "DIAL X: "
                leftText->string.set1Value(lineIndex++, line);
                leftText->string.set1Value(lineIndex++, "| |");
            }
        } else {
            leftText->string.set1Value(lineIndex++, "|No handler|");
            leftText->string.set1Value(lineIndex++, "| |");
        }
        leftText->string.set1Value(lineIndex++, " '--'");
        leftText->string.set1Value(lineIndex++, "");
    }
    
    leftSep->addChild(leftText);
    overlayRoot->addChild(leftSep);

    // Right side text overlay - Project documentation and cassette futurism
    SoSeparator *rightSep = new SoSeparator;
    SoFont *rightFont = new SoFont;
    rightFont->name.setValue("Courier");
    rightFont->size.setValue(12.0f);
    rightSep->addChild(rightFont);
    
    SoTransform *rightTextPos = new SoTransform;
    rightTextPos->translation.setValue(winW - 380, winH - 50, 0);
    rightSep->addChild(rightTextPos);
    
    // Set text color to white
    SoBaseColor *rightTextColor = new SoBaseColor;
    rightTextColor->rgb.setValue(1.0f, 1.0f, 1.0f); // White text
    rightSep->addChild(rightTextColor);
    
    SoText2 *rightText = new SoText2;
    rightText->string.set1Value(0, "DIALPLEX PROJECT OVERVIEW");
    rightText->string.set1Value(1, "==========================");
    if (currentSetIndex >= 0) {
        SbString btnText("ACTIVE SET: ");
        btnText += SbString(currentSetIndex + 1);
        rightText->string.set1Value(2, btnText.getString());
        rightText->string.set1Value(3, "STATUS: ENGAGED");
    } else {
        rightText->string.set1Value(2, "ACTIVE SET: STANDBY");
        rightText->string.set1Value(3, "STATUS: MONITORING");
    }
    rightText->string.set1Value(4, "");
    rightText->string.set1Value(5, "PROJECT ARCHITECTURE:");
    rightText->string.set1Value(6, "====================");
    rightText->string.set1Value(7, "This system demonstrates a modular");
    rightText->string.set1Value(8, "dial control interface using SGI");
    rightText->string.set1Value(9, "Open Inventor and hardware button");
    rightText->string.set1Value(10, "box input. The core innovation is");
    rightText->string.set1Value(11, "breaking dial functionality into");
    rightText->string.set1Value(12, "32 distinct function sets, each");
    rightText->string.set1Value(13, "triggered by a physical button.");
    rightText->string.set1Value(14, "");
    rightText->string.set1Value(15, "IMPLEMENTATION DETAILS:");
    rightText->string.set1Value(16, "========================");
    rightText->string.set1Value(17, "Eight dials control different");
    rightText->string.set1Value(18, "aspects of the 3D scene: camera");
    rightText->string.set1Value(19, "rotation, zoom, object transforms,");
    rightText->string.set1Value(20, "material properties, and visual");
    rightText->string.set1Value(21, "effects. Button presses activate");
    rightText->string.set1Value(22, "this overlay interface, providing");
    rightText->string.set1Value(23, "real-time feedback and function");
    rightText->string.set1Value(24, "reference. The result: 256 unique");
    rightText->string.set1Value(25, "control combinations (32 x 8).");
    rightText->string.set1Value(26, "");
    rightText->string.set1Value(27, "SYSTEM STATUS:");
    rightText->string.set1Value(28, "===============");
    rightText->string.set1Value(29, " DIALS: [||||||||] 8/8 ACTIVE");
    rightText->string.set1Value(30, " BTNS:  [||||||||] 32/32 READY");
    rightText->string.set1Value(31, " OVLY:  [||||||||] FUNCTIONAL");
    rightText->string.set1Value(32, " REND:  [||||||||] 60 FPS");
    rightText->string.set1Value(33, "");
    rightText->string.set1Value(34, "CASSETTE FUTURISM MODE: ACTIVE");
    rightText->string.set1Value(35, "RETRO INTERFACE: ENGAGED");
    rightText->string.set1Value(36, "NEURAL LINK: ESTABLISHED");
    
    rightSep->addChild(rightText);
    overlayRoot->addChild(rightSep);

    // Bottom center - Additional futuristic elements
    SoSeparator *bottomSep = new SoSeparator;
    SoFont *bottomFont = new SoFont;
    bottomFont->name.setValue("Courier");
    bottomFont->size.setValue(13.0f);
    bottomSep->addChild(bottomFont);
    
    SoTransform *bottomTextPos = new SoTransform;
    bottomTextPos->translation.setValue(winW/2 - 350, 100, 0);
    bottomSep->addChild(bottomTextPos);
    
    // Set text color to white
    SoBaseColor *bottomTextColor = new SoBaseColor;
    bottomTextColor->rgb.setValue(1.0f, 1.0f, 1.0f); // White text
    bottomSep->addChild(bottomTextColor);
    
    SoText2 *bottomText = new SoText2;
    bottomText->string.set1Value(0, "CYBERDYNE NEURAL NET PROCESSOR - 256 FUNCTION COMBINATIONS");
    bottomText->string.set1Value(1, "================================================================");
    bottomText->string.set1Value(2, "PRESS ANY BUTTON TO ACTIVATE FUNCTION SET | RELEASE TO RETURN TO STANDBY");
    bottomSep->addChild(bottomText);
    overlayRoot->addChild(bottomSep);

    viewer->setOverlaySceneGraph(overlayRoot);
}

void cleanupOverlay()
{
    if (overlayRoot) {
        overlayRoot->unref();
        overlayRoot = NULL;
    }
} 