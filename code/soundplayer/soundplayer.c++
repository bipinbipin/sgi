/****************************************************************************
 *
 * NAME
 *	soundplayer.c++
 *
 * USAGE
 *	soundplayer [-nodisplay] [-nofork] [-nopocket] [filename]
 *
 * DESCRIPTION
 *	play sounds
 *		- AIFF/AIFF-C, .wav, .au/.snd
 *		- MIDI file, .mid
 *		- MPEG audio bitstream (.mp2 or .mpga)
 *
 * AUTHOR
 *	Bryan James (bryanj)
 *
 ****************************************************************************/
 
#if DEBUG
#define dprintf	printf
#else
#define dprintf if(0)printf
#endif /*DEBUG*/

/* include files */

// Motif
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/DrawingA.h>
#include <Xm/MwmUtil.h>
#include <Xm/FileSB.h>
#include <Xm/Scale.h>
#include <Xm/LabelG.h>
#include <Xm/TextF.h>
#include <Xm/Separator.h>

#include <Sgm/DropPocket.h>
#include <Sgm/ThumbWheel.h>
#include <Sgm/SpringBox.h>

// Include headers for ViewKit classes
#include <Vk/VkApp.h>
#include <Vk/VkWindow.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkRadioSubMenu.h>
#include <Vk/VkFileSelectionDialog.h>
#include <Vk/VkErrorDialog.h>
#include <Vk/VkRunOnce.h>
#include <Vk/VkNameList.h>
#include <Vk/VkIconButton.h>
#include <Vk/VkResource.h>

#include <TransportButton.h>

// General
#include <sys/schedctl.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>    
#include <sys/stat.h>		/* for stat() call */
#include <stdlib.h>
#include <stdio.h>
#include <ulocks.h>
#include <bstring.h>
#include <stropts.h>
#include <poll.h>
#include <limits.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>		/* for basename() */
#include <assert.h>
#include <stropts.h>
#include <dlfcn.h>
#include <math.h>
#include <sys/syssgi.h>

// Digital Media

#include <dmedia/audio.h>
#include <dmedia/audiofile.h>
#include <dmedia/midi.h>
#include "midifile.h"

// Icons
#include "play.xpm"
#include "stop.xpm"
#include "rewind.xpm"

#include "intlresPlayer.h"

// Macros
#define TIMER_INTERVAL 500
#define DONE		(-1)
#define PLAY		1
#define STOP		0
#define PAUSE		3

#define BADFILE		(-1)
#define UNIDENTIFIED	0
#define AUDIOFILE	1
#define MPEGBSTREAM	2
#define MIDIFILE	3

#define MODE_STEREO		0
#define MODE_JOINT_STEREO	1 	/*intensity stereo and/or ms_stereo*/
#define MODE_DUAL_CHANNEL	2
#define MODE_SINGLE_CHANNEL	3

#define EMPHASIS_NONE		0
#define EMPHASIS_50_15_USEC	1
#define EMPHASIS_CCITT_J17	3

#define OKAY   0
#define BAD   -2

/*
 * Values for minimum and maximum dB scale factors
 */
#define SCALE_MIN_DB	(-30)
#define SCALE_MAX_DB	6
#define SCALE_NORM_DB	0
#define TEMPO_MIN_PERCENT   50
#define TEMPO_MAX_PERCENT   150

/*
 * Create some types
 */
 
typedef struct fileinfo_s {
    int filetype;
    int rate;
    int nchannels;
    int bitspersamp;
} fileinfo_t;

/*
 * unpacked info from MPEG audio frame header
 */
typedef struct {
    int layer; 		   /* 1, 2, or 3 */
    int error_protection;  /*  0  or 1 */
    int bitrate;           /* for layer 1:
                            *   32000, 64000, 96000, 128000, 160000,  
                            *   192000, 224000, 256000, 288000, 320000,352000,
                            *   384000, 416000, 448000, or 0 = free format
                            * for layer 2:
                            *   32000, 48000, 56000, 64000, 80000,
                            *   96000, 112000, 128000, 160000, 192000, 224000,
                            *   256000, 320000, 384000, or 0 = free format
                            * for layer 3:
                            *   32000, 40000, 48000, 56000, 64000,
                            *   80000, 96000, 112000, 128000, 160000, 192000,
                            *   224000, 256000, 320000, or 0 = free format
                            */
    int sample_rate;       /* 32000, 44100, or 48000 */
    int padding;           /* 0 or 1 */
    int mode;              /* MODE_STEREO, MODE_JOINT_STEREO,
                            * MODE_DUAL_CHANNEL, or MODE_SINGLE_CHANNEL 
                            */
    int mode_extension;    /* 0, 1, 2, or 3 */
    int copyright;         /* 0 or 1 */
    int original;          /* 0 or 1 */
    int emphasis;          /* EMPHASIS_NONE, EMPHASIS_50_15_USEC, or
                            *   EMPHASIS_CCITT_J17 
                            */
} mpegheader_t;

typedef struct soundplayer_s {
    char	*filename;	/* name of the file */
    int		nplayed;	/* number of samples/bytes played */
    int		ntotal;		/* number of total frames in file */
    int		oldplayed;	/* previous no. samples/bytes */
    int		nowplaying;	/* state of the program */
    int		highwatermark;
    int		lowwatermark;
    int		pid;		/* process id of the audio process */
    struct pollfd PollFD[2];
    ALconfig	config;
    ALport	port;
    long	queueSize;	/* size of the audio queue in samples */
    AFfilehandle fileHdl;		/* for files supported by AF */
    FILE	*fp;
    int		numChannels;
    char 	*tmpBuf;
    int		eof;
    int		errfd;		/* to send back errors to UI thread */
    int		ui_fd;		/* to send normal msgs to UI thread */
    int		midiLibrary;	/* 1 = have libmd.so; 0 = do not */
    int		midiReady;	/* flag: 0 = not ready; 1 = ready */
    int		midiPortCount;
    MDport	midiPort;
    MFfile	*midiFile;
    char**	midiNames;
    int		midiDevice;	/* MIDI device in use; index into names */
    int		midiFileTempo;	/* tempo as held in file */
    int		midiTempo;	/* current tempo of MIDI playback */
    int		midiBeatsPerSec;/* beats per second */
    int		midiTempoScale; /* scale MIDI tempo (percent) [10...200] */
    int		nofork;		/* don't fork the process */
    int		noauto;		/* don't automatically play file */
    int		nopocket;	/* no drop pocket */
    int		nounique;
    fileinfo_t	info;
    mpegheader_t mpegInfo;
    long long   lastPause;
    float	audioScale;	/* floating pt scale value */
    int		audioScaleDB;	/* last value of scale in dB */
    int		cuePosition;	/* where to cue the playback */
    long long   lastStamp;	/* last MIDI timestamp */
} soundplayer_t;

class SpWindow : public VkWindow
{
    public:
	SpWindow(const char	*windowName,
		 ArgList	args,
		 Cardinal	argCount,
		 soundplayer_t  *sp);
	~SpWindow();
	
	soundplayer_t	*_sp;
	
	void SetTempoValue(int tempo);
	
    protected:
	void handleWmDeleteMessage();
	void handleWmQuitMessage();
	
    private:
	Widget _form, _rowcol;
	Widget _tempoWheel;
	Widget _tempoText;
	Widget _levelWheel;
	Widget _levelText;
	
	// callbacks (must be static)
	static void TempoWheelCallback(Widget, XtPointer, XtPointer);
	static void TempoEntryCallback(Widget, XtPointer, XtPointer);
	static void LevelWheelCallback(Widget, XtPointer, XtPointer);
	static void LevelEntryCallback(Widget, XtPointer, XtPointer);
	static void TextNumericalVerifyCB(Widget, XtPointer, XtPointer);
	static void TextPositiveNumericalVerifyCB(Widget, XtPointer, 
							    XtPointer);
	
	// parameter functions
	int  HandleTempoWheel(Widget);
	int  HandleLevelWheel(Widget);
	Boolean IsNumerical(char *s);
	Boolean IsPositiveNumerical(char *s);
};

/* function prototypes */

static double apLinearToDecibel(double, double);
static double apDecibelToLinear(double, double);
static void AllNotesOff(MDport);
static void ApanelCallback(Widget, XtPointer, XtPointer);
static void AudioProcess(void *);
static void AudioProcessErrorProc(XtPointer, int*, XtInputId*);
static void CloneFileCallback(Widget, XtPointer, XtPointer);
static void CreateWidgets(VkApp *, VkWindow *, soundplayer_t *);
static void DragCallback(Widget, XtPointer, XtPointer);
static void ExitAudioProcess(soundplayer_t *);
static void FreeResources(soundplayer_t*);
static void GetMoreAudioSamples(soundplayer_t *);
static void GetMoreMIDIMessages(soundplayer_t *);
static char* GetStringFromXmString(XmString);
static int  HandleNewFile(char *, soundplayer_t*);
static int  IdentifyFileType(char *);
static int  IdentifyMIDIFile(int);
static int  IdentifyMPEGBitstream(int);
static void InitializeSemaphores(soundplayer_t *);
static int  LoadMIDILibrary(void);
static void MessageProc(XtPointer, int*, XtInputId*);
static void MIDIDeviceCallback(Widget, XtPointer, XtPointer);
static void SynthPanelCallback(Widget, XtPointer, XtPointer);
static int  OpenAudioFile(soundplayer_t *);
static int  OpenAudioPort(soundplayer_t *);
static int  OpenMIDIFile(soundplayer_t *);
static int  OpenMIDIPort(soundplayer_t *);
static void OpenFileCallback(Widget, XtPointer, XtPointer);
static void PanicMIDI(MDport);
static int  ParseArgs(int, char**, soundplayer_t*);
static int  ParseSoundPlayerArgs(int, char**, soundplayer_t*);
static int  ParseSfplayArgs(int, char**, soundplayer_t*);
static void PauseCallback(Widget, XtPointer, XtPointer);
static void PlayCallback(Widget, XtPointer, XtPointer);
static void PlayCtrlCallback(Widget, XtPointer, XtPointer);
static void PositionCallback(Widget, XtPointer, XtPointer);
static void ProcessAudioCommand(soundplayer_t *);
static void ProcessMIDICommand(soundplayer_t *);
static void QuitApp(soundplayer_t *);
static void QuitCallback(Widget, XtPointer, XtPointer);
static int  ReadHeader(FILE *, mpegheader_t *, int *);
static void ResetAllControllers(MDport);
static void RewindCallback(Widget, XtPointer, XtPointer);
static void SaveFileCallback(Widget, XtPointer, XtPointer);
static void SendAudioCommand(char *cmd);
static void SendError(int);
static void SendMessage(char *cmd);
static void SendProgramChanges(soundplayer_t*);
static void TimerCallback(XtPointer, XtIntervalId*);
static void UpdateCallback(VkCallbackObject *, void*, void*);
static void UpdateFilenameDisplay(soundplayer_t* sp);
static void Usage(void);

/****************************************************************************
 * 
 * global variables
 * 
 ****************************************************************************/

/* function ptrs for MIDI library */
int		    (*_mdInit)(void);
char*		    (*_mdGetName)(int);
int		    (*_mdSend)(MDport, MDevent*, int);
long long	    (*_mdPause)(MDport);
long long	    (*_mdTell)(MDport);
long long	    (*_mdTellNow)(MDport);
int		    (*_mdPanic)(MDport);
int		    (*_mdWakeup)(MDport);
int		    (*_mdClosePort)(MDport);
int		    (*_mdSetStampMode)(MDport, int);
int		    (*_mdGetStatus)(char*);
int		    (*_mdSetDivision)(MDport, int);
int		    (*_mdSetTempo)(MDport, int);
MDport		    (*_mdOpenOutPort)(char*);
int		    (*_mdGetFd)(MDport);
int		    (*_mdSetOrigin)(MDport, long long);
long long	    (*_mdGetOrigin)(MDport);
int		    (*_mdSetTemposcale)(MDport, double);
void		    (*_mdSetStartPoint)(MDport, long long, long long);
char*		    (*_mdGetDefault)(int);

static Widget		pocket, progIndicator, form;
static Widget		audioFrame, midiFrame;
static XtIntervalId 	timerID;
static XtInputId	inputID[2];
static XtAppContext	app;
static SpWindow		*win;
static VkApp		*vkapp;
static VkRadioSubMenu	*mididevmenu;
static TransportButton  *vkButton1, *vkButton2, *vkButton3;
static VkMenuToggle	*playControlToggle;

#define ERR_NOPORTS	0
#define ERR_BADMPEG	1
#define ERR_NOMIDI	2

char	*errMessage[] = {
    "Unable to open audio port",
    "Unable to decode MPEG audio bitstream",
    "Unable to open MIDI connection"   
};

char	*command;	/* UI command to audio thread */
char	*message;	/* audio thread -> UI thread message */
usema_t	*SendSema;	/* send semaphore: UI to audio thread */
usema_t	*RcvSema;	/* receive semaphore: audio thread to UI */
usema_t	*ErrSema;	/* error semaphore: audio thread to UI */
usema_t	*UISema;	/* UI semaphore: audio thread to UI */
usema_t *startSema;	/* UI waits on this semaphore before sending
			 * messages to the playback process */
int	errNumber;	/* error type */

char	*progName;	/* name of the program */
char	*appTitle;	/* title bar name */
static int audioproc;	/* XXX - shadow of the audio process pid */
static int nodisplay;	/* 1 = nodisplay; 0 = display */

static char* usageString =
"soundplayer [-nofork] [-nodisplay] [-nopocket] [filename]\n";

/*--------------------------------------------------------------------------*/

void
main(int argc, char **argv)
{
    Arg args[10];
    int n, s, i;
    soundplayer_t player;

    progName = strdup(basename(argv[0]));
    /*
     * before we do anything, swap the real and effective user
     * ID's. in the case that the file is setuid root, we swap
     * ID's to insure that the user's file permissions, etc.
     * are used when saving and opening files.
     */
    setreuid(geteuid(), getuid());
    setregid(getegid(), getgid());
    /*
     * initialize parts of the soundplayer_t struct that need it.
     */
    bzero(&player, sizeof(soundplayer_t));
    player.ntotal = 1;	/* need a positive number here */
    player.nowplaying = PAUSE;
    player.audioScale = 1.0f;
    player.audioScaleDB = SCALE_NORM_DB;
    player.midiTempo = -1;
    player.midiFileTempo = 0;
    player.midiTempoScale = 100;
    
    s = ParseArgs(argc, argv, &player);
    if (s == -1) exit(-1);
    
    if (!player.nofork && nodisplay == 0) {	/* fork off */
    	switch(fork()) {
    	    case -1:	/* bad fork */
    	    	fprintf(stderr, "%s: unable to fork process\n",
    	    		progName);
    	    	exit(-1);
    	    case 0:	/* child process */
    	    	break;
    	    default:	/* parent (foreground) process */
    	    	exit(0);
    	}
    }
    // Turn off AL, AF, and Aware messages to the console
    alSetErrorHandler(0);
    afSetErrorHandler(0);
    /*
     * dynamically load the midi library and get ptrs to all
     * the functions we wish to use. this is performed so we
     * don't require MIDI library/driver to be installed. if
     * it is not, we can still play audio files just fine.
     */
    if (LoadMIDILibrary()) {	/* > 0 is error condition */
	player.midiLibrary = 0;
    }
    else {
	player.midiLibrary = 1;
    }
    
    if (player.midiLibrary) {
	/*
	 * query the number of MIDI devices currently configured
	 * and get their names.
	 */
	if ((player.midiPortCount = _mdInit()) <= 0)	
	    player.midiReady = 0;
	else 
	    player.midiReady = 1;
	    
	if (player.midiReady) {
	    player.midiNames = (char **)malloc(player.midiPortCount * 
		sizeof(char*));
	    for (i = 0; i < player.midiPortCount; i++) {
		player.midiNames[i] = strdup(_mdGetName(i));
	    }
	}
    }
    
    if (nodisplay == 0) {
	VkMenuToggle *tgl;
	Window rootReturn, childReturn;
	int rootX, rootY, winX, winY;
	unsigned int maskReturn;
	char geomString[20];

	XtSetLanguageProc( NULL, NULL, NULL );
	
	// Create an application object
	vkapp = new VkApp("SoundPlayer", &argc, argv);
	
	app = vkapp->appContext();

        XtGetSubresources( theApplication->baseWidget(), NULL, "intl", "Intl",
                           intl_resources, intl_num_resources, NULL, 0 );
        errMessage[0] = _intl_UnOpenPort;
        errMessage[1] = _intl_UnDecodeMPEG;
        errMessage[2] = _intl_UnOpenMIDI;

	appTitle = (char *) VkGetResource("title", "SoundPlayer");
	
	/*
	 * enable "run once" capability.
	 */
	if (!player.nounique) {
	    VkNameList *argList = new VkNameList();
	    for (i = 1; i < argc; i++) {
		argList->add(argv[i]);
	    }
	    VkRunOnce  *runOnce = new VkRunOnce(argList);
	    runOnce->addCallback(VkRunOnce::invokedCallback, UpdateCallback,
		&player);
	}
	/*
	 * Query the location of the pointing device (mouse) at the
	 * moment. We place soundplayer automatically at this point.
	 */
	XQueryPointer(vkapp->display(), DefaultRootWindow(vkapp->display()),
	    &rootReturn, &childReturn, &rootX, &rootY, &winX, &winY, 
	    &maskReturn);
	sprintf(geomString, "+%d+%d", rootX, rootY);
	/*
	 * Create an SpWindow.
	 */
	n = 0;
	XtSetArg(args[n], XmNgeometry, geomString);  n++;
	win = new SpWindow("SoundPlayer", args, n, &player);
		
	if (player.midiReady) {
	    tgl = (VkMenuToggle*)mididevmenu->
		findNamedItem(player.midiNames[player.midiDevice]);
	    tgl->setVisualState(TRUE);
	}

	win->show();  // Display the window

	InitializeSemaphores(&player);
	
	/*
	 * Enable message passing from the playing process by adding
	 * the fd's of the error & message semaphores to the Xt main loop.
	 */    
	inputID[0] = XtAppAddInput(app, player.errfd, 
	    (XtPointer)XtInputReadMask, AudioProcessErrorProc, &player);
	uspsema(ErrSema);	/* allow a message to be sent */
	inputID[1] = XtAppAddInput(app, player.ui_fd, 
	    (XtPointer)XtInputReadMask, MessageProc, win);
	uspsema(UISema);	/* allow a message to be sent */
	
	/*
	 * If we have a file (i.e. one was provided on the command line),
	 * do the right thing.
	 */
	if (player.filename) {
	    player.info.filetype = IdentifyFileType(player.filename);
	    if (player.info.filetype == AUDIOFILE || 
		player.info.filetype == MPEGBSTREAM ||
		player.info.filetype == MIDIFILE) {

		    if (HandleNewFile(player.filename, &player)) {
		        if (!player.noauto) {
			    vkButton3->select();
			    vkButton2->deselect();
			} else {
			    vkButton3->deselect();
			    vkButton2->select();
			}
			if (!timerID)
			    timerID = XtAppAddTimeOut(app, TIMER_INTERVAL, 
				    		TimerCallback, &player);   
		    }
		    else {
			vkButton3->deselect();
			vkButton2->select();
		    }
	    }
	    else if (player.info.filetype == UNIDENTIFIED) {
		theErrorDialog->setTitle( _intl_ErrorTitleOFReadError );
                theErrorDialog->post( _intl_ErrorPostCRUnsupportFormat );
	    }
	    else {
		theErrorDialog->setTitle( _intl_ErrorTitleOFReadError );
                theErrorDialog->post( _intl_ErrorPostNotOpenSelFile );
	    }
	}
	else {
	    vkButton2->select();  // turn on the stop button
	}
	theApplication->setVersionString("productInfo");
	vkapp->run();   // Run the application
    }
    else {  /* nodisplay == 1 */
	
	InitializeSemaphores(&player);
	if (player.filename) {
	    player.info.filetype = IdentifyFileType(player.filename);
	    if (player.info.filetype == AUDIOFILE || 
		player.info.filetype == MPEGBSTREAM ||
		(player.info.filetype == MIDIFILE && player.midiReady) ) {
		player.pid = sproc(AudioProcess, PR_SALL & ~PR_SID, &player);
		audioproc = 1;
		while (player.nowplaying != DONE) sleep(1);
	    }
	    else {
		fprintf(stderr, "%s: File format unsupported.\n", progName);
		exit(-1);
	    }
	}
	else {
	    exit(-1);
	}
    }
}

/*--------------------------------------------------------------------------*/

SpWindow::SpWindow(const char	*windowName,
		   ArgList	argList,
		   Cardinal	argCount, 
		   soundplayer_t *sp) 
		 : VkWindow (windowName, argList, argCount)
{
    Arg args[10];
    int i, n, ncols;
    char *buttons[] = {"Rewind", "Pause", "Start"};
    
    _sp = sp;
    
    XtVaSetValues(_baseWidget,
	XmNallowShellResize,        True,
	XmNrecomputeSize,           True,
	XmNmwmDecorations,
	    MWM_DECOR_ALL|MWM_DECOR_RESIZEH|MWM_DECOR_MAXIMIZE,
	XmNmwmFunctions,
	    MWM_FUNC_ALL|MWM_FUNC_RESIZE|MWM_FUNC_MAXIMIZE|MWM_FUNC_QUIT_APP,
	NULL );
    
    _form = XtVaCreateManagedWidget("form", 
				    sgSpringBoxWidgetClass, _mainWindowWidget,
				    XmNorientation, XmVERTICAL, 
				    NULL);
    
    /* if we have a drop pocket, we want 4 columns of widgets, not 3 */
    if (sp->nopocket) {
	ncols = 3;
    }
    else {
	ncols = 4;
    }
    n = 0;
    
    _rowcol = XtVaCreateManagedWidget("rowcol", xmRowColumnWidgetClass, _form,
			    //XmNorientation, XmHORIZONTAL, 
			    XmNpacking, XmPACK_COLUMN, 
			    XmNnumColumns, ncols, 
			    XmNleftAttachment, XmATTACH_FORM, 
			    XmNtopAttachment, XmATTACH_FORM, 
			    XmNrightAttachment, XmATTACH_FORM, 
			    XmNentryAlignment, XmALIGNMENT_CENTER, 
			    XmNadjustLast, False, 
			    NULL);
	
    if (!sp->nopocket) {
	XmString xstr;

        /* add a drop pocket */
	n = 0;
	if (sp->filename) {
	    xstr = XmStringCreate(sp->filename, XmFONTLIST_DEFAULT_TAG);
	    XtSetArg(args[n], SgNname, xstr); n++;
	}
	pocket = XtCreateManagedWidget("filePocket", sgDropPocketWidgetClass,
	    _rowcol, args, n);
	XtAddCallback(pocket, SgNiconUpdateCallback,
	    (XtCallbackProc) ::OpenFileCallback, sp);
	if (sp->filename) {
	    XmStringFree(xstr);
	}
    }
    
    vkButton1 = new TransportButton("Rewind", _rowcol, rewindPixmap);
    vkButton2 = new TransportButton("Stop", _rowcol, stopPixmap, TRUE);
    vkButton3 = new TransportButton("Play", _rowcol, playPixmap, TRUE);

    XtAddCallback(vkButton1->baseWidget(), XmNactivateCallback, 
	(XtCallbackProc)::RewindCallback, sp);
    XtAddCallback(vkButton2->baseWidget(), XmNactivateCallback, 
	(XtCallbackProc)::PauseCallback, sp);
    XtAddCallback(vkButton3->baseWidget(), XmNactivateCallback, 
	(XtCallbackProc)::PlayCallback, sp);

    vkButton1->show();
    vkButton2->show();
    vkButton3->show();

    n = 0;
    XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
    XtSetArg(args[n], XmNrightSpring, 0); n++;
    XtSetArg(args[n], XmNleftSpring, 0); n++;
    XtSetArg(args[n], XmNhorizontalSpring, 50); n++;
    
    XtSetArg(args[n], XmNtopWidget, _rowcol); n++;
    progIndicator = XtCreateManagedWidget("prog", xmScaleWidgetClass,
	_form, args, n);

    XtAddCallback(progIndicator, XmNvalueChangedCallback,
	    (XtCallbackProc)::PositionCallback, sp);
    XtAddCallback(progIndicator, XmNdragCallback,
	    (XtCallbackProc)::DragCallback, sp);

    /*
     * Audio level control widgets
     */
    n = 0;
    XtSetArg(args[n], XmNrightSpring, 0); n++;
    XtSetArg(args[n], XmNleftSpring, 0); n++;
    XtSetArg(args[n], XmNhorizontalSpring, 50); n++;
    audioFrame = XmCreateFrame(_form, "audioControl", args, n);
    
    n = 0;
    XtSetArg(args[n], XmNchildType, XmFRAME_TITLE_CHILD); n++;
    Widget _audioLabel = XmCreateLabelGadget(audioFrame, "audioLabel", 
	args, n);
    XtManageChild(_audioLabel);
    
    // Create a Spring Box for the audio control widgets
    n = 0;
    Widget _audioSpringBox = XtCreateManagedWidget("audioSBox",
    	sgSpringBoxWidgetClass, audioFrame, args, n);
    	
    n = 0;
    XtSetArg(args[n], XmNcolumns, 4); n++;
    _levelText = XtCreateManagedWidget("levelEntry",
	xmTextFieldWidgetClass, _audioSpringBox, args, n);
    XmTextFieldSetString(_levelText, "0");
    XtAddCallback(_levelText, XmNactivateCallback,
	    &SpWindow::LevelEntryCallback, this);
    XtAddCallback(_levelText, XmNmodifyVerifyCallback, 
	    &SpWindow::TextNumericalVerifyCB, this);	    

    n = 0;
    XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
    XtSetArg(args[n], XmNminimum, SCALE_MIN_DB); n++;
    XtSetArg(args[n], XmNmaximum, SCALE_MAX_DB); n++;
    XtSetArg(args[n], XmNvalue, SCALE_NORM_DB); n++;
    _levelWheel = XtCreateManagedWidget("levelControl", 
	sgThumbWheelWidgetClass, _audioSpringBox, args, n);
    XtAddCallback(_levelWheel, XmNdragCallback,
	    &SpWindow::LevelWheelCallback, this);
    XtAddCallback(_levelWheel, XmNvalueChangedCallback,
	    &SpWindow::LevelWheelCallback, this);
    
    /*
     * MIDI tempo control widgets
     */
    n = 0; 
    XtSetArg(args[n], XmNrightSpring, 0); n++;
    XtSetArg(args[n], XmNleftSpring, 0); n++;
    XtSetArg(args[n], XmNhorizontalSpring, 50); n++;
    midiFrame = XmCreateFrame(_form, "midiControl", args, n);
    
    n = 0;
    XtSetArg(args[n], XmNchildType, XmFRAME_TITLE_CHILD); n++;
    Widget _midiLabel = XmCreateLabelGadget(midiFrame, "midiLabel", 
	args, n);
    XtManageChild(_midiLabel);
    
    // Create a Spring Box for the midi control widgets
    n = 0;
    Widget _midiSpringBox = XtCreateManagedWidget("midiSBox",
    	sgSpringBoxWidgetClass, midiFrame, args, n);
    	
    /*
     * Create the text field and the thumbwheel to control the
     * midi tempo.
     */
    n = 0;
    XtSetArg(args[n], XmNcolumns, 4); n++;
    _tempoText = XtCreateManagedWidget("tempoEntry",
	xmTextFieldWidgetClass, _midiSpringBox, args, n);
    XmTextFieldSetString(_tempoText, "100");
    XtAddCallback(_tempoText, XmNactivateCallback,
	    &SpWindow::TempoEntryCallback, this);
    XtAddCallback(_tempoText, XmNmodifyVerifyCallback, 
	    &SpWindow::TextNumericalVerifyCB, this);
  
    n = 0;
    XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
    XtSetArg(args[n], XmNminimum, TEMPO_MIN_PERCENT); n++;
    XtSetArg(args[n], XmNmaximum, TEMPO_MAX_PERCENT); n++;
    _tempoWheel = XtCreateManagedWidget("tempoControl", 
	sgThumbWheelWidgetClass, _midiSpringBox, args, n);
    XtAddCallback(_tempoWheel, XmNdragCallback,
	    &SpWindow::TempoWheelCallback, this);
    XtAddCallback(_tempoWheel, XmNvalueChangedCallback,
	    &SpWindow::TempoWheelCallback, this);
    

    VkSubMenu  *pane  = this->addMenuPane("fileMenu");
    VkSubMenu  *cntl  = this->addMenuPane("optionsMenu");
    
    pane->addAction("fileCloneItem", (XtCallbackProc)::CloneFileCallback, sp);
    pane->addAction("fileOpenItem", (XtCallbackProc)::OpenFileCallback, sp);
    pane->addAction("fileSaveAsItem", (XtCallbackProc)::SaveFileCallback, sp);
    pane->addSeparator();
    pane->addAction("fileQuitItem", (XtCallbackProc)::QuitCallback, sp);
    
    playControlToggle = cntl->addToggle("playbackControlItem", 
				    (XtCallbackProc)::PlayCtrlCallback, sp);
    cntl->addAction("utilControlPanelItem", 
				    (XtCallbackProc)::ApanelCallback, sp);
    if (sp->midiReady) {
	cntl->addAction("midiControlPanelItem", SynthPanelCallback, sp);
	cntl->addSeparator();
	mididevmenu = cntl->addRadioSubmenu("midiDevice");
	for (i = 0; i < sp->midiPortCount; i++) {
	    mididevmenu->addToggle(sp->midiNames[i], 
		MIDIDeviceCallback, sp);
	}
	char *defaultDev = _mdGetDefault(MD_GET_OUTPUT);
	int pos = mididevmenu->getItemPosition(defaultDev);
	sp->midiDevice = pos;
    }
}

/*--------------------------------------------------------------------------*/

SpWindow::~SpWindow()
{
    
}

/*--------------------------------------------------------------------------*/

void
SpWindow::handleWmQuitMessage()
{
    dprintf("handleWmQuitMessage\n");
    QuitApp(_sp);
}

/*--------------------------------------------------------------------------*/

void
SpWindow::handleWmDeleteMessage()
{
    dprintf("handleWmDeleteMessage\n");
    QuitApp(_sp);
}

/*--------------------------------------------------------------------------*/

void
SpWindow::TempoWheelCallback(Widget w, XtPointer clientData, XtPointer
	callData)
{
    int value;
    SgThumbWheelCallbackStruct *cb = (SgThumbWheelCallbackStruct*)callData;
    
    value = ((SpWindow*)clientData)->HandleTempoWheel(w);
    
    ((SpWindow*)clientData)->_sp->midiTempoScale = value;
    SendAudioCommand("Tempo");
}

/*--------------------------------------------------------------------------*/

int
SpWindow::HandleTempoWheel(Widget w)
{
    int value;
    char str[50];   // should be more than enough....
    
    XtVaGetValues(w, XmNvalue, &value, NULL);
    
    sprintf(str, "%d", value);
    str[3] = '\0';    // terminate with extreme prejudice
    XmTextFieldSetString(_tempoText, str);
    
    return value;
}

/*--------------------------------------------------------------------------*/

void
SpWindow::TempoEntryCallback(Widget w, XtPointer clientData, XtPointer
	callData)
{
    int resetText = 0;
    
    char *str = XmTextFieldGetString(w);
    int val = atoi(str);
    
    if (val < TEMPO_MIN_PERCENT) {
	val = TEMPO_MIN_PERCENT;
	resetText = 1;
    }
    else if (val > TEMPO_MAX_PERCENT) {
	val = TEMPO_MAX_PERCENT;
	resetText = 1;
    }
    
    ((SpWindow*)clientData)->_sp->midiTempoScale = val;
    SendAudioCommand("Tempo");
    
    XtVaSetValues(((SpWindow*)clientData)->_tempoWheel, XmNvalue, val, NULL);
	    
    if (resetText) {
	char string[10];
	
	sprintf(string, "%d", ((SpWindow*)clientData)->_sp->midiTempoScale);
	string[3] = '\0';
	XmTextFieldSetString(((SpWindow*)clientData)->_tempoText, string);
    }
    
    XtFree(str);
}

/*--------------------------------------------------------------------------*/

void
SpWindow::SetTempoValue(int tempo)
{
    char str[10];
    
    sprintf(str, "%d", tempo);
    str[3] = '\0';
    XmTextFieldSetString(_tempoText, str);
    
    XtVaSetValues(_tempoWheel, XmNvalue, tempo, SgNhomePosition, tempo, NULL);
}

/*--------------------------------------------------------------------------*/

void
SpWindow::LevelWheelCallback(Widget w, XtPointer clientData, XtPointer
	callData)
{
    int value;
    SgThumbWheelCallbackStruct *cb = (SgThumbWheelCallbackStruct*)callData;
    
    value = ((SpWindow*)clientData)->HandleLevelWheel(w);
    
    ((SpWindow*)clientData)->_sp->audioScaleDB = value;
    ((SpWindow*)clientData)->_sp->audioScale = 
	(float)apDecibelToLinear(value, 1.0f);
    dprintf("%f\n", ((SpWindow*)clientData)->_sp->audioScale);
    
}

/*--------------------------------------------------------------------------*/

int
SpWindow::HandleLevelWheel(Widget w)
{
    int value;
    char str[50];   // should be more than enough....
    
    XtVaGetValues(w, XmNvalue, &value, NULL);
    
    sprintf(str, "%d", value);
    str[3] = '\0';    // terminate with extreme prejudice
    XmTextFieldSetString(_levelText, str);
    
    return value;
}
/*--------------------------------------------------------------------------*/

void
SpWindow::LevelEntryCallback(Widget w, XtPointer clientData, XtPointer
	callData)
{
    int resetText = 0;
    
    char *str = XmTextFieldGetString(w);
    
    int val = atoi(str);
    
    dprintf("SpWindow::LevelEntryCallback, value = %d\n", val);
    
    if (val < SCALE_MIN_DB) {
	val = SCALE_MIN_DB;
	resetText = 1;
    }
    else if (val > SCALE_MAX_DB) {
	val = SCALE_MAX_DB;
	resetText = 1;
    }
    
    ((SpWindow*)clientData)->_sp->audioScaleDB = val;
    ((SpWindow*)clientData)->_sp->audioScale =
					(float)apDecibelToLinear(val, 1.0f);
    XtVaSetValues(((SpWindow*)clientData)->_levelWheel, 
	XmNvalue, val, NULL);
	    
    if (resetText) {
	char string[10];
	
	sprintf(string, "%d", ((SpWindow*)clientData)->_sp->audioScaleDB);
	string[3] = '\0';
	XmTextFieldSetString(((SpWindow*)clientData)->_levelText, string);
    }
    
    XtFree(str);
}

/*--------------------------------------------------------------------------*/

void
SpWindow::TextNumericalVerifyCB(Widget, XtPointer clientData, 
							XtPointer callData)
{
    XmTextVerifyCallbackStruct *cb = (XmTextVerifyCallbackStruct *) callData;

    if (cb && cb->text && cb->text->ptr)
	cb->doit = ((SpWindow*)clientData)->IsNumerical(cb->text->ptr);
}

/*--------------------------------------------------------------------------*/

void
SpWindow::TextPositiveNumericalVerifyCB(Widget, XtPointer clientData, 
							XtPointer callData)
{
    XmTextVerifyCallbackStruct *cb = (XmTextVerifyCallbackStruct *)callData;

    if (cb && cb->text && cb->text->ptr)
	cb->doit = ((SpWindow*)clientData)->IsPositiveNumerical(cb->text->ptr);	
}

/*--------------------------------------------------------------------------*/
Boolean
SpWindow::IsNumerical(char *s)
{
    if (!s)
	return (False);

    int foundNumerical = False;
    for (int i = 0; s[i] != '\0'; i++)
    {
	if (s[i] == ' ' || s[i] == '-' || s[i] == '+' || s[i] == '.')
	    foundNumerical = True;
	else if (s[i] >= '0' && s[i] <= '9')
	    foundNumerical = True;
	else
	    return (False);
    }
    return (foundNumerical);
}
/*--------------------------------------------------------------------------*/
Boolean
SpWindow::IsPositiveNumerical(char *s)
{
    if (!s)
	return (False);

    Boolean foundNumerical = False;
    for (int i = 0; s[i] != '\0'; i++)
    {
	if (s[i] == ' ' || s[i] == '.' || s[i] == '+')
	    foundNumerical = True;
	else if (s[i] >= '0' && s[i] <= '9')
	    foundNumerical = True;
	else
	    return (False);
    }
    return (foundNumerical);
}
/*--------------------------------------------------------------------------*/

static int
ParseArgs(int argc, char **argv, soundplayer_t *sp)
/*
 * Foist off the command line parsing to the appropriate routine
 */
{
    int ret;
    
    if (!strcmp(progName, "soundplayer")) {
	ret = ParseSoundPlayerArgs(argc, argv, sp);	
    }
    else if (!strcmp(progName, "sfplay")) {
	ret = ParseSfplayArgs(argc, argv, sp);
    }
    return ret;
}

/*--------------------------------------------------------------------------*/

static int
ParseSoundPlayerArgs(int argc, char **argv, soundplayer_t *sp)
/*
 * ParseArgs parses the command line arguments and sets flags, etc. in
 * the soundplayer_t structure accordingly.
 *
 * Items set by ParseArgs:
 *	nodisplay	: global variable
 *	noauto		: field of soundplayer_t	NOT IMPLEMENTED YET
 *	nofork		: field of soundplayer_t
 *	filename	: field of soundplayer_t
 *	nowplaying	: field of soundplayer_t
 *
 * return value:
 *	0 = success
 *     -1 = failure
 */
{
    int i, ret = 0;
    char *str;

    for (i = 1; i < argc; i++) {                /* skip argv[0] */
    	str = argv[i];
	dprintf("argv[%d] = %s\n", i, str);
	if (!strncmp("-nodisplay", str, 4)) {
	    nodisplay = 1;
	}
	else if (!strncmp("-nofork", str, 4)) {
	    sp->nofork = 1;
	}
	else if (!strncmp("-noauto", str, 4)) {
	    sp->noauto = 1;
	}
	else if (!strncmp("-nopocket", str, 4)) {
	    sp->nopocket = 1;
	}
	else if (!strncmp("-nounique", str, 4)) {
	    sp->nounique = 1;
	}
	else if ('-' == str[0]) {		/* illegal argument */
	    Usage();
	    ret = -1;
	}
	else {	/* grab the file name */
	    if (i == argc - 1) {
		sp->filename = strdup(str);
		if (!sp->noauto)
		    sp->nowplaying = PLAY;
		else
		    sp->nowplaying = PAUSE;
	    }
	    else {
	    }	    
	}
    }
    if (nodisplay && sp->filename == 0) {
    	/*
    	 * this is a somewhat silly way in which to use the program,
    	 * but i'm not convinced we should alert the user (that is,
    	 * spew some stuff out to a window).
    	 */
    }
    return ret;
}

/*--------------------------------------------------------------------------*/

static int
ParseSfplayArgs(int argc, char **argv, soundplayer_t *sp)
{
    return 0;
}

/*--------------------------------------------------------------------------*/
static void
Usage(void)
{
    fprintf(stderr, "Usage: %s", usageString);
}

/*--------------------------------------------------------------------------*/

#define MIDI_LIBRARY	"libmd.so"

static int
LoadMIDILibrary(void)
/*
 * return val = 0 for no error
 *		positive number for error
 */
{
    void *hdl;
    int  err = 0;
    
    /* open the MIDI library DSO */
    hdl = dlopen(MIDI_LIBRARY, RTLD_LAZY);
    if (hdl == (void*)0) {
	err++;
	return err;
    }
    /* get the pointers to the MIDI library symbols */
    _mdInit	    = (int(*)(void))dlsym(hdl, "mdInit");
    _mdGetName	    = (char*(*)(int))dlsym(hdl, "mdGetName");
    _mdSend	    = (int(*)(MDport, MDevent*, int))dlsym(hdl, "mdSend");
    _mdPause	    = (long long(*)(MDport))dlsym(hdl, "mdPause");
    _mdTell	    = (long long(*)(MDport))dlsym(hdl, "mdTell");
    _mdTellNow	    = (long long(*)(MDport))dlsym(hdl, "mdTellNow");
    _mdPanic	    = (int(*)(MDport))dlsym(hdl, "mdPanic");
    _mdWakeup	    = (int(*)(MDport))dlsym(hdl, "__mdWakeup");
    _mdClosePort    = (int(*)(MDport))dlsym(hdl, "mdClosePort");
    _mdSetStampMode = (int(*)(MDport, int))dlsym(hdl, "mdSetStampMode");
    _mdGetStatus    = (int(*)(char*))dlsym(hdl, "mdGetStatus");
    _mdSetDivision  = (int(*)(MDport, int))dlsym(hdl, "mdSetDivision");
    _mdSetTempo	    = (int(*)(MDport, int))dlsym(hdl, "mdSetTempo");
    _mdOpenOutPort  = (MDport(*)(char*))dlsym(hdl, "mdOpenOutPort");
    _mdGetFd	    = (int(*)(MDport))dlsym(hdl, "mdGetFd");
    _mdSetOrigin    = (int(*)(MDport, long long))dlsym(hdl, "mdSetOrigin");
    _mdGetOrigin    = (long long(*)(MDport))dlsym(hdl, "mdGetOrigin");
    _mdSetTemposcale    = (int(*)(MDport, double))dlsym(hdl,
							"mdSetTemposcale");
    _mdSetStartPoint    = (void(*)(MDport, long long, long long))dlsym(hdl,
							"mdSetStartPoint");
    _mdGetDefault   = (char*(*)(int))dlsym(hdl, "mdGetDefault");
    return err;
}

/*--------------------------------------------------------------------------*/

// utility functions

static int
IdentifyMPEGBitstream(int fd)
/*
 * return values:   0 = not an MPEG bitstream
 *		    1 = MPEG audio bitstream
 */
{
    int w;
    
    // read a word (32 bits) from the file
    read(fd, &w, sizeof(int));
    if (((w & 0xfff80000) == 0xfff80000) &&
	((w & 0x00060000) != 0) &&
	((w & 0x0000f000) != 0x0000f000) &&
	((w & 0x00000c00) != 0x00000c00) &&
	((w & 0x3) != 2)) 
	return 1;   // it's an MPEG file
    else
	return 0;   // not an MPEG bitstream
}

/*--------------------------------------------------------------------------*/

static int
IdentifyMIDIFile(int fd)
/*
 * return values:   0 = not a MIDI file
 *		    1 = MIDI file
 */
{
    char buf[4];
    int  nbytes;
    
    lseek(fd, 0, SEEK_SET);
    if ((nbytes = read(fd, buf, 4)) != 4) return 0;
    else {
	if (!strncmp(buf, "MThd", 4)) return 1;
	else return 0;
    }
}

/*--------------------------------------------------------------------------*/

static int
IdentifyFileType(char *filename)
{
    int fd;
    int t, filetype;
	int supported = 0;
    AFfilehandle hdl = NULL;
    
    filetype = UNIDENTIFIED;
    
    fd = open(filename, O_RDONLY);
    if (fd > 0) {
	t = afIdentifyNamedFD(fd, filename, &supported);
	if (t > 0 && supported && (hdl = afOpenFile(filename, "r", 0)) != NULL) {
	    filetype = AUDIOFILE;
	    afCloseFile(hdl);
	}
	else {	/* assume it is an MPEG bitstream or MIDI file */
	    if (IdentifyMPEGBitstream(fd)) {
		filetype = MPEGBSTREAM;
	    }
	    else if (IdentifyMIDIFile(fd)) {
		filetype = MIDIFILE;
	    }
	}
    }
    else {	/* couldn't open the file */
	filetype = BADFILE;
    }
    if (fd > 0) close(fd);
    dprintf("IdentifyFileType: filetype = %d\n", filetype);
    return filetype;
}
/*--------------------------------------------------------------------------*/

static char*
GetStringFromXmString(XmString string)
{
    XmStringContext     context;
    char                *text;
    XmStringCharSet     charset;
    XmStringDirection   dir;
    Boolean             separator;
    
    XmStringInitContext(&context, string);
    XmStringGetNextSegment(context, &text, &charset, &dir, &separator);
    XmStringFreeContext(context);

    return text;
}


/*--------------------------------------------------------------------------*/

static void
QuitApp(soundplayer_t *sp)
{
    if (sp) {
        if (sp->pid != 0) {
            int stat;

            SendAudioCommand("Exit");
            wait(&stat);
            sp->pid = 0;
        }
    }
    theApplication->quitYourself();
}

/*--------------------------------------------------------------------------*/

static void
FreeResources(soundplayer_t *sp)
{
    if (sp->midiPort != 0 && sp->midiLibrary) {
	
	_mdPause(sp->midiPort);
	PanicMIDI(sp->midiPort);
	
	_mdClosePort(sp->midiPort);
	sp->midiPort = 0;
    }
    if (sp->midiFile != 0) {
	sp->midiFile = 0;
    }
    /* Audio resources */
    if (sp->config != 0) {
	alFreeConfig(sp->config);
	sp->config = 0;
    }
    if (sp->port != 0) {
	alClosePort(sp->port);
	sp->port = 0;
    }
    
    /* Audio file */
    if (sp->fileHdl != 0) {
	afCloseFile(sp->fileHdl);
	sp->fileHdl = 0;
    }
    
    /* MPEG bitstream file */
    if (sp->fp != 0) {
	fclose(sp->fp);
	sp->fp = 0;
    }
}

/*--------------------------------------------------------------------------*/

static void
UpdateFilenameDisplay(soundplayer_t* sp)
{
    Arg args[6];
    int n;
    XmString xstr;
    char *str;
    str = "";

    /*
     * update the drop pocket...if we already have a filename use it,
     * otherwise, we use the null name
     */
    if (!sp->nopocket) {
    	if (sp->filename) {
	    str = sp->filename;
    	} 
    	n = 0;
    	xstr = XmStringCreate(str, XmFONTLIST_DEFAULT_TAG);
    	XtSetArg(args[n], SgNname, xstr); n++;
    	XtSetValues(pocket, args, n);
    	XmStringFree(xstr);
    }
    /*
     * update the window name; remember `win' is a global
     */
    if (sp->filename) {
	win->setTitle(basename(sp->filename));
    } else {
    	win->setTitle(appTitle);
    }
}

/*--------------------------------------------------------------------------*/

static int
HandleNewFile(char *filename, soundplayer_t* sp)
{
    int filetype;
    int retval = 0;
    
    assert(filename != NULL);
    
    dprintf("HandleNewFile: %s\n", filename);

    if (filename) {
	filetype = IdentifyFileType(filename);
	if (filetype == AUDIOFILE || 
	    filetype == MPEGBSTREAM ||
	    filetype == MIDIFILE) {
	    char *tmp;
	    
	    if (filetype == MIDIFILE && !sp->midiLibrary) {
		theErrorDialog->setTitle( _intl_ErrorPostNotOpenSelFile );
		theErrorDialog->post( _intl_NoMIDILib );

		UpdateFilenameDisplay(sp);

		return retval;
	    } else if (filetype == MIDIFILE && !sp->midiReady) {
		theErrorDialog->setTitle( _intl_ErrorPostNotOpenSelFile );
		theErrorDialog->post( _intl_NoMIDIDev );

		UpdateFilenameDisplay(sp);

		return retval;
	    }
	    if (XtIsManaged(midiFrame)) {
		XtUnmanageChild(midiFrame);
		playControlToggle->setVisualState(FALSE);
	    } else if (XtIsManaged(audioFrame)) {
		XtUnmanageChild(audioFrame);
		playControlToggle->setVisualState(FALSE);
	    }
	    sp->info.filetype = filetype;
	    tmp = sp->filename;
	    sp->filename = strdup(filename);
	    free(tmp);
	    
	    UpdateFilenameDisplay(sp);
	    
	    XmScaleSetValue(progIndicator, 0);
	    
	    dprintf("audio process pid = %d\n", sp->pid);
	    if (sp->pid != 0) {
		int stat;

		setreuid(geteuid(), getuid());
		setregid(getegid(), getgid());
		//kill(sp->pid, SIGTERM);
		SendAudioCommand("Exit");
		/* restore old state: ruid = root; euid = user */
		setreuid(geteuid(), getuid());
		setregid(getegid(), getgid());
		dprintf("waiting for process pid=%d to die\n", sp->pid);
		wait(&stat);
		dprintf("back from wait call\n");
		sp->pid = 0;
		audioproc = 0;
		sp->cuePosition = 0;
	    }
	    sp->pid = sproc(AudioProcess, PR_SALL & ~PR_SID, sp);
	    if (sp->pid == -1) {    /* error in sproc */
		printf("bad sproc\n");
	    } else {
		audioproc = 1;
	    }
	    /*
	     * block in the uspsema function until the playback
	     * process is ready.
	     */
	    uspsema(startSema);	    
	    retval = 1;
	}
	else if (filetype == UNIDENTIFIED) {

	    free(filename);
	    theErrorDialog->setTitle( _intl_ErrorTitleOFReadError );
            theErrorDialog->post( _intl_ErrorPostCRUnsupportFormat );
	    
	    UpdateFilenameDisplay(sp);

	} else {

	    free(filename);
	    theErrorDialog->setTitle( _intl_ErrorTitleOFReadError );
            theErrorDialog->post( _intl_ErrorPostNotOpenSelFile );

	    UpdateFilenameDisplay(sp);
	    
	}
    }
    return retval;
}

/*--------------------------------------------------------------------------*/

void
UpdateCallback(VkCallbackObject *comp, void *clientData, void*)
{
    VkRunOnce *obj = (VkRunOnce*)comp;
    int i, nargs;
    char *str;
    soundplayer_t *sp = (soundplayer_t*)clientData;
    
    pid_t pid;
    pid = getpid();
    
    nargs = obj->numArgs();
    
    if (nargs > 0) {
	for (i = 0; i < nargs; i++) {
	    str = obj->arg(i);
	    if ('-' == str[0]) {	/* an option...blow it off for now */
		//Usage();
	    }
	    else {  /* assume this arg is the filename */
		dprintf("filename = %s\n", str);
		sp->nowplaying = PLAY;
		if (HandleNewFile(str, sp)) {
		    vkButton2->deselect();
		    vkButton3->select();
		    timerID = XtAppAddTimeOut(app, TIMER_INTERVAL, 
			    				TimerCallback, sp);
		}
		else {
		    sp->nowplaying = PAUSE;
		    vkButton2->select();
		    vkButton3->deselect();
		}
	    }
	    dprintf("pid = %d: %s\n", pid, obj->arg(i));
	}
    }
}

/*--------------------------------------------------------------------------*/

void
QuitCallback(Widget, XtPointer clientData, XtPointer)
{   
    soundplayer_t *sp = (soundplayer_t*)clientData;
    
    QuitApp(sp);
}

/*--------------------------------------------------------------------------*/

void
SaveFileCallback(Widget, XtPointer clientData, XtPointer)
{   
    soundplayer_t *sp;
    char *filename;
    struct stat s0, s1;
    int err0, err1, err, noerror = 0;

    sp = (soundplayer_t*)clientData;

    err1 = stat(sp->filename, &s1);
    if (err1 == -1) {	/* error in stat call */
	err = oserror();
	switch (err) {
	    case ENOENT:
	    default:
		/* put up a dialog indicating an error */
		theErrorDialog->setTitle(_intl_ErrorTitleSFAError);
                theErrorDialog->post( _intl_ErrorPostNoFileOpen );
		break;
	}
	return;
    }
	
    // File browser
    theFileSelectionDialog->setTitle( _intl_SDTitleSaveFileAs );
    if (theFileSelectionDialog->postAndWait() == VkDialogManager::OK) {
	filename = strdup((char*)theFileSelectionDialog->fileName());

	err0 = stat(filename, &s0);
	if (err0 == -1) {	/* error in stat call */
	    err = oserror();
	    switch (err) {
		case ENOENT:	/* file doesn't exist--that's fine */
		    noerror = 1;
		    break;
		default:
		    /* put up a dialog indicating an error */
		    theErrorDialog->setTitle(_intl_ErrorTitleSFAError);
                    theErrorDialog->post(_intl_ErrorPostNotWriteSelFile);
		    break;
	    }
	}
	
	if (err0 == 0 && err1 == 0) {
	    /* are the files the same? */
	    if (s0.st_ino == s1.st_ino && s0.st_dev == s1.st_dev) {
		/* file are identical */
		theErrorDialog->setTitle(_intl_ErrorTitleSFAError);
                theErrorDialog->post( _intl_ErrorPostFISaveNotPerformed );
	    }
	    else {
		noerror = 1;	/* go ahead and do the link */
	    }
	}
	if (noerror) {	/* git to it man! */
	    FILE *p;
	    char retstr[256], errstr[256];
	    char cmdstr[256];
	    int  retval = 0, exitval;
	    
	    strcpy(retstr,  _intl_UnSaveFile );
	    
	    sprintf(cmdstr, "/bin/cp %s %s 2>&1; exit $?", sp->filename,
		    filename);

	    if ((p = popen(cmdstr, "r")) != NULL) {
		while (fgets(errstr, 256, p) != NULL) {
		    strcat(retstr, errstr);
		    strcat(retstr, " ");
		}
		retval = pclose(p);
		exitval = WEXITSTATUS(retval);
		dprintf("retval = 0x%x exitval = 0x%x\n", retval, exitval);
	    }
	    if (retval != 0) {	/* error */
		theErrorDialog->setTitle(_intl_ErrorTitleSFAError);
                theErrorDialog->post(retstr);
	    }
	}
        free(filename);
    }
}

/*--------------------------------------------------------------------------*/

void
CloneFileCallback(Widget, XtPointer clientData, XtPointer)
{
    soundplayer_t *sp = (soundplayer_t*)clientData;
    char retstr[256];
    char cmdstr[256];
    int  retval = 0, exitval;
	    
    strcpy(retstr,  _intl_UnCloneWindow );
    
    if (sp->filename)
	sprintf(cmdstr, "/usr/sbin/soundplayer -noauto -nounique %s",
	    sp->filename);
    else
	sprintf(cmdstr, "/usr/sbin/soundplayer -nounique");
    
    retval = system(cmdstr);
    exitval = WEXITSTATUS(retval);
    
    if (retval != 0) {	/* error */
	theErrorDialog->setTitle("Clone Window: Error");
	theErrorDialog->post(retstr);
    }
}

/*--------------------------------------------------------------------------*/
void
OpenFileCallback(Widget, XtPointer clientData, XtPointer callData)
{   
    soundplayer_t *sp;
    char *filename;
    XmAnyCallbackStruct *any;
    SgDropPocketCallbackStruct *cbData;
    int needFileSelection = 1;
    
    filename = 0x0;
    sp = (soundplayer_t*)clientData;
    any = (XmAnyCallbackStruct*) callData;
    
    if (any->reason == SgCR_ICON_CHANGE) {
	needFileSelection = 0;
    }
    
    if (needFileSelection) {	/* open selected */
	theFileSelectionDialog->setTitle( _intl_SDTitleOpenFile );
        if (theFileSelectionDialog->postAndWait() == VkDialogManager::OK) {
	    filename = strdup((char*)theFileSelectionDialog->fileName());
	}
    }
    else {	/* drop pocket */
	cbData = (SgDropPocketCallbackStruct*)callData;
	filename = GetStringFromXmString(cbData->iconName);
    }
    HandleNewFile(filename, sp);
}

/*--------------------------------------------------------------------------*/
void
ApanelCallback(Widget, XtPointer, XtPointer)
{   
    if (fork() == 0)	// child
	execl("/usr/sbin/audiopanel", "audiopanel", NULL);
}

/*--------------------------------------------------------------------------*/
void
PlayCtrlCallback(Widget, XtPointer clientData, XtPointer callData)
{   
    soundplayer_t *sp = (soundplayer_t*)clientData;
    XmToggleButtonCallbackStruct *cbs = 
				   (XmToggleButtonCallbackStruct *) callData;
    if (cbs->set) {
	if (sp->info.filetype == MIDIFILE) {
	    XtManageChild(midiFrame);
	} else {
	    XtManageChild(audioFrame);
	}
    } else {
	if (sp->info.filetype == MIDIFILE) {
	    XtUnmanageChild(midiFrame);
	} else {
	    XtUnmanageChild(audioFrame);
	}
    }
}

/*--------------------------------------------------------------------------*/

void
SynthPanelCallback(Widget, XtPointer, XtPointer)
{   
    system("/usr/sbin/synthpanel");
}

/*--------------------------------------------------------------------------*/
void
MIDIDeviceCallback(Widget w, XtPointer clientData, XtPointer callData)
{   
    int pos;
    soundplayer_t *sp = (soundplayer_t*)clientData;
    XmToggleButtonCallbackStruct *cbs = 
	(XmToggleButtonCallbackStruct *) callData;
    int currentDevice;
    
    if (cbs->set) {
	currentDevice = sp->midiDevice;
	pos = mididevmenu->getItemPosition(w);
	if (pos == currentDevice) return;
	dprintf("item position = %d; name: %s; set = %d\n",
	    pos, sp->midiNames[pos], cbs->set);
	sp->midiDevice = pos;   
	if (audioproc) {
	    SendAudioCommand("Change Device");
	}
    }
}

/*--------------------------------------------------------------------------*/
void
DragCallback(Widget w, XtPointer clientData, XtPointer)
{   
    if (timerID) {
	XtRemoveTimeOut(timerID);
	timerID = 0;
    }
    dprintf("drag callback\n");
}

/*--------------------------------------------------------------------------*/
void
PositionCallback(Widget w, XtPointer clientData, XtPointer)
{   
    soundplayer_t *sp = (soundplayer_t*)clientData;
    int val;
    
    vkapp->busy();
    XmScaleGetValue(w, &val);
    sp->cuePosition = val;
    if (audioproc) {
	SendAudioCommand("Cue");
	timerID = XtAppAddTimeOut(app, TIMER_INTERVAL, TimerCallback, sp);
    }
    vkapp->notBusy();
    dprintf("playback position changed: %d\n", val);
}

/*--------------------------------------------------------------------------*/

void
AudioProcessErrorProc(XtPointer, int*, XtInputId*)
{
    theErrorDialog->setTitle("Playback Error");
    theErrorDialog->post(errMessage[errNumber]);
    
    uspsema(ErrSema);			/* ready for another message */
}

/*--------------------------------------------------------------------------*/
void
MessageProc(XtPointer clientData, int*, XtInputId*)
{   
    dprintf("MessageProc: %s\n", message);
    
    SpWindow *w = (SpWindow*)clientData;
    soundplayer_t *sp = w->_sp;
    
    /*
     * Handle the new information we have from the running audio
     * process.
     */
    if (!strcmp(message, "Update MIDI Info")) {
	/*
	 * Audio process successfully opened a new MIDI file and
	 * has placed new information in the soundplayer struct.
	 */
    }
    else if (!strcmp(message, "End")) {
	vkButton3->deselect();
	vkButton2->select();
	if (timerID) {
	    XtRemoveTimeOut(timerID);
	    timerID = 0;
	}
	sp->cuePosition = 0;
	XmScaleSetValue(progIndicator, 0);
    }
    uspsema(UISema);			/* fire when ready.... */
}

/*--------------------------------------------------------------------------*/
void
RewindCallback(Widget, XtPointer client_data, XtPointer)
{
    soundplayer_t *sp = (soundplayer_t*)client_data;

    sp->cuePosition = 0;
    XmScaleSetValue(progIndicator, 0);
    if (audioproc) {
        SendAudioCommand("Cue");
    }
}
/*--------------------------------------------------------------------------*/
void
PauseCallback(Widget, XtPointer client_data, XtPointer)
{
    /*
     * Send message to playback thread
     */
    if (audioproc) {
	SendAudioCommand("Pause");
    }
    
    vkButton3->deselect();
    vkButton2->select();
    if (timerID) {
	XtRemoveTimeOut(timerID);
	timerID = 0;
    }
}
/*--------------------------------------------------------------------------*/
void
PlayCallback(Widget, XtPointer client_data, XtPointer)
{
    soundplayer_t *sp = (soundplayer_t*)client_data;
    /*
     * Send message to playback thread
     */
    if (audioproc) {
	SendAudioCommand("Start");
    }
    
    if (audioproc) {
	vkButton3->select();
	vkButton2->deselect();
    }
    else {
	vkButton3->deselect();
	vkButton2->select();
    }
    /*
     * Start the timer process for the progress bar....
     */
    if (!timerID)
	timerID = XtAppAddTimeOut(app, TIMER_INTERVAL, TimerCallback,
							    (XtPointer)sp);
}

/*--------------------------------------------------------------------------*/

void
TimerCallback(XtPointer clientdata, XtIntervalId *)
{
    int percent;
    soundplayer_t *sp;

    sp = (soundplayer_t *)clientdata;
    
    timerID = XtAppAddTimeOut(app, TIMER_INTERVAL, TimerCallback, 
							    (XtPointer)sp);
    if (sp->oldplayed != sp->nplayed) {
	if (sp->nplayed == sp->ntotal)
	    percent = 100;
	else
	    percent = (sp->nplayed * 100)/sp->ntotal;
	
	dprintf("sp->nplayed = %d;  sp->ntotal = %d\n", sp->nplayed, sp->ntotal);
	dprintf("widget = 0x%x; percent = %d\n", progIndicator, percent);
	assert(percent <= 100);
	if (percent >= 0 && percent <= 100)
	    XmScaleSetValue(progIndicator, percent);
	    
	sp->oldplayed = sp->nplayed;
    }
}

/*--------------------------------------------------------------------------*/

static void
InitializeSemaphores(soundplayer_t *sp)
{
    usptr_t *arena;
    
    usconfig(CONF_ARENATYPE, US_SHAREDONLY);
    arena = usinit(tmpnam(0));
    SendSema = usnewpollsema(arena, 0);
    RcvSema  = usnewsema(arena, 0);
    ErrSema  = usnewpollsema(arena, 0);
    sp->errfd = usopenpollsema(ErrSema, 0777);
    
    UISema  = usnewpollsema(arena, 0);
    sp->ui_fd = usopenpollsema(UISema, 0777);
    
    startSema = usnewsema(arena, 0);
    
    //prctl(PR_SETEXITSIG, (void *)signum);
    prctl(PR_SETEXITSIG, 0);
    sp->PollFD[0].fd	    = usopenpollsema(SendSema, 0777);
    sp->PollFD[0].events    = POLLIN;
}

/*--------------------------------------------------------------------------*/

static void
SendAudioCommand(char *cmd)
{
    /* Send the command */
    command = (char *)cmd;
    usvsema(SendSema);
    /* Wait for response */
    uspsema(RcvSema);
}

/*--------------------------------------------------------------------------*/

static void
SendError(int errno)
{
    errNumber = errno;
    if (!nodisplay)
	usvsema(ErrSema);
    else
	fprintf(stderr, "%s: %s.\n", progName, errMessage[errNumber]);
}

/*--------------------------------------------------------------------------*/

static void
SendMessage(char *cmd)
{
    /* Send the message */
    message = (char *)cmd;
    if (!nodisplay)
	usvsema(UISema);
}

/*--------------------------------------------------------------------------*/

int sigcaught;

static void
CatchSignals(int)
{
	sigcaught++;
}

/*--------------------------------------------------------------------------*/

/*
 * the audio handling process.
 */
static void
AudioProcess(void *v)
{
    soundplayer_t* sp = (soundplayer_t*)v;
    int status;
    signed long long nanos;
    
    assert(0 != sp);
    assert(sp->info.filetype != UNIDENTIFIED &&
    	   sp->info.filetype != BADFILE);

    /* 
     * make it so this process accepts signals
     */
    sigset(SIGHUP,  CatchSignals);
    sigset(SIGTERM, CatchSignals);
    sigset(SIGINT,  CatchSignals);
    sigset(SIGQUIT, CatchSignals);
    sigset(SIGBUS,  CatchSignals);
    sigset(SIGSEGV, CatchSignals);
    prctl(PR_TERMCHILD);
    
    /*
     * swap the real and effective user ID's again...this time we
     * will utilize the root priviledges to establish non-degrading
     * priority for the audio playback.
     */
    setreuid(geteuid(), getuid());
    setregid(getegid(), getgid());
    prctl(PR_RESIDENT);		    // XXX no error check
    if (schedctl(NDPRI, 0, NDPHIMIN) < 0) {
	dprintf("error with schedctl: euid = %d\n", geteuid());
    }
    /* back the old state: ruid = root; euid = user */
    setreuid(geteuid(), getuid());
    setregid(getegid(), getgid());
    
    dprintf("AudioProcess: filetype = %d\n", sp->info.filetype);
    
    if (sp->info.filetype == MIDIFILE) {
	/* open MIDI file */
	OpenMIDIFile(sp);
	
	/* open MIDI port */
	if (OpenMIDIPort(sp) != 0) {
	    SendError(ERR_NOMIDI);
	}
	else {
	    SendMessage("Update MIDI Info");
	
	    /*
	     * Set the origin of the midi port
	     */
	    {
	    long long now;
	    syssgi(SGI_GET_UST,(unsigned long long*)&now,0);
	    _mdSetStartPoint(sp->midiPort, now, sp->midiFile->tell());
	    }

	    /* reset controllers and send MIDI program changes */
	    ResetAllControllers(sp->midiPort);
	    SendProgramChanges(sp);
	}
    }
    else {

	OpenAudioFile(sp);
	
	/*
	 * if we are expected to play the file straight off, then we
	 * will need an audio port. otherwise, we can just wait for the
	 * `play' command from the UI.
	 */
	if (sp->nowplaying == PLAY) {
	
	    status = OpenAudioPort(sp);
	    if (status == -1) {
		dprintf("failed to open audio port\n");
		SendError(ERR_NOPORTS);
	    }
	    dprintf("after OpenAudioPort: sp->port = 0x%x\n", sp->port);
	}
    }
    
    uspsema(SendSema);	    /* put ourselves on the poll queue */
    usvsema(startSema);	    /* release the UI...it can send messages */
    
    if (sp->info.filetype == AUDIOFILE ||
	sp->info.filetype == MPEGBSTREAM) {
	while(sp->nowplaying != DONE) {
	    if (sp->port) {
		/* now deal with sample frames */
		alSetFillPoint(sp->port, sp->queueSize - sp->lowwatermark);
		poll(sp->PollFD, 2, -1);
	    }
	    else {
		poll(sp->PollFD, 1, -1);
	    }
	    if (sigcaught) {	/* caught a signal */
		    kill(getppid(), SIGTERM);
		    exit(-1);
	    }
	    if (sp->PollFD[0].revents & POLLIN) {
		ProcessAudioCommand(sp);
		uspsema(SendSema);
		usvsema(RcvSema);
	    }
	    if (sp->PollFD[1].revents & POLLOUT) {
		GetMoreAudioSamples(sp);
	    }
	}
    }
    else if (sp->info.filetype == MIDIFILE) {
	while (sp->nowplaying != DONE) {
	    if (sp->nowplaying == PLAY) {
		if (sp->midiPort) {
		    poll(sp->PollFD, 2, -1);
		}
		else {
		    poll(sp->PollFD, 1, -1);
		}
		if (sigcaught) {	/* caught a signal */
		    if (sp->midiPort) {
			_mdPause(sp->midiPort);
			PanicMIDI(sp->midiPort);
		    }
		    kill(getppid(), SIGTERM);
		    exit(-1);
		}
		if (sp->PollFD[0].revents & POLLIN) {
		    ProcessMIDICommand(sp);
		    uspsema(SendSema);
		    usvsema(RcvSema);
		}
		if (sp->PollFD[1].revents & POLLOUT) {
		    GetMoreMIDIMessages(sp);
		}
	    }
	    else {
		poll(sp->PollFD, 1, -1);
		if (sigcaught) {	/* caught a signal */
		    if (sp->midiPort) {
			_mdPause(sp->midiPort);
			PanicMIDI(sp->midiPort);
		    }
		    kill(getppid(), SIGTERM);
		    exit(-1);
		}
		if (sp->PollFD[0].revents & POLLIN) {
		    ProcessMIDICommand(sp);
		    uspsema(SendSema);
		    usvsema(RcvSema);
		}
	    }
	}
    }
}

/*--------------------------------------------------------------------------*/

static int 
OpenAudioPort(soundplayer_t *sp)
{
    ALpv	pv[4];
    int		rate_i;
    double 	rate_d;

    dprintf("OpenAudioPort: enter\n");
    pv[0].param = AL_RATE;
    alGetParams(AL_DEFAULT_OUTPUT, pv, 1);
    rate_i = (int)alFixedToDouble(pv[0].value.ll);
    if (rate_i != sp->info.rate) {
	rate_d = (double)(sp->info.rate);
	pv[0].value.ll = alDoubleToFixed(rate_d);
	alSetParams(AL_DEFAULT_OUTPUT, pv, 1);
    }

    sp->config = alNewConfig();
    alSetChannels(sp->config, sp->info.nchannels);
    switch (sp->info.bitspersamp) {
	case 8:
	    alSetWidth(sp->config, AL_SAMPLE_8);
	    break;
	case 16:
	    alSetWidth(sp->config, AL_SAMPLE_16);
	    break;
	case 24:
	    alSetWidth(sp->config, AL_SAMPLE_24);
	    break;
	default:
	    break;
    }
    if (sp->port) {
	alClosePort(sp->port);
	sp->port = 0;
    }
    sp->port = alOpenPort("Sound Player", "w", sp->config);
    if (sp->port != 0) {
    	sp->queueSize = alGetQueueSize(sp->config);	/* qsize in frames */
	sp->PollFD[1].fd     = alGetFD(sp->port);
	sp->PollFD[1].events = POLLOUT;
	alFreeConfig(sp->config);
    	return 0;
    }
    else {
	alFreeConfig(sp->config);
	dprintf("alOpenPort failed\n");
	return -1;
    }
}

/*--------------------------------------------------------------------------*/

static int
ReadHeader(FILE *fp, mpegheader_t *header_info, int *eof)
{
    unsigned char header_buf[4];
    int offset = 0;
    
    offset = ftell(fp);

    if (fread(header_buf, 1, 4, fp) != 4) {
        *eof++;
        return(OKAY);  /* ignore trailing garbage */
    }

    /* check for syncword */
    if ((header_buf[0] != 0xff) || ((header_buf[1] & 0xf0) != 0xf0)) {
        return(BAD);
    }

    /* syncword found, unpack following 20 bits of header */

    /* ID */
    if ((header_buf[1] & 0x08) != 0x08) {
        return(BAD);
    }

    /* Layer */
    switch ((header_buf[1] & 0x05) >> 1) {
        case 3: header_info->layer = 1; break;
        case 2: header_info->layer = 2; break;
        case 1: header_info->layer = 3; break;
        default:  
        case 0:
                return(BAD);
    }

    /* Protection Bit */
    header_info->error_protection = (header_buf[1] & 0x01);

    /* Bitrate Index */
    if (header_info->layer == 1) {
        switch ((header_buf[2] & 0xf0) >> 4) {
            case 0: header_info->bitrate = 0; /* free format */ break;
            case 1: header_info->bitrate = 32000; break;
            case 2: header_info->bitrate = 64000; break;
            case 3: header_info->bitrate = 96000; break;
            case 4: header_info->bitrate = 128000; break;
            case 5: header_info->bitrate = 160000; break;
            case 6: header_info->bitrate = 192000; break;
            case 7: header_info->bitrate = 224000; break;
            case 8: header_info->bitrate = 256000; break;
            case 9: header_info->bitrate = 288000; break;
            case 10: header_info->bitrate = 320000; break;
            case 11: header_info->bitrate = 352000; break;
            case 12: header_info->bitrate = 384000; break;
            case 13: header_info->bitrate = 416000; break;
            case 14: header_info->bitrate = 448000; break;
            case 15: header_info->bitrate = -1; break; /* this is an error */
        }
    }
    else if (header_info->layer == 2) {
        switch ((header_buf[2] & 0xf0) >> 4) {
            case 0: header_info->bitrate = 0; /* free format */ break;
            case 1: header_info->bitrate = 32000; break;
            case 2: header_info->bitrate = 48000; break;
            case 3: header_info->bitrate = 56000; break;
            case 4: header_info->bitrate = 64000; break;
            case 5: header_info->bitrate = 80000; break;
            case 6: header_info->bitrate = 96000; break;
            case 7: header_info->bitrate = 112000; break;
            case 8: header_info->bitrate = 128000; break;
            case 9: header_info->bitrate = 160000; break;
            case 10: header_info->bitrate = 192000; break;
            case 11: header_info->bitrate = 224000; break;
            case 12: header_info->bitrate = 256000; break;
            case 13: header_info->bitrate = 320000; break;
            case 14: header_info->bitrate = 384000; break;
            case 15: header_info->bitrate = -1; break; /* this is an error */
        }
    }
    else /* header_info->layer == 3 */ {
        switch ((header_buf[2] & 0xf0) >> 4) {
            case 0: header_info->bitrate = 0; /* free format */ break;
            case 1: header_info->bitrate = 32000; break;
            case 2: header_info->bitrate = 40000; break;
            case 3: header_info->bitrate = 48000; break;
            case 4: header_info->bitrate = 56000; break;
            case 5: header_info->bitrate = 64000; break;
            case 6: header_info->bitrate = 80000; break;
            case 7: header_info->bitrate = 96000; break;
            case 8: header_info->bitrate = 112000; break;
            case 9: header_info->bitrate = 128000; break;
            case 10: header_info->bitrate = 160000; break;
            case 11: header_info->bitrate = 192000; break;
            case 12: header_info->bitrate = 224000; break;
            case 13: header_info->bitrate = 256000; break;
            case 14: header_info->bitrate = 320000; break;
            case 15: header_info->bitrate = -1; break; /* this is an error */
        }
    }
    if (header_info->bitrate < 0) {
        return(BAD);
    }
 
    /* Sampling Frequency */
    switch ((header_buf[2] & 0x0c) >> 2) {
        case 0: header_info->sample_rate = 44100; break;
        case 1: header_info->sample_rate = 48000; break;
        case 2: header_info->sample_rate = 32000; break;
        default:
        case 3: /* reserved */
            return(BAD);
    }

    /* Padding Bit */
    header_info->padding = ((header_buf[2] & 0x02) > 1);

    /* Private Bit: ignored */

    /* Mode */
    switch ((header_buf[3] & 0xc0) >> 6) {
        default:
        case 0: header_info->mode = MODE_STEREO; break;
        case 1: header_info->mode = MODE_JOINT_STEREO; break;
        case 2: header_info->mode = MODE_DUAL_CHANNEL; break;
        case 3: header_info->mode = MODE_SINGLE_CHANNEL; break;
    }

    /* Mode Extension */
    header_info->mode_extension = ((header_buf[3] & 0x3) >> 4);

    /* Copyright */
    header_info->copyright = ((header_buf[3] & 0x08) >> 3);

    /* Original/Home */
    header_info->original = ((header_buf[3] & 0x04) >> 2);

    /* Emphasis */
    switch (header_buf[3] & 0x03) {
       default:
       case 0: header_info->emphasis = EMPHASIS_NONE; break;
       case 1: header_info->emphasis = EMPHASIS_50_15_USEC; break;
       case 3: header_info->emphasis = EMPHASIS_CCITT_J17; break;
       case 2: 
            return(BAD);
    }
    return(OKAY);
}

/*--------------------------------------------------------------------------*/

static int
TestAudioPort(soundplayer_t *sp)
{
    ALconfig config;
    ALport   port = 0;
    int	     retval = 0;
    long     chan;
    
    config = alNewConfig();
    
    alSetChannels(config, sp->info.nchannels);
    
    switch (sp->info.bitspersamp) {
	case 8:
	    alSetWidth(config, AL_SAMPLE_8);
	    break;
	case 16:
	    alSetWidth(config, AL_SAMPLE_16);
	    break;
	case 24:
	    alSetWidth(config, AL_SAMPLE_24);
	    break;
	default:
	    break;
    }
    
    port = alOpenPort("Sound Player", "w", config);
    alFreeConfig(config);
    
    // some debug stuff
    config = alGetConfig(port);
    chan   = alGetChannels(config);
    dprintf("TestAudioPort: num channels = %ld\n", chan);
    
    if (port == 0) {    // defeat
	dprintf("alOpenPort failed\n");
	retval = -1;
    }
    else {		    // sweet victory!
	alClosePort(port);
    }
    
    if (chan != sp->info.nchannels) {	// didn't get the right config
	retval = -1;
    }
    return retval;
}

/*--------------------------------------------------------------------------*/
static int 
OpenAudioFile(soundplayer_t *sp)
{
    int nframes;
    int firstTry = 1;
    int result;

    sp->fileHdl = afOpenFile(sp->filename, "r", 0);
    assert(sp->fileHdl != 0);
    
    afSetVirtualSampleFormat(sp->fileHdl, AF_DEFAULT_TRACK,
						    AF_SAMPFMT_TWOSCOMP, 16);
    afSetVirtualByteOrder(sp->fileHdl, AF_DEFAULT_TRACK,
						    AF_BYTEORDER_BIGENDIAN);

    sp->info.rate		= afGetRate(sp->fileHdl, AF_DEFAULT_TRACK);
    sp->info.bitspersamp	= 16;
    
    sp->info.nchannels = afGetChannels(sp->fileHdl, AF_DEFAULT_TRACK);
    if (sp->info.nchannels == -1) {	    // error condition
	sp->info.nchannels = 2;
	afSetVirtualChannels(sp->fileHdl, AF_DEFAULT_TRACK,
						    sp->info.nchannels);
    }
    
    nframes = afGetFrameCount(sp->fileHdl, AF_DEFAULT_TRACK);
    dprintf("OpenAudioFile: nframes = %d = 0x%x\n", nframes, nframes);
    
    sp->nplayed = 0;
    sp->ntotal  = nframes;
    
    sp->highwatermark = sp->info.rate;		// one second in frames
    sp->lowwatermark = sp->highwatermark/2;	// half a second

    // Try to get the properly configured port up front...if we can't
    // then send an error to the user interface.
    do {
	result = TestAudioPort(sp);
	if (result == -1) {
	    dprintf("OpenAudioFile: backing off on nchannels....\n");
	    sp->info.nchannels = 2;
	    afSetVirtualChannels(sp->fileHdl, AF_DEFAULT_TRACK,
						    sp->info.nchannels);
	    firstTry = 0;
	}
    } while (result == -1 && firstTry); 
  
    return(0);
}

/*--------------------------------------------------------------------------*/

static int 
OpenMIDIFile(soundplayer_t *sp)
{
    int howmany;
    
    sp->midiFile = new MFfile;

    sp->midiFile->load(sp->filename);
    sp->midiFile->rewind();

    // We want to set the tempo from the beginning, hence we will just
    // search for the first tempo event. We look on track 0 for the tempo.
    // If it is either a type 0 or 1 file, the tempo will still be on track
    // 0 in either case. We don't care about restoring the current seek
    // pointer in the track
    sp->midiFileTempo = 500000;
    MFtrack *track = sp->midiFile->getTrack(0);
    MFfileEvent *fev = track->seekMetaEvent(MIDImeta_SetTempo);
    if (fev != NULL) {
	MFmeta *meta = fev->metaEvent();
	sp->midiFileTempo = (meta->msg[0] << 16) + (meta->msg[1] << 8) + 
								meta->msg[2];
    }

    sp->nplayed = 0;
    sp->ntotal = 0;

    sp->lastStamp = 0;
    
    int notracks = sp->midiFile->numberTracks();
    for (int i = 0; i < notracks; i++ ) {
	MFtrack *track;
	track = sp->midiFile->getTrack(i);
	
	dprintf("track[%d]: last time stamp = %llu\n", i, 
	    track->lastTimeStamp(&howmany));
	
	if (track->lastTimeStamp(&howmany) > sp->lastStamp)
	    sp->lastStamp = track->lastTimeStamp(&howmany);
    }
    
    dprintf("last time stamp = %llu = 0x%llx\n", sp->lastStamp, sp->lastStamp);
    sp->ntotal = 100;
    
    dprintf("total=%d\n", sp->ntotal);
    sp->midiFile->rewind();
    sp->lastPause = 0;
    
    return(0);
}

/*--------------------------------------------------------------------------*/

static void
SendProgramChanges(soundplayer_t* sp)
{
    int	    i;
    signed char state[128];
    int     prog;
    
    if (sp->midiPort == 0) return;
    
    bzero(state, 128);
    
    dprintf("SendProgramChanges: sending program changes\n");
    sp->midiFile->rewind();
    _mdSetStampMode(sp->midiPort, MD_NOSTAMP);
    int notracks = sp->midiFile->numberTracks();
    for (i = 0; i < notracks; i++) {
      MFtrack *track = sp->midiFile->getTrack(i);
      MFfileEvent *fev;
      track->seekBeginning();
      while ((fev = track->nextMidiEvent(MD_PROGRAMCHANGE)) != NULL) {
	_mdSend(sp->midiPort, fev->midiEvent(), 1);
      }
    }

    _mdSetStampMode(sp->midiPort, MD_RELATIVETICKS);
    _mdSetDivision(sp->midiPort, sp->midiFile->division());
    sp->midiFile->rewind();
    dprintf("SendProgramChanges: rewinding midi file\n");
}

/*--------------------------------------------------------------------------*/

static int 
OpenMIDIPort(soundplayer_t *sp)
/*
 * Open a MIDI port on the device indicated by sp->midiDevice which is
 * just an index into the table of names.
 * 
 * Note: This function must be called after a MIDI file has been opened.
 * Otherwise, there is no way of know the MIDI division and tempo to use
 * for the port.
 */
{
#if NOTYET
    int fileFlags;
#endif
    if (sp->midiReady == 0) {
	// MIDI is not ready
	return -1;
    }
    dprintf("opening midi port %s\n", sp->midiNames[sp->midiDevice]);
    sp->midiPort = _mdOpenOutPort(sp->midiNames[sp->midiDevice]);
    if (sp->midiPort == 0)
	return -1;

    sp->PollFD[1].fd = _mdGetFd(sp->midiPort);
    sp->PollFD[1].events = POLLOUT;
    
    _mdSetStampMode(sp->midiPort, MD_RELATIVETICKS);
    _mdSetDivision(sp->midiPort, sp->midiFile->division());
    dprintf("setting tempo of midi port to %d\n", sp->midiFileTempo);
    _mdSetTempo(sp->midiPort, sp->midiFileTempo);
    _mdSetTemposcale(sp->midiPort, (double)sp->midiTempoScale*0.01f);

    return 0;
}

/*--------------------------------------------------------------------------*/
static void
ExitAudioProcess(soundplayer_t *sp)
{
    dprintf("Exiting audio process; pid = %d\n", getpid());

    /* 
     * respond to the UI thread. notice that there is no uspsema associated
     * with the SendSema, since we do not want the UI to send another msg.
     * after we have exited the audio thread.
     */
    usvsema(RcvSema);
    FreeResources(sp);
    
    exit(-1); 
}

/*--------------------------------------------------------------------------*/
static void
ProcessAudioCommand(soundplayer_t *sp)
{
    long pvbuf[2];
    
    if (!strcmp(command, "Start"))  {
	int seekLoc;
	int status;

	if (sp->nowplaying == PLAY) return;
	
	seekLoc = (sp->cuePosition * sp->ntotal) / 100;
	sp->nplayed = seekLoc;
	afSeekFrame(sp->fileHdl, AF_DEFAULT_TRACK, seekLoc);
	dprintf("ProcessAudioCommand: start.\n");
	dprintf("seek loc = %d, sp->nplayed = %d, cue pos = %d\n", 
	    seekLoc, sp->nplayed, sp->cuePosition);
	status = OpenAudioPort(sp);
	sp->nowplaying = PLAY;
    }
    else if (!strcmp(command, "Pause")) {

	if (sp->nowplaying == PAUSE) return;

	if (sp->port) {
	    int filled = alGetFilled(sp->port);	    // in frames
	    sp->cuePosition = (sp->nplayed - filled)*100/sp->ntotal;
	    dprintf("closing...%d frames remain; nplayed = %d; cue pos = %d; \n",
		filled, sp->nplayed, sp->cuePosition);
	    alClosePort(sp->port);
	    sp->port = 0;
	}
        sp->nowplaying = PAUSE;
    }
    else if (!strcmp(command, "Stop")) {
	if (sp->port) {
	    alClosePort(sp->port);
	    sp->port = 0;
	}
        sp->nowplaying = STOP;
	afSeekFrame(sp->fileHdl, AF_DEFAULT_TRACK, 0);
    }
    else if (!strcmp(command, "Cue")) {
	int seekLoc;
	
	seekLoc = (sp->cuePosition * sp->ntotal) / 100;
	dprintf("cuePosition = %d seekLoc = %d\n", sp->cuePosition, 
	    seekLoc);
	    
	if (sp->nowplaying == PLAY) {
	    sp->nowplaying = STOP;
	    afSeekFrame(sp->fileHdl, AF_DEFAULT_TRACK, seekLoc);
	    sp->nplayed = seekLoc;
	    sp->nowplaying = PLAY;
	}
	else if (sp->nowplaying == PAUSE) {
	    afSeekFrame(sp->fileHdl, AF_DEFAULT_TRACK, seekLoc);
	    sp->nplayed = seekLoc;
	}
    }
    else if (!strcmp(command, "Done")) {
	sp->nowplaying = DONE;
    }
    else if (!strcmp(command, "Exit")) {
	ExitAudioProcess(sp);
    }
    
    dprintf("Exiting function ProcessAudioCommand\n");
}

/*--------------------------------------------------------------------------*/
#define MAX_MIDI_CHANNELS 16

static void
RestoreMIDIState(soundplayer_t *sp, long long tick)
/*
 * restore the MIDI state by sending all ``non-MIDI event''
 * messages up to the given time. this is the brute force
 * approach, but it should work wonders.
 */
{
    MDevent progChanges[MAX_MIDI_CHANNELS];
    int     active[MAX_MIDI_CHANNELS];
    
    MFfileEvent *eventBuf;
    int	    nevents;
    int	    done = 0;
    int	    status;
    int     chan, i;
    
    for (i = 0; i < MAX_MIDI_CHANNELS; i++) {
	active[i] = 0;
    }
    
    sp->midiFile->rewind();

    ResetAllControllers(sp->midiPort);
     
    while (!done) {
	
	eventBuf = sp->midiFile->nextEvent();
	
	if (eventBuf != NULL) {
	    MDevent *ev, event;
	    char sysex[6];

	    if (eventBuf->eventType() == MF_MIDI_EVENT) {
		
		ev = eventBuf->midiEvent();
		
		if (ev->stamp >= tick) {
		    goto _myend;
		}
		
		status = _mdGetStatus(ev->msg);
		
		switch (status) {
		    case MD_NOTEON:
		    case MD_NOTEOFF:
			/* do nothing for note on and note off */
			break;
		    case MD_PROGRAMCHANGE:
			chan = ev->msg[0] & 0x0f;			
			bcopy(ev, &progChanges[chan], sizeof(MDevent));
			progChanges[chan].stamp = 0;
			active[chan] = 1;
			break;
		    default:
			bcopy(ev, &event, sizeof(MDevent));
			event.stamp = 0;
			_mdSend(sp->midiPort, &event, 1);
			break;
		}
		
	    }
	} else {
	    done = 1;
	}
    }
_myend:
    for (i = 0; i < MAX_MIDI_CHANNELS; i++) {
	if (active[i]) {
	    dprintf("sending prog change on channel %d\n", i);
	    _mdSend(sp->midiPort, &progChanges[i], 1);
	}
    }
    sp->midiFile->seek(tick);

}

/*--------------------------------------------------------------------------*/

static void
ResetAllControllers(MDport port)
{
    int chan;
    MDevent ev;
    
    ev.msg[1] = MD_RESETALLCONTROLLERS;
    ev.msg[2] = 0;
    ev.stamp  = 0;
    
    for (chan = 0; chan < MAX_MIDI_CHANNELS; chan++) {
	ev.msg[0] = MD_CONTROLCHANGE | chan;
	
	_mdSend(port, &ev, 1);
    }
}

/*--------------------------------------------------------------------------*/

static void
AllNotesOff(MDport port)
{
    int chan;
    MDevent ev;
    
    ev.msg[1] = MD_ALLNOTESOFF;
    ev.msg[2] = 0;
    ev.stamp  = 0;
    
    for (chan = 0; chan < MAX_MIDI_CHANNELS; chan++) {
	ev.msg[0] = MD_CONTROLCHANGE | chan;
	
	_mdSend(port, &ev, 1);
    }
}

/*--------------------------------------------------------------------------*/

static void
PanicMIDI(MDport port)
{
    int chan;
    MDevent ev;
    
    ev.msg[1] = MD_ALLSOUNDOFF;
    ev.msg[2] = 0;
    ev.stamp  = 0;
    
    for (chan = 0; chan < MAX_MIDI_CHANNELS; chan++) {
	ev.msg[0] = MD_CONTROLCHANGE | chan;
	
	_mdSend(port, &ev, 1);
    }
    
    ev.msg[1] = MD_ALLNOTESOFF;
    
    for (chan = 0; chan < MAX_MIDI_CHANNELS; chan++) {
	ev.msg[0] = MD_CONTROLCHANGE | chan;
	
	_mdSend(port, &ev, 1);
    }
}

/*--------------------------------------------------------------------------*/

static int
GetPreviousTempo(MFfile *mf)
/*
 * look for tempo changes on track 0 only! XXX
 */
{
    int tempo = 500000;		// default value
    int bkwd = 1;
    long long stamp;

    MFtrack     *trk = mf->getTrack(0);
    MFfileEvent *fev = trk->prevMetaEvent(MIDImeta_SetTempo);

    if (fev != NULL) {		// got an event in bkwd direction

	
	MFmeta *mev = fev->metaEvent();
	stamp = mev->stamp;
	tempo = (mev->msg[0] << 16) + (mev->msg[1] << 8) + 
						    mev->msg[2];

    } else {			// go forward instead
        bkwd = 0;
	
    	fev = trk->seekMetaEvent(MIDImeta_SetTempo);
        if (fev != NULL) {
	    MFmeta *meta = fev->metaEvent();
	    stamp = meta->stamp;
	    tempo = (meta->msg[0] << 16) + (meta->msg[1] << 8) + 
								meta->msg[2];
        }
    }

    return tempo;

}

/*--------------------------------------------------------------------------*/
static void
ProcessMIDICommand(soundplayer_t *sp)
{
    long long seekLocation;
    
    if (!strcmp(command, "Start")) {
	int tempo;

	if (sp->nowplaying == PLAY) return;

	sp->midiFile->seek(sp->lastPause);
	
	_mdWakeup(sp->midiPort);
	
	RestoreMIDIState(sp, sp->lastPause);
	
	tempo = GetPreviousTempo(sp->midiFile);
	_mdSetTempo(sp->midiPort, tempo);
	
	{
	long long now;
	syssgi(SGI_GET_UST,(unsigned long long*)&now,0);
	_mdSetStartPoint(sp->midiPort, now, sp->lastPause);
	}

	sp->nowplaying = PLAY;

    } else if (!strcmp(command, "Pause")) {

	if (sp->nowplaying == PAUSE) {
	    PanicMIDI(sp->midiPort);
	    return;
	}

	sp->nowplaying = PAUSE;
	seekLocation = _mdPause(sp->midiPort);
	PanicMIDI(sp->midiPort);

	sp->lastPause = seekLocation;

    } else if (!strcmp(command, "Tempo")) {
	double tempo_scale;
	
	tempo_scale = (double)sp->midiTempoScale*0.01f;

	_mdSetTemposcale(sp->midiPort, tempo_scale);

    } else if (!strcmp(command, "Cue")) {
	int seekLoc, tempo;
	
	seekLoc = (sp->cuePosition * sp->lastStamp)/100;

	sp->nplayed = seekLoc;
	sp->lastPause = seekLoc;

	if (sp->nowplaying == PLAY) {
	    
	    sp->nowplaying = STOP;

	    _mdPause(sp->midiPort);
	    AllNotesOff(sp->midiPort);

	    sp->midiFile->seek(sp->lastPause);

	    RestoreMIDIState(sp, sp->lastPause);
	
	    tempo = GetPreviousTempo(sp->midiFile);
	    _mdSetTempo(sp->midiPort, tempo);

	    {
	    long long now;
	    syssgi(SGI_GET_UST,(unsigned long long*)&now,0);
	    _mdSetStartPoint(sp->midiPort, now, sp->lastPause);
	    }

	    sp->nowplaying = PLAY;
	    
	}
    } else if (!strcmp(command, "Change Device")) {
    
	int oldState = sp->nowplaying;
	
	sp->nowplaying = PAUSE;
	
	if (!sp->midiReady) return;
	
	if (sp->midiPort && sp->midiFile) {
	    int tempo;

	    sp->lastPause = _mdPause(sp->midiPort);
	    AllNotesOff(sp->midiPort);
	    
	    sp->midiFile->seek(sp->lastPause);

	    tempo = GetPreviousTempo(sp->midiFile);

	    _mdClosePort(sp->midiPort);
	    sp->midiPort = 0;
	    if (-1 == OpenMIDIPort(sp))
		printf("could not open MIDI port...\n");
	    
	    RestoreMIDIState(sp, sp->lastPause);
	
	    _mdSetTempo(sp->midiPort, tempo);
	    
	    {
	    long long now;
	    syssgi(SGI_GET_UST,(unsigned long long*)&now,0);
	    _mdSetStartPoint(sp->midiPort, now, sp->lastPause);
	    }
	    
	    sp->nowplaying = oldState;

	}
    } else if (!strcmp(command, "Done")) {

        sp->nowplaying = DONE;

    } else if (!strcmp(command, "Exit")) {

	ExitAudioProcess(sp);

    }
}

/*--------------------------------------------------------------------------*/
#define MIN(a,b) ((a)<(b)?(a):(b))

short sampbuf[2*100000];

static void
GetMoreAudioSamples(soundplayer_t *sp)
{
    int	    i,		/* counter */
	    nsamps,	/* number of samples to request from file */
	    nframes,	/* number of frames to request from file */
	    ngot,	/* number of frames received */
	    n, 		/* counter */
	    nchannels, 	/* number of samples per audio frame */
	    closeport;	/* flag */
    float   tmp;
    
    if (sp->port == 0) {
	dprintf("GetMoreAudioSamples: sp->port = 0x%x\n", sp->port);
	return;
    }
    
    nchannels = sp->info.nchannels;
    closeport = 0;
    
    nframes = sp->highwatermark - alGetFilled(sp->port);
    if (nframes > 0) {			/* we have room in the audio queue */
	if (sp->nowplaying == PLAY) {
	    /* 
	     * request either 1/2 second worth of frames, or whatever the
	     * audio queue will hold (up to the highwatermark)
	     */
	    ngot = afReadFrames(sp->fileHdl, AF_DEFAULT_TRACK, sampbuf, nframes);
	    dprintf("nframes=%d ngot=%d\n", nframes, ngot);
	    if (ngot < 0) ngot = 0;
	    sp->nplayed += ngot;
	    /* 
	     * didn't get all the sample frames we requested. two cases:
	     * 		a) end of file
	     * 		b) error in file
	     */
	    if (ngot < nframes) {	
		sp->nowplaying = STOP;
		closeport = 1;
	    }
	    /* scale audio data */
	    n = ngot*nchannels;
	    for (i = 0; i < n; i++) {
		tmp = sampbuf[i] * sp->audioScale;
		
		if (tmp > 32767.0f) sampbuf[i] = 32767;
		else if (tmp < -32768.0f) sampbuf[i] = -32768;
		else sampbuf[i] = tmp;
	    }
	}
	dprintf("calling alWriteFrames with value %d\n", n);
	alWriteFrames(sp->port, sampbuf, ngot);
	if (closeport) {
	    long val;

	    /* wait for the port to drain */
	    while (alGetFilled(sp->port)) sginap(50);

	    /* close port and reset its value */
	    alClosePort(sp->port);
	    sp->port = 0;

	    /* reset file to beginning */
	    sp->nplayed = 0;
	    val = afSeekFrame(sp->fileHdl, AF_DEFAULT_TRACK, 0);
	    dprintf("val after seek = %d\n", val);
	    val = afTellFrame(sp->fileHdl, AF_DEFAULT_TRACK);
	    dprintf("val after seek = %d\n", val);

	    /* end if nodisplay, otherwise alert UI that file is finished */
	    if (nodisplay) 
		sp->nowplaying = DONE;
	    else 
		SendMessage("End");
	}
    }
}

/*--------------------------------------------------------------------------*/

static void
GetMoreMIDIMessages(soundplayer_t *sp)
{
    MFfileEvent *eventBuf;
    int	    nevents;
    int	    status;
    
    if (sp->nowplaying != PLAY) return;
  
    eventBuf = sp->midiFile->nextEvent();

    if (eventBuf != NULL) {
	    MDevent *ev, event;
	    char sysex[6];
	    
	    if (eventBuf->eventType() == MF_MIDI_EVENT) {
		ev = eventBuf->midiEvent();
		
		status = _mdGetStatus(ev->msg);
		
		_mdSend(sp->midiPort, ev, 1);

	    }
	    else if (eventBuf->eventType() == MF_META_EVENT) {
		
		MFmeta *mev = eventBuf->metaEvent();
		
		if (mev->type == MIDImeta_SetTempo) {
		    sysex[0] = MD_META;
		    sysex[1] = 0x51;
		    sysex[2] = 0x03;
		    sysex[3] = mev->msg[0];
		    sysex[4] = mev->msg[1];
		    sysex[5] = mev->msg[2];
		    
		    event.msg[0] = MD_META;
		    event.sysexmsg = sysex;
		    event.stamp = mev->stamp;
		    event.msglen = 6;
		    
		    _mdSend(sp->midiPort, &event, 1);
		}
	    }
	    
	    sp->nplayed = _mdTellNow(sp->midiPort);
	    sp->nplayed = (sp->nplayed*100)/sp->lastStamp;
	  
    }
    else {
	
	sginap(2);
	
	sp->nplayed = _mdTellNow(sp->midiPort);
	sp->nplayed = (sp->nplayed*100)/sp->lastStamp;
	
	if (sp->nplayed > 100) sp->nplayed = 100;
	
	if (sp->nplayed == 100) {
	    
	    sp->nowplaying = PAUSE;
	    sp->nplayed = 0;
	    
	    sp->lastPause = 0;
	    
	    if (nodisplay) sp->nowplaying = DONE;
	    else SendMessage("End");
	}
    }
}

/*--------------------------------------------------------------------------*/

/* **********************************************************************
 * apDecibelToLinear  given number in decibels and a reference,
 *			return number in linear scale 
 *			(in range 0 .. infinity)
 * ********************************************************************** */
    double 
apDecibelToLinear(double number, double referenceValue)
/* number		number (in decibels) be converted to linear scale
  referenceValue   reference value (linear scale) for 'number' 
*/
{
/*
 *   level in dB = 20 * log10(level/reference level)
 *
 *    => 10^(level in dB/20) = level/reference level
 *    => level = reference level * 10^(level in dB / 20)
 */
return (referenceValue*pow(10, number*0.05));
}	/* ---- end apDecibelToLinear() ---- */

/* **********************************************************************
 * apLinearToDecibels  given number in linear scale,
 *			return number in decibel scale relative to reference
 * ********************************************************************** */
    double 
apLinearToDecibels(double number, double referenceValue)
/* number		'number' (in decibels) be converted to linear scale
		    'number' may NOT equal zero.
  referenceValue   reference value (linear scale) for 'number' 
		    'referenceValue' may NOT equal zero.

		    Signs for 'number' and 'referenceValue' must be like.
*/
{
/* check that signs for parameters are the same */
#ifdef DEBUG_DSPUTIL
if ((number < 0 && referenceValue > 0)||
    (number > 0 && referenceValue < 0))
    {
    dprintf("apLinearToDecibels()  signs for number %g and \
		reference value %g must be like.\n", number, referenceValue);
    /* not sure about what error value to return */
    return (0);
    }
#endif

/* check that reference value is not 0 */
if (referenceValue == 0)
    {
/* not sure about what error value to return */
    return (0);
    }
if (number == 0)
    {
/* not sure about what error value to return */
    return (0);
    }

/* level in dB = 20 * log10(level/reference level) */
return (20*log10(number/referenceValue));
}	/* ---- end apLinearToDecibels() ---- */
