#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/sensors/SoNodeSensor.h>

// Sensor callback function:
static void
rootChangedCB(void *, SoSensor *s)
{
   // We know the sensor is really a data sensor:
   SoDataSensor *mySensor = (SoDataSensor *)s;
   
   SoNode *changedNode = mySensor->getTriggerNode();
   SoField *changedField = mySensor->getTriggerField();
   
   printf("The node named '%s' changed\n",
            changedNode->getName().getString());

   if (changedField != NULL) {
      SbName fieldName;
      changedNode->getFieldName(changedField, fieldName);
      printf(" (field %s)\n", fieldName.getString());
   }
   else
      printf(" (no fields changed)\n");
}

main(int, char **)
{
   SoDB::init();

   SoSeparator *root = new SoSeparator;
   root->ref();
   root->setName("Root");

   SoCube *myCube = new SoCube;
   root->addChild(myCube);
   myCube->setName("MyCube");

   SoSphere *mySphere = new SoSphere;
   root->addChild(mySphere);
   mySphere->setName("MySphere");

   SoNodeSensor *mySensor = new SoNodeSensor;

   mySensor->setPriority(0);
   mySensor->setFunction(rootChangedCB);
   mySensor->attach(root);

   // Now, make a few changes to the scene graph; the sensor's
   // callback function will be called immediately after each
   // change.
   myCube->width = 1.0;
   myCube->height = 2.0;
   mySphere->radius = 3.0;
   root->removeChild(mySphere);
}
