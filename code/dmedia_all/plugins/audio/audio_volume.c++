/************************************************************************\
 *	File:		volume.c++					*
 *									*
 *	Simple plug-in which increases or decreases the volume		*
 *	of by a given decibel value.  This shows how to bring up	*
 *	a simple dialog as well.					*
 *									*
\************************************************************************/

#include <stdio.h>
#include <math.h>
#include <dmedia/PremiereXAPI.h>
#include <dmedia/fx_dialog.h>


// Forward declarations
static int setup(PRX_AudioFilter prx);
static int execute(PRX_AudioFilter prx);

static float    _cvt2float(unsigned char *);
static float    _cvt16Bit2float(unsigned char *);
static float    _cvtTwosComp2float(unsigned char *);
static float    _cvt16BitTwosComp2float(unsigned char *);

static void     _store(unsigned char *, float);
static void     _store16Bit(unsigned char *, float);
static void     _storeTwosComp(unsigned char *, float);
static void     _store16BitTwosComp(unsigned char *, float);

static float AudioVolumeScale = 1.0;

/*
 * Define the PluginProperties function.  This function is used by
 * the plugin manager to determine what kind of plugin this is, as
 * well as its name and dispatchFunction.  Generally, a developer would
 */
extern "C" PRX_PropertyList PRX_PluginProperties(unsigned /*apiVersion*/)
{
    static PRX_KeyValueRec plugInProperties[] = {
	{ PK_APIVersion,        (void*) PRX_MAJOR_MINOR(1,0) },
        { PK_DispatchSym,       (void*) "dispatchFunction" },
        { PK_Name,              (void*) "Set Volume" },
        { PK_UniquePrefix,      (void*) "SGI" },
        { PK_PluginType,        (void*) PRX_TYPE_AUDIO_FILTER },
        { PK_PluginVersion,     (void*) PRX_MAJOR_MINOR(1,0) },
	{ PK_HasDialog,		(void*) True },
        { PK_EndOfList,         (void*) 0 },
    };

    return (PRX_PropertyList) &plugInProperties;
}


/*
 * The actual dispatchFunction.  The name of this function must
 * match whatever is in the PK_DispatchSym field of the PluginProperties
 * table.
 */

extern "C" int dispatchFunction(int selector, PRX_AudioFilter prx)
{
    switch (selector) {
      case fsCleanup:
	// Some plugins should define a cleanup routine to tear down any
	// state the plug-in uses
	return dcNoErr;

      case fsExecute:
	return execute(prx);

      case fsSetup:
	return setup(prx);

      default:
	return dcUnsupported;
    }
}


/*
 * Setup the plugin by prompting the user for a gain/attenuation amount.
 */
static int setup(PRX_AudioFilter)
{
    String defResources[] = {
	"Volume.decimalPoints: 2",
	"Volume.scaleWidth: 425",
	"Volume.titleString: Attenuation/Gain (in dB)",
	NULL
    };

    DMFXDialog *dialog = new DMFXDialog("volumeDialog", PRX_GetAppShell(),
				    defResources);

    DMFXDialogScale *scale = dialog->addScale("Volume", 0, -850, 850, 
				      XmHORIZONTAL, True);



    if (dialog->postAndWait() == 
	VkDialogManager::OK) {
	float number = (float) scale->getValue() / 100.0f;
	AudioVolumeScale = powf(10, number * 0.05);
	return dcNoErr;
    } else {
	// Indicates that the user pressed cancel to abort operation
	return dcOtherErr;
    }
}
	

/*
 * Execute the plugin
 */
static int execute(PRX_AudioFilter prx)
{
    unsigned char *src                  = prx->src;
    unsigned char *dst                  = prx->dst;
    int            bytesPerSample       = (prx->sampleFormat&ga16Bit) ? 2 : 1;
    int		   sampleCount          = prx->size / bytesPerSample;
    float          value;
    int            i;
    float          (*cvt2float)(unsigned char *);
    void           (*store)(unsigned char *, float);

    /* Figure out which converters to use */
    switch (prx->sampleFormat & (ga16Bit|gaTwosComp)) {
      case ga16Bit:
	cvt2float = _cvt16Bit2float;
	store     = _store16Bit;
	break;

      case gaTwosComp:
	cvt2float = _cvtTwosComp2float;
	store     = _storeTwosComp;
	break;

      case gaTwosComp|ga16Bit:
	cvt2float = _cvt16BitTwosComp2float;
	store     = _store16BitTwosComp;
	break;

      default:
	cvt2float = _cvt2float;
	store     = _store;
	break;
    }

    /* Iterate through all of the samples */
    for (i = 0; i < sampleCount; i++) {
	value = cvt2float(src);
	src += bytesPerSample;
	value *= AudioVolumeScale;
	store(dst, value);
	dst += bytesPerSample;
    }

    return dcNoErr;
}


/*
 * Handy utility routines.
 */

static float _cvt2float(unsigned char *ptr)
{
    float value = *ptr;
    return (value - 128.0f) / 128.0f;
}

static float _cvt16Bit2float(unsigned char *ptr)
{
    float value = *((unsigned short*) ptr);
    return (value - 32768.0f) / 32768.0f;
}


static float _cvtTwosComp2float(unsigned char *ptr)
{
    float value = *((signed char*) ptr);
    return ((value > 0.0) ? value/127.0 : value / 128.0);
}


static float _cvt16BitTwosComp2float(unsigned char *ptr)
{
    float value = *((signed short*) ptr);
    return ((value > 0.0) ? value/32767.0 : value / 32768.0);
    
}

/* Store as 8-bit unsigned */
static void _store(unsigned char *ptr, float value)
{
    *((unsigned char*) ptr) = (unsigned char) (127.50 * (value + 1.0));
}


/* Store as 16-bit unsigned */
static void _store16Bit(unsigned char *ptr, float value)
{
    *((unsigned short*) ptr) = (unsigned short) (32767.50 * (value + 1.0));
}


/* Store as 8-bit two's complement */
static void _storeTwosComp(unsigned char *ptr, float value)
{
    *((signed char*) ptr) = (signed char) ((value > 0.0) ? (value * 127.0) : (value * 128.0));
}


/* Store as 16-bit two's complement */
static void _store16BitTwosComp(unsigned char *ptr, float value)
{
    *((short*) ptr) = (short) ((value > 0.0) ? (value * 32767.0) : 
			       (value * 32768.0));
}
