#ifndef __OUTPUTFILE_H__
#define __OUTPUTFILE_H__

#include <audiofile.h>

class OutputFile {
  public:
    OutputFile(char*);
    ~OutputFile();
    
    int getFD()			{ return _fd; }
    AFfilehandle getAFHandle()	{ return _handle; }
    int isOpen()		{ return _handle != AF_NULL_FILEHANDLE; }
    
    int open(int);
	void flush();
    
  protected:
    int _fd;
    AFfilehandle _handle;
};

#endif
