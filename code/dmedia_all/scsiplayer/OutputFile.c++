// $Id: OutputFile.c++,v 1.2 1996/10/14 03:42:28 pw Exp $

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "OutputFile.h"

OutputFile::OutputFile(char* file)
{
    _handle = AF_NULL_FILEHANDLE;
    _fd = ::open(file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
}

OutputFile::~OutputFile()
{
    if (_handle)
	AFclosefile(_handle);
    else if (_fd)
	close(_fd);
}

int OutputFile::open(int frequency)
{
  AFfilesetup setup;
    
  if (_fd != -1) {
    setup = AFnewfilesetup();
    afInitFileFormat(setup, AF_FILE_AIFF); 
    AFinitchannels(setup, AF_DEFAULT_TRACK, 2);
    afInitRate(setup, AF_DEFAULT_TRACK, (double) frequency);
    AFinitcompression(setup,AF_DEFAULT_TRACK, AF_COMPRESSION_NONE);
    afInitSampleFormat(setup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    _handle = AFopenfd(_fd, "w", setup);
    if (_handle == AF_NULL_FILEHANDLE)
      return 0;
    else
      return 1;
  } else
    return 0;
}

void
OutputFile::flush()
{
	AFsyncfile(_handle);
}
