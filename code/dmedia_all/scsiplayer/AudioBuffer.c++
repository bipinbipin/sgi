//////////////////////////////////////////////////////////////////////////////
//
// AudioBuffer.c++
//
//                Manage and play a buffer of audio samples
//
//
// Copyright 1996, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
//
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
///////////////////////////////////////////////////////////////////////////////


#include <fcntl.h>
#include <limits.h>
#include <libc.h>
#include <malloc.h>
#include <osfcn.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <sys/cachectl.h>
#include <dmedia/dm_audio.h>
#include "AudioBuffer.h"
#include "OutputFile.h"

//#define DBG(x) {x;}
#define DBG(x) {}  

#define MAX_BUFWRITESIZE 100

long AudioBuffer::lastError = 0;
int AudioBuffer::errorHandlerSet = 0;
char AudioBuffer::errorString[1024];

void
AudioBuffer::errorHandler(long code, const char* fmt, ...)
{	
  lastError = code;
  switch (code) {
  case AL_BAD_NO_PORTS:
    fprintf(stderr, "All audio ports are busy.");
    break;
  case AL_BAD_QSIZE:
    fprintf(stderr, "An audio port was opened with an incorrect size.");
    break;
  case AL_BAD_OUT_OF_MEM:
    fprintf(stderr, "The requested operation has run out of memory!");
    break;
  default:
    fprintf(stderr, "Unexpected problem in audio library. Error code %d.\n%s", code, fmt);
    break;
  }
}

char* AudioBuffer::getErrorString()
{
  long error = lastError;
  lastError = 0;
  return error? errorString : NULL;
}

AudioBuffer::AudioBuffer(const char* name, Direction direction, int inSize, int outSize)
{
  name = strdup(name ? name : "");

  // set the error handler for AL errors.
  if (!errorHandlerSet) {
    alSetErrorHandler(&AudioBuffer::errorHandler);
    errorString[0] = 0;
    errorHandlerSet = 1;
  }	
  // get a new ALconfig strucure with default settings (to be used
  // later to open an audio port.
  _config = alNewConfig();
  // override the default sample width.
  alSetWidth(_config, AL_SAMPLE_16);
	
  _inSize = inSize;
  _outSize = outSize;

  // don't open the ports here. Instead,  we wait * until we 
  // actually need them. This allows us both to conserve
  // system resources and to minimize latency at the output.
  _inPort = NULL;
  _outPort = NULL;
  _inFile = NULL;
  _outputFile = NULL;
  _recording = 0;
  _inBuf = _outBuf = NULL;
  _inRate = _outRate = NULL;

  _atEOF = 0;
  _stopped = 0;
  _direction = direction;
  _gain = 1.0;
  _gainCache = 0.0;

  dmACCreate(&_converter);
  dmParamsCreate(&_sourceParams);
  dmParamsSetEnum(_sourceParams, DM_AUDIO_FORMAT, DM_AUDIO_TWOS_COMPLEMENT);
  dmParamsSetInt(_sourceParams, DM_AUDIO_WIDTH, 16);
  dmParamsSetEnum(_sourceParams, DM_AUDIO_BYTE_ORDER, DM_AUDIO_LITTLE_ENDIAN);
  dmParamsSetInt(_sourceParams, DM_AUDIO_CHANNELS, 2);
  dmParamsSetFloat(_sourceParams, DM_AUDIO_RATE, (double)44100.0); 
  dmParamsSetString(_sourceParams, DM_AUDIO_COMPRESSION, DM_AUDIO_UNCOMPRESSED);

  dmParamsCreate(&_destParams);
  dmParamsSetEnum(_destParams, DM_AUDIO_BYTE_ORDER, DM_AUDIO_BIG_ENDIAN);
  dmParamsSetFloat(_destParams, DM_AUDIO_PCM_MAP_MAXCLIP, 32767.0);
  dmParamsSetFloat(_destParams, DM_AUDIO_PCM_MAP_MINCLIP, -32768.0);
  dmParamsSetFloat(_destParams, DM_AUDIO_PCM_MAP_INTERCEPT, 0.0);

  dmACSetParams(_converter, _sourceParams, _destParams, NULL);

}

void AudioBuffer::closeOutput()
{
  if (_outPort != (ALport)0) {
    alClosePort(_outPort);
    _outPort = 0;
  }
  if (_outputFile) {
    /*
     * update,  but do not close, the output file.
     * This allows us to write snippets into the same file
     * across audio port opens/closes.
     */
    afSyncFile(_outputFile->getAFHandle());
  }
}


void AudioBuffer::closeInput()
{
  if (_inPort != (ALport)0) {
    alClosePort(_inPort);
    _inPort = 0;
    _stopped = 0;
  }
}

AudioBuffer::~AudioBuffer()
{
  closeOutput();
  closeInput();
  if (_config) {
    alFreeConfig(_config);
  }
  if (_outBuf) {
    delete _outBuf;
  }
  if (_name) {
    delete _name;
  }
}



void AudioBuffer::setOutputRate(int rate)
{
  ALpv pvbuf[1];
  pvbuf[0].param = AL_RATE;
  _outRate = (double)rate;
  pvbuf[0].value.ll = alDoubleToFixed(_outRate);
  if (alSetParams(AL_DEFAULT_OUTPUT, pvbuf, 1) < 0) 
    printf("alSetParams failed in setOutputRate: %s\n", alGetErrorString(oserror()));

}

void AudioBuffer::setInputRate(int rate)
{
  ALpv pvbuf[1];
  pvbuf[0].param = AL_RATE;
  _inRate = (double)rate;
  pvbuf[0].value.ll = alDoubleToFixed(_inRate);
  if (alSetParams(AL_DEFAULT_INPUT, pvbuf, 1) < 0) 
    printf("alSetParams failed in setInputRate: %s\n", alGetErrorString(oserror()));

}


float AudioBuffer::getGain()
{
  return _gain;
}


void AudioBuffer::setGain(float gain)
{
  _gain = gain;
}


double AudioBuffer::getInputRate()
{
  ALpv pvbuf[1];

  if (_inFile) {
    /*
     * if we have an input file open, return the rate at which its
     * data was recorded.
     */
    return _inRate;
  }
  else {
    /*
     * otherwise get the rate from the hardware input
     */
    pvbuf[0].param = AL_RATE;
    if (alGetParams(AL_DEFAULT_INPUT, pvbuf, 1) < 0) 
      printf("alGetParams failed: %s\n", alGetErrorString(oserror()));

    return alFixedToDouble(pvbuf[0].value.ll);
  }
}

double AudioBuffer::getOutputRate()
{
  ALpv pvbuf[1];

  pvbuf[0].param = AL_RATE;

  if (alGetParams(AL_DEFAULT_OUTPUT, pvbuf, 1) < 0) 
    printf("alGetParams failed: %s\n", alGetErrorString(oserror()));
  
  return alFixedToDouble(pvbuf[0].value.ll);
}

void AudioBuffer::startRecordingToFile(char* filename)
{
  if (!_outputFile) {
    _outputFile = new OutputFile(filename);
    _outputFile->open(_outRate);
  }
  _recording = 1;
}

void AudioBuffer::pauseRecordingToFile()
{
  _recording = 0;
}

void AudioBuffer::stopRecordingToFile()
{
  if (_outputFile) {
    delete _outputFile;
    _outputFile = NULL;
  }
  _recording = 0;
}

void AudioBuffer::setInputFile(AFfilehandle file)
{
  _inFile = file;
  if (_inFile) {
    _inRate = (int) afGetRate(_inFile, AF_DEFAULT_TRACK);
  }
  _atEOF = 0;
}


int AudioBuffer::isAtEOF() const
{
  /*
   * returns True if we're at the end of the input file.
   */
  return (_direction == in) ? _atEOF : 0;
}

void AudioBuffer::stopInput()
{
  if (_direction == in && !_stopped && _inPort) {
    _remainingSamps = alGetFilled(_inPort);
    _stopped = 1;
  }
}

int AudioBuffer::read(char *buf, int count)
{
  char nbuf[60];
  int fill, i;
  long nread;
	
  /*
   * check to see if we've opened the port yet.
   * If not, do so. 
   */
  if (_inPort == 0 && _direction != out) {
    alSetQueueSize(_config, _inSize);
    sprintf(nbuf, "%sInput", _name);
    _inPort = alOpenPort(nbuf, "r", _config);
    if (_direction == in) _silence = 0;
  }
  if (_inPort == 0) {
    return 0;
  }
	
  if (_inBuf == (short *)0)
    _inBuf = new short[_inSize];
  if (_inFile) {
    nread = afReadFrames(_inFile, AF_DEFAULT_TRACK, _inBuf, count/4);
    nread *= 2;
  }
  else {
    nread = count/4;
    if (_stopped) {
      if (nread > _remainingSamps)
	nread = _remainingSamps;
      _remainingSamps -= nread;
    }
    if (nread > 0)     
      alReadFrames(_inPort, _inBuf, nread/2);
  }	
  if (nread < count/2) {
    /*
     * if @ end-of-file, zero the rest of the input buffer.
     * also set the at-end-of-file flag.
     */
    bzero(_inBuf + nread, count-(2*(int)nread));
    _atEOF = 1;
  }
  for (i = 0, fill = 0; i < count; fill++) {
    register short sample = _inBuf[fill];
    if (-68 <= sample && sample <= 10)
      _silence++;
    else {
#ifdef TESTING
      if (silence > 4000)
	printf("silence = %d, lastval = %d\n", _silence, _inBuf[fill]);
#endif
      _silence = 0;
    }
    buf[i++] = sample & 0xff;
    buf[i++] = sample >> 8;
  }
  return count;
}


// int AudioBuffer::playXn(char* buf, int count, int speed)
// {
//     char nbuf[60];
//     int fill, i;

//     /*
//      * check to see if we've opened the port yet.
//      * If not, do so. 
//      */
//     if (outPort == 0 && (direction != in)) {
// 	ALsetqueuesize(config, outSize);
// 	sprintf(nbuf, "%sOutput", name);
// 	outPort = ALopenport(nbuf, "w", config);
//     }
//     if (outPort == 0) {
// 	return 0;
//     }
	
//     if (outBuf == (short *)0)
// 	outBuf = new short[outSize];
//     for (i = 0, fill = 0; i < count; i += (2 * speed), fill++) {
// 	outBuf[fill] = buf[i] | (buf[i+1] << 8);
//     }
//     ALwritesamps(outPort, outBuf, fill);
//     return count;
// }


void ReportDetailedDmError()
{
  char error_detail[DM_MAX_ERROR_DETAIL];
  int errornum;
  dmGetError(&errornum, error_detail);
  fprintf(stderr,"DM error: %s\n", error_detail[errornum]);
}

int AudioBuffer::play(char* buf, int byteCount)
{
  static char nbuf[60];
  /*
   * check to see if we've opened the port yet.
   * If not, do so. 
   */
  if (_outPort == 0 && _direction != in) {
    alSetQueueSize(_config, _outSize/2);
    sprintf(nbuf, "%sOutput", _name);
    _outPort = alOpenPort(nbuf, "w", _config);
  }
  if (_outPort == 0) {
    return 0;
  }
	
//   int tmpRate = getOutputRate();
//   if (_outRate != tmpRate) {
//     fprintf(stderr,"New rate = %d\n", tmpRate);
//     dmParamsSetFloat(_destParams, DM_AUDIO_RATE, (double)tmpRate); 
//     dmACSetParams(_converter, NULL, _destParams, NULL);
//     _outRate = tmpRate;
//   }

  if (_gain != _gainCache) {
    double slope = 32767.0 * _gain;
    DBG(fprintf(stderr,"_gain = %f; slope = %f\n", _gain, slope));
    dmParamsSetFloat(_destParams, DM_AUDIO_PCM_MAP_SLOPE, slope);
    dmACSetParams(_converter, NULL, _destParams, NULL);
    _gainCache = _gain;
  }

  if (_outBuf == (short*)0) {
    _outBuf = new short[_outSize*2];
  }
  int inFrameCount = byteCount/4;
  //int outFrameCount = inFrameCount*(float)((float)_outRate/(float)_inRate);

  if (dmACConvert(_converter, buf, _outBuf, &inFrameCount, &inFrameCount) == DM_FAILURE)
    ReportDetailedDmError();
    
  /*
   * XXX we can be smart about this. if AL write would block, 
   * we can do the other one first.
   */
  alWriteFrames(_outPort, _outBuf, inFrameCount);
  if (_recording) {
    if (afWriteFrames(_outputFile->getAFHandle(),
		      AF_DEFAULT_TRACK, buf, inFrameCount)
	< inFrameCount) {
      fprintf(stderr, "Error writing to output file (is your disk full)?\nClosing file automatically.");
      delete _outputFile;
      _outputFile = 0;    
    }
  }
  return byteCount;

} // play


void AudioBuffer::flushRecordingToFile()
{
  if (_outputFile) {
    _outputFile->flush();
  }
}

void AudioBuffer::flushAudioBuffer()
{
  if (_outPort != NULL)
    while (alGetFilled(_outPort) != 0) {
      sginap(1);
    }
}
