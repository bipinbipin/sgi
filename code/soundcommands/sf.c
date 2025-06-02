/*****************************************************************************
 *
 *   Common routines for sound file tools
 *        
 * GetPathTail        get tail of file's path name
 * Empty        Return TRUE if string (usu.fileName) contains only 
 *            spaces and tabs
 * BasicMiscellaneous
 * BasicInstrument 
 * BasicTrack 
 * BasicSoundFile 
 * CloseSoundFile
 *
 * DataFormatName    Assemble string containing data format, 
 *            using short or long format
 * CompressionParameters  Assemble string containing  
 *                compression parameters
 * DurationInSeconds    Convert to format hours:minutes:seconds
 * SizeInBytes        Convert to bytes with metric system prefixes
 * FindCopyright    return ptr to copyright misc chunk.
 * FindFileFormatName    return  ptr to file format name
 * FindCompressionName    return  ptr to compression name
 * Numeric     
 * ReadChannelMatrixFile    read channel matrix file 
 * ParseKeywords         parse soundfile descriptor keywords, as seen 
 *                in sfconvert, etc.  Return # keywords found
 * OpenInSoundFile            open sound file, recognize format
 * ReadTrackParameters             Read track parameters
 * ReadTrackAIFFParameters         Read track AIFF parameters 
 * ReadInstrumentAIFFParameters        Read AIFF/C instrument chunk parameters
 * ReadMiscellaneousData        Read AIFF/C,NeXT,IRCAM miscellaneous chunk    
 * ReadSoundFileParameters        Read file header parameters and AIFF data

 * PrintSoundFileParameters Print contents of sound file headers
 * PrintFormats            Print supported formats
 * PrintCompressions        Print supported compression methods    
 * PrintTrackParameters    print common audio file parameters,
 *                universal among supported file formats    
 * PrintTrackAIFFParameters print AIFF and AIFF-C specific information     
 * PrintInstrumentAIFFParameters Print AIFF/C instrument chunk parameters
 * PrintMiscellaneousData   Print AIFF,AIFC,NeXT,IRCAM miscellaneous chunk    
 * MIDINoteNumberToMusicalNotation    convert MIDI Note to string
 *                    w/musical notation  
 * CopyTrackAudioData            Copy audio track data
 * CleanUpSoundFile
 * PrintError print    out error message
 * CatchAFError        error-reporting callback routine for audio file library
 * InitErrorReporting
 * OnSignalInterrupt
 * CheckAwareEncoderLicense
 * CheckAwareDecoderLicense
 *
 * WriteMiscellaneousData        Write AIFF,AIFC,NeXT,IRCAM miscellaneous data 
 * WriteInstrumentAIFFParameters    Write instrument parameters 
 * WriteTrackAIFFParameters        Write track AIFF/C parameters 
 * WriteSoundFileParameters        Write file header parameters  
 *
 *
 *        Written by Chris Pirazzi, Scott Porter, Gints Klimanis
 *                1991-
 *****************************************************************************/

#include <audioutil.h>
#include "sf.h"

char *applicationName;
long lastAFError = -1;
long lastALError = -1;
char reportAFError = FALSE;
char sfPrint = TRUE;

static char spaces[] = "   ";

char MIDINoteNumberToMusicalNotation(char midiNoteNumber, char *noteString);

int caughtSignalInterrupt = FALSE;

char awareCompressionOptions[] =
"Aware Compression Options:\n"
"    -aw_targetbitrate    bits/second\n"
"    -aw_bitratepolicy    fixed, constantquality, lossless\n"
"    -aw_channelpolicy    stereo, joint, independent\n"
"    -aw_noisetomaskratio [-13 .. +13] Decibels";

char compressionAlgorithms[] =
"Compression Algorithms:\n"
"    Standard:     alaw  ulaw  g722  g726  g728  gsm  dvi\n"
"    MPEG:         mpeg1L1   mpeg1L2\n"
"    MultiRate:    awmultirate awlossless\n";

char compressionAlgorithmDescription[] =
"Compression Algorithms:\n"
"    alaw        CCITT G.711 A-law\n"
"    ulaw        CCITT G.711 u-law\n"
"    g722        CCITT G.722\n"
"    g726        CCITT G.726\n"
"    g728        CCITT G.728\n"
"    gsm         GSM\n"
"    dvi         DVI Audio\n"
"\n"
"    mpeg1L1     MPEG1 Layer I\n"
"    mpeg1L2     MPEG1 Layer II\n"
"    awmultirate Aware multirate\n"
"    awlossless  Aware lossless\n";

/* keyword routines  */
char *sfKeywordUsage =
"    byteorder   <e>    endian {big,little}\n"
"    channels    <n>    \n"
"    compression <c>    standard {alaw,ulaw,g722,g726,g728,gsm,dvi}\n"
"                       MPEG     {mpegL1, mpeg1L2}\n"
"                       Aware    {awmultirate,awlossless}\n"
"    dataoffset  <o>    raw input data starts at this byte offset\n"
"    framecount  <f>    raw input data has this many frames\n"
"    double      <s>    double precision 64-bit data, optional slope s\n"
"    float       <s>    single precision 32-bit data, optional slope s\n"
"    format      <f>    {aiff, aifc, next, ircam, wave, voc, iff, avr, smp, sf2, raw}\n"
"    integer     <n s>  n-bit integer, where s:\n"
"                        2scomp  :   2's complement signed data\n"
"                        unsigned:   unsigned data\n"
"    pcmmap <slope><intercept minclip maxclip> pcm mapping\n"
"    rate        <r>    sampling rate (Hertz)\n";


int inList(int value, int *list, int len)
{
    int i;
    for(i=0; i < len; i++) {
	if(list[i] == value) return 1;
    }
    return 0;
}

/* ******************************************************************
 * SFError:     
 * ****************************************************************** */
void 
SFError(char *format, ...)
{
    if (reportAFError)
    {
	va_list ap;
	fprintf( stderr, "%s: ", applicationName );

	va_start(ap, format);
	vfprintf( stderr, format, ap );
	va_end(ap);

	fprintf(stderr, "\n" );
    }
}   /* ---- end SFError() ---- */

/* ******************************************************************
 * SFPrint:     
 * ****************************************************************** */
void 
SFPrint(char *format, ...)
{
    if (sfPrint != SF_NONE)
    {
	va_list ap;
	va_start(ap, format);
	vfprintf( stdout, format, ap );
	va_end(ap);

	fprintf(stdout, "\n" );
    }
}   /* ---- end SFPrint() ---- */

/* ******************************************************************
 * DefaultAFError:    default application-defined error reporting routine
 *            for Audio Library
 * ****************************************************************** */
void
DefaultAFError(long code, const char *description)
{
    switch (code)
    {
#if 0
    case AF_BAD_CODEC_LICENSE:
	SFError("License unavailable for Aware %s decoder\n",
	    compression==AF_COMPRESSION_AWARE_MULTIRATE?"MultiRate":"MPEG");
	break;
#endif
    default:
	SFError("(Audio File Library error %d): %s\n",
	    code, description);
	break;
    }
} /* ---- end DefaultAFError() ---- */

/* ******************************************************************
 * DefaultALError: default application-defined error reporting routine
 *            for Audio Library
 * ****************************************************************** */
void
DefaultALError(long code, const char *description, ...)
{
    fprintf(stderr, "%s (Audio Library error %d): %s\n",
        applicationName, code, description);
} /* ---- end DefaultALError() ---- */

/* ******************************************************************
 * QuietAFError:     
 * ****************************************************************** */
void 
QuietAFError(long error, const char *description)
{
    lastAFError = error;
}   /* ---- end QuietAFError() ---- */

/* ******************************************************************
 * QuietALError:    
 * ****************************************************************** */
void
QuietALError(long code, const char *description, ...)
{
} /* ---- end QuietALError() ---- */

/* ******************************************************************
 * LoudAFError:    
 * ****************************************************************** */
void 
LoudAFError(long error, const char *description)
{
    lastAFError = error;
    SFError("%s", description);
}   /* ---- end LoudAFError() ---- */

/* ******************************************************************
 * LoudALError:    
 * ***************************************************************** */
void 
LoudALError(long error, const char *description, ...)
{
    lastALError = error;
    SFError("%s", description);
}   /* ---- end LoudALError() ---- */

/* ******************************************************************
 * GetPathLead:    allocate and return ptr to path lead up to file name
 * ****************************************************************** */
char *
GetPathLead(char *path)
{
    char *lead;
    char *p = strrchr(path, '/');

    if (!p)
	return (NULL);
    lead = StringDuplicate(path);
    p = strrchr(lead, '/');
    *p = '\0';
    return (lead);
} /* ---- end GetPathLead() ---- */

/* ******************************************************************
 * GetPathTail:    get tail of file's path name
 * ****************************************************************** */
char *
GetPathTail(char *path)
{
    char *p = strrchr(path, '/');
    if (p)
	p++;
    else 
	p = path;

    return (StringDuplicate(p));
} /* ---- end GetPathTail() ---- */

/* ******************************************************************
 * Empty:    Return TRUE if string (usu.fileName) contains only 
 *        spaces and tabs
 * ****************************************************************** */
bool 
Empty( char *string )
{
    return ( strspn( string, " \t" ) == strlen( string ) );
} /* ---- end Empty() ---- */

/* ******************************************************************
 * MatchArg:    Return TRUE if given userString matches targetString
 *        to at least maxOf(minimum match, length of userString)
 * ****************************************************************** */
bool 
MatchArg( char *userString, char *targetString, int minimumMatch )
{
    int matchlen = max(strlen(userString), minimumMatch);

    return ( !strncmp(userString, targetString, matchlen) );
} /* ---- end MatchArg() ---- */

/* ******************************************************************
 * DataFormatName:  Assemble string containing data format, 
 *            using short or long format
 * ****************************************************************** */
void
DataFormatName(AFfilehandle handle, int track, char *out, char printFormat)
/* printFormat:        SF_SHORT or other */
{
    int    sampleFormat, sampleWidth, compressionType;
    char    *stringPtr;

    afGetSampleFormat(handle, track, &sampleFormat, &sampleWidth);
    compressionType = afGetCompression(handle, track);

    /* 
 * print short data format - output should not be space-padded on either side
 *                         - output should fit in 8 bytes
 */
    if (SF_SHORT == printFormat)
    {
	if ( AF_COMPRESSION_NONE != compressionType )
	{
	    stringPtr = (char *) afQueryPointer(AF_QUERYTYPE_COMPRESSION,
	        AF_QUERY_LABEL, compressionType, 
	        0, 0);
	    if ((int)stringPtr > 0)
		strcpy(out, stringPtr);
	    else
		strcpy(out, "compress"); /* why put space at end?? */
	}
	/* NOT COMPRESSED -- do normal thing */
	else 
	{
	    switch (sampleFormat)
	    {
	    case AF_SAMPFMT_FLOAT:
		strcpy(out, "float");
		break;

	    case AF_SAMPFMT_DOUBLE:
		strcpy(out, "double");
		break;

	    case AF_SAMPFMT_TWOSCOMP:
		sprintf(out, "%2ubit 2-", sampleWidth);
		break;

	    case AF_SAMPFMT_UNSIGNED:
		sprintf(out, "%2ubit u-", sampleWidth);
		break;
	    default:
		strcpy(out, "unknown");
		break;
	    }
	}
    }

    /*
 * print normal data format
 */
    else
    {
	if ( AF_COMPRESSION_NONE != compressionType )
	{
	    stringPtr = (char *) afQueryPointer(AF_QUERYTYPE_COMPRESSION,
	        AF_QUERY_NAME, compressionType, 0, 0);
	    if ((int)stringPtr > 0)
		strcpy(out, stringPtr);
	    else
		strcpy(out, "Unrecognized");
	    if (compressionType != AF_COMPRESSION_MPEG1)
		strcat(out, " compression");
	}

	/* NOT COMPRESSED -- do normal thing */
	else 
	{
	    char *endian = 
	    (AF_BYTEORDER_BIGENDIAN == afGetByteOrder(handle, track)) ?
	    "big endian" : "little endian";

	    double slope, intercept, minClip, maxClip;
	    afGetPCMMapping(handle, track, &slope, &intercept, &minClip, &maxClip);
	    switch (sampleFormat)
	    {
	    case AF_SAMPFMT_FLOAT:
		sprintf(out, 
		    "single precision (32-bit) floating point, %s",
		    endian);
		break;

	    case AF_SAMPFMT_DOUBLE:
		sprintf(out, 
		    "double precision (64-bit) floating point, %s",
		    endian);
		break;

	    case AF_SAMPFMT_TWOSCOMP:
	    case AF_SAMPFMT_UNSIGNED:
		{
		    char *sign = (AF_SAMPFMT_TWOSCOMP == sampleFormat) ?
		    "2's complement" : "unsigned";

		    if (sampleWidth <= 8)
			sprintf(out, "%u-bit integer (%s)", 
			    sampleWidth, sign);
			/* write endian string only for multiple bytes */
		    else
			sprintf(out, "%u-bit integer (%s, %s)", 
			    sampleWidth, sign, 
			    endian);
		}
		break;
	    default:
		strcpy(out, "unknown");
		break;
	    }
	}
    }
} /* ---- end DataFormatName() ---- */

/* ******************************************************************
 * DataFormatExtras:  Assemble string containing data format extras
 *            Currently, extras are for floating point sample format
 * ****************************************************************** */
void
DataFormatExtras(Track *track, char *out, char printFormat)
/* printFormat:        currently makes no difference */
{

    out[0] = '\0';

    /* extras for no compression */
    if (AF_COMPRESSION_NONE == track->compressionType)
    {
	/* double    slope, intercept, minClip, maxClip; */
	/* int    sampleFormat, sampleWidth; */
	/*     afGetSampleFormat(handle, track, &sampleFormat, &sampleWidth); */
	if (AF_SAMPFMT_FLOAT == track->sampleFormat || AF_SAMPFMT_DOUBLE == track->sampleFormat)
	{
	    /*     afGetPCMMapping(handle, track, &slope, &intercept, &minClip, &maxClip);      */
	    sprintf(out, "slope=%.15g, intercept=%.15g", track->pcmMapSlope, track->pcmMapIntercept);
	}
	return;
    }
    /* extras for compression */
    else if (AF_COMPRESSION_MPEG1 == track->compressionType)
    {
	Aware *aware = &(track->aware);
	char layer[100];

	/* MPEG compression:  add layer, bit rate, bit rate policy */
	if        (aware->layer == AF_AWARE_LAYER_I)
	    strcpy(layer, "MPEG Layer I compression");
	else if (aware->layer == AF_AWARE_LAYER_II)
	    strcpy(layer, "MPEG Layer II compression");
	else 
	    strcpy(layer, "MPEG Layer ? compression");

#ifdef POLICY_NEEDED
	{
	    char policy[100];
	    if        (aware->bitRatePolicy == AF_AWARE_FIXED_RATE)
		strcpy(policy, "Fixed Rate");
	    else if (aware->bitRatePolicy == AF_AWARE_CONST_QUAL)
		strcpy(policy, "Constant Quality");
	    else if (aware->bitRatePolicy == AF_AWARE_LOSSLESS)
		strcpy(policy, "LossLess");
	    else 
		policy[0] = '\0';
	}
#endif

	sprintf(out, "%s, %d bits/second", layer, aware->bitRateTarget);
#ifdef SAFE
	printf("bitRateTargetSpecified=%d\n", aware->bitRateTargetSpecified);
	printf("channelPolicySpecified=%d\n", aware->channelPolicySpecified);
	printf("bitRatePolicySpecified=%d\n", aware->bitRatePolicySpecified);
	printf("constantQualityNoiseToMaskRatioSpecified=%d\n", aware->constantQualityNoiseToMaskRatioSpecified);

	printf("bitRateTarget=%d\n", aware->bitRateTarget);
	printf("channelPolicy=%d\n", aware->channelPolicy);
	printf("bitRatePolicy=%d\n", aware->bitRatePolicy);
	printf("constantQualityNoiseToMaskRatio=%g\n", aware->constantQualityNoiseToMaskRatio);
#endif
    }
} /* ---- end DataFormatExtras() ---- */

/* ******************************************************************
 * AwareCompressionParameters:  Assemble string containing Aware 
 *                compression parameters
 * ****************************************************************** */
void
AwareCompressionParameters(AFfilehandle handle, int id, 
char *out, char printFormat)
/* printFormat:        SF_SHORT or SF_LONG */
{
    char *stringPtr;
    int compression = afGetCompression(handle, id);

    if ( AF_COMPRESSION_NONE != compression )
    {
	stringPtr = (char *) afQueryPointer(AF_QUERYTYPE_COMPRESSION,
	    AF_QUERY_LABEL, compression, 0, 0);
	if ((int)stringPtr > 0)
	{
	    strcpy(out, stringPtr);
	    strcat(out, " ");
	}
	else
	    strcpy(out, "unkn ");
    }
    /* NOT COMPRESSED -- do normal thing */
    else 
    {

#ifdef IS_NEEDED
	if (AF_FILE_AIFFC == file->formatID)
	{
	    /* compression for AIFF_C file only */
	    if ((AF_COMPRESSION_AWARE_MULTIRATE == track->compressionType)|| 
	        (AF_COMPRESSION_MPEG1 == track->compressionType))
	    {
		pvlist = AUpvnew(MAX_AWARE_OPTS);

		AUpvsetparam  (pvlist, 0, AF_AWARE_PARAM_LAYER);
		AUpvsetvaltype(pvlist, 0, AU_PVTYPE_LONG);

		AUpvsetparam  (pvlist, 1, AF_AWARE_PARAM_BITRATE_POLICY);
		AUpvsetvaltype(pvlist, 1, AU_PVTYPE_LONG);

		AUpvsetparam  (pvlist, 2, AF_AWARE_PARAM_BITRATE_TARGET);
		AUpvsetvaltype(pvlist, 2, AU_PVTYPE_LONG);

		afGetCompressionParams(handle, id, 
		    &track->compressionType, pvlist, 3);

		AUpvgetval(pvlist, 0, &track->aware.layer);
		AUpvgetval(pvlist, 1, &track->aware.bitRatePolicy);
		AUpvgetval(pvlist, 2, &track->aware.bitRateTarget);

		AUpvfree(pvlist);
	    }
	}
#endif

    }
} /* ---- end AwareCompressionParameters() ---- */

/* ******************************************************************
 * DurationInSeconds:    Convert to format hours:minutes:seconds
 * ****************************************************************** */
void
DurationInSeconds(double seconds, char *out, char printFormat)
/*
   seconds - time value
   out - assumed long enough
   printFormat - SF_SHORT or !SF_SHORT.  Short format uses time labels such
    as hrs, min, sec
*/
{
    int    hours, minutes, wholeSeconds;
    double    subSeconds, milliSeconds, microSeconds, nanoSeconds;
    char    s2[50];
    bool    isShort = (printFormat==SF_SHORT);

    if (seconds < 0)
    {
	strcpy(out, isShort ? "?sec" : "? time");
	return;
    }
    if (0 == seconds)
    {
	strcpy(out, isShort ? "0 sec" : "0 seconds");
	return;
    }

    /* compute hours, minutes and seconds */
    wholeSeconds = (int) seconds;
    minutes = (wholeSeconds/60)%60;
    hours = wholeSeconds/3600;
    subSeconds = seconds - ((double) wholeSeconds);
    wholeSeconds %= 60;

    /* duration less than 1 hour  */
    if (hours < 1)
    {
	/* duration less than 1 minute  */
	if (minutes < 1)
	{
	    /* duration less than 0.1 second. 
This way, 900 milliseconds are printed as .900 seconds */
	    /* subsecond units not printed for SF_SHORT format */
	    if (seconds < 0.1 && !isShort)
	    {
		milliSeconds = subSeconds*1000;
		if (milliSeconds < 0.1)
		{
		    microSeconds = milliSeconds*1000;

		    /* print in nano seconds */
		    if (microSeconds < 0.1)
		    {
			nanoSeconds = microSeconds*1000;

			sprintf(out, "%.4g nSecond", nanoSeconds);
			/* pluralize units *rounded* to integer */
			if ((int)(nanoSeconds+0.001) != 1)
			    strcat(out, "s");
		    }
		    /* print in micro seconds */
		    else
		    {
			sprintf(out, "%.4g uSecond", microSeconds);
			/* pluralize units *rounded* to integer */
			if ((int)(microSeconds+0.001) != 1)
			    strcat(out, "s");
		    }
		}
		/* print in milli seconds */
		else
		{
		    sprintf(out, "%.3g mSecond", milliSeconds);
		    /* pluralize units *rounded* to integer */
		    if ((int) (milliSeconds+0.001) != 1)
			strcat(out, "s");
		}
	    }
	    /* duration greater than 1 second or printFormat=SF_SHORT, Use Minute:Second format */
	    else
	    {
		if (isShort)
		    sprintf(out, "%g sec", seconds);
		else
		{
		    sprintf(out, "%.3g second", seconds);
		    /* pluralize units */
		    if (seconds != 1)
			strcat(out, "s");
		}
	    }
	}
	/* duration >= 1 minute.  Use Minute:Second format */
	else
	{
	    sprintf(out, "%d:%.2d", minutes, wholeSeconds);
	    if (subSeconds != 0)
	    {
		sprintf(s2, ".%.3g", subSeconds);
		strcat(out, s2+2);    /* offset avoids leading "0."  */
	    }
	    strcat(out, isShort ? " min" : " minute");

	    /* pluralize units to integer */
	    if (!isShort && !(minutes == 1 && wholeSeconds == 0 && subSeconds == 0))
		strcat(out, "s");
	}
    }

    /* duration >= 1 hour.  Use Hour:Minute:Second format */
    else
    {
	sprintf(out, "%d:%.2d:%.2d", hours, minutes, wholeSeconds);
	if (subSeconds != 0)
	{
	    sprintf(s2, ".%.3g", subSeconds);
	    strcat(out, s2+2);    /* offset avoids leading "0."  */
	}
	strcat(out, isShort ? " hrs" : " hour");

	/* pluralize units */
	if (!isShort && !(hours == 1 && minutes == 0 && wholeSeconds == 0))
	    strcat(out, "s");
    }
} /* ---- end DurationInSeconds() ---- */

/* ******************************************************************
 * SizeInBytes:        Convert to bytes with metric system prefixes
 * ****************************************************************** */
void
SizeInBytes(int totalBytes, char *out, char printFormat)
{
    char    s[50];
    double    bytes;

    if (totalBytes < 0)
    {
	sprintf(out, "? Bytes");
	return;
    }

    /* print size with 3 significant digits in range */
    bytes = (double) totalBytes;
    /* units in Giga bytes */
    if    (totalBytes > 1024*1024*1024)
    {
	bytes /= 1024*1024*1024;
	s[0] = 'G';
	s[1] = '\0';
    }
    /* units in Mega bytes */
    else if (totalBytes > 1024*1024)
    {
	bytes /= 1024*1024;
	s[0] = 'M';
	s[1] = '\0';
    }
    /* units in Kilo bytes */
    else if (totalBytes > 1024)
    {
	bytes /= 1024;
	s[0] = 'K';
	s[1] = '\0';
    }
    /* units in bytes */
    else 
	s[0] = '\0';

    if    (printFormat == SF_SHORT)
	sprintf(out, "%.2f %sByte", bytes, s);
    else 
	sprintf(out, "%.3g %sByte", bytes, s);

    /* pluralize bytes */
    if (bytes != 1)
	strcat(out, "s");

    /* printf("SizeInBytes: totalBytes=%d -> '%s'\n", totalBytes, out); */
} /* ---- end SizeInBytes() ---- */

/* ******************************************************************
 * RateInHertz:        Convert to Hertz with metric system prefixes
 * ****************************************************************** */
void
RateInHertz(double rate, char *out, char printFormat)
{
    char    s[50];

    if (rate < 0)
    {
	sprintf(out, "? Hertz");
	return;
    }

    /* print size with 3 significant digits in range */

    /* units in Giga Hertz */
    if    (rate >= 1000000000)
    {
	rate *= 0.000000001;
	/*    s[0] = 'G'; s[1] = '\0';*/
	strcpy(s, "GHz");
    }
    /* units in Mega Hertz */
    else if (rate >= 1000000)
    {
	rate *= 0.000001;
	/*    s[0] = 'M'; s[1] = '\0';*/
	strcpy(s, "MHz");
    }
    /* units in Kilo Hertz */
    else if (rate >= 1000)
    {
	rate *= 0.001;
	/*    s[0] = 'K'; s[1] = '\0';*/
	strcpy(s, "KHz");
    }
    /* units in Hertz */
    else 
    {
	/*    s[0] = '\0';*/
	strcpy(s, "Hertz");
    }

    if    (printFormat == SF_SHORT)
	sprintf(out, "%8gHz", rate, s);
    else
	sprintf(out, "%.15g %s", rate, s);
} /* ---- end RateInHertz() ---- */

/* ******************************************************************
 * FindCopyright:    return ptr to printable copyright 
 *                miscellaneous chunk.
 * ****************************************************************** */
char *
FindCopyright( AFfilehandle handle )
{
    int        i;
    static char    buffer[256];
    int        nids = afGetMiscIDs(handle, NULL);
    int        *ids;

    if (!nids || (NULL == (ids = malloc( sizeof(int) * nids ))))
	return (NULL);

    afGetMiscIDs(handle, ids);
    for(i = 0; i < nids; i++)
    {
	if ((AF_MISC_COPY == afGetMiscType(handle,ids[i])) &&
	    0 != afGetMiscSize(handle,ids[i]))
	{
	    int bytes;
	    afSeekMisc(handle, ids[i], 0);
	    bytes = afReadMisc(handle, ids[i], buffer, 255);
	    if (bytes >= 0)
	    {
		int j;
		buffer[bytes] = '\0';
		/* ensure all characters are printable */
		for (j = 0; j < bytes; j++)
		{
		    if (!isprint(buffer[j]))
			buffer[j] = ' ';
		}
	    }
	    else
		strcpy(buffer, "--");

	    free(ids);
	    return (buffer);
	}
    }

    free(ids);
    return (NULL);
} /* ---- end FindCopyright() ---- */

/* ******************************************************************
 * FindFileFormatName:    return  ptr to file format name.
 *                AF_FILE_UNKNOWN for none found
 * ****************************************************************** */
static int 
FindFileFormatName(char *name)
{
    int    i;
    int    nFormats = afQueryLong(AF_QUERYTYPE_FILEFMT, AF_QUERY_ID_COUNT, 0,0,0);
    long    *ids = afQueryPointer(AF_QUERYTYPE_FILEFMT, AF_QUERY_IDS, 0,0,0);

    for( i = 0; i < nFormats; i++ )
    {
	int id = ids[i];
	if (id != AF_FILE_RAWDATA)
	{
	    char *label = afQueryPointer(AF_QUERYTYPE_FILEFMT,AF_QUERY_LABEL,id,0,0);
	    if ( !strncasecmp(name, label, 4) )
	    {
		free(ids);
		return (id);
	    }
	}
    }

    free(ids);
    return (AF_FILE_UNKNOWN);
} /* ---- end FindFileFormatName() ---- */

/* ******************************************************************
 * FindCompressionName:        return  ptr to compression name
 *                    AF_COMPRESSION_UNKNOWN for none found
 * ****************************************************************** */
static int 
FindCompressionName(char *name, int *sampleFormat, int *sampleWidth)
{
    int    i;
    int    compressionTypeCount;
    int    *ids;
    int    special;

    /* we test for old ("awmpeg*") names for backwards compatibility */

    /* -- NOTE test for awmpeg1L2 must come before test for awmpeg1 !! -- */

    /* test for mpeg1L2 first */
    if      (MatchArg(name, "awmpeg1L2", 9) || MatchArg(name, "awmpeg2", 7)
        || MatchArg(name, "mpeg1L2", 7) || MatchArg(name, "mpeg2", 5))
    {
	special = AF_COMPRESSION_DEFAULT_MPEG1_LAYERII;
	name = "mpg1";
    }
    /* then test for mpeg1 */
    else if (MatchArg(name, "awmpeg1L1", 9) || MatchArg(name, "awmpeg1", 7)
        || MatchArg(name, "mpeg1L1", 7) || MatchArg(name, "mpeg1", 5))
    {
	special = AF_COMPRESSION_DEFAULT_MPEG1_LAYERI;
	name = "mpg1";
    }
    else if (MatchArg(name, "awmultirate", 4))
    {
	special = AF_COMPRESSION_AWARE_DEFAULT_MULTIRATE;
	name = "awmr";
    }
    else if (MatchArg(name, "awlossless", 3) ||
        MatchArg(name, "awlsls", 3))
    {
	special = AF_COMPRESSION_AWARE_DEFAULT_LOSSLESS;
	name = "awmr";
    }
    else
    {
	special = -1;
    }

    compressionTypeCount = afQueryLong(AF_QUERYTYPE_COMPRESSION,AF_QUERY_ID_COUNT,0,0,0);
    ids = afQueryPointer(AF_QUERYTYPE_COMPRESSION, AF_QUERY_IDS, 0,0,0);
    for( i = 0; i < compressionTypeCount; i++ )
    {
	int id = ids[i];
	char *label = afQueryPointer(AF_QUERYTYPE_COMPRESSION,
	    AF_QUERY_LABEL, id, 0, 0);
	if ( !strncasecmp(name, label, 4) )
	{
	    free(ids);
	    *sampleFormat = afQueryLong(AF_QUERYTYPE_COMPRESSION,
	        AF_QUERY_NATIVE_SAMPFMT, id, 0, 0);
	    *sampleWidth = afQueryLong(AF_QUERYTYPE_COMPRESSION,
	        AF_QUERY_NATIVE_SAMPWIDTH, id, 0, 0);
	    return ((special >= 0) ? special : id);
	}
    }

    free(ids);
    return (AF_COMPRESSION_UNKNOWN);
} /* ---- end FindCompressionName() ---- */

/* ******************************************************************
 * Numeric:    Is 1st character is i[0 .. 9], a '.', or '-' ?    
 * ****************************************************************** */
char 
Numeric(char *s)
{
    return (((*s >= '0')&&(*s <= '9')) || ('.' == *s) || ('-' == *s));
} /* ---- end Numeric() ---- */

/* ******************************************************************
 * ReadChannelMatrixFile:    read channel matrix file 
 *                Return Boolean success matrix 
 *                as array of doubles     
 * ****************************************************************** */
bool 
ReadChannelMatrixFile(char *fileName, int vchans, int fchans, 
double **ret)
{
    double        *vals = malloc(sizeof(double) * vchans * fchans);
    int        i;
    FILE        *fp;

    ZAP();
    if (NULL == (fp = fopen(fileName, "r")))
    {
	SFError("Unable to open matrix file %s", fileName);
	free(vals);
	*ret = NULL;
	ZAP();
	return (FALSE);
    }

    ZAP();
    for(i = 0; i < vchans*fchans; i++)
    {
	if (fscanf(fp, "%lf", &vals[i]) != 1)
	{
	    SFError("Not enough values in matrix file %s", fileName);
	    fclose(fp);
	    free(vals);
	    *ret = NULL;
	    ZAP();
	    return (FALSE);
	}
#if 0
	fprintf(stderr, "got channel matrix val %g\n", vals[i]);
#endif
    }

    ZAP();
    fclose(fp);
    *ret = vals;
    ZAP();
    return (TRUE);
} /* ---- end ReadChannelMatrixFile() ---- */

/* ******************************************************************
 * ParseKeywords:     parse soundfile descriptor keywords, as seen 
 *            in sfconvert, etc.  Return # keywords found
 * ****************************************************************** */
int 
ParseKeywords(char **arg, SoundFile *file, char **matrixFile)
{
    bool        loopFlag;
    Track        *track;
    int        keyWordsFound;

    /* NO -- matrix file is assumed initialized to start with
   if (matrixFile) 
    *matrixFile = NULL;
*/

    track = file->track;
    ZAP();
    keyWordsFound = 0;
    for (loopFlag = TRUE; (*arg)&&(loopFlag); arg++, keyWordsFound++)
    {
	switch (tolower(*arg[0]))
	{
	case 'a':
	    /* aw_targetbitrate */
	    if (!strcmp(*arg, "aw_targetbitrate"))
	    {
		if ( !*++arg )
		{
		    SFError("'aw_targetbitrate' requires value");
		    return (-1);
		}
		if (!isdigit(*arg[0]))
		{
		    SFError("Invalid %s: %s\n", *(arg-1), *arg);
		    exit(1);
		}
		track->aware.bitRateTarget = atoi(*arg);
		switch (track->aware.bitRateTarget)
		{
		case 32000:  /* I, II */
		case 48000:  /*    II */
		case 56000:  /*    II */
		case 64000:  /* I, II */
		case 96000:  /* I, II */
		case 112000: /*    II */
		case 128000: /* I, II */
		case 160000: /* I, II */
		case 192000: /* I, II */
		case 224000: /* I, II */
		case 256000: /* I, II */
		case 228000: /* I     */
		case 320000: /* I, II */
		case 352000: /* I     */
		case 384000: /* I, II */
		case 416000: /* I     */
		case 448000: /* I     */
		    break;
		default:
		    SFError("Invalid %s: '%s'", *(arg-1), *arg);
		    return (-1);
		}
		track->aware.bitRateTargetSpecified = TRUE;
		keyWordsFound++;
	    }

	    /* aw_channelpolicy */
	    else if (!strcmp(*arg, "aw_channelpolicy"))
	    {
		if ( !*++arg )
		{
		    SFError("'aw_channelpolicy requires value");
		    return (-1);
		}

		if	(!strcmp(*arg, "stereo"))
		    track->aware.channelPolicy = AF_AWARE_STEREO;
		else if (!strcmp(*arg, "independent"))
		    track->aware.channelPolicy = AF_AWARE_INDEPENDENT;
		else if (!strcmp(*arg, "joint"))
		    track->aware.channelPolicy = AF_AWARE_JOINT_STEREO;
		else 
		{
		    SFError("Invalid %s: '%s'", *(arg-1), *arg);
		    return (-1);
		}
		track->aware.channelPolicySpecified = TRUE;
		keyWordsFound++;
	    }

	    /* aw_bitratepolicy */
	    else if (!strcmp(*arg, "aw_bitratepolicy"))
	    {
		if ( !*++arg )
		{
		    SFError("'aw_bitratepolicy requires value");
		    return (-1);
		}
		if	(!strcmp(*arg, "fixed"))
		    track->aware.bitRatePolicy = AF_AWARE_FIXED_RATE;
		else if (!strcmp(*arg, "constantquality"))
		    track->aware.bitRatePolicy = AF_AWARE_CONST_QUAL;
		else if (!strcmp(*arg, "lossless"))
		    track->aware.bitRatePolicy = AF_AWARE_LOSSLESS;
		else 
		{
		    SFError("Invalid %s: '%s'", *(arg-1), *arg);
		    return (-1);
		}
		track->aware.bitRatePolicySpecified = TRUE;
		keyWordsFound++;
	    }

	    /* aw_noisetomaskratio */
	    else if (!strcmp(*arg, "aw_noisetomaskratio"))
	    {
		double noiseToMaskRatio;

		if ( !*++arg )
		{
		    SFError("'aw_noisetomaskratio requires value");
		    return (-1);
		}
		if (!Numeric(*arg))
		{
		    SFError("Invalid %s: %s\n", *(arg-1), *arg);
		    exit(1);
		}
		noiseToMaskRatio = atof(*arg);
		if ((noiseToMaskRatio < -13) || (noiseToMaskRatio > 13))
		{
		    SFError("Invalid %s: '%s'\n", *(arg-1), *arg);
		    exit(1);
		}
		track->aware.constantQualityNoiseToMaskRatio = noiseToMaskRatio;
		track->aware.constantQualityNoiseToMaskRatio = TRUE;
		keyWordsFound++;
	    }
	    break;

	    /* byteorder */
	case 'b':
	    if ( !*++arg )
	    {
		SFError("Specify byteorder:  big, little");
		return (-1);
	    }

	    switch ( tolower((*arg)[0]) )
	    {
	    case 'b':
		track->byteOrder = AF_BYTEORDER_BIGENDIAN;
		break;
	    case 'l':
		track->byteOrder = AF_BYTEORDER_LITTLEENDIAN;
		break;
	    default:
		SFError("Invalid byte order '%s'", *arg);
		return (-1);
	    }
	    keyWordsFound++;
	    break;

	case 'c':
	    switch (tolower((*arg)[1]))
	    {
		/* channels */
	    case 'h':
		if ( !*++arg )
		{
		    SFError("'channels' requires count");
		    return (-1);
		}
		keyWordsFound++;
		track->channelCount = atoi(*arg);
		if (track->channelCount <= 0)
		{
		    SFError("Invalid %s: '%s'", *arg-1, *arg);
		    return (-1);
		}
		break;

		/* compression */
	    case 'o':
		{
		    if ( !*++arg )
		    {
			SFError("'compression' requires algorithm name");
			return (-1);
		    }

		    keyWordsFound++;
		    if (!strcmp(*arg, "none"))
			track->compressionType = AF_COMPRESSION_NONE;
		    else
		    {
			track->compressionType = FindCompressionName(*arg, 
			    &track->sampleFormat, 
			    &track->sampleWidth);

			if (AF_COMPRESSION_UNKNOWN == track->compressionType)
			{
			    if	(!strcmp(*arg, "none"))
				track->compressionType = AF_COMPRESSION_NONE;
			    else if (!strcmp(*arg, "g722"))
				track->compressionType = AF_COMPRESSION_G722;
			    else if (!strcmp(*arg, "g726"))
				track->compressionType = AF_COMPRESSION_G726;
			    else if (!strcmp(*arg, "g728"))
				track->compressionType = AF_COMPRESSION_G728;
			    else if (!strcmp(*arg, "gsm"))
				track->compressionType = AF_COMPRESSION_GSM;
			    else if (!strcmp(*arg, "dvi"))
				track->compressionType = AF_COMPRESSION_DVI_AUDIO;
			    else if	(!strcmp(*arg, "awmpeg1L1") || !strcmp(*arg, "awmpeg1")
			        || !strcmp(*arg, "mpeg1L1") || !strcmp(*arg, "mpeg1"))
			    {
				track->compressionType = AF_COMPRESSION_MPEG1;
				track->aware.layer	  = AF_AWARE_LAYER_I; /* whatever */
				track->aware.defaultConfiguration 
				    = AF_COMPRESSION_DEFAULT_MPEG1_LAYERI;
				track->aware.bitRateTargetSpecified = FALSE;
				track->aware.channelPolicySpecified = FALSE;
				track->aware.bitRatePolicySpecified = FALSE;
				track->aware.constantQualityNoiseToMaskRatioSpecified = FALSE;
			    }
			    else if (!strcmp(*arg, "awmpeg1L2") || !strcmp(*arg, "awmpeg2")
			        || !strcmp(*arg, "mpeg1L2") || !strcmp(*arg, "mpeg2"))
			    {
				track->compressionType = AF_COMPRESSION_MPEG1;
				track->aware.layer	  = AF_AWARE_LAYER_II;
				track->aware.defaultConfiguration 
				    = AF_COMPRESSION_DEFAULT_MPEG1_LAYERII;
				track->aware.bitRateTargetSpecified = FALSE;
				track->aware.channelPolicySpecified = FALSE;
				track->aware.bitRatePolicySpecified = FALSE;
				track->aware.constantQualityNoiseToMaskRatioSpecified = FALSE;
			    }
			    else if (!strcmp(*arg, "awlossless") || !strcmp(*arg, "awlsls"))
			    {
				track->compressionType = AF_COMPRESSION_AWARE_MULTIRATE;
				track->aware.layer	  = AF_AWARE_LAYER_I; /* whatever */
				track->aware.defaultConfiguration 
				    = AF_COMPRESSION_AWARE_DEFAULT_LOSSLESS;
				track->aware.bitRateTargetSpecified = FALSE;
				track->aware.channelPolicySpecified = FALSE;
				track->aware.bitRatePolicySpecified = FALSE;
				track->aware.constantQualityNoiseToMaskRatioSpecified = FALSE;
			    }
			    else if (!strcmp(*arg, "awmultirate") || !strcmp(*arg, "awmulti"))
			    {
				track->compressionType = AF_COMPRESSION_AWARE_MULTIRATE;
				track->aware.layer	  = AF_AWARE_LAYER_I; /* whatever */
				track->aware.defaultConfiguration
				    = AF_COMPRESSION_AWARE_DEFAULT_MULTIRATE;
				track->aware.bitRateTargetSpecified = FALSE;
				track->aware.channelPolicySpecified = FALSE;
				track->aware.bitRatePolicySpecified = FALSE;
				track->aware.constantQualityNoiseToMaskRatioSpecified = FALSE;
			    }
			    else
			    {
				SFError("Unsupported compression method: '%s'", *arg);
				SFError("%s", compressionAlgorithmDescription);
				return (-1);
			    }
			}
		    }

		    track->sampleFormat = AF_SAMPFMT_TWOSCOMP;
		    track->sampleWidth = 16;
		}
		break;

	    default:
		SFError("Unknown keyword '%s'", *arg );
		return (-1);
	    }
	    break;

	    /* dataoffset */
	case 'd':
	    switch (tolower((*arg)[1]))
	    {
	    case 'a':
		if ( !*++arg )
		{
		    SFError("'dataoff' requires offset");
		    return (-1);
		}
		if (!isdigit(*arg[0]))
		{
		    SFError("Invalid %s: %s\n", *(arg-1), *arg);
		    exit(1);
		}
		track->dataOffset = atoi(*arg);
		/* prevent negative offsets */
		if (track->dataOffset < 0)
		{
		    SFError("Invalid %s: %s\n", *(arg-1), *arg);
		    exit(1);
		}
		keyWordsFound++;
		break;

		/* double */
	    case 'o':
		{
		    double slope;
		    /* slope ("maxamp") */
		    if (*(arg+1) && Numeric(*(arg+1)))
		    {
			slope = atof( *(++arg) );
			keyWordsFound++;
			if ( 0 == slope)
			{
			    SFError("Must specify non-zero slope with 'double'");
			    return (-1);
			}

			track->pcmMapSpecified = TRUE;
			track->pcmMapSlope	   = slope;
		    }

		    track->compressionType = AF_COMPRESSION_NONE;
		    track->sampleFormat    = AF_SAMPFMT_DOUBLE;
		    track->sampleWidth     = 64;
		}
		break;

	    default:
		SFError("Unknown keyword %s", *arg );
		return (-1);
	    }
	    break;

	    /* end */
	case 'e':
	    loopFlag = FALSE;
	    return (keyWordsFound);

	case 'f':
	    switch (tolower((*arg)[1]))
	    {
		/* float */
	    case 'l':
		{
		    double slope;

		    /* slope ("maxamp") */
		    if ((*(arg+1))&&(Numeric(*(arg+1))))
		    {
			slope = atof(*(++arg));
			keyWordsFound++;
			if (0 == slope)
			{
			    SFError("Must specify non-zero slope with 'float'");
			    return (-1);
			}
			track->pcmMapSpecified = TRUE;
			track->pcmMapSlope = slope;
		    }

		    track->compressionType = AF_COMPRESSION_NONE;
		    track->sampleFormat    = AF_SAMPFMT_FLOAT;
		    track->sampleWidth     = 32;
		}
		break;

		/* format */
	    case 'o':
		if ( !*++arg )
		{
		    SFError("'format' requires format name");
		    return (-1);
		}

		keyWordsFound++;
		file->formatID = FindFileFormatName(*arg);
		if (AF_FILE_UNKNOWN == file->formatID)
		{
		    /* raw file? */
		    if (!strcmp(*arg, "raw"))
			file->formatID = AF_FILE_RAWDATA;
		    else
		    {
			SFError("Format '%s' not supported", *arg );
			PrintFormats();
			return (-1);
		    }
		}
		break;

		/* framecount */
	    case 'r':
		if ( !*++arg )
		{
		    SFError("'framecount' requires count of sample frames");
		    return (-1);
		}
		keyWordsFound++;
		track->frameCount = atoi(*arg);
		if (track->frameCount < 0)
		{
		    SFError("Invalid framecount: '%s'", *arg );
		    return (-1);
		}
		break;

		/* unknown keyword */
	    default:
		SFError("Unknown keyword %s", *arg );
		return (-1);
	    }
	    break;

	    /* integer */
	case 'i':
	    if ( !*(arg+1) || !*(arg+2) )
	    {
		SFError("'integer' requires # bits and sign type");
		return (-1);
	    }

	    track->sampleWidth = atoi( *++arg );
	    if ((track->sampleWidth < 1) || (track->sampleWidth > 32))
	    {
		SFError("Invalid bit count '%s'. Choose value in range [1 .. 32]",
		    *arg);
		return (-1);
	    }
	    track->compressionType = AF_COMPRESSION_NONE;
	    keyWordsFound++;

	    switch (tolower(**++arg))
	    {
		/* 2's complement */
	    case '2':
		track->sampleFormat = AF_SAMPFMT_TWOSCOMP;
		break;
		/* unsigned */
	    case 'u':
		track->sampleFormat = AF_SAMPFMT_UNSIGNED;
		break;
	    default:
		SFError("Invalid integer sign type: '%s'", *arg);
		return (-1);
	    }
	    keyWordsFound++;
	    break;

	case 'm':
	    switch (tolower((*arg)[1]))
	    {
		/* mulaw -- supported for backwards compatibility */
	    case 'u':
		track->compressionType = AF_COMPRESSION_G711_ULAW;
		track->sampleFormat    = AF_SAMPFMT_TWOSCOMP;
		track->sampleWidth     = 16;
		break;

		/* matrix -- supported as a keyword in undocumented form, for testsuite */
	    case 'a':
		if ( !*++arg )
		{
		    SFError("'matrix' requires name of matrix file");
		    return (-1);
		}
		if (matrixFile)
		    *matrixFile = StringDuplicate(*arg);
		keyWordsFound++;
		break;

	    default:
		SFError("Unknown keyword %s", *arg );
		return (-1);
	    }
	    break;

	    /* pcmmap slope [ intercept [ minClip maxClip ] ]*/
	case 'p':
	    {
		double slope, intercept = 0, minClip = 0, maxClip = 0;

		/* slope */
		if ( !*++arg )
		{
		    SFError("'pcm' requires at least one of {slope,intercept,minclip,maxclip}");
		    return (-1);
		}
		keyWordsFound++;
		if (!Numeric(*arg))
		{
		    SFError("Invalid 'pcmmap' slope: '%s'", *arg);
		    return (-1);
		}
		if (0 == (slope = atof(*arg)))
		{
		    SFError("Slope for 'pcmmap' must be nonzero");
		    return (-1);
		}
		/* intercept */
		if ( *(arg+1) && Numeric(*(arg+1)) )
		{
		    intercept = atof(*++arg);
		    keyWordsFound++;
		    /* minClip, maxClip */
		    if ( *(arg+1) && Numeric(*(arg+1)) )
		    {
			if (!Numeric(*++arg))
			{
			    SFError("Invalid 'pcmmap' minclip: '%s'", *arg);
			    return (-1);
			}
			keyWordsFound++;
			minClip = atof(*arg);
			/* need max clip if min clip supplied */
			if ( !*++arg )
			{
			    SFError("'pcmmap' requires maxClip if minClip is given");
			    return (-1);
			}
			if (!Numeric(*arg))
			{
			    SFError("Invalid 'pcmmap' maxclip: '%s'", *arg);
			    return (-1);
			}
			keyWordsFound++;
			maxClip = atof(*arg);
		    }
		}

		track->pcmMapSpecified = TRUE;
		track->pcmMapSlope     = slope;
		track->pcmMapIntercept = intercept;
		track->pcmMapMinClip   = minClip;
		track->pcmMapMaxClip   = maxClip;
	    }
	    break;

	    /* rate */
	case 'r':
	    {
		if ( !*++arg )
		{
		    SFError("'rate' requires sampling rate");
		    return (-1);
		}
		track->samplingRate = atof( *arg );
		if (track->samplingRate <= 0)
		{
		    SFError("Sampling rate '%s' not supported", *arg);
		    return (-1);
		}
		keyWordsFound++;
	    }
	    break;

	default:
	    SFError("Unknown keyword %s", *arg );
	    return (-1);
	}
    }

    return (keyWordsFound);
} /* ---- end ParseKeywords() ---- */

/* ******************************************************************
 * PrintSoundFileParameters:    Print contents of sound file headers
 *                Currenly prints ONE track
 * ****************************************************************** */
void 
PrintSoundFileParameters(SoundFile *file, char printFormat)
{
    int    i;

    if (SF_LONG == printFormat)
    {
	/* print miscellaneous information for AIFF, AIFF-C, NeXT, and IRCAM files */
	if (file->mayHaveMiscs)
	{
	    if (file->miscellaneousCount)
	    {
		SFPrint("Miscellaneous:");
		for (i = 0; i < file->miscellaneousCount; i++)
		    PrintMiscellaneousData(file->miscellaneous[i]);
		SFPrint("");
	    }
	}

	/* print information for files with instruments */
	if (file->mayHaveExtras)
	{
	    PrintTrackAIFFParameters(file->track);
	    if (file->instrumentCount)
	    {
		int i;
		for(i = 0; i < file->instrumentCount; i++) {
		    SFPrint("\nInstrument %d:", file->instrumentIDs[i]);
		    PrintInstrumentAIFFParameters(file->instruments[i]);
		}
	    }

	    /* need one extra spacer only if some data is present */
	    if (file->track->markerCount || file->track->aesDataSeen ||
	        file->instrumentCount)
		SFPrint("");
	}
    }

    PrintTrackParameters(file, file->trackID, printFormat);
} /* ---- end PrintSoundFileParameters() ---- */

/* ******************************************************************
 * PrintFormats:    Print supported formats
 * ****************************************************************** */
void
PrintFormats( void )
{
    int i;

    int nFormats = afQueryLong(AF_QUERYTYPE_FILEFMT, AF_QUERY_ID_COUNT, 0, 0, 0);
    int *ids = afQueryPointer(AF_QUERYTYPE_FILEFMT, AF_QUERY_IDS, 0, 0, 0);

    SFPrint("Supported audio file formats:\n");

    for( i = 0; i < nFormats; i++ )
    {
	if (ids[i] != AF_FILE_RAWDATA)
	{
	    SFPrint("%5.5s    %-40.40s", 
	        afQueryPointer(AF_QUERYTYPE_FILEFMT, AF_QUERY_LABEL, ids[i], 0, 0),
	        afQueryPointer(AF_QUERYTYPE_FILEFMT, AF_QUERY_DESC, ids[i], 0, 0));
	}
    }

    free(ids);
} /* ---- end PrintFormats() ---- */

/* ******************************************************************
 * PrintCompressions:    Print supported compression methods    
 * ****************************************************************** */
void 
PrintCompressions( void )
{
    int i;

    int compressionTypeCount = afQueryLong(AF_QUERYTYPE_COMPRESSION, AF_QUERY_ID_COUNT, 0, 0, 0);
    int *ids = afQueryPointer(AF_QUERYTYPE_COMPRESSION, AF_QUERY_IDS, 0, 0, 0);

    SFPrint("Supported compression methods:\n" );
    for( i = 0; i < compressionTypeCount; i++ )
    {
	if (ids[i] != AF_COMPRESSION_NONE)
	{
	    char *label, *name;
	    label = afQueryPointer(AF_QUERYTYPE_COMPRESSION, 
	        AF_QUERY_LABEL, ids[i], 0, 0),
	        name = afQueryPointer(AF_QUERYTYPE_COMPRESSION, 
	        AF_QUERY_NAME, ids[i], 0, 0);
	    SFPrint("%4.4s    %-40.40s", label, name);
	}
    }

    SFPrint("");
    free(ids);
} /* ---- end PrintCompressions() ---- */

/* ******************************************************************
 * PrintSoundFileHandle:    print audio file handle parameters    
 * ****************************************************************** */
void 
PrintSoundFileHandle( AFfilehandle handle )
{
    int        id;
    int        sampleFormat, sampleWidth;
    double        slope, intercept, minClip, maxClip;
    int        dataOffset;
    int        trackID;
    int        instrumentCount;

    int        miscellaneousCount;

    afGetTrackIDs(handle, &trackID);

    /* 
 * print virtual track parameters 
 */
    SFPrint("Virtual Track Parameters");
    id = trackID;
    afGetVirtualSampleFormat(handle, id, 
        &sampleFormat, &sampleWidth);
    SFPrint("    byteOrder=%d, channelCount=%d, sampleFormat=%d,sampleWidth=%d", 
        afGetVirtualByteOrder(handle, id),
        afGetVirtualChannels (handle, id),
        sampleFormat, sampleWidth);

    SFPrint("    samplingRate=%.15g,", 
        afGetVirtualRate(handle, id));

    afGetVirtualPCMMapping(handle, id,
        &slope, &intercept, &minClip, &maxClip);
    SFPrint("    pcmmap: slope=%.15g,intercept=%.15g,minClip=%.15g,maxClip=%.15g", 
        slope, intercept,
        minClip, maxClip);

    /* 
 * print actual track parameters 
 */
    id = trackID;
    SFPrint("Actual Track Parameters");
    afGetSampleFormat(handle, id, &sampleFormat, &sampleWidth);
    SFPrint("    byteOrder=%d, channelCount=%d, sampleFormat=%d,sampleWidth=%d", 
        afGetByteOrder(handle, id),
        afGetChannels (handle, id),
        sampleFormat, sampleWidth);

    SFPrint("    samplingRate=%.15g, compressionType=%d", 
        afGetRate(handle, id),
        afGetCompression(handle, id));

    afGetPCMMapping(handle, id,
        &slope, &intercept, &minClip, &maxClip);
    SFPrint("    pcmmap: slope=%.15g,intercept=%.15g,minClip=%.15g,maxClip=%.15g", 
        slope, intercept,
        minClip, maxClip);

    dataOffset = afGetDataOffset(handle, id);
    SFPrint("    dataOffset=%d (%X hex)", dataOffset, dataOffset);

    /* 
 * print instrument chunk parameters 
 */
    if ((instrumentCount = afGetInstIDs(handle, NULL)) > 0) {
	Instrument  instrument;
	int    *instrumentIDs;
	int i;

	instrumentIDs = calloc(instrumentCount, sizeof(int));
	afGetInstIDs(handle, instrumentIDs);

	for(i = 0; i < instrumentCount; i++) {
	    SFPrint("\nInstrument %d:", instrumentIDs[i]);
	    ReadInstrumentAIFFParameters(handle, trackID, instrumentIDs[i],
	        &instrument);
	    PrintInstrumentAIFFParameters(&instrument);
	}
	cfree(instrumentIDs);
    }

    /*
 * print miscellaneous data 
 */
    miscellaneousCount = afGetMiscIDs(handle, NULL);
    if (miscellaneousCount)
    {
	Miscellaneous   miscellaneous;
	int        *miscellaneousIDs;
	int            i;

	miscellaneousIDs = (int *) malloc(miscellaneousCount*sizeof(int));
	if (!miscellaneousIDs)
	{
	    SFError("Failed to allocate memory for Miscellaneous Chunk IDs");
	    return;
	}
	afGetMiscIDs(handle, miscellaneousIDs);

	SFPrint("\nMiscellaneous:");
	for (i = 0; i < miscellaneousCount; i++)
	{
	    ReadMiscellaneousData(handle, miscellaneousIDs[i], &miscellaneous);
	    if (miscellaneous.text)
	    {
		PrintMiscellaneousData(&miscellaneous);
		free(miscellaneous.text);
	    }
	}
    }
} /* ---- end PrintSoundFileHandle() ---- */

/* ******************************************************************
 * PrintTrackParameters:   print common audio file parameters,
 *                universal among supported file formats    
 * ****************************************************************** */
void 
PrintTrackParameters(SoundFile *file, int id, char printFormat)
/*  id            audio track# 
    printFormat        SF_NONE, SF_SHORT, SF_NORMAL, SF_LONG
 */
{
    char        dataFormatBuffer[300], dataFormatExtras[300];
    char        s[2000], duration[100], byteSize[100], rateHz[100];
    char        channelString[50], frameString[50];
    double        seconds;
    int        bytes = -1;
    char        *copyRight;
    AFfilehandle    handle;
    Track        *track;

    handle = file->handle;
    track  = file->track;

    track->frameCount = afGetFrameCount(handle, file->trackID);
    seconds = track->frameCount/track->samplingRate;

    /* SF_NORMAL, SF_LONG: print out info in 7-8 lines */
    if (SF_NORMAL == printFormat || SF_LONG == printFormat)
    {
	/* proper grammar code */
	if (1 == track->channelCount)
	    strcpy(channelString, "channel");
	else
	    strcpy(channelString, "channels");

	if        (-1 == track->frameCount)
	    sprintf(frameString, ", unknown frame count");
	else if (1 == track->frameCount)
	    strcpy(frameString, ", 1 frame");
	else
	    sprintf(frameString, ", %d frames", track->frameCount);

	/* format duration : hours:minutes:seconds */
	if (track->frameCount != -1)
	{
	    bytes = (int)(((float)track->frameCount)*track->frameSizeInBytes);
	    DurationInSeconds(seconds, duration, printFormat);
	}
	else
	{
	    if (track->compressionType == AF_COMPRESSION_MPEG1)
	    {
		Aware *aware = &(track->aware);
		struct stat statBuf;
		if (!stat(file->name, &statBuf))
		{
		    /* playing time = fileSize * 8 / bitrate */
		    bytes = statBuf.st_size;
		    seconds = bytes*8;
		    seconds /= (double) aware->bitRateTarget;
		    DurationInSeconds(seconds, duration, printFormat);

		    /* Chance to compute frame count = sampling rate * seconds */
		    track->frameCount = seconds * track->samplingRate;
		    sprintf(frameString, ", %d frames", track->frameCount);
		}
		else
		    strcpy(duration, "unmeasured MPEG bitstream");
	    }
	    else
		strcpy(duration, "Unknown");
	}

	/* format size : Bytes, KBytes, MBytes, GBytes */
	SizeInBytes(bytes, byteSize, printFormat);
	/* format samplingRate : Hz, KHz, MHz, GHz */
	RateInHertz(track->samplingRate, rateHz, printFormat);

	DataFormatName(handle,   id, dataFormatBuffer, printFormat);
	DataFormatExtras(track, dataFormatExtras, printFormat);

	/* if no extras, don't print them */
	if ('\0' == dataFormatExtras[0])
	{
	    sprintf(s, 
	        "File Name      %s\n"
	        "File Format    %s (%s)\n"
	        "Data Format    %s\n"
	        "Audio Data     %s begins at offset %lu (%X hex)\n"
	        "               %u %s%s\n"
	        "Sampling Rate  %s\n"
	        "Duration       %s",
	        file->name, 
	        file->formatName, file->formatLabel,
	        dataFormatBuffer,
	        byteSize, track->dataOffset, track->dataOffset,
	        track->channelCount, channelString, frameString, 
	        rateHz, duration);
	}
	/* print extra line for data format extras */
	else
	{
	    sprintf(s, 
	        "File Name      %s\n"
	        "File Format    %s (%s)\n"
	        "Data Format    %s\n"
	        "               %s\n"
	        "Audio Data     %s begins at offset %lu (%X hex)\n"
	        "               %u %s%s\n"
	        "Sampling Rate  %s\n"
	        "Duration       %s",
	        file->name, 
	        file->formatName, file->formatLabel,
	        dataFormatBuffer,
	        dataFormatExtras,
	        byteSize, track->dataOffset, track->dataOffset,
	        track->channelCount, channelString, frameString, 
	        rateHz, duration);
	}

	/* print copyright string if present.  Print only for SF_NORMAL
    print format, as SF_LONG print format will print copyright
    string in miscellaneous data */
	if (SF_NORMAL == printFormat && (copyRight = FindCopyright(handle)))
	    SFPrint("%s\nCopyright      %s", s, copyRight);
	else 
	    SFPrint("%s", s);
    }

    /* SF_SHORT:    print out info in 1 line, with all entries in fixed column
        positions.  This feature important */
    else if (SF_SHORT == printFormat)
    {
	/* known frame count, as for raw MPEG bitstream */
	if (track->frameCount != -1)
	{
	    /* format duration : hours:minutes:seconds (seconds rounded to milliSeconds) */
	    seconds = (double) ((int) (seconds*1000 + 0.5));
	    DurationInSeconds(seconds*0.001, duration, printFormat);
	}
	else
	{
	    /* MPEG: determine duration and frame count from bitrate and file length */
	    if (track->compressionType == AF_COMPRESSION_MPEG1)
	    {
		Aware *aware = &(track->aware);
		struct stat statBuf;
		if (!stat(file->name, &statBuf))
		{
		    /* playing time = fileSize * 8 / bitrate */
		    bytes    = statBuf.st_size;
		    seconds  = bytes*8;
		    seconds /= (double) aware->bitRateTarget;
		    DurationInSeconds(seconds, duration, printFormat);

		    /* Chance to compute frame count = sampling rate * seconds */
		    track->frameCount = seconds * track->samplingRate;
		    sprintf(frameString, ", %d frames", track->frameCount);
		}
		else
		    strcpy(duration, "? sec");
	    }
	    else
		strcpy(duration, "? sec");
	}

	DataFormatName(handle, id, dataFormatBuffer, printFormat);
	sprintf(s, "%14s %8g Hz %2uch %8.8s %-4.4s %s",
	    duration, 
	    track->samplingRate, 
	    track->channelCount,
	    dataFormatBuffer, 
	    file->formatLabel, 
	    file->name);
	SFPrint("%s", s);
    }
} /* ---- end PrintTrackParameters() ---- */

/* ******************************************************************
 * PrintTrackAIFFParameters:  print AIFF and AIFF-C specific information     
 * ****************************************************************** */
void
PrintTrackAIFFParameters(Track *data)
{
    int    i;

    if ((data->aesDataSeen)||(data->markerCount))
    {
	SFPrint("Audio Track:");

	/* print AES data in hexadecimal (only 1st 4 of 24 bytes) */
	/* althought there are 24 bytes, the other 20 are labelled "Reserved for 
Future Use" in the AES3 specificaiont */
	if (data->aesDataSeen)
	{
	    SFPrint("%sAES channel status data        = [%02x %02x %02x %02x]",
	        spaces, 
	        data->aesData[0], data->aesData[1],
	        data->aesData[2], data->aesData[3]);
	}

	/* need extra spacer onyl when both are present */
	if ((data->aesDataSeen)&&(data->markerCount))
	    SFPrint("");

	/* print track markers */
	if (data->markerCount)
	{
	    SFPrint("%s %d Markers", spaces, data->markerCount);
	    SFPrint("%s   #     Position   Name", spaces);
	    for (i = 0; i < data->markerCount; i++)
	    {
		SFPrint("%s%4d   %10d   \"%s\"",
		    spaces, 
		    data->markerIDs[i], data->markerPositions[i],
		    data->markerNames[i]);
	    }
	}
    }
} /* ---- end PrintTrackAIFFParameters() ---- */

/* ******************************************************************
 * PrintInstrumentAIFFParameters:     
 * ****************************************************************** */
void
PrintInstrumentAIFFParameters(Instrument *data)
{
    int        i;
    char        baseName[40], lowName[40], highName[40];
    static char    loopOff[]                = "(off)";
    static char    loopForward[]            = "(forward)";
    static char    loopForwardAndBackward[] = "(forward&back)";
    static char    longestLoopMode[]        = "              ";
    char        loopMode[MAX_LOOPS][20];


    /* 
 * print keyboard and velocity maps, detune and gain 
 */
    MIDINoteNumberToMusicalNotation((char)data->midiKeyboardMap[LOW], lowName);
    MIDINoteNumberToMusicalNotation((char)data->midiKeyboardMap[HIGH], highName);
    MIDINoteNumberToMusicalNotation((char)data->midiKeyboardMap[BASE], baseName);

    SFPrint("%sKeyboard Map (low..high,base) = %4d .. %4d,%4d (MIDI note#)", 
        spaces, 
        data->midiKeyboardMap[LOW], 
        data->midiKeyboardMap[HIGH], 
        data->midiKeyboardMap[BASE]);

    SFPrint("%s                                %4s .. %4s,%4s (MIDI note name)",
        spaces, lowName, highName, baseName);

    SFPrint("%sVelocity Map (low..high)      = %4d .. %4d",
        spaces, data->midiVelocity[LOW], data->midiVelocity[HIGH]);

    SFPrint("%sDetune                        = %4d Cents",
        spaces, data->detuneInCents);
    SFPrint("%sGain                          = %4d Decibels",
        spaces, data->gainInDecibels);


    /* 
 *  print loops 
 */
    for (i = 0; (i < data->loopCount)&&(i < MAX_LOOPS); i++)
    {
	/* format loops so they fit into constant length string */
	int j = 0;
	switch (data->loopMode[i])
	{
	default:
	case 0:
	    for (; loopOff[j] != '\0'; j++)
		loopMode[i][j] = loopOff[j];
	    break;
	case 1:
	    for (; loopForward[j] != '\0'; j++)
		loopMode[i][j] = loopForward[j];
	    break;
	case 2:
	    for (; loopForwardAndBackward[j] != '\0'; j++)
		loopMode[i][j] = loopForwardAndBackward[j];
	    break;
	}
	while (longestLoopMode[j] != '\0')
	    loopMode[i][j++] = ' ';
	loopMode[i][j] = '\0';
    }

    SFPrint("\n%s                              ----- Sustain -----   ----- Release -----", spaces);
    SFPrint("%sLoop Mode                   =  %4d %s   %4d %s",
        spaces, 
        data->loopMode[LOOP_SUSTAIN], loopMode[LOOP_SUSTAIN],
        data->loopMode[LOOP_RELEASE], loopMode[LOOP_RELEASE]);
    SFPrint("%sLoop Start (Marker#, Frame) = #%4d,%13d   #%4d,%13d",
        spaces, 
        data->loopStartMarkerID[LOOP_SUSTAIN], data->loopStartFrame[LOOP_SUSTAIN],
        data->loopStartMarkerID[LOOP_RELEASE], data->loopStartFrame[LOOP_RELEASE]);
    SFPrint("%sLoop End   (Marker#, Frame) = #%4d,%13d   #%4d,%13d",
        spaces, 
        data->loopEndMarkerID[LOOP_SUSTAIN], data->loopEndFrame[LOOP_SUSTAIN],
        data->loopEndMarkerID[LOOP_RELEASE], data->loopEndFrame[LOOP_RELEASE]);

#ifdef OLDE
    SFPrint("%s    Marker ID (start,end)    = %d [frame %d],%d [frame %d] ",
        spaces, 
        data->loopStartMarkerID [LOOP_SUSTAIN],
        data->loopStartFrame    [LOOP_SUSTAIN],
        data->loopEndMarkerID   [LOOP_SUSTAIN],
        data->loopEndFrame        [LOOP_SUSTAIN]);
#endif
} /* ---- end PrintInstrumentAIFFParameters() ---- */

/* ******************************************************************
 * PrintMiscellaneousData:     Print AIFF/C miscellaneous chunk
 *                (also found in NeXT files, but not yet
 *                supported)
 * ****************************************************************** */
void 
PrintMiscellaneousData(Miscellaneous *data)
{
    switch (data->type)
    {
    case AF_MISC_COPY:
	SFPrint("%sCopyright  = \"%s\"", spaces,  data->text);
	break;
    case AF_MISC_AUTH:
	SFPrint("%sAuthor     = \"%s\"", spaces, data->text);
	break;
    case AF_MISC_ANNO:
	SFPrint("%sAnnotation = \"%s\"", spaces, data->text);
	break;
    case AF_MISC_NAME:
	SFPrint("%sName       = \"%s\"", spaces, data->text);
	break;
    case AF_MISC_MIDI:
	SFPrint("%sMIDI System Exclusive Data: %d bytes", 
	    spaces, data->size);
	break;
    case AF_MISC_APPL:
	SFPrint("%sApplication Specific Data:  %d bytes", 
	    spaces, data->size);
	break;
    case AF_MISC_NeXT:
	SFPrint("%sNeXT-Specific Data:  %d bytes", 
	    spaces, data->size);
	break;
    case AF_MISC_COMMENT:
	SFPrint("%sComment = \"%s\"", spaces, data->text);
	break;
    case AF_MISC_IRCAM_PEAKAMP:
	SFPrint("%sIRCAM Peakamp Data:  %d bytes", 
	    spaces, data->size);
	break;
    case AF_MISC_ICRD:
	SFPrint("%sCreation Data = \"%s\"", spaces, data->text);
	break;
    case AF_MISC_ISFT:
	SFPrint("%sSoftware = \"%s\"", spaces, data->text);
	break;

    case AF_MISC_UNRECOGNIZED:
    default:
	SFError("%s%d bytes of unrecognized data in chunk type=%d",
	    spaces, data->size, data->type);
	break;
    }
} /* ---- end PrintMiscellaneousData() ---- */

/* **********************************************************************
 * MIDINoteNumberToMusicalNotation:    convert MIDI Note to string
 *                    w/musical notation 
 *
 *                range [0..127] -> [C-1..G9], where 
 *                middle C is C4=60. 
 * ********************************************************************** */
char
MIDINoteNumberToMusicalNotation(char noteNumber, char *note)
/* noteNumber        note# to convert
    note        ptr to space for output */
/* FIXXXX: add option to return flats instead of sharps */
{
    char    *ptr;
    int    octave;

    if (noteNumber > 127)
	return (0);

    /* determine note letter */
    ptr = note;
    switch (((int) noteNumber)%12)
    {
    case 0:
	*ptr++ = 'C';
	break;
    case 1:
	*ptr++ = 'C';
	*ptr++ = '#';
	break;
    case 2:
	*ptr++ = 'D';
	break;
    case 3:
	*ptr++ = 'D';
	*ptr++ = '#';
	break;
    case 4:
	*ptr++ = 'E';
	break;
    case 5:
	*ptr++ = 'F';
	break;
    case 6:
	*ptr++ = 'F';
	*ptr++ = '#';
	break;
    case 7:
	*ptr++ = 'G';
	break;
    case 8:
	*ptr++ = 'G';
	*ptr++ = '#';
	break;
    case 9:
	*ptr++ = 'A';
	break;
    case 10:
	*ptr++ = 'A';
	*ptr++ = '#';
	break;
    case 11:
	*ptr++ = 'B';
	break;
    }

    /* determine octave# around C0 = MIDI note #12 */
    octave = ((int) noteNumber)/12 - 1;
    if (octave < 0)
    {
	*ptr++ = '-';
	octave = -octave;
    }
    *ptr++ = '0' + ((char) octave);
    /* terminate string */
    *ptr = '\0';

#ifdef DEBUG_DSPUTIL
    SFPrint("MIDINoteNumberToMusicalNotation() octave=%d", octave);
    SFPrint("MIDINoteNumberToMusicalNotation(): %d -> '%s'", noteNumber, note);
#endif
    return (1);
}    /* ---- end MIDINoteNumberToMusicalNotation() ---- */

/* ***************************************************************************
 * CopyTrackAudioData:        Copy audio track data
 *                Return Boolean success
 * ****************************************************************************/
char
CopyTrackAudioData(AFfilehandle inHandle, AFfilehandle outHandle, 
int id, int framesToTransfer, double fileHeaderSamplingRate, 
char verbose)
{
    void        *buffer;
    int        framesTransferred;
    int        framesRead, framesToRead;
    int        framesWritten, framesToWrite;
    char        done = FALSE;
    char        ok;
    int        channelCount;

    /*printf("CopyTrackAudioData(): framesToTransfer=%d\n", framesToTransfer);*/

    /* allocate buffer of largest libaudiofile data type: double */
    channelCount = afGetVirtualChannels(inHandle, id);
    if (channelCount < 1)
    {
	SFError("Invalid inHandle virtual channel count: %d."
	    "Unable to copy Track Audio Data.", channelCount);
	return (FALSE);
    }

    framesToRead  = 16384;
    buffer = (char *) malloc(framesToRead*channelCount*sizeof(double));
    if (!buffer)
    {
	SFError("Unable to allocate memory for Track Audio Data copy");
	return (FALSE);
    }
    framesTransferred = 0;
    ok = TRUE;
    while (!done & !caughtSignalInterrupt)
    {
	framesRead = afReadFrames(inHandle, id, buffer, framesToRead);
	if (framesRead < 0)
	{
	    SFError("Bad read of audio track data: %s",
	        dmGetError(NULL, NULL));
	    done = TRUE;
	    ok   = FALSE;
	}

	if (framesTransferred < framesToTransfer-framesRead)
	    framesToWrite = framesRead;
	else
	    framesToWrite = framesToTransfer-framesTransferred;

	if (framesToWrite <= 0)
	    break;
	framesWritten      = afWriteFrames(outHandle, id, buffer, framesToWrite);
	framesTransferred += framesWritten;

	/* printf % completed */
	if (verbose)
	    SFPrint("%6.2f%% audio data transferred", 
	        100*((float)framesTransferred/(float)framesToTransfer));

	if (framesWritten < framesToWrite)
	{
	    SFError("Short write copying audio track data: %s",
	        dmGetError(NULL, NULL));
	    done = TRUE;
	    ok   = FALSE;
	}
    }

    /* set value of header sampling rate, which may be specified to be different than
    data sampling rate */
    if (UNSPECIFIED_SAMPLING_RATE != fileHeaderSamplingRate)
	_AFeditrate(outHandle, id, fileHeaderSamplingRate);

    free(buffer);
    return (ok);
}   /* ---- end CopyTrackAudioData() ---- */

/* ***************************************************************************
 * CopyTrackAIFFParameters:    copy AES data and markers.
 *                Allocate memory where necessary
 *                Return Boolean success.
 * ****************************************************************************/
char 
CopyTrackAIFFParameters(Track *inTrack, Track *outTrack)
{
    int    i;
    int    sizeInBytes;

    /* copy AES data */
    outTrack->aesDataSeen = inTrack->aesDataSeen;
    if (outTrack->aesDataSeen)
    {
	for (i = 0; i < 24; i++)
	    outTrack->aesData[i] = inTrack->aesData[i];
    }

    /* set up markers (for AIFF and AIFF-C file formats only) */
    outTrack->markerCount = inTrack->markerCount;
    if (outTrack->markerCount)
    {
	sizeInBytes = outTrack->markerCount*sizeof(int);
	outTrack->markerIDs = (int *) malloc(sizeInBytes);
	if (!outTrack->markerIDs)
	{
	    SFError("No memory for marker IDs.");
	    return (FALSE);
	}
	outTrack->markerPositions = (int *) calloc(outTrack->markerCount, sizeof(int));
	if (!outTrack->markerPositions)
	{
	    free(outTrack->markerIDs);
	    SFError("No memory for marker positions.");
	    return (FALSE);
	}
	bcopy(inTrack->markerIDs, outTrack->markerIDs, sizeInBytes);

	/* allocate and copy marker names */
	outTrack->markerNames = (char **) malloc(outTrack->markerCount*sizeof(char *));
	if (!outTrack->markerNames)
	{
	    free(outTrack->markerPositions);
	    free(outTrack->markerIDs);
	    SFError("No memory for marker names.");
	    return (FALSE);
	}

	for (i = 0; i < outTrack->markerCount; i++)
	{
	    outTrack->markerPositions[i] = inTrack->markerPositions[i];

	    /* strlen dies on null strings, it seems.  So create NULL string */
	    if (!inTrack->markerNames[i])
	    {
		sizeInBytes = strlen(inTrack->markerNames[i])+1;
		outTrack->markerNames[i] = (char *) malloc(sizeInBytes);
		strcpy(outTrack->markerNames[i], inTrack->markerNames[i]);
	    }
	    else
	    {
		outTrack->markerNames[i]    = (char *) malloc(1);
		outTrack->markerNames[i][0] = '\0';
	    }
	}
    }
    return (TRUE);
}   /* ---- end CopyTrackAIFFParameters() ---- */

/* ***************************************************************************
 * CloseSoundFile:    Close file
 *            Return Boolean success
 * ****************************************************************************/
char
CloseSoundFile(SoundFile *file)
{
    if (afCloseFile(file->handle) < 0)
    {
	SFError("Unable to close file '%s'", file->name);
	return (FALSE);
    }
    file->handle = AF_NULL_FILEHANDLE;
    return (TRUE);
}   /* ---- end CloseSoundFile() ---- */

/* ***************************************************************************
 * CleanUpSoundFile:    Free members of SoundFile and close file
 *            Return Boolean success
 * ****************************************************************************/
char
CleanUpSoundFile(SoundFile *file)
{
    int i;
    if (file->setUp) afFreeFileSetup(file->setUp);
    file->setUp = NULL;
    if (file->track) free(file->track);
    file->track = NULL;
    if(file->instrumentIDs) free(file->instrumentIDs);
    file->instrumentIDs = NULL;
    if(file->instruments) {
	for(i = 0; i < file->instrumentCount; i++)
	    if(file->instruments[i]) free(file->instruments[i]);
	free(file->instruments);
    }
    file->instruments = NULL;
    if(file->miscellaneousIDs) free(file->miscellaneousIDs);
    file->miscellaneousIDs = NULL;
    if(file->miscellaneous) {
	for(i = 0; i < file->miscellaneousCount; i++)
	    if(file->miscellaneous[i]) free(file->miscellaneous[i]);
	free(file->miscellaneous);
    }
    file->miscellaneous = NULL;
    if (!CloseSoundFile(file))
	return (FALSE);
    return (TRUE);
}   /* ---- end CleanUpSoundFile() ---- */

/* **********************************************************************
 * PrintError:        print out error message
 * **********************************************************************/
void
PrintSFError(Options *options, const char *t)
{
    static char myname[80];

    if (options)
	strcpy(myname, options->applicationName);
    else if (t)
	fprintf(stderr, "%s: %s\n", myname, t);
}   /* ---- end PrintSFError() ---- */

/* **********************************************************************
 * CatchAFError:        error-reporting callback routine for audio file library
 * ***********************************************************************/
void
CatchAFError(long code, const char *s)
{
    static char t[512];

    sprintf(t, "Audio File Library error (%d) %s", code, s);
    PrintSFError(NULL, t);
}   /* ---- end CatchAFError() ---- */

/* **********************************************************************
 * InitErrorReporting:
 * ***********************************************************************/
void
InitErrorReporting(Options *options)
{
    afSetErrorHandler(CatchAFError);
    PrintSFError(options, NULL);
}   /* ---- end InitErrorReporting() ---- */

/* **********************************************************************
 * OnSignalInterrupt:
 * ***********************************************************************/
void 
OnSignalInterrupt()
{
    caughtSignalInterrupt++;
}   /* ---- end OnSignalInterrupt() ---- */

/* ***********************************************************************
 * CheckAwareEncoderLicense:
 *                Return Boolean success
 * ************************************************************************/
char
CheckAwareEncoderLicense(SoundFile *file, char verbose)
{
    char    *message;
    int    license;

    if (AF_COMPRESSION_MPEG1 == file->track->compressionType)
    {
	if (verbose)
	    SFPrint("Checking license for Aware encoder ...");

	AUchecklicense(AU_LICENSE_SGI_MPEG_ENCODER, &license, &message);
	if (verbose)
	    SFPrint("done");

	if (license != AU_LICENSE_OK)
	{
	    SFError("Permission denied to use Aware MPEG encoder");
	    SFError("%s", message);
	    return (FALSE);
	}
    }
    else if (AF_COMPRESSION_AWARE_MULTIRATE == file->track->compressionType)
    {
	if (verbose)
	    SFPrint("Checking license for Aware encoder ...");

	AUchecklicense(AU_LICENSE_AWARE_MULTIRATE_ENCODER, &license, &message);
	if (verbose)
	    SFPrint("Done");

	if (license != AU_LICENSE_OK)
	{
	    SFError("Permission denied to use Aware MultiRate encoder.");
	    SFError("%s", message);
	    return (FALSE);
	}
    }
    return (TRUE);
}   /* ---- end CheckAwareEncoderLicense() ---- */

/* ***************************************************************************
 * CheckAwareDecoderLicense:
 * ****************************************************************************/
char
CheckAwareDecoderLicense(SoundFile *file, char verbose)
{
    char    *message;
    int    license;

    if (AF_COMPRESSION_MPEG1 == file->track->compressionType)
    {
	if (verbose)
	    SFPrint("Checking license for Aware decoder ...");

	AUchecklicense(AU_LICENSE_SGI_MPEG_DECODER, &license, &message);
	if (verbose)
	    SFPrint("done");

	if (license != AU_LICENSE_OK)
	{
	    SFError("Permission denied to use Aware MPEG decoder");
	    SFError("%s", message);
	    return (FALSE);
	}
    }
    else if (AF_COMPRESSION_AWARE_MULTIRATE == file->track->compressionType)
    {
	if (verbose)
	    SFPrint("Checking license for Aware decoder ...");

	AUchecklicense(AU_LICENSE_AWARE_MULTIRATE_DECODER, &license, &message);
	if (verbose)
	    SFPrint("done");

	if (license != AU_LICENSE_OK)
	{
	    SFError("Permission denied to use Aware MultiRate decoder");
	    SFError("%s", message);
	    return (FALSE);
	}
    }
    return (TRUE);
}   /* ---- end CheckAwareDecoderLicense() ---- */

/* ***************************************************************************
 * OpenInSoundFile:   open file, recognize format.
 *             Return Boolean success
 * ****************************************************************************/
int
OpenInSoundFile(SoundFile *file)
{
    file->fd = open(file->name, file->modeCode);
    if (file->fd < 0)
    {
	int errorNumber = errno;
	SFError("Failed to open input file '%s'. %s.", file->name, strerror(errorNumber));
	return (FALSE);
    }

    /* identify file format if input file not specified as raw */
    if (file->formatID != AF_FILE_RAWDATA)
    {
	int supported = 0;
	file->formatID = afIdentifyNamedFD(file->fd, file->name, &supported);
	if (file->formatID == AF_FILE_UNKNOWN ||
	    file->formatID == AF_FILE_UNSUPPORTED)
	{
	    SFError("Failed to recognize '%s' sound file format.", 
	        file->name);
	    close(file->fd);
	    return (FALSE);
	}
	else
	{
	    /* see if format is implemented in the library */
	    if (!afQueryLong(AF_QUERYTYPE_FILEFMT, 
	        AF_QUERY_IMPLEMENTED, file->formatID, 0, 0))
	    {
		SFError("File '%s': this program does not yet support "
		    "'%s' format.",
		    file->name,
		    afQueryPointer(AF_QUERYTYPE_FILEFMT, 
		    AF_QUERY_NAME, file->formatID, 0, 0));
		close(file->fd);
		return (FALSE);
	    }
	    /* we're ok ! */
	}
    }

    /* acquire file handle */
    if (file->formatID != AF_FILE_RAWDATA)
    {
	file->handle = afOpenNamedFD(file->fd, file->modeString,
	    AF_NULL_FILESETUP, file->name);
	if (AF_NULL_FILEHANDLE != file->handle)
	    afGetTrackIDs(file->handle, &file->trackID);
    }
    else
    {
	int    id;
	Track *track = file->track;
	file->trackID = AF_DEFAULT_TRACK;
	id = file->trackID;

	if (UNSPECIFIED_BYTE_ORDER == track->byteOrder)
	    track->byteOrder = AF_BYTEORDER_BIGENDIAN;
	if (UNSPECIFIED_CHANNEL_COUNT == track->channelCount)
	    track->channelCount = 1;
	if (UNSPECIFIED_SAMPLE_FORMAT == track->sampleFormat)
	    track->sampleFormat = AF_SAMPFMT_TWOSCOMP;
	if (UNSPECIFIED_SAMPLE_WIDTH == track->sampleWidth)
	    track->sampleWidth = 8;
	if (UNSPECIFIED_SAMPLING_RATE == track->samplingRate)
	    track->samplingRate = 8000;
	if (UNSPECIFIED_COMPRESSION == track->compressionType)
	    track->compressionType = AF_COMPRESSION_NONE;

	if (UNSPECIFIED_FRAME_COUNT != track->frameCount)
	    afInitFrameCount(file->setUp, id, track->frameCount);

	afInitDataOffset    (file->setUp, id, track->dataOffset);
	afInitFileFormat    (file->setUp, file->formatID);
	afInitByteOrder    (file->setUp, id, track->byteOrder);
	afInitChannels    (file->setUp, id, track->channelCount);
	afInitSampleFormat    (file->setUp, id, track->sampleFormat, track->sampleWidth);
	afInitRate        (file->setUp, id, track->samplingRate);
	afInitCompression    (file->setUp, id, track->compressionType);

	if (track->pcmMapSpecified)
	    afInitPCMMapping(file->setUp, id, 
	        track->pcmMapSlope, track->pcmMapIntercept, 
	        track->pcmMapMinClip, track->pcmMapMaxClip);

	file->handle = afOpenNamedFD(file->fd, file->modeString,
	    file->setUp, file->name);

#ifdef SAFE
	printf("OpenInSoundFile(): format=%d\n", file->formatID);
	printf("OpenInSoundFile(): byteOrder=%d\n", track->byteOrder);
	printf("OpenInSoundFile(): channelCount=%d\n", track->channelCount);
	printf("OpenInSoundFile(): dataOffset=%d\n", track->dataOffset);
	printf("OpenInSoundFile(): sampleFormat=%d\n", track->sampleFormat);
	printf("OpenInSoundFile(): sampleWidth=%d\n", track->sampleWidth);
	printf("OpenInSoundFile(): samplingRate=%.15g\n", track->samplingRate);
	printf("OpenInSoundFile(): compressionType=%d\n", track->compressionType);
	printf("OpenInSoundFile(): pcmmap specified=%d: slope=%g,intercept=%g,min=%g,max=%g\n", 
	    track->pcmMapSpecified, track->pcmMapSlope, track->pcmMapIntercept, 
	    track->pcmMapMinClip, track->pcmMapMaxClip);
#endif 
    }

#ifdef SAFE
    printf("OpenInSoundFile(): fd=%d,handle=%d\n", file->fd, file->handle);
    printf("OpenInSoundFile(): modeCode=%X,modeString='%s'\n", file->modeCode, file->modeString);
#endif

    if (AF_NULL_FILEHANDLE == file->handle)
    {
	SFError("Failed to attach an audio file struct to '%s': %s",
	    file->name, dmGetError(NULL, NULL));
	close(file->fd);
	return (FALSE);
    }

    /* set some virtual parameters that don't seem to take automatically */
    if (AF_NULL_FILEHANDLE != file->handle)
    {
	afSetVirtualByteOrder(file->handle, file->trackID, AF_BYTEORDER_BIGENDIAN);

	file->mayHaveMiscs = (afQueryLong(AF_QUERYTYPE_MISC, AF_QUERY_TYPE_COUNT,
	    file->formatID, 0, 0) > 0);
	file->mayHaveExtras = afQueryLong(AF_QUERYTYPE_INST, AF_QUERY_SUPPORTED,
	    file->formatID, 0, 0);
	switch (file->formatID)
	{
	case AF_FILE_AIFF:
	case AF_FILE_AIFFC:
	    file->isAIFForC = TRUE;
	    afGetFileFormat(file->handle, &file->formatVersion);
	    break;
	default:
	    break;
	}
    }

    file->formatLabel = afQueryPointer(AF_QUERYTYPE_FILEFMT, AF_QUERY_LABEL, file->formatID, 0, 0);
    file->formatName = afQueryPointer(AF_QUERYTYPE_FILEFMT, AF_QUERY_DESC, file->formatID, 0, 0);

#ifdef SAFE
    printf("OpenInSoundFile(): file->formatID=%d\n", file->formatID);
    printf("OpenInSoundFile(): file->formatLabel='%s'\n", file->formatLabel);
    printf("OpenInSoundFile(): file->formatName='%s'\n", file->formatName);
#endif
    return (TRUE);
}   /* ---- end OpenInSoundFile() ---- */

/* ***************************************************************************
 * OpenOutSoundFile: create output file and set its virtual AF parameters 
 *            Return Boolean success.
 * ****************************************************************************/
int
OpenOutSoundFile(SoundFile *outFile, SoundFile *modelFile, char *matrixFile /* NULL if none */)
{
    AFfilehandle    handle;
    Track        *modelTrack;
    int        id;

    outFile->fd = open(outFile->name, outFile->modeCode, 0666);
    if (outFile->fd < 0)
    {
	int errorNumber = errno;
	SFError("Failed to create output file '%s'. %s.", 
	    outFile->name, strerror(errorNumber));
	return (FALSE);
    }

    outFile->handle = afOpenFile(outFile->name, outFile->modeString, outFile->setUp);
    if (AF_NULL_FILEHANDLE == outFile->handle)
    {
	SFError("Failed to create output file '%s': %s", outFile->name,
	    dmGetError(NULL, NULL));
	return (FALSE);
    }

    handle     = outFile->handle;
    modelTrack = modelFile->track;
    afGetTrackIDs(outFile->handle, &outFile->trackID);
    id = outFile->trackID;

#ifdef PRINT_IN_TRACK
    printf("OpenOutSoundFile(): track: byteOrder=%d\n", modelTrack->byteOrder);
    printf("track: channelCount=%d\n", modelTrack->channelCount);
    printf("track: sampleFormat=%d\n", modelTrack->sampleFormat);
    printf("track: sampleWidth=%d\n", modelTrack->sampleWidth);
    printf("track: samplingRate=%.15g\n", modelTrack->samplingRate);

    printf("track: compressionType=%d\n", modelTrack->compressionType);
    printf("track: slope=%.15g,intercept=%.15g\n", 
        modelTrack->slope, modelTrack->intercept);
    printf("track: minClip=%.15g,maxClip=%.15g\n", 
        modelTrack->minClip, modelTrack->maxClip);
#endif

    /* 
 * configure virtual parameters:  format read from input file 
 */
    afSetVirtualByteOrder(handle, id, AF_BYTEORDER_BIGENDIAN);
    afSetVirtualChannels(handle, id, modelTrack->channelCount);

    if (matrixFile)
    {
	double *matrix;
	int vchans = afGetVirtualChannels(handle, id);
	int fchans = afGetChannels(handle, id);
	ZAP();
	if (!ReadChannelMatrixFile(matrixFile, vchans, fchans, &matrix))
	    return(FALSE);
	ZAP();
	afSetChannelMatrix(handle, id, matrix);
	ZAP();
	free(matrix);
	ZAP();
    }

    afSetVirtualSampleFormat(handle, id, modelTrack->sampleFormat, modelTrack->sampleWidth);
    afSetVirtualRate(handle, id, modelTrack->samplingRate);

    if (modelTrack->pcmMapSpecified)
	afSetVirtualPCMMapping(handle, id,
	    modelTrack->pcmMapSlope, modelTrack->pcmMapIntercept,
	    modelTrack->pcmMapMinClip, modelTrack->pcmMapMaxClip);

#ifdef SAFE
    if (modelTrack->pcmMapSpecified)
	printf("specified, virtual pcm: slope=%g, inter=%g, min=%g, max=%g\n",
	    modelTrack->pcmMapSlope, modelTrack->pcmMapIntercept,
	    modelTrack->pcmMapMinClip, modelTrack->pcmMapMaxClip);
    printf("name='%s'\n", outFile->name);
    printf("fd=%d,handle=%d\n", outFile->fd, outFile->handle);
    printf("modeCode=%X,modeString='%s'\n", outFile->modeCode, outFile->modeString);
#endif

#ifdef TESTSUITE
    if ((outFile->formatID == AF_FILE_AIFFC ||
	 outFile->formatID == AF_FILE_AIFF)
	 && outFile->instrumentCount > 0)
    {
	afSetLoopStart(handle, AF_DEFAULT_INST, 1, 1);
	afSetLoopEnd(handle, AF_DEFAULT_INST, 1, 1);
	afSetLoopStart(handle, AF_DEFAULT_INST, 2, 1);
	afSetLoopEnd(handle, AF_DEFAULT_INST, 2, 1);
    }
#endif
    return (TRUE);
}   /* ---- end OpenOutSoundFile() ---- */

/* ***************************************************************************
 * BasicTrack:    initialize with invalid values
 * ****************************************************************************/
void 
BasicTrack(Track *track)
{
    int        i;

    track->channelCount = UNSPECIFIED_CHANNEL_COUNT;
    track->frameCount   = UNSPECIFIED_FRAME_COUNT;
    track->frameSizeSpecified = FALSE;
    track->frameSizeInBytes = -1;
    track->sampleFormat = UNSPECIFIED_SAMPLE_FORMAT;
    track->sampleWidth  = UNSPECIFIED_SAMPLE_WIDTH;
    if (track->sampleWidth == 24)
	track->bytesPerSample = 4;
    else
	track->bytesPerSample = track->sampleWidth / 8;
    track->samplingRate = UNSPECIFIED_SAMPLING_RATE;

    track->byteOrder  = UNSPECIFIED_BYTE_ORDER;
    track->dataOffset = 0;

    track->pcmMapSpecified        = FALSE;
    track->pcmMapSlopeSpecified     = FALSE;
    track->pcmMapInterceptSpecified = FALSE;
    track->pcmMapMinClipSpecified   = FALSE;
    track->pcmMapMaxClipSpecified   = FALSE;
    track->pcmMapSlope     = 1;
    track->pcmMapIntercept = 0;
    track->pcmMapMinClip   = 0;
    track->pcmMapMaxClip   = 0;

    track->compressionType = UNSPECIFIED_COMPRESSION;
    track->compressionName = NULL;

    /* Aware compression parameters */
    track->aware.layer          = -1;
    track->aware.defaultConfiguration = -1;
    track->aware.bitRateTargetSpecified = FALSE;
    track->aware.channelPolicySpecified = FALSE;
    track->aware.bitRatePolicySpecified = FALSE;
    track->aware.constantQualityNoiseToMaskRatioSpecified = FALSE;

    /* initialize markers */
    track->markerCount    = 0;
    track->markerIDs    = NULL;
    track->markerPositions    = NULL;
    track->markerNames    = NULL;

    /* initialize AES data */
    track->aesDataSeen = FALSE;
    for (i = 0; i < 24; i++)
	track->aesData[i] = 0;
}   /* ---- end BasicTrack() ---- */

/* ***************************************************************************
 * BasicInstrument:    initialize with invalid values
 * ****************************************************************************/
void 
BasicInstrument(Instrument *data)
{
    int    i;

    data->midiKeyboardMap[LOW]  = -1;
    data->midiKeyboardMap[HIGH] = -1;
    data->midiKeyboardMap[BASE] = -1;
    data->midiVelocity[LOW]        = -1;
    data->midiVelocity[HIGH]    = -1;

    data->gainInDecibels = 0;
    data->detuneInCents = 0;

    data->loopCount = 0;
    for (i = 0; i < MAX_LOOPS; i++)
    {
	data->loopIDs        [i] = -1;
	data->loopMode        [i] = -1;
	data->loopStartMarkerID [i]    = -1;
	data->loopEndMarkerID   [i]    = -1;
	data->loopStartFrame    [i] = -1;
	data->loopEndFrame        [i] = -1;
    }
}   /* ---- end BasicInstrument() ---- */

/* ***************************************************************************
 * BasicMiscellaneous:    allocate and initialize new data structure.
 *                if model provided,  initialize with contents 
 *                of model
 * ****************************************************************************/
void 
BasicMiscellaneous(Miscellaneous *data)
{
    data->type = AF_MISC_UNRECOGNIZED;
    data->size = 0;
    data->text = NULL;
}   /* ---- end BasicMiscellaneous() ---- */

/* ***************************************************************************
 * BasicSoundFile:    initialize with invalid values
 * ****************************************************************************/
void 
BasicSoundFile(SoundFile *file)
{
    /* initialize flags */
    file->fd = -1;
    file->name = NULL;

    file->modeCode = O_RDONLY;
    file->modeString[0] = 'r';  /* "r" */
    file->modeString[1] = '\0';
    file->handle = NULL;
    file->setUp  = afNewFileSetup();

    file->formatID      = UNSPECIFIED_FILE_FORMAT;
    file->formatLabel   = NULL;
    file->formatName    = NULL;
    file->formatVersion = UNSPECIFIED_FILE_FORMAT_VERSION;
    file->isAIFForC     = FALSE;
    file->mayHaveExtras = FALSE;
    file->mayHaveMiscs  = FALSE;

    /* initialize Track flags */
    file->trackCount = 0;
    file->trackID    = -1;
    file->track      = NewTrack(NULL);

    /* initialize Instrument flags */
    file->instrumentCount = 0;
    file->instrumentIDs    = NULL;
    file->instruments      = NULL;

    /* initialize Miscellaneous flags */
    file->miscellaneousCount = 0;
    file->miscellaneousIDs   = NULL;
    file->miscellaneous     = NULL;
}   /* ---- end BasicSoundFile() ---- */

/* ***************************************************************************
 * NewTrack:        allocate and initialize new data structure.
 *            If model provided, initialize new data structure 
 *            with contents of model
 * ****************************************************************************/
Track * 
NewTrack(Track *model)
{
    int    i;
    Track    *data = (Track *) malloc(sizeof(Track));
    int    size;

    if (!data)
	return (NULL);

    /* if no model supplied, return base values */
    if (!model)
    {
	BasicTrack(data);
	return (data);
    }

    /* replicate model (handles Aware structure aswell) */
    bcopy(model, data, sizeof(Track));
    data->compressionName = StringDuplicate(model->compressionName);

    /* replicate marker IDs, positions and names */
    size = model->markerCount*sizeof(int);
    data->markerIDs       = (int *) malloc(size);
    data->markerPositions = (int *) malloc(size);
    data->markerNames     = (char **) malloc(model->markerCount*sizeof(char *));
    for (i = 0; i < model->markerCount; i++)
    {
	data->markerIDs     [i] = model->markerIDs[i];
	data->markerPositions[i] = model->markerPositions[i];
	data->markerNames     [i] = StringDuplicate(model->markerNames[i]);
    }
    return (data);
}   /* ---- end NewTrack() ---- */

/* ***************************************************************************
 * NewInstrument:    allocate and initialize new data structure.
 *            If model provided, initialize new data structure 
 *            with contents of model
 * ****************************************************************************/
Instrument * 
NewInstrument(Instrument *model)
{
    Instrument *data = (Instrument *) malloc(sizeof(Instrument));

    if (!data)
	return (NULL);

    /* if no model supplied, return base values */
    if (!model)
    {
	BasicInstrument(data);
	return (data);
    }

    /* replicate model */
    bcopy(model, data, sizeof(Instrument));
    return (data);
}   /* ---- end NewInstrument() ---- */

/* ***************************************************************************
 * NewMiscellaneous:    allocate and initialize new data structure.
 *            If model provided, initialize new data structure 
 *            with contents of model
 * ****************************************************************************/
Miscellaneous * 
NewMiscellaneous(Miscellaneous *model)
{
    Miscellaneous *data = (Miscellaneous *) malloc(sizeof(Miscellaneous));

    if (!data)
	return (NULL);

    /* if no model supplied, return base values */
    if (!model)
    {
	BasicMiscellaneous(data);
	return (data);
    }

    /* replicate model */
    data->size = model->size;
    data->type = model->type;
    /* text chunks are not necessarily strings, 
    so can't use strdup() to replicate */
    if (data->size)
    {
	data->text = (char *) malloc(data->size);
	bcopy(model->text, data->text, model->size);
    }
    else
	data->text = NULL;

    return (data);
}   /* ---- end NewMiscellaneous() ---- */

/* ***************************************************************************
 * NewSoundFile:    allocate and initialize new data structure.
 *            if model provided,  initialize new data structure 
 *            with contents of model
 * ****************************************************************************/
SoundFile * 
NewSoundFile(SoundFile *model)
{
    int        i;
    SoundFile    *file = (SoundFile *) malloc(sizeof(SoundFile));
    int        size;

    if (!file)
	return (NULL);

    /* if no model supplied, return base values */
    if (!model)
    {
	BasicSoundFile(file);
	return (file);
    }

    /* replicate model */
    bcopy(model, file, sizeof(SoundFile));

    /* allocate new file setup */
    file->setUp = afNewFileSetup();
    /* replicate strings */
    file->name      = StringDuplicate(model->name);
    file->formatLabel = StringDuplicate(model->formatLabel);
    file->formatName  = StringDuplicate(model->formatName);

    /* replicate Track */
    file->track = NewTrack(model->track);

    /* replicate Instruments */
    file->instruments = calloc(model->instrumentCount, sizeof(Instrument **));
    for(i = 0; i < model->instrumentCount; i++)
	file->instruments[i] = NewInstrument(model->instruments[i]);

    file->instrumentCount = model->instrumentCount;

    /* replicate Miscellaneous IDs */
    size = model->miscellaneousCount*sizeof(int);
    file->miscellaneousIDs = (int *) malloc(size);
    bcopy(model->miscellaneousIDs, file->miscellaneousIDs, size);

    /* replicate Miscellaneous chunks */
    size = model->miscellaneousCount*sizeof(int *);
    file->miscellaneous = (Miscellaneous **) malloc(size);
    for (i = 0; i < model->miscellaneousCount; i++)
	file->miscellaneous[i] = NewMiscellaneous(model->miscellaneous[i]);

    return (file);
}   /* ---- end NewSoundFile() ---- */

/* ***************************************************************************
 * SetUpOutSoundFile:    initialize an AFfilesetup for basic, track, instrument 
 *            and miscellaneous data 
 * ****************************************************************************/
void 
SetUpOutSoundFile(AFfilesetup setUp, SoundFile *inFile, SoundFile *outFile, 
int applicationID)
{
    int    i;
    int    count;

    afInitFileFormat(setUp, outFile->formatID);

    /* transfer and set universal track parameters */
    outFile->trackCount = inFile->trackCount;
    outFile->trackID = inFile->trackID;
    afInitTrackIDs(setUp, &outFile->trackID, outFile->trackCount);
    /* FIXXXX: SetUpTrackParameters() needs separated data transfer and AFinit()'s */
    SetUpTrackParameters(setUp, outFile->trackID, inFile->track, outFile, applicationID);

    /* override libaudiofile defaults:  prevent addition of
    chunks not present in input file to output file */
    afInitMarkIDs(setUp, outFile->trackID, NULL, 0);
    afInitInstIDs(setUp, NULL, 0);
    afInitMiscIDs(setUp, NULL, 0);

    /* Transfer instruments when input and output files BOTH may have them */
    if (inFile->mayHaveExtras && outFile->mayHaveExtras)
    {
	Track *inTrack = inFile->track;
	Track *outTrack = outFile->track;

	/* transfer track AIFF markers */
	if (count = outTrack->markerCount = inTrack->markerCount)
	{
	    outTrack->markerIDs = (int *) malloc(count*sizeof(int));
	    bcopy(inTrack->markerIDs, outTrack->markerIDs, count*sizeof(int));
	    outTrack->markerPositions = (int *) malloc(count*sizeof(int));
	    bcopy(inTrack->markerPositions, outTrack->markerPositions, count*sizeof(int));

	    outTrack->markerNames = (char **) malloc(count*sizeof(char *));
	    for (i = 0; i < count; i++)
		outTrack->markerNames[i] = StringDuplicate(inTrack->markerNames[i]);
	}

	/* transfer track AES data */
	if (outTrack->aesDataSeen = inTrack->aesDataSeen)
	{
	    for (i = 0; i < 24; i++)
		outTrack->aesData[i] = inTrack->aesData[i];
	}

	/* set track AIFF data */
	SetUpTrackAIFFParameters(setUp, outFile->trackID, outFile->track);

	/* transfer and set instrument data */
	if ((outFile->instrumentCount = inFile->instrumentCount) > 0)
	{
	    int i;
	    outFile->instruments = calloc(outFile->instrumentCount,
	        sizeof(Instrument **));
	    outFile->instrumentIDs = calloc(outFile->instrumentCount,
					    sizeof(int));
	    for(i = 0; i < inFile->instrumentCount; i++)
		outFile->instrumentIDs[i] = inFile->instrumentIDs[i];

	    afInitInstIDs(setUp, outFile->instrumentIDs,
	        outFile->instrumentCount);

	    for(i = 0; i < inFile->instrumentCount; i++) {
		outFile->instruments[i] = NewInstrument(inFile->instruments[i]);
		SetUpInstrumentAIFFParameters(setUp, outFile->instrumentIDs[i],
		    outFile->instruments[i]);
	    }
	}
    }

    /* Copy ALL miscellaneous chunks when input file {AIFF,AIFC} and 
    output file {AIFF,AIFC} */
    if (inFile->isAIFForC && outFile->isAIFForC) {
	/* transfer and set miscellaneous data */
	if (count = outFile->miscellaneousCount = inFile->miscellaneousCount)
	{
	    outFile->miscellaneousIDs = (int *) malloc(count*sizeof(int));
	    bcopy(inFile->miscellaneousIDs, outFile->miscellaneousIDs, count*sizeof(int));
	    outFile->miscellaneous = (Miscellaneous **) malloc(count*sizeof(int *));
	    afInitMiscIDs(setUp, outFile->miscellaneousIDs, count);
	    for (i = 0; i < count; i++)
	    {
		outFile->miscellaneous[i] = NewMiscellaneous(inFile->miscellaneous[i]);
		SetUpMiscellaneousData(setUp, outFile->miscellaneousIDs[i], 
		    outFile->miscellaneous[i]);
	    }
	}
    }
    else if (inFile->mayHaveMiscs) {
	if(AF_FILE_WAVE == outFile->formatID) {
	    /* transfer and set compatible miscellaneous data */
	    if (count = outFile->miscellaneousCount = inFile->miscellaneousCount) {
		int validMiscs = 0;
		int j = 0;
		outFile->miscellaneousIDs = (int *) malloc(count*sizeof(int));
		for (i = 0; i < count; i++)
		    if (AF_MISC_COPY == inFile->miscellaneous[i]->type
		        || AF_MISC_AUTH == inFile->miscellaneous[i]->type
		        || AF_MISC_NAME == inFile->miscellaneous[i]->type
		        || AF_MISC_COMMENT == inFile->miscellaneous[i]->type)
		    {
			validMiscs++;
			outFile->miscellaneousIDs[i] = inFile->miscellaneousIDs[i];
		    }
		outFile->miscellaneous = (Miscellaneous **) malloc(validMiscs*sizeof(int *));
		outFile->miscellaneousCount = validMiscs;
		afInitMiscIDs(setUp, outFile->miscellaneousIDs, validMiscs);
		for (i = 0; i < count; i++) {
		    if (AF_MISC_COPY == inFile->miscellaneous[i]->type
		        || AF_MISC_AUTH == inFile->miscellaneous[i]->type
		        || AF_MISC_NAME == inFile->miscellaneous[i]->type
		        || AF_MISC_COMMENT == inFile->miscellaneous[i]->type)
		    {
			outFile->miscellaneous[j] = NewMiscellaneous(inFile->miscellaneous[i]);
			SetUpMiscellaneousData(setUp, outFile->miscellaneousIDs[j], 
			    outFile->miscellaneous[j]);
			j++;
		    }
		}
	    }
	}

	/* copy single miscellaneous copyright, NeXT-specific, or comment chunk
    when input file supports miscs and output file {NeXT,IRCAM} */
	else if(AF_FILE_NEXTSND == outFile->formatID
	    || AF_FILE_IRCAM == outFile->formatID)
	{
	    /* transfer and set miscellaneous data */
	    if (inFile->miscellaneousCount) {
		/* since NeXT format accepts only the NeXT-specfic chunk, check 
        if any of the miscellaneous chunks can be converted.  If one found,
        do it.
        For now, this will have to do for IRCAM files as well.
        */

		char haveChunk = FALSE;
		for (i = 0; i < inFile->miscellaneousCount; i++) {
		    if (AF_MISC_NeXT == inFile->miscellaneous[i]->type
		        || AF_MISC_COPY == inFile->miscellaneous[i]->type
		        || AF_MISC_COMMENT == inFile->miscellaneous[i]->type)
		    {
			haveChunk = TRUE;
			break;
		    }
		}

		if (haveChunk) {
		    int id = inFile->miscellaneousIDs[i];
		    int miscType;
		    switch(outFile->formatID) {
		    case AF_FILE_NEXTSND:
			miscType = AF_MISC_NeXT;
			break;
		    case AF_FILE_IRCAM:
			miscType = AF_MISC_COMMENT;
			break;
		    default:
			break;
		    }
		    outFile->miscellaneousCount  = 1;
		    outFile->miscellaneousIDs    = (int *) malloc(sizeof(int));
		    outFile->miscellaneousIDs[0] = id;
		    outFile->miscellaneous    = (Miscellaneous **) malloc(sizeof(int *));
		    /* this has to be reset to allow "misc conversion" 
           we can get away with this since this value is not used
           anywhere else.
        */
		    inFile->miscellaneous[i]->type = miscType;
		    outFile->miscellaneous[0] = NewMiscellaneous(inFile->miscellaneous[i]);
		    afInitMiscIDs(setUp, &id, 1);
		    SetUpMiscellaneousData(setUp, id, inFile->miscellaneous[i]);
		}

	    }
	}
    }
}   /* ---- end SetUpOutSoundFile() ---- */

/* ***************************************************************************
 * SetUpTrackParameters:
 * ****************************************************************************/
void 
SetUpTrackParameters(AFfilesetup setUp, int id, Track *inTrack, SoundFile *outFile, 
int applicationID)
{
    int        counter;
    AUpvlist    pvlist;
    Track        *outTrack;
    Aware        *outAware;

    outTrack = outFile->track;
    outAware = &(outFile->track->aware);

    /* set up channels */
    if (UNSPECIFIED_CHANNEL_COUNT == outTrack->channelCount)
	outTrack->channelCount = inTrack->channelCount;
    afInitChannels(setUp, id, outTrack->channelCount);

    /* set up sample format */
    if (UNSPECIFIED_SAMPLE_WIDTH == outTrack->sampleWidth)
	outTrack->sampleWidth = inTrack->sampleWidth;
    if (UNSPECIFIED_SAMPLE_FORMAT == outTrack->sampleFormat)
	outTrack->sampleFormat = inTrack->sampleFormat;
    afInitSampleFormat(setUp, id, outTrack->sampleFormat, outTrack->sampleWidth);

    /* set up output sampling rate.  Header sampling rate may be different 
 * from actual data sampling rate and will be adjusted AFTER CopyTrackAudioData() */
    if (UNSPECIFIED_SAMPLING_RATE == outTrack->samplingRate)
	outTrack->samplingRate = inTrack->samplingRate;
    afInitRate(setUp, id, outTrack->samplingRate);

    /* WAVE files (from Intel CPU machines) use little endian byte order */
    if (UNSPECIFIED_BYTE_ORDER == outTrack->byteOrder)
	outTrack->byteOrder = inTrack->byteOrder;
    if (AF_FILE_WAVE == outFile->formatID
        || AF_FILE_SAMPLEVISION == outFile->formatID
        || AF_FILE_SOUNDFONT2 == outFile->formatID)
	outTrack->byteOrder = AF_BYTEORDER_LITTLEENDIAN;
    afInitByteOrder(setUp, id, outTrack->byteOrder);

    /* setup data offset */
    afInitDataOffset(setUp, id, 0);

#ifdef SAFE
    printf("SetUpTrackParameters(): specified=%d, slope=%g, inter=%g, min=%g, max=%g\n",
        outTrack->pcmMapSpecified,
        outTrack->pcmMapSlope, outTrack->pcmMapIntercept,
        outTrack->pcmMapMinClip, outTrack->pcmMapMaxClip);
#endif

    /* setup pcm mapping */
    if (outTrack->pcmMapSpecified)
	afInitPCMMapping(setUp, id, 
	    outTrack->pcmMapSlope, outTrack->pcmMapIntercept, 
	    outTrack->pcmMapMinClip, outTrack->pcmMapMaxClip);

    /* initialize compression FIXXXXX  this could be wrong */
    if (UNSPECIFIED_COMPRESSION == outTrack->compressionType)
    {
	switch (applicationID)
	{
	case AIFC_RESAMPLE:
	case AIFC_COMPRESS:
	    outTrack->compressionType = inTrack->compressionType;
	    if ((AF_COMPRESSION_AWARE_MULTIRATE == outTrack->compressionType) ||
	        (AF_COMPRESSION_MPEG1 == outTrack->compressionType))
	    {
		pvlist = AUpvnew(MAX_AWARE_OPTS);
		counter = 0;
		if (AF_COMPRESSION_MPEG1 == outTrack->compressionType)
		{
		    AUpvsetparam   (pvlist, counter, AF_AWARE_PARAM_LAYER);
		    AUpvsetvaltype (pvlist, counter, AU_PVTYPE_LONG);
		    AUpvsetval        (pvlist, counter++, &outAware->layer);
		}
		if (outTrack->aware.bitRateTargetSpecified)
		{
		    AUpvsetparam   (pvlist, counter, AF_AWARE_PARAM_BITRATE_TARGET);
		    AUpvsetvaltype (pvlist, counter, AU_PVTYPE_LONG);
		    AUpvsetval        (pvlist, counter++, &outAware->bitRateTarget);
		}
		if (outTrack->aware.channelPolicySpecified)
		{
		    AUpvsetparam   (pvlist, counter, AF_AWARE_PARAM_CHANNEL_POLICY);
		    AUpvsetvaltype (pvlist, counter, AU_PVTYPE_LONG);
		    AUpvsetval        (pvlist, counter++, &outAware->channelPolicy);
		}
		if (outTrack->aware.bitRatePolicySpecified)
		{
		    AUpvsetparam   (pvlist, counter, AF_AWARE_PARAM_BITRATE_POLICY);
		    AUpvsetvaltype (pvlist, counter, AU_PVTYPE_LONG);
		    AUpvsetval        (pvlist, counter++, &outAware->bitRatePolicy);
		}
		if (outTrack->aware.constantQualityNoiseToMaskRatioSpecified)
		{
		    AUpvsetparam   (pvlist, counter, AF_AWARE_PARAM_CONST_QUAL_NMR);
		    AUpvsetvaltype (pvlist, counter,AU_PVTYPE_DOUBLE);
		    AUpvsetval        (pvlist, counter++,  &outAware->constantQualityNoiseToMaskRatio);
		}
		afInitCompressionParams(setUp,
		    id,
		    outTrack->aware.defaultConfiguration,
		    pvlist,
		    counter);
	    }
	    break;

	case AIFF_TO_AIFC:
	case AIFC_TO_AIFF:
	case AIFC_DECOMPRESS:
	    outTrack->compressionType = AF_COMPRESSION_NONE;
	    break;
	default:
	    outTrack->compressionType = inTrack->compressionType;
	    break;
	}
    }
    afInitCompression(setUp, id, outTrack->compressionType);
}   /* ---- end SetUpTrackParameters() ---- */

/* ***************************************************************************
 * SetUpTrackAIFFParameters:    initialize file set up for track AIFF parameters
 * ****************************************************************************/
void 
SetUpTrackAIFFParameters(AFfilesetup setUp, int id, Track *track)
{
    afInitAESChannelDataTo(setUp, id, track->aesDataSeen);

    /* set up markers: ids and names */
    if (track->markerCount)
    {
	int        i;
	afInitMarkIDs(setUp, id, track->markerIDs, track->markerCount);
	for (i = 0; i < track->markerCount; i++)
	    afInitMarkName(setUp, id, track->markerIDs[i], track->markerNames[i]);
    }
    else
	afInitMarkIDs(setUp, id, NULL, 0);
}   /* ---- end SetUpTrackAIFFParameters() ---- */

/* ***************************************************************************
 * SetUpInstrumentAIFFParameters:    initialize file set up for instrument chunk
 * ****************************************************************************/
void 
SetUpInstrumentAIFFParameters(AFfilesetup setUp, int id, Instrument *data)
{
    if (data->loopCount)
	afInitLoopIDs(setUp, id, data->loopIDs, data->loopCount);
    else
	afInitLoopIDs(setUp, id, NULL, 0);
}   /* ---- end SetUpInstrumentAIFFParameters() ---- */

/* ***************************************************************************
 * SetUpMiscellaneousData:
 * ****************************************************************************/
void
SetUpMiscellaneousData(AFfilesetup setUp, int id, Miscellaneous *data)
{
    afInitMiscSize(setUp, id, data->size);
    afInitMiscType(setUp, id, data->type);
}   /* ---- end SetUpMiscellaneousData() ---- */

/* ******************************************************************
 * ReadSoundFileParameters:    Read file header parameters and AIFF data
 *                Return Boolean success
 * ****************************************************************** */
char 
ReadSoundFileParameters(AFfilehandle handle, SoundFile *file)
{
    int    i;

    file->trackCount = afGetTrackIDs(handle, NULL);
    afGetTrackIDs(handle, &file->trackID);
    ReadTrackParameters(handle, file->trackID, file->track, file->formatID);

    /* extra work for files which support insts */
    if (file->mayHaveExtras)
    {
	/* acquire marker and AES data */
	ReadTrackAIFFParameters(handle, file->trackID, file->track);

	/* acquire instrument data */
	file->instrumentCount = afGetInstIDs(handle, NULL);
	if (file->instrumentCount)
	{
	    file->instrumentIDs = calloc(file->instrumentCount,
	        sizeof(*file->instrumentIDs));
	    file->instruments = calloc(file->instrumentCount,
	        sizeof(Instrument **));
	    afGetInstIDs(handle, file->instrumentIDs);
	    for(i = 0; i < file->instrumentCount; i++) {
		file->instruments[i] = NewInstrument(NULL);
		ReadInstrumentAIFFParameters(handle, file->trackID,
		    file->instrumentIDs[i],
		    file->instruments[i]);
	    }
	}
    }


    /* extra work for AIFF,AIFF-C,NeXT,IRCAM, etc.,  files */
    if (file->mayHaveMiscs || AF_FILE_NEXTSND == file->formatID
        || AF_FILE_IRCAM == file->formatID)
    {
	/* acquire miscellaneous data (AIFF/AIFF-C,NeXT,IRCAM file formats only) */
	file->miscellaneousCount = afGetMiscIDs(handle, NULL);
	if (file->miscellaneousCount)
	{
	    file->miscellaneousIDs = (int *) malloc(file->miscellaneousCount*sizeof(int));
	    if (!file->miscellaneousIDs)
	    {
		SFError("Failed to allocate memory for miscellaneous data IDs");
		return (FALSE);
	    }
	    afGetMiscIDs(handle, file->miscellaneousIDs);
	    file->miscellaneous = (Miscellaneous **)
	        malloc(file->miscellaneousCount*sizeof(int));
	    if (!file->miscellaneous)
	    {
		SFError("Failed to allocate memory for miscellaneous data");
		return (FALSE);
	    }

	    for (i = 0; i < file->miscellaneousCount; i++)
	    {
		file->miscellaneous[i] = (Miscellaneous *)
		    malloc(file->miscellaneousCount*sizeof(Miscellaneous));
		if (!file->miscellaneous[i])
		{
		    SFError("Failed to allocate memory for miscellaneous data");
		    return (FALSE);
		}

		ReadMiscellaneousData(handle, file->miscellaneousIDs[i], file->miscellaneous[i]);
	    }
	}
    }

    return (TRUE);
} /* ---- end ReadSoundFileParameters() ---- */

/* ******************************************************************
 * ReadTrackParameters:     Read track parameters
 * ****************************************************************** */
void 
ReadTrackParameters(AFfilehandle handle, int id, Track *d, int formatID)
{
    char    *s;

    /* even works for raw files, since file setup was configured */
    d->frameCount = afGetFrameCount(handle, id);

#ifdef NEEDED_HUH 
    if (formatID != AF_FILE_RAWDATA)
#endif
	{
	    afGetSampleFormat(handle, id, &d->sampleFormat, &d->sampleWidth);
	    d->frameSizeInBytes   = afGetFrameSize(handle, id, FALSE);
	    d->frameSizeSpecified = TRUE;
	    d->channelCount = afGetChannels(handle, id);
	    d->samplingRate = afGetRate(handle, id);

	    /* WAVE files (Intel CPU machines use little endian byte order) */
	    d->byteOrder  = afGetByteOrder(handle, id);
	    d->dataOffset = afGetDataOffset(handle, id);

	    afGetPCMMapping(handle, id, &d->pcmMapSlope, &d->pcmMapIntercept, 
	        &d->pcmMapMinClip, &d->pcmMapMaxClip);
	    d->pcmMapSpecified = TRUE;
#ifdef SAFE
	    printf("ReadTrackParameters(): specified=%d, slope=%g, inter=%g, min=%g, max=%g\n",
	        d->pcmMapSpecified, &d->pcmMapSlope, &d->pcmMapIntercept, 
	        &d->pcmMapMinClip, &d->pcmMapMaxClip);
#endif

	    /* compression type */
	    d->compressionType = afGetCompression(handle, id);
	    s = afGetCompressionName(handle, id);
	    /* strlen dies on null strings, it seems */
	    if (!s)
	    {
		d->compressionName    = (char *) malloc(1);
		d->compressionName[0] = '\0';
	    }
	    else 
	    {
		d->compressionName = (char *) malloc(strlen(s)+1);
		strcpy(d->compressionName, s);
	    }

	    /* Aware MultiRate compression for AIFF-C files only */
	    if (AF_COMPRESSION_AWARE_MULTIRATE == d->compressionType)
	    {
		AUpvlist pvlist = AUpvnew(MAX_AWARE_OPTS);

		AUpvsetparam  (pvlist, 0, AF_AWARE_PARAM_CHANNEL_POLICY);
		AUpvsetvaltype(pvlist, 0, AU_PVTYPE_LONG);

		AUpvsetparam  (pvlist, 1, AF_AWARE_PARAM_BITRATE_POLICY);
		AUpvsetvaltype(pvlist, 1, AU_PVTYPE_LONG);

		afGetCompressionParams(handle, id, &d->compressionType, pvlist, 2);

		AUpvgetval(pvlist, 0, &d->aware.channelPolicy);
		AUpvgetval(pvlist, 1, &d->aware.bitRatePolicy);

		AUpvfree(pvlist);
	    }
	    /* MPEG compression for AIFF-C files only */
	    else if (AF_COMPRESSION_MPEG1 == d->compressionType)
	    {
		AUpvlist pvlist = AUpvnew(MAX_AWARE_OPTS);

		AUpvsetparam  (pvlist, 0, AF_AWARE_PARAM_CHANNEL_POLICY);
		AUpvsetvaltype(pvlist, 0, AU_PVTYPE_LONG);

		AUpvsetparam  (pvlist, 1, AF_AWARE_PARAM_BITRATE_POLICY);
		AUpvsetvaltype(pvlist, 1, AU_PVTYPE_LONG);

		AUpvsetparam  (pvlist, 2, AF_AWARE_PARAM_BITRATE_TARGET);
		AUpvsetvaltype(pvlist, 2, AU_PVTYPE_LONG);

		AUpvsetparam  (pvlist, 3, AF_AWARE_PARAM_LAYER);
		AUpvsetvaltype(pvlist, 3, AU_PVTYPE_LONG);

		afGetCompressionParams(handle, id, &d->compressionType, pvlist, 4);

		AUpvgetval(pvlist, 0, &d->aware.channelPolicy);
		AUpvgetval(pvlist, 1, &d->aware.bitRatePolicy);
		AUpvgetval(pvlist, 2, &d->aware.bitRateTarget);
		AUpvgetval(pvlist, 3, &d->aware.layer);

		AUpvfree(pvlist);
	    }

	}
} /* ---- end ReadTrackParameters() ---- */

/* ******************************************************************
 * ReadTrackAIFFParameters:     Read track AIFF parameters 
 *                (audio data NOT written)
 *                Return Boolean success.
 * ****************************************************************** */
char 
ReadTrackAIFFParameters(AFfilehandle handle, int id, Track *data)
{
    int            i;
    char            *s;

    /* 
 * read track markers
 */
    data->markerCount = afGetMarkIDs(handle, id, NULL);
    if (data->markerCount)
    {
	data->markerIDs       = (int *)  calloc(data->markerCount, sizeof(int));
	data->markerPositions = (int *)  calloc(data->markerCount, sizeof(int));
	data->markerNames     = (char **) calloc(data->markerCount, sizeof(char *));
	if ((!data->markerIDs)||(!data->markerPositions)||(!data->markerNames))
	{
	    SFError("Memory allocation failure for track marker read.");
	    if (data->markerIDs)
		free(data->markerIDs);
	    if (data->markerPositions)
		free(data->markerPositions);
	    if (data->markerNames)
		free(data->markerNames);
	    return (FALSE);
	}

	afGetMarkIDs(handle, id, data->markerIDs);
	for (i = 0; i < data->markerCount; i++)
	{
	    data->markerPositions[i] = afGetMarkPosition(handle, id, data->markerIDs[i]);
	    s = afGetMarkName(handle, id, data->markerIDs[i]);

	    /* strlen() dies on null strings, it seems.  So create NULL string */
	    if (!s)
	    {
		data->markerNames[i] = (char *) malloc(1);
		data->markerNames[i][0] = '\0';
	    }
	    else 
	    {
		data->markerNames[i] = (char *) malloc(strlen(s)+1);
		strcpy(data->markerNames[i], s);
	    }
	}
    }
    else
    {
	data->markerIDs       = NULL;
	data->markerPositions = NULL;
	data->markerNames     = NULL;
    }

    /* read AES data bytes */
    data->aesDataSeen = afGetAESChannelData(handle, id, data->aesData);

    return (TRUE);
} /* ---- end ReadTrackAIFFParameters() ---- */

/* ***************************************************************************
 * ReadInstrumentAIFFParameters:    read AIFF/C instrument chunk parameters
 * ****************************************************************************/
void
ReadInstrumentAIFFParameters(AFfilehandle handle, int trackID, int instID, 
Instrument *data)
{
    int    i, nparams;
    if((nparams = afQueryLong(AF_QUERYTYPE_INSTPARAM, AF_QUERY_ID_COUNT,
        afGetFileFormat(handle, NULL), 0, 0)) > 0)
    {
	int *paramIDs = (int *) afQueryPointer(
	    AF_QUERYTYPE_INSTPARAM, AF_QUERY_IDS,
	    afGetFileFormat(handle, NULL), 0, 0);

	/* acquire keyboard+velocity map and playback parameters */
	if(inList(AF_INST_MIDI_BASENOTE, paramIDs, nparams))
	    data->midiKeyboardMap[BASE] = afGetInstParamLong(handle, instID,
	        AF_INST_MIDI_BASENOTE);
	if(inList(AF_INST_MIDI_LONOTE, paramIDs, nparams))
	    data->midiKeyboardMap[LOW]  = afGetInstParamLong(handle, instID,
	        AF_INST_MIDI_LONOTE);
	if(inList(AF_INST_MIDI_HINOTE, paramIDs, nparams))
	    data->midiKeyboardMap[HIGH] = afGetInstParamLong(handle, instID,
	        AF_INST_MIDI_HINOTE);

	if(inList(AF_INST_MIDI_LOVELOCITY, paramIDs, nparams))
	    data->midiVelocity[LOW]  = afGetInstParamLong(handle, instID,
	        AF_INST_MIDI_LOVELOCITY);
	if(inList(AF_INST_MIDI_HIVELOCITY, paramIDs, nparams))
	    data->midiVelocity[HIGH] = afGetInstParamLong(handle, instID,
	        AF_INST_MIDI_HIVELOCITY);

	if(inList(AF_INST_NUMCENTS_DETUNE, paramIDs, nparams))
	    data->detuneInCents  = afGetInstParamLong(handle, instID,
	        AF_INST_NUMCENTS_DETUNE);
	if(inList(AF_INST_NUMDBS_GAIN, paramIDs, nparams))
	    data->gainInDecibels = afGetInstParamLong(handle, instID,
	        AF_INST_NUMDBS_GAIN);
	free(paramIDs);
    }

    /* 
 * read loop data (currently supports ONLY MAX_LOOPS=2 loops) 
 */
    data->loopCount = afGetLoopIDs(handle, instID, data->loopIDs);
    for (i = 0; (i < data->loopCount)&&(i < MAX_LOOPS); i++)
    {
	data->loopMode[i]           = afGetLoopMode(handle, instID, data->loopIDs[i]);
	data->loopStartMarkerID[i] = afGetLoopStart(handle, instID, data->loopIDs[i]);
	data->loopEndMarkerID[i]   = afGetLoopEnd(handle, instID, data->loopIDs[i]);

	/* if loop exists, get loop start and end frame positions */
	if (data->loopStartMarkerID[i] > 0)
	{
	    data->loopStartFrame[i] = afGetMarkPosition(handle, trackID, 
	        data->loopStartMarkerID[i]);
	    data->loopEndFrame[i]   = afGetMarkPosition(handle, trackID,
	        data->loopEndMarkerID[i]);
	}
	else
	{
	    data->loopStartFrame[i] = 0;
	    data->loopEndFrame[i] = 0;
	}
    }
}   /* ---- end ReadInstrumentAIFFParameters() ---- */

/* ******************************************************************
 * ReadMiscellaneousData:    read AIFF,AIFC,NeXT,IRCAM miscellaneous chunk 
 * ****************************************************************** */
char 
ReadMiscellaneousData(AFfilehandle handle, int id, Miscellaneous *data)
{
    int    bytesRead;
    char    ok = TRUE;

    data->type = afGetMiscType(handle, id);
    switch (data->type)
    {
    case AF_MISC_COPY:
    case AF_MISC_AUTH:
    case AF_MISC_NAME:
    case AF_MISC_ANNO:
    case AF_MISC_APPL:
    case AF_MISC_MIDI:
    case AF_MISC_PCMMAP:
    case AF_MISC_NeXT:
    case AF_MISC_IRCAM_PEAKAMP:
    case AF_MISC_COMMENT:
    case AF_MISC_ICRD:
    case AF_MISC_ISFT:
	data->size = afGetMiscSize(handle, id);
	if (data->size)
	{
	    data->text = (char *) malloc(data->size + 1); /* extra for terminator */
	    if (!data->text)
	    {
		SFError("Unable to allocate memory for miscellaneous data");
		return (FALSE);
	    }

	    afSeekMisc(handle, id, 0);
	    bytesRead = afReadMisc(handle, id, data->text, data->size);

	    /* terminate text segment */
	    data->text[bytesRead] = '\0';
	    if (bytesRead != data->size)
		ok = FALSE;
	}
	break;

    case AF_MISC_UNRECOGNIZED:
    default:
	SFError("Warning: Unsupported MISC chunk with type=%d"
	    "will be skipped", data->type);
	ok = TRUE;                  /* it's just a warning */
	break;
    }
    return (ok);
} /* ---- end ReadMiscellaneousData() ---- */

/* ******************************************************************
 * WriteSoundFileParameters:    Write file header parameters and 
 *                AIFF,AIFC,NeXT,IRCAM data
 * ****************************************************************** */
void 
WriteSoundFileParameters(AFfilehandle handle, SoundFile *file)
{
    int    i;
    int    outFileFormatID;
    char    outFileSupportsMiscs = FALSE;
    char    outFileSupportsInsts = FALSE;

    outFileFormatID = afGetFileFormat(handle, NULL);
    switch (outFileFormatID)
    {
    case AF_FILE_AIFF:
    case AF_FILE_AIFFC:
	outFileSupportsMiscs = TRUE;
	outFileSupportsInsts = TRUE;
	break;
    case AF_FILE_WAVE:
	/*    case AF_FILE_NEXTSND: */ /* commented out due to code below */
	/*    case AF_FILE_BICSF:   */
	outFileSupportsMiscs = TRUE;
	break;
    case AF_FILE_SOUNDESIGNER2:
    case AF_FILE_AVR:
    case AF_FILE_SAMPLEVISION:
	outFileSupportsInsts = TRUE;
	break;
    default:
	break;
    }
    WriteTrackParameters(handle, file->trackID, file->track);

    /* write extra parameters */
    if (file->mayHaveExtras && outFileSupportsInsts) {
	WriteTrackAIFFParameters(handle, file->trackID, file->track);
	if (file->instrumentCount)
	    for(i = 0; i < file->instrumentCount; i++)
		WriteInstrumentAIFFParameters(handle, file->instrumentIDs[i], 
		    file->instruments[i]);
    }

    if (file->mayHaveMiscs && outFileSupportsMiscs) {
	for (i = 0; i < file->miscellaneousCount; i++) {
	    WriteMiscellaneousData(handle, file->miscellaneousIDs[i], 
	        file->miscellaneous[i]);
	}
    }

    /* write single NeXT specific chunk or single IRCAM comment chunk */
    if (file->mayHaveMiscs &&
        (AF_FILE_NEXTSND == outFileFormatID
        || AF_FILE_IRCAM == outFileFormatID))
    {
	for (i = 0; i < file->miscellaneousCount; i++) {
	    if (AF_MISC_NeXT == file->miscellaneous[i]->type
	        && AF_FILE_NEXTSND == outFileFormatID) {
		WriteMiscellaneousData(handle, file->miscellaneousIDs[i], 
		    file->miscellaneous[i]);
		break;
	    }
	    if (AF_MISC_COMMENT == file->miscellaneous[i]->type
	        && AF_FILE_IRCAM == outFileFormatID) {
		WriteMiscellaneousData(handle, file->miscellaneousIDs[i], 
		    file->miscellaneous[i]);
		break;
	    }
	}
    }
} /* ---- end WriteSoundFileParameters() ---- */

/* ***************************************************************************
 * WriteTrackParameters:  write track parameters to open file 
 *                (audio data NOT written)
 * ****************************************************************************/
void 
WriteTrackParameters(AFfilehandle handle, int id, Track *d)
{
    double slope, inter, min, max;

#if 0
    /* set channel matrix */
    afSetChannelMatrix(handle, id, matrix);
#endif

    /* set track PCM mapping, only if specified */
    if (d->pcmMapSpecified)
	afSetTrackPCMMapping(handle, id, 
	    d->pcmMapSlope, d->pcmMapIntercept, 
	    d->pcmMapMinClip, d->pcmMapMaxClip);

#ifdef SAFE
    afGetPCMMapping(handle, id, &slope, &inter, &min, &max);
    printf("WriteTrackParameters(): specified=%d, slope=%g, inter=%g, min=%g, max=%g\n",
        d->pcmMapSpecified, slope, inter, min, max);
#endif
}   /* ---- end WriteTrackParameters() ---- */

/* ***************************************************************************
 * WriteTrackAIFFParameters:  write track AIFF parameters to open file 
 *                (audio data NOT written)
 * ****************************************************************************/
void 
WriteTrackAIFFParameters(AFfilehandle handle, int id, Track *data)
{
    int    i;

    /* write AES data */
    if (data->aesDataSeen)
	afSetAESChannelData(handle, id, data->aesData);

    /* write marker data */
    for (i = 0; i < data->markerCount; i++)
	afSetMarkPosition(handle, id, data->markerIDs[i], data->markerPositions[i]);
}   /* ---- end WriteTrackAIFFParameters() ---- */

/* ***************************************************************************
 * WriteInstrumentAIFFParameters:  write instrument parameters to open file
 * ****************************************************************************/
void 
WriteInstrumentAIFFParameters(AFfilehandle handle, int id, Instrument *data)
{
    int    i, nparams;
    if((nparams = afQueryLong(AF_QUERYTYPE_INSTPARAM, AF_QUERY_ID_COUNT,
        afGetFileFormat(handle, NULL), 0, 0)) > 0)
    {
	int *paramIDs = (int *) afQueryPointer(
	    AF_QUERYTYPE_INSTPARAM, AF_QUERY_IDS,
	    afGetFileFormat(handle, NULL), 0, 0);

	if(inList(AF_INST_MIDI_BASENOTE, paramIDs, nparams))
	    afSetInstParamLong(handle, id, AF_INST_MIDI_BASENOTE, data->midiKeyboardMap[BASE]);
	if(inList(AF_INST_MIDI_LONOTE, paramIDs, nparams))
	    afSetInstParamLong(handle, id, AF_INST_MIDI_LONOTE,   data->midiKeyboardMap[LOW]);
	if(inList(AF_INST_MIDI_HINOTE, paramIDs, nparams))
	    afSetInstParamLong(handle, id, AF_INST_MIDI_HINOTE,   data->midiKeyboardMap[HIGH]);

	if(inList(AF_INST_MIDI_LOVELOCITY, paramIDs, nparams))
	    afSetInstParamLong(handle, id, AF_INST_MIDI_LOVELOCITY, data->midiVelocity[LOW]);
	if(inList(AF_INST_MIDI_HIVELOCITY, paramIDs, nparams))
	    afSetInstParamLong(handle, id, AF_INST_MIDI_HIVELOCITY, data->midiVelocity[HIGH]);

	if(inList(AF_INST_NUMDBS_GAIN, paramIDs, nparams))
	    afSetInstParamLong(handle, id, AF_INST_NUMDBS_GAIN,     data->gainInDecibels);
	if(inList(AF_INST_NUMCENTS_DETUNE, paramIDs, nparams))
	    afSetInstParamLong(handle, id, AF_INST_NUMCENTS_DETUNE, data->detuneInCents);

	if(inList(AF_INST_SUSLOOPID, paramIDs, nparams))
	    afSetInstParamLong(handle, id, AF_INST_SUSLOOPID, data->loopIDs[LOOP_SUSTAIN]);
	if(inList(AF_INST_RELLOOPID, paramIDs, nparams))
	    afSetInstParamLong(handle, id, AF_INST_RELLOOPID, data->loopIDs[LOOP_RELEASE]);
	free(paramIDs);
    }
    for (i = 0; i < MAX_LOOPS; i++)
    {
	afSetLoopStart(handle, id, data->loopIDs[i], data->loopStartMarkerID[i]);
	afSetLoopEnd(handle, id, data->loopIDs[i],   data->loopEndMarkerID[i]);
	afSetLoopMode(handle, id, data->loopIDs[i],  data->loopMode[i]);
    }
}   /* ---- end WriteInstrumentAIFFParameters() ---- */

/* ***************************************************************************
 * WriteMiscellaneousData:  write miscellaneous data to open file
 *                Return Boolean success.
 * ****************************************************************************/
char  
WriteMiscellaneousData(AFfilehandle handle, int id, Miscellaneous *data)
{
    int bytesWritten;

    if (data->size)
    {
	afSeekMisc(handle, id, 0);
	bytesWritten = afWriteMisc(handle, id, data->text, data->size);
	if (bytesWritten < data->size)
	{
	    SFError("Short write on miscellaneous data");
	    return (FALSE);
	}
    }
    return (TRUE);
}   /* ---- end WriteMiscellaneousData() ---- */

/* ***************************************************************************
 * CullMiscellaneousAIFFData:    deallocate application specific and MIDI chunks
 *                Return # of chunks removed.
 * ****************************************************************************/
void   
CullMiscellaneousAIFFData(SoundFile *file)
{
    int    i;
    int    chunksRemoved = 0;

    /* pack non-null miscellaneous chunk ptrs to  base of ptr arrary */
    for (i = 0; i < file->miscellaneousCount;)
    {
	Miscellaneous *m = file->miscellaneous[i];
	if (m)
	{
#if 0
	    /* deallocate AF_MISC_APPL and AF_MISC_MIDI miscellaneous chunks */
	    if ((AF_MISC_APPL == m->type)||(AF_MISC_MIDI == m->type))
#else
		/* deallocate AF_MISC_APPL miscellaneous chunks */
		/* MIDI is left in because of bug in this routine - DS */
		if (AF_MISC_APPL == m->type)
#endif
		    {
			free(m->text);
			free(m);
			/* shuffle over next chunk */
			if ((i+1) < file->miscellaneousCount)
			{
			    file->miscellaneous[i] = file->miscellaneous[i+1];
			    file->miscellaneous[i+1] = NULL;
			}
			/* arrived at last chunk */
			else
			    file->miscellaneous[i++] = NULL;

			chunksRemoved++;
		    }
		else
		    i++;
	}
	else
	    i++;
    }

    file->miscellaneousCount -= chunksRemoved;
}   /* ---- end CullMiscellaneousAIFFData() ---- */

/* **********************************************************************
 * IsSoundFile:   Return Boolean sound file recognition
 * **********************************************************************/
char 
IsSoundFile(char *path)
{
    int fd, formatID;

    /* this method does not check for a proper/complete audio file header,
 * but is much faster than afOpenFile() & afCloseFile() */
    fd = open(path, O_RDONLY);
    if (fd < 0)
	return (FALSE);

    /* identify file format if input file not specified as raw */
    formatID = afIdentifyNamedFD(fd, path, (int*) 0);
    if ((AF_FILE_UNKNOWN == formatID) || (AF_FILE_UNSUPPORTED == formatID))
    {
	close(fd);
	return (FALSE);
    }

    /* see if format is implemented in the library */
    if (!afQueryLong(AF_QUERYTYPE_FILEFMT, 
        AF_QUERY_IMPLEMENTED, formatID, 0, 0))
    {
	close(fd);
	return (FALSE);
    }

    close(fd);
    return (TRUE);
}   /* ---- end IsSoundFile() ---- */

/* **********************************************************************
 * StringDuplicate:   strdup() plus NULL checking and empty string return
 * **********************************************************************/
char *
StringDuplicate(char *s)
{
    if (s)
	return(strdup(s));
    else
    {
	char *t = (char *) malloc(sizeof(char));
	*t = '\0';
	return (t);
    }
}   /* ---- end StringDuplicate() ---- */
