#ifndef OVERLAY_H
#define OVERLAY_H

#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoSeparator.h>

// Forward declaration
class DialHandler;

// Button box layout: 6 rows, variable columns
extern const int rowCols[6];

struct ButtonBoxPos { 
    int row, col; 
};

// Map each button index (0-31) to its (row, col) in the grid
extern const ButtonBoxPos buttonBoxMap[32];

// Initialize the overlay system
void initOverlay(SoXtExaminerViewer* viewer);

// Update the overlay scene graph based on current state
void updateOverlaySceneGraph(SoXtExaminerViewer* viewer, int currentSetIndex, bool overlayVisible, DialHandler* currentHandler, bool stereoEnabled = false);

// Cleanup overlay resources
void cleanupOverlay();

#endif // OVERLAY_H 
