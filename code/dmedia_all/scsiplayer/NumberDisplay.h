#ifndef _numberDisplay_h
#define _numberDisplay_h

#include <VkComponent.h>
#include <AvTime.h>
#include <AvInt.h>

//
//	This is a utility class the is used by Cdplayer/Datplayer to display
//	program time/number/title etc.
//	It creates two label widgets (the right hand side one in a frame)
//	inside a form a wiget.
//

#define AV_INT -1

// NB: Not all of these are implemented yet.
//
enum LabelLocation {LabelOnLeft, LableOnTop, LableOnRight, LabelOnBottom, NoLabel};

class NumberDisplay : public VkComponent
{
public:
  NumberDisplay(const char* name,
		Widget parent, LabelLocation whereLabel,
		int timeType,	
		VkCallbackMethod timeChangedFunc,
		VkCallbackMethod selectionChangedFunc = NULL);
  ~NumberDisplay();
  const char* className();

  AvTime* avTime() { return _avTime; };
  AvInt* avInt() { return _avInt; };

  void display(long long time, int timescale);
  void display(int);
  void setLabel(char*);

private:

  void nullCallback(VkCallbackObject *, void*, void*);

  AvTime* _avTime;
  AvInt* _avInt;
  Widget _labelWidget;

};

#endif
