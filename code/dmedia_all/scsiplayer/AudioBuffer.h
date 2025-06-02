//
// $Author: pw $ $Version$ $Date: 1998/01/09 02:29:35 $
//
//----------------------------------------------------------------------
#ifndef AudioBuffer_h
#define AudioBuffer_h

#include <sys/types.h>
#include <audio.h>
#include <audiofile.h>
#include <dmedia/dm_audioconvert.h>

enum Direction {in, out, bi};

class FilePopup;
class OutputFile;

class AudioBuffer {
public:
  AudioBuffer(const char*, Direction, int, int);
  ~AudioBuffer();

  int play(char *buf, int count);
  void flushRecordingToFile();
  int read(char *buf, int count);
  void setInputRate(int rate);
  void setOutputRate(int rate);
  void setGain(float gain);
  void startRecordingToFile(char*);
  void stopRecordingToFile();
  void pauseRecordingToFile();
  void setInputFile(AFfilehandle file);
  int isAtEOF() const;
  double getInputRate();
  double getOutputRate();
  float getGain();
  int getSilence()	{ return _silence; }
  ALport getPort()	{ return _outPort; }
  char* getErrorString();
  void closeOutput();
  void closeInput();
  void stopInput();
  void flushAudioBuffer();
  void swapCopy(short* src, short *dst, int n);

protected:

  char* _name;
  int _inSize;
  int _outSize;
  int _silence;
  short* _inBuf;
  short* _outBuf;
  ALport _inPort;
  ALport _outPort;
  ALconfig _config;
  AFfilehandle _inFile;
  DMaudioconverter _converter;
  DMparams* _sourceParams;
  DMparams* _destParams;
  DMparams* _conversionParams;

  OutputFile* _outputFile;
  int _recording;
  int _atEOF;
  float _gain;
  float _gainCache;
  double _inRate;
  double _outRate;
  long _remainingSamps;
  int _stopped;
  Direction _direction;
  //FilePopup* _filePopup;

  static void errorHandler(long code, const char* fmt, ...);
  static int errorHandlerSet;
  static long lastError;
  static char errorString[1024];

};
#endif
