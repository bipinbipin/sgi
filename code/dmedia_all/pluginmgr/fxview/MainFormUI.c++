/////////////////////////////////////////////////////////////
//
// Source file for MainFormUI
//
//    This class implements the user interface created in 
//    RapidApp.
//
//    Restrict changes to those sections between
//    the "//--- Start/End editable code block" markers
//
//    This will allow RapidApp to integrate changes more easily
//
//    This class is a ViewKit user interface "component".
//    For more information on how components are used, see the
//    "ViewKit Programmers' Manual", and the RapidApp
//    User's Guide.
//
//
/////////////////////////////////////////////////////////////


#include "MainFormUI.h" // Generated header file for this class

#include <GL/GLwMDrawA.h> 
#include <Xm/Form.h> 
#include <Xm/Frame.h> 
#include <Xm/Label.h> 
#include <Xm/List.h> 
#include <Xm/PushB.h> 
#include <Xm/RowColumn.h> 
#include <Xm/Scale.h> 
#include <Xm/ScrolledW.h> 
#include <Xm/ToggleB.h> 
#include <Vk/VkResource.h>
//---- Start editable code block: headers and declarations


//---- End editable code block: headers and declarations



// These are default resources for widgets in objects of this class
// All resources will be prepended by *<name> at instantiation,
// where <name> is the name of the specific instance, as well as the
// name of the baseWidget. These are only defaults, and may be overriden
// in a resource file by providing a more specific resource name

String  MainFormUI::_defaultMainFormUIResources[] = {
        "*customizeButton.labelString:  Customize Effect...",
        "*destAlphaLabel.labelString:  Destination Alpha:",
        "*frameSlider.titleString:  Frame",
        "*label1.labelString:  Special Effects:",
        "*label2.labelString:  Destination:",
        "*sourceAButton.labelString:  Load Source A",
        "*sourceBButton.labelString:  Load Source B",
        "*toggle1.labelString:  Filter",
        "*toggle2.labelString:  Transition",
        //---- Start editable code block: MainFormUI Default Resources


        //---- End editable code block: MainFormUI Default Resources
        (char*)NULL
};

MainFormUI::MainFormUI(const char *name) : VkComponent(name) 
{ 
    // No widgets are created by this constructor.
    // If an application creates a component using this constructor,
    // It must explictly call create at a later time.
    // This is mostly useful when adding pre-widget creation
    // code to a derived class constructor.

    //---- Start editable code block: MainForm constructor 2


    //---- End editable code block: MainForm constructor 2


} // End Constructor




MainFormUI::MainFormUI(const char *name, Widget parent) : VkComponent(name) 
{ 
    //---- Start editable code block: MainForm pre-create


    //---- End editable code block: MainForm pre-create



    // Call creation function to build the widget tree.

     create(parent);

    //---- Start editable code block: MainForm constructor


    //---- End editable code block: MainForm constructor


} // End Constructor


MainFormUI::~MainFormUI() 
{
    // Base class destroys widgets

    //---- Start editable code block: MainFormUI destructor


    //---- End editable code block: MainFormUI destructor
} // End destructor



void MainFormUI::create ( Widget parent )
{
    // Load any class-defaulted resources for this object

    setDefaultResources(parent, _defaultMainFormUIResources  );


    // Create an unmanaged widget as the top of the widget hierarchy

    _baseWidget = _mainForm = XtVaCreateWidget ( _name,
                                                 xmFormWidgetClass,
                                                 parent, 
                                                 XmNresizePolicy, XmRESIZE_GROW, 
                                                 (XtPointer) NULL ); 

    // install a callback to guard against unexpected widget destruction

    installDestroyHandler();


    // Create widgets used in this component
    // All variables are data members of this class

    _effectsForm = XtVaCreateManagedWidget  ( "effectsForm",
                                               xmFormWidgetClass,
                                               _baseWidget, 
                                               XmNresizePolicy, XmRESIZE_GROW, 
                                               XmNtopAttachment, XmATTACH_FORM, 
                                               XmNbottomAttachment, XmATTACH_FORM, 
                                               XmNleftAttachment, XmATTACH_FORM, 
                                               XmNrightAttachment, XmATTACH_FORM, 
                                               XmNtopPosition, 0, 
                                               XmNbottomPosition, 0, 
                                               XmNrightPosition, 0, 
                                               XmNtopOffset, 10, 
                                               XmNbottomOffset, 15, 
                                               XmNleftOffset, 20, 
                                               XmNrightOffset, 10, 
                                               XmNwidth, 758, 
                                               XmNheight, 474, 
                                               (XtPointer) NULL ); 


    _customizeButton = XtVaCreateManagedWidget  ( "customizeButton",
                                           xmPushButtonWidgetClass,
                                           _effectsForm, 
                                           XmNlabelType, XmSTRING, 
                                           XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                           XmNbottomAttachment, XmATTACH_NONE, 
                                           XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                           XmNrightAttachment, XmATTACH_NONE, 
                                           XmNtopPosition, 0, 
                                           XmNleftPosition, 0, 
                                           XmNtopOffset, 0, 
                                           XmNbottomOffset, 0, 
                                           XmNleftOffset, 0, 
                                           XmNrightOffset, 0, 
                                           XmNwidth, 134, 
                                           XmNheight, 30, 
                                           (XtPointer) NULL ); 

    XtAddCallback ( _customizeButton,
                    XmNactivateCallback,
                    &MainFormUI::doSetupCallback,
                    (XtPointer) this ); 


    _destAlphaLabel = XtVaCreateManagedWidget  ( "destAlphaLabel",
                                                  xmLabelWidgetClass,
                                                  _effectsForm, 
                                                  XmNlabelType, XmSTRING, 
                                                  XmNrecomputeSize, True, 
                                                  XmNtopAttachment, XmATTACH_NONE, 
                                                  XmNbottomAttachment, XmATTACH_WIDGET, 
                                                  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                                  XmNrightAttachment, XmATTACH_NONE, 
                                                  XmNtopPosition, 0, 
                                                  XmNbottomPosition, 0, 
                                                  XmNleftPosition, 0, 
                                                  XmNtopOffset, 0, 
                                                  XmNbottomOffset, 0, 
                                                  XmNleftOffset, 0, 
                                                  XmNrightOffset, 0, 
                                                  XmNwidth, 133, 
                                                  XmNheight, 20, 
                                                  (XtPointer) NULL ); 


    _alphaGLFrame = XtVaCreateManagedWidget  ( "alphaGLFrame",
                                                  xmFrameWidgetClass,
                                                  _effectsForm, 
                                                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                                  XmNbottomAttachment, XmATTACH_NONE, 
                                                  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                                  XmNrightAttachment, XmATTACH_NONE, 
                                                  XmNtopPosition, 0, 
                                                  XmNleftPosition, 0, 
                                                  XmNtopOffset, 0, 
                                                  XmNbottomOffset, 0, 
                                                  XmNleftOffset, 0, 
                                                  XmNrightOffset, 0, 
                                                  XmNwidth, 246, 
                                                  XmNheight, 186, 
                                                  (XtPointer) NULL ); 


    _alpha_GLWidget = XtVaCreateManagedWidget  ( "alpha_GLWidget",
                                                    glwMDrawingAreaWidgetClass,
                                                  _alphaGLFrame, 
                                                    GLwNrgba, True, 
                                                    GLwNredSize, 1, 
                                                    GLwNgreenSize, 1, 
                                                    GLwNblueSize, 1, 
                                                    GLwNallocateBackground, True, 
                                                    XmNwidth, 240, 
                                                    XmNheight, 180, 
                                                    (XtPointer) NULL ); 

    XtAddCallback ( _alpha_GLWidget,
                    GLwNinputCallback,
                    &MainFormUI::glInputCallback,
                    (XtPointer) this ); 

    _label1 = XtVaCreateManagedWidget  ( "label1",
                                          xmLabelWidgetClass,
                                          _effectsForm, 
                                          XmNlabelType, XmSTRING, 
                                          XmNrecomputeSize, True, 
                                          XmNtopAttachment, XmATTACH_NONE, 
                                          XmNbottomAttachment, XmATTACH_WIDGET, 
                                          XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                          XmNrightAttachment, XmATTACH_NONE, 
                                          XmNtopPosition, 0, 
                                          XmNbottomPosition, 0, 
                                          XmNleftPosition, 0, 
                                          XmNtopOffset, 0, 
                                          XmNbottomOffset, 0, 
                                          XmNleftOffset, 0, 
                                          XmNrightOffset, 0, 
                                          XmNwidth, 110, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label2 = XtVaCreateManagedWidget  ( "label2",
                                          xmLabelWidgetClass,
                                          _effectsForm, 
                                          XmNlabelType, XmSTRING, 
                                          XmNrecomputeSize, True, 
                                          XmNtopAttachment, XmATTACH_NONE, 
                                          XmNbottomAttachment, XmATTACH_WIDGET, 
                                          XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                          XmNrightAttachment, XmATTACH_NONE, 
                                          XmNtopPosition, 0, 
                                          XmNbottomPosition, 0, 
                                          XmNleftPosition, 0, 
                                          XmNtopOffset, 0, 
                                          XmNbottomOffset, 0, 
                                          XmNleftOffset, 0, 
                                          XmNrightOffset, 0, 
                                          XmNwidth, 89, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _frameSlider = XtVaCreateManagedWidget  ( "frameSlider",
                                               xmScaleWidgetClass,
                                               _effectsForm, 
                                               XmNslidingMode, XmSLIDER, 
                                               SgNslanted, False, 
                                               XmNvalue, 1, 
                                               XmNmaximum, 30, 
                                               XmNminimum, 1, 
                                               XmNorientation, XmHORIZONTAL, 
                                               XmNshowValue, True, 
                                               XmNtopAttachment, XmATTACH_NONE, 
                                               XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                               XmNleftAttachment, XmATTACH_FORM, 
                                               XmNrightAttachment, XmATTACH_FORM, 
                                               XmNbottomWidget, _alphaGLFrame, 
                                               XmNtopPosition, 0, 
                                               XmNbottomPosition, 0, 
                                               XmNleftPosition, 0, 
                                               XmNrightPosition, 0, 
                                               XmNtopOffset, 0, 
                                               XmNbottomOffset, 0, 
                                               XmNleftOffset, 548, 
                                               XmNrightOffset, 20, 
                                               XmNwidth, 174, 
                                               XmNheight, 54, 
                                               (XtPointer) NULL ); 

    XtAddCallback ( _frameSlider,
                    XmNvalueChangedCallback,
                    &MainFormUI::doFrameSliderCallback,
                    (XtPointer) this ); 

    XtAddCallback ( _frameSlider,
                    XmNdragCallback,
                    &MainFormUI::doFrameSliderCallback,
                    (XtPointer) this ); 


    _sourceBButton = XtVaCreateManagedWidget  ( "sourceBButton",
                                                 xmPushButtonWidgetClass,
                                                 _effectsForm, 
                                                 XmNlabelType, XmSTRING, 
                                                 XmNtopAttachment, XmATTACH_WIDGET, 
                                                 XmNbottomAttachment, XmATTACH_NONE, 
                                                 XmNleftAttachment, XmATTACH_FORM, 
                                                 XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                                 XmNtopPosition, 0, 
                                                 XmNleftPosition, 0, 
                                                 XmNrightPosition, 0, 
                                                 XmNtopOffset, -2, 
                                                 XmNbottomOffset, 0, 
                                                 XmNleftOffset, 0, 
                                                 XmNrightOffset, 0, 
                                                 XmNwidth, 246, 
                                                 XmNheight, 32, 
                                                 (XtPointer) NULL ); 

    XtAddCallback ( _sourceBButton,
                    XmNactivateCallback,
                    &MainFormUI::loadSourceBCallback,
                    (XtPointer) this ); 


    _sourceAButton = XtVaCreateManagedWidget  ( "sourceAButton",
                                                 xmPushButtonWidgetClass,
                                                 _effectsForm, 
                                                 XmNlabelType, XmSTRING, 
                                                 XmNtopAttachment, XmATTACH_WIDGET, 
                                                 XmNbottomAttachment, XmATTACH_NONE, 
                                                 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                                 XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                                 XmNtopPosition, 0, 
                                                 XmNleftPosition, 0, 
                                                 XmNrightPosition, 0, 
                                                 XmNtopOffset, -2, 
                                                 XmNbottomOffset, 0, 
                                                 XmNleftOffset, 0, 
                                                 XmNrightOffset, 0, 
                                                 XmNwidth, 246, 
                                                 XmNheight, 32, 
                                                 (XtPointer) NULL ); 

    XtAddCallback ( _sourceAButton,
                    XmNactivateCallback,
                    &MainFormUI::loadSourceACallback,
                    (XtPointer) this ); 


    _radiobox = XtVaCreateManagedWidget  ( "radiobox",
                                            xmRowColumnWidgetClass,
                                            _effectsForm, 
                                            XmNorientation, XmHORIZONTAL, 
                                            XmNpacking, XmPACK_COLUMN, 
                                            XmNradioBehavior, True, 
                                            XmNradioAlwaysOne, True, 
                                            XmNtopAttachment, XmATTACH_WIDGET, 
                                            XmNbottomAttachment, XmATTACH_NONE, 
                                            XmNleftAttachment, XmATTACH_WIDGET, 
                                            XmNrightAttachment, XmATTACH_NONE, 
                                            XmNtopPosition, 0, 
                                            XmNleftPosition, 0, 
                                            XmNtopOffset, -3, 
                                            XmNbottomOffset, 0, 
                                            XmNleftOffset, 36, 
                                            XmNrightOffset, 0, 
                                            XmNwidth, 189, 
                                            XmNheight, 32, 
                                            (XtPointer) NULL ); 


    _toggle1 = XtVaCreateManagedWidget  ( "toggle1",
                                           xmToggleButtonWidgetClass,
                                           _radiobox, 
                                           XmNlabelType, XmSTRING, 
                                           XmNset, True, 
                                           (XtPointer) NULL ); 

    XtAddCallback ( _toggle1,
                    XmNvalueChangedCallback,
                    &MainFormUI::doFiltersCallback,
                    (XtPointer) this ); 


    _toggle2 = XtVaCreateManagedWidget  ( "toggle2",
                                           xmToggleButtonWidgetClass,
                                           _radiobox, 
                                           XmNlabelType, XmSTRING, 
                                           (XtPointer) NULL ); 

    XtAddCallback ( _toggle2,
                    XmNvalueChangedCallback,
                    &MainFormUI::doTransitionsCallback,
                    (XtPointer) this ); 


    _resultGLFrame1 = XtVaCreateManagedWidget  ( "resultGLFrame1",
                                                  xmFrameWidgetClass,
                                                  _effectsForm, 
                                                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                                  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                                  XmNleftAttachment, XmATTACH_WIDGET, 
                                                  XmNrightAttachment, XmATTACH_NONE, 
                                                  XmNtopPosition, 0, 
                                                  XmNbottomPosition, 0, 
                                                  XmNleftPosition, 0, 
                                                  XmNrightPosition, 0, 
                                                  XmNtopOffset, 0, 
                                                  XmNbottomOffset, 0, 
                                                  XmNleftOffset, 36, 
                                                  XmNrightOffset, 0, 
                                                  XmNwidth, 246, 
                                                  XmNheight, 186, 
                                                  (XtPointer) NULL ); 


    _result_GLWidget = XtVaCreateManagedWidget  ( "result_GLWidget",
                                                   glwMDrawingAreaWidgetClass,
                                                   _resultGLFrame1, 
                                                   GLwNrgba, True, 
                                                   GLwNredSize, 1, 
                                                   GLwNgreenSize, 1, 
                                                   GLwNblueSize, 1, 
                                                   GLwNallocateBackground, True, 
                                                   XmNwidth, 240, 
                                                   XmNheight, 180, 
                                                   (XtPointer) NULL ); 

    XtAddCallback ( _result_GLWidget,
                    GLwNinputCallback,
                    &MainFormUI::glInputCallback,
                    (XtPointer) this ); 

    _effectsScrolledWindow = XtVaCreateManagedWidget  ( "effectsScrolledWindow",
                                                         xmScrolledWindowWidgetClass,
                                                         _effectsForm, 
                                                         XmNscrollBarDisplayPolicy, XmSTATIC, 
                                                         XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                                         XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                                         XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                                         XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                                         XmNbottomWidget, _resultGLFrame1, 
                                                         XmNleftWidget, _radiobox, 
                                                         XmNrightWidget, _radiobox, 
                                                         XmNtopPosition, 0, 
                                                         XmNleftPosition, 0, 
                                                         XmNrightPosition, 0, 
                                                         XmNtopOffset, 0, 
                                                         XmNbottomOffset, 0, 
                                                         XmNleftOffset, 0, 
                                                         XmNrightOffset, 0, 
                                                         XmNwidth, 189, 
                                                         XmNheight, 186, 
                                                         (XtPointer) NULL ); 


    _scrolledList = XtVaCreateManagedWidget  ( "scrolledList",
                                                xmListWidgetClass,
                                                _effectsScrolledWindow, 
                                                XmNwidth, 183, 
                                                XmNheight, 180, 
                                                (XtPointer) NULL ); 


    _srcBGLFrame = XtVaCreateManagedWidget  ( "srcBGLFrame",
                                               xmFrameWidgetClass,
                                               _effectsForm, 
                                               XmNtopAttachment, XmATTACH_WIDGET, 
                                               XmNbottomAttachment, XmATTACH_NONE, 
                                               XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                               XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET, 
                                               XmNtopWidget, _sourceAButton, 
                                               XmNtopPosition, 0, 
                                               XmNbottomPosition, 0, 
                                               XmNleftPosition, 0, 
                                               XmNrightPosition, 0, 
                                               XmNtopOffset, 20, 
                                               XmNbottomOffset, 0, 
                                               XmNleftOffset, 0, 
                                               XmNrightOffset, 0, 
                                               XmNwidth, 246, 
                                               XmNheight, 186, 
                                               (XtPointer) NULL ); 


    _sourceB_GLWidget = XtVaCreateManagedWidget  ( "sourceB_GLWidget",
                                                    glwMDrawingAreaWidgetClass,
                                                    _srcBGLFrame, 
                                                    GLwNrgba, True, 
                                                    GLwNredSize, 1, 
                                                    GLwNgreenSize, 1, 
                                                    GLwNblueSize, 1, 
                                                    GLwNallocateBackground, True, 
                                                    XmNwidth, 240, 
                                                    XmNheight, 180, 
                                                    (XtPointer) NULL ); 

    XtAddCallback ( _sourceB_GLWidget,
                    GLwNinputCallback,
                    &MainFormUI::glInputCallback,
                    (XtPointer) this ); 

    _srcAGLFrame = XtVaCreateManagedWidget  ( "srcAGLFrame",
                                               xmFrameWidgetClass,
                                               _effectsForm, 
                                               XmNtopAttachment, XmATTACH_FORM, 
                                               XmNbottomAttachment, XmATTACH_NONE, 
                                               XmNleftAttachment, XmATTACH_FORM, 
                                               XmNrightAttachment, XmATTACH_NONE, 
                                               XmNbottomPosition, 0, 
                                               XmNrightPosition, 0, 
                                               XmNtopOffset, 20, 
                                               XmNbottomOffset, 0, 
                                               XmNleftOffset, 0, 
                                               XmNrightOffset, 0, 
                                               XmNwidth, 246, 
                                               XmNheight, 186, 
                                               (XtPointer) NULL ); 


    _sourceA_GLWidget = XtVaCreateManagedWidget  ( "sourceA_GLWidget",
                                                    glwMDrawingAreaWidgetClass,
                                                    _srcAGLFrame, 
                                                    GLwNrgba, True, 
                                                    GLwNredSize, 1, 
                                                    GLwNgreenSize, 1, 
                                                    GLwNblueSize, 1, 
                                                    GLwNallocateBackground, True, 
                                                    XmNwidth, 240, 
                                                    XmNheight, 180, 
                                                    (XtPointer) NULL ); 

    XtAddCallback ( _sourceA_GLWidget,
                    GLwNinputCallback,
                    &MainFormUI::glInputCallback,
                    (XtPointer) this ); 

    XtVaSetValues ( _customizeButton, 
                    XmNtopWidget, _alphaGLFrame, 
                    XmNleftWidget, _effectsScrolledWindow, 
                    (XtPointer) NULL );
    XtVaSetValues ( _destAlphaLabel, 
                    XmNbottomWidget, _alphaGLFrame, 
                    XmNleftWidget, _alphaGLFrame, 
                    (XtPointer) NULL );
    XtVaSetValues ( _alphaGLFrame, 
                    XmNtopWidget, _srcBGLFrame, 
                    XmNleftWidget, _resultGLFrame1, 
                    (XtPointer) NULL );
    XtVaSetValues ( _label1, 
                    XmNbottomWidget, _effectsScrolledWindow, 
                    XmNleftWidget, _effectsScrolledWindow, 
                    (XtPointer) NULL );
    XtVaSetValues ( _label2, 
                    XmNbottomWidget, _resultGLFrame1, 
                    XmNleftWidget, _resultGLFrame1, 
                    (XtPointer) NULL );
    XtVaSetValues ( _sourceBButton, 
                    XmNtopWidget, _srcBGLFrame, 
                    XmNrightWidget, _srcBGLFrame, 
                    (XtPointer) NULL );
    XtVaSetValues ( _sourceAButton, 
                    XmNtopWidget, _srcAGLFrame, 
                    XmNleftWidget, _srcAGLFrame, 
                    XmNrightWidget, _srcAGLFrame, 
                    (XtPointer) NULL );
    XtVaSetValues ( _radiobox, 
                    XmNtopWidget, _resultGLFrame1, 
                    XmNleftWidget, _resultGLFrame1, 
                    (XtPointer) NULL );
    XtVaSetValues ( _resultGLFrame1, 
                    XmNtopWidget, _srcAGLFrame, 
                    XmNbottomWidget, _srcAGLFrame, 
                    XmNleftWidget, _srcAGLFrame, 
                    (XtPointer) NULL );
    XtVaSetValues ( _effectsScrolledWindow, 
                    XmNtopWidget, _srcAGLFrame, 
                    (XtPointer) NULL );
    XtVaSetValues ( _srcBGLFrame, 
                    XmNleftWidget, _srcAGLFrame, 
                    XmNrightWidget, _srcAGLFrame, 
                    (XtPointer) NULL );
    //---- Start editable code block: MainFormUI create


    //---- End editable code block: MainFormUI create
}

const char * MainFormUI::className()
{
    return ("MainFormUI");
} // End className()




/////////////////////////////////////////////////////////////// 
// The following functions are static member functions used to 
// interface with Motif.
/////////////////////////////////// 

void MainFormUI::doFiltersCallback ( Widget    w,
                                     XtPointer clientData,
                                     XtPointer callData ) 
{ 
    MainFormUI* obj = ( MainFormUI * ) clientData;
    obj->doFilters ( w, callData );
}

void MainFormUI::doFrameSliderCallback ( Widget    w,
                                         XtPointer clientData,
                                         XtPointer callData ) 
{ 
    MainFormUI* obj = ( MainFormUI * ) clientData;
    obj->doFrameSlider ( w, callData );
}

void MainFormUI::doSetupCallback ( Widget    w,
                                   XtPointer clientData,
                                   XtPointer callData ) 
{ 
    MainFormUI* obj = ( MainFormUI * ) clientData;
    obj->doSetup ( w, callData );
}

void MainFormUI::doTransitionsCallback ( Widget    w,
                                         XtPointer clientData,
                                         XtPointer callData ) 
{ 
    MainFormUI* obj = ( MainFormUI * ) clientData;
    obj->doTransitions ( w, callData );
}

void MainFormUI::glInputCallback ( Widget    w,
                                   XtPointer clientData,
                                   XtPointer callData ) 
{ 
    MainFormUI* obj = ( MainFormUI * ) clientData;
    obj->glInput ( w, callData );
}

void MainFormUI::loadSourceACallback ( Widget    w,
                                       XtPointer clientData,
                                       XtPointer callData ) 
{ 
    MainFormUI* obj = ( MainFormUI * ) clientData;
    obj->loadSourceA ( w, callData );
}

void MainFormUI::loadSourceBCallback ( Widget    w,
                                       XtPointer clientData,
                                       XtPointer callData ) 
{ 
    MainFormUI* obj = ( MainFormUI * ) clientData;
    obj->loadSourceB ( w, callData );
}



/////////////////////////////////////////////////////////////// 
// The following functions are called from the menu items 
// in this window.
/////////////////////////////////// 

void MainFormUI::doFilters ( Widget, XtPointer ) 
{
    // This virtual function is called from doFiltersCallback.
    // This function is normally overriden by a derived class.

}

void MainFormUI::doFrameSlider ( Widget, XtPointer ) 
{
    // This virtual function is called from doFrameSliderCallback.
    // This function is normally overriden by a derived class.

}

void MainFormUI::doSetup ( Widget, XtPointer ) 
{
    // This virtual function is called from doSetupCallback.
    // This function is normally overriden by a derived class.

}

void MainFormUI::doTransitions ( Widget, XtPointer ) 
{
    // This virtual function is called from doTransitionsCallback.
    // This function is normally overriden by a derived class.

}

void MainFormUI::glInput ( Widget, XtPointer ) 
{
    // This virtual function is called from glInputCallback.
    // This function is normally overriden by a derived class.

}

void MainFormUI::loadSourceA ( Widget, XtPointer ) 
{
    // This virtual function is called from loadSourceACallback.
    // This function is normally overriden by a derived class.

}

void MainFormUI::loadSourceB ( Widget, XtPointer ) 
{
    // This virtual function is called from loadSourceBCallback.
    // This function is normally overriden by a derived class.

}



//---- Start editable code block: End of generated code


//---- End editable code block: End of generated code
