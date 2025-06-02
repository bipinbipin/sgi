/* PPE: The Precision Pulse Engine */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <fcntl.h>
#include <math.h>
#include <malloc.h>
#include <string.h>
#include <audiofile.h>
#include <audio.h>
#include <dmedia/midi.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <bstring.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/fpu.h>
#include <stropts.h>
#include <poll.h>

#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/Display.h>
#include <Xm/RowColumn.h>
#include <Xm/Frame.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/CascadeB.h>
#include <Xm/MessageB.h>
#include <Xm/FileSB.h>
#include <Xm/DrawingA.h>
#include <Xm/Scale.h>
#include <Xm/SelectioB.h>
#include <Xm/BulletinB.h>
#include <Xm/Form.h>
#include <Xm/TextF.h>
#include <Xm/Text.h>
#include <Xm/Label.h>
#include <Xm/ToggleB.h>
#include <Xm/ToggleBG.h>
#include <Xm/ScrollBar.h>
#include <Xm/Separator.h>
#include <Xm/List.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <Sgm/Dial.h>

#include "Matrix.h"

#define PI 3.141592654
#define TWOPI 6.2831853
#define MAXCLIENTS 64   /* Networking things */
#define MAXSENDTO 8
#define PORTNUM 7778


#define XtNwwwbrowser "wwwBrowser"
#define XtCwwwbrowser "WwwBrowser"
#define XtNwwwfile    "wwwFile"
#define XtCwwwfile    "WwwFile"

static char play_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0xc0, 0x00, 0xc0, 0x01,
   0xc0, 0x03, 0xc0, 0x07, 0xc0, 0x0f, 0xc0, 0x07, 0xc0, 0x03, 0xc0, 0x01,
   0xc0, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00};

static char stop_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x03,
   0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xc0, 0x03, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


typedef struct {
  char *www_browser;
  char *www_file;
} AppData;
AppData app_data;

static XtResource resources[] = {
  { XtNwwwbrowser, XtCwwwbrowser, XmRString, sizeof(char*),
    XtOffsetOf(AppData, www_browser), XmRImmediate, NULL },
  { XtNwwwfile, XtCwwwfile, XmRString, sizeof(char*),
    XtOffsetOf(AppData, www_file), XmRImmediate, NULL },
};

struct drm {
  int playing, ptr, type, numsamp[77];
  double vol;
  char name[4];
  short int *samp[77];
} drums[13];

struct smp {
  int playing, ptr, numsamp;
  char name[4];
  short int *samp;
} samps[13];

struct {
  int numsteps;
  int step[32][13];
} drumpatterns[32], samppatterns[32];

struct {
  int numsteps;
  int step[32];
} pcfpatterns[32];

struct {
  int numsteps;
  int pitch[32], dur[32], accent[32];
} syn1patterns[32], syn2patterns[32];

Widget topLevel, mainWindow, menuBar, frame, largeBB, dw, sketchpad;
Widget trackBB, tracksep1, tracksep2, tracksep3;
Widget topBB, topsep1, topsep2, topsep3, bpmDial, bpmLabel, playB, stopB;
Widget fileMenu, fileButton, quit, load, save, help, helpButton, helpMenu,
  helpBox, temp, loadFileBox, saveFileBox, bounce, bounceFileBox;
Widget editMenu, editButton, cut, copy, paste;
Widget winMenu, winButton, drumwin, songwin;
Widget helpMenu, helpButton, help;
Widget syn1BB, syn1sep1, syn1waveRadio, syn1waveSaw, syn1waveSqr, syn1waveSmp,
  syn1tuneDial, syn1tuneLabel, syn1pwmDial, syn1pwmLabel, syn1decayDial,
  syn1decayLabel, syn1lpCutDial, syn1lpCutLabel, syn1lpResDial, syn1lpResLabel,
  syn1lpEnvDial, syn1lpEnvLabel, syn1volDial, syn1volLabel, syn1panDial,
  syn1panLabel, syn1distDial, syn1distLabel, syn1delayDial, syn1delayLabel,
  syn1pcfToggle, syn1accentDial, syn1accentLabel;
Widget syn1beats8, syn1beats16, syn1beats32, syn1beatsRadio,
       syn2beats8, syn2beats16, syn2beats32, syn2beatsRadio,
       drumbeats8, drumbeats16, drumbeats32, drumbeatsRadio;
Widget syn2BB, syn2sep1, syn2waveRadio, syn2waveSaw, syn2waveSqr, syn2waveSmp,
  syn2tuneDial, syn2tuneLabel, syn2pwmDial, syn2pwmLabel, syn2decayDial,
  syn2decayLabel, syn2lpCutDial, syn2lpCutLabel, syn2lpResDial, syn2lpResLabel,
  syn2lpEnvDial, syn2lpEnvLabel, syn2volDial, syn2volLabel, syn2panDial,
  syn2panLabel, syn2distDial, syn2distLabel, syn2delayDial, syn2delayLabel,
  syn2pcfToggle, syn2accentDial, syn2accentLabel;
Widget sampBB, sampsep1, samplpCutDial, samplpCutLabel, samplpResDial,
  samplpResLabel, sampvolDial, sampvolLabel, samppanDial, samppanLabel,
  sampdistDial, sampdistLabel, sampdelayDial, sampdelayLabel, samppcfToggle;
Widget drumBB, drumBB2, drumBlabel, drumBvolDial, drumBvolLabel, drumBtuneDial,
  drumBtuneLabel, drumBattackDial, drumBattackLabel, drumBdecayDial,
  drumBdecayLabel, drumBtr808, drumSlabel, drumSvolDial, drumSvolLabel,
  drumStr808, drumStuneDial,
  drumStuneLabel, drumStoneDial, drumStoneLabel, drumSsnappyDial,
  drumSsnappyLabel, drumLlabel, drumLvolDial, drumLvolLabel, drumLtuneDial,
  drumLtuneLabel, drumLdecayDial, drumLdecayLabel, drumMlabel, drumMvolDial,
  drumMvolLabel, drumMtuneDial, drumMtuneLabel, drumMdecayDial, drumMdecayLabel,
  drumHlabel, drumHvolDial, drumHvolLabel, drumHtuneDial, drumHtuneLabel,
  drumHdecayDial, drumHdecayLabel, drumRIMlabel, drumRIMvolDial,
  drumRIMvolLabel, drumRIMvelDial, drumRIMvelLabel, drumHANlabel,
  drumHANvolDial, drumHANvolLabel, drumHANvelDial, drumHANvelLabel,
  drumHHClabel, drumHHCvolDial, drumHHCvolLabel, drumHHCdecayDial,
  drumHHCdecayLabel, drumHHOlabel, drumHHOvolDial, drumHHOvolLabel,
  drumHHOdecayDial, drumHHOdecayLabel, drumCSHlabel, drumCSHvolDial,
  drumCSHvolLabel, drumCSHtuneDial, drumCSHtuneLabel, drumRIDlabel,
  drumRIDvolDial, drumRIDvolLabel, drumRIDtuneDial, drumRIDtuneLabel,
  drumCLOlabel, drumCLOvolDial, drumCLOvolLabel, drumCLOcombDial,
  drumCLOcombLabel, drumOPClabel, drumOPCvolDial, drumOPCvolLabel,
  drumOPCcombDial, drumOPCcombLabel;
Widget drumvolDial, drumvolLabel, drumpanDial, drumpanLabel,
  drumdelayDial, drumdelayLabel, drumpcfToggle;
Widget songBB, songMatrix, songToggle, loopToggle;
Widget fileWarning;
Widget syn1patBut[8], syn1bankBut[8], syn1bankLabel,
  syn2patBut[8], syn2bankBut[8], syn2bankLabel,
  samppatBut[8], sampbankBut[8], sampbankLabel,
  drumpatBut[8], drumbankBut[8], drumbankLabel;
Widget delayTimeDial, delayTimeLabel, delayPanDial, delayPanLabel,
  delayFeedDial, delayFeedLabel, delayLabel;
Widget pcfLabel, pcfFrqDial, pcfFrqLabel, pcfQDial, pcfQLabel,
  pcfAmntDial, pcfAmntLabel, pcfDecayDial, pcfDecayLabel,
  pcfPatternDial, pcfPatternLabel, pcfBpToggle;
Widget stepSlider, saveSett, clearSett, labelSett, syn1Sett, syn2Sett,
  sampSett, drumSett, sep1Sett, sep2Sett;
Widget sendtoBB, sendtoButton, sendtoToggle[MAXSENDTO], sendtoText[MAXSENDTO],
  sendtoOkButton, sendtoWarning, sendtoTogLabel, sendtoTextLabel, sendtoAccept,
  connectWarning, slavefailWarning;

typedef struct {
  int tune, pwm, decay, cutoff, reson, envmod, vol, pan, distor, delay,
    accent, pcf, wave, set;
} syntsett;
syntsett Syntsett1, Syntsett2, Synt1Vec[32][8], Synt2Vec[32][8];

typedef struct {
  int vol, pan, distor, delay, cutoff, reson, pcf, set;
} sampsett;
sampsett Sampsett, SampVec[32][8];

typedef struct {
  int bpm, freq, reson, amnt, decay, pat, bp, steps, pan, fback, set;
} globalsett;
globalsett Globalsett, GlobalVec[32][8];

typedef struct {
  int vol, pan, delay, pcf;
  int bassvol, basstune, bassattck, bassdecay, bass808,
    snarevol, snaretune, snaretone, snaresnap, snare808,
    ltomvol, ltomtune, ltomdecay, mtomvol, mtomtune, mtomdecay,
    htomvol, htomtune, htomdecay, rimshvol, rimshveloc,
    hclapvol, hclapveloc, clhhvol, clhhdecay, ophhvol, ophhdecay,
    crashvol, crashtune, ridevol, ridetune, cohhvol, cohhcombi,
    ochhvol, ochhcombi, set;    
} drumsett;
drumsett Drumsett, DrumVec[32][8];

Window theRoot, theWindow;
XtAppContext app_context;
Colormap theCmap;
Display *theDisplay;
int screen;
Pixmap bitmap, asynPatchBM;
Pixmap playpixmap, noplaypixmap, stoppixmap, nostoppixmap;
XColor red, blue, yellow, green, black, white, skyblue, brown, gray, purple,
  markcolor;
GC showGC, bitGC, markerGC;

int playing=0, accepting=0, bpm=120, bpmsamp, bouncing=0;
int wavetyp1=2, wavetyp2=1, delaytime;
double wf1, wf2, q1, f1, q2, f2, pwm1, pwm2, decay1, decay2, emod1, emod2,
  vol1, vol2, dist1, dist2, distsamp, delay1, delay2, delaysmp,
  drumvol, drumdelay, syn1pan, syn2pan, samppan, drumpan, delaypan, delayfeed,
  pcffrq, pcfq, pcfamnt, pcfdecay, fsamp, qsamp, sampvol,
  acc1, acc2;
double delayline[262144];
int syn1pat, syn2pat, samppat, drumpat, syn1pcf, syn2pcf, samppcf, drumpcf,
  pcfpattern, pcfBp;
int bpmsamp6, autstep=0, autwhere=1, songwhere=0;
char bouncefilename[100];

int server_socket, client_sockets[MAXCLIENTS];
struct timeval timeout;
struct sockaddr_in me_sockaddr, sendto_addresses[MAXSENDTO];
fd_set client_sockets_set;
int sendto_sockets[MAXSENDTO], sendto_dummy[MAXSENDTO],
  sendto_connected[MAXSENDTO];

int dummy[8]={0, 1, 2, 3, 4, 5, 6, 7};

void readcontrol(int);

void flush_all_underflows_to_zero()
  {
    union fpc_csr f;
    f.fc_word = get_fpc_csr();
    f.fc_struct.flush = 1;
    set_fpc_csr(f.fc_word);
  }

void openWarn(fil)
char *fil;
{
  XmString text;
  char streng[100];

  sprintf(streng,"Can't open file %s",fil);
  text=XmStringCreateSimple(streng);
  XtVaSetValues(fileWarning,XmNmessageString,text,NULL);
  XtManageChild(fileWarning);
  XtFree(text);
}

void SetWatchCursor(w)
Widget w;
{
    static Cursor watch = NULL;

    if(!watch) watch = XCreateFontCursor(XtDisplay(w),XC_watch);

    XDefineCursor(XtDisplay(w),XtWindow(w),watch);
    XDefineCursor(XtDisplay(topLevel),XtWindow(topLevel),watch);
    XmUpdateDisplay(w);
    XmUpdateDisplay(topLevel);
}

void SetGrabCursor(w)
Widget w;
{
    static Cursor grab = NULL;

    if(!grab) grab = XCreateFontCursor(XtDisplay(w),XC_hand2);

    XDefineCursor(XtDisplay(w),XtWindow(w),grab);
    XmUpdateDisplay(w);
}

void ResetCursor(w)
Widget w;
{
    XUndefineCursor(XtDisplay(w),XtWindow(w));
    XUndefineCursor(XtDisplay(topLevel),XtWindow(topLevel));
    XmUpdateDisplay(w);
    XmUpdateDisplay(topLevel);
}

void Cancel(w, client, call)
Widget w;
XtPointer client, call;
{
  Widget dialog=(Widget) client;
  XtUnmanageChild(dialog);
}

void Quit(w, client, call)
Widget w;
XtPointer client, call;
{
  int i;

  for (i=0; i<MAXCLIENTS; i++)
    if (client_sockets[i]>=0) close(client_sockets[i]);
  close(server_socket);

  exit(0);
}

void RestoreWin(w, client, call)
Widget w;
XtPointer client, call;
{
  XCopyArea(theDisplay, bitmap, XtWindow(sketchpad),
    DefaultGCOfScreen(XtScreen(sketchpad)), 0, 0, 580, 600, 0, 0);
}

void redraw_drum(void)
{
  int i, j, stepwidth;
  char *navn;

  stepwidth=32*16./drumpatterns[drumpat].numsteps;
  XSetForeground(theDisplay, showGC, white.pixel);
  XFillRectangle(theDisplay, bitmap, showGC, 0, 450, 580, 600);
  if (autwhere==4) {
    XSetForeground(theDisplay, showGC, yellow.pixel);
    XFillRectangle(theDisplay, bitmap, showGC, autstep*stepwidth+50, 460, stepwidth, 130);
  }
  XSetForeground(theDisplay, showGC, blue.pixel);
  for (i=0; i<=drumpatterns[drumpat].numsteps; i++) {
    XDrawLine(theDisplay, bitmap, showGC, 50+i*stepwidth, 460, 50+i*stepwidth, 590);
  }
  for (i=0; i<14; i++)
    XDrawLine(theDisplay, bitmap, showGC, 50, 590-i*10, 562, 590-i*10);
  for (i=0; i<13; i++) {
    switch (i) {
      case 0: navn="BA"; break;
      case 1: navn="SN"; break;
      case 2: navn="LT"; break;
      case 3: navn="MT"; break;
      case 4: navn="HT"; break;
      case 5: navn="RS"; break;
      case 6: navn="HC"; break;
      case 7: navn="CH"; break;
      case 8: navn="OH"; break;
      case 9: navn="CR"; break;
      case 10: navn="RD"; break;
      case 11: navn="CO"; break;
      case 12: navn="OC"; break;
    }
    XDrawString(theDisplay,bitmap,showGC, 3+(i%2)*20, 590-i*10,
      navn, strlen(navn));
  }
  XSetForeground(theDisplay, showGC, red.pixel);
  for (i=0; i<drumpatterns[drumpat].numsteps; i++) {
    for (j=0; j<13; j++) {
      if (drumpatterns[drumpat].step[i][j])
        XFillRectangle(theDisplay, bitmap, showGC, i*stepwidth+54, 582-j*10, stepwidth-8, 7); 
    }
  }
}

void redraw_syn1(void)
{
  int i;
  double stepwidth;

  stepwidth=32*16./syn1patterns[syn1pat].numsteps;
  XSetForeground(theDisplay, showGC, white.pixel);
  XFillRectangle(theDisplay, bitmap, showGC, 0, 0, 580, 150);
  if (autwhere==1) {
    XSetForeground(theDisplay, showGC, yellow.pixel);
    XFillRectangle(theDisplay, bitmap, showGC, autstep*stepwidth+50, 0, stepwidth, 150);
  }
  XSetForeground(theDisplay, showGC, black.pixel);
  for (i=0; i<3; i++) {
    XFillRectangle(theDisplay, bitmap, showGC, 0, 142-i*48, 30, 4);
    XFillRectangle(theDisplay, bitmap, showGC, 0, 130-i*48, 30, 4);
    XFillRectangle(theDisplay, bitmap, showGC, 0, 122-i*48, 30, 4);
    XFillRectangle(theDisplay, bitmap, showGC, 0, 110-i*48, 30, 4);
    XFillRectangle(theDisplay, bitmap, showGC, 0, 102-i*48, 30, 4);
    XDrawLine(theDisplay, bitmap, showGC, 0, 144-i*48, 562, 144-i*48);
    XDrawLine(theDisplay, bitmap, showGC, 0, 138-i*48, 562, 138-i*48);
    XDrawLine(theDisplay, bitmap, showGC, 0, 132-i*48, 562, 132-i*48);
    XDrawLine(theDisplay, bitmap, showGC, 0, 124-i*48, 562, 124-i*48);
    XDrawLine(theDisplay, bitmap, showGC, 0, 118-i*48, 562, 118-i*48);
    XDrawLine(theDisplay, bitmap, showGC, 0, 112-i*48, 562, 112-i*48);
    XDrawLine(theDisplay, bitmap, showGC, 0, 104-i*48, 562, 104-i*48);
  }
  XDrawLine(theDisplay, bitmap, showGC, 0, 149, 562, 149);
  for (i=0; i<=syn1patterns[syn1pat].numsteps; i++) {
    XDrawLine(theDisplay, bitmap, showGC, 50+i*stepwidth, 0, 50+i*stepwidth, 149);
  }
  for (i=0; i<syn1patterns[syn1pat].numsteps; i++) {
    if (syn1patterns[syn1pat].accent[i])
      XSetForeground(theDisplay, showGC, red.pixel);
    else XSetForeground(theDisplay, showGC, brown.pixel);
    if (syn1patterns[syn1pat].dur[i])
      XFillRectangle(theDisplay, bitmap, showGC,
        i*stepwidth+50, 146-syn1patterns[syn1pat].pitch[i]*4, stepwidth/2, 4);
    if (syn1patterns[syn1pat].dur[i]==2)
      XDrawLine(theDisplay, bitmap, showGC,
        i*stepwidth+stepwidth/2+50, 148-syn1patterns[syn1pat].pitch[i]*4,
        i*stepwidth+stepwidth+49,
        148-syn1patterns[syn1pat].pitch[(i+1)%syn1patterns[syn1pat].numsteps]*4);
  }
}

void redraw_syn2(void)
{
  int i;
  double stepwidth;

  stepwidth=32*16./syn2patterns[syn2pat].numsteps;
  XSetForeground(theDisplay, showGC, white.pixel);
  XFillRectangle(theDisplay, bitmap, showGC, 0, 150, 580, 150);
  if (autwhere==2) {
    XSetForeground(theDisplay, showGC, yellow.pixel);
    XFillRectangle(theDisplay, bitmap, showGC, autstep*stepwidth+50, 150, stepwidth, 150);
  }
  XSetForeground(theDisplay, showGC, blue.pixel);
  for (i=0; i<3; i++) {
    XFillRectangle(theDisplay, bitmap, showGC, 0, 292-i*48, 30, 4);
    XFillRectangle(theDisplay, bitmap, showGC, 0, 280-i*48, 30, 4);
    XFillRectangle(theDisplay, bitmap, showGC, 0, 272-i*48, 30, 4);
    XFillRectangle(theDisplay, bitmap, showGC, 0, 260-i*48, 30, 4);
    XFillRectangle(theDisplay, bitmap, showGC, 0, 252-i*48, 30, 4);
    XDrawLine(theDisplay, bitmap, showGC, 0, 294-i*48, 562, 294-i*48);
    XDrawLine(theDisplay, bitmap, showGC, 0, 288-i*48, 562, 288-i*48);
    XDrawLine(theDisplay, bitmap, showGC, 0, 282-i*48, 562, 282-i*48);
    XDrawLine(theDisplay, bitmap, showGC, 0, 274-i*48, 562, 274-i*48);
    XDrawLine(theDisplay, bitmap, showGC, 0, 268-i*48, 562, 268-i*48);
    XDrawLine(theDisplay, bitmap, showGC, 0, 262-i*48, 562, 262-i*48);
    XDrawLine(theDisplay, bitmap, showGC, 0, 254-i*48, 562, 254-i*48);
  }
  XDrawLine(theDisplay, bitmap, showGC, 0, 299, 562, 299);
  for (i=0; i<=syn2patterns[syn2pat].numsteps; i++) {
    XDrawLine(theDisplay, bitmap, showGC, 50+i*stepwidth, 150, 50+i*stepwidth, 299);
  }
  for (i=0; i<syn2patterns[syn2pat].numsteps; i++) {
    if (syn2patterns[syn2pat].accent[i])
      XSetForeground(theDisplay, showGC, red.pixel);
    else XSetForeground(theDisplay, showGC, brown.pixel);
    if (syn2patterns[syn2pat].dur[i])
      XFillRectangle(theDisplay, bitmap, showGC,
        i*stepwidth+50, 296-syn2patterns[syn2pat].pitch[i]*4, stepwidth/2, 4);
    if (syn2patterns[syn2pat].dur[i]==2)
      XDrawLine(theDisplay, bitmap, showGC,
        i*stepwidth+stepwidth/2+50, 298-syn2patterns[syn2pat].pitch[i]*4,
        i*stepwidth+stepwidth+49,
        298-syn2patterns[syn2pat].pitch[(i+1)%syn2patterns[syn2pat].numsteps]*4);
  }
}

void redraw_samp(void)
{
  int i, j, stepwidth;
  char *navn;

  stepwidth=32*16./samppatterns[samppat].numsteps;
  XSetForeground(theDisplay, showGC, white.pixel);
  XFillRectangle(theDisplay, bitmap, showGC, 0, 300, 580, 150);
  if (autwhere==3) {
    XSetForeground(theDisplay, showGC, yellow.pixel);
    XFillRectangle(theDisplay, bitmap, showGC, 50+autstep*stepwidth, 310, stepwidth, 130);
  }

  XSetForeground(theDisplay, showGC, black.pixel);
  for (i=0; i<=samppatterns[samppat].numsteps; i++) {
    XDrawLine(theDisplay, bitmap, showGC, 50+i*stepwidth, 310, 50+i*stepwidth, 440);
  }
  for (i=0; i<14; i++)
    XDrawLine(theDisplay, bitmap, showGC, 50, 440-i*10, 562, 440-i*10);
  for (i=0; i<13; i++) {
    switch (i) {
      case 0: navn="0"; break;
      case 1: navn="1"; break;
      case 2: navn="2"; break;
      case 3: navn="3"; break;
      case 4: navn="4"; break;
      case 5: navn="5"; break;
      case 6: navn="6"; break;
      case 7: navn="7"; break;
      case 8: navn="8"; break;
      case 9: navn="9"; break;
      case 10: navn="10"; break;
      case 11: navn="11"; break;
      case 12: navn="12"; break;
    }
    XDrawString(theDisplay,bitmap,showGC, 3+(i%2)*20, 440-i*10,
      navn, strlen(navn));
  }
  XSetForeground(theDisplay, showGC, red.pixel);
  for (i=0; i<samppatterns[samppat].numsteps; i++) {
    for (j=0; j<13; j++) {
      if (samppatterns[samppat].step[i][j])
        XFillRectangle(theDisplay, bitmap, showGC, i*stepwidth+54, 432-j*10, stepwidth-8, 7); 
    }
  }
}

void redraw_all(void)
{
  redraw_syn1();
  redraw_syn2();
  redraw_samp();
  redraw_drum();
  RestoreWin(NULL, NULL, NULL);
}

void StepSet1(int step)
{
  if (Synt1Vec[step][syn1pat].set) {
    if (Synt1Vec[step][syn1pat].tune!=Syntsett1.tune) {
      Syntsett1.tune=Synt1Vec[step][syn1pat].tune;
      SgDialSetValue(syn1tuneDial, Syntsett1.tune);
      wf1=pow(2.,(double)Syntsett1.tune/12.)*27.5/44100.;
    }
    if (Synt1Vec[step][syn1pat].pwm!=Syntsett1.pwm) {
      Syntsett1.pwm=Synt1Vec[step][syn1pat].pwm;
      SgDialSetValue(syn1pwmDial, Syntsett1.pwm);
      pwm1=(double)Syntsett1.pwm/15000000.;
    }
    if (Synt1Vec[step][syn1pat].decay!=Syntsett1.decay) {
      Syntsett1.decay=Synt1Vec[step][syn1pat].decay;
      SgDialSetValue(syn1decayDial, Syntsett1.decay);
      decay1=1.-(double)Syntsett1.decay/200000.;
    }
    if (Synt1Vec[step][syn1pat].cutoff!=Syntsett1.cutoff) {
      Syntsett1.cutoff=Synt1Vec[step][syn1pat].cutoff;
      SgDialSetValue(syn1lpCutDial, Syntsett1.cutoff);
      f1=(3000.*(exp(Syntsett1.cutoff/50.)-1.)/(exp(2.)-1.))*TWOPI/44100.;
    }
    if (Synt1Vec[step][syn1pat].reson!=Syntsett1.reson) {
      Syntsett1.reson=Synt1Vec[step][syn1pat].reson;
      SgDialSetValue(syn1lpResDial, Syntsett1.reson);
      q1=1./((double)Syntsett1.reson/10.);
    }
    if (Synt1Vec[step][syn1pat].envmod!=Syntsett1.envmod) {
      Syntsett1.envmod=Synt1Vec[step][syn1pat].envmod;
      SgDialSetValue(syn1lpEnvDial, Syntsett1.envmod);
      emod1=2.*PI*(double)Syntsett1.envmod/44100.;
    }
    if (Synt1Vec[step][syn1pat].vol!=Syntsett1.vol) {
      Syntsett1.vol=Synt1Vec[step][syn1pat].vol;
      SgDialSetValue(syn1volDial, Syntsett1.vol);
      vol1=(double)Syntsett1.vol/100.;
    }
    if (Synt1Vec[step][syn1pat].pan!=Syntsett1.pan) {
      Syntsett1.pan=Synt1Vec[step][syn1pat].pan;
      SgDialSetValue(syn1panDial, Syntsett1.pan);
      syn1pan=(double)Syntsett1.pan/100.;
    }
    if (Synt1Vec[step][syn1pat].distor!=Syntsett1.distor) {
      Syntsett1.distor=Synt1Vec[step][syn1pat].distor;
      SgDialSetValue(syn1distDial, Syntsett1.distor);
      dist1=(double)Syntsett1.distor+1.;
    }
    if (Synt1Vec[step][syn1pat].delay!=Syntsett1.delay) {
      Syntsett1.delay=Synt1Vec[step][syn1pat].delay;
      SgDialSetValue(syn1delayDial, Syntsett1.delay);
      delay1=(double)Syntsett1.delay/100.;
    }
    if (Synt1Vec[step][syn1pat].accent!=Syntsett1.accent) {
      Syntsett1.accent=Synt1Vec[step][syn1pat].accent;
      SgDialSetValue(syn1accentDial, Syntsett1.accent);
      acc1=(double)Syntsett1.accent/10.;
    }
    if (Synt1Vec[step][syn1pat].pcf!=Syntsett1.pcf) {
      Syntsett1.pcf=Synt1Vec[step][syn1pat].pcf;
      if (Syntsett1.pcf) XmToggleButtonSetState(syn1pcfToggle, True, True);
        else XmToggleButtonSetState(syn1pcfToggle, False, True);
    }
    if (Synt1Vec[step][syn1pat].wave!=Syntsett1.wave) {
      Syntsett1.wave=Synt1Vec[step][syn1pat].wave;
      if (Syntsett1.wave==0) XmToggleButtonSetState(syn1waveSaw, True, True);
        else if (Syntsett1.wave==1) XmToggleButtonSetState(syn1waveSqr, True, True);
        else XmToggleButtonSetState(syn1waveSmp, True, True);
    }
  }
}

void StepSet2(int step)
{
  if (Synt2Vec[step][syn2pat].set) {
    if (Synt2Vec[step][syn2pat].tune!=Syntsett2.tune) {
      Syntsett2.tune=Synt2Vec[step][syn2pat].tune;
      SgDialSetValue(syn2tuneDial, Syntsett2.tune);
      wf2=pow(2.,(double)Syntsett2.tune/12.)*27.5/44100.;
    }
    if (Synt2Vec[step][syn2pat].pwm!=Syntsett2.pwm) {
      Syntsett2.pwm=Synt2Vec[step][syn2pat].pwm;
      SgDialSetValue(syn2pwmDial, Syntsett2.pwm);
      pwm2=(double)Syntsett2.pwm/15000000.;
    }
    if (Synt2Vec[step][syn2pat].decay!=Syntsett2.decay) {
      Syntsett2.decay=Synt2Vec[step][syn2pat].decay;
      SgDialSetValue(syn2decayDial, Syntsett2.decay);
      decay2=1.-(double)Syntsett2.decay/200000.;
    }
    if (Synt2Vec[step][syn2pat].cutoff!=Syntsett2.cutoff) {
      Syntsett2.cutoff=Synt2Vec[step][syn2pat].cutoff;
      SgDialSetValue(syn2lpCutDial, Syntsett2.cutoff);
      f2=(3000.*(exp(Syntsett2.cutoff/50.)-1.)/(exp(2.)-1.))*TWOPI/44100.;
    }
    if (Synt2Vec[step][syn2pat].reson!=Syntsett2.reson) {
      Syntsett2.reson=Synt2Vec[step][syn2pat].reson;
      SgDialSetValue(syn2lpResDial, Syntsett2.reson);
      q2=1./((double)Syntsett2.reson/10.);
    }
    if (Synt2Vec[step][syn2pat].envmod!=Syntsett2.envmod) {
      Syntsett2.envmod=Synt2Vec[step][syn2pat].envmod;
      SgDialSetValue(syn2lpEnvDial, Syntsett2.envmod);
      emod2=2.*PI*(double)Syntsett2.envmod/44100.;
    }
    if (Synt2Vec[step][syn2pat].vol!=Syntsett2.vol) {
      Syntsett2.vol=Synt2Vec[step][syn2pat].vol;
      SgDialSetValue(syn2volDial, Syntsett2.vol);
      vol2=(double)Syntsett2.vol/100.;
    }
    if (Synt2Vec[step][syn2pat].pan!=Syntsett2.pan) {
      Syntsett2.pan=Synt2Vec[step][syn2pat].pan;
      SgDialSetValue(syn2panDial, Syntsett2.pan);
      syn2pan=(double)Syntsett2.pan/100.;
    }
    if (Synt2Vec[step][syn2pat].distor!=Syntsett2.distor) {
      Syntsett2.distor=Synt2Vec[step][syn2pat].distor;
      SgDialSetValue(syn2distDial, Syntsett2.distor);
      dist2=(double)Syntsett2.distor+1.;
    }
    if (Synt2Vec[step][syn2pat].delay!=Syntsett2.delay) {
      Syntsett2.delay=Synt2Vec[step][syn2pat].delay;
      SgDialSetValue(syn2delayDial, Syntsett2.delay);
      delay2=(double)Syntsett2.delay/100.;
    }
    if (Synt2Vec[step][syn2pat].accent!=Syntsett2.accent) {
      Syntsett2.accent=Synt2Vec[step][syn2pat].accent;
      SgDialSetValue(syn2accentDial, Syntsett2.accent);
      acc2=(double)Syntsett2.accent/10.;
    }
    if (Synt2Vec[step][syn2pat].pcf!=Syntsett2.pcf) {
      Syntsett2.pcf=Synt2Vec[step][syn2pat].pcf;
      if (Syntsett2.pcf) XmToggleButtonSetState(syn2pcfToggle, True, True);
        else XmToggleButtonSetState(syn2pcfToggle, False, True);
    }
    if (Synt2Vec[step][syn2pat].wave!=Syntsett2.wave) {
      Syntsett2.wave=Synt2Vec[step][syn2pat].wave;
      if (Syntsett2.wave==0) XmToggleButtonSetState(syn2waveSaw, True, True);
        else if (Syntsett2.wave==1) XmToggleButtonSetState(syn2waveSqr, True, True);
        else XmToggleButtonSetState(syn2waveSmp, True, True);
    }
  }
}

void StepSetSamp(int step)
{
  if (SampVec[step][samppat].set) {
    if (SampVec[step][samppat].vol!=Sampsett.vol) {
      Sampsett.vol=SampVec[step][samppat].vol;
      SgDialSetValue(sampvolDial, Sampsett.vol);
      sampvol=(double)Sampsett.vol/200.;
    }
    if (SampVec[step][samppat].pan!=Sampsett.pan) {
      Sampsett.pan=SampVec[step][samppat].pan;
      SgDialSetValue(samppanDial, Sampsett.pan);
      samppan=(double)Sampsett.pan/100.;
    }
    if (SampVec[step][samppat].distor!=Sampsett.distor) {
      Sampsett.distor=SampVec[step][samppat].distor;
      SgDialSetValue(sampdistDial, Sampsett.distor);
      distsamp=(double)Sampsett.distor/20.+1.;
    }
    if (SampVec[step][samppat].delay!=Sampsett.delay) {
      Sampsett.delay=SampVec[step][samppat].delay;
      SgDialSetValue(sampdelayDial, Sampsett.delay);
      delaysmp=(double)Sampsett.delay/100.;
    }
    if (SampVec[step][samppat].cutoff!=Sampsett.cutoff) {
      Sampsett.cutoff=SampVec[step][samppat].cutoff;
      SgDialSetValue(samplpCutDial, Sampsett.cutoff);
      fsamp=2.*PI*(double)Sampsett.cutoff/44100.;
    }
    if (SampVec[step][samppat].reson!=Sampsett.reson) {
      Sampsett.reson=SampVec[step][samppat].reson;
      SgDialSetValue(samplpResDial, Sampsett.reson);
      qsamp=1./((double)Sampsett.reson/10.);
    }
    if (SampVec[step][samppat].pcf!=Sampsett.pcf) {
      Sampsett.pcf=SampVec[step][samppat].pcf;
      if (Sampsett.pcf) XmToggleButtonSetState(samppcfToggle, True, True);
        else XmToggleButtonSetState(samppcfToggle, False, True);
    }
  }
}

void StepSetDrum(int step)
{
  if (DrumVec[step][drumpat].set) {
    if (DrumVec[step][drumpat].vol!=Drumsett.vol) {
      Drumsett.vol=DrumVec[step][drumpat].vol;
      SgDialSetValue(drumvolDial, Drumsett.vol);
      drumvol=(double)Drumsett.vol/100.;
    }
    if (DrumVec[step][drumpat].pan!=Drumsett.pan) {
      Drumsett.pan=DrumVec[step][drumpat].pan;
      SgDialSetValue(drumpanDial, Drumsett.pan);
      drumpan=(double)Drumsett.pan/100.;
    }
    if (DrumVec[step][drumpat].delay!=Drumsett.delay) {
      Drumsett.delay=DrumVec[step][drumpat].delay;
      SgDialSetValue(drumdelayDial, Drumsett.delay);
      drumdelay=(double)Drumsett.delay/100.;
    }
    if (DrumVec[step][drumpat].pcf!=Drumsett.pcf) {
      Drumsett.pcf=DrumVec[step][drumpat].pcf;
      if (Drumsett.pcf) XmToggleButtonSetState(drumpcfToggle, True, True);
        else XmToggleButtonSetState(drumpcfToggle, False, True);
    }
  }
}

void BpmDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;
  XmString text;
  char streng[50];

  bpm=calldata->position;
  bpmsamp=(int)(44100./(bpm/7.5)+0.5);
  bpmsamp6=bpmsamp*0.6;
  sprintf(streng,"%3d BPM", bpm);
  text=XmStringCreateLocalized(streng);
  XtVaSetValues(bpmLabel,XmNlabelString,text,NULL);
  XtFree(text);
  Globalsett.bpm=bpm;
}

void StepSlider(w, client, call)
Widget w;
XtPointer client, call;
{
  XmScaleCallbackStruct *calldata=(XmScaleCallbackStruct *) call;
  XmString text;
  char streng[50];

  autstep=calldata->value-1;
  sprintf(streng,"Step %2d", autstep+1);
  text=XmStringCreateLocalized(streng);
  XtVaSetValues(stepSlider,XmNtitleString,text,NULL);
  XtFree(text);
  switch (autwhere) {
    case 1: redraw_syn1(); break;
    case 2: redraw_syn2(); break;
    case 3: redraw_samp(); break;
    case 4: redraw_drum(); break;
  }
  RestoreWin(NULL, NULL, NULL);
  if (autstep<syn1patterns[syn1pat].numsteps) StepSet1(autstep);
  if (autstep<syn2patterns[syn2pat].numsteps) StepSet2(autstep);
  if (autstep<samppatterns[samppat].numsteps) StepSetSamp(autstep);
  if (autstep<drumpatterns[drumpat].numsteps) StepSetDrum(autstep);
}

void Syn1wave(w, client, call)
Widget w;
XtPointer client, call;
{
  if (XmToggleButtonGetState(syn1waveSaw)) wavetyp1=1;
  else if (XmToggleButtonGetState(syn1waveSqr)) wavetyp1=2;
  else wavetyp1=3;
  Syntsett1.wave=wavetyp1;
}

void Syn1beats(w, client, call)
Widget w;
XtPointer client, call;
{
  if (XmToggleButtonGetState(syn1beats8)) syn1patterns[syn1pat].numsteps=8;
  else if (XmToggleButtonGetState(syn1beats16)) syn1patterns[syn1pat].numsteps=16;
  else syn1patterns[syn1pat].numsteps=32;
  redraw_syn1(); RestoreWin(NULL,NULL,NULL);
}

void Syn2beats(w, client, call)
Widget w;
XtPointer client, call;
{
  if (XmToggleButtonGetState(syn2beats8)) syn2patterns[syn2pat].numsteps=8;
  else if (XmToggleButtonGetState(syn2beats16)) syn2patterns[syn2pat].numsteps=16;
  else syn2patterns[syn2pat].numsteps=32;
  redraw_syn2(); RestoreWin(NULL,NULL,NULL);
}

void Drumbeats(w, client, call)
Widget w;
XtPointer client, call;
{
  if (XmToggleButtonGetState(drumbeats8)) drumpatterns[drumpat].numsteps=8;
  else if (XmToggleButtonGetState(drumbeats16)) drumpatterns[drumpat].numsteps=16;
  else drumpatterns[drumpat].numsteps=32;
  redraw_drum(); RestoreWin(NULL,NULL,NULL);
}

void Syn1tuneDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  wf1=pow(2.,(double)(calldata->position)/12.)*27.5/44100.;
  Syntsett1.tune=calldata->position;
}

void Syn1pwmDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  pwm1=(double)(calldata->position)/15000000.;
  Syntsett1.pwm=calldata->position;
}

void Syn1decayDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  decay1=1.-(double)(calldata->position)/200000.;
  Syntsett1.decay=calldata->position;
}

void Syn1lpCutDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  f1=(3000.*(exp(calldata->position/50.)-1.)/(exp(2.)-1.))*TWOPI/44100.;
  Syntsett1.cutoff=calldata->position;
}

void Syn1lpResDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  q1=1./((double)(calldata->position)/10.);
  Syntsett1.reson=calldata->position;
}

void Syn1lpEnvDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  emod1=2.*PI*(double)(calldata->position)/44100.;
  Syntsett1.envmod=calldata->position;
}

void Syn1accentDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  acc1=(double)(calldata->position)/10.;
  Syntsett1.accent=calldata->position;
}

void Syn1volDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  vol1=(double)(calldata->position)/100.;
  Syntsett1.vol=calldata->position;
}

void Syn1panDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  syn1pan=(double)(calldata->position)/100.;
  Syntsett1.pan=calldata->position;
}

void Syn1distDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  dist1=(double)(calldata->position)+1.;
  Syntsett1.distor=calldata->position;
}

void Syn1delayDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  delay1=(double)(calldata->position)/100.;
  Syntsett1.delay=calldata->position;
}

void Syn1pcfToggle(w, client, call)
Widget w;
XtPointer client, call;
{
  if (XmToggleButtonGetState(syn1pcfToggle)) syn1pcf=1; else syn1pcf=0;
  Syntsett1.pcf=syn1pcf;
}

void Syn2wave(w, client, call)
Widget w;
XtPointer client, call;
{
  if (XmToggleButtonGetState(syn2waveSaw)) wavetyp2=1;
  else if (XmToggleButtonGetState(syn2waveSqr)) wavetyp2=2;
  else wavetyp2=3;
  Syntsett2.wave=wavetyp2;
}

void Syn2tuneDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  wf2=pow(2.,(double)(calldata->position)/12.)*27.5/44100.;
  Syntsett2.tune=calldata->position;
}

void Syn2pwmDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  pwm2=(double)(calldata->position)/15000000.;
  Syntsett2.pwm=calldata->position;
}

void Syn2decayDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  decay2=1.-(double)(calldata->position)/200000.;
  Syntsett2.decay=calldata->position;
}

void Syn2lpCutDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  f2=(3000.*(exp(calldata->position/50.)-1.)/(exp(2.)-1.))*TWOPI/44100.;
  Syntsett2.cutoff=calldata->position;
}

void Syn2lpResDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  q2=1./((double)(calldata->position)/10.);
  Syntsett2.reson=calldata->position;
}

void Syn2lpEnvDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  emod2=2.*PI*(double)(calldata->position)/44100.;
  Syntsett2.envmod=calldata->position;
}

void Syn2accentDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  acc2=(double)(calldata->position)/10.;
  Syntsett2.accent=calldata->position;
}

void Syn2volDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  vol2=(double)(calldata->position)/100.;
  Syntsett2.vol=calldata->position;
}

void Syn2panDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  syn2pan=(double)(calldata->position)/100.;
  Syntsett2.pan=calldata->position;
}

void Syn2distDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  dist2=(double)(calldata->position)+1.;
  Syntsett2.distor=calldata->position;
}

void Syn2delayDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  delay2=(double)(calldata->position)/100.;
  Syntsett2.delay=calldata->position;
}

void Syn2pcfToggle(w, client, call)
Widget w;
XtPointer client, call;
{
  if (XmToggleButtonGetState(syn2pcfToggle)) syn2pcf=1; else syn2pcf=0;
  Syntsett2.pcf=syn2pcf;
}

void SamplpCutDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

/*  fsamp=2.*PI*(double)(calldata->position)/44100.; */
  fsamp=(3000.*(exp(calldata->position/50.)-1.)/(exp(2.)-1.))*TWOPI/44100.;
  Sampsett.cutoff=calldata->position;
}

void SamplpResDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  qsamp=1./((double)(calldata->position)/10.);
  Sampsett.reson=calldata->position;
}

void SampvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  sampvol=(double)(calldata->position)/200.;
  Sampsett.vol=calldata->position;
}

void SamppanDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  samppan=(double)(calldata->position)/100.;
  Sampsett.pan=calldata->position;
}

void SampdistDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  distsamp=(double)(calldata->position)/20.+1.;
  Sampsett.distor=calldata->position;
}

void SampdelayDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  delaysmp=(double)(calldata->position)/100.;
  Sampsett.delay=calldata->position;
}

void SamppcfToggle(w, client, call)
Widget w;
XtPointer client, call;
{
  if (XmToggleButtonGetState(samppcfToggle)) samppcf=1; else samppcf=0;
  Sampsett.pcf=samppcf;
}

void DelayTimeDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  delaytime=(int)(calldata->position);
  Globalsett.steps=calldata->position;
}

void DelayPanDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  delaypan=(double)(calldata->position)/100.;
  Globalsett.pan=calldata->position;
}

void DelayFeedDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  delayfeed=(double)(calldata->position)/100.;
  Globalsett.fback=calldata->position;
}

void PcfFrqDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  pcffrq=(double)(calldata->position);
  Globalsett.freq=calldata->position;
}

void PcfQDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  pcfq=1./((double)(calldata->position)/10.);
  Globalsett.reson=calldata->position;
}

void PcfAmntDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  pcfamnt=(double)(calldata->position)/100.;
  Globalsett.amnt=calldata->position;
}

void PcfDecayDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  pcfdecay=1.-(double)(calldata->position)/500000.;
  Globalsett.decay=calldata->position;
}

void PcfPatternDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;
  XmString text;
  char streng[50];

  pcfpattern=calldata->position;
  sprintf(streng,"Pat %d", pcfpattern);
  text=XmStringCreateLocalized(streng);
  XtVaSetValues(pcfPatternLabel,XmNlabelString,text,NULL);
  XtFree(text);
  Globalsett.pat=calldata->position;
}

void PcfBpToggle(w, client, call)
Widget w;
XtPointer client, call;
{
  if (XmToggleButtonGetState(pcfBpToggle)) pcfBp=1; else pcfBp=0;
  Globalsett.bp=pcfBp;
}

void DrumBvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drums[0].vol=(double)(calldata->position)/200.;
  Drumsett.bassvol=calldata->position;
}

void DrumBtypeDial(w, client, call)
Widget w;
XtPointer client, call;
{
  int tune, attack, decay, tr808;

  if (!XmToggleButtonGetState(drumBtr808)) {
    SgDialGetValue(drumBtuneDial, &tune);
    Drumsett.basstune=tune; tune=(tune*4)/11;
    SgDialGetValue(drumBattackDial, &attack);
    Drumsett.bassattck=attack; attack=(attack*2)/11;
    SgDialGetValue(drumBdecayDial, &decay);
    Drumsett.bassdecay=decay; decay=(decay*4)/11;
    drums[0].type=tune*6+(attack?4+decay/3:decay);
  } else {
    SgDialGetValue(drumBattackDial, &attack);
    Drumsett.bassattck=attack; attack=(attack*5)/11;
    SgDialGetValue(drumBdecayDial, &decay);
    Drumsett.bassdecay=decay; decay=(decay*5)/11;
    drums[0].type=attack*5+decay+24;
  }
}

void DrumSvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drums[1].vol=(double)(calldata->position)/200.;
  Drumsett.snarevol=calldata->position;
}

void DrumStypeDial(w, client, call)
Widget w;
XtPointer client, call;
{
  int tune, tone, snappy;

  if (!XmToggleButtonGetState(drumStr808)) {
    SgDialGetValue(drumStuneDial, &tune);
    Drumsett.snaretune=tune; tune=(tune*4)/11;
    SgDialGetValue(drumStoneDial, &tone);
    Drumsett.snaretone=tone; tone=(tone*4)/11;
    SgDialGetValue(drumSsnappyDial, &snappy);
    Drumsett.snaresnap=snappy; snappy=(snappy*4)/11;
    drums[1].type=tune*13+(tone?tone*3+(snappy*3)/4+1:snappy);
  } else {
    SgDialGetValue(drumStuneDial, &tune);
    Drumsett.snaretune=tune; tune=(tune*5)/11;
    SgDialGetValue(drumSsnappyDial, &snappy);
    Drumsett.snaresnap=snappy; snappy=(snappy*5)/11;
    drums[1].type=tune*5+snappy+52;
  }
}

void DrumLvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drums[2].vol=(double)(calldata->position)/200.;
  Drumsett.ltomvol=calldata->position;
}

void DrumLtypeDial(w, client, call)
Widget w;
XtPointer client, call;
{
  int tune, decay;

  SgDialGetValue(drumLtuneDial, &tune);
  Drumsett.ltomtune=tune; tune=(tune*4)/11;
  SgDialGetValue(drumLdecayDial, &decay);
  Drumsett.ltomdecay=decay; decay=(decay*4)/11;
  drums[2].type=tune*4+decay;
}

void DrumMvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drums[3].vol=(double)(calldata->position)/200.;
  Drumsett.mtomvol=calldata->position;
}

void DrumMtypeDial(w, client, call)
Widget w;
XtPointer client, call;
{
  int tune, decay;

  SgDialGetValue(drumMtuneDial, &tune);
  Drumsett.mtomtune=tune; tune=(tune*4)/11;
  SgDialGetValue(drumMdecayDial, &decay);
  Drumsett.mtomdecay=decay; decay=(decay*4)/11;
  drums[3].type=tune*4+decay;
}

void DrumHvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drums[4].vol=(double)(calldata->position)/200.;
  Drumsett.htomvol=calldata->position;
}

void DrumHtypeDial(w, client, call)
Widget w;
XtPointer client, call;
{
  int tune, decay;

  SgDialGetValue(drumHtuneDial, &tune);
  Drumsett.htomtune=tune; tune=(tune*4)/11;
  SgDialGetValue(drumHdecayDial, &decay);
  Drumsett.htomdecay=decay; decay=(decay*4)/11;
  drums[4].type=tune*4+decay;
}

void DrumRIMvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drums[5].vol=(double)(calldata->position)/200.;
  Drumsett.rimshvol=calldata->position;
}

void DrumRIMtypeDial(w, client, call)
Widget w;
XtPointer client, call;
{
  int velocity;

  SgDialGetValue(drumRIMvelDial, &velocity);
  Drumsett.rimshveloc=velocity; velocity=(velocity*2)/11;
  drums[5].type=velocity;
}

void DrumHANvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drums[6].vol=(double)(calldata->position)/200.;
  Drumsett.hclapvol=calldata->position;
}

void DrumHANtypeDial(w, client, call)
Widget w;
XtPointer client, call;
{
  int velocity;

  SgDialGetValue(drumHANvelDial, &velocity);
  Drumsett.hclapveloc=velocity; velocity=(velocity*2)/11;
  drums[6].type=velocity;
}

void DrumHHCvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drums[7].vol=(double)(calldata->position)/200.;
  Drumsett.clhhvol=calldata->position;
}

void DrumHHCtypeDial(w, client, call)
Widget w;
XtPointer client, call;
{
  int decay;

  SgDialGetValue(drumHHCdecayDial, &decay);
  Drumsett.clhhdecay=decay; decay=(decay*6)/11;
  drums[7].type=decay;
}

void DrumHHOvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drums[8].vol=(double)(calldata->position)/200.;
  Drumsett.ophhvol=calldata->position;
}

void DrumHHOtypeDial(w, client, call)
Widget w;
XtPointer client, call;
{
  int decay;

  SgDialGetValue(drumHHOdecayDial, &decay);
  Drumsett.ophhdecay=decay; decay=(decay*6)/11;
  drums[8].type=decay;
}

void DrumCSHvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drums[9].vol=(double)(calldata->position)/200.;
  Drumsett.crashvol=calldata->position;
}

void DrumCSHtypeDial(w, client, call)
Widget w;
XtPointer client, call;
{
  int tune;

  SgDialGetValue(drumCSHtuneDial, &tune);
  Drumsett.crashtune=tune; tune=(tune*6)/11;
  drums[9].type=tune;
}

void DrumRIDvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drums[10].vol=(double)(calldata->position)/200.;
  Drumsett.ridevol=calldata->position;
}

void DrumRIDtypeDial(w, client, call)
Widget w;
XtPointer client, call;
{
  int tune;

  SgDialGetValue(drumRIDtuneDial, &tune);
  Drumsett.ridetune=tune; tune=(tune*6)/11;
  drums[10].type=tune;
}

void DrumCLOvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drums[11].vol=(double)(calldata->position)/200.;
  Drumsett.cohhvol=calldata->position;
}

void DrumCLOtypeDial(w, client, call)
Widget w;
XtPointer client, call;
{
  int combi;

  SgDialGetValue(drumCLOcombDial, &combi);
  Drumsett.cohhcombi=combi; combi=(combi*4)/11;
  drums[11].type=combi;
}

void DrumOPCvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drums[12].vol=(double)(calldata->position)/200.;
  Drumsett.ochhvol=calldata->position;
}

void DrumOPCtypeDial(w, client, call)
Widget w;
XtPointer client, call;
{
  int combi;

  SgDialGetValue(drumOPCcombDial, &combi);
  Drumsett.ochhcombi=combi; combi=(combi*4)/11;
  drums[12].type=combi;
}

void DrumvolDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drumvol=(double)(calldata->position)/100.;
  Drumsett.vol=calldata->position;
}

void DrumpanDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drumpan=(double)(calldata->position)/100.;
  Drumsett.pan=calldata->position;
}

void DrumdelayDial(w, client, call)
Widget w;
XtPointer client, call;
{
  SgDialCallbackStruct *calldata=(SgDialCallbackStruct *) call;

  drumdelay=(double)(calldata->position)/100.;
  Drumsett.delay=calldata->position;
}

void DrumpcfToggle(w, client, call)
Widget w;
XtPointer client, call;
{
  if (XmToggleButtonGetState(drumpcfToggle)) drumpcf=1; else drumpcf=0;
  Drumsett.pcf=drumpcf;
}

void Syn1patBut(w, client, call)
Widget w;
XtPointer client, call;
{
  int i=*(int *)client;

  XtVaSetValues(syn1patBut[syn1pat], XmNbackground, gray.pixel,NULL);
  XtVaSetValues(syn1patBut[i], XmNbackground, red.pixel,NULL);
  syn1pat=i;
  redraw_syn1(); RestoreWin(NULL, NULL, NULL);
}

void Syn2patBut(w, client, call)
Widget w;
XtPointer client, call;
{
  int i=*(int *)client;

  XtVaSetValues(syn2patBut[syn2pat], XmNbackground, gray.pixel,NULL);
  XtVaSetValues(syn2patBut[i], XmNbackground, red.pixel,NULL);
  syn2pat=i;
  redraw_syn2(); RestoreWin(NULL, NULL, NULL);
}

void SamppatBut(w, client, call)
Widget w;
XtPointer client, call;
{
  int i=*(int *)client;

  XtVaSetValues(samppatBut[samppat], XmNbackground, gray.pixel,NULL);
  XtVaSetValues(samppatBut[i], XmNbackground, red.pixel,NULL);
  samppat=i;
  redraw_samp(); RestoreWin(NULL, NULL, NULL);
}

void DrumpatBut(w, client, call)
Widget w;
XtPointer client, call;
{
  int i=*(int *)client;

  XtVaSetValues(drumpatBut[drumpat], XmNbackground, gray.pixel,NULL);
  XtVaSetValues(drumpatBut[i], XmNbackground, red.pixel,NULL);
  drumpat=i;
  redraw_drum(); RestoreWin(NULL, NULL, NULL);
}

void writeknobs(FILE *fil)
{
  int v;

  fprintf(fil, "%d\n", Syntsett1.tune);
  fprintf(fil, "%d\n", Syntsett1.pwm);
  fprintf(fil, "%d\n", Syntsett1.decay);
  fprintf(fil, "%d\n", Syntsett1.cutoff);
  fprintf(fil, "%d\n", Syntsett1.reson);
  fprintf(fil, "%d\n", Syntsett1.envmod);
  fprintf(fil, "%d\n", Syntsett1.vol);
  fprintf(fil, "%d\n", Syntsett1.pan);
  fprintf(fil, "%d\n", Syntsett1.distor);
  fprintf(fil, "%d\n", Syntsett1.delay);
  fprintf(fil, "%d\n", Syntsett1.accent);
  fprintf(fil, "%d\n", Syntsett1.wave);
  fprintf(fil, "%d\n", Syntsett1.pcf);

  fprintf(fil, "%d\n", Syntsett2.tune);
  fprintf(fil, "%d\n", Syntsett2.pwm);
  fprintf(fil, "%d\n", Syntsett2.decay);
  fprintf(fil, "%d\n", Syntsett2.cutoff);
  fprintf(fil, "%d\n", Syntsett2.reson);
  fprintf(fil, "%d\n", Syntsett2.envmod);
  fprintf(fil, "%d\n", Syntsett2.vol);
  fprintf(fil, "%d\n", Syntsett2.pan);
  fprintf(fil, "%d\n", Syntsett2.distor);
  fprintf(fil, "%d\n", Syntsett2.delay);
  fprintf(fil, "%d\n", Syntsett2.accent);
  fprintf(fil, "%d\n", Syntsett2.wave);
  fprintf(fil, "%d\n", Syntsett2.pcf);

  fprintf(fil, "%d\n", Sampsett.vol);
  fprintf(fil, "%d\n", Sampsett.pan);
  fprintf(fil, "%d\n", Sampsett.distor);
  fprintf(fil, "%d\n", Sampsett.delay);
  fprintf(fil, "%d\n", Sampsett.cutoff);
  fprintf(fil, "%d\n", Sampsett.reson);
  fprintf(fil, "%d\n", Sampsett.pcf);

  fprintf(fil, "%d\n", Drumsett.vol);
  fprintf(fil, "%d\n", Drumsett.pan);
  fprintf(fil, "%d\n", Drumsett.delay);
  fprintf(fil, "%d\n", Drumsett.pcf);
  fprintf(fil, "%d\n", Drumsett.bassvol);
  fprintf(fil, "%d\n", Drumsett.basstune);
  fprintf(fil, "%d\n", Drumsett.bassattck);
  fprintf(fil, "%d\n", Drumsett.bassdecay);
  fprintf(fil, "%d\n", Drumsett.bass808);
  fprintf(fil, "%d\n", Drumsett.snarevol);
  fprintf(fil, "%d\n", Drumsett.snaretune);
  fprintf(fil, "%d\n", Drumsett.snaretone);
  fprintf(fil, "%d\n", Drumsett.snaresnap);
  fprintf(fil, "%d\n", Drumsett.snare808);
  fprintf(fil, "%d\n", Drumsett.ltomvol);
  fprintf(fil, "%d\n", Drumsett.ltomtune);
  fprintf(fil, "%d\n", Drumsett.ltomdecay);
  fprintf(fil, "%d\n", Drumsett.mtomvol);
  fprintf(fil, "%d\n", Drumsett.mtomtune);
  fprintf(fil, "%d\n", Drumsett.mtomdecay);
  fprintf(fil, "%d\n", Drumsett.htomvol);
  fprintf(fil, "%d\n", Drumsett.htomtune);
  fprintf(fil, "%d\n", Drumsett.htomdecay);
  fprintf(fil, "%d\n", Drumsett.rimshvol);
  fprintf(fil, "%d\n", Drumsett.rimshveloc);
  fprintf(fil, "%d\n", Drumsett.hclapvol);
  fprintf(fil, "%d\n", Drumsett.hclapveloc);
  fprintf(fil, "%d\n", Drumsett.clhhvol);
  fprintf(fil, "%d\n", Drumsett.clhhdecay);
  fprintf(fil, "%d\n", Drumsett.ophhvol);
  fprintf(fil, "%d\n", Drumsett.ophhdecay);
  fprintf(fil, "%d\n", Drumsett.crashvol);
  fprintf(fil, "%d\n", Drumsett.crashtune);
  fprintf(fil, "%d\n", Drumsett.ridevol);
  fprintf(fil, "%d\n", Drumsett.ridetune);
  fprintf(fil, "%d\n", Drumsett.cohhvol);
  fprintf(fil, "%d\n", Drumsett.cohhcombi);
  fprintf(fil, "%d\n", Drumsett.ochhvol);
  fprintf(fil, "%d\n", Drumsett.ochhcombi);

  fprintf(fil, "%d\n", Globalsett.freq);
  fprintf(fil, "%d\n", Globalsett.reson);
  fprintf(fil, "%d\n", Globalsett.amnt);
  fprintf(fil, "%d\n", Globalsett.decay);
  fprintf(fil, "%d\n", Globalsett.pat);
  fprintf(fil, "%d\n", Globalsett.bp);
  fprintf(fil, "%d\n", Globalsett.steps);
  fprintf(fil, "%d\n", Globalsett.pan);
  fprintf(fil, "%d\n", Globalsett.fback);
  fprintf(fil, "%d\n", Globalsett.bpm);
}

void readknobs(FILE *fil)
{
  int v, d;
  XmString text;
  char streng[50];

  fscanf(fil, "%d", &v); Syntsett1.tune=v;
  SgDialSetValue(syn1tuneDial, v); wf1=pow(2.,(double)v/12.)*27.5/44100.;
  fscanf(fil, "%d", &v); Syntsett1.pwm=v;
  SgDialSetValue(syn1pwmDial, v); pwm1=(double)v/15000000.;
  fscanf(fil, "%d", &v); Syntsett1.decay=v;
  SgDialSetValue(syn1decayDial, v); decay1=1.-(double)v/200000.;
  fscanf(fil, "%d", &v); Syntsett1.cutoff=v;
  SgDialSetValue(syn1lpCutDial, v);
  f1=(3000.*(exp(v/50.)-1.)/(exp(2.)-1.))*TWOPI/44100.;
  fscanf(fil, "%d", &v); Syntsett1.reson=v;
  SgDialSetValue(syn1lpResDial, v); q1=1./((double)v/10.);
  fscanf(fil, "%d", &v); Syntsett1.envmod=v;
  SgDialSetValue(syn1lpEnvDial, v); emod1=2.*PI*(double)v/44100.;
  fscanf(fil, "%d", &v); Syntsett1.vol=v;
  SgDialSetValue(syn1volDial, v); vol1=(double)v/100.;
  fscanf(fil, "%d", &v); Syntsett1.pan=v;
  SgDialSetValue(syn1panDial, v); syn1pan=(double)v/100.;
  fscanf(fil, "%d", &v); Syntsett1.distor=v;
  SgDialSetValue(syn1distDial, v); dist1=(double)v+1.;
  fscanf(fil, "%d", &v); Syntsett1.delay=v;
  SgDialSetValue(syn1delayDial, v); delay1=(double)v/100.;
  fscanf(fil, "%d", &v); Syntsett1.accent=v;
  SgDialSetValue(syn1accentDial, v); acc1=(double)v/10.;
  fscanf(fil, "%d", &v); wavetyp1=v; Syntsett1.wave=v;
  if (v==1) XmToggleButtonSetState(syn1waveSaw, True, True);
    else if (v==2) XmToggleButtonSetState(syn1waveSqr, True, True);
      else XmToggleButtonSetState(syn1waveSmp, True, True);
  fscanf(fil, "%d", &v); syn1pcf=v; Syntsett1.pcf=v;
  if (v) XmToggleButtonSetState(syn1pcfToggle, True, True);
    else XmToggleButtonSetState(syn1pcfToggle, False, True);

  fscanf(fil, "%d", &v); Syntsett2.tune=v;
  SgDialSetValue(syn2tuneDial, v); wf2=pow(2.,(double)v/12.)*27.5/44100.;
  fscanf(fil, "%d", &v); Syntsett2.pwm=v;
  SgDialSetValue(syn2pwmDial, v); pwm2=(double)v/15000000.;
  fscanf(fil, "%d", &v); Syntsett2.decay=v;
  SgDialSetValue(syn2decayDial, v); decay2=1.-(double)v/200000.;
  fscanf(fil, "%d", &v); Syntsett2.cutoff=v;
  SgDialSetValue(syn2lpCutDial, v);
  f2=(3000.*(exp(v/50.)-1.)/(exp(2.)-1.))*TWOPI/44100.;
  fscanf(fil, "%d", &v); Syntsett2.reson=v;
  SgDialSetValue(syn2lpResDial, v); q2=1./((double)v/10.);
  fscanf(fil, "%d", &v); Syntsett2.envmod=v;
  SgDialSetValue(syn2lpEnvDial, v); emod2=2.*PI*(double)v/44100.;
  fscanf(fil, "%d", &v); Syntsett2.vol=v;
  SgDialSetValue(syn2volDial, v); vol2=(double)v/100.;
  fscanf(fil, "%d", &v); Syntsett2.pan=v;
  SgDialSetValue(syn2panDial, v); syn2pan=(double)v/100.;
  fscanf(fil, "%d", &v); Syntsett2.distor=v;
  SgDialSetValue(syn2distDial, v); dist2=(double)v+1.;
  fscanf(fil, "%d", &v); Syntsett2.delay=v;
  SgDialSetValue(syn2delayDial, v); delay2=(double)v/100.;
  fscanf(fil, "%d", &v); Syntsett2.accent=v;
  SgDialSetValue(syn2accentDial, v); acc2=(double)v/10.;
  fscanf(fil, "%d", &v); wavetyp2=v; Syntsett2.wave=v;
  if (v==1) XmToggleButtonSetState(syn2waveSaw, True, True);
    else if (v==2) XmToggleButtonSetState(syn2waveSqr, True, True);
      else XmToggleButtonSetState(syn2waveSmp, True, True);
  fscanf(fil, "%d", &v); syn2pcf=v; Syntsett2.pcf=v;
  if (v) XmToggleButtonSetState(syn2pcfToggle, True, True);
    else XmToggleButtonSetState(syn2pcfToggle, False, True);

  fscanf(fil, "%d", &v); Sampsett.vol=v;
  SgDialSetValue(sampvolDial, v); sampvol=(double)v/200.;
  fscanf(fil, "%d", &v); Sampsett.pan=v;
  SgDialSetValue(samppanDial, v); samppan=(double)v/100.;
  fscanf(fil, "%d", &v); Sampsett.distor=v;
  SgDialSetValue(sampdistDial, v); distsamp=(double)v/20.+1.;
  fscanf(fil, "%d", &v); Sampsett.delay=v;
  SgDialSetValue(sampdelayDial, v); delaysmp=(double)v/100.;
  fscanf(fil, "%d", &v); Sampsett.cutoff=v;
  SgDialSetValue(samplpCutDial, v);
  fsamp=(3000.*(exp(v/50.)-1.)/(exp(2.)-1.))*TWOPI/44100.;
  fscanf(fil, "%d", &v); Sampsett.reson=v;
  SgDialSetValue(samplpResDial, v); qsamp=1./((double)v/10.);
  fscanf(fil, "%d", &v); samppcf=v; Sampsett.pcf=v;
  if (v) XmToggleButtonSetState(samppcfToggle, True, True);
    else XmToggleButtonSetState(samppcfToggle, False, True);

  fscanf(fil, "%d", &v); Drumsett.vol=v;
  SgDialSetValue(drumvolDial, v); drumvol=(double)v/100.;
  fscanf(fil, "%d", &v); Drumsett.pan=v;
  SgDialSetValue(drumpanDial, v); drumpan=(double)v/100.;
  fscanf(fil, "%d", &v); Drumsett.delay=v;
  SgDialSetValue(drumdelayDial, v); drumdelay=(double)v/100.;
  fscanf(fil, "%d", &v); drumpcf=v; Drumsett.pcf=v;
  if (v) XmToggleButtonSetState(drumpcfToggle, True, True);
    else XmToggleButtonSetState(drumpcfToggle, False, True);
  fscanf(fil, "%d", &Drumsett.bassvol);
  drums[0].vol=(double)(Drumsett.bassvol)/200.;
  SgDialSetValue(drumBvolDial, Drumsett.bassvol);
  fscanf(fil, "%d", &Drumsett.basstune);
  SgDialSetValue(drumBtuneDial, Drumsett.basstune);
  fscanf(fil, "%d", &Drumsett.bassattck);
  SgDialSetValue(drumBattackDial, Drumsett.bassattck);
  fscanf(fil, "%d", &Drumsett.bassdecay);
  SgDialSetValue(drumBdecayDial, Drumsett.bassdecay);
  fscanf(fil, "%d", &Drumsett.bass808);
  if (Drumsett.bass808) XmToggleButtonSetState(drumBtr808, True, True);
    else XmToggleButtonSetState(drumBtr808, False, True);
  DrumBtypeDial(NULL,NULL,NULL);
  fscanf(fil, "%d", &Drumsett.snarevol);
  drums[1].vol=(double)(Drumsett.snarevol)/200.;
  SgDialSetValue(drumSvolDial, Drumsett.snarevol);
  fscanf(fil, "%d", &Drumsett.snaretune);
  SgDialSetValue(drumStuneDial, Drumsett.snaretune);
  fscanf(fil, "%d", &Drumsett.snaretone);
  SgDialSetValue(drumStoneDial, Drumsett.snaretone);
  fscanf(fil, "%d", &Drumsett.snaresnap);
  SgDialSetValue(drumSsnappyDial, Drumsett.snaresnap);
  fscanf(fil, "%d", &Drumsett.snare808);
  if (Drumsett.snare808) XmToggleButtonSetState(drumStr808, True, True);
    else XmToggleButtonSetState(drumStr808, False, True);
  DrumStypeDial(NULL,NULL,NULL);
  fscanf(fil, "%d", &Drumsett.ltomvol);
  drums[2].vol=(double)(Drumsett.ltomvol)/200.;
  SgDialSetValue(drumLvolDial, Drumsett.ltomvol);
  fscanf(fil, "%d", &Drumsett.ltomtune);
  SgDialSetValue(drumLtuneDial, Drumsett.ltomtune);
  fscanf(fil, "%d", &Drumsett.ltomdecay);
  SgDialSetValue(drumLdecayDial, Drumsett.ltomdecay);
  DrumLtypeDial(NULL,NULL,NULL);
  fscanf(fil, "%d", &Drumsett.mtomvol);
  drums[3].vol=(double)(Drumsett.mtomvol)/200.;
  SgDialSetValue(drumMvolDial, Drumsett.mtomvol);
  fscanf(fil, "%d", &Drumsett.mtomtune);
  SgDialSetValue(drumMtuneDial, Drumsett.mtomtune);
  fscanf(fil, "%d", &Drumsett.mtomdecay);
  SgDialSetValue(drumMdecayDial, Drumsett.mtomdecay);
  DrumMtypeDial(NULL,NULL,NULL);
  fscanf(fil, "%d", &Drumsett.htomvol);
  drums[4].vol=(double)(Drumsett.htomvol)/200.;
  SgDialSetValue(drumHvolDial, Drumsett.htomvol);
  fscanf(fil, "%d", &Drumsett.htomtune);
  SgDialSetValue(drumHtuneDial, Drumsett.htomtune);
  fscanf(fil, "%d", &Drumsett.htomdecay);
  SgDialSetValue(drumHdecayDial, Drumsett.htomdecay);
  DrumHtypeDial(NULL,NULL,NULL);
  fscanf(fil, "%d", &Drumsett.rimshvol);
  drums[5].vol=(double)(Drumsett.rimshvol)/200.;
  SgDialSetValue(drumRIMvolDial, Drumsett.rimshvol);
  fscanf(fil, "%d", &Drumsett.rimshveloc);
  SgDialSetValue(drumRIMvelDial, Drumsett.rimshveloc);
  DrumRIMtypeDial(NULL,NULL,NULL);
  fscanf(fil, "%d", &Drumsett.hclapvol);
  drums[6].vol=(double)(Drumsett.hclapvol)/200.;
  SgDialSetValue(drumHANvolDial, Drumsett.hclapvol);
  DrumHANtypeDial(NULL,NULL,NULL);
  fscanf(fil, "%d", &Drumsett.hclapveloc);
  SgDialSetValue(drumHANvelDial, Drumsett.hclapveloc);
  fscanf(fil, "%d", &Drumsett.clhhvol);
  drums[7].vol=(double)(Drumsett.clhhvol)/200.;
  SgDialSetValue(drumHHCvolDial, Drumsett.clhhvol);
  fscanf(fil, "%d", &Drumsett.clhhdecay);
  SgDialSetValue(drumHHCdecayDial, Drumsett.clhhdecay);
  DrumHHCtypeDial(NULL,NULL,NULL);
  fscanf(fil, "%d", &Drumsett.ophhvol);
  drums[8].vol=(double)(Drumsett.ophhvol)/200.;
  SgDialSetValue(drumHHOvolDial, Drumsett.ophhvol);
  fscanf(fil, "%d", &Drumsett.ophhdecay);
  SgDialSetValue(drumHHOdecayDial, Drumsett.ophhdecay);
  DrumHHOtypeDial(NULL,NULL,NULL);
  fscanf(fil, "%d", &Drumsett.crashvol);
  drums[9].vol=(double)(Drumsett.crashvol)/200.;
  SgDialSetValue(drumCSHvolDial, Drumsett.crashvol);
  fscanf(fil, "%d", &Drumsett.crashtune);
  SgDialSetValue(drumCSHtuneDial, Drumsett.crashtune);
  DrumCSHtypeDial(NULL,NULL,NULL);
  fscanf(fil, "%d", &Drumsett.ridevol);
  drums[10].vol=(double)(Drumsett.ridevol)/200.;
  SgDialSetValue(drumRIDvolDial, Drumsett.ridevol);
  DrumRIDtypeDial(NULL,NULL,NULL);
  fscanf(fil, "%d", &Drumsett.ridetune);
  SgDialSetValue(drumRIDtuneDial, Drumsett.ridetune);
  fscanf(fil, "%d", &Drumsett.cohhvol);
  drums[11].vol=(double)(Drumsett.cohhvol)/200.;
  SgDialSetValue(drumCLOvolDial, Drumsett.cohhvol);
  fscanf(fil, "%d", &Drumsett.cohhcombi);
  SgDialSetValue(drumCLOcombDial, Drumsett.cohhcombi);
  DrumCLOtypeDial(NULL,NULL,NULL);
  fscanf(fil, "%d", &Drumsett.ochhvol);
  drums[12].vol=(double)(Drumsett.ochhvol)/200.;
  SgDialSetValue(drumOPCvolDial, Drumsett.ochhvol);
  fscanf(fil, "%d", &Drumsett.ochhcombi);
  SgDialSetValue(drumOPCcombDial, Drumsett.ochhcombi);
  DrumOPCtypeDial(NULL,NULL,NULL);

  fscanf(fil, "%d", &v); Globalsett.freq=v;
  SgDialSetValue(pcfFrqDial, v); pcffrq=(double)v;
  fscanf(fil, "%d", &v); Globalsett.reson=v;
  SgDialSetValue(pcfQDial, v); pcfq=1./((double)v/10.);
  fscanf(fil, "%d", &v); Globalsett.amnt=v;
  SgDialSetValue(pcfAmntDial, v); pcfamnt=(double)v/100.;
  fscanf(fil, "%d", &v); Globalsett.decay=v;
  SgDialSetValue(pcfDecayDial, v);  pcfdecay=1.-(double)v/500000.;
  fscanf(fil, "%d", &v); Globalsett.pat=v;
  SgDialSetValue(pcfPatternDial, v); pcfpattern=v;
  sprintf(streng,"Pat %d", pcfpattern);
  text=XmStringCreateLocalized(streng);
  XtVaSetValues(pcfPatternLabel,XmNlabelString,text,NULL);
  XtFree(text);
  fscanf(fil, "%d", &v); Globalsett.bp=v; pcfBp=v;
  if (v) XmToggleButtonSetState(pcfBpToggle, True, True);
    else XmToggleButtonSetState(pcfBpToggle, False, True);
  fscanf(fil, "%d", &v); Globalsett.steps=v;
  SgDialSetValue(delayTimeDial, v); delaytime=v;
  fscanf(fil, "%d", &v); Globalsett.pan=v;
  SgDialSetValue(delayPanDial, v); delaypan=(double)v/100.;
  fscanf(fil, "%d", &v); Globalsett.fback=v;
  SgDialSetValue(delayFeedDial, v); delayfeed=(double)v/100.;
 
  fscanf(fil, "%d", &v); Globalsett.bpm=v;
  SgDialSetValue(bpmDial, v); bpm=v;
  bpmsamp=(int)(44100./(bpm/7.5)+0.5);
  bpmsamp6=bpmsamp*0.6;
  sprintf(streng,"%3d BPM", bpm);
  text=XmStringCreateLocalized(streng);
  XtVaSetValues(bpmLabel,XmNlabelString,text,NULL);
  XtFree(text);
}

void Save(w, client, call)
Widget w;
XtPointer client, call;
{
  XtManageChild(saveFileBox);
}

void SaveOk(w, client, call)
Widget w;
XtPointer client, call;
{
  XmFileSelectionBoxCallbackStruct
    *calldata=(XmFileSelectionBoxCallbackStruct*) call; 
  char *filename;
  FILE *fil;
  int pat, j, k;
  static XmStringCharSet charset =
                           (XmStringCharSet) XmSTRING_DEFAULT_CHARSET;

  XmStringGetLtoR(calldata->value, charset, &filename);

  fil=fopen(filename, "w");
  writeknobs(fil);
  
  for (pat=0; pat<32; pat++) {
    fprintf(fil, "%d\n", syn1patterns[pat].numsteps);
    for (j=0; j<syn1patterns[pat].numsteps; j++) {
      fprintf(fil, "%d %d %d\n", syn1patterns[pat].pitch[j],
        syn1patterns[pat].dur[j], syn1patterns[pat].accent[j]);
    }
    fprintf(fil, "%d\n", syn2patterns[pat].numsteps);
    for (j=0; j<syn2patterns[pat].numsteps; j++) {
      fprintf(fil, "%d %d %d\n", syn2patterns[pat].pitch[j],
        syn2patterns[pat].dur[j], syn2patterns[pat].accent[j]);
    }
    fprintf(fil, "%d\n", samppatterns[pat].numsteps);
    for (j=0; j<samppatterns[pat].numsteps; j++) {
      for (k=0; k<13; k++)
        fprintf(fil, "%d ", samppatterns[pat].step[j][k]);
      fprintf(fil, "\n");
    }
    fprintf(fil, "%d\n", drumpatterns[pat].numsteps);
    for (j=0; j<drumpatterns[pat].numsteps; j++) {
      for (k=0; k<13; k++)
        fprintf(fil, "%d ", drumpatterns[pat].step[j][k]);
      fprintf(fil, "\n");
    }
  }
  for (k=0; k<4; k++) {
    for (j=0; j<64; j++) {
      if (XbaeMatrixGetCell(songMatrix, k, j)==NULL) pat=0;
        else pat=atoi(XbaeMatrixGetCell(songMatrix, k, j));
      if ((pat<0) || (pat>8)) pat=0; 
      fprintf(fil, "%d ", pat);
    }
    fprintf(fil, "\n");
  }
  fputc('.', fil);
  for (k=0; k<32; k++) for (j=0; j<8; j++)
    fwrite(&Synt1Vec[k][j], sizeof(syntsett), 1, fil);
  for (k=0; k<32; k++) for (j=0; j<8; j++)
    fwrite(&Synt2Vec[k][j], sizeof(syntsett), 1, fil);
  for (k=0; k<32; k++) for (j=0; j<8; j++)
    fwrite(&SampVec[k][j], sizeof(sampsett), 1, fil);
  for (k=0; k<32; k++) for (j=0; j<8; j++)
    fwrite(&DrumVec[k][j], sizeof(drumsett), 1, fil);
  fclose(fil);

  XtFree(filename);
  XtUnmanageChild(saveFileBox);
}

void Load(w, client, call)
Widget w;
XtPointer client, call;
{
  XtManageChild(loadFileBox);
}

void LoadOk(w, client, call)
Widget w;
XtPointer client, call;
{
  XmFileSelectionBoxCallbackStruct
    *calldata=(XmFileSelectionBoxCallbackStruct*) call; 
  char *filename;
  static XmStringCharSet charset =
                           (XmStringCharSet) XmSTRING_DEFAULT_CHARSET;
  FILE *fil;
  int pat=0, j, k;
  char str[5], c;

  XmStringGetLtoR(calldata->value, charset, &filename);

  fil=fopen(filename, "r");
  if (fil==NULL) return;

  readknobs(fil);

  while (!feof(fil) && (pat<32)) {
    fscanf(fil, "%d", &syn1patterns[pat].numsteps);
    for (j=0; j<syn1patterns[pat].numsteps; j++) {
      fscanf(fil, "%d", &syn1patterns[pat].pitch[j]);
      fscanf(fil, "%d", &syn1patterns[pat].dur[j]);
      fscanf(fil, "%d", &syn1patterns[pat].accent[j]);
    }
    fscanf(fil, "%d", &syn2patterns[pat].numsteps);
    for (j=0; j<syn2patterns[pat].numsteps; j++) {
      fscanf(fil, "%d", &syn2patterns[pat].pitch[j]);
      fscanf(fil, "%d", &syn2patterns[pat].dur[j]);
      fscanf(fil, "%d", &syn2patterns[pat].accent[j]);
    }
    fscanf(fil, "%d", &samppatterns[pat].numsteps);
    for (j=0; j<samppatterns[pat].numsteps; j++) {
      for (k=0; k<13; k++)
        fscanf(fil, "%d ", &samppatterns[pat].step[j][k]);
    }
    fscanf(fil, "%d", &drumpatterns[pat].numsteps);
    for (j=0; j<drumpatterns[pat].numsteps; j++) {
      for (k=0; k<13; k++)
        fscanf(fil, "%d ", &drumpatterns[pat].step[j][k]);
    }
    pat++;
  }
  for (k=0; k<4; k++) {
    if (feof(fil)) break;
    for (j=0; j<64; j++) {
      fscanf(fil, "%d", &pat);
      sprintf(str, "%d", pat);
      if (pat) XbaeMatrixSetCell(songMatrix, k, j, str);
    }
  }

  while (fgetc(fil)!='.') printf("fn\n");;
  for (k=0; k<32; k++) for (j=0; j<8; j++) {
    fread(&Synt1Vec[k][j], sizeof(syntsett), 1, fil);
  }
  for (k=0; k<32; k++) for (j=0; j<8; j++)
    fread(&Synt2Vec[k][j], sizeof(syntsett), 1, fil);
  for (k=0; k<32; k++) for (j=0; j<8; j++)
    fread(&SampVec[k][j], sizeof(sampsett), 1, fil);
  for (k=0; k<32; k++) for (j=0; j<8; j++)
    fread(&DrumVec[k][j], sizeof(drumsett), 1, fil);
  fclose(fil);
  XtFree(filename);
  XtUnmanageChild(loadFileBox);
  redraw_all();
}

void ShowHelp(w, client, call)
Widget w;
XtPointer client, call;
{
  Widget dialog=(Widget) client;
  XtManageChild(dialog);
}

void Play(w, client, call)
Widget w;
XtPointer client, call;
{
  ALport audioport;
  ALconfig config;
  long pvbuf[8];
  AFfilehandle outfile;
  AFfilesetup afsetup;
  short int mixbuff[2048];
  long t=0;
  int bp=0, bn=1024, i, patsamp, step, dpt=0, opt,
    prevstep1, prevstep2, nextstep1, nextstep2, delaysamps;
  int step1, step2=0, stepdrum, stepsamp,
    stepsamp1, stepsamp2, stepsampd, stepsamps;
  double s1, w1, s2, w2, env1=0., env2=0., sust1=0., sust2=0.,
    nextwfa1, nextwfa2;
  double l1, b1, h1, d11=0., d21=0., l2, b2, h2, d12=0., d22=0., fA1=0., fA2=0.;
  double lsam, bsam, hsam, d1sam=0., d2sam=0.;
  double lpcf, bpcf, hpcf, d1pcf=0., d2pcf=0., fpcf, pcfin, bpminv;
  double sy1panL, sy1panR, sy2panL, sy2panR, sapanL, sapanR, drupanL, drupanR,
    delpanL, delpanR, sampanL, sampanR, bpmsamp4inv;
  double ca[21], cb[21], c1, ds, ss, envb1, envb2, fe1, fe2,
    wfa1, wfa2, left, right, pw, fL1, fL2;
  int j, k, dur1, dur2, type, accent1, accent2, pcfstep, songmode;
  int newsock, sel;
  struct drm *drum;
  struct smp *sampl;
  char buf[50];

  if (playing) return;
  if (bouncing) {
    afsetup=AFnewfilesetup();
    AFinitchannels(afsetup, AF_DEFAULT_TRACK, 2);
    AFinitrate(afsetup, AF_DEFAULT_TRACK, 44100);
    AFinitsampfmt(afsetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    outfile = AFopenfile(bouncefilename, "w", afsetup);
    if (outfile==NULL) { openWarn(bouncefilename); return; }
  }

  strcpy(buf,"play");
  for (i=0; i<MAXSENDTO; i++) if (sendto_connected[i])
    write(sendto_sockets[i], buf, strlen(buf)+1);

  playing=1; songwhere=0;
  if (XmToggleButtonGetState(songToggle)) songmode=1; else songmode=0;

  XtVaSetValues(playB,XmNlabelType,XmPIXMAP,XmNlabelPixmap,playpixmap,NULL);

  pvbuf[0] = AL_OUTPUT_RATE;  pvbuf[1] = 44100;
  pvbuf[2] = AL_INPUT_RATE;   pvbuf[3] = 44100;
  pvbuf[4] = AL_CHANNEL_MODE;
  pvbuf[5]=AL_STEREO;
  ALsetparams(AL_DEFAULT_DEVICE, pvbuf, 6);
  config=ALnewconfig();
  ALsetqueuesize(config, (int)(22050));
/*  ALsetchannels(config, 2); */
  audioport=ALopenport("PPE", "w", config);

  for (i=0; i<13; i++) drums[i].playing=0;
  for (i=0; i<13; i++) samps[i].playing=0;
  for (i=0; i<262144; i++) delayline[i]=0.;

  w1=0.; env1=0.;
  w2=0.; env2=0.;


  do {


  /* Felles */

    patsamp=t%(bpmsamp*32);
    stepsamp=patsamp%bpmsamp;
    stepsamp1=patsamp%(int)(bpmsamp*32./syn1patterns[syn1pat].numsteps);
    stepsamp2=patsamp%(int)(bpmsamp*32./syn2patterns[syn2pat].numsteps);
    stepsampd=patsamp%(int)(bpmsamp*32./drumpatterns[drumpat].numsteps);
    stepsamps=patsamp%(int)(bpmsamp*32./samppatterns[samppat].numsteps);

    pcfstep=t%bpmsamp; /* One step per 1/32 (2 periods/bar for 16 beats/bar) */
    if (pcfstep==0) {
      pcfstep=(t%(bpmsamp*pcfpatterns[pcfpattern].numsteps))/bpmsamp;
      if (pcfpatterns[pcfpattern].step[pcfstep]) {
        fpcf=TWOPI*(pcffrq+
          pcfamnt*pcfpatterns[pcfpattern].step[pcfstep]*20.)/44100.;
      }
    }

    if (stepsamp==0) {

      if (patsamp==0) {
        int s1p, s2p, sap, drp;

        delaysamps=delaytime*bpmsamp;
        if (songmode) {
          if (songwhere==64) { playing=0; songwhere=0; }
          if ((XbaeMatrixGetCell(songMatrix, 0, songwhere)==NULL) ||
              (XbaeMatrixGetCell(songMatrix, 1, songwhere)==NULL) ||
              (XbaeMatrixGetCell(songMatrix, 2, songwhere)==NULL) ||
              (XbaeMatrixGetCell(songMatrix, 3, songwhere)==NULL)) {
              playing=0; songwhere=0; }

          s1p=atoi(XbaeMatrixGetCell(songMatrix, 0, songwhere))-1;
          s2p=atoi(XbaeMatrixGetCell(songMatrix, 1, songwhere))-1;
          sap=atoi(XbaeMatrixGetCell(songMatrix, 2, songwhere))-1;
          drp=atoi(XbaeMatrixGetCell(songMatrix, 3, songwhere))-1;

          if ((s1p<0) || (s1p>7) || (s2p<0) || (s2p>7)
            || (sap<0) || (sap>7) || (drp<0) || (drp>7))
            { playing=0; songwhere=0; } else {
	  if (s1p!=syn1pat) {
            XtVaSetValues(syn1patBut[syn1pat], XmNbackground, gray.pixel,NULL);
            XtVaSetValues(syn1patBut[s1p], XmNbackground, red.pixel,NULL);
	    syn1pat=s1p; redraw_syn1(); RestoreWin(NULL,NULL,NULL);
          }
	  if (s2p!=syn2pat) {
            XtVaSetValues(syn2patBut[syn2pat], XmNbackground, gray.pixel,NULL);
            XtVaSetValues(syn2patBut[s2p], XmNbackground, red.pixel,NULL);
	    syn2pat=s2p; redraw_syn2(); RestoreWin(NULL,NULL,NULL);
          }
	  if (sap!=samppat) {
            XtVaSetValues(samppatBut[samppat], XmNbackground, gray.pixel,NULL);
            XtVaSetValues(samppatBut[sap], XmNbackground, red.pixel,NULL);
	    samppat=sap; redraw_samp(); RestoreWin(NULL,NULL,NULL);
          }
	  if (drp!=drumpat) {
            XtVaSetValues(drumpatBut[drumpat], XmNbackground, gray.pixel,NULL);
            XtVaSetValues(drumpatBut[drp], XmNbackground, red.pixel,NULL);
	    drumpat=drp; redraw_drum(); RestoreWin(NULL,NULL,NULL);
          }
 
            }

          if (playing) songwhere++;
            else if (XmToggleButtonGetState(loopToggle)) playing=1;
        }
      }

      bpmsamp4inv=1./(bpmsamp*0.4);
      delpanL=delaypan; delpanR=1.-delpanL;
    }

    step=patsamp/bpmsamp;
    if (stepsamp1==0) {
      step1=patsamp*syn1patterns[syn1pat].numsteps/(32.*bpmsamp);
      StepSet1(step1);
      sy1panL=syn1pan; sy1panR=1.-sy1panL;

      dur1=syn1patterns[syn1pat].dur[step1];
      accent1=syn1patterns[syn1pat].accent[step1];

      prevstep1=(step1-1+syn1patterns[syn1pat].numsteps)%syn1patterns[syn1pat].numsteps;
      nextstep1=(step1+1)%syn1patterns[syn1pat].numsteps;

      wfa1=wf1*pow(2.,syn1patterns[syn1pat].pitch[step1]/12.);
      nextwfa1=wf1*pow(2., syn1patterns[syn1pat].pitch[nextstep1]/12.);

      if (dur1==0) { env1=0.; sust1=0.; }
      else if (syn1patterns[syn1pat].dur[prevstep1]!=2) { env1=1.; sust1=1.; }
    }

    if (stepsamp2==0) {
      step2=patsamp*syn2patterns[syn2pat].numsteps/(32.*bpmsamp);
      StepSet2(step2);
      sy2panL=syn2pan; sy2panR=1.-sy2panL;

      dur2=syn2patterns[syn2pat].dur[step2];
      accent2=syn2patterns[syn2pat].accent[step2];

      prevstep2=(step2-1+syn2patterns[syn2pat].numsteps)%syn2patterns[syn2pat].numsteps;
      nextstep2=(step2+1)%syn2patterns[syn2pat].numsteps;

      wfa2=wf2*pow(2.,syn2patterns[syn2pat].pitch[step2]/12.);
      nextwfa2=wf2*pow(2., syn2patterns[syn2pat].pitch[nextstep2]/12.);

      if (dur2==0) { env2=0.; sust2=0.; }
      else if (syn2patterns[syn2pat].dur[prevstep2]!=2) { env2=1.; sust2=1.; }

    }

    if (stepsampd==0) {
      stepdrum=patsamp*drumpatterns[drumpat].numsteps/(32.*bpmsamp);
      StepSetDrum(stepdrum);
      drupanL=drumpan; drupanR=1.-drupanL;
      for (i=0; i<13; i++) {
        if (drumpatterns[drumpat].step[stepdrum][i]) {
          drums[i].playing=1; drums[i].ptr=0;
        }
      }
    }

    if (stepsamps==0) {
      stepsamp=patsamp*samppatterns[samppat].numsteps/(32.*bpmsamp);
      StepSetSamp(stepsamp);
      sampanL=samppan; sampanR=1.-sampanL;
      for (i=0; i<13; i++) {
        if (samppatterns[samppat].step[stepsamp][i]) {
          samps[i].playing=1; samps[i].ptr=0;
        }
      }
    }


  /* Sampler */

    ss=0.;
    for (i=0; i<13; i++) if (samps[i].playing) {
      sampl=&samps[i];
      ss+=sampl->samp[sampl->ptr];
      if (sampl->ptr<sampl->numsamp-1) sampl->ptr++;
        else sampl->playing=0;
    }
    lsam=d2sam+fsamp*d1sam; hsam=ss-lsam-qsamp*d1sam; bsam=fsamp*hsam+d1sam;
    d1sam=bsam; d2sam=lsam;
    ss=bsam;

  /* Synth 1 */

    j=t&131071;
    pw=pwm1*(j<65536?j:131072-j);
    if (wavetyp1==1) s1=(w1>pw?2.*w1-1.:-1.);
      else if (wavetyp1==2) s1=(w1<pw+0.5?-1.:1.);
        else s1=ss*1e-4;

    if (s1>0.7) s1=0.7; if (s1<-0.7) s1=-0.7; 

    envb1=sust1+(accent1?env1:0.);
    if (dur1) {
      if (stepsamp1<100) if (syn1patterns[syn1pat].dur[prevstep1]!=2)
        envb1*=stepsamp1*0.01;
      if ((dur1==1) || (syn1patterns[syn1pat].dur[nextstep1]==0)) {
        if (stepsamp1>bpmsamp6+100) envb1=0.;
        else if (stepsamp1>bpmsamp6)
          envb1*=1.-(stepsamp1-bpmsamp6)*0.01;
      }
    }
    w1+=wfa1; if (w1>1.) w1-=1.;
    if ((dur1==2) && (stepsamp1>bpmsamp6)) wfa1+=(nextwfa1-wfa1)*bpmsamp4inv;

    fe1=(f1+emod1*env1)*(wavetyp1==3?wfa1*100.:1.);
    l1=d21+fe1*d11; h1=s1-l1-(accent1?q1/(1.+env1*acc1):q1)*d11; b1=fe1*h1+d11;
    d11=b1; d21=l1;
/*
    fL1=exp(-fe1);
    fA1=fA1*fL1+(1.-fL1)*l1;
    s1=fA1*envb1;
*/s1=l1*envb1;
    s1=200000.*atan(0.01*s1*dist1)/(dist1<100.?dist1:100.);

    env1*=decay1;


  /* Synth 2 */

    pw=pwm2*(j<65536?j:131072-j);
    if (wavetyp2==1) s2=(w2>pw?2.*w2-1.:-1.);
      else if (wavetyp2==2) s2=(w2<pw+0.5?-1.:1.);
        else s2=ss*1e-4;

    if (s2>0.7) s2=0.7; if (s2<-0.7) s2=-0.7; 

    envb2=sust2+(accent2?env2:0.);
    if (dur2) {
      if (stepsamp2<100) if (syn2patterns[syn2pat].dur[prevstep2]!=2)
        envb2*=stepsamp2*0.01;
      if ((dur2==1) || (syn2patterns[syn2pat].dur[nextstep2]==0)) {
        if (stepsamp2>bpmsamp6+100) envb2=0.;
        else if (stepsamp2>bpmsamp6)
          envb2*=1.-(stepsamp2-bpmsamp6)*0.01;
      }
    }
    w2+=wfa2; if (w2>1.) w2-=1.;
    if ((dur2==2) && (stepsamp2>bpmsamp6)) wfa2+=(nextwfa2-wfa2)*bpmsamp4inv;

    fe2=(f2+emod2*env2)*(wavetyp2==3?wfa2*100.:1.);
    l2=d22+fe2*d12; h2=s2-l2-(accent2?q2/(1.+env2*acc2):q2)*d12; b2=fe2*h2+d12;
    d12=b2; d22=l2;
/*
    fL2=exp(-fe2);
    fA2=fA2*fL2+(1.-fL2)*l2;
    s2=fA2*envb2;
*/s2=l2*envb2;
    s2=200000.*atan(0.01*s2*dist2)/(dist2<100.?dist2:100.);

    env2*=decay2;


  /* Drum */

    ds=0.;
    for (i=0; i<13; i++) if (drums[i].playing) {
      drum=&drums[i]; type=drum->type;
      ds+=drum->samp[type][drum->ptr]*drum->vol;
      if (drum->ptr<drum->numsamp[type]-1) drum->ptr++;
        else drum->playing=0;
    }


    s1*=vol1; s2*=vol2; ds*=drumvol; ss*=sampvol;

    opt=dpt-delaysamps; opt+=(262144&(-(opt<0)));
    delayline[dpt]=delay1*s1+delay2*s2+drumdelay*ds+delaysmp*ss+
      delayfeed*delayline[opt];
    dpt++; dpt=dpt&262143;

    pcfin=0.;
    if (syn1pcf) { pcfin+=s1; s1=0.; }
    if (syn2pcf) { pcfin+=s2; s2=0.; }
    if (drumpcf) { pcfin+=ds; ds=0.; }
    if (samppcf) { pcfin+=ss; ss=0.; }

    lpcf=d2pcf+fpcf*d1pcf; hpcf=pcfin-lpcf-pcfq*d1pcf;
    bpcf=fpcf*hpcf+d1pcf; d1pcf=bpcf; d2pcf=lpcf;
    fpcf*=pcfdecay;

    left=s1*sy1panL+s2*sy2panL+ds*drupanL+delpanL*delayline[opt]+
      +sampanL*ss+(pcfBp?bpcf:lpcf)*0.6;
    if (left>32767.) left=32767.; if (left<-32767.) left=-32767.;
    right=s1*sy1panR+s2*sy2panR+ds*drupanR+delpanR*delayline[opt]+
      sampanR*ss+(pcfBp?bpcf:lpcf)*0.6;
    if (right>32767.) right=32767.; if (right<-32767.) right=-32767.;
    mixbuff[bp]=(short int)(left);
    mixbuff[bp+1]=(short int)(right);

    t++; bn--; bp+=2;
    if (bn==0) {
      bn=1024; bp=0;
      ALwritesamps(audioport, mixbuff, 2048);
      if (bouncing) AFwriteframes(outfile, AF_DEFAULT_TRACK, mixbuff, 1024);
      while (XtAppPending(app_context)) {
        XtAppProcessEvent(app_context,XtIMXEvent|XtIMTimer|XtIMAlternateInput);
      }
      FD_ZERO(&client_sockets_set);
      for (i=0; i<MAXCLIENTS; i++) { newsock=client_sockets[i];
        if (newsock>=0) FD_SET(newsock, &client_sockets_set);
      }
      timeout.tv_sec=0; timeout.tv_usec=0;
      sel=select(FD_SETSIZE, &client_sockets_set, NULL, NULL, &timeout);
      if (sel>0) for (i=0; i<MAXCLIENTS; i++) {
        newsock=client_sockets[i];
        if (newsock>=0) {
          if (FD_ISSET(newsock, &client_sockets_set)) readcontrol(i);
        }
      }
    }
  } while (playing);
  ALcloseport(audioport);
  if (bouncing) AFclosefile(outfile);
  playing=0; bouncing=0;
  XtVaSetValues(playB,XmNlabelPixmap,noplaypixmap,NULL);
  XtVaSetValues(stopB,XmNlabelPixmap,nostoppixmap,NULL);
}

void Stop(w, client, call)
Widget w;
XtPointer client, call;
{
  char buf[20];
  int i;

  playing=0;
  strcpy(buf,"stop");
  for (i=0; i<MAXSENDTO; i++) if (sendto_connected[i])
    write(sendto_sockets[i], buf, strlen(buf)+1);
}

void Bounce(w, client, call)
Widget w;
XtPointer client, call;
{
  XtManageChild(bounceFileBox);
}

void BounceOk(w, client, call)
Widget w;
XtPointer client, call;
{
  XmFileSelectionBoxCallbackStruct
    *calldata=(XmFileSelectionBoxCallbackStruct*) call;
  char *filename;
  static XmStringCharSet charset =
                           (XmStringCharSet) XmSTRING_DEFAULT_CHARSET;

  XmStringGetLtoR(calldata->value, charset, &filename);
  strcpy(bouncefilename, filename);
  XtFree(filename);
  XtUnmanageChild(bounceFileBox);
  bouncing=1;
  Play(NULL,NULL,NULL);
}

void SaveSett(w, client, call)
Widget w;
XtPointer client, call;
{
  switch (autwhere) {
    case 1: memcpy(&Synt1Vec[autstep][syn1pat], &Syntsett1, sizeof(syntsett));
      Synt1Vec[autstep][syn1pat].set=1; break;
    case 2: memcpy(&Synt2Vec[autstep][syn2pat], &Syntsett2, sizeof(syntsett));
      Synt2Vec[autstep][syn2pat].set=1; break;
    case 3: memcpy(&SampVec[autstep][samppat], &Sampsett, sizeof(sampsett));
      SampVec[autstep][samppat].set=1; break;
    case 4: memcpy(&DrumVec[autstep][drumpat], &Drumsett, sizeof(drumsett));
      DrumVec[autstep][samppat].set=1; break;
  }
  autstep++;
  switch (autwhere) {
    case 1: if (autstep==syn1patterns[syn1pat].numsteps) autstep=0; break;
    case 2: if (autstep==syn2patterns[syn2pat].numsteps) autstep=0; break;
    case 3: if (autstep==samppatterns[samppat].numsteps) autstep=0; break;
    case 4: if (autstep==drumpatterns[drumpat].numsteps) autstep=0; break;
  }
  XtVaSetValues(stepSlider, XmNvalue, autstep+1, NULL);
  switch (autwhere) {
    case 1: redraw_syn1(); StepSet1(autstep); break;
    case 2: redraw_syn2(); StepSet2(autstep); break;
    case 3: redraw_samp(); StepSetSamp(autstep); break;
    case 4: redraw_drum(); StepSetDrum(autstep); break;
  }
  RestoreWin(NULL, NULL, NULL);
}

void ClearSett(w, client, call)
Widget w;
XtPointer client, call;
{
  switch (autwhere) {
    case 1: Synt1Vec[autstep][syn1pat].set=0; break;
    case 2: Synt2Vec[autstep][syn2pat].set=0; break;
    case 3: SampVec[autstep][samppat].set=0; break;
    case 4: DrumVec[autstep][drumpat].set=0; break;
  }
  autstep++; if (autstep==16) autstep=0;
  XtVaSetValues(stepSlider, XmNvalue, autstep+1, NULL);
  switch (autwhere) {
    case 1: redraw_syn1(); StepSet1(autstep); break;
    case 2: redraw_syn2(); StepSet2(autstep); break;
    case 3: redraw_samp(); StepSetSamp(autstep); break;
    case 4: redraw_drum(); StepSetDrum(autstep); break;
  }
  RestoreWin(NULL, NULL, NULL);
}

void Syn1Sett(w, client, call)
Widget w;
XtPointer client, call;
{
  autwhere=1;
  redraw_all();
}

void Syn2Sett(w, client, call)
Widget w;
XtPointer client, call;
{
  autwhere=2;
  redraw_all();
}

void SampSett(w, client, call)
Widget w;
XtPointer client, call;
{
  autwhere=3;
  redraw_all();
}

void DrumSett(w, client, call)
Widget w;
XtPointer client, call;
{
  autwhere=4;
  redraw_all();
}

void Drumwin(w, client, call)
Widget w;
XtPointer client, call;
{
  XtManageChild(drumBB);
}

void Songwin(w, client, call)
Widget w;
XtPointer client, call;
{
  XtManageChild(songBB);
}


static void Keydown(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
  KeySym keysym;
  char key_string[1];
  int key_string_len;

  key_string_len=XLookupString((XKeyEvent *) event, key_string,
    sizeof key_string, &keysym, (XComposeStatus *) NULL);
  if ((keysym==XK_BackSpace) || (keysym==XK_Delete)) {
  }
}

void syn1click(int x, int y)
{
  int i, step, pitch, slew, stepwidth;

  stepwidth=32*16./syn1patterns[syn1pat].numsteps;
  if (x<50) {
  } else if ((y<150) && (y>8)) {
    step=(x-50)/stepwidth;
    pitch=(149-y)/4;
    slew=(x-50)%stepwidth>stepwidth/2;
    if (slew) {
      if (syn1patterns[syn1pat].dur[step]==1) syn1patterns[syn1pat].dur[step]=2;
        else if (syn1patterns[syn1pat].dur[step]==2) syn1patterns[syn1pat].dur[step]=1;
    } else {
      if ((syn1patterns[syn1pat].dur[step]) && (syn1patterns[syn1pat].pitch[step]==pitch)) {
        syn1patterns[syn1pat].dur[step]=0;
      } else {
        syn1patterns[syn1pat].dur[step]=1;
        syn1patterns[syn1pat].pitch[step]=pitch;
      }
    }
  }
  redraw_syn1(); RestoreWin(NULL,NULL,NULL);
}

void syn2click(int x, int y)
{
  int i, step, pitch, slew, stepwidth;

  stepwidth=32*16./syn2patterns[syn2pat].numsteps;
  if (x<50) {
  } else if ((y<300) && (y>158)) {
    step=(x-50)/stepwidth;
    pitch=(299-y)/4;
    slew=(x-50)%stepwidth>stepwidth/2;
    if (slew) {
      if (syn2patterns[syn2pat].dur[step]==1) syn2patterns[syn2pat].dur[step]=2;
        else if (syn2patterns[syn2pat].dur[step]==2) syn2patterns[syn2pat].dur[step]=1;
    } else {
      if ((syn2patterns[syn2pat].dur[step]) && (syn2patterns[syn2pat].pitch[step]==pitch)) {
        syn2patterns[syn2pat].dur[step]=0;
      } else {
        syn2patterns[syn2pat].dur[step]=1;
        syn2patterns[syn2pat].pitch[step]=pitch;
      }
    }
  }
  redraw_syn2(); RestoreWin(NULL,NULL,NULL);
}

void sampclick(int x, int y)
{
  int i, step, sampl, stepwidth;

  stepwidth=32*16./samppatterns[samppat].numsteps;
  if (x<50) {
  } else if ((y<440) && (y>312)) {
    step=(x-50)/stepwidth;
    sampl=(440-y)/10;
    if (samppatterns[samppat].step[step][sampl])
      samppatterns[samppat].step[step][sampl]=0;
    else samppatterns[samppat].step[step][sampl]=1;
  }
  redraw_samp(); RestoreWin(NULL,NULL,NULL);
}

void drumclick(int x, int y)
{
  int i, step, drum, stepwidth;

  stepwidth=32*16./drumpatterns[drumpat].numsteps;
  if (x<50) {
  } else if ((y<590) && (y>462)) {
    step=(x-50)/stepwidth;
    drum=(590-y)/10;
    if (drumpatterns[drumpat].step[step][drum])
      drumpatterns[drumpat].step[step][drum]=0;
    else drumpatterns[drumpat].step[step][drum]=1;
  }
  redraw_drum(); RestoreWin(NULL,NULL,NULL);
}

void syn1midclick(int x, int y)
{
  int i, step, pitch, slew, stepwidth;

  stepwidth=32*16./syn1patterns[syn1pat].numsteps;
  if (x<50) {
  } else if ((y<150) && (y>8)) {
    step=(x-50)/stepwidth;
    if (syn1patterns[syn1pat].accent[step])
      syn1patterns[syn1pat].accent[step]=0;
    else syn1patterns[syn1pat].accent[step]=1;
  }
  redraw_syn1(); RestoreWin(NULL,NULL,NULL);
}

void syn2midclick(int x, int y)
{
  int i, step, pitch, slew, stepwidth;

  stepwidth=32*16./syn2patterns[syn2pat].numsteps;
  if (x<50) {
  } else if ((y<300) && (y>158)) {
    step=(x-50)/stepwidth;
    if (syn2patterns[syn2pat].accent[step])
      syn2patterns[syn2pat].accent[step]=0;
    else syn2patterns[syn2pat].accent[step]=1;
  }
  redraw_syn2(); RestoreWin(NULL,NULL,NULL);
}

static void LeftClick(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
  int x, y, track, i, pt=-1;
  double tid;

  XButtonEvent *motion=(XButtonEvent *)event;
  x=motion->x; y=motion->y;
  if ((x<0) || (x>580) || (y<0) || (y>600)) return;

  if (y>450) drumclick(x, y);
  else if (y>300) sampclick(x, y);
  else if (y>150) syn2click(x, y);
  else syn1click(x, y);
}

static void MidClick(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
  int x, y, track, i, pt=-1;
  double tid;

  XButtonEvent *motion=(XButtonEvent *)event;
  x=motion->x; y=motion->y;
  if ((x<0) || (x>580) || (y<0) || (y>600)) return;

  if (y>450) ;
  else if (y>300) ;
  else if (y>150) syn2midclick(x, y);
  else syn1midclick(x, y);
}


void readdrum(char *name, int drum, int type)
{
  AFfilehandle file;
  AFfilesetup setup;
  int j;

  setup=NULL;
  file=AFopenfile(name, "r", setup);
  if (file==NULL) {
    fprintf(stderr, "Couldn't open drum file %s\n", name);
    exit(1);
  } else {
    drums[drum].numsamp[type]=(int)AFgetframecnt(file,  AF_DEFAULT_TRACK);
    drums[drum].samp[type]=(short int *)calloc(drums[drum].numsamp[type],
      sizeof(short int));
    AFreadframes(file, AF_DEFAULT_TRACK, drums[drum].samp[type],
      drums[drum].numsamp[type]);
    AFclosefile(file);
  }
}

void init_drums(void)
{
  fprintf(stderr, "Reading drum samples ... ");

  readdrum("TR909/bt0a0d0.aiff", 0, 0);
  readdrum("TR909/bt0a0d3.aiff", 0, 1);
  readdrum("TR909/bt0a0d7.aiff", 0, 2);
  readdrum("TR909/bt0a0da.aiff", 0, 3);
  readdrum("TR909/bt0aad0.aiff", 0, 4);
  readdrum("TR909/bt0aada.aiff", 0, 5);
  readdrum("TR909/bt3a0d0.aiff", 0, 6);
  readdrum("TR909/bt3a0d3.aiff", 0, 7);
  readdrum("TR909/bt3a0d7.aiff", 0, 8);
  readdrum("TR909/bt3a0da.aiff", 0, 9);
  readdrum("TR909/bt3aad0.aiff", 0, 10);
  readdrum("TR909/bt3aada.aiff", 0, 11);
  readdrum("TR909/bt7a0d0.aiff", 0, 12);
  readdrum("TR909/bt7a0d3.aiff", 0, 13);
  readdrum("TR909/bt7a0d7.aiff", 0, 14);
  readdrum("TR909/bt7a0da.aiff", 0, 15);
  readdrum("TR909/bt7aad0.aiff", 0, 16);
  readdrum("TR909/bt7aada.aiff", 0, 17);
  readdrum("TR909/btaa0d0.aiff", 0, 18);
  readdrum("TR909/btaa0d3.aiff", 0, 19);
  readdrum("TR909/btaa0d7.aiff", 0, 20);
  readdrum("TR909/btaa0da.aiff", 0, 21);
  readdrum("TR909/btaaad0.aiff", 0, 22);
  readdrum("TR909/btaaada.aiff", 0, 23);
  readdrum("TR808/bd0000.aiff", 0, 24);
  readdrum("TR808/bd0025.aiff", 0, 25);
  readdrum("TR808/bd0050.aiff", 0, 26);
  readdrum("TR808/bd0075.aiff", 0, 27);
  readdrum("TR808/bd0010.aiff", 0, 28);
  readdrum("TR808/bd2500.aiff", 0, 29);
  readdrum("TR808/bd2525.aiff", 0, 30);
  readdrum("TR808/bd2550.aiff", 0, 31);
  readdrum("TR808/bd2575.aiff", 0, 32);
  readdrum("TR808/bd2510.aiff", 0, 33);
  readdrum("TR808/bd5000.aiff", 0, 34);
  readdrum("TR808/bd5025.aiff", 0, 35);
  readdrum("TR808/bd5050.aiff", 0, 36);
  readdrum("TR808/bd5075.aiff", 0, 37);
  readdrum("TR808/bd5010.aiff", 0, 38);
  readdrum("TR808/bd7500.aiff", 0, 39);
  readdrum("TR808/bd7525.aiff", 0, 40);
  readdrum("TR808/bd7550.aiff", 0, 41);
  readdrum("TR808/bd7575.aiff", 0, 42);
  readdrum("TR808/bd7510.aiff", 0, 43);
  readdrum("TR808/bd1000.aiff", 0, 44);
  readdrum("TR808/bd1025.aiff", 0, 45);
  readdrum("TR808/bd1050.aiff", 0, 46);
  readdrum("TR808/bd1075.aiff", 0, 47);
  readdrum("TR808/bd1010.aiff", 0, 48);

  readdrum("TR909/st0t0s0.aiff", 1, 0);
  readdrum("TR909/st0t0s3.aiff", 1, 1);
  readdrum("TR909/st0t0s7.aiff", 1, 2);
  readdrum("TR909/st0t0sa.aiff", 1, 3);
  readdrum("TR909/st0t3s3.aiff", 1, 4);
  readdrum("TR909/st0t3s7.aiff", 1, 5);
  readdrum("TR909/st0t3sa.aiff", 1, 6);
  readdrum("TR909/st0t7s3.aiff", 1, 7);
  readdrum("TR909/st0t7s7.aiff", 1, 8);
  readdrum("TR909/st0t7sa.aiff", 1, 9);
  readdrum("TR909/st0tas3.aiff", 1, 10);
  readdrum("TR909/st0tas7.aiff", 1, 11);
  readdrum("TR909/st0tasa.aiff", 1, 12);
  readdrum("TR909/st3t0s0.aiff", 1, 13);
  readdrum("TR909/st3t0s3.aiff", 1, 14);
  readdrum("TR909/st3t0s7.aiff", 1, 15);
  readdrum("TR909/st3t0sa.aiff", 1, 16);
  readdrum("TR909/st3t3s3.aiff", 1, 17);
  readdrum("TR909/st3t3s7.aiff", 1, 18);
  readdrum("TR909/st3t3sa.aiff", 1, 19);
  readdrum("TR909/st3t7s3.aiff", 1, 20);
  readdrum("TR909/st3t7s7.aiff", 1, 21);
  readdrum("TR909/st3t7sa.aiff", 1, 22);
  readdrum("TR909/st3tas3.aiff", 1, 23);
  readdrum("TR909/st3tas7.aiff", 1, 24);
  readdrum("TR909/st3tasa.aiff", 1, 25);
  readdrum("TR909/st7t0s0.aiff", 1, 26);
  readdrum("TR909/st7t0s3.aiff", 1, 27);
  readdrum("TR909/st7t0s7.aiff", 1, 28);
  readdrum("TR909/st7t0sa.aiff", 1, 29);
  readdrum("TR909/st7t3s3.aiff", 1, 30);
  readdrum("TR909/st7t3s7.aiff", 1, 31);
  readdrum("TR909/st7t3sa.aiff", 1, 32);
  readdrum("TR909/st7t7s3.aiff", 1, 33);
  readdrum("TR909/st7t7s7.aiff", 1, 34);
  readdrum("TR909/st7t7sa.aiff", 1, 35);
  readdrum("TR909/st7tas3.aiff", 1, 36);
  readdrum("TR909/st7tas7.aiff", 1, 37);
  readdrum("TR909/st7tasa.aiff", 1, 38);
  readdrum("TR909/stat0s0.aiff", 1, 39);
  readdrum("TR909/stat0s3.aiff", 1, 40);
  readdrum("TR909/stat0s7.aiff", 1, 41);
  readdrum("TR909/stat0sa.aiff", 1, 42);
  readdrum("TR909/stat3s3.aiff", 1, 43);
  readdrum("TR909/stat3s7.aiff", 1, 44);
  readdrum("TR909/stat3sa.aiff", 1, 45);
  readdrum("TR909/stat7s3.aiff", 1, 46);
  readdrum("TR909/stat7s7.aiff", 1, 47);
  readdrum("TR909/stat7sa.aiff", 1, 48);
  readdrum("TR909/statas3.aiff", 1, 49);
  readdrum("TR909/statas7.aiff", 1, 50);
  readdrum("TR909/statasa.aiff", 1, 51);
  readdrum("TR808/sd0000.aiff", 1, 52);
  readdrum("TR808/sd0025.aiff", 1, 53);
  readdrum("TR808/sd0050.aiff", 1, 54);
  readdrum("TR808/sd0075.aiff", 1, 55);
  readdrum("TR808/sd0010.aiff", 1, 56);
  readdrum("TR808/sd2500.aiff", 1, 57);
  readdrum("TR808/sd2525.aiff", 1, 58);
  readdrum("TR808/sd2550.aiff", 1, 59);
  readdrum("TR808/sd2575.aiff", 1, 60);
  readdrum("TR808/sd2510.aiff", 1, 61);
  readdrum("TR808/sd5000.aiff", 1, 62);
  readdrum("TR808/sd5025.aiff", 1, 63);
  readdrum("TR808/sd5050.aiff", 1, 64);
  readdrum("TR808/sd5075.aiff", 1, 65);
  readdrum("TR808/sd5010.aiff", 1, 66);
  readdrum("TR808/sd7500.aiff", 1, 67);
  readdrum("TR808/sd7525.aiff", 1, 68);
  readdrum("TR808/sd7550.aiff", 1, 69);
  readdrum("TR808/sd7575.aiff", 1, 70);
  readdrum("TR808/sd7510.aiff", 1, 71);
  readdrum("TR808/sd1000.aiff", 1, 72);
  readdrum("TR808/sd1025.aiff", 1, 73);
  readdrum("TR808/sd1050.aiff", 1, 74);
  readdrum("TR808/sd1075.aiff", 1, 75);
  readdrum("TR808/sd1010.aiff", 1, 76);

  readdrum("TR909/lt0d0.aiff", 2, 0);
  readdrum("TR909/lt0d3.aiff", 2, 1);
  readdrum("TR909/lt0d7.aiff", 2, 2);
  readdrum("TR909/lt0da.aiff", 2, 3);
  readdrum("TR909/lt3d0.aiff", 2, 4);
  readdrum("TR909/lt3d3.aiff", 2, 5);
  readdrum("TR909/lt3d7.aiff", 2, 6);
  readdrum("TR909/lt3da.aiff", 2, 7);
  readdrum("TR909/lt7d0.aiff", 2, 8);
  readdrum("TR909/lt7d3.aiff", 2, 9);
  readdrum("TR909/lt7d7.aiff", 2, 10);
  readdrum("TR909/lt7da.aiff", 2, 11);
  readdrum("TR909/ltad0.aiff", 2, 12);
  readdrum("TR909/ltad3.aiff", 2, 13);
  readdrum("TR909/ltad7.aiff", 2, 14);
  readdrum("TR909/ltada.aiff", 2, 15);

  readdrum("TR909/mt0d0.aiff", 3, 0);
  readdrum("TR909/mt0d3.aiff", 3, 1);
  readdrum("TR909/mt0d7.aiff", 3, 2);
  readdrum("TR909/mt0da.aiff", 3, 3);
  readdrum("TR909/mt3d0.aiff", 3, 4);
  readdrum("TR909/mt3d3.aiff", 3, 5);
  readdrum("TR909/mt3d7.aiff", 3, 6);
  readdrum("TR909/mt3da.aiff", 3, 7);
  readdrum("TR909/mt7d0.aiff", 3, 8);
  readdrum("TR909/mt7d3.aiff", 3, 9);
  readdrum("TR909/mt7d7.aiff", 3, 10);
  readdrum("TR909/mt7da.aiff", 3, 11);
  readdrum("TR909/mtad0.aiff", 3, 12);
  readdrum("TR909/mtad3.aiff", 3, 13);
  readdrum("TR909/mtad7.aiff", 3, 14);
  readdrum("TR909/mtada.aiff", 3, 15);

  readdrum("TR909/ht0d0.aiff", 4, 0);
  readdrum("TR909/ht0d3.aiff", 4, 1);
  readdrum("TR909/ht0d7.aiff", 4, 2);
  readdrum("TR909/ht0da.aiff", 4, 3);
  readdrum("TR909/ht3d0.aiff", 4, 4);
  readdrum("TR909/ht3d3.aiff", 4, 5);
  readdrum("TR909/ht3d7.aiff", 4, 6);
  readdrum("TR909/ht3da.aiff", 4, 7);
  readdrum("TR909/ht7d0.aiff", 4, 8);
  readdrum("TR909/ht7d3.aiff", 4, 9);
  readdrum("TR909/ht7d7.aiff", 4, 10);
  readdrum("TR909/ht7da.aiff", 4, 11);
  readdrum("TR909/htad0.aiff", 4, 12);
  readdrum("TR909/htad3.aiff", 4, 13);
  readdrum("TR909/htad7.aiff", 4, 14);
  readdrum("TR909/htada.aiff", 4, 15);

  readdrum("TR909/rim63.aiff", 5, 0);
  readdrum("TR909/rim127.aiff", 5, 1);

  readdrum("TR909/handclp1.aiff", 6, 0);
  readdrum("TR909/handclp2.aiff", 6, 1);

  readdrum("TR909/hhcd0.aiff", 7, 0);
  readdrum("TR909/hhcd2.aiff", 7, 1);
  readdrum("TR909/hhcd4.aiff", 7, 2);
  readdrum("TR909/hhcd6.aiff", 7, 3);
  readdrum("TR909/hhcd8.aiff", 7, 4);
  readdrum("TR909/hhcda.aiff", 7, 5);

  readdrum("TR909/hhod0.aiff", 8, 0);
  readdrum("TR909/hhod2.aiff", 8, 1);
  readdrum("TR909/hhod4.aiff", 8, 2);
  readdrum("TR909/hhod6.aiff", 8, 3);
  readdrum("TR909/hhod8.aiff", 8, 4);
  readdrum("TR909/hhoda.aiff", 8, 5);

  readdrum("TR909/cshd0.aiff", 9, 0);
  readdrum("TR909/cshd2.aiff", 9, 1);
  readdrum("TR909/cshd4.aiff", 9, 2);
  readdrum("TR909/cshd6.aiff", 9, 3);
  readdrum("TR909/cshd8.aiff", 9, 4);
  readdrum("TR909/cshda.aiff", 9, 5);

  readdrum("TR909/rided0.aiff", 10, 0);
  readdrum("TR909/rided2.aiff", 10, 1);
  readdrum("TR909/rided4.aiff", 10, 2);
  readdrum("TR909/rided6.aiff", 10, 3);
  readdrum("TR909/rided8.aiff", 10, 4);
  readdrum("TR909/rideda.aiff", 10, 5);

  readdrum("TR909/clop1.aiff", 11, 0);
  readdrum("TR909/clop2.aiff", 11, 1);
  readdrum("TR909/clop3.aiff", 11, 2);
  readdrum("TR909/clop4.aiff", 11, 3);

  readdrum("TR909/opcl1.aiff", 12, 0);
  readdrum("TR909/opcl2.aiff", 12, 1);
  readdrum("TR909/opcl3.aiff", 12, 2);
  readdrum("TR909/opcl4.aiff", 12, 3);

  fprintf(stderr, "OK\n");
}

void readsamp(char *name, int sampl)
{
  AFfilehandle file;
  AFfilesetup setup;
  int j;

  setup=NULL;
  file=AFopenfile(name, "r", setup);
  if (file==NULL) {
    fprintf(stderr, "Couldn't open sample file %s\n", name);
    exit(1);
  } else {
    samps[sampl].numsamp=(int)AFgetframecnt(file,  AF_DEFAULT_TRACK);
    samps[sampl].samp=(short int *)calloc(samps[sampl].numsamp,
      sizeof(short int));
    AFreadframes(file, AF_DEFAULT_TRACK, samps[sampl].samp,
      samps[sampl].numsamp);
    AFclosefile(file);
  }
}

void init_samps(void)
{
  int i;
  char s[100];

  fprintf(stderr, "Reading samples ... ");
  for (i=0; i<13; i++) {
    sprintf(s, "SAMPS/samp%d.aiff", i);
    readsamp(s, i);
  }

  fprintf(stderr, "OK\n");
}

void init_values(void)
{
  int i, j;

  for (i=0; i<13; i++) {
    drums[i].vol=0.4; drums[i].type=0;
  }
  for (i=0; i<32; i++) for (j=0; j<8; j++) {
    Synt1Vec[i][j].set=0; Synt2Vec[i][j].set=0;
    SampVec[i][j].set=0; DrumVec[i][j].set=0; GlobalVec[i][j].set=0;
  }

  playing=0; bpm=120; bpmsamp=2756;
  bpmsamp6=bpmsamp*0.6;
  wavetyp1=2; wavetyp2=1;
  wf1=55./44100.; wf2=27.5/44100.;
  q1=0.5; f1=(3000.*(exp(50./50.)-1.)/(exp(2.)-1.))*TWOPI/44100.;
  q2=0.5; f2=(3000.*(exp(50./50.)-1.)/(exp(2.)-1.))*TWOPI/44100.;
  pwm1=20./15000000.; pwm2=20./15000000.;
  decay1=1.-(20./200000.); decay2=1.-(20./200000.);
  emod1=TWOPI*100./44100.; emod2=TWOPI*100./44100.;
  acc1=5.; acc2=5.;
  vol1=0.8; vol2=0.8; dist1=1.; dist2=1.; delay1=0.; delay2=0.; delaysmp=0.;
  drumvol=0.8; drumdelay=0.; distsamp=1.;
  syn1pat=0; syn2pat=0; samppat=0; drumpat=0;
  syn1pan=0.5; syn2pan=0.5; drumpan=0.5; samppan=0.5; delaypan=0.5;
  delayfeed=0.3; delaytime=3; pcffrq=300.; pcfq=0.25; pcfamnt=0.8;
  pcfdecay=1.-20./500000.;
  syn1pcf=0; syn2pcf=0; samppcf=0; drumpcf=0; pcfpattern=0;
  fsamp=3000.*TWOPI/44100.;
  qsamp=1.4; sampvol=0.4;

  Syntsett1.tune=12; Syntsett1.pwm=20; Syntsett1.decay=20;
  Syntsett1.cutoff=50; Syntsett1.reson=10; Syntsett1.envmod=100;
  Syntsett1.vol=80; Syntsett1.pan=50; Syntsett1.distor=0;
  Syntsett1.delay=0; Syntsett1.accent=50; Syntsett1.pcf=0;
  Syntsett1.wave=2;

  Syntsett2.tune=12; Syntsett2.pwm=20; Syntsett2.decay=20;
  Syntsett2.cutoff=50; Syntsett2.reson=10; Syntsett2.envmod=100;
  Syntsett2.vol=80; Syntsett2.pan=50; Syntsett2.distor=0;
  Syntsett2.delay=0; Syntsett2.accent=50; Syntsett2.pcf=0;
  Syntsett2.wave=2;

  Sampsett.vol=80; Sampsett.pan=50; Sampsett.distor=0; Sampsett.delay=0;
  Sampsett.cutoff=50; Sampsett.reson=7; Sampsett.pcf=0;

  Drumsett.vol=80; Drumsett.pan=50; Drumsett.delay=0; Drumsett.pcf=0;
  Drumsett.bassvol=80; Drumsett.basstune=0; Drumsett.bassattck=0;
  Drumsett.bassdecay=0; Drumsett.bass808=0;
  Drumsett.snarevol=80; Drumsett.snaretune=0; Drumsett.snaretone=0;
  Drumsett.snaresnap=0; Drumsett.snare808=0;
  Drumsett.ltomvol=80; Drumsett.ltomtune=0; Drumsett.ltomdecay=0;
  Drumsett.mtomvol=80; Drumsett.mtomtune=0; Drumsett.mtomdecay=0;
  Drumsett.htomvol=80; Drumsett.htomtune=0; Drumsett.htomdecay=0;
  Drumsett.rimshvol=80; Drumsett.rimshveloc=0; Drumsett.hclapvol=80;
  Drumsett.hclapveloc=0; Drumsett.clhhvol=80; Drumsett.clhhdecay=0;
  Drumsett.ophhvol=80; Drumsett.ophhdecay=0; Drumsett.crashvol=80;
  Drumsett.crashtune=0; Drumsett.ridevol=80; Drumsett.ridetune=0;
  Drumsett.cohhvol=80; Drumsett.cohhcombi=0;
  Drumsett.ochhvol=80; Drumsett.ochhcombi=0;

  Globalsett.bpm=120; Globalsett.freq=300; Globalsett.reson=40;
  Globalsett.amnt=80; Globalsett.decay=20; Globalsett.pat=0;
  Globalsett.bp=0; Globalsett.steps=3; Globalsett.pan=50; Globalsett.fback=30;
}

void init_patterns(void)
{
  int i, j, k, ant;
  FILE *fil;

  for (i=0; i<32; i++) {
    drumpatterns[i].numsteps=16;
    for (j=0; j<32; j++) {
      for (k=0; k<13; k++) drumpatterns[i].step[j][k]=0;
      if (j%2==0) drumpatterns[i].step[j][0]=1;
    }
    samppatterns[i].numsteps=16;
    for (j=0; j<32; j++) {
      for (k=0; k<13; k++) samppatterns[i].step[j][k]=0;
      if (j%8==0) samppatterns[i].step[j][0]=1;
    }
    syn1patterns[i].numsteps=16;
    for (j=0; j<32; j++) {
      syn1patterns[i].pitch[j]=19; syn1patterns[i].dur[j]=0;
      if (j%8==0) syn1patterns[i].dur[j]=1;
      syn1patterns[i].accent[j]=0;
    }
    syn2patterns[i].numsteps=16;
    for (j=0; j<32; j++) {
      syn2patterns[i].pitch[j]=12; syn2patterns[i].dur[j]=0;
      if (j%4==0) syn2patterns[i].dur[j]=1;
      syn2patterns[i].accent[j]=0;
    }
  }
  fil=fopen("pcfpatterns", "r");
  if (fil==NULL) { fprintf(stderr, "Couldn't open 'pcfpatterns' file\n");
    exit(1); }
  while (!feof(fil)) {
    fscanf(fil, "%d", &i); fscanf(fil, "%d", &ant);
    if ((ant>32) || (i>32)) { fprintf(stderr, "Error in 'pcfpatterns' file\n");
      exit(1); }
    pcfpatterns[i].numsteps=ant;
    for (j=0; j<ant; j++) {
      fscanf(fil, "%d", &(pcfpatterns[i].step[j]));
    }
  }
  fclose(fil);
}

void connectWarn(host)
char *host;
{
  XmString text;
  char streng[100];

  sprintf(streng,"Accepted connection from %s",host);
  text=XmStringCreateSimple(streng);
  XtVaSetValues(connectWarning,XmNmessageString,text,NULL);
  XtManageChild(connectWarning);
  XtFree(text);
}

void slavefailWarn(host)
char *host;
{
  XmString text;
  char streng[100];

  sprintf(streng,"Couldn't contact Ppe on %s",host);
  text=XmStringCreateSimple(streng);
  XtVaSetValues(slavefailWarning,XmNmessageString,text,NULL);
  XtManageChild(slavefailWarning);
  XtFree(text);
}

void AcceptConnect(w, client, call)
Widget w;
XtPointer client, call;
{
  int newsock, i, sel;
  int me_sockaddr_len=sizeof(me_sockaddr);
  struct hostent *callinginfo;

  if (XmToggleButtonGetState(w)) accepting=1; else accepting=0;
  while (accepting) {
    if (XtAppPending(app_context))
      XtAppProcessEvent(app_context,XtIMXEvent|XtIMTimer|XtIMAlternateInput);
    if ((newsock=accept(server_socket, (struct sockaddr *)&me_sockaddr,
     &me_sockaddr_len))>=0) {
      callinginfo=gethostbyaddr(&(me_sockaddr.sin_addr.s_addr),
        sizeof(struct in_addr), AF_INET);
      for (i=0; i<MAXCLIENTS; i++) if (client_sockets[i]==-1) break;
      if (i==MAXCLIENTS) printf("Error: Max number of controlling clients exceeded\n");
      else {
        connectWarn(callinginfo->h_name);
        client_sockets[i]=newsock;
      }
    }
    FD_ZERO(&client_sockets_set);
    for (i=0; i<MAXCLIENTS; i++) { newsock=client_sockets[i];
      if (newsock>=0) FD_SET(newsock, &client_sockets_set);
    }
    timeout.tv_sec=0; timeout.tv_usec=0;
    sel=select(FD_SETSIZE, &client_sockets_set, NULL, NULL, &timeout);
    if (sel>0) for (i=0; i<MAXCLIENTS; i++) {
      newsock=client_sockets[i];
      if (newsock>=0) {
        if (FD_ISSET(newsock, &client_sockets_set)) readcontrol(i);
      }
    }
  }
}

void SendtoConnect(w, client, call)
Widget w;
XtPointer client, call;
{
  int i=*(int *)client;
  struct hostent *nameinfo;
  char *txt, buf[10];

  if (XmToggleButtonGetState(sendtoToggle[i])) {
    txt=XmTextGetString(sendtoText[i]);
    if (strlen(txt)==0) {
      slavefailWarn("**Not given **");
      XmToggleButtonSetState(sendtoToggle[i],False,False);
      return;
    }
    nameinfo=gethostbyname(txt); XtFree(txt);
    bcopy((char *)nameinfo->h_addr,
      (char *)&sendto_addresses[i].sin_addr, nameinfo->h_length);
    sendto_addresses[i].sin_family=AF_INET;
    sendto_addresses[i].sin_port=htons(PORTNUM);
    sendto_sockets[i]=socket(AF_INET, SOCK_STREAM, 0);
    if (connect(sendto_sockets[i], (struct sockaddr *)&sendto_addresses[i],
      sizeof(sendto_addresses[i]))==-1) {
        slavefailWarn(nameinfo->h_name);
        close(sendto_sockets[i]);
        XmToggleButtonSetState(sendtoToggle[i],False,False);
      } else sendto_connected[i]=1;
  } else {
    strcpy(buf,"close");
    write(sendto_sockets[i], buf, strlen(buf)+1);
    close(sendto_sockets[i]);
    sendto_connected[i]=0;
  }
}

void readcontrol(sockpt)
int sockpt;
{
  char message[200], c='*';
  int p=0;
  int sock=client_sockets[sockpt];

  while (c) {
    if (read(sock, &c, 1)!=1) return;
    message[p++]=c;
    if (p==200) { printf("Socket message too long\n"); break; }
  }
  if (strcasecmp("play",message)==0) Play(NULL,NULL,NULL);
  if (strcasecmp("stop",message)==0) playing=0;
  if (strcasecmp("close",message)==0) { close(sock); client_sockets[sockpt]=-1; }
}

void SendtoB(w, client, call)
Widget w;
XtPointer client, call;
{
  XtManageChild(sendtoBB);
}

main(argc, argv)
int argc;
char **argv;
{
  XmString text;
  Pixmap pixelmap, bitmap2;
  int defdepth;
  static XtActionsRec actions[]={
    {"Keydown",     Keydown},
    {"LeftClick",   LeftClick},
    {"MidClick",    MidClick},
  };
  char streng[5];
  short widths[64];
  String columnlabels[64]={
    "1","2","3","4","5","6","7","8","1","2","3","4","5","6","7","8",
    "1","2","3","4","5","6","7","8","1","2","3","4","5","6","7","8",
    "1","2","3","4","5","6","7","8","1","2","3","4","5","6","7","8",
    "1","2","3","4","5","6","7","8","1","2","3","4","5","6","7","8"
  };
  String rowlabels[4]={
    "Syn1", "Syn2", "Samp", "Drum"
  };


  int i, j;

  printf("Starting PPE ."); fflush(stdout);

  for (i=0; i<64; i++) widths[i]=1;
  topLevel=XtVaAppInitialize(
    &app_context,"Ppe",NULL,0,&argc,argv,NULL,NULL);
  XtVaGetApplicationResources(topLevel, &app_data, resources,
    XtNumber(resources), NULL);
  dw = XmGetXmDisplay(XtDisplay(topLevel));
  XtVaSetValues(dw, XmNdragInitiatorProtocolStyle, XmDRAG_NONE, NULL);

  mainWindow=XtVaCreateManagedWidget(
    "mainWindow",xmMainWindowWidgetClass,topLevel, NULL);

  menuBar=XmCreateMenuBar(mainWindow,"menuBar",NULL,0);
  XtManageChild(menuBar);

  frame=XtVaCreateManagedWidget(
    "frame",xmFrameWidgetClass,mainWindow, NULL);

  XmMainWindowSetAreas(mainWindow, menuBar, NULL, NULL, NULL, frame);

  largeBB = XtVaCreateManagedWidget(
    "largeBB", xmBulletinBoardWidgetClass, frame, NULL);

  sketchpad = XtVaCreateManagedWidget(
    "sketchpad",xmDrawingAreaWidgetClass, largeBB, NULL);

  XtAppAddActions(app_context, actions, XtNumber(actions));
  XtAddCallback(sketchpad,XmNexposeCallback,RestoreWin,0);

  theDisplay = XtDisplay(sketchpad);
  theWindow = XtWindow(sketchpad);
  screen = DefaultScreen(theDisplay);
  theRoot = RootWindowOfScreen(XtScreen(sketchpad));
  theCmap = DefaultColormap( theDisplay, DefaultScreen(theDisplay) );
  bitmap = XCreatePixmap(XtDisplay(sketchpad),theRoot, 580, 600,
    DefaultDepthOfScreen(XtScreen(sketchpad)));
  bitGC = XCreateGC( theDisplay, bitmap, 0, 0 );
  showGC = XCreateGC( theDisplay, theRoot, 0, 0 );
  markerGC = XCreateGC( theDisplay, theRoot, 0, 0 );

  XParseColor(theDisplay,theCmap,"red",&red);
  XAllocColor( theDisplay, theCmap,&red);
  XParseColor(theDisplay,theCmap,"blue",&blue);
  XAllocColor( theDisplay, theCmap,&blue);
  XParseColor(theDisplay,theCmap,"yellow",&yellow);
  XAllocColor( theDisplay, theCmap,&yellow);
  XParseColor(theDisplay,theCmap,"black",&black);
  XAllocColor( theDisplay, theCmap,&black);
  XParseColor(theDisplay,theCmap,"green",&green);
  XAllocColor( theDisplay, theCmap,&green);
  XParseColor(theDisplay,theCmap,"white",&white);
  XAllocColor( theDisplay, theCmap,&white);
  XParseColor(theDisplay,theCmap,"brown",&brown);
  XAllocColor( theDisplay, theCmap,&brown);
  XParseColor(theDisplay,theCmap,"gray75",&gray);
  XAllocColor( theDisplay, theCmap,&gray);
  XParseColor(theDisplay,theCmap,"skyblue",&skyblue);
  XAllocColor( theDisplay, theCmap,&skyblue);
  XParseColor(theDisplay,theCmap,"violet",&purple);
  XAllocColor( theDisplay, theCmap,&purple);
  XParseColor(theDisplay,theCmap,"gray10",&markcolor);
  XAllocColor( theDisplay, theCmap,&markcolor);


  trackBB=XtVaCreateManagedWidget("trackBB",xmBulletinBoardWidgetClass,
    largeBB, NULL);
  tracksep1=XtVaCreateManagedWidget("tracksep1",xmSeparatorWidgetClass,
    trackBB, NULL);
  tracksep2=XtVaCreateManagedWidget("tracksep2",xmSeparatorWidgetClass,
    trackBB, NULL);
  tracksep3=XtVaCreateManagedWidget("tracksep3",xmSeparatorWidgetClass,
    trackBB, NULL);

  for (i=0; i<8; i++) {
    sprintf(streng,"%d",i+1);
    text=XmStringCreateLocalized(streng);
    syn1patBut[i]=XtVaCreateManagedWidget("syn1patBut",
      xmPushButtonWidgetClass,trackBB, XmNlabelString,text, XmNwidth, 26,
      XmNheight, 26, XmNx, (i%4)*28, XmNy, (i/4)*28+1,
      XmNbackground, gray.pixel, NULL);
    XtAddCallback(syn1patBut[i],XmNactivateCallback,Syn1patBut,&dummy[i]);
    syn2patBut[i]=XtVaCreateManagedWidget("syn2patBut",
      xmPushButtonWidgetClass,trackBB, XmNlabelString,text, XmNwidth, 26,
      XmNheight, 26, XmNx, (i%4)*28, XmNy, (i/4)*28+152,
      XmNbackground, gray.pixel, NULL);
    XtAddCallback(syn2patBut[i],XmNactivateCallback,Syn2patBut,&dummy[i]);
    samppatBut[i]=XtVaCreateManagedWidget("samppatBut",
      xmPushButtonWidgetClass,trackBB, XmNlabelString,text, XmNwidth, 26,
      XmNheight, 26, XmNx, (i%4)*28, XmNy, (i/4)*28+302,
      XmNbackground, gray.pixel, NULL);
    XtAddCallback(samppatBut[i],XmNactivateCallback,SamppatBut,&dummy[i]);
    drumpatBut[i]=XtVaCreateManagedWidget("drumpatBut",
      xmPushButtonWidgetClass,trackBB, XmNlabelString,text, XmNwidth, 26,
      XmNheight, 26, XmNx, (i%4)*28, XmNy, (i/4)*28+452,
      XmNbackground, gray.pixel, NULL);
    XtAddCallback(drumpatBut[i],XmNactivateCallback,DrumpatBut,&dummy[i]);
    XtFree(text);
  }
  XtVaSetValues(syn1patBut[0], XmNbackground, red.pixel, NULL);
  XtVaSetValues(syn2patBut[0], XmNbackground, red.pixel, NULL);
  XtVaSetValues(samppatBut[0], XmNbackground, red.pixel, NULL);
  XtVaSetValues(drumpatBut[0], XmNbackground, red.pixel, NULL);

  syn1beatsRadio=XtVaCreateManagedWidget("syn1beatsRadio",
    xmRowColumnWidgetClass,trackBB,NULL);
  syn1beats8=XtVaCreateManagedWidget("syn1beats8",
    xmToggleButtonWidgetClass,syn1beatsRadio,NULL);
  XtAddCallback(syn1beats8,XmNvalueChangedCallback,Syn1beats,NULL);
  syn1beats16=XtVaCreateManagedWidget("syn1beats16",
    xmToggleButtonWidgetClass,syn1beatsRadio,NULL);
  XtAddCallback(syn1beats16,XmNvalueChangedCallback,Syn1beats,NULL);
  syn1beats32=XtVaCreateManagedWidget("syn1beats32",
    xmToggleButtonWidgetClass,syn1beatsRadio,NULL);
  XtAddCallback(syn1beats32,XmNvalueChangedCallback,Syn1beats,NULL);

  syn2beatsRadio=XtVaCreateManagedWidget("syn2beatsRadio",
    xmRowColumnWidgetClass,trackBB,NULL);
  syn2beats8=XtVaCreateManagedWidget("syn2beats8",
    xmToggleButtonWidgetClass,syn2beatsRadio,NULL);
  XtAddCallback(syn2beats8,XmNvalueChangedCallback,Syn2beats,NULL);
  syn2beats16=XtVaCreateManagedWidget("syn2beats16",
    xmToggleButtonWidgetClass,syn2beatsRadio,NULL);
  XtAddCallback(syn2beats16,XmNvalueChangedCallback,Syn2beats,NULL);
  syn2beats32=XtVaCreateManagedWidget("syn2beats32",
    xmToggleButtonWidgetClass,syn2beatsRadio,NULL);
  XtAddCallback(syn2beats32,XmNvalueChangedCallback,Syn2beats,NULL);

  drumbeatsRadio=XtVaCreateManagedWidget("drumbeatsRadio",
    xmRowColumnWidgetClass,trackBB,NULL);
  drumbeats8=XtVaCreateManagedWidget("drumbeats8",
    xmToggleButtonWidgetClass,drumbeatsRadio,NULL);
  XtAddCallback(drumbeats8,XmNvalueChangedCallback,Drumbeats,NULL);
  drumbeats16=XtVaCreateManagedWidget("drumbeats16",
    xmToggleButtonWidgetClass,drumbeatsRadio,NULL);
  XtAddCallback(drumbeats16,XmNvalueChangedCallback,Drumbeats,NULL);
  drumbeats32=XtVaCreateManagedWidget("drumbeats32",
    xmToggleButtonWidgetClass,drumbeatsRadio,NULL);
  XtAddCallback(drumbeats32,XmNvalueChangedCallback,Drumbeats,NULL);

  topBB=XtVaCreateManagedWidget("topBB",xmBulletinBoardWidgetClass,
    largeBB, NULL);
  topsep1=XtVaCreateManagedWidget("topsep1",xmSeparatorWidgetClass,
    topBB, NULL);
  topsep2=XtVaCreateManagedWidget("topsep2",xmSeparatorWidgetClass,
    topBB, NULL);
  topsep3=XtVaCreateManagedWidget("topsep3",xmSeparatorWidgetClass,
    topBB, NULL);
  bpmDial=XtVaCreateManagedWidget("bpmDial",sgDialWidgetClass, topBB, NULL);
  bpmLabel=XtVaCreateManagedWidget("bpmLabel",xmLabelWidgetClass, topBB, NULL);
  XtAddCallback(bpmDial,XmNvalueChangedCallback,BpmDial,0);
  XtAddCallback(bpmDial,XmNdragCallback,BpmDial,0);
  playB=XtVaCreateManagedWidget("playB",xmPushButtonWidgetClass,topBB,NULL);
  XtAddCallback(playB,XmNactivateCallback,Play,NULL);
  stopB=XtVaCreateManagedWidget("stopB",xmPushButtonWidgetClass,topBB,NULL);
  XtAddCallback(stopB,XmNactivateCallback,Stop,NULL);

  stepSlider=XtVaCreateManagedWidget("stepSlider",xmScaleWidgetClass,
    topBB,NULL);
  XtAddCallback(stepSlider,XmNvalueChangedCallback,StepSlider,0);
  XtAddCallback(stepSlider,XmNdragCallback,StepSlider,0);
  saveSett=XtVaCreateManagedWidget("saveSett",xmPushButtonWidgetClass,topBB,NULL);
  XtAddCallback(saveSett,XmNactivateCallback,SaveSett,NULL);
  clearSett=XtVaCreateManagedWidget("clearSett",xmPushButtonWidgetClass,topBB,NULL);
  XtAddCallback(clearSett,XmNactivateCallback,ClearSett,NULL);
  labelSett=XtVaCreateManagedWidget("labelSett",xmLabelWidgetClass, topBB, NULL);
  syn1Sett=XtVaCreateManagedWidget("syn1Sett",xmPushButtonWidgetClass,topBB,NULL);
  XtAddCallback(syn1Sett,XmNactivateCallback,Syn1Sett,NULL);
  syn2Sett=XtVaCreateManagedWidget("syn2Sett",xmPushButtonWidgetClass,topBB,NULL);
  XtAddCallback(syn2Sett,XmNactivateCallback,Syn2Sett,NULL);
  sampSett=XtVaCreateManagedWidget("sampSett",xmPushButtonWidgetClass,topBB,NULL);
  XtAddCallback(sampSett,XmNactivateCallback,SampSett,NULL);
  drumSett=XtVaCreateManagedWidget("drumSett",xmPushButtonWidgetClass,topBB,NULL);
  XtAddCallback(drumSett,XmNactivateCallback,DrumSett,NULL);
  sep1Sett=XtVaCreateManagedWidget("sep1Sett",xmSeparatorWidgetClass,
    topBB, NULL);
  sep2Sett=XtVaCreateManagedWidget("sep2Sett",xmSeparatorWidgetClass,
    topBB, NULL);

  delayTimeDial=XtVaCreateManagedWidget("delayTimeDial",sgDialWidgetClass,
    topBB, NULL);
  delayTimeLabel=XtVaCreateManagedWidget("delayTimeLabel",xmLabelWidgetClass,
    topBB, NULL);
  XtAddCallback(delayTimeDial,XmNvalueChangedCallback,DelayTimeDial,0);
  XtAddCallback(delayTimeDial,XmNdragCallback,DelayTimeDial,0);
  delayPanDial=XtVaCreateManagedWidget("delayPanDial",sgDialWidgetClass,
    topBB, NULL);
  delayPanLabel=XtVaCreateManagedWidget("delayPanLabel",xmLabelWidgetClass,
    topBB, NULL);
  XtAddCallback(delayPanDial,XmNvalueChangedCallback,DelayPanDial,0);
  XtAddCallback(delayPanDial,XmNdragCallback,DelayPanDial,0);
  delayFeedDial=XtVaCreateManagedWidget("delayFeedDial",sgDialWidgetClass,
    topBB, NULL);
  delayFeedLabel=XtVaCreateManagedWidget("delayFeedLabel",xmLabelWidgetClass,
    topBB, NULL);
  XtAddCallback(delayFeedDial,XmNvalueChangedCallback,DelayFeedDial,0);
  XtAddCallback(delayFeedDial,XmNdragCallback,DelayFeedDial,0);
  delayLabel=XtVaCreateManagedWidget("delayLabel",xmLabelWidgetClass,
    topBB, NULL);

  pcfFrqDial=XtVaCreateManagedWidget("pcfFrqDial",sgDialWidgetClass,
    topBB, NULL);
  pcfFrqLabel=XtVaCreateManagedWidget("pcfFrqLabel",xmLabelWidgetClass,
    topBB, NULL);
  XtAddCallback(pcfFrqDial,XmNvalueChangedCallback,PcfFrqDial,0);
  XtAddCallback(pcfFrqDial,XmNdragCallback,PcfFrqDial,0);
  pcfQDial=XtVaCreateManagedWidget("pcfQDial",sgDialWidgetClass,
    topBB, NULL);
  pcfQLabel=XtVaCreateManagedWidget("pcfQLabel",xmLabelWidgetClass,
    topBB, NULL);
  XtAddCallback(pcfQDial,XmNvalueChangedCallback,PcfQDial,0);
  XtAddCallback(pcfQDial,XmNdragCallback,PcfQDial,0);
  pcfAmntDial=XtVaCreateManagedWidget("pcfAmntDial",sgDialWidgetClass,
    topBB, NULL);
  pcfAmntLabel=XtVaCreateManagedWidget("pcfAmntLabel",xmLabelWidgetClass,
    topBB, NULL);
  XtAddCallback(pcfAmntDial,XmNvalueChangedCallback,PcfAmntDial,0);
  XtAddCallback(pcfAmntDial,XmNdragCallback,PcfAmntDial,0);
  pcfDecayDial=XtVaCreateManagedWidget("pcfDecayDial",sgDialWidgetClass,
    topBB, NULL);
  pcfDecayLabel=XtVaCreateManagedWidget("pcfDecayLabel",xmLabelWidgetClass,
    topBB, NULL);
  XtAddCallback(pcfDecayDial,XmNvalueChangedCallback,PcfDecayDial,0);
  XtAddCallback(pcfDecayDial,XmNdragCallback,PcfDecayDial,0);
  pcfPatternDial=XtVaCreateManagedWidget("pcfPatternDial",sgDialWidgetClass,
    topBB, NULL);
  pcfPatternLabel=XtVaCreateManagedWidget("pcfPatternLabel",xmLabelWidgetClass,
    topBB, NULL);
  XtAddCallback(pcfPatternDial,XmNvalueChangedCallback,PcfPatternDial,0);
  XtAddCallback(pcfPatternDial,XmNdragCallback,PcfPatternDial,0);
  pcfBpToggle=XtVaCreateManagedWidget("pcfBpToggle",
    xmToggleButtonWidgetClass,topBB,NULL);
  XtAddCallback(pcfBpToggle,XmNvalueChangedCallback,PcfBpToggle,NULL);

  pcfLabel=XtVaCreateManagedWidget("pcfLabel",xmLabelWidgetClass,
    topBB, NULL);

  syn1BB=XtVaCreateManagedWidget("syn1BB",xmBulletinBoardWidgetClass,
    largeBB, NULL);
  syn1sep1=XtVaCreateManagedWidget("syn1sep1",xmSeparatorWidgetClass,
    syn1BB, NULL);
  syn1waveRadio=XtVaCreateManagedWidget("syn1waveRadio",
    xmRowColumnWidgetClass,syn1BB,NULL);
  syn1waveSaw=XtVaCreateManagedWidget("saw1",
    xmToggleButtonWidgetClass,syn1waveRadio,NULL);
  XtAddCallback(syn1waveSaw,XmNvalueChangedCallback,Syn1wave,NULL);
  syn1waveSqr=XtVaCreateManagedWidget("sqr1",
    xmToggleButtonWidgetClass,syn1waveRadio,NULL);
  XtAddCallback(syn1waveSqr,XmNvalueChangedCallback,Syn1wave,NULL);
  syn1waveSmp=XtVaCreateManagedWidget("smp1",
    xmToggleButtonWidgetClass,syn1waveRadio,NULL);
  XtAddCallback(syn1waveSmp,XmNvalueChangedCallback,Syn1wave,NULL);
  syn1tuneDial=XtVaCreateManagedWidget("syn1tuneDial",sgDialWidgetClass,
    syn1BB, NULL);
  syn1tuneLabel=XtVaCreateManagedWidget("syn1tuneLabel",xmLabelWidgetClass,
    syn1BB, NULL);
  XtAddCallback(syn1tuneDial,XmNvalueChangedCallback,Syn1tuneDial,0);
  XtAddCallback(syn1tuneDial,XmNdragCallback,Syn1tuneDial,0);
  syn1pwmDial=XtVaCreateManagedWidget("syn1pwmDial",sgDialWidgetClass,
    syn1BB, NULL);
  syn1pwmLabel=XtVaCreateManagedWidget("syn1pwmLabel",xmLabelWidgetClass,
    syn1BB, NULL);
  XtAddCallback(syn1pwmDial,XmNvalueChangedCallback,Syn1pwmDial,0);
  XtAddCallback(syn1pwmDial,XmNdragCallback,Syn1pwmDial,0);
  syn1decayDial=XtVaCreateManagedWidget("syn1decayDial",sgDialWidgetClass,
    syn1BB, NULL);
  syn1decayLabel=XtVaCreateManagedWidget("syn1decayLabel",xmLabelWidgetClass,
    syn1BB, NULL);
  XtAddCallback(syn1decayDial,XmNvalueChangedCallback,Syn1decayDial,0);
  XtAddCallback(syn1decayDial,XmNdragCallback,Syn1decayDial,0);
  syn1lpCutDial=XtVaCreateManagedWidget("syn1lpCutDial",sgDialWidgetClass,
    syn1BB, NULL);
  syn1lpCutLabel=XtVaCreateManagedWidget("syn1lpCutLabel",xmLabelWidgetClass,
    syn1BB, NULL);
  XtAddCallback(syn1lpCutDial,XmNvalueChangedCallback,Syn1lpCutDial,0);
  XtAddCallback(syn1lpCutDial,XmNdragCallback,Syn1lpCutDial,0);
  syn1lpResDial=XtVaCreateManagedWidget("syn1lpResDial",sgDialWidgetClass,
    syn1BB, NULL);
  syn1lpResLabel=XtVaCreateManagedWidget("syn1lpResLabel",xmLabelWidgetClass,
    syn1BB, NULL);
  XtAddCallback(syn1lpResDial,XmNvalueChangedCallback,Syn1lpResDial,0);
  XtAddCallback(syn1lpResDial,XmNdragCallback,Syn1lpResDial,0);
  syn1lpEnvDial=XtVaCreateManagedWidget("syn1lpEnvDial",sgDialWidgetClass,
    syn1BB, NULL);
  syn1lpEnvLabel=XtVaCreateManagedWidget("syn1lpEnvLabel",xmLabelWidgetClass,
    syn1BB, NULL);
  XtAddCallback(syn1lpEnvDial,XmNvalueChangedCallback,Syn1lpEnvDial,0);
  XtAddCallback(syn1lpEnvDial,XmNdragCallback,Syn1lpEnvDial,0);
  syn1accentDial=XtVaCreateManagedWidget("syn1accentDial",sgDialWidgetClass,
    syn1BB, NULL);
  syn1accentLabel=XtVaCreateManagedWidget("syn1accentLabel",xmLabelWidgetClass,
    syn1BB, NULL);
  XtAddCallback(syn1accentDial,XmNvalueChangedCallback,Syn1accentDial,0);
  XtAddCallback(syn1accentDial,XmNdragCallback,Syn1accentDial,0);
  syn1volDial=XtVaCreateManagedWidget("syn1volDial",sgDialWidgetClass,
    syn1BB, NULL);
  syn1volLabel=XtVaCreateManagedWidget("syn1volLabel",xmLabelWidgetClass,
    syn1BB, NULL);
  XtAddCallback(syn1volDial,XmNvalueChangedCallback,Syn1volDial,0);
  XtAddCallback(syn1volDial,XmNdragCallback,Syn1volDial,0);
  syn1panDial=XtVaCreateManagedWidget("syn1panDial",sgDialWidgetClass,
    syn1BB, NULL);
  syn1panLabel=XtVaCreateManagedWidget("syn1panLabel",xmLabelWidgetClass,
    syn1BB, NULL);
  XtAddCallback(syn1panDial,XmNvalueChangedCallback,Syn1panDial,0);
  XtAddCallback(syn1panDial,XmNdragCallback,Syn1panDial,0);
  syn1distDial=XtVaCreateManagedWidget("syn1distDial",sgDialWidgetClass,
    syn1BB, NULL);
  syn1distLabel=XtVaCreateManagedWidget("syn1distLabel",xmLabelWidgetClass,
    syn1BB, NULL);
  XtAddCallback(syn1distDial,XmNvalueChangedCallback,Syn1distDial,0);
  XtAddCallback(syn1distDial,XmNdragCallback,Syn1distDial,0);
  syn1delayDial=XtVaCreateManagedWidget("syn1delayDial",sgDialWidgetClass,
    syn1BB, NULL);
  syn1delayLabel=XtVaCreateManagedWidget("syn1delayLabel",xmLabelWidgetClass,
    syn1BB, NULL);
  XtAddCallback(syn1delayDial,XmNvalueChangedCallback,Syn1delayDial,0);
  XtAddCallback(syn1delayDial,XmNdragCallback,Syn1delayDial,0);
  syn1pcfToggle=XtVaCreateManagedWidget("syn1pcfToggle",
    xmToggleButtonWidgetClass,syn1BB,NULL);
  XtAddCallback(syn1pcfToggle,XmNvalueChangedCallback,Syn1pcfToggle,NULL);

  syn2BB=XtVaCreateManagedWidget("syn2BB",xmBulletinBoardWidgetClass,
    largeBB, NULL);
  syn2sep1=XtVaCreateManagedWidget("syn2sep1",xmSeparatorWidgetClass,
    syn2BB, NULL);
  syn2waveRadio=XtVaCreateManagedWidget("syn2waveRadio",
    xmRowColumnWidgetClass,syn2BB,NULL);
  syn2waveSaw=XtVaCreateManagedWidget("saw2",
    xmToggleButtonWidgetClass,syn2waveRadio,NULL);
  XtAddCallback(syn2waveSaw,XmNvalueChangedCallback,Syn2wave,NULL);
  syn2waveSqr=XtVaCreateManagedWidget("sqr2",
    xmToggleButtonWidgetClass,syn2waveRadio,NULL);
  XtAddCallback(syn2waveSqr,XmNvalueChangedCallback,Syn2wave,NULL);
  syn2waveSmp=XtVaCreateManagedWidget("smp2",
    xmToggleButtonWidgetClass,syn2waveRadio,NULL);
  XtAddCallback(syn2waveSmp,XmNvalueChangedCallback,Syn2wave,NULL);
  syn2tuneDial=XtVaCreateManagedWidget("syn2tuneDial",sgDialWidgetClass,
    syn2BB, NULL);
  syn2tuneLabel=XtVaCreateManagedWidget("syn2tuneLabel",xmLabelWidgetClass,
    syn2BB, NULL);
  XtAddCallback(syn2tuneDial,XmNvalueChangedCallback,Syn2tuneDial,0);
  XtAddCallback(syn2tuneDial,XmNdragCallback,Syn2tuneDial,0);
  syn2pwmDial=XtVaCreateManagedWidget("syn2pwmDial",sgDialWidgetClass,
    syn2BB, NULL);
  syn2pwmLabel=XtVaCreateManagedWidget("syn2pwmLabel",xmLabelWidgetClass,
    syn2BB, NULL);
  XtAddCallback(syn2pwmDial,XmNvalueChangedCallback,Syn2pwmDial,0);
  XtAddCallback(syn2pwmDial,XmNdragCallback,Syn2pwmDial,0);
  syn2decayDial=XtVaCreateManagedWidget("syn2decayDial",sgDialWidgetClass,
    syn2BB, NULL);
  syn2decayLabel=XtVaCreateManagedWidget("syn2decayLabel",xmLabelWidgetClass,
    syn2BB, NULL);
  XtAddCallback(syn2decayDial,XmNvalueChangedCallback,Syn2decayDial,0);
  XtAddCallback(syn2decayDial,XmNdragCallback,Syn2decayDial,0);
  syn2lpCutDial=XtVaCreateManagedWidget("syn2lpCutDial",sgDialWidgetClass,
    syn2BB, NULL);
  syn2lpCutLabel=XtVaCreateManagedWidget("syn2lpCutLabel",xmLabelWidgetClass,
    syn2BB, NULL);
  XtAddCallback(syn2lpCutDial,XmNvalueChangedCallback,Syn2lpCutDial,0);
  XtAddCallback(syn2lpCutDial,XmNdragCallback,Syn2lpCutDial,0);
  syn2lpResDial=XtVaCreateManagedWidget("syn2lpResDial",sgDialWidgetClass,
    syn2BB, NULL);
  syn2lpResLabel=XtVaCreateManagedWidget("syn2lpResLabel",xmLabelWidgetClass,
    syn2BB, NULL);
  XtAddCallback(syn2lpResDial,XmNvalueChangedCallback,Syn2lpResDial,0);
  XtAddCallback(syn2lpResDial,XmNdragCallback,Syn2lpResDial,0);
  syn2lpEnvDial=XtVaCreateManagedWidget("syn2lpEnvDial",sgDialWidgetClass,
    syn2BB, NULL);
  syn2lpEnvLabel=XtVaCreateManagedWidget("syn2lpEnvLabel",xmLabelWidgetClass,
    syn2BB, NULL);
  XtAddCallback(syn2lpEnvDial,XmNvalueChangedCallback,Syn2lpEnvDial,0);
  XtAddCallback(syn2lpEnvDial,XmNdragCallback,Syn2lpEnvDial,0);
  syn2accentDial=XtVaCreateManagedWidget("syn2accentDial",sgDialWidgetClass,
    syn2BB, NULL);
  syn2accentLabel=XtVaCreateManagedWidget("syn2accentLabel",xmLabelWidgetClass,
    syn2BB, NULL);
  XtAddCallback(syn2accentDial,XmNvalueChangedCallback,Syn2accentDial,0);
  XtAddCallback(syn2accentDial,XmNdragCallback,Syn2accentDial,0);
  syn2volDial=XtVaCreateManagedWidget("syn2volDial",sgDialWidgetClass,
    syn2BB, NULL);
  syn2volLabel=XtVaCreateManagedWidget("syn2volLabel",xmLabelWidgetClass,
    syn2BB, NULL);
  XtAddCallback(syn2volDial,XmNvalueChangedCallback,Syn2volDial,0);
  XtAddCallback(syn2volDial,XmNdragCallback,Syn2volDial,0);
  syn2panDial=XtVaCreateManagedWidget("syn2panDial",sgDialWidgetClass,
    syn2BB, NULL);
  syn2panLabel=XtVaCreateManagedWidget("syn2panLabel",xmLabelWidgetClass,
    syn2BB, NULL);
  XtAddCallback(syn2panDial,XmNvalueChangedCallback,Syn2panDial,0);
  XtAddCallback(syn2panDial,XmNdragCallback,Syn2panDial,0);
  syn2distDial=XtVaCreateManagedWidget("syn2distDial",sgDialWidgetClass,
    syn2BB, NULL);
  syn2distLabel=XtVaCreateManagedWidget("syn2distLabel",xmLabelWidgetClass,
    syn2BB, NULL);
  XtAddCallback(syn2distDial,XmNvalueChangedCallback,Syn2distDial,0);
  XtAddCallback(syn2distDial,XmNdragCallback,Syn2distDial,0);
  syn2delayDial=XtVaCreateManagedWidget("syn2delayDial",sgDialWidgetClass,
    syn2BB, NULL);
  syn2delayLabel=XtVaCreateManagedWidget("syn2delayLabel",xmLabelWidgetClass,
    syn2BB, NULL);
  XtAddCallback(syn2delayDial,XmNvalueChangedCallback,Syn2delayDial,0);
  XtAddCallback(syn2delayDial,XmNdragCallback,Syn2delayDial,0);
  syn2pcfToggle=XtVaCreateManagedWidget("syn2pcfToggle",
    xmToggleButtonWidgetClass,syn2BB,NULL);
  XtAddCallback(syn2pcfToggle,XmNvalueChangedCallback,Syn2pcfToggle,NULL);

  printf("."); fflush(stdout);

  sampBB=XtVaCreateManagedWidget("sampBB",xmBulletinBoardWidgetClass,
    largeBB, NULL);
  sampsep1=XtVaCreateManagedWidget("sampsep1",xmSeparatorWidgetClass,
    sampBB, NULL);
  samplpCutDial=XtVaCreateManagedWidget("samplpCutDial",sgDialWidgetClass,
    sampBB, NULL);
  samplpCutLabel=XtVaCreateManagedWidget("samplpCutLabel",xmLabelWidgetClass,
    sampBB, NULL);
  XtAddCallback(samplpCutDial,XmNvalueChangedCallback,SamplpCutDial,0);
  XtAddCallback(samplpCutDial,XmNdragCallback,SamplpCutDial,0);
  samplpResDial=XtVaCreateManagedWidget("samplpResDial",sgDialWidgetClass,
    sampBB, NULL);
  samplpResLabel=XtVaCreateManagedWidget("samplpResLabel",xmLabelWidgetClass,
    sampBB, NULL);
  XtAddCallback(samplpResDial,XmNvalueChangedCallback,SamplpResDial,0);
  XtAddCallback(samplpResDial,XmNdragCallback,SamplpResDial,0);
  sampvolDial=XtVaCreateManagedWidget("sampvolDial",sgDialWidgetClass,
    sampBB, NULL);
  sampvolLabel=XtVaCreateManagedWidget("sampvolLabel",xmLabelWidgetClass,
    sampBB, NULL);
  XtAddCallback(sampvolDial,XmNvalueChangedCallback,SampvolDial,0);
  XtAddCallback(sampvolDial,XmNdragCallback,SampvolDial,0);
  samppanDial=XtVaCreateManagedWidget("samppanDial",sgDialWidgetClass,
    sampBB, NULL);
  samppanLabel=XtVaCreateManagedWidget("samppanLabel",xmLabelWidgetClass,
    sampBB, NULL);
  XtAddCallback(samppanDial,XmNvalueChangedCallback,SamppanDial,0);
  XtAddCallback(samppanDial,XmNdragCallback,SamppanDial,0);
  sampdistDial=XtVaCreateManagedWidget("sampdistDial",sgDialWidgetClass,
    sampBB, NULL);
  sampdistLabel=XtVaCreateManagedWidget("sampdistLabel",xmLabelWidgetClass,
    sampBB, NULL);
  XtAddCallback(sampdistDial,XmNvalueChangedCallback,SampdistDial,0);
  XtAddCallback(sampdistDial,XmNdragCallback,SampdistDial,0);
  sampdelayDial=XtVaCreateManagedWidget("sampdelayDial",sgDialWidgetClass,
    sampBB, NULL);
  sampdelayLabel=XtVaCreateManagedWidget("sampdelayLabel",xmLabelWidgetClass,
    sampBB, NULL);
  XtAddCallback(sampdelayDial,XmNvalueChangedCallback,SampdelayDial,0);
  XtAddCallback(sampdelayDial,XmNdragCallback,SampdelayDial,0);
  samppcfToggle=XtVaCreateManagedWidget("samppcfToggle",
    xmToggleButtonWidgetClass,sampBB,NULL);
  XtAddCallback(samppcfToggle,XmNvalueChangedCallback,SamppcfToggle,NULL);

  drumBB=XmCreateBulletinBoardDialog(topLevel,"drumBB",NULL,0);
  drumBB2=XtVaCreateManagedWidget("drumBB2",xmBulletinBoardWidgetClass,
    largeBB, NULL);
  drumBlabel=XtVaCreateManagedWidget("drumBlabel",xmLabelWidgetClass,
    drumBB, NULL);
  drumBvolDial=XtVaCreateManagedWidget("drumBvolDial",sgDialWidgetClass,
    drumBB, NULL);
  drumBvolLabel=XtVaCreateManagedWidget("drumBvolLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumBvolDial,XmNvalueChangedCallback,DrumBvolDial,0);
  XtAddCallback(drumBvolDial,XmNdragCallback,DrumBvolDial,0);
  drumBtuneDial=XtVaCreateManagedWidget("drumBtuneDial",sgDialWidgetClass,
    drumBB, NULL);
  drumBtuneLabel=XtVaCreateManagedWidget("drumBtuneLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumBtuneDial,XmNvalueChangedCallback,DrumBtypeDial,0);
  XtAddCallback(drumBtuneDial,XmNdragCallback,DrumBtypeDial,0);
  drumBattackDial=XtVaCreateManagedWidget("drumBattackDial",sgDialWidgetClass,
    drumBB, NULL);
  drumBattackLabel=XtVaCreateManagedWidget("drumBattackLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumBattackDial,XmNvalueChangedCallback,DrumBtypeDial,0);
  XtAddCallback(drumBattackDial,XmNdragCallback,DrumBtypeDial,0);
  drumBdecayDial=XtVaCreateManagedWidget("drumBdecayDial",sgDialWidgetClass,
    drumBB, NULL);
  drumBdecayLabel=XtVaCreateManagedWidget("drumBdecayLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumBdecayDial,XmNvalueChangedCallback,DrumBtypeDial,0);
  XtAddCallback(drumBdecayDial,XmNdragCallback,DrumBtypeDial,0);
  drumBtr808=XtVaCreateManagedWidget("drumBtr808",xmToggleButtonWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumBtr808,XmNvalueChangedCallback,DrumBtypeDial,0);
  drumSlabel=XtVaCreateManagedWidget("drumSlabel",xmLabelWidgetClass,
    drumBB, NULL);
  drumSvolDial=XtVaCreateManagedWidget("drumSvolDial",sgDialWidgetClass,
    drumBB, NULL);
  drumSvolLabel=XtVaCreateManagedWidget("drumSvolLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumSvolDial,XmNvalueChangedCallback,DrumSvolDial,0);
  XtAddCallback(drumSvolDial,XmNdragCallback,DrumSvolDial,0);
  drumStuneDial=XtVaCreateManagedWidget("drumStuneDial",sgDialWidgetClass,
    drumBB, NULL);
  drumStuneLabel=XtVaCreateManagedWidget("drumStuneLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumStuneDial,XmNvalueChangedCallback,DrumStypeDial,0);
  XtAddCallback(drumStuneDial,XmNdragCallback,DrumStypeDial,0);
  drumStoneDial=XtVaCreateManagedWidget("drumStoneDial",sgDialWidgetClass,
    drumBB, NULL);
  drumStoneLabel=XtVaCreateManagedWidget("drumStoneLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumStoneDial,XmNvalueChangedCallback,DrumStypeDial,0);
  XtAddCallback(drumStoneDial,XmNdragCallback,DrumStypeDial,0);
  drumSsnappyDial=XtVaCreateManagedWidget("drumSsnappyDial",sgDialWidgetClass,
    drumBB, NULL);
  drumSsnappyLabel=XtVaCreateManagedWidget("drumSsnappyLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumSsnappyDial,XmNvalueChangedCallback,DrumStypeDial,0);
  XtAddCallback(drumSsnappyDial,XmNdragCallback,DrumStypeDial,0);
  drumStr808=XtVaCreateManagedWidget("drumStr808",xmToggleButtonWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumStr808,XmNvalueChangedCallback,DrumStypeDial,0);
  drumLlabel=XtVaCreateManagedWidget("drumLlabel",xmLabelWidgetClass,
    drumBB, NULL);
  drumLvolDial=XtVaCreateManagedWidget("drumLvolDial",sgDialWidgetClass,
    drumBB, NULL);
  drumLvolLabel=XtVaCreateManagedWidget("drumLvolLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumLvolDial,XmNvalueChangedCallback,DrumLvolDial,0);
  XtAddCallback(drumLvolDial,XmNdragCallback,DrumLvolDial,0);
  drumLtuneDial=XtVaCreateManagedWidget("drumLtuneDial",sgDialWidgetClass,
    drumBB, NULL);
  drumLtuneLabel=XtVaCreateManagedWidget("drumLtuneLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumLtuneDial,XmNvalueChangedCallback,DrumLtypeDial,0);
  XtAddCallback(drumLtuneDial,XmNdragCallback,DrumLtypeDial,0);
  drumLdecayDial=XtVaCreateManagedWidget("drumLdecayDial",sgDialWidgetClass,
    drumBB, NULL);
  drumLdecayLabel=XtVaCreateManagedWidget("drumLdecayLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumLdecayDial,XmNvalueChangedCallback,DrumLtypeDial,0);
  XtAddCallback(drumLdecayDial,XmNdragCallback,DrumLtypeDial,0);
  drumMlabel=XtVaCreateManagedWidget("drumMlabel",xmLabelWidgetClass,
    drumBB, NULL);
  drumMvolDial=XtVaCreateManagedWidget("drumMvolDial",sgDialWidgetClass,
    drumBB, NULL);
  drumMvolLabel=XtVaCreateManagedWidget("drumMvolLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumMvolDial,XmNvalueChangedCallback,DrumMvolDial,0);
  XtAddCallback(drumMvolDial,XmNdragCallback,DrumMvolDial,0);
  drumMtuneDial=XtVaCreateManagedWidget("drumMtuneDial",sgDialWidgetClass,
    drumBB, NULL);
  drumMtuneLabel=XtVaCreateManagedWidget("drumMtuneLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumMtuneDial,XmNvalueChangedCallback,DrumMtypeDial,0);
  XtAddCallback(drumMtuneDial,XmNdragCallback,DrumMtypeDial,0);
  drumMdecayDial=XtVaCreateManagedWidget("drumMdecayDial",sgDialWidgetClass,
    drumBB, NULL);
  drumMdecayLabel=XtVaCreateManagedWidget("drumMdecayLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumMdecayDial,XmNvalueChangedCallback,DrumMtypeDial,0);
  XtAddCallback(drumMdecayDial,XmNdragCallback,DrumMtypeDial,0);
  drumHlabel=XtVaCreateManagedWidget("drumHlabel",xmLabelWidgetClass,
    drumBB, NULL);
  drumHvolDial=XtVaCreateManagedWidget("drumHvolDial",sgDialWidgetClass,
    drumBB, NULL);
  drumHvolLabel=XtVaCreateManagedWidget("drumHvolLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumHvolDial,XmNvalueChangedCallback,DrumHvolDial,0);
  XtAddCallback(drumHvolDial,XmNdragCallback,DrumHvolDial,0);
  drumHtuneDial=XtVaCreateManagedWidget("drumHtuneDial",sgDialWidgetClass,
    drumBB, NULL);
  drumHtuneLabel=XtVaCreateManagedWidget("drumHtuneLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumHtuneDial,XmNvalueChangedCallback,DrumHtypeDial,0);
  XtAddCallback(drumHtuneDial,XmNdragCallback,DrumHtypeDial,0);
  drumHdecayDial=XtVaCreateManagedWidget("drumHdecayDial",sgDialWidgetClass,
    drumBB, NULL);
  drumHdecayLabel=XtVaCreateManagedWidget("drumHdecayLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumHdecayDial,XmNvalueChangedCallback,DrumHtypeDial,0);
  XtAddCallback(drumHdecayDial,XmNdragCallback,DrumHtypeDial,0);
  drumRIMlabel=XtVaCreateManagedWidget("drumRIMlabel",xmLabelWidgetClass,
    drumBB, NULL);
  drumRIMvolDial=XtVaCreateManagedWidget("drumRIMvolDial",sgDialWidgetClass,
    drumBB, NULL);
  drumRIMvolLabel=XtVaCreateManagedWidget("drumRIMvolLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumRIMvolDial,XmNvalueChangedCallback,DrumRIMvolDial,0);
  XtAddCallback(drumRIMvolDial,XmNdragCallback,DrumRIMvolDial,0);
  drumRIMvelDial=XtVaCreateManagedWidget("drumRIMvelDial",sgDialWidgetClass,
    drumBB, NULL);
  drumRIMvelLabel=XtVaCreateManagedWidget("drumRIMvelLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumRIMvelDial,XmNvalueChangedCallback,DrumRIMtypeDial,0);
  XtAddCallback(drumRIMvelDial,XmNdragCallback,DrumRIMtypeDial,0);
  drumHANlabel=XtVaCreateManagedWidget("drumHANlabel",xmLabelWidgetClass,
    drumBB, NULL);
  drumHANvolDial=XtVaCreateManagedWidget("drumHANvolDial",sgDialWidgetClass,
    drumBB, NULL);
  drumHANvolLabel=XtVaCreateManagedWidget("drumHANvolLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumHANvolDial,XmNvalueChangedCallback,DrumHANvolDial,0);
  XtAddCallback(drumHANvolDial,XmNdragCallback,DrumHANvolDial,0);
  drumHANvelDial=XtVaCreateManagedWidget("drumHANvelDial",sgDialWidgetClass,
    drumBB, NULL);
  drumHANvelLabel=XtVaCreateManagedWidget("drumHANvelLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumHANvelDial,XmNvalueChangedCallback,DrumHANtypeDial,0);
  XtAddCallback(drumHANvelDial,XmNdragCallback,DrumHANtypeDial,0);
  drumHHClabel=XtVaCreateManagedWidget("drumHHClabel",xmLabelWidgetClass,
    drumBB, NULL);
  drumHHCvolDial=XtVaCreateManagedWidget("drumHHCvolDial",sgDialWidgetClass,
    drumBB, NULL);
  drumHHCvolLabel=XtVaCreateManagedWidget("drumHHCvolLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumHHCvolDial,XmNvalueChangedCallback,DrumHHCvolDial,0);
  XtAddCallback(drumHHCvolDial,XmNdragCallback,DrumHHCvolDial,0);
  drumHHCdecayDial=XtVaCreateManagedWidget("drumHHCdecayDial",sgDialWidgetClass,
    drumBB, NULL);
  drumHHCdecayLabel=XtVaCreateManagedWidget("drumHHCdecayLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumHHCdecayDial,XmNvalueChangedCallback,DrumHHCtypeDial,0);
  XtAddCallback(drumHHCdecayDial,XmNdragCallback,DrumHHCtypeDial,0);
  drumHHOlabel=XtVaCreateManagedWidget("drumHHOlabel",xmLabelWidgetClass,
    drumBB, NULL);
  drumHHOvolDial=XtVaCreateManagedWidget("drumHHOvolDial",sgDialWidgetClass,
    drumBB, NULL);
  drumHHOvolLabel=XtVaCreateManagedWidget("drumHHOvolLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumHHOvolDial,XmNvalueChangedCallback,DrumHHOvolDial,0);
  XtAddCallback(drumHHOvolDial,XmNdragCallback,DrumHHOvolDial,0);
  drumHHOdecayDial=XtVaCreateManagedWidget("drumHHOdecayDial",sgDialWidgetClass,
    drumBB, NULL);
  drumHHOdecayLabel=XtVaCreateManagedWidget("drumHHOdecayLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumHHOdecayDial,XmNvalueChangedCallback,DrumHHOtypeDial,0);
  XtAddCallback(drumHHOdecayDial,XmNdragCallback,DrumHHOtypeDial,0);
  drumCSHlabel=XtVaCreateManagedWidget("drumCSHlabel",xmLabelWidgetClass,
    drumBB, NULL);
  drumCSHvolDial=XtVaCreateManagedWidget("drumCSHvolDial",sgDialWidgetClass,
    drumBB, NULL);
  drumCSHvolLabel=XtVaCreateManagedWidget("drumCSHvolLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumCSHvolDial,XmNvalueChangedCallback,DrumCSHvolDial,0);
  XtAddCallback(drumCSHvolDial,XmNdragCallback,DrumCSHvolDial,0);
  drumCSHtuneDial=XtVaCreateManagedWidget("drumCSHtuneDial",sgDialWidgetClass,
    drumBB, NULL);
  drumCSHtuneLabel=XtVaCreateManagedWidget("drumCSHtuneLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumCSHtuneDial,XmNvalueChangedCallback,DrumCSHtypeDial,0);
  XtAddCallback(drumCSHtuneDial,XmNdragCallback,DrumCSHtypeDial,0);
  drumRIDlabel=XtVaCreateManagedWidget("drumRIDlabel",xmLabelWidgetClass,
    drumBB, NULL);
  drumRIDvolDial=XtVaCreateManagedWidget("drumRIDvolDial",sgDialWidgetClass,
    drumBB, NULL);
  drumRIDvolLabel=XtVaCreateManagedWidget("drumRIDvolLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumRIDvolDial,XmNvalueChangedCallback,DrumRIDvolDial,0);
  XtAddCallback(drumRIDvolDial,XmNdragCallback,DrumRIDvolDial,0);
  drumRIDtuneDial=XtVaCreateManagedWidget("drumRIDtuneDial",sgDialWidgetClass,
    drumBB, NULL);
  drumRIDtuneLabel=XtVaCreateManagedWidget("drumRIDtuneLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumRIDtuneDial,XmNvalueChangedCallback,DrumRIDtypeDial,0);
  XtAddCallback(drumRIDtuneDial,XmNdragCallback,DrumRIDtypeDial,0);
  drumCLOlabel=XtVaCreateManagedWidget("drumCLOlabel",xmLabelWidgetClass,
    drumBB, NULL);
  drumCLOvolDial=XtVaCreateManagedWidget("drumCLOvolDial",sgDialWidgetClass,
    drumBB, NULL);
  drumCLOvolLabel=XtVaCreateManagedWidget("drumCLOvolLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumCLOvolDial,XmNvalueChangedCallback,DrumCLOvolDial,0);
  XtAddCallback(drumCLOvolDial,XmNdragCallback,DrumCLOvolDial,0);
  drumCLOcombDial=XtVaCreateManagedWidget("drumCLOcombDial",sgDialWidgetClass,
    drumBB, NULL);
  drumCLOcombLabel=XtVaCreateManagedWidget("drumCLOcombLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumCLOcombDial,XmNvalueChangedCallback,DrumCLOtypeDial,0);
  XtAddCallback(drumCLOcombDial,XmNdragCallback,DrumCLOtypeDial,0);
  drumOPClabel=XtVaCreateManagedWidget("drumOPClabel",xmLabelWidgetClass,
    drumBB, NULL);
  drumOPCvolDial=XtVaCreateManagedWidget("drumOPCvolDial",sgDialWidgetClass,
    drumBB, NULL);
  drumOPCvolLabel=XtVaCreateManagedWidget("drumOPCvolLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumOPCvolDial,XmNvalueChangedCallback,DrumOPCvolDial,0);
  XtAddCallback(drumOPCvolDial,XmNdragCallback,DrumOPCvolDial,0);
  drumOPCcombDial=XtVaCreateManagedWidget("drumOPCcombDial",sgDialWidgetClass,
    drumBB, NULL);
  drumOPCcombLabel=XtVaCreateManagedWidget("drumOPCcombLabel",xmLabelWidgetClass,
    drumBB, NULL);
  XtAddCallback(drumOPCcombDial,XmNvalueChangedCallback,DrumOPCtypeDial,0);
  XtAddCallback(drumOPCcombDial,XmNdragCallback,DrumOPCtypeDial,0);

  drumvolDial=XtVaCreateManagedWidget("drumvolDial",sgDialWidgetClass,
    drumBB2, NULL);
  drumvolLabel=XtVaCreateManagedWidget("drumvolLabel",xmLabelWidgetClass,
    drumBB2, NULL);
  XtAddCallback(drumvolDial,XmNvalueChangedCallback,DrumvolDial,0);
  XtAddCallback(drumvolDial,XmNdragCallback,DrumvolDial,0);
  drumpanDial=XtVaCreateManagedWidget("drumpanDial",sgDialWidgetClass,
    drumBB2, NULL);
  drumpanLabel=XtVaCreateManagedWidget("drumpanLabel",xmLabelWidgetClass,
    drumBB2, NULL);
  XtAddCallback(drumpanDial,XmNvalueChangedCallback,DrumpanDial,0);
  XtAddCallback(drumpanDial,XmNdragCallback,DrumpanDial,0);
  drumdelayDial=XtVaCreateManagedWidget("drumdelayDial",sgDialWidgetClass,
    drumBB2, NULL);
  drumdelayLabel=XtVaCreateManagedWidget("drumdelayLabel",xmLabelWidgetClass,
    drumBB2, NULL);
  XtAddCallback(drumdelayDial,XmNvalueChangedCallback,DrumdelayDial,0);
  XtAddCallback(drumdelayDial,XmNdragCallback,DrumdelayDial,0);
  drumpcfToggle=XtVaCreateManagedWidget("drumpcfToggle",
    xmToggleButtonWidgetClass,drumBB2,NULL);
  XtAddCallback(drumpcfToggle,XmNvalueChangedCallback,DrumpcfToggle,NULL);
  

  songToggle=XtVaCreateManagedWidget("songToggle",
    xmToggleButtonWidgetClass,topBB,NULL);
  loopToggle=XtVaCreateManagedWidget("loopToggle",
    xmToggleButtonWidgetClass,topBB,NULL);
  songBB=XmCreateFormDialog(topLevel,"songBB",NULL,0);
  songMatrix = XtVaCreateManagedWidget("songMatrix",
    xbaeMatrixWidgetClass, songBB,
    XmNcolumnWidths,   widths,
    XmNcells,          NULL,
    XmNcolumnLabels,   columnlabels,
    XmNrowLabels,      rowlabels,
    XmNrowLabelWidth,  5,
/*    XmNcolumnAlignments,columnalignments, */
/*    XmNbottomWidget,   sovervOkButton, */
    NULL);
/*
  XtAddCallback(songMatrix, XmNselectCellCallback, MatrixSelect, NULL);
  XtAddCallback(songMatrix, XmNenterCellCallback,  MatrixEnter, NULL);
  XtAddCallback(songMatrix, XmNleaveCellCallback,  MatrixLeave, NULL);
*/

  printf("."); fflush(stdout);


  /* FILE MENU */

  fileButton=XtVaCreateManagedWidget(
    "fileButton",xmCascadeButtonWidgetClass,menuBar,NULL);
  fileMenu=XmCreatePulldownMenu(menuBar,"fileMenu",NULL,0);
  XtVaSetValues(fileButton,XmNsubMenuId,fileMenu,NULL);

  fileWarning=XmCreateWarningDialog(mainWindow,"fileWarning",NULL,0);
  temp=XmMessageBoxGetChild(fileWarning,XmDIALOG_HELP_BUTTON);
  XtUnmanageChild(temp);
  temp=XmMessageBoxGetChild(fileWarning,XmDIALOG_CANCEL_BUTTON);
  XtUnmanageChild(temp);

  load=XtVaCreateManagedWidget("load",xmPushButtonWidgetClass,fileMenu,
    XmNmnemonic,XStringToKeysym("L"),XmNaccelerator,"Ctrl<Key>L",
    XmNacceleratorText,XmStringCreateLocalized("Ctrl+L"),NULL);
  loadFileBox=XmCreateFileSelectionDialog(topLevel,"loadFileBox",NULL,0);
  temp=XmFileSelectionBoxGetChild(loadFileBox,XmDIALOG_HELP_BUTTON);
  XtUnmanageChild(temp);
  XtAddCallback(loadFileBox,XmNcancelCallback,Cancel,loadFileBox);
  XtAddCallback(loadFileBox,XmNokCallback,LoadOk,NULL);
  XtAddCallback(load,XmNactivateCallback,Load,NULL);

  save=XtVaCreateManagedWidget("save",xmPushButtonWidgetClass,fileMenu,
    XmNmnemonic,XStringToKeysym("S"),XmNaccelerator,"Ctrl<Key>S",
    XmNacceleratorText,XmStringCreateLocalized("Ctrl+S"),NULL);
  saveFileBox=XmCreateFileSelectionDialog(topLevel,"saveFileBox",NULL,0);
  temp=XmFileSelectionBoxGetChild(saveFileBox,XmDIALOG_HELP_BUTTON);
  XtUnmanageChild(temp);
  XtAddCallback(saveFileBox,XmNcancelCallback,Cancel,saveFileBox);
  XtAddCallback(saveFileBox,XmNokCallback,SaveOk,NULL);
  XtAddCallback(save,XmNactivateCallback,Save,NULL);
  
  bounce=XtVaCreateManagedWidget("bounce",xmPushButtonWidgetClass,fileMenu,
    XmNmnemonic,XStringToKeysym("B"),XmNaccelerator,"Ctrl<Key>B",
    XmNacceleratorText,XmStringCreateLocalized("Ctrl+B"),NULL);
  bounceFileBox=XmCreateFileSelectionDialog(topLevel,"bounceFileBox",NULL,0);
  temp=XmFileSelectionBoxGetChild(bounceFileBox,XmDIALOG_HELP_BUTTON);
  XtUnmanageChild(temp);
  XtAddCallback(bounceFileBox,XmNcancelCallback,Cancel,bounceFileBox);
  XtAddCallback(bounceFileBox,XmNokCallback,BounceOk,NULL);
  XtAddCallback(bounce,XmNactivateCallback,Bounce,NULL);

  quit=XtVaCreateManagedWidget(
    "quit",xmPushButtonWidgetClass,fileMenu,
    XmNmnemonic,XStringToKeysym("Q"),XmNaccelerator,"Ctrl<Key>Q",
    XmNacceleratorText,XmStringCreateLocalized("Ctrl+Q"),NULL);
  XtAddCallback(quit,XmNactivateCallback,Quit,0);


  /* EDIT MENU */

  text=XmStringCreateLocalized("Edit");
  editButton=XtVaCreateManagedWidget(
    "editButton",xmCascadeButtonWidgetClass,menuBar,
    XmNlabelString,text,NULL);

  editMenu=XmCreatePulldownMenu(menuBar,"editMenu",NULL,0);
  XtVaSetValues(editButton,XmNsubMenuId,editMenu,NULL);

  text=XmStringCreateLocalized("Cut");
  cut=XtVaCreateManagedWidget(
    "cut",xmPushButtonWidgetClass,editMenu,XmNlabelString,text,
    XmNaccelerator,"Ctrl<Key>X",
    XmNacceleratorText,XmStringCreateLocalized("Ctrl+X"),NULL);
/*  XtAddCallback(cut,XmNactivateCallback,Cut,NULL); */

  text=XmStringCreateLocalized("Copy");
  copy=XtVaCreateManagedWidget(
    "copy",xmPushButtonWidgetClass,editMenu,XmNlabelString,text,
    XmNaccelerator,"Ctrl<Key>C",
    XmNacceleratorText,XmStringCreateLocalized("Ctrl+C"),NULL);
/*  XtAddCallback(copy,XmNactivateCallback,Copy,NULL); */

  text=XmStringCreateLocalized("Paste");
  paste=XtVaCreateManagedWidget(
    "paste",xmPushButtonWidgetClass,editMenu,XmNlabelString,text,
    XmNaccelerator,"Ctrl<Key>V",
    XmNacceleratorText,XmStringCreateLocalized("Ctrl+V"),NULL);
/*  XtAddCallback(paste,XmNactivateCallback,Paste,NULL); */
  XtVaSetValues(paste,XmNsensitive,False,NULL);


  /* WINDOWS MENU */

  text=XmStringCreateLocalized("Windows");
  winButton=XtVaCreateManagedWidget(
    "winButton",xmCascadeButtonWidgetClass,menuBar,
    XmNlabelString,text,NULL);

  winMenu=XmCreatePulldownMenu(menuBar,"winMenu",NULL,0);
  XtVaSetValues(winButton,XmNsubMenuId,winMenu,NULL);

  text=XmStringCreateLocalized("Drum");
  drumwin=XtVaCreateManagedWidget(
    "drumwin",xmPushButtonWidgetClass,winMenu,XmNlabelString,text,
    XmNaccelerator,"Ctrl<Key>D",
    XmNacceleratorText,XmStringCreateLocalized("Ctrl+D"),NULL);
  XtAddCallback(drumwin,XmNactivateCallback,Drumwin,NULL);

  text=XmStringCreateLocalized("Song");
  songwin=XtVaCreateManagedWidget(
    "songwin",xmPushButtonWidgetClass,winMenu,XmNlabelString,text,
    XmNaccelerator,"Ctrl<Key>N",
    XmNacceleratorText,XmStringCreateLocalized("Ctrl+N"),NULL);
  XtAddCallback(songwin,XmNactivateCallback,Songwin,NULL);


  sendtoButton=XtVaCreateManagedWidget(
    "sendtoB",xmPushButtonWidgetClass,winMenu,
    XmNmnemonic,XStringToKeysym("I"),XmNaccelerator,"Ctrl<Key>I",
    XmNacceleratorText,XmStringCreateLocalized("Ctrl+I"),NULL);
  XtAddCallback(sendtoButton,XmNactivateCallback,SendtoB,NULL);

  sendtoBB=XmCreateBulletinBoardDialog(topLevel,"sendtoBB",NULL,0);
  sendtoOkButton=XtVaCreateManagedWidget("sendtoOk",xmPushButtonWidgetClass,
    sendtoBB,NULL);
  XtVaSetValues(sendtoBB,XmNdefaultButton,sendtoOkButton);
  sendtoAccept=XtVaCreateManagedWidget("sendtoAccept",xmToggleButtonWidgetClass,    sendtoBB,NULL);
  XtAddCallback(sendtoAccept, XmNvalueChangedCallback, AcceptConnect, NULL);

  sendtoTogLabel=XtVaCreateManagedWidget("sendtoLabel1",
    xmLabelWidgetClass,sendtoBB,NULL);
  sendtoTextLabel=XtVaCreateManagedWidget("sendtoLabel2",
    xmLabelWidgetClass,sendtoBB,NULL);

  for (i=0; i<MAXSENDTO; i++) {
    sendto_dummy[i]=i; sendto_connected[i]=0;
    sendtoToggle[i]=XtVaCreateManagedWidget("sendtoToggle",
      xmToggleButtonWidgetClass,sendtoBB,NULL);
    XtVaSetValues(sendtoToggle[i],XmNx,20,XmNy,i*40+45,NULL);
    XtAddCallback(sendtoToggle[i],XmNvalueChangedCallback,SendtoConnect,
      &sendto_dummy[i]);
    sendtoText[i]=XtVaCreateManagedWidget("sendtoText",
      xmTextFieldWidgetClass,sendtoBB,NULL);
    XtVaSetValues(sendtoText[i],XmNx,60,XmNy,i*40+40,NULL);
  }



  printf("."); fflush(stdout);

  /* HELP MENU */

  helpButton=XtVaCreateManagedWidget(
    "helpButton",xmCascadeButtonWidgetClass,menuBar,NULL);
  XtVaSetValues(menuBar,XmNmenuHelpWidget,helpButton,NULL);
  helpMenu=XmCreatePulldownMenu(menuBar,"helpMenu",NULL,0);
  XtVaSetValues(helpButton,XmNsubMenuId,helpMenu,NULL);

  help=XtVaCreateManagedWidget(
    "help",xmPushButtonWidgetClass,helpMenu,NULL);
  helpBox=XmCreateMessageDialog(topLevel,"helpBox",NULL,0);
  text=XmStringCreateLtoR(
    "PPE is the Precicion Pulse Engine.\n\n\
             Version 0.00,   Oyvind Hammer, NoTAM 1998\n\n\
Please send your comments to oyvindha@notam.uio.no.",
     XmFONTLIST_DEFAULT_TAG);
  XtVaSetValues(helpBox,XmNmessageString,text,XmNwidth,500,NULL);

  temp=XmMessageBoxGetChild(helpBox,XmDIALOG_CANCEL_BUTTON);
  XtUnmanageChild(temp);
  temp=XmMessageBoxGetChild(helpBox,XmDIALOG_HELP_BUTTON);
  XtUnmanageChild(temp);
  XtAddCallback(help,XmNactivateCallback,ShowHelp,helpBox);

  connectWarning=XmCreateWarningDialog(mainWindow,"connectWarning",NULL,0);
  temp=XmMessageBoxGetChild(connectWarning,XmDIALOG_HELP_BUTTON);
  XtUnmanageChild(temp);
  temp=XmMessageBoxGetChild(connectWarning,XmDIALOG_CANCEL_BUTTON);
  XtUnmanageChild(temp);

  slavefailWarning=XmCreateWarningDialog(mainWindow,"slavefailWarning",NULL,0);
  temp=XmMessageBoxGetChild(slavefailWarning,XmDIALOG_HELP_BUTTON);
  XtUnmanageChild(temp);
  temp=XmMessageBoxGetChild(slavefailWarning,XmDIALOG_CANCEL_BUTTON);
  XtUnmanageChild(temp);

  XtRealizeWidget(topLevel);

  defdepth=DefaultDepthOfScreen(XtScreen(topLevel));
  playpixmap=XCreatePixmapFromBitmapData(XtDisplay(topLevel),
    XtWindow(topLevel),play_bits, 16, 16, black.pixel, red.pixel, defdepth);
  noplaypixmap=XCreatePixmapFromBitmapData(XtDisplay(topLevel),
    XtWindow(topLevel),play_bits, 16, 16, black.pixel, yellow.pixel, defdepth);
  stoppixmap=XCreatePixmapFromBitmapData(XtDisplay(topLevel),
    XtWindow(topLevel),stop_bits, 16, 16, black.pixel, red.pixel, defdepth);
  nostoppixmap=XCreatePixmapFromBitmapData(XtDisplay(topLevel),
    XtWindow(topLevel),stop_bits, 16, 16, black.pixel, yellow.pixel, defdepth);
  XtVaSetValues(playB,XmNlabelType,XmPIXMAP,XmNlabelPixmap,
    noplaypixmap,NULL);
  XtVaSetValues(stopB,XmNlabelType,XmPIXMAP,XmNlabelPixmap,
    nostoppixmap,NULL);

  flush_all_underflows_to_zero();
  init_drums();
  init_samps();
  init_values();
  init_patterns();
  redraw_all();

  /* Set me up as a server, accepting controls. */

  server_socket=socket(AF_INET, SOCK_STREAM, 0);
  me_sockaddr.sin_family=AF_INET;
  me_sockaddr.sin_port=htons(PORTNUM);
  me_sockaddr.sin_addr.s_addr=htonl(INADDR_ANY);
  bind(server_socket, (struct sockaddr *)&me_sockaddr,sizeof(me_sockaddr));
  listen(server_socket, 5);
  fcntl(server_socket, F_SETFL, FNDELAY);
  for (i=0; i<MAXCLIENTS; i++) client_sockets[i]=-1;

  XtAppMainLoop(app_context);

}
