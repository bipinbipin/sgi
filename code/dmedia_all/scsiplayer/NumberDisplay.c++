#include <stdio.h>
#include <string.h>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/Label.h>
#include <Xm/RowColumn.h>
#include <Sgm/SpringBox.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/TextF.h>
#include <VkResource.h>

#include "NumberDisplay.h"


NumberDisplay::NumberDisplay(const char* name,
			     Widget parent, 
			     LabelLocation whereLabel, 
			     int timeType,
			     VkCallbackMethod timeChangedFunc,
			     VkCallbackMethod selectionChangedFunc)
    : VkComponent(name)
{

  _baseWidget = XtVaCreateManagedWidget(name, xmFormWidgetClass,
					parent,
					NULL ); 

  Widget numberDisplaySpringBox =
    XtVaCreateManagedWidget("numberDisplaySpringBox", 
			    sgSpringBoxWidgetClass,
			    _baseWidget, 
			    XmNorientation, XmHORIZONTAL,
			    NULL);
    
  if (whereLabel != NoLabel)
    _labelWidget = XtVaCreateManagedWidget("numberDisplayLabel", xmLabelWidgetClass,
					   _baseWidget,
					   XmNalignment, XmALIGNMENT_CENTER,
					   NULL);
  // figure out what goes where.
  switch (whereLabel)
    {
    case LabelOnBottom:
      XtVaSetValues(numberDisplaySpringBox,
		    XmNtopAttachment, XmATTACH_FORM,
		    XmNleftAttachment, XmATTACH_FORM,
		    XmNrightAttachment, XmATTACH_FORM,
		    NULL); 
      XtVaSetValues(_labelWidget,
		    XmNbottomAttachment, XmATTACH_FORM,
		    XmNtopAttachment, XmATTACH_WIDGET,
		    XmNtopWidget, numberDisplaySpringBox,
		    XmNleftAttachment, XmATTACH_FORM,
		    XmNrightAttachment, XmATTACH_FORM,
		    NULL);
      break;
    case LabelOnLeft:
      XtVaSetValues(numberDisplaySpringBox,
		    XmNtopAttachment, XmATTACH_FORM,
		    XmNrightAttachment, XmATTACH_FORM,
		    XmNbottomAttachment, XmATTACH_FORM,
		    NULL); 
      XtVaSetValues(_labelWidget,
		    XmNleftAttachment, XmATTACH_FORM,
		    XmNtopAttachment, XmATTACH_FORM,
		    XmNrightAttachment, XmATTACH_WIDGET,
		    XmNrightWidget, numberDisplaySpringBox,
		    XmNbottomAttachment, XmATTACH_FORM,
		    NULL);
    }

    
  if (!timeChangedFunc)
    timeChangedFunc = (VkCallbackMethod)NumberDisplay::nullCallback;

  switch (timeType) {

  case AV_DIGITALCLOCKTIME:
    _avTime = new AvTime("numDisplayAvTime", numberDisplaySpringBox,
			 this, NULL,
			 timeChangedFunc,
			 NULL, NULL, NULL, False,
			 AvTIME_DIGITALCLOCKTIME_BITMASK);
    _avTime->setTimeUnitToDigitalClockTime();
    _avTime->show();
    break;

  case AV_NORMALTIME:
    _avTime = new AvTime("numDisplayAvTime", numberDisplaySpringBox,
			 this, NULL,
			 timeChangedFunc,
			 selectionChangedFunc,
			 NULL, NULL, True,
			 AvTIME_NORMALTIME_BITMASK);
    _avTime->setTimeUnitToNormalTime();
    _avTime->show();
    break;

  case AV_INT:
    _avInt = new AvInt("numDisplayAvInt", numberDisplaySpringBox,
		       this, NULL,
		       2, timeChangedFunc,
		       True);
    _avInt->show();
    break;
  }

  installDestroyHandler();

}


const char* NumberDisplay::className()
{
  return ("NumberDisplay");
} 


NumberDisplay::~NumberDisplay()
{
  if (_avTime)
   delete _avTime;
  if (_avInt)
   delete _avInt;
}


void NumberDisplay::display(int newnum)
{
  if (!_avInt->isEditing())
    _avInt->setInt(newnum);
}

void NumberDisplay::display(long long time, int timescale)
{
  if (!_avTime->isEditing())
    _avTime->setTime(time, timescale);
}


void NumberDisplay::setLabel(char* label)
{
char string[200];
  sprintf(string, "%s", label); 
  XmString xmstr =  XmStringLtoRCreate(string, XmSTRING_DEFAULT_CHARSET);
  XtVaSetValues(_labelWidget, XmNlabelString, xmstr, NULL);
  XmStringFree(xmstr);
}

void NumberDisplay::nullCallback(VkCallbackObject *, void*, void*)
{
}
