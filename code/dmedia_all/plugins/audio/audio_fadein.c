/************************************************************************\
 *	File:		fadein.c					*
 *									*
 * 	Do a simple linear fade out on the selected area.  Note		*
 *	that a linear fade of amplitude != a linear fade of perceived	*
 *	intensity, since  amplitude = intensity^2.			*
 *									*
\************************************************************************/

#include <stdlib.h>
#include <dmedia/PremiereXAPI.h>

/* Define the latest API version this code is compatible with */
#define MY_API_VERSION      PRX_MAJOR_MINOR(1,0)

/* Define the version of this plugin */
#define MY_PLUGIN_VERSION   PRX_MAJOR_MINOR(1,0)

/* Audio Filter (PRX_TYPE_VIDEO_AUDIO) properties */

static PRX_KeyValueRec privProps[] = {
    /* Standard Properties */
    { PK_APIVersion,        (void *) MY_API_VERSION },
    { PK_PluginType,        (void *) PRX_TYPE_AUDIO_FILTER },
    { PK_PluginVersion,     (void *) MY_PLUGIN_VERSION },
    { PK_UniquePrefix,      (void *) "SGI" },
#if FADE_OUT
    { PK_Name,              (void *) "Fade Out" },
    { PK_DispatchSym,       (void *) "fadeOut" },
#else
    { PK_Name,		    (void *) "Fade In" },
    { PK_DispatchSym,	    (void *) "fadeIn" },
#endif
    { PK_EndOfList,         (void *) 0 }
};

static float	_cvt2float(unsigned char *);
static float	_cvt16Bit2float(unsigned char *);
static float	_cvtTwosComp2float(unsigned char *);
static float	_cvt16BitTwosComp2float(unsigned char *);

static void	_store(unsigned char *, float);
static void	_store16Bit(unsigned char *, float);
static void	_storeTwosComp(unsigned char *, float);
static void	_store16BitTwosComp(unsigned char *, float);


PRX_PropertyList
PRX_PluginProperties(unsigned apiVersion)
{
    if (MY_API_VERSION <= apiVersion) {

        /* It is always okay to return a property list based
           on an API version that is equal to or less than the
           apiVersion passed in. 
           
           For a plug-in with Motif UI elements, now would be a good
           time to initialize your app-defaults and read your widget
           data in.  For example, you could retrieve the PK_Name string
           from a resource default, allowing localization to occur
           without recompiling the plug-in.
           
           For this example, we just use a static array for properties,
           which is not recommended for serious commercial plug-ins. */

        return(privProps);

    } else {

        /* If you use any advanced API features, here would be a
           chance to downgrade your property list to use only
           the features supported by apiVersion.  Otherwise,
           just return NULL, which means your plug-in can't be
           used with this old apiVersion of Premiere. */

        return((PRX_PropertyList)NULL);

    }
}


/*
 * Process samples.  Note that we don't do anything special for stereo;
 * we just treat it as twice as many mono channels.  If we were doing 
 * some kind of filtering in which we had to maintain state per-channel,
 * though, we couldn't get away with this.
 */
int
fadeIn(int selector, PRX_AudioFilter prx)
{

    if (selector == fsExecute)
    {

	/*
	 * NOTE: prx can be NULL if selector == fsCleanup
	 */
	unsigned char 	*src = prx->src;
	unsigned char 	*dst = prx->dst;
	int 		sampleCount;
	int		totalNumberOfSamples;
	int		bytesPerSample;
	float		value;
	int		i;
	float 		(*cvt2float)(unsigned char *);
	void  		(*store)(unsigned char *, float);
	float 		slope, curVal;
	
	bytesPerSample       = (prx->sampleFormat & ga16Bit) ? 2 : 1;
	sampleCount          = prx->size / bytesPerSample;
	totalNumberOfSamples = prx->total / bytesPerSample;

#ifdef FADE_OUT
	  slope = -1.0f / (float) totalNumberOfSamples;
	  curVal = 1.0f + slope * prx->firstByte / bytesPerSample;
#else
	  slope =  1.0f / (float) totalNumberOfSamples;
	  curVal = 0.0f + slope * prx->firstByte / bytesPerSample;
#endif
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
	      store	= _store16BitTwosComp;
	      break;

	    default:
	      cvt2float = _cvt2float;
	      store	= _store;
	      break;
	  }

	  /* Iterate through all of the samples */
	  for (i = 0; i < sampleCount; i++) {
	      value = cvt2float(src);
	      src += bytesPerSample;
	      value *= curVal;
	      curVal += slope;
	      
	      store(dst, value);
	      dst += bytesPerSample;
	  }
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
