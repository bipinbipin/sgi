/* SoundPlayer intlresPlayer.h
   Internationalize: Added below code. By Shoji Kuwahara. 5/3/95 
*/

#ifndef _INTL_RES_
#define _INTL_RES_


int   _intl_dummy  = 0;
char *_intl_ErrorTitleOFReadError;
char *_intl_ErrorPostCRUnsupportFormat;
char *_intl_ErrorPostNotOpenSelFile;
char *_intl_ErrorTitleSFAError;
char *_intl_ErrorPostNoFileOpen;
char *_intl_ErrorPostNotWriteSelFile;
char *_intl_ErrorPostFISaveNotPerformed;
char *_intl_ErrorPostNotSaveAcrossFile;
char *_intl_ErrorPostFileExists;
char *_intl_ErrorPostErrorSaving;
char *_intl_SDTitleSaveFileAs;
char *_intl_SDTitleOpenFile;
char *_intl_UnOpenPort;
char *_intl_UnDecodeMPEG;
char *_intl_UnOpenMIDI;
char *_intl_UnSaveFile;
char *_intl_UnCloneWindow;
char *_intl_NoMIDILib;
char *_intl_NoMIDIDev;









static XtResource intl_resources[] = {
  {                   "dummy",          NULL,  XtRInt,     sizeof(XtRInt), 
    (Cardinal)  &_intl_dummy,
     XtRInt,    &_intl_dummy,     
  },
  {                   "ErrorTitleOFReadError",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_ErrorTitleOFReadError,
     XtRString,       "Open File: Read Error",     
  },
  {                   "ErrorPostCRUnsupportFormat",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_ErrorPostCRUnsupportFormat,
     XtRString,       "Cannot read selected file: Unsupported file format",     
  },
  {                   "ErrorPostNotOpenSelFile",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_ErrorPostNotOpenSelFile,
     XtRString,       "Cannot open selected file.",     
  },
  {                   "ErrorTitleSFAError",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_ErrorTitleSFAError,
     XtRString,       "Save File As: Error",     
  },
  {                   "ErrorPostNoFileOpen",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_ErrorPostNoFileOpen,
     XtRString,       "No file currently open.",     
  },
  {                   "ErrorPostNotWriteSelFile",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_ErrorPostNotWriteSelFile,
     XtRString,       "Cannot write selected file.",     
  },
  {                   "ErrorPostFISaveNotPerformed",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_ErrorPostFISaveNotPerformed,
     XtRString,       "Files are identical: Save not performed.",     
  },
  {                   "ErrorPostNotSaveAcrossFile",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_ErrorPostNotSaveAcrossFile,
     XtRString,       "Cannot save across file systems yet.",     
  },
  {                   "ErrorPostFileExists",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_ErrorPostFileExists,
     XtRString,       "File already exists: Save not performed.",     
  },
  {                   "ErrorPostErrorSaving",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_ErrorPostErrorSaving,
     XtRString,       "Error saving file.",     
  },
  {                   "SDTitleSaveFileAs",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_SDTitleSaveFileAs,
     XtRString,       "Sound Player: Save File As",     
  },

  {                   "UnOpenPort",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_UnOpenPort,
     XtRString,       "Unable to open audio port",     
  },

  {                   "UnDecodeMPEG",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_UnDecodeMPEG,
     XtRString,       "Unable to decode MPEG audio bitstream",     
  },

  {                   "UnOpenMIDI",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_UnOpenMIDI,
     XtRString,       "Unable to open MIDI connection",     
  },

  {                   "UnSaveFile",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_UnSaveFile,
     XtRString,       "Unable to save file:",     
  },

  {                   "UnCloneWindow",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_UnCloneWindow,
     XtRString,       "Unable to clone window",     
  },

  {                   "UnNoMIDILib",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_NoMIDILib,
     XtRString,       "MIDI Library was not found.\n\nMake certain that software subsystem dmedia_eoe.sw.midi is properly installed.",     
  },

  {                   "UnNoMIDIDev",    NULL,  XtRString,  sizeof(XtRString), 
    (Cardinal)  &_intl_NoMIDIDev,
     XtRString,       "No MIDI devices are configured.\n\nConfigure a serial port for MIDI using the system administration tools.",     
  },
};
static int intl_num_resources = XtNumber( intl_resources );


#endif  // _INTL_RES_



