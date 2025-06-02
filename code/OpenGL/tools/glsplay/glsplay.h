/*------------------------------*\
|* define
\*------------------------------*/
#define W_VARIABLE_STR(x, y) XtVaTypedArg, (x), XmRString, (y), strlen((y))+1

/*------------------------------*\
|* var
\*------------------------------*/
extern XtAppContext GLSapp;
extern XtInputId GLSinputID;
extern Widget GLSwidget;

/*------------------------------*\
|* proto
\*------------------------------*/
extern void ShowWindowInit(char *);

/*------------------------------*\
|* bottom
\*------------------------------*/
